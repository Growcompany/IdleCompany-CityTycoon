// 국가 서랍 실기 검증 ― 격리 -game 에서 진짜 월드맵 패널로 서랍을 열고 잰다(디자이너 렌더가 못 보는 것: 매니저 상태·토글 그룹·스크롤 막대 이벤트·입력 경로).
// 세이브 안전: 매니저 상태를 메모리에서만 바꾸고 시험 끝에 되돌린다. 격리 러너는 저장 자체를 막는다(CF1).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Panel/WorldMapPanelWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Panel/BuildOpenWidget.h"
#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Element/Trade/WorldFactoryLineWidget.h"
#include "UI/Element/Trade/MineLineWidget.h"
#include "UI/Element/Trade/DrawerInfoRowWidget.h"
#include "UI/Element/Trade/DrawerGoodsTileWidget.h"
#include "UI/Element/Trade/DrawerHeroRowWidget.h"
#include "UI/Element/Trade/DrawerPageToggleWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/UIBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/MineManager.h"
#include "Manager/TradeOrderManager.h"
#include "Manager/WorldMapManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/TradePort.h"
#include "Player/MainMapPlayerController.h"
#include "Table/CountryInfoTable.h"
#include "Table/ProjectDataTable.h"
#include "Table/ResourceInfo.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Data/MineUpgradeData.h"
#include "Data/WorldMapTypes.h"
#include "Enum/QualityGrade.h"
#include "Enum/WidgetType.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/UniformGridPanel.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
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
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 형제 시험 파일과 재정의 충돌
namespace CGRDrawerLive
{
	static constexpr int32 CaptureWidths[] = { 2304, 3120 };
	static constexpr int32 CaptureHeight = 1440;
	static constexpr double Tol = 1.0;

