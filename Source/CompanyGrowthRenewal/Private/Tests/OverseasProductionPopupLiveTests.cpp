// 생산 설정 창 실기 검증 ― 격리 -game 에서 실제 UI_ProductionStartPopup 을 띄워 2304·3120×1440 오프스크린으로 그리고 잰다.
// 세이브 안전: 매니저 상태(주문서·자원·공장·창고)를 메모리에서만 바꾸고 장면마다·끝에 되돌린다. 격리 러너는 저장을 막는다.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Panel/ProductionStartPopupWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Common/MaterialReqRowWidget.h"
#include "UI/Element/Cards/ProductionOrderRowWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/WorldMapManager.h"
#include "Table/CountryInfoTable.h"
#include "Table/ProjectDataTable.h"
#include "Table/ProductRecipeTable.h"
#include "Data/WorldMapTypes.h"
#include "Enum/WidgetType.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Input/HittestGrid.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/SNullWidget.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/StrongObjectPtr.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 형제 시험 파일과 재정의 충돌
namespace CGRPopupLive
{
	static constexpr int32 PopupWidths[] = { 2304, 3120 };
	static constexpr int32 PopupHeight = 1440;
	static constexpr double PopupTol = 1.0;

	static FString PopupDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Saved/WorldMapUIOverhaul/Task8/8a/live"));
	}

	static UWorld* PopupPlayWorld()
	{
		if (!GEngine) { return nullptr; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World()
				&& Context.World()->GetGameInstance())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static bool PopupShown(const UWidget* Widget)
	{
		const UWidget* Cur = Widget;
		while (Cur)
		{
			const ESlateVisibility Vis = Cur->GetVisibility();
			if (Vis == ESlateVisibility::Collapsed || Vis == ESlateVisibility::Hidden) { return false; }
			const UWidget* Parent = Cur->GetParent();
			if (const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Parent))
			{
				if (Switcher->GetActiveWidget() != Cur) { return false; }
			}
			if (Parent) { Cur = Parent; continue; }
			const UWidgetTree* Tree = Cast<UWidgetTree>(Cur->GetOuter());
			Cur = Tree ? Cast<UUserWidget>(Tree->GetOuter()) : nullptr;
		}
		return Widget != nullptr;
	}

	struct FPBox
	{
		bool bValid = false;
		double L = 0, T = 0, W = 0, H = 0;
		double R() const { return L + W; }
		double B() const { return T + H; }
	};

	static FPBox PBoxOf(const UWidget* Widget)
	{
		FPBox Out;
		if (!Widget || !PopupShown(Widget)) { return Out; }
		const FGeometry& Geo = Widget->GetCachedGeometry();
		const FVector2D Pos = Geo.GetAbsolutePosition();
		const FVector2D Size = Geo.GetAbsoluteSize();
		Out.bValid = Size.X > 0.5 && Size.Y > 0.5;
		Out.L = Pos.X; Out.T = Pos.Y; Out.W = Size.X; Out.H = Size.Y;
		return Out;
	}

	static double POverlap(const FPBox& A, const FPBox& B)
	{
		if (!A.bValid || !B.bValid) { return 0.0; }
		const double OW = FMath::Min(A.R(), B.R()) - FMath::Max(A.L, B.L);
		const double OH = FMath::Min(A.B(), B.B()) - FMath::Max(A.T, B.T);
		return (OW > 0.5 && OH > 0.5) ? OW * OH : 0.0;
	}

	static FString PBoxStr(const FPBox& B)
	{
		return B.bValid ? FString::Printf(TEXT("(%.0f,%.0f %.0fx%.0f)"), B.L, B.T, B.W, B.H) : TEXT("(none)");
	}

	// 글자 잉크 상자 = 실제로 그려지는 글자 폭(칸 폭이 아니라) ― 말줄임·DownOnly 를 거친 뒤의 겹침을 본다
	static FPBox PTextInk(UTextBlock* Text)
	{
		FPBox Box = PBoxOf(Text);
		if (!Box.bValid || !Text) { return Box; }
		const FVector2D Desired = Text->GetDesiredSize();
		// 칸보다 긴 글자는 말줄임 + 제 칸 자르기일 때만 칸 안에 머문다 ― 아니면 칸 밖으로 그려진다(7b 실측)
		const bool bClipped = Text->GetTextOverflowPolicy() == ETextOverflowPolicy::Ellipsis
			&& Text->GetClipping() == EWidgetClipping::ClipToBounds;
		const double InkW = Desired.X > Box.W && bClipped ? Box.W : Desired.X;
		if (InkW > 0.5 && FMath::Abs(InkW - Box.W) > 0.5)
		{
			const FByteProperty* JustifyProp = CastField<FByteProperty>(UTextLayoutWidget::StaticClass()->FindPropertyByName(TEXT("Justification")));
			const ETextJustify::Type Justify = JustifyProp
				? static_cast<ETextJustify::Type>(JustifyProp->GetPropertyValue_InContainer(Text)) : ETextJustify::Left;
			if (Justify == ETextJustify::Right) { Box.L = Box.R() - InkW; }
			else if (Justify == ETextJustify::Center) { Box.L += (Box.W - InkW) * 0.5; }
			Box.W = InkW;
		}
		return Box;
	}

	struct FPStage
	{
		TSharedPtr<SVirtualWindow> Window;
		TUniquePtr<FHittestGrid> Grid;
		TUniquePtr<FWidgetRenderer> Renderer;
		TStrongObjectPtr<UTextureRenderTarget2D> Target;
		FIntPoint Size = FIntPoint::ZeroValue;

		bool Init(const TSharedRef<SWidget>& Content, FIntPoint InSize)
		{
			Size = InSize;
			const FVector2D DrawSize(Size.X, Size.Y);
			Window = SNew(SVirtualWindow).Size(DrawSize);
			Window->SetContent(Content);
			Window->Resize(DrawSize);
			Grid = MakeUnique<FHittestGrid>();
			Renderer = MakeUnique<FWidgetRenderer>(true);
			// raw target ― true target 은 하드웨어 sRGB 쓰기가 Slate 셰이더 감마와 겹친다
			Target.Reset(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
			return Target.IsValid();
		}

		bool Draw(int32 Passes, FImage* OutImage)
		{
			if (!Window.IsValid() || !Target.IsValid()) { return false; }
			const FVector2D DrawSize(Size.X, Size.Y);
			const FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize, FSlateLayoutTransform(1.f));
			for (int32 Pass = 0; Pass < Passes; ++Pass)
			{
				const FPaintArgs PaintArgs(nullptr, *Grid, FVector2D::ZeroVector, FApp::GetCurrentTime(), 0.016f);
				Renderer->DrawWindow(PaintArgs, Target.Get(), Window.ToSharedRef(), WindowGeometry,
					WindowGeometry.GetLayoutBoundingRect(), 0.016f);
				FlushRenderingCommands();
			}
			if (!OutImage) { return true; }
			if (!FImageUtils::GetRenderTargetImage(Target.Get(), *OutImage) || OutImage->Format != ERawImageFormat::BGRA8)
			{
				return false;
			}
			OutImage->GammaSpace = EGammaSpace::sRGB;
			return true;
		}

		void Detach()
		{
			if (Window.IsValid()) { Window->SetContent(SNullWidget::NullWidget); }
			Window.Reset();
		}
	};

	static bool PSavePNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	struct FPChecker
	{
		FAutomationTestBase* Test = nullptr;
		FString Scene;
		int32 Fails = 0;
		int32 Checks = 0;
		TArray<FString> FailLines;

		void True(const FString& What, bool bCond)
		{
			++Checks;
			if (!bCond)
			{
				++Fails;
				FailLines.Add(What);
				Test->AddError(FString::Printf(TEXT("[%s] %s"), *Scene, *What));
			}
		}
		void Near(const FString& What, double Got, double Want, double Tolerance = PopupTol)
		{
			True(FString::Printf(TEXT("%s = %.1f (기대 %.1f±%.1f)"), *What, Got, Want, Tolerance), FMath::Abs(Got - Want) <= Tolerance);
		}
	};

	struct FPopupBackup
	{
		bool bCaptured = false;
		TArray<FProductionOrder> Orders;
		int32 NextOrderID = 1;
		TMap<EResourceType, int64> Resources;
		TMap<ECountryType, FCountryFactoryData> Factory;
		TMap<FIntPoint, TPair<int64, EQualityGrade>> Goods;

		void Capture(UGameInstance* GI)
		{
			if (UProductionOrderManager* O = GI->GetSubsystem<UProductionOrderManager>())
			{
				Orders = O->GetAllOrders();
				NextOrderID = O->GetNextOrderID();
			}
			if (UResourceItemManager* R = GI->GetSubsystem<UResourceItemManager>()) { Resources = R->GetAllResources(); }
			if (UWorldFactoryManager* F = GI->GetSubsystem<UWorldFactoryManager>()) { F->SerializeForSave(Factory); }
			if (UWorldMapManager* W = GI->GetSubsystem<UWorldMapManager>())
			{
				for (const FIntPoint& Key : W->GetAllProductKeys())
				{
					Goods.Add(Key, TPair<int64, EQualityGrade>(W->GetProductAmount(Key), W->GetProductGrade(Key)));
				}
			}
			bCaptured = true;
		}

		void Restore(UGameInstance* GI)
		{
			if (!bCaptured || !GI) { return; }
			if (UProductionOrderManager* O = GI->GetSubsystem<UProductionOrderManager>())
			{
				O->SetActiveOrders(Orders);
				O->SetNextOrderID(NextOrderID);
			}
			if (UResourceItemManager* R = GI->GetSubsystem<UResourceItemManager>()) { R->SetAllResources(Resources, true); }
			if (UWorldFactoryManager* F = GI->GetSubsystem<UWorldFactoryManager>()) { F->LoadFromSave(Factory); }
			if (UWorldMapManager* W = GI->GetSubsystem<UWorldMapManager>())
			{
				for (const FIntPoint& Key : W->GetAllProductKeys())
				{
					const int64 Have = W->GetProductAmount(Key);
					if (Have > 0) { W->ConsumeProduct(Key, Have, false); }
				}
				for (const TPair<FIntPoint, TPair<int64, EQualityGrade>>& Pair : Goods)
				{
					if (Pair.Value.Key > 0) { W->AddProduct(Pair.Key, Pair.Value.Key, Pair.Value.Value, false); }
				}
			}
			bCaptured = false;
		}
	};

	struct FRecipeProduct
	{
		ECompanyType Company = ECompanyType::None;
		int32 Index = 0;
		FText Name;
		TArray<TPair<EResourceType, int32>> Materials;
		int32 InputIndex = 0;
		int32 InputQty = 0;
	};

	struct FPopupFixtures
	{
		UGameInstance* GI = nullptr;
		UTableManagerSubsystem* Tables = nullptr;
		UProductionOrderManager* Orders = nullptr;
		UResourceItemManager* Resources = nullptr;
		UWorldFactoryManager* FactoryMgr = nullptr;
		UWorldMapManager* Warehouse = nullptr;
		ECountryType Country = ECountryType::None;
		FRecipeProduct Main;      // 재료 3~5종, 부품 입력 없음
		FRecipeProduct Other;     // 레일 둘째 줄(재료 부족 표시용) ― Main 에 없는 재료가 있으면 그걸 0 으로
		FRecipeProduct Longest;   // 가장 긴 제품명
		FRecipeProduct Most;      // 재료 칸이 가장 많은 제품(스크롤 장면)
		FRecipeProduct Part;      // 부품 입력(칩)이 있는 제품 ― 칩이 최대를 정하는 장면
		FText PartInputName;
		EResourceType OtherOnly = EResourceType::None;

		bool Resolve(UGameInstance* InGI, FString& OutError)
		{
			GI = InGI;
			Tables = GI->GetSubsystem<UTableManagerSubsystem>();
			Orders = GI->GetSubsystem<UProductionOrderManager>();
			Resources = GI->GetSubsystem<UResourceItemManager>();
			FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();
			Warehouse = GI->GetSubsystem<UWorldMapManager>();
			if (!Tables || !Orders || !Resources || !FactoryMgr || !Warehouse)
			{
				OutError = TEXT("매니저 서브시스템이 하나 이상 없습니다");
				return false;
			}
			bool bOk = false;
			const FCountryInfoTable Info = Tables->GetCountryInfo(ECountryType::Korea, bOk);
			if (!bOk || !Info.bSupportsFactory || Info.SupportedIndustries.Num() == 0)
			{
				OutError = TEXT("대한민국 공장 DT 행이 없거나 지원 산업이 비었습니다");
				return false;
			}
			Country = ECountryType::Korea;
			TArray<FRecipeProduct> All;
			for (const ECompanyType Industry : Info.SupportedIndustries)
			{
				for (const FProjectData& Proj : Tables->GetProjectsByCompanyType(Industry))
				{
					bool bRecipe = false;
					const FProductRecipeTable Recipe = Tables->GetProductRecipe(Industry, Proj.ProjectIndex, bRecipe);
					if (!bRecipe || Proj.ProjectName.IsEmpty()) { continue; }
					FRecipeProduct P;
					P.Company = Industry;
					P.Index = Proj.ProjectIndex;
					P.Name = Proj.ProjectName;
					Recipe.CollectMaterials(P.Materials);
					Recipe.GetProductInput(P.InputIndex, P.InputQty);
					if (P.Materials.Num() > 0) { All.Add(P); }
				}
			}
			int32 BestLen = 0;
			int32 BestCount = 0;
			for (const FRecipeProduct& P : All)
			{
				if (Main.Index == 0 && P.InputQty == 0 && P.Materials.Num() >= 3 && P.Materials.Num() <= 5) { Main = P; }
				const int32 Len = P.Name.ToString().Len();
				if (Len > BestLen) { BestLen = Len; Longest = P; }
				const int32 Count = P.Materials.Num() + (P.InputQty > 0 ? 1 : 0);
				if (Count > BestCount) { BestCount = Count; Most = P; }
				if (P.InputQty > 0 && (Part.Index == 0 || P.Materials.Num() < Part.Materials.Num())) { Part = P; }
			}
			for (const FRecipeProduct& P : All)
			{
				if (P.Index == Main.Index && P.Company == Main.Company) { continue; }
				for (const TPair<EResourceType, int32>& M : P.Materials)
				{
					const bool bInMain = Main.Materials.ContainsByPredicate([&M](const TPair<EResourceType, int32>& X) { return X.Key == M.Key; });
					if (!bInMain) { Other = P; OtherOnly = M.Key; break; }
				}
				if (Other.Index != 0) { break; }
			}
			if (Other.Index == 0)
			{
				for (const FRecipeProduct& P : All) { if (P.Index != Main.Index || P.Company != Main.Company) { Other = P; break; } }
			}
			if (Part.Index != 0)
			{
				bool bPartOk = false;
				PartInputName = Tables->GetProjectData(Part.Company, Part.InputIndex, bPartOk).ProjectName;
			}
			if (Main.Index == 0 || Other.Index == 0 || Longest.Index == 0 || Most.Index == 0 || Part.Index == 0 || PartInputName.IsEmpty())
			{
				OutError = FString::Printf(TEXT("픽스처 제품을 못 골랐습니다(재료 3~5종 %d, 둘째 %d)"), Main.Index, Other.Index);
				return false;
			}
			return true;
		}

		FProductionOrder MakeOrder(int32 Id, const FRecipeProduct& P, int32 Remaining, EQualityGrade Grade) const
		{
			FProductionOrder O;
			O.OrderID = Id;
			O.CompanyType = P.Company;
			O.ProjectIndex = P.Index;
			O.ProductName = P.Name.ToString();
			O.Quantity = Remaining;
			O.RemainingQuantity = Remaining;
			O.Grade = Grade;
			return O;
		}

		// 모든 원자재를 넉넉히(필요×100), 그다음 덮어쓸 것만 Overrides
		void SetMaterials(const TMap<EResourceType, int64>& Overrides)
		{
			TMap<EResourceType, int64> All = Resources->GetAllResources();
			for (const EResourceType Type : FProductRecipeTable::GetAllRawMaterialTypes()) { All.Add(Type, 100000); }
			for (const TPair<EResourceType, int64>& Pair : Overrides) { All.Add(Pair.Key, Pair.Value); }
			Resources->SetAllResources(All, true);
		}

		void SetEmptyFactory(const TMap<ECountryType, FCountryFactoryData>& Base)
		{
			TMap<ECountryType, FCountryFactoryData> Data = Base;
			FCountryFactoryData& D = Data.FindOrAdd(Country);
			D.ActiveLines.Reset();
			FactoryMgr->LoadFromSave(Data);
		}

		void StockInputs(const FRecipeProduct& P)
		{
			if (P.InputQty > 0)
			{
				Warehouse->AddProduct(MakeProductKey(P.Company, P.InputIndex), static_cast<int64>(P.InputQty) * 1000, EQualityGrade::B, false);
			}
		}
	};

	enum class EPopupScene : uint8 { Normal, NoLimit, MaxOne, LongName, Short, ManyMaterials, PartLimit, StartAndClose };

	struct FPopupSceneSpec
	{
		FString Label;
		EPopupScene Kind = EPopupScene::Normal;
	};

	class FProductionPopupCommand : public IAutomationLatentCommand
	{
	public:
		FProductionPopupCommand(FAutomationTestBase* InTest, const FPopupFixtures& InW)
			: Test(InTest), W(InW)
		{
			Scenes = {
				{ TEXT("normal"), EPopupScene::Normal }, { TEXT("nolimit"), EPopupScene::NoLimit },
				{ TEXT("max1"), EPopupScene::MaxOne }, { TEXT("longname"), EPopupScene::LongName },
				{ TEXT("short"), EPopupScene::Short }, { TEXT("manymats"), EPopupScene::ManyMaterials },
				{ TEXT("partlimit"), EPopupScene::PartLimit }, { TEXT("start"), EPopupScene::StartAndClose } };
			Backup.Capture(W.GI);
			Report = MakeShared<FJsonObject>();
			IFileManager::Get().MakeDirectory(*PopupDir(), true);
		}

		virtual ~FProductionPopupCommand() override
		{
			Popup.Reset();
			Backup.Restore(W.GI);
			FString Serialized;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
			FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
			FFileHelper::SaveStringToFile(Serialized, *(PopupDir() / TEXT("popup_geometry.json")), FFileHelper::EEncodingOptions::ForceUTF8);
		}

		virtual bool Update() override
		{
			if (SceneIndex >= Scenes.Num()) { return true; }
			if (!bWarmed)
			{
				// 새 프로세스 첫 오프스크린 렌더는 재질 배치가 빠진다(7b 실측) ― 한 장면을 먼저 버린다
				Seed(Scenes[0]);
				if (UProductionStartPopupWidget* P = MakePopup())
				{
					for (const int32 Width : PopupWidths)
					{
						FPStage Warm;
						if (Warm.Init(P->TakeWidget(), FIntPoint(Width, PopupHeight))) { Warm.Draw(6, nullptr); }
						Warm.Detach();
					}
				}
				Popup.Reset();
				bWarmed = true;
				return false;
			}
			RunScene(Scenes[SceneIndex]);
			++SceneIndex;
			return SceneIndex >= Scenes.Num();
		}

	private:
		static constexpr int32 MainOrderId = 7001;
		static constexpr int32 OtherOrderId = 7002;
		static constexpr int32 MainRemaining = 8;

		int32 ExpectedMax = 0;
		const FRecipeProduct* Selected = nullptr;

		void Seed(const FPopupSceneSpec& Spec)
		{
			Backup.Restore(W.GI);
			Backup.Capture(W.GI);
			W.SetEmptyFactory(Backup.Factory);
			Selected = &W.Main;
			if (Spec.Kind == EPopupScene::LongName) { Selected = &W.Longest; }
			if (Spec.Kind == EPopupScene::ManyMaterials) { Selected = &W.Most; }
			if (Spec.Kind == EPopupScene::PartLimit) { Selected = &W.Part; }
			const FRecipeProduct& P = *Selected;
			if (Spec.Kind == EPopupScene::PartLimit)
			{
				// 칩 2개분만 ― 원자재는 넉넉하니 칩이 최대(2)를 정한다
				W.Warehouse->AddProduct(MakeProductKey(P.Company, P.InputIndex), static_cast<int64>(P.InputQty) * 2, EQualityGrade::B, false);
			}
			else
			{
				W.StockInputs(P);
			}
			W.StockInputs(W.Other);
			TMap<EResourceType, int64> Overrides;
			const TPair<EResourceType, int32>& Cap = P.Materials[0];
			switch (Spec.Kind)
			{
			case EPopupScene::Normal:   Overrides.Add(Cap.Key, static_cast<int64>(Cap.Value) * 3); break;
			case EPopupScene::MaxOne:   Overrides.Add(Cap.Key, static_cast<int64>(Cap.Value) * 1 + FMath::Max(0, Cap.Value - 1)); break;
			case EPopupScene::Short:    for (const TPair<EResourceType, int32>& M : P.Materials) { Overrides.Add(M.Key, M.Key == Cap.Key ? FMath::Max(0, Cap.Value - 1) : 100000); } break;
			case EPopupScene::ManyMaterials: Overrides.Add(Cap.Key, static_cast<int64>(Cap.Value) * 2); break;
			default: break;
			}
			if (Spec.Kind == EPopupScene::Normal && W.OtherOnly != EResourceType::None) { Overrides.Add(W.OtherOnly, 0); }
			W.SetMaterials(Overrides);
			TArray<FProductionOrder> List;
			List.Add(W.MakeOrder(MainOrderId, P, MainRemaining, EQualityGrade::C));
			List.Add(W.MakeOrder(OtherOrderId, W.Other, 40, EQualityGrade::A));
			W.Orders->SetActiveOrders(List);
			EResourceType Bottleneck = EResourceType::None;
			bool bBound = false;
			ExpectedMax = W.Orders->GetMaxProducible(List[0], Bottleneck, bBound);
		}

		UProductionStartPopupWidget* MakePopup()
		{
			TSubclassOf<UUserWidget> Cls = W.Tables->GetWidgetClass(EWidgetType::ProductionStartPopup);
			UProductionStartPopupWidget* P = Cls ? CreateWidget<UProductionStartPopupWidget>(W.GI, Cls) : nullptr;
			if (!P) { Test->AddError(TEXT("생산 설정 창을 만들지 못했습니다(EWidgetType::ProductionStartPopup)")); return nullptr; }
			// 실제 경로(CountryDetailWidget::HandleEmptyLineStart)와 같은 순서: 초기화 → 화면 위젯 생성
			P->InitializeForCountry(W.Country, MainOrderId);
			Popup.Reset(P);
			return P;
		}

		template <typename T>
		T* Find(UUserWidget* Host, const TCHAR* Name) const
		{
			return Host ? Cast<T>(Host->GetWidgetFromName(FName(Name))) : nullptr;
		}

		static FString TextOf(const UTextBlock* T) { return T ? T->GetText().ToString() : FString(); }

		// 실제 뷰포트에 올리고(서랍과 같은 AddToViewport) 생산 시작 → 라인이 생기고 창이 닫힌다
		void RunStartAndClose(const FPopupSceneSpec& Spec)
		{
			FPChecker C;
			C.Test = Test;
			C.Scene = Spec.Label;
			UProductionStartPopupWidget* P = MakePopup();
			if (!P) { return; }
			P->AddToViewport(100);
			const int32 LinesBefore = W.FactoryMgr->GetActiveLineCount(W.Country);
			UCostActionButtonWidget* Accept = Cast<UCostActionButtonWidget>(P->GetWidgetFromName(TEXT("AcceptButton")));
			UButtonWidget* Inner = Accept ? Cast<UButtonWidget>(Accept->GetWidgetFromName(TEXT("Btn"))) : nullptr;
			C.True(TEXT("생산 시작 안쪽 버튼 = 표준 텍스트 버튼"), Inner != nullptr);
			C.True(TEXT("열린 뒤 창이 뷰포트에 있다"), P->IsInViewport());
			if (Inner) { Inner->SimulateClick(); }
			C.True(FString::Printf(TEXT("생산 시작 → 라인 %d → %d"), LinesBefore, W.FactoryMgr->GetActiveLineCount(W.Country)),
				W.FactoryMgr->GetActiveLineCount(W.Country) == LinesBefore + 1);
			C.True(TEXT("생산 시작 → 창이 닫힌다(뷰포트에서 빠짐)"), !P->IsInViewport());
			if (P->IsInViewport()) { P->RemoveFromParent(); }
			TSharedPtr<FJsonObject> SceneJson = MakeShared<FJsonObject>();
			SceneJson->SetNumberField(TEXT("checks"), C.Checks);
			SceneJson->SetNumberField(TEXT("fails"), C.Fails);
			Report->SetObjectField(Spec.Label, SceneJson);
			Test->AddInfo(FString::Printf(TEXT("[%s] 단언 %d · 실패 %d"), *C.Scene, C.Checks, C.Fails));
			Popup.Reset();
		}

		void RunScene(const FPopupSceneSpec& Spec)
		{
			Seed(Spec);
			if (Spec.Kind == EPopupScene::StartAndClose) { RunStartAndClose(Spec); return; }
			UProductionStartPopupWidget* P = MakePopup();
			if (!P) { return; }
			TSharedRef<SWidget> Slate = P->TakeWidget();
			TSharedPtr<FJsonObject> SceneJson = MakeShared<FJsonObject>();
			for (const int32 Width : PopupWidths)
			{
				FPChecker C;
				C.Test = Test;
				C.Scene = FString::Printf(TEXT("%s_%d"), *Spec.Label, Width);
				FPStage Stage;
				FImage Frame;
				const bool bDrawn = Stage.Init(Slate, FIntPoint(Width, PopupHeight)) && Stage.Draw(6, &Frame);
				C.True(TEXT("렌더 성공"), bDrawn);
				if (bDrawn)
				{
					PSavePNG(Frame, PopupDir() / (C.Scene + TEXT(".png")));
					CheckLayout(C, *P, Width, Spec);
					CheckState(C, *P, Spec);
					if (Spec.Kind == EPopupScene::Normal && Width == PopupWidths[0])
					{
						CheckInteractions(C, *P, Stage);
					}
				}
				TSharedPtr<FJsonObject> WidthJson = MakeShared<FJsonObject>();
				WidthJson->SetNumberField(TEXT("checks"), C.Checks);
				WidthJson->SetNumberField(TEXT("fails"), C.Fails);
				TArray<TSharedPtr<FJsonValue>> FailArray;
				for (const FString& Line : C.FailLines) { FailArray.Add(MakeShared<FJsonValueString>(Line)); }
				WidthJson->SetArrayField(TEXT("fail_lines"), FailArray);
				SceneJson->SetObjectField(FString::FromInt(Width), WidthJson);
				Test->AddInfo(FString::Printf(TEXT("[%s] 단언 %d · 실패 %d"), *C.Scene, C.Checks, C.Fails));
				Stage.Detach();
			}
			Report->SetObjectField(Spec.Label, SceneJson);
			Popup.Reset();
		}

		void CheckLayout(FPChecker& C, UProductionStartPopupWidget& P, int32 Width, const FPopupSceneSpec& Spec)
		{
			const FPBox Shell = PBoxOf(P.GetWidgetFromName(TEXT("ModalFrame")));
			C.True(FString::Printf(TEXT("셸 %s 가 화면 안"), *PBoxStr(Shell)),
				Shell.bValid && Shell.L >= -PopupTol && Shell.R() <= Width + PopupTol && Shell.T >= -PopupTol && Shell.B() <= PopupHeight + PopupTol);
			C.Near(TEXT("셸 가운데 = 화면 가운데"), Shell.L + Shell.W * 0.5, Width * 0.5, 2.0);
			const FPBox Plate = PBoxOf(P.GetWidgetFromName(TEXT("DetailRoot")));
			C.True(FString::Printf(TEXT("상세 판 %s 이 셸 안"), *PBoxStr(Plate)),
				Plate.bValid && Plate.L >= Shell.L && Plate.R() <= Shell.R() + PopupTol && Plate.B() <= Shell.B() + PopupTol);
			FPBox Inner = Plate;
			Inner.L += 32; Inner.T += 32; Inner.W -= 64; Inner.H -= 64;

			// 버튼 크기 = 버튼 자체
			const FPBox Accept = PBoxOf(P.GetWidgetFromName(TEXT("AcceptButton")));
			C.Near(TEXT("생산 시작 폭"), Accept.W, 560.0);
			C.Near(TEXT("생산 시작 높이"), Accept.H, 148.0);
			C.Near(TEXT("생산 시작 오른쪽 = 판 안쪽 오른쪽"), Accept.R(), Inner.R(), 2.0);
			const TCHAR* Steps[] = { TEXT("BulkBtn_x1"), TEXT("MinusButton"), TEXT("PlusButton"), TEXT("MaxJumpButton") };
			TArray<FPBox> StepBoxes;
			for (const TCHAR* Name : Steps)
			{
				const FPBox B = PBoxOf(P.GetWidgetFromName(Name));
				StepBoxes.Add(B);
				C.Near(FString::Printf(TEXT("%s 높이"), Name), B.H, 120.0);
				C.True(FString::Printf(TEXT("%s 폭 %.0f ≥ 120"), Name, B.W), B.bValid && B.W >= 120.0 - PopupTol);
			}
			const FPBox Qty = PBoxOf(P.GetWidgetFromName(TEXT("QuantityBox")));
			C.Near(TEXT("수량 칸 폭"), Qty.W, 220.0);
			C.Near(TEXT("수량 칸 높이"), Qty.H, 120.0);
			C.True(FString::Printf(TEXT("버튼 순서 최소 < − < 수량 < + < 최대 %s %s %s %s %s"), *PBoxStr(StepBoxes[0]), *PBoxStr(StepBoxes[1]),
				*PBoxStr(Qty), *PBoxStr(StepBoxes[2]), *PBoxStr(StepBoxes[3])),
				StepBoxes[0].R() <= StepBoxes[1].L && StepBoxes[1].R() <= Qty.L && Qty.R() <= StepBoxes[2].L && StepBoxes[2].R() <= StepBoxes[3].L);
			C.True(TEXT("1·10·50 묶음 버튼은 화면에 없다"),
				!PopupShown(P.GetWidgetFromName(TEXT("BulkBtn_x10"))) && !PopupShown(P.GetWidgetFromName(TEXT("BulkBtn_x50"))));

			const FPBox Band = PBoxOf(P.GetWidgetFromName(TEXT("LimitBand")));
			if (Band.bValid)
			{
				C.Near(TEXT("주황 띠 높이"), Band.H, 88.0);
				C.Near(TEXT("주황 띠 폭 = 판 안쪽 폭"), Band.W, Inner.W, 2.0);
			}

			// 판 안 덩어리 겹침 0 + 판 안쪽
			TArray<TPair<FString, FPBox>> Blocks;
			Blocks.Emplace(TEXT("머리(그림·이름)"), PBoxOf(P.GetWidgetFromName(TEXT("DetailHead"))));
			Blocks.Emplace(TEXT("재료 제목 줄"), PBoxOf(P.GetWidgetFromName(TEXT("MaterialLabelRow"))));
			Blocks.Emplace(TEXT("재료 칸"), PBoxOf(P.GetWidgetFromName(TEXT("MaterialScroll"))));
			Blocks.Emplace(TEXT("수량 줄"), PBoxOf(P.GetWidgetFromName(TEXT("QuantityRow"))));
			Blocks.Emplace(TEXT("주황 띠"), Band);
			Blocks.Emplace(TEXT("아래 줄"), PBoxOf(P.GetWidgetFromName(TEXT("BottomRow"))));
			for (int32 A = 0; A < Blocks.Num(); ++A)
			{
				const FPBox& BA = Blocks[A].Value;
				if (!BA.bValid) { if (A != 4) { C.True(Blocks[A].Key + TEXT(" 이 보인다"), false); } continue; }
				C.True(FString::Printf(TEXT("%s %s 가 판 안쪽"), *Blocks[A].Key, *PBoxStr(BA)),
					BA.L >= Inner.L - PopupTol && BA.R() <= Inner.R() + PopupTol && BA.T >= Inner.T - PopupTol && BA.B() <= Inner.B() + PopupTol);
				for (int32 K = A + 1; K < Blocks.Num(); ++K)
				{
					C.True(FString::Printf(TEXT("%s ↔ %s 겹침 0"), *Blocks[A].Key, *Blocks[K].Key), POverlap(BA, Blocks[K].Value) <= 0.0);
				}
			}

			// 이름 글자가 판 밖으로 안 나간다(긴 제품명)
			UTextBlock* Name = Find<UTextBlock>(&P, TEXT("DetailNameText"));
			const FPBox NameInk = PTextInk(Name);
			C.True(FString::Printf(TEXT("제품명 잉크 %s 가 판 안쪽"), *PBoxStr(NameInk)), NameInk.bValid && NameInk.R() <= Inner.R() + PopupTol);
			C.True(TEXT("제품명 ↔ 그림 겹침 0"), POverlap(NameInk, PBoxOf(P.GetWidgetFromName(TEXT("DetailCoverPlate")))) <= 0.0);
			if (Name) { C.Near(TEXT("제품명 pt"), Name->GetFont().Size, 44.0, 0.01); }
			UTextBlock* QtyText = Find<UTextBlock>(&P, TEXT("QuantityText"));
			if (QtyText) { C.Near(TEXT("수량 pt"), QtyText->GetFont().Size, 45.0, 0.01); }
			const FPBox Summary = PTextInk(Find<UTextBlock>(&P, TEXT("SummaryText")));
			C.True(FString::Printf(TEXT("총 시간 %s ↔ 최대 버튼 겹침 0"), *PBoxStr(Summary)), POverlap(Summary, StepBoxes[3]) <= 0.0 && Summary.L >= StepBoxes[3].R());
			const FPBox Chain = PTextInk(Find<UTextBlock>(&P, TEXT("ChainTimeText")));
			C.True(TEXT("아래 안내 ↔ 생산 시작 겹침 0"), POverlap(Chain, Accept) <= 0.0);

			// 재료 카드: 높이 170, 3열, 겹침 0, 필요 40pt · 보유 22pt
			UPanelWidget* Grid = Find<UPanelWidget>(&P, TEXT("MaterialGridBox"));
			UScrollBox* MatScroll = Find<UScrollBox>(&P, TEXT("MaterialScroll"));
			const FPBox ScrollBox = PBoxOf(MatScroll);
			TArray<FPBox> Cards;
			int32 CardIndex = 0;
			for (UWidget* Child : Grid ? Grid->GetAllChildren() : TArray<UWidget*>())
			{
				UMaterialReqRowWidget* Row = Cast<UMaterialReqRowWidget>(Child);
				if (!Row || !PopupShown(Row)) { continue; }
				const FPBox Card = PBoxOf(Row->GetWidgetFromName(TEXT("CardLip")));
				const bool bOnScreen = Card.bValid && Card.B() <= ScrollBox.B() + PopupTol;
				if (!bOnScreen) { ++CardIndex; continue; }   // 스크롤 아래 칸은 아직 안 그려졌다
				Cards.Add(Card);
				C.Near(FString::Printf(TEXT("재료 칸%d 높이(턱 포함)"), CardIndex + 1), Card.H, 175.0);
				UTextBlock* Need = Find<UTextBlock>(Row, TEXT("NeedText"));
				UTextBlock* Have = Find<UTextBlock>(Row, TEXT("HaveText"));
				if (Need) { C.Near(TEXT("필요 pt"), Need->GetFont().Size, 40.0, 0.01); }
				if (Have) { C.Near(TEXT("보유 pt"), Have->GetFont().Size, 22.0, 0.01); }
				const FPBox NeedInk = PTextInk(Need);
				const FPBox HaveInk = PTextInk(Have);
				C.True(FString::Printf(TEXT("재료 칸%d 필요 %s 이 보유 %s 보다 왼쪽·겹침 0"), CardIndex + 1, *PBoxStr(NeedInk), *PBoxStr(HaveInk)),
					NeedInk.bValid && HaveInk.bValid && NeedInk.R() <= HaveInk.L && POverlap(NeedInk, HaveInk) <= 0.0);
				const FPBox NameBox = PTextInk(Find<UTextBlock>(Row, TEXT("NameText")));
				const FPBox Lack = PTextInk(Find<UTextBlock>(Row, TEXT("LackText")));
				C.True(TEXT("재료 이름 ↔ 부족 글 겹침 0"), POverlap(NameBox, Lack) <= 0.0);
				C.True(FString::Printf(TEXT("재료 칸%d %s 가 칸 안"), CardIndex + 1, *PBoxStr(NameBox)), NameBox.bValid && NameBox.R() <= Card.R() + PopupTol);
				++CardIndex;
			}
			C.True(FString::Printf(TEXT("재료 칸이 보인다(%d)"), Cards.Num()), Cards.Num() > 0);
			for (int32 A = 0; A < Cards.Num(); ++A)
			{
				for (int32 K = A + 1; K < Cards.Num(); ++K)
				{
					C.True(FString::Printf(TEXT("재료 칸%d ↔ %d 겹침 0"), A + 1, K + 1), POverlap(Cards[A], Cards[K]) <= 0.0);
				}
			}
			if (Cards.Num() >= 3)
			{
				C.Near(TEXT("재료 첫 줄 = 3칸(셋째 칸 같은 높이)"), Cards[2].T, Cards[0].T, 1.0);
				C.Near(TEXT("재료 칸 간격"), Cards[1].L - Cards[0].R(), 16.0, 1.5);
			}
			if (Spec.Kind == EPopupScene::ManyMaterials)
			{
				C.True(FString::Printf(TEXT("재료가 많으면 재료 칸만 스크롤(넘침 %.1f)"), MatScroll ? MatScroll->GetScrollOffsetOfEnd() : -1.f),
					MatScroll && MatScroll->GetScrollOffsetOfEnd() > 0.5f);
			}

			// 레일 카드: 높이 196, 그림 124, 선택 링
			UPanelWidget* Rail = Find<UPanelWidget>(&P, TEXT("OrderRailBox"));
			int32 RailIndex = 0;
			for (UWidget* Child : Rail ? Rail->GetAllChildren() : TArray<UWidget*>())
			{
				UProductionOrderRowWidget* Row = Cast<UProductionOrderRowWidget>(Child);
				if (!Row) { continue; }
				const FPBox Face = PBoxOf(Row->GetWidgetFromName(TEXT("RowFace")));
				C.Near(FString::Printf(TEXT("주문 카드%d 높이"), RailIndex + 1), Face.H, 196.0);
				const FPBox Cover = PBoxOf(Row->GetWidgetFromName(TEXT("CoverPlate")));
				C.Near(FString::Printf(TEXT("주문 카드%d 그림 판"), RailIndex + 1), Cover.W, 124.0);
				const FPBox RowName = PTextInk(Find<UTextBlock>(Row, TEXT("NameText")));
				const FPBox Meta = PTextInk(Find<UTextBlock>(Row, TEXT("MetaText")));
				C.True(FString::Printf(TEXT("주문 카드%d 글자 %s %s 가 카드 안·겹침 0"), RailIndex + 1, *PBoxStr(RowName), *PBoxStr(Meta)),
					RowName.bValid && Meta.bValid && RowName.R() <= Face.R() + PopupTol && Meta.R() <= Face.R() + PopupTol && POverlap(RowName, Meta) <= 0.0);
				C.True(FString::Printf(TEXT("주문 카드%d 선택 링 = 고른 주문만"), RailIndex + 1),
					PopupShown(Row->GetWidgetFromName(TEXT("SelectRing"))) == (Row->GetOrderID() == MainOrderId));
				++RailIndex;
			}
			C.Near(TEXT("주문 카드 수"), RailIndex, 2.0, 0.0);
		}

		void CheckState(FPChecker& C, UProductionStartPopupWidget& P, const FPopupSceneSpec& Spec)
		{
			UTextBlock* QtyText = Find<UTextBlock>(&P, TEXT("QuantityText"));
			const int32 Want = FMath::Max(1, ExpectedMax);
			C.True(FString::Printf(TEXT("처음 수량 「%s」 = 최대 %d"), *TextOf(QtyText), Want), TextOf(QtyText) == FString::FromInt(Want));
			UTextBlock* Band = Find<UTextBlock>(&P, TEXT("CapacityText"));
			const bool bBand = PopupShown(P.GetWidgetFromName(TEXT("LimitBand")));
			const bool bExpectBand = Spec.Kind != EPopupScene::NoLimit && Spec.Kind != EPopupScene::LongName;
			// 재료 종 수 = 원자재 + 부품 입력. 첫 줄(3칸) 밖이면 「+N종 더 있습니다」
			const int32 Cells = Selected ? Selected->Materials.Num() + (Selected->InputQty > 0 ? 1 : 0) : 0;
			UTextBlock* MatLabel = Find<UTextBlock>(&P, TEXT("MaterialLabelText"));
			C.True(FString::Printf(TEXT("재료 제목 「%s」 = 「필요 재료 %d종 (1개 만들 때)」"), *TextOf(MatLabel), Cells),
				TextOf(MatLabel).StartsWith(FString::Printf(TEXT("필요 재료 %d종 (1개 만들 때)"), Cells)));
			UTextBlock* More = Find<UTextBlock>(&P, TEXT("MaterialMoreText"));
			const bool bMore = PopupShown(More);
			C.True(FString::Printf(TEXT("「+N종」 표시 %s 「%s」 (칸 %d)"), bMore ? TEXT("보임") : TEXT("숨김"), *TextOf(More), Cells),
				Cells > 3 ? (bMore && TextOf(More) == FString::Printf(TEXT("+%d종 더 있습니다"), Cells - 3)) : !bMore);
			if (bMore)
			{
				C.True(TEXT("「+N종」 ↔ 재료 제목 겹침 0"), POverlap(PTextInk(More), PTextInk(MatLabel)) <= 0.0);
			}
			C.True(FString::Printf(TEXT("주황 띠 %s (기대 %s) 「%s」"), bBand ? TEXT("펴짐") : TEXT("접힘"), bExpectBand ? TEXT("펴짐") : TEXT("접힘"), *TextOf(Band)),
				bBand == bExpectBand);
			if (bExpectBand && Spec.Kind != EPopupScene::Short)
			{
				C.True(FString::Printf(TEXT("띠 문장 「%s」 = 「…개뿐이라 최대 %d개까지 만들 수 있습니다」"), *TextOf(Band), ExpectedMax),
					TextOf(Band).Contains(FString::Printf(TEXT("개뿐이라 최대 %d개까지 만들 수 있습니다"), ExpectedMax)));
			}
			if (Spec.Kind == EPopupScene::Short)
			{
				C.True(FString::Printf(TEXT("1개도 못 만들 때 띠 「%s」"), *TextOf(Band)), TextOf(Band).Contains(TEXT("1개도 만들 수 없습니다")));
			}
			UTextBlock* Label = Find<UTextBlock>(P.GetWidgetFromName(TEXT("AcceptButton")) ? Cast<UUserWidget>(P.GetWidgetFromName(TEXT("AcceptButton"))) : nullptr, TEXT("BtnText"));
			const FString WantLabel = Spec.Kind == EPopupScene::Short ? TEXT("재료 부족") : TEXT("생산 시작");
			C.True(FString::Printf(TEXT("생산 시작 라벨 「%s」 = 「%s」"), *TextOf(Label), *WantLabel), TextOf(Label) == WantLabel);
			UTextBlock* Min = Find<UTextBlock>(Cast<UUserWidget>(P.GetWidgetFromName(TEXT("BulkBtn_x1"))), TEXT("ButtonText"));
			UTextBlock* Max = Find<UTextBlock>(Cast<UUserWidget>(P.GetWidgetFromName(TEXT("MaxJumpButton"))), TEXT("ButtonText"));
			C.True(FString::Printf(TEXT("버튼 라벨 「%s」「%s」 = 최소·최대"), *TextOf(Min), *TextOf(Max)), TextOf(Min) == TEXT("최소") && TextOf(Max) == TEXT("최대"));
			UWidget* MinBtn = P.GetWidgetFromName(TEXT("BulkBtn_x1"));
			UWidget* MaxBtn = P.GetWidgetFromName(TEXT("MaxJumpButton"));
			C.True(TEXT("최소 = 수량이 1보다 클 때만 누를 수 있다"), MinBtn && MinBtn->GetIsEnabled() == (Want > 1));
			C.True(TEXT("최대 = 지금 최대가 아닐 때만 누를 수 있다"), MaxBtn && !MaxBtn->GetIsEnabled());

			// 최대를 정하는 재료 = 첫 칸 + 주황 보유
			UPanelWidget* Grid = Find<UPanelWidget>(&P, TEXT("MaterialGridBox"));
			UMaterialReqRowWidget* First = nullptr;
			for (UWidget* Child : Grid ? Grid->GetAllChildren() : TArray<UWidget*>())
			{
				if ((First = Cast<UMaterialReqRowWidget>(Child)) != nullptr) { break; }
			}
			UTextBlock* FirstHave = Find<UTextBlock>(First, TEXT("HaveText"));
			UTextBlock* FirstNeed = Find<UTextBlock>(First, TEXT("NeedText"));
			UProgressBar* FirstBar = Find<UProgressBar>(First, TEXT("FillBar"));
			C.True(FString::Printf(TEXT("보유 글 「%s」 = 「보유 N」"), *TextOf(FirstHave)), TextOf(FirstHave).StartsWith(TEXT("보유 ")));
			if (Selected && Selected->Materials.Num() > 0 && FirstNeed && Spec.Kind != EPopupScene::NoLimit && Spec.Kind != EPopupScene::LongName)
			{
				const int32 CapNeed = Spec.Kind == EPopupScene::PartLimit ? Selected->InputQty : Selected->Materials[0].Value;
				C.True(FString::Printf(TEXT("첫 칸 필요 「%s」 = 최대를 정한 칸 1개당 %d"), *TextOf(FirstNeed), CapNeed),
					TextOf(FirstNeed) == FString::FromInt(CapNeed));
				if (Spec.Kind == EPopupScene::PartLimit)
				{
					const FString PartName = W.PartInputName.ToString();
					const FString FirstName = TextOf(Find<UTextBlock>(First, TEXT("NameText")));
					C.True(FString::Printf(TEXT("칩이 최대를 정하면 첫 칸 = 부품 「%s」(기대 「%s」)"), *FirstName, *PartName), FirstName == PartName);
					C.True(FString::Printf(TEXT("칩 병목 띠 「%s」 가 부품 이름을 말한다"), *TextOf(Band)), TextOf(Band).StartsWith(PartName));
				}
				const FLinearColor HaveInk = FirstHave ? FirstHave->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::Black;
				C.True(FString::Printf(TEXT("최대를 정한 재료 보유 글 = 주황/빨강 계열(%.2f,%.2f,%.2f)"), HaveInk.R, HaveInk.G, HaveInk.B),
					HaveInk.R > HaveInk.G * 2.0f && HaveInk.R > HaveInk.B * 4.0f);
				const FLinearColor Fill = FirstBar ? FirstBar->GetFillColorAndOpacity() : FLinearColor::Black;
				C.True(FString::Printf(TEXT("최대를 정한 재료 막대 = 주황/빨강(%.2f,%.2f,%.2f)"), Fill.R, Fill.G, Fill.B), Fill.R > Fill.B * 4.0f);
			}
			if (Spec.Kind == EPopupScene::NoLimit && FirstBar)
			{
				const FLinearColor Fill = FirstBar->GetFillColorAndOpacity();
				C.True(FString::Printf(TEXT("여유 재료 막대 = 파랑(%.2f,%.2f,%.2f)"), Fill.R, Fill.G, Fill.B), Fill.B > Fill.R * 3.0f);
			}
			// 레일 둘째 줄: 재료가 0 이면 「재료 부족」
			if (Spec.Kind == EPopupScene::Normal && W.OtherOnly != EResourceType::None)
			{
				UPanelWidget* Rail = Find<UPanelWidget>(&P, TEXT("OrderRailBox"));
				for (UWidget* Child : Rail ? Rail->GetAllChildren() : TArray<UWidget*>())
				{
					UProductionOrderRowWidget* Row = Cast<UProductionOrderRowWidget>(Child);
					if (!Row || Row->GetOrderID() != OtherOrderId) { continue; }
					C.True(FString::Printf(TEXT("레일 둘째 줄 「%s」 = 재료 부족"), *TextOf(Find<UTextBlock>(Row, TEXT("MetaText")))),
						TextOf(Find<UTextBlock>(Row, TEXT("MetaText"))) == TEXT("재료 부족"));
				}
			}
			UTextBlock* Heading = Find<UTextBlock>(&P, TEXT("OrderCountText"));
			C.True(FString::Printf(TEXT("레일 소제목 「%s」 = 「주문 2건 · … 공장 1번 라인」"), *TextOf(Heading)),
				TextOf(Heading).StartsWith(TEXT("주문 2건 · ")) && TextOf(Heading).EndsWith(TEXT("공장 1번 라인")));
		}

		// 최소 → 1, 최대 → 최대, + 로 넘기면 회색 「재료 부족」 + 부족 글
		void CheckInteractions(FPChecker& C, UProductionStartPopupWidget& P, FPStage& Stage)
		{
			UButtonWidget* Min = Cast<UButtonWidget>(P.GetWidgetFromName(TEXT("BulkBtn_x1")));
			UButtonWidget* Max = Cast<UButtonWidget>(P.GetWidgetFromName(TEXT("MaxJumpButton")));
			UButtonWidget* Plus = Cast<UButtonWidget>(P.GetWidgetFromName(TEXT("PlusButton")));
			UTextBlock* QtyText = Find<UTextBlock>(&P, TEXT("QuantityText"));
			if (!Min || !Max || !Plus) { C.True(TEXT("수량 버튼 4종이 표준 텍스트 버튼"), false); return; }
			UButtonWidget* Minus = Cast<UButtonWidget>(P.GetWidgetFromName(TEXT("MinusButton")));
			auto FirstCardBar = [this, &P]() -> UProgressBar*
			{
				UPanelWidget* Grid = Find<UPanelWidget>(&P, TEXT("MaterialGridBox"));
				for (UWidget* Child : Grid ? Grid->GetAllChildren() : TArray<UWidget*>())
				{
					if (UMaterialReqRowWidget* Row = Cast<UMaterialReqRowWidget>(Child)) { return Find<UProgressBar>(Row, TEXT("FillBar")); }
				}
				return nullptr;
			};
			Min->SimulateClick();
			C.True(FString::Printf(TEXT("최소 → 「%s」 = 1"), *TextOf(QtyText)), TextOf(QtyText) == TEXT("1"));
			// 막대 = 필요×수량÷보유: 최대를 정한 칸은 보유 = 필요×최대 라 수량 1 에서 1/최대
			UProgressBar* Bar1 = FirstCardBar();
			C.Near(TEXT("수량 1 의 첫 칸 막대"), Bar1 ? Bar1->GetPercent() : -1.0, 1.0 / FMath::Max(1, ExpectedMax), 0.01);
			C.True(TEXT("수량 1 이면 − 를 못 누른다"), Minus && !Minus->GetIsEnabled());
			if (Minus) { Minus->SimulateClick(); }
			C.True(FString::Printf(TEXT("− 를 눌러도 1 아래로 안 내려간다 「%s」"), *TextOf(QtyText)), TextOf(QtyText) == TEXT("1"));
			// 다른 주문으로 갔다 오면 수량이 그 주문의 최대로 다시 잡힌다
			UProductionOrderRowWidget* OtherRow = nullptr;
			UProductionOrderRowWidget* MainRow = nullptr;
			if (UPanelWidget* Rail = Find<UPanelWidget>(&P, TEXT("OrderRailBox")))
			{
				for (UWidget* Child : Rail->GetAllChildren())
				{
					if (UProductionOrderRowWidget* Row = Cast<UProductionOrderRowWidget>(Child))
					{
						if (Row->GetOrderID() == OtherOrderId) { OtherRow = Row; } else { MainRow = Row; }
					}
				}
			}
			if (OtherRow && MainRow)
			{
				EResourceType OtherBottleneck = EResourceType::None;
				bool bOtherBound = false;
				FProductionOrder OtherOrder;
				for (const FProductionOrder& O : W.Orders->GetAllOrders()) { if (O.OrderID == OtherOrderId) { OtherOrder = O; } }
				const int32 OtherMax = FMath::Max(1, W.Orders->GetMaxProducible(OtherOrder, OtherBottleneck, bOtherBound));
				OtherRow->OnOrderRowClicked.ExecuteIfBound(OtherOrderId);
				C.True(FString::Printf(TEXT("다른 주문 → 수량 「%s」 = 그 주문의 최대 %d"), *TextOf(QtyText), OtherMax), TextOf(QtyText) == FString::FromInt(OtherMax));
				MainRow->OnOrderRowClicked.ExecuteIfBound(MainOrderId);
				C.True(FString::Printf(TEXT("다시 고른 주문 → 수량 「%s」 = 최대 %d(1 이 아니라)"), *TextOf(QtyText), ExpectedMax),
					TextOf(QtyText) == FString::FromInt(ExpectedMax));
			}
			else
			{
				C.True(TEXT("레일에 주문 카드 2장"), false);
			}
			Min->SimulateClick();
			Max->SimulateClick();
			UProgressBar* BarMax = FirstCardBar();
			C.Near(TEXT("최대 수량의 첫 칸 막대"), BarMax ? BarMax->GetPercent() : -1.0, 1.0, 0.01);
			C.True(FString::Printf(TEXT("최대 → 「%s」 = %d"), *TextOf(QtyText), ExpectedMax), TextOf(QtyText) == FString::FromInt(ExpectedMax));
			Plus->SimulateClick();
			C.True(FString::Printf(TEXT("+ 로 최대 넘김 → 「%s」 = %d"), *TextOf(QtyText), ExpectedMax + 1), TextOf(QtyText) == FString::FromInt(ExpectedMax + 1));
			UTextBlock* Label = Find<UTextBlock>(Cast<UUserWidget>(P.GetWidgetFromName(TEXT("AcceptButton"))), TEXT("BtnText"));
			C.True(FString::Printf(TEXT("넘기면 생산 시작 라벨 「%s」 = 재료 부족"), *TextOf(Label)), TextOf(Label) == TEXT("재료 부족"));
			UTextBlock* MatLabel = Find<UTextBlock>(&P, TEXT("MaterialLabelText"));
			C.True(FString::Printf(TEXT("넘기면 재료 제목 「%s」 에 부족 종 수가 글로"), *TextOf(MatLabel)), TextOf(MatLabel).Contains(TEXT("종 부족")));
			FImage Over;
			if (Stage.Draw(4, &Over)) { PSavePNG(Over, PopupDir() / (C.Scene + TEXT("_over.png"))); }
			Max->SimulateClick();
		}

		FAutomationTestBase* Test;
		FPopupFixtures W;
		TArray<FPopupSceneSpec> Scenes;
		FPopupBackup Backup;
		TStrongObjectPtr<UProductionStartPopupWidget> Popup;
		TSharedPtr<FJsonObject> Report;
		int32 SceneIndex = 0;
		bool bWarmed = false;
	};
}

// ClientContext ― 격리 -game 러너에서만 돈다(플레이 월드가 필요하다)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasProductionPopupLiveTest, "CGR.Overseas.DrawerLive.ProductionPopup",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FOverseasProductionPopupLiveTest::RunTest(const FString& Parameters)
{
	using namespace CGRPopupLive;
	UWorld* PlayWorld = PopupPlayWorld();
	UGameInstance* GI = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
	if (!FApp::CanEverRender() || !GI)
	{
		AddError(TEXT("렌더 가능한 플레이 월드(-game)가 필요합니다 ― 값이 전부 0 이면 겹침 단언이 조용히 통과합니다."));
		return false;
	}
	FPopupFixtures W;
	FString Error;
	if (!W.Resolve(GI, Error)) { AddError(Error); return false; }
	AddInfo(FString::Printf(TEXT("픽스처: 주 제품 「%s」(재료 %d) · 둘째 「%s」 · 최장 「%s」 · 최다 「%s」(%d칸)"),
		*W.Main.Name.ToString(), W.Main.Materials.Num(), *W.Other.Name.ToString(), *W.Longest.Name.ToString(),
		*W.Most.Name.ToString(), W.Most.Materials.Num() + (W.Most.InputQty > 0 ? 1 : 0)));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FProductionPopupCommand>(this, W));
	return true;
}

#endif