	// 진행 기록 원칙과 같은 곳 ― 격리 -game 의 Saved 는 실행마다 따로라 프로젝트 Saved 에 남긴다
	static FString LiveDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Saved/WorldMapUIOverhaul/Task7/live"));
	}

	static UWorld* FindPlayWorld()
	{
		if (!GEngine) { return nullptr; }
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == Wanted && Context.World() && Context.World()->GetGameInstance())
				{
					return Context.World();
				}
			}
		}
		return nullptr;
	}

	static bool CallNoArg(UObject* Target, const TCHAR* FunctionName)
	{
		UFunction* Fn = Target ? Target->FindFunction(FName(FunctionName)) : nullptr;
		if (!Fn || Fn->NumParms != 0) { return false; }
		Target->ProcessEvent(Fn, nullptr);
		return true;
	}

	static bool ClickPin(UWorldMapPanelWidget* Panel, ECountryType Country)
	{
		UCountryNameWidget* Pin = Panel ? Panel->GetMedallions().FindRef(Country) : nullptr;
		if (!Pin) { return false; }
		// 해금은 세이브를 건드리지 않고 이 인스턴스에서만 연다
		Pin->SetLockState(false);
		return CallNoArg(Pin, TEXT("HandleButtonClicked"));
	}

	static bool DisableAutoActivate(UUserWidget* Widget)
	{
		FBoolProperty* Prop = Widget ? FindFProperty<FBoolProperty>(Widget->GetClass(), TEXT("bAutoActivate")) : nullptr;
		if (!Prop) { return false; }
		Prop->SetPropertyValue_InContainer(Widget, false);
		return true;
	}

	static UCountryDetailWidget* ActiveDetail(UWorldMapPanelWidget* Panel)
	{
		return Panel ? Cast<UCountryDetailWidget>(Panel->GetActiveDrawerWidget()) : nullptr;
	}

	template <typename T>
	static T* Named(UUserWidget* Host, const TCHAR* Name)
	{
		return Host ? Cast<T>(Host->GetWidgetFromName(FName(Name))) : nullptr;
	}

	// 자기·조상 어디든 접혔거나 꺼진 스위처 페이지면 「안 보임」 ― 그 지오메트리는 마지막으로 그려진 때의 낡은 값이다
	static bool IsShown(const UWidget* Widget)
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

	struct FBox2
	{
		bool bValid = false;
		double L = 0, T = 0, W = 0, H = 0;
		double R() const { return L + W; }
		double B() const { return T + H; }
		FVector2D Center() const { return FVector2D(L + W * 0.5, T + H * 0.5); }
	};

	// 레이아웃 박스(렌더 변형 제외) ― 펀치 애니가 끼어도 슬롯 크기는 그대로 잰다
	static FBox2 BoxOf(const UWidget* Widget)
	{
		FBox2 Out;
		if (!Widget || !IsShown(Widget)) { return Out; }
		const FGeometry& Geo = Widget->GetCachedGeometry();
		const FVector2D Pos = Geo.GetAbsolutePosition();
		const FVector2D Size = Geo.GetAbsoluteSize();
		Out.bValid = Size.X > 0.5 && Size.Y > 0.5;
		Out.L = Pos.X; Out.T = Pos.Y; Out.W = Size.X; Out.H = Size.Y;
		return Out;
	}

	static double Overlap(const FBox2& A, const FBox2& B)
	{
		if (!A.bValid || !B.bValid) { return 0.0; }
		const double OW = FMath::Min(A.R(), B.R()) - FMath::Max(A.L, B.L);
		const double OH = FMath::Min(A.B(), B.B()) - FMath::Max(A.T, B.T);
		return (OW > 0.5 && OH > 0.5) ? OW * OH : 0.0;
	}

	static FString BoxStr(const FBox2& B)
	{
		return B.bValid ? FString::Printf(TEXT("(%.0f,%.0f %.0fx%.0f)"), B.L, B.T, B.W, B.H) : TEXT("(none)");
	}

	static FLinearColor Ink(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	static bool SameInk(const FLinearColor& A, const FLinearColor& B)
	{
		return FMath::Abs(A.R - B.R) < 0.01f && FMath::Abs(A.G - B.G) < 0.01f && FMath::Abs(A.B - B.B) < 0.01f;
	}

	// 오프스크린 무대 ― 가상 창 + 히트 격자를 유지해 렌더 뒤에도 같은 지오메트리를 잴 수 있다
	struct FStage
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
			// raw target ― true target 은 하드웨어 sRGB 쓰기가 Slate 셰이더 감마와 겹친다(TableRender 와 같은 이유)
			Target.Reset(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
			return Target.IsValid();
		}

		bool Draw(int32 Passes, float Delta, FImage* OutImage)
		{
			if (!Window.IsValid() || !Target.IsValid()) { return false; }
			const FVector2D DrawSize(Size.X, Size.Y);
			const FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize, FSlateLayoutTransform(1.f));
			for (int32 Pass = 0; Pass < Passes; ++Pass)
			{
				const float PassDelta = Pass == 0 ? Delta : 0.016f;
				const FPaintArgs PaintArgs(nullptr, *Grid, FVector2D::ZeroVector, FApp::GetCurrentTime(), PassDelta);
				Renderer->DrawWindow(PaintArgs, Target.Get(), Window.ToSharedRef(), WindowGeometry,
					WindowGeometry.GetLayoutBoundingRect(), PassDelta);
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

	static FLinearColor MeanColor(const FImage& Frame, double L, double T, double R, double B)
	{
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(L), 0, Frame.SizeX - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(T), 0, Frame.SizeY - 1);
		const int32 X1 = FMath::Clamp(FMath::CeilToInt(R), X0 + 1, Frame.SizeX);
		const int32 Y1 = FMath::Clamp(FMath::CeilToInt(B), Y0 + 1, Frame.SizeY);
		const FColor* Pixels = reinterpret_cast<const FColor*>(Frame.RawData.GetData());
		double SR = 0, SG = 0, SB = 0;
		int64 N = 0;
		for (int32 Y = Y0; Y < Y1; ++Y)
		{
			for (int32 X = X0; X < X1; ++X)
			{
				const FColor& C = Pixels[static_cast<int64>(Y) * Frame.SizeX + X];
				SR += C.R; SG += C.G; SB += C.B; ++N;
			}
		}
		return N > 0 ? FLinearColor(SR / N, SG / N, SB / N, 255.f) : FLinearColor::Black;
	}

	static bool SavePNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	// ── 위젯 지오메트리 덤프(중첩 UserWidget 안까지) ──
	static void DumpTree(UUserWidget* Host, const FString& Prefix, TArray<TSharedPtr<FJsonValue>>& Out, int32 Depth = 0)
	{
		if (!Host || !Host->WidgetTree || Depth > 6) { return; }
		Host->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			const FBox2 B = BoxOf(Widget);
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			const FString Name = Prefix + Widget->GetName();
			Obj->SetStringField(TEXT("name"), Name);
			Obj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
			Obj->SetBoolField(TEXT("shown"), IsShown(Widget));
			Obj->SetNumberField(TEXT("x"), B.L); Obj->SetNumberField(TEXT("y"), B.T);
			Obj->SetNumberField(TEXT("w"), B.W); Obj->SetNumberField(TEXT("h"), B.H);
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Obj->SetStringField(TEXT("text"), Text->GetText().ToString());
				Obj->SetNumberField(TEXT("font"), Text->GetFont().Size);
			}
			Out.Add(MakeShared<FJsonValueObject>(Obj));
			if (UUserWidget* Child = Cast<UUserWidget>(Widget))
			{
				DumpTree(Child, Name + TEXT("/"), Out, Depth + 1);
			}
		});
	}

	// ── 매니저 상태 백업/복원 ──
	struct FFixtureBackup
	{
		bool bCaptured = false;
		TMap<ECountryType, FCountryFactoryData> Factory;
		TMap<ECountryType, FCountryMineData> Mine;
		TArray<FTradeOrder> Orders;
		int32 NextOrderId = 1;
		int32 Combo = 0;
		TMap<FIntPoint, TPair<int64, EQualityGrade>> Goods;
		TMap<EResourceType, int64> Resources;
		TArray<FName> PendingKeys;

		void Capture(UGameInstance* GI)
		{
			if (UWorldFactoryManager* F = GI->GetSubsystem<UWorldFactoryManager>()) { F->SerializeForSave(Factory); }
			if (UMineManager* M = GI->GetSubsystem<UMineManager>()) { M->SerializeForSave(Mine); }
			if (UTradeOrderManager* O = GI->GetSubsystem<UTradeOrderManager>()) { O->SerializeForSave(Orders, NextOrderId, Combo); }
			if (UWorldMapManager* W = GI->GetSubsystem<UWorldMapManager>())
			{
				for (const FIntPoint& Key : W->GetAllProductKeys())
				{
					Goods.Add(Key, TPair<int64, EQualityGrade>(W->GetProductAmount(Key), W->GetProductGrade(Key)));
				}
			}
			if (UResourceItemManager* R = GI->GetSubsystem<UResourceItemManager>()) { Resources = R->GetAllResources(); }
			bCaptured = true;
		}

		void Restore(UGameInstance* GI)
		{
			if (!bCaptured || !GI) { return; }
			if (UCloudEconomySubsystem* Cloud = GI->GetSubsystem<UCloudEconomySubsystem>())
			{
				for (const FName& Key : PendingKeys) { Cloud->EndPendingAction(Key); }
			}
			PendingKeys.Reset();
			if (UWorldFactoryManager* F = GI->GetSubsystem<UWorldFactoryManager>()) { F->LoadFromSave(Factory); }
			if (UMineManager* M = GI->GetSubsystem<UMineManager>()) { M->LoadFromSave(Mine); }
			if (UTradeOrderManager* O = GI->GetSubsystem<UTradeOrderManager>()) { O->LoadFromSave(Orders, NextOrderId, Combo); }
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
			if (UResourceItemManager* R = GI->GetSubsystem<UResourceItemManager>()) { R->SetAllResources(Resources, true); }
			bCaptured = false;
		}
	};

	struct FProduct
	{
		ECompanyType Company = ECompanyType::None;
		int32 Index = 0;
		FText Name;
		TSoftObjectPtr<UTexture2D> Icon;
	};

	struct FDrawerFixtures
	{
		UGameInstance* GI = nullptr;
		UTableManagerSubsystem* Tables = nullptr;
		UWorldFactoryManager* FactoryMgr = nullptr;
		UMineManager* MineMgr = nullptr;
		UTradeOrderManager* OrderMgr = nullptr;
		UWorldMapManager* Warehouse = nullptr;
		UResourceItemManager* Resources = nullptr;
		UCloudEconomySubsystem* Cloud = nullptr;
		ECountryType FactoryCountry = ECountryType::None;
		ECountryType OtherFactoryCountry = ECountryType::None;
		ECountryType MineCountry = ECountryType::None;
		ECountryType PortCountry = ECountryType::None;
		// 흰 칩 위 잉크 대비 시험용 ― 밝은 자원색(리튬)을 캐는 나라
		ECountryType LithiumCountry = ECountryType::None;
		TArray<EResourceType> MineResources;
		// 공장 줄 = 그 나라 산업 제품, 무역항 = 그중 판매가가 있는 제품(창고에 넣으면 「모두 판매」가 열린다)
		TArray<FProduct> Products;
		TArray<FProduct> PortProducts;
		FProduct Longest;

		bool Resolve(UGameInstance* InGI, FString& OutError)
		{
			GI = InGI;
			Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
			FactoryMgr = GI ? GI->GetSubsystem<UWorldFactoryManager>() : nullptr;
			MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
			OrderMgr = GI ? GI->GetSubsystem<UTradeOrderManager>() : nullptr;
			Warehouse = GI ? GI->GetSubsystem<UWorldMapManager>() : nullptr;
			Resources = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
			Cloud = GI ? GI->GetSubsystem<UCloudEconomySubsystem>() : nullptr;
			if (!Tables || !FactoryMgr || !MineMgr || !OrderMgr || !Warehouse || !Resources || !Cloud)
			{
				OutError = TEXT("매니저 서브시스템이 하나 이상 없습니다");
				return false;
			}
			const UEnum* CountryEnum = StaticEnum<ECountryType>();
			TArray<ECountryType> FactoryCountries;
			int32 BestMineKinds = 0;
			for (int32 I = 0; CountryEnum && I < CountryEnum->NumEnums() - 1; ++I)
			{
				const ECountryType Country = static_cast<ECountryType>(CountryEnum->GetValueByIndex(I));
				bool bOk = false;
				const FCountryInfoTable Info = Tables->GetCountryInfo(Country, bOk);
				if (!bOk) { continue; }
				if (Info.bSupportsFactory) { FactoryCountries.Add(Country); }
				const bool bPreferredMine = Country == ECountryType::Canada;
				if (Info.bSupportsMine && Info.MinableResources.Num() >= 3 && MineCountry != ECountryType::Canada
					&& (bPreferredMine || Info.MinableResources.Num() > BestMineKinds))
				{
					MineCountry = Country;
					BestMineKinds = Info.MinableResources.Num();
					MineResources = Info.MinableResources;
				}
				if (Info.bSupportsMine && Info.MinableResources.Contains(EResourceType::Lithium) && LithiumCountry == ECountryType::None)
				{
					LithiumCountry = Country;
				}
				if (Info.bIsTradeHub && (Country == ECountryType::Singapore || PortCountry == ECountryType::None))
				{
					PortCountry = Country;
				}
			}
			FactoryCountry = FactoryCountries.Contains(ECountryType::Korea) ? ECountryType::Korea
				: FactoryCountries.Num() > 0 ? FactoryCountries[0] : ECountryType::None;
			for (const ECountryType Country : FactoryCountries)
			{
				if (Country != FactoryCountry) { OtherFactoryCountry = Country; break; }
			}
			if (FactoryCountry == ECountryType::None || MineCountry == ECountryType::None || PortCountry == ECountryType::None
				|| LithiumCountry == ECountryType::None)
			{
				OutError = FString::Printf(TEXT("픽스처 나라를 못 찾았습니다(공장 %d 광산 %d 무역항 %d)"),
					static_cast<int32>(FactoryCountry), static_cast<int32>(MineCountry), static_cast<int32>(PortCountry));
				return false;
			}
			bool bFactoryInfoOk = false;
			const FCountryInfoTable FactoryInfo = Tables->GetCountryInfo(FactoryCountry, bFactoryInfoOk);
			TArray<ECompanyType> FactoryIndustries;
			if (bFactoryInfoOk && FactoryInfo.RepresentativeIndustry != ECompanyType::None)
			{
				FactoryIndustries.Add(FactoryInfo.RepresentativeIndustry);
			}
			for (const ECompanyType Industry : FactoryInfo.SupportedIndustries) { FactoryIndustries.AddUnique(Industry); }
			const UTradePort* Port = GI->GetSubsystem<UTradePort>();
			const UEnum* CompanyEnum = StaticEnum<ECompanyType>();
			int32 LongestLen = 0;
			for (int32 I = 0; CompanyEnum && I < CompanyEnum->NumEnums() - 1; ++I)
			{
				const ECompanyType Company = static_cast<ECompanyType>(CompanyEnum->GetValueByIndex(I));
				for (const FProjectData& Proj : Tables->GetProjectsByCompanyType(Company))
				{
					if (Proj.ProjectName.IsEmpty()) { continue; }
					FProduct P{ Company, Proj.ProjectIndex, Proj.ProjectName, Proj.Icon };
					const bool bIcon = !Proj.Icon.IsNull();
					if (bIcon && FactoryIndustries.Contains(Company) && Products.Num() < 12) { Products.Add(P); }
					// 미리보기 판매는 창고 재고로 잘리므로 판매가(기본 단가)로 거른다 ― 공장 나라 산업 제품만(실제 흐름)
					if (bIcon && Port && FactoryIndustries.Contains(Company) && PortProducts.Num() < 8
						&& Port->GetBasePrice(Company, Proj.ProjectIndex) > 0)
					{
						PortProducts.Add(P);
					}
					if (Proj.ProjectName.ToString().Len() > LongestLen)
					{
						LongestLen = Proj.ProjectName.ToString().Len();
						Longest = P;
					}
				}
			}
			if (Products.Num() < 6 || PortProducts.Num() < 4)
			{
				OutError = FString::Printf(TEXT("픽스처 제품 부족: 공장 나라 산업 제품 %d개(6 필요), 무역항 판매 가능 제품 %d개(4 필요)"),
					Products.Num(), PortProducts.Num());
				return false;
			}
			return true;
		}

		FWorldFactoryLineState MakeLine(int32 Id, const FProduct& P, int64 Target, float Rate, double ElapsedSec) const
		{
			FWorldFactoryLineState Line;
			Line.LineId = Id;
			Line.CompanyType = P.Company;
			Line.ProductIndex = P.Index;
			Line.ProductName = P.Name;
			Line.IconPath = P.Icon;
			Line.RatePerSecond = Rate;
			Line.TargetQty = Target;
			Line.StartWallUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(ElapsedSec);
			return Line;
		}

		// 생산 중 = 38/81 · 남은 약 5분, 완료 = 243/243
		FWorldFactoryLineState Producing(int32 Id, const FProduct& P) const
		{
			const float Rate = 81.f / 480.f;
			return MakeLine(Id, P, 81, Rate, 38.2 / Rate);
		}
		FWorldFactoryLineState Completed(int32 Id, const FProduct& P) const
		{
			return MakeLine(Id, P, 243, 1.f, 86400.0);
		}

		int32 FactoryLevelForSlots(ECountryType Country, int32 Slots, const TMap<ECountryType, FCountryFactoryData>& Base)
		{
			for (int32 Level = 0; Level <= 20; ++Level)
			{
				TMap<ECountryType, FCountryFactoryData> Data = Base;
				Data.FindOrAdd(Country).UpgradeLevels.Add(EWorldFactoryUpgradeType::LineExpansion, Level);
				FactoryMgr->LoadFromSave(Data);
				if (FactoryMgr->GetMaxLines(Country) >= Slots) { return Level; }
			}
			return 0;
		}

		void SeedFactory(ECountryType Country, const TArray<FWorldFactoryLineState>& Lines,
			const TMap<EWorldFactoryUpgradeType, int32>& Levels, const TMap<ECountryType, FCountryFactoryData>& Base)
		{
			TMap<ECountryType, FCountryFactoryData> Data = Base;
			FCountryFactoryData& D = Data.FindOrAdd(Country);
			D.ActiveLines = Lines;
			D.UpgradeLevels = Levels;
			int32 MaxId = 0;
			for (const FWorldFactoryLineState& L : Lines) { MaxId = FMath::Max(MaxId, L.LineId); }
			D.NextLineId = MaxId + 1;
			FactoryMgr->LoadFromSave(Data);
		}

		FMineLineState MineLine(EResourceType Resource, int64 MaxStorage, double FillRatio) const
		{
			bool bOk = false;
			const FResourceInfo Info = Tables->GetResourceInfo(Resource, bOk);
			FMineLineState Line;
			Line.Resource = Resource;
			Line.BaseRatePerSecond = bOk ? Info.MiningRatePerMinute / 60.f : 0.1f;
			Line.OverrideMaxStorage = MaxStorage;
			const double Rate = FMath::Max(1e-3, static_cast<double>(Line.BaseRatePerSecond));
			Line.StartWallUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(FillRatio >= 1.0 ? 864000.0 : (MaxStorage * FillRatio + 0.1) / Rate);
			return Line;
		}

		void SeedMine(ECountryType Country, const TArray<FMineLineState>& Lines, const TMap<EMineUpgradeType, int32>& Levels,
			const TMap<ECountryType, FCountryMineData>& Base)
		{
			TMap<ECountryType, FCountryMineData> Data = Base;
			FCountryMineData& D = Data.FindOrAdd(Country);
			D.ActiveLines = Lines;
			D.UpgradeLevels = Levels;
			MineMgr->LoadFromSave(Data);
		}

		void SetWarehouse(const TArray<TPair<FIntPoint, int64>>& Goods)
		{
			for (const FIntPoint& Key : Warehouse->GetAllProductKeys())
			{
				const int64 Have = Warehouse->GetProductAmount(Key);
				if (Have > 0) { Warehouse->ConsumeProduct(Key, Have, false); }
			}
			for (const TPair<FIntPoint, int64>& G : Goods)
			{
				Warehouse->AddProduct(G.Key, G.Value, EQualityGrade::B, false);
			}
		}

		void SetOrders(const TArray<FTradeOrder>& Orders)
		{
			OrderMgr->LoadFromSave(Orders, 9000, 0);
		}

		FTradeOrder Order(int32 Id, const FProduct& P, int64 Qty, ETradeOrderTier Tier, float Reward) const
		{
			FTradeOrder O;
			O.OrderId = Id;
			O.Tier = Tier;
			O.CompanyType = P.Company;
			O.ProjectIndex = P.Index;
			O.RequestedQuantity = Qty;
			O.RemainingQuantity = Qty;
			O.RewardMultiplier = Reward;
			O.ExpireTime = FDateTime::UtcNow() + FTimespan::FromHours(6.0);
			O.bAccepted = true;
			O.bViewed = true;
			return O;
		}

		void SetMoney(int64 Money)
		{
			TMap<EResourceType, int64> All = Resources->GetAllResources();
			All.Add(EResourceType::Money, Money);
			All.Add(EResourceType::Diamond, FMath::Max<int64>(All.FindRef(EResourceType::Diamond), 5000));
			Resources->SetAllResources(All, true);
		}

		FName FactoryPendingKey(ECountryType Country, int32 LineId) const
		{
			// WorldFactoryManager.cpp FactoryInstantFinishKey 와 같은 형식 ― 결제 대기 중(잠김) 상태 픽스처
			return FName(*FString::Printf(TEXT("factory.instantFinish.%d.%d"), static_cast<int32>(Country), LineId));
		}

		FName MinePendingKey(ECountryType Country, EResourceType Resource) const
		{
			return FName(*FString::Printf(TEXT("mine.instantFinish.%d.%d"), static_cast<int32>(Country), static_cast<int32>(Resource)));
		}
	};

	// ── 장면 단언 ──
	struct FChecker
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
		void Near(const FString& What, double Got, double Want, double Tolerance = Tol)
		{
			True(FString::Printf(TEXT("%s = %.1f (기대 %.1f±%.0f)"), *What, Got, Want, Tolerance), FMath::Abs(Got - Want) <= Tolerance);
		}
	};

	enum class ERowKind : uint8 { Factory, Mine, Empty, Locked, Order, Upgrade, Unknown };

	static ERowKind KindOf(UWidget* Row)
	{
		if (Cast<UWorldFactoryLineWidget>(Row)) { return ERowKind::Factory; }
		if (Cast<UMineLineWidget>(Row)) { return ERowKind::Mine; }
		if (Cast<UUpgradeSlot>(Row)) { return ERowKind::Upgrade; }
		if (UDrawerInfoRowWidget* Info = Cast<UDrawerInfoRowWidget>(Row))
		{
			if (IsShown(Info->GetWidgetFromName(TEXT("LockIcon")))) { return ERowKind::Locked; }
			if (IsShown(Info->GetWidgetFromName(TEXT("EmptyPlate")))) { return ERowKind::Empty; }
			return ERowKind::Order;
		}
		return ERowKind::Unknown;
	}

	static const TCHAR* KindName(ERowKind Kind)
	{
		switch (Kind)
		{
		case ERowKind::Factory: return TEXT("factory");
		case ERowKind::Mine:    return TEXT("mine");
		case ERowKind::Empty:   return TEXT("empty");
		case ERowKind::Locked:  return TEXT("locked");
		case ERowKind::Order:   return TEXT("order");
		case ERowKind::Upgrade: return TEXT("upgrade");
		default:                return TEXT("unknown");
		}
	}

	// 줄 안에서 지금 보이는 버튼(셀이 아니라 버튼 자체)
	static UWidget* RowButton(UUserWidget* Row, ERowKind Kind)
	{
		if (Kind == ERowKind::Upgrade)
		{
			return Cast<UUpgradeSlot>(Row)->GetUpgradeButton();
		}
		for (const TCHAR* Name : { TEXT("InstantFinishButton"), TEXT("RecieveButton"), TEXT("ActionButton") })
		{
			UWidget* Button = Row->GetWidgetFromName(Name);
			if (Button && IsShown(Button)) { return Button; }
		}
		return nullptr;
	}

	static UWidget* RowPicture(UUserWidget* Row, ERowKind Kind)
	{
		if (Kind == ERowKind::Upgrade)
		{
			UWidget* Icon = Row->GetWidgetFromName(TEXT("IconImage"));
			return Icon ? Icon->GetParent() : nullptr;
		}
		return Row->GetWidgetFromName(TEXT("PlateCell"));
	}

	static UWidget* RowName(UUserWidget* Row, ERowKind Kind)
	{
		for (const TCHAR* Name : { TEXT("ProductText"), TEXT("NameText"), TEXT("DescriptionText") })
		{
			if (UWidget* Text = Row->GetWidgetFromName(Name)) { return Text; }
		}
		return nullptr;
	}

	static void CollectTexts(UUserWidget* Host, TArray<UTextBlock*>& Out, int32 Depth = 0)
	{
		if (!Host || !Host->WidgetTree || Depth > 4) { return; }
		Host->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				if (IsShown(Text) && !Text->GetText().IsEmpty()) { Out.Add(Text); }
			}
			else if (UUserWidget* Child = Cast<UUserWidget>(Widget))
			{
				// 버튼 안 글자는 버튼 박스로 따로 본다
				if (!Cast<UCostActionButtonWidget>(Child) && !Cast<UCommonButtonBase>(Child))
				{
					CollectTexts(Child, Out, Depth + 1);
				}
			}
		});
	}

	// 글자 상자는 그 줄 칸 폭이 아니라 실제 글자 폭으로 잰다(왼쪽 정렬 기준)
	static FBox2 TextInkBox(UTextBlock* Text)
	{
		FBox2 B = BoxOf(Text);
		if (!B.bValid) { return B; }
		const FVector2D Desired = Text->GetDesiredSize();
		// 칸보다 긴 글자는 말줄임 + 제 칸 자르기일 때만 칸 안에 머문다 ― 아니면 칸 밖으로 그려진다(7b 실기에서 실측)
		const bool bClipped = Text->GetTextOverflowPolicy() == ETextOverflowPolicy::Ellipsis
			&& Text->GetClipping() == EWidgetClipping::ClipToBounds;
		const double InkW = Desired.X > B.W && bClipped ? B.W : Desired.X;
		if (InkW > 0.5 && FMath::Abs(InkW - B.W) > 0.5)
		{
			const FByteProperty* JustifyProp = CastField<FByteProperty>(UTextLayoutWidget::StaticClass()->FindPropertyByName(TEXT("Justification")));
			const ETextJustify::Type Justify = JustifyProp
				? static_cast<ETextJustify::Type>(JustifyProp->GetPropertyValue_InContainer(Text)) : ETextJustify::Left;
			if (Justify == ETextJustify::Right) { B.L = B.R() - InkW; }
			else if (Justify == ETextJustify::Center) { B.L = B.L + (B.W - InkW) * 0.5; }
			B.W = InkW;
		}
		return B;
	}

	struct FRowResult
	{
		ERowKind Kind = ERowKind::Unknown;
		FBox2 Box;
		UUserWidget* Row = nullptr;
	};

	// 줄 목록 한 개 ― 줄 문법(198/140/160 · 그림 158/124 · 글자 x 202 · 버튼 268×148 · 막대 58) + 줄 안 겹침 0
	// bScrolled = 스크롤 장면: 막대 밖으로 잘려 한 번도 안 그려진 줄은 건너뛰고(지오메트리 0) 끝까지 내린 두 번째 렌더에서 잰다
	// 이번 렌더에서 그려진 줄인가 ― 잘려 안 그려진 줄은 지난 렌더(다른 폭·다른 스크롤)의 낡은 지오메트리를 들고 있다
	static bool IsFreshRow(const FBox2& Box, const FBox2& PlateInner, const FBox2& Visible)
	{
		return Box.bValid && FMath::Abs(Box.L - PlateInner.L) <= 2.0
			&& (!Visible.bValid || (Box.B() > Visible.T + 0.5 && Box.T < Visible.B() - 0.5));
	}

	static TArray<FRowResult> CheckRows(FChecker& C, UPanelWidget* List, const FBox2& PlateInner, bool bAllowBelowPlate,
		bool bScrolled = false, int32 FirstIndex = 0, const FBox2& Visible = FBox2())
	{
		TArray<FRowResult> Rows;
		if (!List) { C.True(TEXT("줄 목록 위젯이 있다"), false); return Rows; }
		for (UWidget* Child : List->GetAllChildren())
		{
			UUserWidget* Row = Cast<UUserWidget>(Child);
			if (!Row || !IsShown(Row)) { continue; }
			FRowResult R;
			R.Kind = KindOf(Row);
			R.Box = BoxOf(Row);
			R.Row = Row;
			Rows.Add(R);
		}
		for (int32 I = FirstIndex; I < Rows.Num(); ++I)
		{
			const FRowResult& R = Rows[I];
			UUserWidget* Row = R.Row;
			const FString Tag = FString::Printf(TEXT("줄%d(%s)"), I + 1, KindName(R.Kind));
			if (bScrolled && !IsFreshRow(R.Box, PlateInner, Visible)) { continue; }
			const double WantH = R.Kind == ERowKind::Locked ? 140.0 : R.Kind == ERowKind::Order ? 160.0 : 198.0;
			C.Near(Tag + TEXT(" 높이"), R.Box.H, WantH);
			C.Near(Tag + TEXT(" 폭"), R.Box.W, 1064.0);
			C.True(FString::Printf(TEXT("%s 가 판 안쪽 가로 안 %s ⊂ %s"), *Tag, *BoxStr(R.Box), *BoxStr(PlateInner)),
				R.Box.L >= PlateInner.L - Tol && R.Box.R() <= PlateInner.R() + Tol);
			if (!bAllowBelowPlate)
			{
				C.True(FString::Printf(TEXT("%s 가 스크롤 없이 판 안 (아래 %.0f ≤ %.0f)"), *Tag, R.Box.B(), PlateInner.B()),
					R.Box.B() <= PlateInner.B() + Tol);
			}
			if (I > FirstIndex && (!bScrolled || IsFreshRow(Rows[I - 1].Box, PlateInner, Visible)))
			{
				C.Near(FString::Printf(TEXT("줄%d↔줄%d 간격"), I, I + 1), R.Box.T - Rows[I - 1].Box.B(), 16.0);
				C.True(FString::Printf(TEXT("줄%d↔줄%d 겹침 0"), I, I + 1), Overlap(R.Box, Rows[I - 1].Box) <= 0.0);
			}

			const FBox2 Picture = BoxOf(RowPicture(Row, R.Kind));
			if (R.Kind == ERowKind::Order)
			{
				C.Near(Tag + TEXT(" 그림 칸 높이"), BoxOf(Row->GetWidgetFromName(TEXT("IconPlate"))).H, 124.0);
			}
			else if (R.Kind != ERowKind::Locked)
			{
				C.Near(Tag + TEXT(" 그림 칸 폭"), Picture.W, 158.0);
				C.Near(Tag + TEXT(" 그림 칸 높이"), Picture.H, 158.0);
			}
			UTextBlock* NameText = Cast<UTextBlock>(RowName(Row, R.Kind));
			const FBox2 NameBox = BoxOf(NameText);
			C.Near(Tag + TEXT(" 글자 시작 x(줄 기준)"), NameBox.L - R.Box.L, 202.0);
			if (NameText && R.Kind != ERowKind::Upgrade)
			{
				C.Near(Tag + TEXT(" 이름 글자 pt"), NameText->GetFont().Size, 34.0, 0.01);
			}
			if (R.Kind == ERowKind::Factory || R.Kind == ERowKind::Mine)
			{
				C.Near(Tag + TEXT(" 막대 바깥 높이"), BoxOf(Row->GetWidgetFromName(TEXT("BarSize"))).H, 58.0);
				if (UTextBlock* Status = Named<UTextBlock>(Row, TEXT("StatusText")))
				{
					C.Near(Tag + TEXT(" 상태 글자 pt"), Status->GetFont().Size, 28.0, 0.01);
				}
			}
			UWidget* Button = RowButton(Row, R.Kind);
			const FBox2 ButtonBox = BoxOf(Button);
			if (R.Kind != ERowKind::Locked && R.Kind != ERowKind::Order)
			{
				C.Near(Tag + TEXT(" 버튼 폭(버튼 자체)"), ButtonBox.W, 268.0);
				C.Near(Tag + TEXT(" 버튼 높이(버튼 자체)"), ButtonBox.H, 148.0);
				C.True(FString::Printf(TEXT("%s 버튼이 줄 안 %s ⊂ %s"), *Tag, *BoxStr(ButtonBox), *BoxStr(R.Box)),
					ButtonBox.L >= R.Box.L - Tol && ButtonBox.R() <= R.Box.R() + Tol && ButtonBox.T >= R.Box.T - Tol && ButtonBox.B() <= R.Box.B() + Tol);
			}
			// 줄 안 겹침 ― 글자끼리 · 글자↔버튼 · 글자↔그림(번호 원은 그림 위 겹침이 의도)
			TArray<UTextBlock*> Texts;
			CollectTexts(Row, Texts);
			for (int32 A = 0; A < Texts.Num(); ++A)
			{
				const FBox2 TA = TextInkBox(Texts[A]);
				const FString NA = Texts[A]->GetName();
				const bool bBadge = NA.Contains(TEXT("Number"));
				if (ButtonBox.bValid)
				{
					C.True(FString::Printf(TEXT("%s 글자 %s 「%s」 %s ↔ 버튼 %s 겹침 0"), *Tag, *NA, *Texts[A]->GetText().ToString(), *BoxStr(TA), *BoxStr(ButtonBox)),
						Overlap(TA, ButtonBox) <= 0.0);
				}
				if (!bBadge && Picture.bValid && R.Kind != ERowKind::Locked)
				{
					C.True(FString::Printf(TEXT("%s 글자 %s ↔ 그림 칸 겹침 0"), *Tag, *NA), Overlap(TA, Picture) <= 0.0);
				}
				C.True(FString::Printf(TEXT("%s 글자 %s 가 줄 안"), *Tag, *NA),
					TA.L >= R.Box.L - Tol && TA.R() <= R.Box.R() + Tol && TA.T >= R.Box.T - Tol && TA.B() <= R.Box.B() + Tol);
				for (int32 K = A + 1; K < Texts.Num(); ++K)
				{
					const FString NK = Texts[K]->GetName();
					// 막대 안 글자는 막대 위에 얹힌다
					if (NA.Contains(TEXT("Bar")) != NK.Contains(TEXT("Bar"))) { continue; }
					if (bBadge || NK.Contains(TEXT("Number"))) { continue; }
					const FBox2 TK = TextInkBox(Texts[K]);
					C.True(FString::Printf(TEXT("%s 글자 %s %s ↔ %s %s 겹침 0"), *Tag, *NA, *BoxStr(TA), *NK, *BoxStr(TK)), Overlap(TA, TK) <= 0.0);
				}
			}
		}
		return Rows;
	}

	struct FSceneSpec
	{
		FString Label;
		EDioramaModule Facility = EDioramaModule::Factory;
		bool bUpgradePage = false;
		bool bExpectScroll = false;
		bool bLithiumCountry = false;
		// 단언·저장 없이 한 번 그리기만 ― 오프스크린 첫 렌더는 칩 재질이 아직 안 올라와 흰 칩 면이 빠진다(Task 6 README 「첫 렌더」)
		bool bWarmupOnly = false;
		TFunction<void(FDrawerFixtures&, FFixtureBackup&)> Seed;
		// 장면별 추가 단언(상태 색·잠금 등). Frame 은 그 폭의 렌더
		TFunction<void(FChecker&, UCountryDetailWidget&, const FImage&, const TArray<FRowResult>&)> Extra;
	};

	static ECountryType CountryFor(const FDrawerFixtures& W, EDioramaModule Facility)
	{
		return Facility == EDioramaModule::Mine ? W.MineCountry : Facility == EDioramaModule::Port ? W.PortCountry : W.FactoryCountry;
	}

	static int32 TabFor(EDioramaModule Facility)
	{
		return Facility == EDioramaModule::Mine ? UCountryDetailWidget::TAB_INDEX_MINE
			: Facility == EDioramaModule::Port ? UCountryDetailWidget::TAB_INDEX_PORT : UCountryDetailWidget::TAB_INDEX_FACTORY;
	}

	static bool IsPurple(const FLinearColor& M) { return M.B > M.G + 30.f && M.R > M.G + 10.f; }
	static bool IsGray(const FLinearColor& M) { return FMath::Max3(M.R, M.G, M.B) - FMath::Min3(M.R, M.G, M.B) < 30.f; }

	static double RelLuma(const FLinearColor& Linear) { return 0.2126 * Linear.R + 0.7152 * Linear.G + 0.0722 * Linear.B; }
	static bool IsGold(const FLinearColor& M) { return M.R > M.B + 60.f && M.G > M.B + 25.f; }
	static bool IsGreen(const FLinearColor& M) { return M.G > M.R + 25.f && M.G > M.B + 25.f; }
	static FString ColorStr(const FLinearColor& M) { return FString::Printf(TEXT("rgb(%.0f,%.0f,%.0f)"), M.R, M.G, M.B); }

	// 버튼 면 색 = 아래쪽 띠(라벨·칩 아래)
	static FLinearColor ButtonFace(const FImage& Frame, const FBox2& B)
	{
		return MeanColor(Frame, B.L + 30, B.T + B.H * 0.84, B.R() - 30, B.T + B.H * 0.92);
	}

	// 이름으로 찾되 중첩 UserWidget(버튼 안 칩 등) 안까지 내려간다
	static UWidget* FindDeep(UUserWidget* Host, const TCHAR* Name, int32 Depth = 0)
	{
		if (!Host || !Host->WidgetTree || Depth > 4) { return nullptr; }
		if (UWidget* Direct = Host->GetWidgetFromName(FName(Name))) { return Direct; }
		UWidget* Found = nullptr;
		Host->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Found)
			{
				if (UUserWidget* Child = Cast<UUserWidget>(Widget)) { Found = FindDeep(Child, Name, Depth + 1); }
			}
		});
		return Found;
	}

	static bool ChipShown(UWidget* Button)
	{
		UUserWidget* AsUser = Cast<UUserWidget>(Button);
		return AsUser && IsShown(AsUser->GetWidgetFromName(TEXT("CostWidget")));
	}

	static UTextBlock* ChipAmount(UWidget* Button)
	{
		return Cast<UTextBlock>(FindDeep(Cast<UUserWidget>(Button), TEXT("ResourceAmountText")));
	}

	static UObject* ChipIcon(UWidget* Button)
	{
		UImage* Icon = Cast<UImage>(FindDeep(Cast<UUserWidget>(Button), TEXT("ResourceImage")));
		return Icon ? Icon->GetBrush().GetResourceObject() : nullptr;
	}

	static UObject* ResourceIcon(UUserWidget& Any, EResourceType Type)
	{
		UGameInstance* AnyGI = Any.GetGameInstance();
		UTableManagerSubsystem* Tables = AnyGI ? AnyGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		bool bOk = false;
		const FResourceInfo Info = Tables ? Tables->GetResourceInfo(Type, bOk) : FResourceInfo();
		return bOk && !Info.Icon.IsNull() ? Info.Icon.LoadSynchronous() : nullptr;
	}

	static FString ChipText(UWidget* Button)
	{
		UTextBlock* Amount = ChipAmount(Button);
		return Amount ? Amount->GetText().ToString() : FString();
	}

	// 칩 숫자 = 진한 잉크 #1B2330, 대비 ≥ 4.5:1(칩 면은 렌더 픽셀, 글자 위 띠), 받을 것이면 「+」 앞글자
	static void CheckChipCount(FChecker& C, const FImage& Frame, UWidget* Button, const TCHAR* Tag, bool bIncoming)
	{
		UTextBlock* Amount = ChipAmount(Button);
		UWidget* Face = FindDeep(Cast<UUserWidget>(Button), TEXT("CommonBorder_85"));
		if (!Amount || !ChipShown(Button) || !Face)
		{
			C.True(FString::Printf(TEXT("%s 칩이 보인다"), Tag), false);
			return;
		}
		const FLinearColor CountInk = Amount->GetColorAndOpacity().GetSpecifiedColor();
		const FBox2 F = BoxOf(Face);
		const FLinearColor FaceSRGB = MeanColor(Frame, F.Center().X - 6, F.T + 2, F.Center().X + 6, F.T + 6);
		const FLinearColor FaceLinear = FLinearColor::FromSRGBColor(FColor(
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FaceSRGB.R), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FaceSRGB.G), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FaceSRGB.B), 0, 255))));
		const double LumaInk = RelLuma(CountInk);
		const double LumaFace = RelLuma(FaceLinear);
		const double Ratio = (FMath::Max(LumaInk, LumaFace) + 0.05) / (FMath::Min(LumaInk, LumaFace) + 0.05);
		C.True(FString::Printf(TEXT("%s 칩 숫자 잉크 = #1B2330"), Tag), SameInk(CountInk, Ink(0x1B, 0x23, 0x30)));
		C.True(FString::Printf(TEXT("%s 칩 숫자 대비 %.1f:1 ≥ 4.5 (칩 면 %s)"), Tag, Ratio, *ColorStr(FaceSRGB)), Ratio >= 4.5);
		const FString Text = Amount->GetText().ToString();
		C.True(FString::Printf(TEXT("%s 칩 「%s」 앞글자 %s"), Tag, *Text, bIncoming ? TEXT("「+」") : TEXT("없음")),
			bIncoming ? Text.StartsWith(TEXT("+")) : !Text.StartsWith(TEXT("+")));
	}
}


// ═════════════════════════════════════════════════════════════════════════════
// CGR.Overseas.DrawerLive.LayoutGeometry ― 실제 서랍을 진짜 매니저 상태로 열고 두 폭에서 잰다 + 캡처를 남긴다
// ═════════════════════════════════════════════════════════════════════════════
namespace CGRDrawerLive
{
	class FLayoutGeometryCommand : public IAutomationLatentCommand
	{
	public:
		FLayoutGeometryCommand(FAutomationTestBase* InTest, UWorldMapPanelWidget* InPanel, const FDrawerFixtures& InWorld, TArray<FSceneSpec>&& InScenes)
			: Test(InTest), Panel(InPanel), W(InWorld), Scenes(MoveTemp(InScenes))
		{
			PanelSlate = Panel->TakeWidget();
			Stack = Cast<UCommonActivatableWidgetStack>(Panel->GetWidgetFromName(TEXT("DrawerStack")));
			bReducedBefore = FMotionGate::IsReduced();
			FMotionGate::SetReduced(true);
			Backup.Capture(W.GI);
			Report = MakeShared<FJsonObject>();
			IFileManager::Get().MakeDirectory(*LiveDir(), true);
		}

		virtual ~FLayoutGeometryCommand() override
		{
			if (Stack) { Stack->SetTransitionDuration(0.0f); Stack->ClearWidgets(); }
			Panel->ResetFocus(false);
			Backup.Restore(W.GI);
			FMotionGate::SetReduced(bReducedBefore);
			FString Serialized;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
			FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
			FFileHelper::SaveStringToFile(Serialized, *(LiveDir() / TEXT("layout_geometry.json")), FFileHelper::EEncodingOptions::ForceUTF8);
		}

		virtual bool Update() override
		{
			if (!Stack)
			{
				Test->AddError(TEXT("DrawerStack 을 못 찾았습니다"));
				return true;
			}
			if (SceneIndex >= Scenes.Num()) { return true; }
			const double Now = FPlatformTime::Seconds();
			FSceneSpec& Spec = Scenes[SceneIndex];
			if (Phase == 0)
			{
				Stack->SetTransitionDuration(0.0f);
				Backup.Restore(W.GI);
				Backup.Capture(W.GI);
				Spec.Seed(W, Backup);
				const ECountryType Country = Spec.bLithiumCountry ? W.LithiumCountry : CountryFor(W, Spec.Facility);
				ClickPin(Panel.Get(), Country);
				if (UCountryDetailWidget* Detail = ActiveDetail(Panel.Get()))
				{
					Detail->SwitchToTab(TabFor(Spec.Facility));
					Detail->SetCurrentCountry(Country);
				}
				Phase = 1;
				PhaseAt = Now;
				return false;
			}
			if (Phase == 1)
			{
				// 다음 틱 타이머(빈 라인 갱신)와 활성화가 한 프레임 돌고 난 뒤에 페이지를 고른다
				if (Now - PhaseAt < 0.25) { return false; }
				UCountryDetailWidget* Detail = ActiveDetail(Panel.Get());
				if (!Detail)
				{
					Test->AddError(FString::Printf(TEXT("[%s] 핀 클릭 뒤 서랍이 열리지 않았습니다"), *Spec.Label));
					++SceneIndex; Phase = 0;
					return false;
				}
				UDrawerPageToggleWidget* Toggle = Named<UDrawerPageToggleWidget>(Detail, TEXT("PageToggle"));
				UButtonWidget* Want = Named<UButtonWidget>(Toggle, Spec.bUpgradePage ? TEXT("UpgradeSegment") : TEXT("ProductionSegment"));
				if (Want && !Want->GetSelected() && Spec.Facility != EDioramaModule::Port)
				{
					// 실제 버튼 클릭 경로(그룹 배타선택 → 토글 방송 → 서랍 페이지 전환)
					Want->SimulateClick();
				}
				Phase = 2;
				PhaseAt = Now;
				return false;
			}
			if (Now - PhaseAt < 0.25) { return false; }
			RunScene(Spec);
			++SceneIndex;
			Phase = 0;
			return SceneIndex >= Scenes.Num();
		}

	private:
		void RunScene(FSceneSpec& Spec)
		{
			UCountryDetailWidget* Detail = ActiveDetail(Panel.Get());
			if (!Detail) { Test->AddError(Spec.Label + TEXT(" 서랍 없음")); return; }
			if (Spec.bWarmupOnly)
			{
				for (const int32 Width : CaptureWidths)
				{
					FStage Warm;
					if (Warm.Init(PanelSlate.ToSharedRef(), FIntPoint(Width, CaptureHeight))) { Warm.Draw(6, 0.1f, nullptr); }
					Warm.Detach();
				}
				return;
			}
			TSharedPtr<FJsonObject> SceneJson = MakeShared<FJsonObject>();
			for (const int32 Width : CaptureWidths)
			{
				FChecker C;
				C.Test = Test;
				C.Scene = FString::Printf(TEXT("%s_%d"), *Spec.Label, Width);
				FStage Stage;
				FImage Frame;
				const bool bDrawn = Stage.Init(PanelSlate.ToSharedRef(), FIntPoint(Width, CaptureHeight)) && Stage.Draw(6, 0.1f, &Frame);
				C.True(TEXT("렌더 성공"), bDrawn);
				if (!bDrawn) { Stage.Detach(); continue; }
				SavePNG(Frame, LiveDir() / (C.Scene + TEXT(".png")));
				CheckScene(C, Spec, *Detail, Frame, Width);
				if (Spec.bExpectScroll && Spec.Facility != EDioramaModule::Port)
				{
					CheckScrolledTail(C, Spec, *Detail, Stage);
				}

				TArray<TSharedPtr<FJsonValue>> Widgets;
				DumpTree(Detail, TEXT(""), Widgets);
				TSharedPtr<FJsonObject> Dump = MakeShared<FJsonObject>();
				Dump->SetStringField(TEXT("label"), C.Scene);
				Dump->SetNumberField(TEXT("width"), Width);
				Dump->SetNumberField(TEXT("height"), CaptureHeight);
				Dump->SetArrayField(TEXT("widgets"), Widgets);
				FString DumpText;
				const TSharedRef<TJsonWriter<>> DumpWriter = TJsonWriterFactory<>::Create(&DumpText);
				FJsonSerializer::Serialize(Dump.ToSharedRef(), DumpWriter);
				FFileHelper::SaveStringToFile(DumpText, *(LiveDir() / (C.Scene + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8);

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
		}

		void CheckScene(FChecker& C, FSceneSpec& Spec, UCountryDetailWidget& Detail, const FImage& Frame, int32 Width)
		{
			// 서랍 바깥(테 포함) = 1160, 화면 오른쪽에 붙는다. 테 안 셸 = 1120(테 20 + 여백 4 = 시안 24)
			const FBox2 Outer = BoxOf(Detail.GetWidgetFromName(TEXT("UIE_PanelBorder")));
			const FBox2 Shell = BoxOf(Detail.GetWidgetFromName(TEXT("ShellVBox")));
			C.Near(TEXT("서랍 바깥 폭"), Outer.W, 1160.0);
			C.Near(TEXT("서랍 바깥 오른쪽 = 화면 오른쪽"), Outer.R(), Width);
			C.Near(TEXT("셸 폭"), Shell.W, 1120.0);
			UPanelHeaderWidget* Header = Named<UPanelHeaderWidget>(&Detail, TEXT("PanelHeader"));
			const FBox2 CloseBox = BoxOf(Header ? Header->GetCloseButton() : nullptr);
			C.True(FString::Printf(TEXT("헤더 X %s 가 화면 안"), *BoxStr(CloseBox)), CloseBox.bValid && CloseBox.R() <= Width + Tol);
			const FBox2 Toggle = BoxOf(Detail.GetWidgetFromName(TEXT("PageToggle")));
			const TCHAR* PlateName = Spec.Facility == EDioramaModule::Mine ? TEXT("MinePlate")
				: Spec.Facility == EDioramaModule::Port ? TEXT("PortPlate") : TEXT("FactoryPlate");
			const FBox2 Plate = BoxOf(Detail.GetWidgetFromName(PlateName));
			FBox2 Inner = Plate;
			Inner.L += 24; Inner.T += 24; Inner.W -= 48; Inner.H -= 48;
			C.True(FString::Printf(TEXT("본문 판 %s 이 화면 안"), *BoxStr(Plate)), Plate.bValid && Plate.B() <= CaptureHeight + Tol && Plate.R() <= Width + Tol);
			C.Near(TEXT("본문 판 폭"), Plate.W, 1112.0);
			C.Near(TEXT("본문 판 아래 = 화면 아래 - 24"), Plate.B(), CaptureHeight - 24.0);

			if (Spec.Facility == EDioramaModule::Port)
			{
				C.True(TEXT("무역항은 토글이 없다"), !Toggle.bValid);
				const TArray<FRowResult> Rows = CheckPort(C, Detail, Inner);
				if (Spec.Extra) { Spec.Extra(C, Detail, Frame, Rows); }
			}
			else
			{
				C.Near(TEXT("토글 높이"), Toggle.H, 120.0);
				C.Near(TEXT("토글 폭"), Toggle.W, 1112.0);
				C.Near(TEXT("토글 왼쪽 = 서랍 바깥 + 24"), Toggle.L - Outer.L, 24.0);
				C.True(TEXT("토글이 판 위에 겹치지 않는다"), Toggle.bValid && Toggle.B() <= Plate.T + Tol);
				CheckToggle(C, Detail, Frame, Spec.bUpgradePage);
				const bool bMine = Spec.Facility == EDioramaModule::Mine;
				if (Spec.bUpgradePage)
				{
					UPanelWidget* List = Named<UPanelWidget>(&Detail, bMine ? TEXT("MineEnhanceList") : TEXT("FactoryEnhanceList"));
					const TArray<FRowResult> Rows = CheckRows(C, List, Inner, false);
					C.Near(TEXT("강화 줄 수"), Rows.Num(), 5.0, 0.0);
					UScrollBox* Scroll = Named<UScrollBox>(&Detail, bMine ? TEXT("MineEnhanceScroll") : TEXT("FactoryEnhanceScroll"));
					C.True(FString::Printf(TEXT("강화 5줄이 스크롤 없이 들어간다(넘침 %.1f)"), Scroll ? Scroll->GetScrollOffsetOfEnd() : -1.f),
						Scroll && Scroll->GetScrollOffsetOfEnd() <= 0.5f);
					if (Spec.Extra) { Spec.Extra(C, Detail, Frame, Rows); }
				}
				else
				{
					UDrawerHeroRowWidget* Hero = Named<UDrawerHeroRowWidget>(&Detail, bMine ? TEXT("MineHero") : TEXT("FactoryHero"));
					CheckHero(C, Hero, Inner);
					UScrollBox* Scroll = Named<UScrollBox>(&Detail, bMine ? TEXT("MineLineScroll") : TEXT("FactoryLineScroll"));
					UPanelWidget* List = Named<UPanelWidget>(&Detail, bMine ? TEXT("MineLineList") : TEXT("FactoryLineList"));
					const TArray<FRowResult> Rows = CheckRows(C, List, Inner, Spec.bExpectScroll, Spec.bExpectScroll, 0, BoxOf(Scroll));
					const float End = Scroll ? Scroll->GetScrollOffsetOfEnd() : -1.f;
					if (Spec.bExpectScroll)
					{
						C.True(FString::Printf(TEXT("라인이 많으면 목록이 스크롤된다(넘침 %.1f > 0)"), End), End > 0.5f);
						const UVerticalBoxSlot* ScrollSlot = Scroll ? Cast<UVerticalBoxSlot>(Scroll->Slot) : nullptr;
						C.True(FString::Printf(TEXT("막대가 보이면 스크롤 상자가 판 안쪽 여백으로 나간다(오른쪽 %.1f < 0)"),
							ScrollSlot ? ScrollSlot->GetPadding().Right : 0.f), ScrollSlot && ScrollSlot->GetPadding().Right < -0.5f);
					}
					else
					{
						C.True(FString::Printf(TEXT("이 장면은 스크롤 없이 들어간다(넘침 %.1f)"), End), Scroll && End <= 0.5f);
					}
					if (Hero && Rows.Num() > 0)
					{
						C.Near(TEXT("요약 줄↔첫 줄 간격"), Rows[0].Box.T - BoxOf(Hero).B(), 16.0);
					}
					if (Spec.Extra) { Spec.Extra(C, Detail, Frame, Rows); }
				}
			}
		}

		// 처음 렌더에서 막대 밖이라 안 그려진 줄(잠긴 줄 등)을 목록 끝까지 내린 렌더에서 잰다
		void CheckScrolledTail(FChecker& C, const FSceneSpec& Spec, UCountryDetailWidget& Detail, FStage& Stage)
		{
			const bool bMine = Spec.Facility == EDioramaModule::Mine;
			UScrollBox* Scroll = Named<UScrollBox>(&Detail, bMine ? TEXT("MineLineScroll") : TEXT("FactoryLineScroll"));
			UPanelWidget* List = Named<UPanelWidget>(&Detail, bMine ? TEXT("MineLineList") : TEXT("FactoryLineList"));
			if (!Scroll || !List) { C.True(TEXT("스크롤 목록이 있다"), false); return; }
			const FBox2 TopPlate = BoxOf(Detail.GetWidgetFromName(bMine ? TEXT("MinePlate") : TEXT("FactoryPlate")));
			FBox2 TopInner = TopPlate;
			TopInner.L += 24; TopInner.T += 24; TopInner.W -= 48; TopInner.H -= 48;
			const FBox2 TopVisible = BoxOf(Scroll);
			int32 FirstUnpainted = INDEX_NONE;
			int32 Index = 0;
			for (UWidget* Child : List->GetAllChildren())
			{
				if (!IsShown(Child)) { continue; }
				if (FirstUnpainted == INDEX_NONE && !IsFreshRow(BoxOf(Child), TopInner, TopVisible)) { FirstUnpainted = Index; }
				++Index;
			}
			C.True(TEXT("스크롤 장면 = 처음 화면 밖에 줄이 있다"), FirstUnpainted != INDEX_NONE);
			Scroll->ScrollToEnd();
			FImage EndFrame;
			if (!Stage.Draw(4, 0.016f, &EndFrame)) { C.True(TEXT("끝까지 내린 렌더"), false); return; }
			SavePNG(EndFrame, LiveDir() / (C.Scene + TEXT("_end.png")));
			const FBox2 Plate = BoxOf(Detail.GetWidgetFromName(bMine ? TEXT("MinePlate") : TEXT("FactoryPlate")));
			FBox2 Inner = Plate;
			Inner.L += 24; Inner.T += 24; Inner.W -= 48; Inner.H -= 48;
			const FString Scene = C.Scene;
			C.Scene = Scene + TEXT("_end");
			CheckRows(C, List, Inner, false, true, FMath::Max(0, FirstUnpainted - 1), BoxOf(Scroll));
			C.Scene = Scene;
			Scroll->ScrollToStart();
		}

		void CheckHero(FChecker& C, UDrawerHeroRowWidget* Hero, const FBox2& Inner)
		{
			const FBox2 HeroBox = BoxOf(Hero);
			C.Near(TEXT("요약 줄 높이"), HeroBox.H, 178.0);
			C.Near(TEXT("요약 줄 폭"), HeroBox.W, 1064.0);
			C.Near(TEXT("요약 줄 위 = 판 안쪽 위"), HeroBox.T, Inner.T);
			UWidget* Action = Hero ? Hero->GetWidgetFromName(TEXT("ActionButton")) : nullptr;
			const FBox2 ActionBox = BoxOf(Action);
			C.Near(TEXT("요약 버튼 폭(버튼 자체)"), ActionBox.W, 268.0);
			C.Near(TEXT("요약 버튼 높이(버튼 자체)"), ActionBox.H, 148.0);
			for (const TCHAR* Name : { TEXT("TitleText"), TEXT("DetailText") })
			{
				UTextBlock* Text = Named<UTextBlock>(Hero, Name);
				const FBox2 TB = TextInkBox(Text);
				C.True(FString::Printf(TEXT("요약 줄 %s %s ↔ 버튼 %s 겹침 0"), Name, *BoxStr(TB), *BoxStr(ActionBox)), Overlap(TB, ActionBox) <= 0.0);
			}
		}

		void CheckToggle(FChecker& C, UCountryDetailWidget& Detail, const FImage& Frame, bool bUpgrade)
		{
			UDrawerPageToggleWidget* Toggle = Named<UDrawerPageToggleWidget>(&Detail, TEXT("PageToggle"));
			UButtonWidget* Production = Named<UButtonWidget>(Toggle, TEXT("ProductionSegment"));
			UButtonWidget* Upgrade = Named<UButtonWidget>(Toggle, TEXT("UpgradeSegment"));
			if (!Production || !Upgrade) { C.True(TEXT("토글 두 칸이 있다"), false); return; }
			UButtonWidget* Selected = bUpgrade ? Upgrade : Production;
			UButtonWidget* Other = bUpgrade ? Production : Upgrade;
			C.True(TEXT("고른 칸이 그룹에서 선택됐다"), Selected->GetSelected() && !Other->GetSelected());
			C.True(TEXT("서랍 페이지가 토글과 같다"),
				Toggle->GetPage() == (bUpgrade ? CountryDrawer::EDrawerPage::Upgrade : CountryDrawer::EDrawerPage::Production));
			UTextBlock* SelText = Named<UTextBlock>(Selected, TEXT("ButtonText"));
			UTextBlock* OtherText = Named<UTextBlock>(Other, TEXT("ButtonText"));
			C.True(TEXT("선택 칸 글자 잉크 = #1B2330"), SelText && SameInk(SelText->GetColorAndOpacity().GetSpecifiedColor(), Ink(0x1B, 0x23, 0x30)));
			C.True(TEXT("비선택 칸 글자 잉크 = #8F9AAB"), OtherText && SameInk(OtherText->GetColorAndOpacity().GetSpecifiedColor(), Ink(0x8F, 0x9A, 0xAB)));
			if (SelText) { C.Near(TEXT("토글 글자 pt"), SelText->GetFont().Size, 34.0, 0.01); }
			const FBox2 SB = BoxOf(Selected);
			const FBox2 OB = BoxOf(Other);
			const FLinearColor Knob = MeanColor(Frame, SB.L + 30, SB.T + SB.H * 0.25, SB.L + 110, SB.T + SB.H * 0.6);
			const FLinearColor Groove = MeanColor(Frame, OB.L + 30, OB.T + OB.H * 0.25, OB.L + 110, OB.T + OB.H * 0.6);
			const float KnobLuma = 0.2126f * Knob.R + 0.7152f * Knob.G + 0.0722f * Knob.B;
			const float GrooveLuma = 0.2126f * Groove.R + 0.7152f * Groove.G + 0.0722f * Groove.B;
			C.True(FString::Printf(TEXT("선택 칸에 흰 손잡이가 그려진다(휘도 %.0f ≥ 200, %s)"), KnobLuma, *ColorStr(Knob)), KnobLuma >= 200.f);
			C.True(FString::Printf(TEXT("비선택 칸은 어두운 홈(휘도 %.0f ≤ 60)"), GrooveLuma), GrooveLuma <= 60.f);
		}

		TArray<FRowResult> CheckPort(FChecker& C, UCountryDetailWidget& Detail, const FBox2& Inner)
		{
			UDrawerHeroRowWidget* Hero = Named<UDrawerHeroRowWidget>(&Detail, TEXT("PortHero"));
			CheckHero(C, Hero, Inner);
			UPanelWidget* Orders = Named<UPanelWidget>(&Detail, TEXT("PortOrderList"));
			const TArray<FRowResult> Rows = CheckRows(C, Orders, Inner, true);
			TArray<TPair<FString, FBox2>> Blocks;
			Blocks.Emplace(TEXT("요약 줄"), BoxOf(Hero));
			Blocks.Emplace(TEXT("「주문」 소제목"), BoxOf(Detail.GetWidgetFromName(TEXT("PortOrderHeader"))));
			for (int32 I = 0; I < Rows.Num(); ++I) { Blocks.Emplace(FString::Printf(TEXT("주문 줄%d"), I + 1), Rows[I].Box); }
			Blocks.Emplace(TEXT("「보관 중인 완제품」 소제목"), BoxOf(Detail.GetWidgetFromName(TEXT("PortGoodsHeader"))));
			UUniformGridPanel* Grid = Named<UUniformGridPanel>(&Detail, TEXT("PortGoodsList"));
			TArray<FBox2> Tiles;
			if (Grid)
			{
				for (UWidget* Child : Grid->GetAllChildren())
				{
					if (Cast<UDrawerGoodsTileWidget>(Child) && IsShown(Child)) { Tiles.Add(BoxOf(Child)); }
				}
			}
			for (int32 I = 0; I < Tiles.Num(); ++I)
			{
				Blocks.Emplace(FString::Printf(TEXT("완제품 칸%d"), I + 1), Tiles[I]);
				C.Near(FString::Printf(TEXT("완제품 칸%d 폭"), I + 1), Tiles[I].W, 254.0);
				if (I > 0 && FMath::Abs(Tiles[I].T - Tiles[I - 1].T) < 1.0)
				{
					C.Near(FString::Printf(TEXT("완제품 칸%d↔%d 간격"), I, I + 1), Tiles[I].L - Tiles[I - 1].R(), 16.0);
				}
			}
			if (Tiles.Num() > 0)
			{
				C.Near(TEXT("완제품 첫 칸 왼쪽 = 줄 왼쪽"), Tiles[0].L, Inner.L);
			}
			Blocks.Emplace(TEXT("빈 창고 안내"), BoxOf(Detail.GetWidgetFromName(TEXT("PortEmptyText"))));
			for (int32 A = 0; A < Blocks.Num(); ++A)
			{
				if (!Blocks[A].Value.bValid) { continue; }
				C.True(FString::Printf(TEXT("%s %s 가 판 안쪽 가로 안"), *Blocks[A].Key, *BoxStr(Blocks[A].Value)),
					Blocks[A].Value.L >= Inner.L - 8 - Tol && Blocks[A].Value.R() <= Inner.R() + 8 + Tol);
				for (int32 K = A + 1; K < Blocks.Num(); ++K)
				{
					if (!Blocks[K].Value.bValid) { continue; }
					C.True(FString::Printf(TEXT("%s %s ↔ %s %s 겹침 0"), *Blocks[A].Key, *BoxStr(Blocks[A].Value), *Blocks[K].Key, *BoxStr(Blocks[K].Value)),
						Overlap(Blocks[A].Value, Blocks[K].Value) <= 0.0);
				}
			}
			return Rows;
		}

		FAutomationTestBase* Test;
		TStrongObjectPtr<UWorldMapPanelWidget> Panel;
		TSharedPtr<SWidget> PanelSlate;
		UCommonActivatableWidgetStack* Stack = nullptr;
		FDrawerFixtures W;
		TArray<FSceneSpec> Scenes;
		FFixtureBackup Backup;
		TSharedPtr<FJsonObject> Report;
		int32 SceneIndex = 0;
		int32 Phase = 0;
		double PhaseAt = 0.0;
		bool bReducedBefore = false;
	};

	static TArray<FSceneSpec> BuildScenes()
	{
		TArray<FSceneSpec> Scenes;
		using FRows = TArray<FRowResult>;

		auto ExpectKinds = [](FChecker& C, const FRows& Rows, std::initializer_list<ERowKind> Kinds)
		{
			TArray<FString> Got;
			for (const FRowResult& R : Rows) { Got.Add(KindName(R.Kind)); }
			TArray<FString> Want;
			for (ERowKind K : Kinds) { Want.Add(KindName(K)); }
			C.True(FString::Printf(TEXT("줄 구성 [%s] = 기대 [%s]"), *FString::Join(Got, TEXT(",")), *FString::Join(Want, TEXT(","))), Got == Want);
		};

		// 생산 중 줄의 즉시 완료 = 열림(보라 + 다이아 칩) / 결제 대기 = 잠김(회색, 칩 없음)
		auto CheckInstant = [](FChecker& C, const FImage& Frame, const FRowResult& Row, bool bAvailable)
		{
			UWidget* Button = Row.Row->GetWidgetFromName(TEXT("InstantFinishButton"));
			C.True(TEXT("생산 중 줄에 즉시 완료 버튼이 보인다"), IsShown(Button));
			const FBox2 B = BoxOf(Button);
			const FLinearColor Face = ButtonFace(Frame, B);
			if (bAvailable)
			{
				C.True(FString::Printf(TEXT("즉시 완료(열림) 면이 보라 %s"), *ColorStr(Face)), IsPurple(Face));
				C.True(FString::Printf(TEXT("즉시 완료(열림) 다이아 칩이 보인다(「%s」)"), *ChipText(Button)), ChipShown(Button) && !ChipText(Button).IsEmpty());
			}
			else
			{
				C.True(FString::Printf(TEXT("즉시 완료(잠김) 면이 회색 %s"), *ColorStr(Face)), IsGray(Face));
				C.True(TEXT("즉시 완료(잠김) 칩이 숨는다"), !ChipShown(Button));
			}
		};

		auto FactoryBase = [](FDrawerFixtures& W, FFixtureBackup& B, int32 Slots, TArray<FWorldFactoryLineState> Lines, bool bPendingFirst)
		{
			const int32 Level = W.FactoryLevelForSlots(W.FactoryCountry, Slots, B.Factory);
			TMap<EWorldFactoryUpgradeType, int32> Levels;
			Levels.Add(EWorldFactoryUpgradeType::LineExpansion, Level);
			W.SeedFactory(W.FactoryCountry, Lines, Levels, B.Factory);
			W.SetMoney(1000000000);
			if (bPendingFirst && Lines.Num() > 0)
			{
				const FName Key = W.FactoryPendingKey(W.FactoryCountry, Lines[0].LineId);
				if (W.Cloud->BeginPendingAction(Key)) { B.PendingKeys.Add(Key); }
			}
		};

		{
			FSceneSpec S;
			S.Label = TEXT("factory_production");
			{
				FSceneSpec Warm;
				Warm.Label = TEXT("_warmup");
				Warm.bWarmupOnly = true;
				Warm.Seed = [FactoryBase](FDrawerFixtures& W, FFixtureBackup& B)
				{
					FactoryBase(W, B, 3, { W.Producing(1, W.Products[0]), W.Completed(2, W.Products[1]) }, false);
				};
				Scenes.Add(Warm);
			}
			S.Seed = [FactoryBase](FDrawerFixtures& W, FFixtureBackup& B)
			{
				FactoryBase(W, B, 3, { W.Producing(1, W.Products[0]), W.Completed(2, W.Products[1]) }, false);
			};
			S.Extra = [ExpectKinds, CheckInstant](FChecker& C, UCountryDetailWidget& Detail, const FImage& Frame, const FRows& Rows)
			{
				ExpectKinds(C, Rows, { ERowKind::Factory, ERowKind::Factory, ERowKind::Empty, ERowKind::Locked });
				UDrawerHeroRowWidget* Hero = Named<UDrawerHeroRowWidget>(&Detail, TEXT("FactoryHero"));
				CheckChipCount(C, Frame, Hero ? Hero->GetWidgetFromName(TEXT("ActionButton")) : nullptr, TEXT("공장 「모두 받기」"), true);
				if (Rows.Num() >= 3)
				{
					CheckInstant(C, Frame, Rows[0], true);
					CheckChipCount(C, Frame, Rows[0].Row->GetWidgetFromName(TEXT("InstantFinishButton")), TEXT("즉시 완료 다이아"), false);
					CheckChipCount(C, Frame, Rows[1].Row->GetWidgetFromName(TEXT("RecieveButton")), TEXT("완료 줄 받기"), true);
					UWidget* Receive = Rows[1].Row->GetWidgetFromName(TEXT("RecieveButton"));
					const FLinearColor Face = ButtonFace(Frame, BoxOf(Receive));
					C.True(FString::Printf(TEXT("완료 줄 받기 버튼이 금색 %s"), *ColorStr(Face)), IsShown(Receive) && IsGold(Face));
					UWidget* Start = Rows[2].Row->GetWidgetFromName(TEXT("ActionButton"));
					const FLinearColor StartFace = ButtonFace(Frame, BoxOf(Start));
					C.True(FString::Printf(TEXT("빈 라인 생산 시작 버튼이 파랑 %s"), *ColorStr(StartFace)), StartFace.B > StartFace.R + 40.f);
				}
			};
			Scenes.Add(S);
		}
		{
			FSceneSpec S;
			S.Label = TEXT("factory_instant_locked");
			S.Seed = [FactoryBase](FDrawerFixtures& W, FFixtureBackup& B)
			{
				FactoryBase(W, B, 3, { W.Producing(1, W.Products[0]), W.Completed(2, W.Products[1]) }, true);
			};
			S.Extra = [CheckInstant](FChecker& C, UCountryDetailWidget&, const FImage& Frame, const FRows& Rows)
			{
				if (Rows.Num() > 0) { CheckInstant(C, Frame, Rows[0], false); }
			};
			Scenes.Add(S);
		}
		{
			FSceneSpec S;
			S.Label = TEXT("factory_longname");
			S.Seed = [FactoryBase](FDrawerFixtures& W, FFixtureBackup& B)
			{
				FactoryBase(W, B, 3, { W.Producing(1, W.Longest), W.Completed(2, W.Longest) }, false);
			};
			S.Extra = [](FChecker& C, UCountryDetailWidget&, const FImage&, const FRows& Rows)
			{
				C.True(TEXT("최장 제품명 장면에 줄이 있다"), Rows.Num() >= 2);
			};
			Scenes.Add(S);
		}
		{
			FSceneSpec S;
			S.Label = TEXT("factory_scroll");
			S.bExpectScroll = true;
			S.Seed = [FactoryBase](FDrawerFixtures& W, FFixtureBackup& B)
			{
				FactoryBase(W, B, 5, { W.Producing(1, W.Products[0]), W.Completed(2, W.Products[1]), W.Producing(3, W.Products[2]),
					W.Completed(4, W.Products[3]), W.Producing(5, W.Products[4]) }, false);
			};
			Scenes.Add(S);
		}
		auto UpgradeStates = [](FChecker& C, const FImage& Frame, const FRows& Rows, bool bRequireShort)
			{
				int32 Maxed = 0, Short = 0, Open = 0;
				for (const FRowResult& R : Rows)
				{
					UUpgradeSlot* UpSlot = Cast<UUpgradeSlot>(R.Row);
					UCostActionButtonWidget* Button = UpSlot ? UpSlot->GetUpgradeButton() : nullptr;
					const FLinearColor Face = ButtonFace(Frame, BoxOf(Button));
					UTextBlock* Next = Named<UTextBlock>(UpSlot, TEXT("NextValueText"));
					UTextBlock* Arrow = Named<UTextBlock>(UpSlot, TEXT("NextValueArrow"));
					UWidget* LockedOverlay = UpSlot ? UpSlot->GetWidgetFromName(TEXT("LockedOverlay")) : nullptr;
					const FString Chip = ChipText(Button);
					if (!ChipShown(Button))
					{
						++Maxed;
						C.True(FString::Printf(TEXT("최대 줄 버튼이 회색 %s"), *ColorStr(Face)), IsGray(Face));
						C.True(TEXT("최대 줄에 잠금 덮개가 없다"), !IsShown(LockedOverlay));
						C.True(TEXT("서랍 강화 줄에 화살표 NextValueArrow 가 있다(이름이 없으면 C++ 가 못 접는다)"), Arrow != nullptr);
						C.True(FString::Printf(TEXT("최대 줄은 다음 값을 보이지 않는다(다음 값 「%s」 %s)"),
							Next ? *Next->GetText().ToString() : TEXT("?"), IsShown(Next) ? TEXT("보임") : TEXT("숨김")),
							!IsShown(Next) && !IsShown(Arrow));
					}
					else
					{
						C.True(FString::Printf(TEXT("강화 가능/돈 부족 줄 버튼은 초록 유지 %s"), *ColorStr(Face)), IsGreen(Face));
						UTextBlock* Amount = ChipAmount(Button);
						const FLinearColor AmountInk = Amount ? Amount->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::White;
						const bool bRed = AmountInk.R > 0.5f && AmountInk.G < 0.2f && AmountInk.B < 0.2f;
						(bRed ? Short : Open) += 1;
					}
				}
				C.True(FString::Printf(TEXT("강화 상태 3종이 다 있다(가능 %d · 돈 부족 %d · 최대 %d)"), Open, Short, Maxed), Open >= 1 && (Short >= 1 || !bRequireShort) && Maxed >= 1);
			};

		{
			FSceneSpec S;
			S.Label = TEXT("factory_upgrade");
			S.bUpgradePage = true;
			S.Seed = [](FDrawerFixtures& W, FFixtureBackup& B)
			{
				TMap<EWorldFactoryUpgradeType, int32> Levels;
				int32 RushMax = 10;
				FWorldFactoryUpgradeDefinition Def;
				if (W.Tables->GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType::RushProduction, Def) && Def.MaxLevel > 0) { RushMax = Def.MaxLevel; }
				Levels.Add(EWorldFactoryUpgradeType::LineExpansion, 0);
				Levels.Add(EWorldFactoryUpgradeType::Automation, 1);
				Levels.Add(EWorldFactoryUpgradeType::QualityControl, 2);
				Levels.Add(EWorldFactoryUpgradeType::StorageExpansion, 3);
				Levels.Add(EWorldFactoryUpgradeType::RushProduction, RushMax);
				W.SeedFactory(W.FactoryCountry, { W.Completed(1, W.Products[0]) }, Levels, B.Factory);
				// 가장 비싼 강화 하나만 돈이 모자라게
				int64 MaxCost = 0;
				for (EWorldFactoryUpgradeType T : { EWorldFactoryUpgradeType::LineExpansion, EWorldFactoryUpgradeType::Automation,
					EWorldFactoryUpgradeType::QualityControl, EWorldFactoryUpgradeType::StorageExpansion })
				{
					MaxCost = FMath::Max(MaxCost, W.FactoryMgr->GetUpgradeCost(W.FactoryCountry, T));
				}
				W.SetMoney(FMath::Max<int64>(0, MaxCost - 1));
			};
			S.Extra = [UpgradeStates](FChecker& C, UCountryDetailWidget&, const FImage& Frame, const FRows& Rows)
			{
				UpgradeStates(C, Frame, Rows, true);
			};
			Scenes.Add(S);
		}

		auto MineBase = [](FDrawerFixtures& W, FFixtureBackup& B, bool bPending)
		{
			TMap<EMineUpgradeType, int32> Levels;
			// 빈 라인이 보이도록 라인 증설로 3칸 이상
			for (int32 Level = 0; Level <= 10; ++Level)
			{
				Levels.Add(EMineUpgradeType::LineExpansion, Level);
				W.SeedMine(W.MineCountry, {}, Levels, B.Mine);
				if (W.MineMgr->GetMaxLines(W.MineCountry) >= 3) { break; }
			}
			TArray<FMineLineState> Lines = { W.MineLine(W.MineResources[0], 5, 0.6), W.MineLine(W.MineResources[1], 5, 1.0) };
			W.SeedMine(W.MineCountry, Lines, Levels, B.Mine);
			W.SetMoney(1000000000);
			if (bPending)
			{
				const FName Key = W.MinePendingKey(W.MineCountry, W.MineResources[0]);
				if (W.Cloud->BeginPendingAction(Key)) { B.PendingKeys.Add(Key); }
			}
		};
		{
			FSceneSpec S;
			S.Label = TEXT("mine_production");
			S.Facility = EDioramaModule::Mine;
			S.Seed = [MineBase](FDrawerFixtures& W, FFixtureBackup& B) { MineBase(W, B, false); };
			S.Extra = [CheckInstant](FChecker& C, UCountryDetailWidget& Detail, const FImage& Frame, const FRows& Rows)
			{
				C.True(FString::Printf(TEXT("광산 줄 = 채광 중·가득 참·빈 라인(+잠긴 줄) (%d줄)"), Rows.Num()),
					Rows.Num() >= 3 && Rows[0].Kind == ERowKind::Mine && Rows[1].Kind == ERowKind::Mine && Rows[2].Kind == ERowKind::Empty);
				if (Rows.Num() >= 2)
				{
					CheckInstant(C, Frame, Rows[0], true);
					UWidget* Receive = Rows[1].Row->GetWidgetFromName(TEXT("RecieveButton"));
					const FLinearColor Face = ButtonFace(Frame, BoxOf(Receive));
					C.True(FString::Printf(TEXT("가득 참 줄 받기 버튼이 금색 %s"), *ColorStr(Face)), IsShown(Receive) && IsGold(Face));
					const UMineLineWidget* FullLine = Cast<UMineLineWidget>(Rows[1].Row);
					UObject* Want = FullLine ? ResourceIcon(Detail, FullLine->GetResource()) : nullptr;
					C.True(FString::Printf(TEXT("받기 칩 = 그 라인 자원 아이콘(%s)"), Want ? *Want->GetName() : TEXT("?")),
						Want && ChipIcon(Receive) == Want);
					CheckChipCount(C, Frame, Receive, TEXT("광산 가득 참 줄 받기"), true);
					CheckChipCount(C, Frame, Rows[0].Row->GetWidgetFromName(TEXT("InstantFinishButton")), TEXT("광산 즉시 완료 다이아"), false);
					for (int32 I = 0; I < 2; ++I)
					{
						UTextBlock* Number = Named<UTextBlock>(Rows[I].Row, TEXT("LineNumberText"));
						C.True(FString::Printf(TEXT("광산 줄%d 번호 원 「%d」(공장 줄과 같은 문법)"), I + 1, I + 1),
							IsShown(Named<UWidget>(Rows[I].Row, TEXT("NumberBadge"))) && Number && Number->GetText().ToString() == FString::FromInt(I + 1));
					}
				}
				UDrawerHeroRowWidget* Hero = Named<UDrawerHeroRowWidget>(&Detail, TEXT("MineHero"));
				UWidget* HeroButton = Hero ? Hero->GetWidgetFromName(TEXT("ActionButton")) : nullptr;
				UObject* Box = ResourceIcon(Detail, EResourceType::Box);
				C.True(TEXT("자원이 섞인 「모두 받기」 칩 = 상자 아이콘"), Box && ChipIcon(HeroButton) == Box);
				CheckChipCount(C, Frame, HeroButton, TEXT("광산 「모두 받기」"), true);
			};
			Scenes.Add(S);
		}
		{
			// 밝은 자원색(리튬 UIColor ≈ #EEF3F9)이 흰 칩 위에서 사라지지 않는가 + 한 자원 「모두 받기」 칩 = 그 자원 아이콘
			FSceneSpec S;
			S.Label = TEXT("mine_lithium");
			S.Facility = EDioramaModule::Mine;
			S.bLithiumCountry = true;
			S.Seed = [](FDrawerFixtures& W, FFixtureBackup& B)
			{
				TMap<EMineUpgradeType, int32> Levels;
				Levels.Add(EMineUpgradeType::LineExpansion, 0);
				W.SeedMine(W.LithiumCountry, { W.MineLine(EResourceType::Lithium, 5, 1.0) }, Levels, B.Mine);
				W.SetMoney(1000000000);
			};
			S.Extra = [](FChecker& C, UCountryDetailWidget& Detail, const FImage& Frame, const FRows& Rows)
			{
				C.True(FString::Printf(TEXT("리튬 광산 첫 줄 = 가득 참 리튬 (%d줄)"), Rows.Num()), Rows.Num() >= 1 && Rows[0].Kind == ERowKind::Mine);
				UObject* Lithium = ResourceIcon(Detail, EResourceType::Lithium);
				if (Rows.Num() >= 1)
				{
					UWidget* Receive = Rows[0].Row->GetWidgetFromName(TEXT("RecieveButton"));
					C.True(TEXT("리튬 받기 칩 = 리튬 아이콘"), Lithium && ChipIcon(Receive) == Lithium);
					CheckChipCount(C, Frame, Receive, TEXT("리튬 받기"), true);
				}
				UDrawerHeroRowWidget* Hero = Named<UDrawerHeroRowWidget>(&Detail, TEXT("MineHero"));
				UWidget* HeroButton = Hero ? Hero->GetWidgetFromName(TEXT("ActionButton")) : nullptr;
				C.True(TEXT("한 자원 「모두 받기」 칩 = 그 자원(리튬) 아이콘"), Lithium && ChipIcon(HeroButton) == Lithium);
				CheckChipCount(C, Frame, HeroButton, TEXT("리튬 「모두 받기」"), true);
			};
			Scenes.Add(S);
		}
		{
			FSceneSpec S;
			S.Label = TEXT("mine_instant_locked");
			S.Facility = EDioramaModule::Mine;
			S.Seed = [MineBase](FDrawerFixtures& W, FFixtureBackup& B) { MineBase(W, B, true); };
			S.Extra = [CheckInstant](FChecker& C, UCountryDetailWidget&, const FImage& Frame, const FRows& Rows)
			{
				if (Rows.Num() > 0) { CheckInstant(C, Frame, Rows[0], false); }
			};
			Scenes.Add(S);
		}
		{
			FSceneSpec S;
			S.Label = TEXT("mine_upgrade");
			S.Facility = EDioramaModule::Mine;
			S.bUpgradePage = true;
			S.Seed = [MineBase](FDrawerFixtures& W, FFixtureBackup& B)
			{
				MineBase(W, B, false);
				TMap<EMineUpgradeType, int32> Levels;
				Levels.Add(EMineUpgradeType::LineExpansion, W.MineMgr->GetUpgradeLevel(W.MineCountry, EMineUpgradeType::LineExpansion));
				// 레벨 상한이 DT 에 있는 강화를 최대로(채광 속도는 상한 0 = 무제한이라 「최대」가 안 된다)
				FMineUpgradeDefinition Def;
				const int32 OfflineMax = W.Tables->GetMineUpgradeDefinition(EMineUpgradeType::OfflineCatchupBoost, Def) && Def.MaxLevel > 0 ? Def.MaxLevel : 0;
				Levels.Add(EMineUpgradeType::OfflineCatchupBoost, OfflineMax);
				Levels.Add(EMineUpgradeType::MiningRate, 3);
				Levels.Add(EMineUpgradeType::MiningStorage, 1);
				W.SeedMine(W.MineCountry, { W.MineLine(W.MineResources[0], 5, 0.6) }, Levels, B.Mine);
			};
			S.Extra = [UpgradeStates](FChecker& C, UCountryDetailWidget& Detail, const FImage& Frame, const FRows& Rows)
			{
				UpgradeStates(C, Frame, Rows, false);
				UGameInstance* AnyGI = Detail.GetGameInstance();
				UTableManagerSubsystem* Tables = AnyGI ? AnyGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
				FMineUpgradeDefinition Def;
				const bool bDef = Tables && Tables->GetMineUpgradeDefinition(EMineUpgradeType::OfflineCatchupBoost, Def) && Def.MaxLevel > 0;
				C.True(TEXT("오프라인 효율 DT 상한이 있다(최대 줄 픽스처 전제)"), bDef);
				bool bFound = false;
				for (const FRowResult& R : Rows)
				{
					UUpgradeSlot* UpSlot = Cast<UUpgradeSlot>(R.Row);
					UTextBlock* Title = Named<UTextBlock>(UpSlot, TEXT("DescriptionText"));
					if (bDef && Title && Title->GetText().EqualTo(Def.DisplayName))
					{
						bFound = true;
						C.True(TEXT("오프라인 효율 줄 = 레벨 상한 최대(칩 없음·다음 값 없음)"),
							!ChipShown(UpSlot->GetUpgradeButton()) && !IsShown(UpSlot->GetWidgetFromName(TEXT("NextValueText"))));
					}
				}
				C.True(TEXT("오프라인 효율 줄을 찾았다"), bFound);
			};
			Scenes.Add(S);
		}

		auto PortBase = [](FDrawerFixtures& W, int32 Orders, int32 Goods)
		{
			TArray<FTradeOrder> List;
			if (Orders >= 1) { List.Add(W.Order(9001, W.PortProducts[0], 80, ETradeOrderTier::Normal, 1.5f)); }
			if (Orders >= 2) { List.Add(W.Order(9002, W.PortProducts[1], 40, ETradeOrderTier::Urgent, 2.4f)); }
			W.SetOrders(List);
			TArray<TPair<FIntPoint, int64>> Stock;
			for (int32 I = 0; I < Goods; ++I)
			{
				Stock.Emplace(FIntPoint(static_cast<int32>(W.PortProducts[I].Company), W.PortProducts[I].Index), I == 0 ? 120 : 15 + I * 7);
			}
			W.SetWarehouse(Stock);
			W.SetMoney(1000000000);
		};
		for (const TTuple<const TCHAR*, int32, int32> Def : { MakeTuple(TEXT("port"), 2, 4), MakeTuple(TEXT("port_nogoods"), 2, 0), MakeTuple(TEXT("port_noorders"), 0, 4) })
		{
			FSceneSpec S;
			S.Label = Def.Get<0>();
			S.Facility = EDioramaModule::Port;
			const int32 Orders = Def.Get<1>();
			const int32 Goods = Def.Get<2>();
			S.Seed = [PortBase, Orders, Goods](FDrawerFixtures& W, FFixtureBackup&) { PortBase(W, Orders, Goods); };
			S.Extra = [Orders, Goods](FChecker& C, UCountryDetailWidget& Detail, const FImage& Frame, const FRows& Rows)
			{
				C.Near(TEXT("주문 줄 수"), Rows.Num(), Orders, 0.0);
				UDrawerHeroRowWidget* Hero = Named<UDrawerHeroRowWidget>(&Detail, TEXT("PortHero"));
				UWidget* Sell = Hero ? Hero->GetWidgetFromName(TEXT("ActionButton")) : nullptr;
				const FLinearColor SellFace = ButtonFace(Frame, BoxOf(Sell));
				if (Goods > 0)
				{
					C.True(FString::Printf(TEXT("팔 물건이 있으면 「모두 판매」 초록 %s"), *ColorStr(SellFace)), IsGreen(SellFace));
					UObject* Money = ResourceIcon(Detail, EResourceType::Money);
					C.True(FString::Printf(TEXT("「모두 판매」 돈 칩 「%s」"), *ChipText(Sell)),
						ChipShown(Sell) && !ChipText(Sell).IsEmpty() && Money && ChipIcon(Sell) == Money);
					CheckChipCount(C, Frame, Sell, TEXT("「모두 판매」"), true);
				}
				else
				{
					C.True(FString::Printf(TEXT("팔 물건이 없으면 「모두 판매」 회색 %s"), *ColorStr(SellFace)), IsGray(SellFace) && !ChipShown(Sell));
				}
				for (const FRowResult& R : Rows)
				{
					UTextBlock* Name = Named<UTextBlock>(R.Row, TEXT("NameText"));
					UTextBlock* Detail2 = Named<UTextBlock>(R.Row, TEXT("DetailText"));
					const FString NameStr = Name ? Name->GetText().ToString() : FString();
					const FString DetailStr = Detail2 ? Detail2->GetText().ToString() : FString();
					C.True(FString::Printf(TEXT("주문 이름 「%s」 = 「<등급> 주문 · <제품명>」"), *NameStr), NameStr.Contains(TEXT(" 주문 · ")));
					C.True(FString::Printf(TEXT("주문 설명 「%s」 = 「<N>개 납품 · 보상 ×<배율>」"), *DetailStr),
						DetailStr.Contains(TEXT("개 납품 · 보상 ×")));
				}
				C.True(TEXT("「주문」 소제목 = 주문이 있을 때만"), IsShown(Detail.GetWidgetFromName(TEXT("PortOrderHeader"))) == (Orders > 0));
				C.True(TEXT("빈 창고 안내 = 완제품이 없을 때만"), IsShown(Detail.GetWidgetFromName(TEXT("PortEmptyText"))) == (Goods == 0));
				UUniformGridPanel* Grid = Named<UUniformGridPanel>(&Detail, TEXT("PortGoodsList"));
				int32 Tiles = 0;
				for (UWidget* Child : Grid ? Grid->GetAllChildren() : TArray<UWidget*>()) { Tiles += Cast<UDrawerGoodsTileWidget>(Child) && IsShown(Child) ? 1 : 0; }
				C.Near(TEXT("완제품 칸 수"), Tiles, Goods, 0.0);
			};
			Scenes.Add(S);
		}
		return Scenes;
	}
}

// ClientContext ― 에디터(PIE 없는) 시험 목록에는 안 뜨고 격리 -game 러너에서만 돈다(플레이 월드가 필요하다)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerLayoutGeometryTest, "CGR.Overseas.DrawerLive.LayoutGeometry",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FOverseasDrawerLayoutGeometryTest::RunTest(const FString& Parameters)
{
	using namespace CGRDrawerLive;
	UWorld* PlayWorld = FindPlayWorld();
	UGameInstance* GI = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
	if (!FApp::CanEverRender() || !GI)
	{
		AddError(TEXT("렌더 가능한 플레이 월드(-game)가 필요합니다 ― 값이 전부 0 이면 겹침 단언이 조용히 통과합니다."));
		return false;
	}
	FDrawerFixtures W;
	FString Error;
	if (!W.Resolve(GI, Error)) { AddError(Error); return false; }
	UWorldMapPanelWidget* Panel = CreateWidget<UWorldMapPanelWidget>(GI, W.Tables->GetWidgetClass(EWidgetType::WorldMapPanel));
	if (!TestNotNull(TEXT("월드맵 패널"), Panel) || !DisableAutoActivate(Panel)) { return false; }
	AddInfo(FString::Printf(TEXT("픽스처 나라: 공장 %d · 광산 %d(자원 %d종) · 무역항 %d · 최장 제품명 「%s」"),
		static_cast<int32>(W.FactoryCountry), static_cast<int32>(W.MineCountry), W.MineResources.Num(),
		static_cast<int32>(W.PortCountry), *W.Longest.Name.ToString()));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FLayoutGeometryCommand>(this, Panel, W, BuildScenes()));
	return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// CGR.Overseas.DrawerLive.LiveFlows ― 실제 뷰포트에 올린 월드맵에서 Slate 입력으로 흐름을 탄다.
// 누름 입력 = 터치 손가락 1번(실제 마우스 커서 포인터와 상태를 안 나눈다), 누르기 전 캡처·눌린 버튼·히트 사슬을 단언.
// 대기 = 조건 폴링(시간 상한만 둔다). 빨간 점(D4)·페이지 유지는 입력과 무관하게 SimulateClick 으로 페이지를 고른다.
// ═════════════════════════════════════════════════════════════════════════════
namespace CGRDrawerLive
{
	static TSet<FKey> GNoButtons;
	static constexpr uint32 TapFinger = 1;

	static FWidgetPath HitPathAt(const FVector2D& At)
	{
		FSlateApplication& App = FSlateApplication::Get();
		TArray<TSharedRef<SWindow>> Windows;
		App.GetAllVisibleWindowsOrdered(Windows);
		return App.LocateWindowUnderMouse(At, Windows, false);
	}

	static bool PathContains(const FWidgetPath& Path, const UWidget* Target)
	{
		const TSharedPtr<SWidget> TargetSlate = Target ? Target->GetCachedWidget() : nullptr;
		if (!TargetSlate.IsValid()) { return false; }
		for (int32 I = 0; I < Path.Widgets.Num(); ++I)
		{
			if (Path.Widgets[I].Widget == TargetSlate.ToSharedRef()) { return true; }
		}
		return false;
	}

	static bool PathHasButton(const FWidgetPath& Path)
	{
		for (int32 I = 0; I < Path.Widgets.Num(); ++I)
		{
			if (Path.Widgets[I].Widget->GetTypeAsString().Contains(TEXT("Button"))) { return true; }
		}
		return false;
	}

	static FString HitReport(const FVector2D& At, const UWidget* Target)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const FWidgetPath Path = HitPathAt(At);
		FString Tail;
		for (int32 I = FMath::Max(0, Path.Widgets.Num() - 4); I < Path.Widgets.Num(); ++I)
		{
			Tail += Path.Widgets[I].Widget->GetTypeAsString() + TEXT(">");
		}
		const FVector2D RealCursor = App.GetCursorPos();
		return FString::Printf(TEXT("지점(%.0f,%.0f) 대상%s 사슬끝[%s] 앱활성=%d 실제커서(%.0f,%.0f) 캡처=%d 눌린마우스=%d"),
			At.X, At.Y, PathContains(Path, Target) ? TEXT("맞음") : TEXT("빗나감"), *Tail, App.IsActive() ? 1 : 0,
			RealCursor.X, RealCursor.Y, App.HasAnyMouseCaptor() ? 1 : 0, App.GetPressedMouseButtons().Num());
	}

	// 누르기 전 단언 ― 캡처 없음 · 눌린 마우스 버튼 없음 · 대상이 히트 사슬 안
	static bool ReadyToPress(const FVector2D& At, const UWidget* Target)
	{
		FSlateApplication& App = FSlateApplication::Get();
		return !App.HasAnyMouseCaptor() && App.GetPressedMouseButtons().Num() == 0 && PathContains(HitPathAt(At), Target);
	}

	static void TouchTap(const FVector2D& At)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const uint32 User = static_cast<uint32>(App.GetUserIndexForMouse());
		App.ProcessTouchStartedEvent(nullptr, FPointerEvent(User, TapFinger, At, At, 1.f, true));
		App.ProcessTouchEndedEvent(FPointerEvent(User, TapFinger, At, At, 1.f, false));
	}

	static void SlateWheel(const FVector2D& At, float Delta)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const uint32 User = static_cast<uint32>(App.GetUserIndexForMouse());
		const uint32 Pointer = FSlateApplicationBase::CursorPointerIndex;
		App.ProcessMouseMoveEvent(FPointerEvent(User, Pointer, At, At + FVector2D(1, 1), GNoButtons, EKeys::Invalid, 0.f, FModifierKeysState()));
		App.ProcessMouseWheelOrGestureEvent(FPointerEvent(User, Pointer, At, At, GNoButtons, EKeys::Invalid, Delta, FModifierKeysState()), nullptr);
	}

	static void TouchDrag(const FVector2D& From, const FVector2D& To, int32 Steps)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const uint32 User = static_cast<uint32>(App.GetUserIndexForMouse());
		App.ProcessTouchStartedEvent(nullptr, FPointerEvent(User, TapFinger, From, From, 1.f, true));
		FVector2D Last = From;
		for (int32 I = 1; I <= Steps; ++I)
		{
			const FVector2D Next = FMath::Lerp(From, To, static_cast<double>(I) / Steps);
			App.ProcessTouchMovedEvent(FPointerEvent(User, TapFinger, Next, Last, 1.f, true));
			Last = Next;
		}
		App.ProcessTouchEndedEvent(FPointerEvent(User, TapFinger, To, Last, 1.f, false));
	}

	// 실제 뷰포트 기하: 절대 좌표 = 데스크톱 좌표
	static FVector2D AbsCenter(const UWidget* Widget)
	{
		const FGeometry& Geo = Widget->GetCachedGeometry();
		return Geo.LocalToAbsolute(Geo.GetLocalSize() * 0.5);
	}

	static double LocalWidth(const UWidget* Widget)
	{
		return Widget ? Widget->GetCachedGeometry().GetLocalSize().X : 0.0;
	}

	static bool HasGeometry(const UWidget* Widget)
	{
		return Widget && IsShown(Widget) && Widget->GetCachedGeometry().GetLocalSize().X > 0.5;
	}

	/** 단계 = 조건(매 프레임 폴링, 상한 초) → 동작. 조건이 상한 안에 안 서면 그 이름으로 실패를 남기고, bContinueOnTimeout 이면 동작을 그대로 탄다. */
	struct FFlowStep
	{
		FString Name;
		TFunction<bool()> Ready;
		TFunction<void()> Act;
		double Timeout = 3.0;
		bool bContinueOnTimeout = false;
	};

	class FLiveFlowsCommand : public IAutomationLatentCommand
	{
	public:
		FLiveFlowsCommand(FAutomationTestBase* InTest, UWorld* InPlayWorld, const FDrawerFixtures& InWorld, UUIBase* InUIBase, UBuildOpenWidget* InEntry)
			: Test(InTest), PlayWorld(InPlayWorld), W(InWorld), UIBase(InUIBase), Entry(InEntry)
		{
			Backup.Capture(W.GI);
			bReducedBefore = FMotionGate::IsReduced();
			FMotionGate::SetReduced(true);
			// 해금 게이트 = EntryRoundTrip 과 같은 픽스처(매니저가 없으면 열어 둔다). 끝나면 되돌린다
			CachedSaveProp = FindFProperty<FObjectProperty>(UBuildOpenWidget::StaticClass(), TEXT("CachedSaveMgr"));
			if (CachedSaveProp)
			{
				OriginalCachedSave = CachedSaveProp->GetObjectPropertyValue_InContainer(Entry);
				CachedSaveProp->SetObjectPropertyValue_InContainer(Entry, nullptr);
			}
			BuildSteps();
			StepStartedAt = FPlatformTime::Seconds();
		}

		virtual ~FLiveFlowsCommand() override
		{
			if (UWorldMapPanelWidget* P = Panel.Get())
			{
				if (P->IsActivated()) { P->DeactivateWidget(); }
			}
			Backup.Restore(W.GI);
			FMotionGate::SetReduced(bReducedBefore);
			if (CachedSaveProp) { CachedSaveProp->SetObjectPropertyValue_InContainer(Entry, OriginalCachedSave); }
		}

		virtual bool Update() override
		{
			if (StepIndex >= Steps.Num()) { return true; }
			FFlowStep& Step = Steps[StepIndex];
			++FramesInStep;
			const double Waited = FPlatformTime::Seconds() - StepStartedAt;
			// 같은 프레임에서 두 단계를 넘지 않는다 ― 앞 동작의 레이아웃·라우터 갱신이 한 프레임은 돌아야 한다
			const bool bReady = FramesInStep >= 2 && (!Step.Ready || Step.Ready());
			if (!bReady && Waited < Step.Timeout) { return false; }
			if (!bReady)
			{
				Test->AddError(FString::Printf(TEXT("[흐름 %d] 「%s」 조건이 %.1f초 안에 서지 않았습니다"), StepIndex, *Step.Name, Step.Timeout));
				if (!Step.bContinueOnTimeout) { return true; }
			}
			if (Step.Act) { Step.Act(); }
			++StepIndex;
			FramesInStep = 0;
			StepStartedAt = FPlatformTime::Seconds();
			return StepIndex >= Steps.Num();
		}

	private:
		UCountryDetailWidget* Detail() const { return ActiveDetail(Panel.Get()); }
		UDrawerPageToggleWidget* Toggle() const { return Named<UDrawerPageToggleWidget>(Detail(), TEXT("PageToggle")); }
		UButtonWidget* Segment(bool bUpgrade) const
		{
			return Named<UButtonWidget>(Toggle(), bUpgrade ? TEXT("UpgradeSegment") : TEXT("ProductionSegment"));
		}
		bool OnPage(bool bUpgrade) const
		{
			UDrawerPageToggleWidget* T = Toggle();
			return T && T->GetPage() == (bUpgrade ? CountryDrawer::EDrawerPage::Upgrade : CountryDrawer::EDrawerPage::Production);
		}
		bool DotShown() const { return IsShown(Toggle() ? Toggle()->GetWidgetFromName(TEXT("ProductionDot")) : nullptr); }
		bool DrawerOpenOn(ECountryType Country) const
		{
			UCountryDetailWidget* D = Detail();
			return D && D->IsActivated() && D->GetCurrentCountry() == Country && HasGeometry(Toggle());
		}
		bool DrawerClosed() const
		{
			UCountryDetailWidget* D = Detail();
			return !(D && D->IsActivated());
		}
		void PressCheck(const TCHAR* What, const UWidget* Target, const FVector2D& At)
		{
			const bool bOk = Target && ReadyToPress(At, Target);
			Test->AddInfo(FString::Printf(TEXT("[누름] %s %s"), What, *HitReport(At, Target)));
			Test->TestTrue(FString::Printf(TEXT("%s 누르기 전: 캡처 없음·눌린 마우스 없음·대상이 히트 사슬 안"), What), bOk);
		}
		void Esc()
		{
			FTSTicker::GetCoreTicker().Tick(0.016f);
			UCommonUIActionRouterBase* Router = UCommonUIActionRouterBase::Get(*Panel);
			LastEsc = Router ? Router->ProcessInput(EKeys::Escape, IE_Pressed) : ERouteUIInputResult::Unhandled;
			if (Router) { Router->ProcessInput(EKeys::Escape, IE_Released); }
		}
		void SeedFactoryLines(int32 Slots, const TArray<FWorldFactoryLineState>& Lines)
		{
			const int32 Level = W.FactoryLevelForSlots(W.FactoryCountry, Slots, Backup.Factory);
			TMap<EWorldFactoryUpgradeType, int32> Levels;
			Levels.Add(EWorldFactoryUpgradeType::LineExpansion, Level);
			W.SeedFactory(W.FactoryCountry, Lines, Levels, Backup.Factory);
		}
		void Add(const TCHAR* Name, TFunction<bool()> Ready, TFunction<void()> Act, double Timeout = 3.0, bool bContinue = false)
		{
			FFlowStep Step;
			Step.Name = Name;
			Step.Ready = MoveTemp(Ready);
			Step.Act = MoveTemp(Act);
			Step.Timeout = Timeout;
			Step.bContinueOnTimeout = bContinue;
			Steps.Add(MoveTemp(Step));
		}

		void BuildSteps()
		{
			Add(TEXT("시작 전 열린 프롬프트 정리"), [this]()
			{
				// 기동 직후 안내 창(출석·공지)이 진입 가드를 막는다 ― 격리 세션이니 닫는다
				if (UIBase->GetPromptStackCount() > 0 && (FramesInStep % 20) == 2)
				{
					if (UCommonActivatableWidget* Top = UIBase->GetActivePromptWidget())
					{
						Test->AddInfo(FString::Printf(TEXT("시작 전 열린 프롬프트 닫기: %s"), *Top->GetClass()->GetName()));
						Top->DeactivateWidget();
					}
				}
				return UIBase->GetPromptStackCount() == 0;
			}, [this]()
			{
				AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(PlayWorld->GetFirstPlayerController());
				if (PC && PC->GetCurrentInputMode() == EInputMode::UI) { PC->GoToNormalMode(); }
				Test->TestTrue(TEXT("도크 월드맵 버튼 핸들러"), CallNoArg(Entry, TEXT("OnWorldMapButtonClicked")));
			}, 10.0);

			Add(TEXT("월드맵이 프롬프트 스택에 올라와 활성"), [this]()
			{
				UWorldMapPanelWidget* P = Cast<UWorldMapPanelWidget>(UIBase->GetActivePromptWidget());
				return P && P->IsActivated();
			}, [this]()
			{
				Panel.Reset(Cast<UWorldMapPanelWidget>(UIBase->GetActivePromptWidget()));
				if (UCommonActivatableWidgetStack* Drawers = Cast<UCommonActivatableWidgetStack>(Panel->GetWidgetFromName(TEXT("DrawerStack"))))
				{
					Drawers->SetTransitionDuration(0.0f);
				}
				// 1번 라인은 시드 후 6초 뒤 저절로 끝난다 ― 강화를 보는 동안 끝나야 한다
				SeedFactoryLines(3, { W.MakeLine(1, W.Products[0], 12, 2.f, 0.0), W.Producing(2, W.Products[1]) });
				Test->TestTrue(TEXT("공장 나라 핀 클릭"), ClickPin(Panel.Get(), W.FactoryCountry));
			}, 5.0);

			// ── 입력 경로: 터치 탭으로 「강화」 ──
			// 누를 수 있을 때까지(대상이 히트 사슬 맨 위 ― 스택 전환 중 입력 차단 막이 걷힐 때까지) 기다린 뒤 누른다
			Add(TEXT("공장 서랍이 열리고 「강화」 칸이 눌릴 수 있음"), [this]()
			{
				return DrawerOpenOn(W.FactoryCountry) && HasGeometry(Segment(true)) && ReadyToPress(AbsCenter(Segment(true)), Segment(true));
			}, [this]()
			{
				const FVector2D At = AbsCenter(Segment(true));
				PressCheck(TEXT("「강화」 칸"), Segment(true), At);
				TouchTap(At);
			});
			Add(TEXT("터치 탭으로 강화 페이지"), [this]() { return OnPage(true); }, [this]()
			{
				Test->TestTrue(TEXT("터치 탭으로 강화 페이지가 열린다(토글 히트테스트 포함)"), OnPage(true));
				// 빨간 점 검사는 입력과 무관해야 한다 ― 탭이 실패했으면 그룹 경로로 강화를 고른다
				if (!OnPage(true) && Segment(true)) { Segment(true)->SimulateClick(); }
			}, 2.0, true);
			Steps[Steps.Num() - 2].bContinueOnTimeout = true;

			// ── D4: 강화 보는 중 자연 완료 → 빨간 점 ──
			Add(TEXT("강화 페이지(완료 전)"), [this]() { return OnPage(true); }, [this]()
			{
				Test->TestTrue(TEXT("완료 전: 강화 페이지 + 아직 받을 것이 없어 빨간 점 없음"), OnPage(true) && !DotShown());
				FWorldFactoryLineState State;
				Test->TestTrue(TEXT("완료 전 전제: 1번 라인이 아직 진행 중"), W.FactoryMgr->GetLineState(W.FactoryCountry, 1, State) && !State.bCompleted);
			});
			Add(TEXT("1번 라인이 매니저에서 끝남"), [this]()
			{
				FWorldFactoryLineState State;
				return W.FactoryMgr->GetLineState(W.FactoryCountry, 1, State) && State.bCompleted;
			}, nullptr, 15.0);
			Add(TEXT("빨간 점이 뜸"), [this]() { return DotShown(); }, [this]()
			{
				Test->TestTrue(TEXT("★강화를 보는 중 라인이 저절로 끝나면 「생산」 칸에 빨간 점이 뜬다"), DotShown() && OnPage(true));
				Test->TestTrue(TEXT("다른 나라 핀 클릭"), W.OtherFactoryCountry != ECountryType::None && ClickPin(Panel.Get(), W.OtherFactoryCountry));
			}, 1.5, true);   // 경계 타이머는 완료 순간 +0.05초 ― 10초 상한이 대신 통과시키지 못하게 짧게

			// ── 나라 전환·같은 나라 재클릭 ──
			Add(TEXT("다른 나라 서랍"), [this]() { return DrawerOpenOn(W.OtherFactoryCountry); }, [this]()
			{
				Test->TestTrue(TEXT("다른 나라를 누르면 생산 페이지로 돌아간다"), OnPage(false));
				Test->TestTrue(TEXT("공장 나라 다시 클릭"), ClickPin(Panel.Get(), W.FactoryCountry));
			});
			Add(TEXT("공장 나라로 돌아옴"), [this]() { return DrawerOpenOn(W.FactoryCountry); }, [this]()
			{
				if (Segment(true)) { Segment(true)->SimulateClick(); }
			});
			Add(TEXT("강화 페이지"), [this]() { return OnPage(true); }, [this]()
			{
				Test->TestTrue(TEXT("같은 나라 재클릭"), ClickPin(Panel.Get(), W.FactoryCountry));
			});
			Add(TEXT("재클릭 후 몇 프레임"), [this]() { return FramesInStep >= 6 && DrawerOpenOn(W.FactoryCountry); }, [this]()
			{
				Test->TestTrue(TEXT("같은 나라를 다시 누르면 보던 강화 페이지를 지킨다"), OnPage(true));
				Esc();
			});

			// ── 닫기 순서: ESC → X → 바깥 ──
			Add(TEXT("ESC 로 서랍 닫힘"), [this]() { return DrawerClosed(); }, [this]()
			{
				Test->TestTrue(FString::Printf(TEXT("ESC 가 라우터에서 처리된다(%d)"), static_cast<int32>(LastEsc)), LastEsc == ERouteUIInputResult::Handled);
				Test->TestTrue(TEXT("ESC 1번 = 서랍만 닫히고 월드맵은 남는다"), DrawerClosed() && Panel->IsActivated());
				ClickPin(Panel.Get(), W.FactoryCountry);
			}, 2.0, true);
			Add(TEXT("X 검증용 서랍(X 가 눌릴 수 있음)"), [this]()
			{
				UPanelHeaderWidget* Header = Named<UPanelHeaderWidget>(Detail(), TEXT("PanelHeader"));
				UCloseButtonWidget* Close = Header ? Header->GetCloseButton() : nullptr;
				return DrawerOpenOn(W.FactoryCountry) && HasGeometry(Close) && ReadyToPress(AbsCenter(Close), Close);
			}, [this]()
			{
				UPanelHeaderWidget* Header = Named<UPanelHeaderWidget>(Detail(), TEXT("PanelHeader"));
				UCloseButtonWidget* Close = Header ? Header->GetCloseButton() : nullptr;
				if (!Close) { Test->AddError(TEXT("헤더 X 없음")); return; }
				const FVector2D At = AbsCenter(Close);
				PressCheck(TEXT("헤더 X"), Close, At);
				TouchTap(At);
			});
			Add(TEXT("X 탭으로 서랍 닫힘"), [this]() { return DrawerClosed(); }, [this]()
			{
				Test->TestTrue(TEXT("X 터치 탭 = 서랍이 닫히고 월드맵은 남는다"), DrawerClosed() && Panel->IsActivated());
				ClickPin(Panel.Get(), W.FactoryCountry);
			}, 2.0, true);
			Steps[Steps.Num() - 2].bContinueOnTimeout = true;
			Add(TEXT("바깥 검증용 서랍"), [this]() { return DrawerOpenOn(W.FactoryCountry); }, [this]()
			{
				UWidget* Shell = Detail()->GetWidgetFromName(TEXT("ShellVBox"));
				const FGeometry& ShellGeo = Shell->GetCachedGeometry();
				const FVector2D ShellLeft = ShellGeo.LocalToAbsolute(FVector2D(0, ShellGeo.GetLocalSize().Y * 0.5));
				const FGeometry& PanelGeo = Panel->GetCachedGeometry();
				const FVector2D PanelLeft = PanelGeo.LocalToAbsolute(FVector2D::ZeroVector);
				const FVector2D PanelBottom = PanelGeo.LocalToAbsolute(PanelGeo.GetLocalSize());
				// 지도 쪽에서 버튼(핀·말풍선)이 히트 사슬에 없는 지점을 고른다 ― 핀을 누르면 나라가 바뀌어 「안 닫힘」이 공허해진다
				OutsidePoint = FVector2D::ZeroVector;
				for (int32 Gy = 1; Gy <= 7 && OutsidePoint.IsZero(); ++Gy)
				{
					for (int32 Gx = 1; Gx <= 7 && OutsidePoint.IsZero(); ++Gx)
					{
						const FVector2D Candidate(FMath::Lerp(PanelLeft.X, ShellLeft.X, Gx / 8.0), FMath::Lerp(PanelLeft.Y, PanelBottom.Y, Gy / 8.0));
						if (!PathHasButton(HitPathAt(Candidate))) { OutsidePoint = Candidate; }
					}
				}
				Test->TestFalse(TEXT("지도 쪽에 버튼이 없는 바깥 지점을 찾았다"), OutsidePoint.IsZero());
				Test->AddInfo(TEXT("[누름] 서랍 바깥(지도) ") + HitReport(OutsidePoint, nullptr));
				TouchTap(OutsidePoint);
			});
			Add(TEXT("바깥 탭 뒤 몇 프레임"), [this]() { return FramesInStep >= 6; }, [this]()
			{
				// 판정(컨트롤러): 바깥 = 지도라 서랍을 닫지 않는다
				Test->TestTrue(TEXT("서랍 바깥(지도, 버튼 없는 곳) 탭은 서랍을 닫지 않는다"), !DrawerClosed());
				Test->TestTrue(TEXT("바깥 탭 뒤에도 서랍 나라가 그대로다"), Detail() && Detail()->GetCurrentCountry() == W.FactoryCountry);
				Test->TestTrue(TEXT("바깥 탭 뒤에도 월드맵은 남는다"), Panel->IsActivated());
				SeedFactoryLines(5, { W.Producing(1, W.Products[0]), W.Completed(2, W.Products[1]), W.Producing(3, W.Products[2]),
					W.Completed(4, W.Products[3]), W.Producing(5, W.Products[4]) });
				if (UCountryDetailWidget* D = Detail()) { D->SetCurrentCountry(W.FactoryCountry); }
				if (Segment(false)) { Segment(false)->SimulateClick(); }
			});

			// ── 스크롤: 막대 이벤트·휠·끌기·막대 사라짐 ──
			Add(TEXT("라인 5개 = 스크롤 + 막대 여백"), [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				const UVerticalBoxSlot* ScrollSlot = Scroll ? Cast<UVerticalBoxSlot>(Scroll->Slot) : nullptr;
				return OnPage(false) && Scroll && Scroll->GetScrollOffsetOfEnd() > 0.5f && ScrollSlot && ScrollSlot->GetPadding().Right < -0.5f;
			}, [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				UPanelWidget* List = Named<UPanelWidget>(Detail(), TEXT("FactoryLineList"));
				const UVerticalBoxSlot* ScrollSlot = Scroll ? Cast<UVerticalBoxSlot>(Scroll->Slot) : nullptr;
				Test->TestTrue(FString::Printf(TEXT("라인 5개 = 스크롤 생김(넘침 %.0f) + 막대 이벤트로 스크롤 상자가 판 여백으로(오른쪽 %.1f)"),
					Scroll ? Scroll->GetScrollOffsetOfEnd() : -1.f, ScrollSlot ? ScrollSlot->GetPadding().Right : 0.f),
					Scroll && Scroll->GetScrollOffsetOfEnd() > 0.5f && ScrollSlot && ScrollSlot->GetPadding().Right < -0.5f);
				Test->TestEqual(TEXT("막대가 보여도 줄 폭 1064"), LocalWidth(List && List->GetChildrenCount() > 0 ? List->GetChildAt(0) : nullptr), 1064.0, 1.0);
				UUserWidget* Row = List && List->GetChildrenCount() > 1 ? Cast<UUserWidget>(List->GetChildAt(1)) : nullptr;
				UWidget* Body = Row ? Row->GetWidgetFromName(TEXT("BarSize")) : nullptr;
				if (!Body || !Scroll) { Test->AddError(TEXT("줄 몸통(막대) 위젯 없음")); return; }
				Scroll->ScrollToStart();
				BodyPoint = AbsCenter(Body);
				PressCheck(TEXT("줄 몸통(휠)"), Scroll, BodyPoint);
				SlateWheel(BodyPoint, -1.f);
			}, 3.0, true);
			Add(TEXT("휠로 내려감"), [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				return Scroll && Scroll->GetScrollOffset() > 1.f;
			}, [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				Test->TestTrue(FString::Printf(TEXT("줄 몸통 위 휠 = 목록이 내려간다(오프셋 %.0f)"), Scroll ? Scroll->GetScrollOffset() : 0.f),
					Scroll && Scroll->GetScrollOffset() > 1.f);
				if (Scroll) { Scroll->ScrollToStart(); }
			}, 2.0, true);
			Add(TEXT("목록 처음으로"), [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				return Scroll && Scroll->GetScrollOffset() < 0.5f;
			}, [this]()
			{
				PressCheck(TEXT("줄 몸통(끌기)"), Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll")), BodyPoint);
				TouchDrag(BodyPoint, BodyPoint - FVector2D(0, 220), 12);
			});
			Add(TEXT("끌기로 내려감"), [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				return Scroll && Scroll->GetScrollOffset() > 1.f;
			}, [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				Test->TestTrue(FString::Printf(TEXT("줄 몸통을 끌면 목록이 내려간다(오프셋 %.0f)"), Scroll ? Scroll->GetScrollOffset() : 0.f),
					Scroll && Scroll->GetScrollOffset() > 1.f);
				SeedFactoryLines(3, { W.Producing(1, W.Products[0]) });
				if (UCountryDetailWidget* D = Detail()) { D->SetCurrentCountry(W.FactoryCountry); }
			}, 2.0, true);
			Add(TEXT("라인이 줄면 막대·여백 사라짐"), [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				const UVerticalBoxSlot* ScrollSlot = Scroll ? Cast<UVerticalBoxSlot>(Scroll->Slot) : nullptr;
				return Scroll && Scroll->GetScrollOffsetOfEnd() <= 0.5f && ScrollSlot && FMath::IsNearlyZero(ScrollSlot->GetPadding().Right);
			}, [this]()
			{
				UScrollBox* Scroll = Named<UScrollBox>(Detail(), TEXT("FactoryLineScroll"));
				const UVerticalBoxSlot* ScrollSlot = Scroll ? Cast<UVerticalBoxSlot>(Scroll->Slot) : nullptr;
				UPanelWidget* List = Named<UPanelWidget>(Detail(), TEXT("FactoryLineList"));
				Test->TestTrue(TEXT("라인이 줄면 스크롤이 사라지고 여백이 0 으로 돌아온다"),
					Scroll && Scroll->GetScrollOffsetOfEnd() <= 0.5f && ScrollSlot && FMath::IsNearlyZero(ScrollSlot->GetPadding().Right));
				Test->TestEqual(TEXT("막대가 없어도 줄 폭 1064"), LocalWidth(List && List->GetChildrenCount() > 0 ? List->GetChildAt(0) : nullptr), 1064.0, 1.0);
				// Task 8a: 빈 줄 「생산 시작」 = 실제 서랍 경로(HandleEmptyLineStart → AddToViewport)로 생산 설정 창
				UButtonWidget* Start = EmptyRowStart();
				Test->TestTrue(TEXT("빈 줄 「생산 시작」 버튼이 있다"), Start != nullptr);
				if (Start) { Start->SimulateClick(); }
			}, 3.0, true);
			Add(TEXT("빈 줄 → 생산 설정 창이 뜸"), [this]() { return Detail() && Detail()->IsProductionStartPopupOpen(); }, [this]()
			{
				Test->TestTrue(TEXT("서랍 빈 줄 「생산 시작」 → 생산 설정 창이 뷰포트에 뜬다"), Detail() && Detail()->IsProductionStartPopupOpen());
				Esc();
			}, 3.0, true);
			Add(TEXT("ESC → 생산 설정 창만 닫힘"), [this]() { return Detail() && !Detail()->IsProductionStartPopupOpen(); }, [this]()
			{
				Test->TestTrue(FString::Printf(TEXT("ESC 가 라우터에서 처리된다(%d)"), static_cast<int32>(LastEsc)), LastEsc == ERouteUIInputResult::Handled);
				Test->TestTrue(TEXT("ESC 1번 = 생산 설정 창만 닫히고 서랍은 남는다"), !DrawerClosed() && Detail() && !Detail()->IsProductionStartPopupOpen());
				// 광산: 6초 뒤 가득 차는 라인, 자동 수령 꺼짐
				SeedMineEdge(false);
				Test->TestTrue(TEXT("광산 나라 핀 클릭"), ClickPin(Panel.Get(), W.MineCountry));
			}, 3.0, true);

			// ── 광산 가득 참 경계 + 자동 수령 ──
			Add(TEXT("광산 서랍"), [this]() { return DrawerOpenOn(W.MineCountry) && HasGeometry(Segment(true)); }, [this]()
			{
				Segment(true)->SimulateClick();
			}, 5.0);
			Add(TEXT("광산 강화 페이지(가득 차기 전)"), [this]() { return OnPage(true); }, [this]()
			{
				FMineLineState State;
				Test->TestTrue(TEXT("광산 가득 차기 전: 빨간 점 없음"), !DotShown()
					&& W.MineMgr->GetLineState(W.MineCountry, W.MineResources[0], State) && !State.bSaturated);
			});
			Add(TEXT("광산 라인이 가득 참"), [this]()
			{
				FMineLineState State;
				return W.MineMgr->GetLineState(W.MineCountry, W.MineResources[0], State) && State.bSaturated;
			}, nullptr, 15.0);
			Add(TEXT("광산 빨간 점"), [this]() { return DotShown(); }, [this]()
			{
				Test->TestTrue(TEXT("★광산 강화를 보는 중 라인이 가득 차면 「생산」 칸에 빨간 점이 뜬다"), DotShown());
				SeedMineEdge(true);
				MineFullAt = FPlatformTime::Seconds() + 6.0;
				if (UCountryDetailWidget* D = Detail()) { D->SetCurrentCountry(W.MineCountry); }
			}, 1.5, true);
			Add(TEXT("자동 수령 켠 광산(같은 나라라 강화 유지)"), [this]() { return OnPage(true) && W.MineMgr->IsAutoClaimEnabled(W.MineCountry); },
				[this]()
			{
				Test->TestTrue(TEXT("자동 수령 픽스처: 강화 페이지 유지 + 빨간 점 없음"), OnPage(true) && !DotShown());
			});
			// 수령은 클라우드 경제 응답을 거칠 수 있어 「비워졌나」가 아니라 「가득 찬 시각을 넘겼나」를 기다린다
			Add(TEXT("자동 수령 광산이 가득 찰 시각을 넘김"), [this]() { return FPlatformTime::Seconds() > MineFullAt + 2.0; }, [this]()
			{
				FMineLineState State;
				const bool bLive = W.MineMgr->GetLineState(W.MineCountry, W.MineResources[0], State);
				Test->AddInfo(FString::Printf(TEXT("[흐름] 자동 수령 광산: 가득 참 %d · 적재 %lld"), bLive && State.bSaturated ? 1 : 0, bLive ? State.CurrentQty : -1));
				Test->TestFalse(TEXT("자동 수령 나라는 가득 차도(바로 받으니) 빨간 점이 없다"), DotShown());
				Esc();
			}, 15.0, true);

			// ── ESC 두 번 = 서랍 → 월드맵 ──
			Add(TEXT("ESC 1번째로 서랍만 닫힘"), [this]() { return DrawerClosed(); }, [this]()
			{
				Test->TestTrue(TEXT("ESC 첫 번째는 서랍만 닫는다"), DrawerClosed() && Panel->IsActivated());
				Esc();
			}, 2.0, true);
			Add(TEXT("ESC 2번째로 월드맵 닫힘"), [this]() { return !Panel->IsActivated(); }, [this]()
			{
				AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(PlayWorld->GetFirstPlayerController());
				Test->TestFalse(TEXT("ESC 두 번째는 월드맵을 닫는다"), Panel->IsActivated());
				Test->TestTrue(TEXT("닫은 뒤 입력 모드 Normal"), PC && PC->GetCurrentInputMode() == EInputMode::Normal);
			}, 2.0, true);
		}

		// 공장 목록의 첫 빈 줄(UIE_DrawerInfoRow) 「생산 시작」 버튼
		UButtonWidget* EmptyRowStart() const
		{
			UPanelWidget* List = Named<UPanelWidget>(Detail(), TEXT("FactoryLineList"));
			for (UWidget* Child : List ? List->GetAllChildren() : TArray<UWidget*>())
			{
				UDrawerInfoRowWidget* Row = Cast<UDrawerInfoRowWidget>(Child);
				UButtonWidget* Button = Row ? Named<UButtonWidget>(Row, TEXT("ActionButton")) : nullptr;
				if (Button && IsShown(Button) && Button->GetIsEnabled()) { return Button; }
			}
			return nullptr;
		}

		// 광산 1줄이 지금부터 6초 뒤 가득 찬다(한도 6). bAutoClaim = 자동 수령 강화 1
		void SeedMineEdge(bool bAutoClaim)
		{
			TMap<EMineUpgradeType, int32> Levels;
			Levels.Add(EMineUpgradeType::LineExpansion, 0);
			Levels.Add(EMineUpgradeType::AutoClaim, bAutoClaim ? 1 : 0);
			W.SeedMine(W.MineCountry, {}, Levels, Backup.Mine);
			FMineLineState Line = W.MineLine(W.MineResources[0], 6, 0.0);
			// 실효 속도 = 기본 × 국가 배율 ― 1개/초가 되도록 기본을 픽스처가 정한다(DT 채광 속도와 무관)
			const double RateMul = W.MineMgr->GetRateMultiplier(W.MineCountry);
			Line.BaseRatePerSecond = RateMul > 0.0 ? static_cast<float>(1.0 / RateMul) : 1.f;
			Line.StartWallUtc = FDateTime::UtcNow();
			W.SeedMine(W.MineCountry, { Line }, Levels, Backup.Mine);
		}

		FAutomationTestBase* Test;
		UWorld* PlayWorld;
		FDrawerFixtures W;
		UUIBase* UIBase;
		UBuildOpenWidget* Entry;
		FObjectProperty* CachedSaveProp = nullptr;
		UObject* OriginalCachedSave = nullptr;
		TStrongObjectPtr<UWorldMapPanelWidget> Panel;
		FFixtureBackup Backup;
		TArray<FFlowStep> Steps;
		int32 StepIndex = 0;
		int32 FramesInStep = 0;
		double StepStartedAt = 0.0;
		double MineFullAt = 0.0;
		ERouteUIInputResult LastEsc = ERouteUIInputResult::Unhandled;
		FVector2D OutsidePoint = FVector2D::ZeroVector;
		FVector2D BodyPoint = FVector2D::ZeroVector;
		bool bReducedBefore = false;
	};
}

// ClientContext ― 에디터(PIE 없는) 시험 목록에는 안 뜨고 격리 -game 러너에서만 돈다(플레이 월드가 필요하다)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasDrawerLiveFlowsTest, "CGR.Overseas.DrawerLive.LiveFlows",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FOverseasDrawerLiveFlowsTest::RunTest(const FString& Parameters)
{
	using namespace CGRDrawerLive;
	UWorld* PlayWorld = FindPlayWorld();
	UGameInstance* GI = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	AMainMapPlayerController* PC = PlayWorld ? Cast<AMainMapPlayerController>(PlayWorld->GetFirstPlayerController()) : nullptr;
	if (!FApp::CanEverRender() || !GI || !UIBase || !PC)
	{
		AddError(TEXT("실제 뷰포트·UIBase·MainMap 컨트롤러가 있는 -game 세션이 필요합니다."));
		return false;
	}
	FDrawerFixtures W;
	FString Error;
	if (!W.Resolve(GI, Error)) { AddError(Error); return false; }

	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayWorld, Found, UBuildOpenWidget::StaticClass(), false);
	UBuildOpenWidget* Entry = nullptr;
	for (UUserWidget* Candidate : Found)
	{
		UBuildOpenWidget* AsEntry = Cast<UBuildOpenWidget>(Candidate);
		if (AsEntry && (!Entry || AsEntry->IsActivated())) { Entry = AsEntry; }
	}
	if (!Entry)
	{
		AddError(TEXT("라이브 BuildOpen 도크가 없습니다 ― 실제 진입 경로로 월드맵을 열 수 없습니다."));
		return false;
	}
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FLiveFlowsCommand>(this, PlayWorld, W, UIBase, Entry));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
