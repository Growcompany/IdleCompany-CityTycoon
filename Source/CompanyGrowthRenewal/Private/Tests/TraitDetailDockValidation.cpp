#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Element/Building/TraitCardSlotWidget.h"
#include "UI/Element/Building/TraitDetailPopupWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/ItemCardWidget.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Enum/BuildingTraitTarget.h"
#include "AssetCompilingManager.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

// 실제 건물로 패널을 열고(OpenManagePanel), 특성을 메모리로만 지급(저장 안 함)한 뒤 진짜 카드 클릭 경로로
// 패널 옆 상세 카드를 연다. 배치·글자 크기는 화면 밖 2304/3120 렌더 지오메트리로, 재탭·빈 곳·ESC 는 실제 처리기로 본다.
// 한계: 터치 입력 대신 카드/배경 버튼의 델리게이트를 직접 부른다(물리 클릭 판정 아님). 격리 -game 전용(세이브 쓰기 억제).
namespace TraitDetailDock
{
	static constexpr int32 CaptureWidths[] = { 2304, 3120 };
	static constexpr int32 CaptureHeight = 1440;
	static constexpr double Tol = 3.0;

	struct FBox2
	{
		bool bFound = false;
		double L = 0, T = 0, W = 0, H = 0;
		double R() const { return L + W; }
		double B() const { return T + H; }
	};

	static FString Dir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("TraitDetailDock"));
	}

	static FBox2 Measure(const UWidget* Widget, FVector2D Origin)
	{
		FBox2 Out;
		// 부모가 접힌 글자는 지난 배치의 크기를 들고 있다 ― 조상까지 본다
		for (const UWidget* W = Widget; W; W = W->GetParent())
		{
			if (W->GetVisibility() == ESlateVisibility::Collapsed) return Out;
		}
		if (Widget == nullptr) return Out;
		const FGeometry& G = Widget->GetCachedGeometry();
		const FVector2D P = FVector2D(G.GetAbsolutePosition()) - Origin;
		Out.bFound = true;
		Out.L = P.X; Out.T = P.Y;
		Out.W = G.GetLocalSize().X; Out.H = G.GetLocalSize().Y;
		return Out;
	}

	static TSharedPtr<FJsonObject> BoxJson(const FBox2& B)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetBoolField(TEXT("found"), B.bFound);
		O->SetNumberField(TEXT("left"), B.L); O->SetNumberField(TEXT("top"), B.T);
		O->SetNumberField(TEXT("width"), B.W); O->SetNumberField(TEXT("height"), B.H);
		return O;
	}

	static bool Draw(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& Out)
	{
		FWidgetRenderer Renderer(true);
		// raw target(false) ― sRGB 타깃은 Slate 감마와 겹쳐 뿌옇게 찍힌다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Widget, 1.f, FVector2D(Size.X, Size.Y), 0.f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), Out) || Out.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		Out.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static void SavePng(const FImage& Img, const FString& Path)
	{
		TArray64<uint8> Bytes;
		FImageUtils::CompressImage(Bytes, TEXT("png"), Img);
		FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	static float FontSize(const UUserWidget& Owner, const TCHAR* Name)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Owner.GetWidgetFromName(FName(Name)));
		return Text ? Text->GetFont().Size : -1.f;
	}

	static bool IsShown(const UWidget* W)
	{
		return W != nullptr && W->GetVisibility() != ESlateVisibility::Collapsed && W->GetVisibility() != ESlateVisibility::Hidden;
	}

	static UGameInstance* FindReadyGameInstance()
	{
		if (GEngine == nullptr) return nullptr;
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != Wanted || World == nullptr || !World->HasBegunPlay() || World->GetMapName().Contains(TEXT("Loading")))
				{
					continue;
				}
				UCGGameInstance* GI = Cast<UCGGameInstance>(World->GetGameInstance());
				const AMainMapPlayerController* PC = GI ? Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()) : nullptr;
				if (PC && PC->GetPawn() && GI->GetSubsystem<UTableManagerSubsystem>())
				{
					return GI;
				}
			}
		}
		return nullptr;
	}

	class FDockCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FDockCommand(FAutomationTestBase* InTest) : T(*InTest), Start(FPlatformTime::Seconds()) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (Now < WaitUntil) return false;
			switch (Step)
			{
			case 0: return StepFindWorld(Now);
			case 1: return StepOpenPanel();
			case 2: return StepOpenDetail();
			case 3: return StepMeasure();
			case 4: return StepBehaviour();
			default: return Finish();
			}
		}

	private:
		FAutomationTestBase& T;
		double Start;
		double WaitUntil = 0.0;
		int32 Step = 0;
		UGameInstance* GI = nullptr;
		ABuildingBaseActor* Building = nullptr;
		TWeakObjectPtr<UBuildingManagePanelWidget> Panel;
		FName EquipID, OtherID, BlockedID, LongID;
		// 빈 슬롯 없음 상태를 만들 때 채울 장착 가능 특성들
		TArray<FName> FillIDs;
		TMap<int32, double> FooterBottomByWidth;
		TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();

		void Wait(double Seconds) { WaitUntil = FPlatformTime::Seconds() + Seconds; }

		bool StepFindWorld(double Now)
		{
			GI = FindReadyGameInstance();
			if (!GI)
			{
				if (Now - Start < 120.0) return false;
				T.AddError(TEXT("120초 안에 플레이 월드가 서지 않았습니다 ― 격리 -game 에서 실행할 것"));
				return true;
			}
			UBuildingTraitManagerSubsystem* Traits = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
			UTableManagerSubsystem* Tables = GI->GetSubsystem<UTableManagerSubsystem>();
			for (TActorIterator<ABuildingBaseActor> It(GI->GetWorld()); It; ++It)
			{
				if (It->GetBuildingIndex() >= 0 && Traits && Traits->GetUnlockedSlotCount(It->GetBuildingIndex()) > 0)
				{
					Building = *It;
					break;
				}
			}
			if (!T.TestTrue(TEXT("해금 슬롯이 있는 건물"), Building != nullptr) || !Tables)
			{
				return true;
			}
			// 장착 가능 2종 + 업종 제한 1종 + 이름 가장 긴 1종 ― 저장은 하지 않는다
			const int32 BIdx = Building->GetBuildingIndex();
			int32 LongestName = 0;
			for (const FBuildingTraitTableRow& Row : Tables->GetAllBuildingTraits())
			{
				const bool bCan = Traits->CanEquipTraitToBuilding(BIdx, Row.TraitID);
				if (bCan && EquipID.IsNone()) EquipID = Row.TraitID;
				else if (bCan && OtherID.IsNone()) OtherID = Row.TraitID;
				if (!bCan && BlockedID.IsNone()) BlockedID = Row.TraitID;
				if (bCan && Row.TraitID != EquipID && Row.TraitID != OtherID && FillIDs.Num() < 3) FillIDs.Add(Row.TraitID);
				if (bCan && Row.DisplayName.ToString().Len() > LongestName)
				{
					LongestName = Row.DisplayName.ToString().Len();
					LongID = Row.TraitID;
				}
			}
			for (const FName Id : { EquipID, OtherID, BlockedID, LongID })
			{
				if (!Id.IsNone()) Traits->GrantTrait(Id, 1, /*bShouldSave*/ false);
			}
			for (const FName Id : FillIDs)
			{
				Traits->GrantTrait(Id, 1, /*bShouldSave*/ false);
			}
			T.TestTrue(TEXT("픽스처 특성(장착 가능 2·제한 1)"), !EquipID.IsNone() && !OtherID.IsNone() && !BlockedID.IsNone());
			FAssetCompilingManager::Get().FinishAllCompilation();
			if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
			Step = 1;
			return false;
		}

		bool bPanelRequested = false;

		bool StepOpenPanel()
		{
			AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(Cast<UCGGameInstance>(GI)->GetCurrentPlayerController());
			if (!bPanelRequested)
			{
				// 스택의 활성 위젯은 push 다음 틱에 갱신된다
				Building->OpenManagePanel(PC);
				bPanelRequested = true;
				Wait(0.3);
				return false;
			}
			if (UUIManagerSubsystem* UIM = GI->GetSubsystem<UUIManagerSubsystem>())
			{
				if (UUIBase* Base = UIM->GetUIBase())
				{
					Panel = Cast<UBuildingManagePanelWidget>(Base->GetActiveBottomWidget());
				}
			}
			if (!T.TestTrue(TEXT("실제 경로로 패널 열림"), Panel.IsValid())) return true;
			Panel->SetTargetBuilding(Building, EBuildingManageTab::Trait);
			Wait(1.5);
			Step = 2;
			return false;
		}

		UTraitCardSlotWidget* FindCard(FName Id) const
		{
			const UPanelWidget* Grid = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("TraitCardContainer")));
			for (int32 i = 0; Grid && i < Grid->GetChildrenCount(); ++i)
			{
				UTraitCardSlotWidget* Card = Cast<UTraitCardSlotWidget>(Grid->GetChildAt(i));
				if (Card && Card->GetTraitID() == Id) return Card;
			}
			return nullptr;
		}

		bool Click(FName Id)
		{
			UTraitCardSlotWidget* Card = FindCard(Id);
			return Card && Card->OnItemCardClicked.ExecuteIfBound(Id);
		}

		bool StepOpenDetail()
		{
			T.TestTrue(TEXT("특성 탭 격자에 픽스처 카드"), Panel.IsValid() && FindCard(EquipID) != nullptr);
			// 이 건물에 못 끼는 특성은 맨 뒤, 카드 위 「프로젝트/제조」 글자는 없다
			if (const UPanelWidget* Grid = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("TraitCardContainer"))))
			{
				UBuildingTraitManagerSubsystem* Traits = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
				bool bSeenBlocked = false, bOrderOk = true, bAnyBlocked = false;
				for (int32 i = 0; i < Grid->GetChildrenCount(); ++i)
				{
					const UTraitCardSlotWidget* Card = Cast<UTraitCardSlotWidget>(Grid->GetChildAt(i));
					if (!Card) continue;
					const bool bCan = Traits->CanEquipTraitToBuilding(Building->GetBuildingIndex(), Card->GetTraitID());
					if (!bCan) { bSeenBlocked = true; bAnyBlocked = true; }
					else if (bSeenBlocked) { bOrderOk = false; }
					T.TestFalse(TEXT("카드에 전용 글자 없음"), IsShown(Card->GetWidgetFromName(TEXT("RequirementBadgeBorder"))));
				}
				T.TestTrue(TEXT("못 끼는 특성이 격자에 있음(픽스처)"), bAnyBlocked);
				T.TestTrue(TEXT("못 끼는 특성은 맨 뒤"), bOrderOk);
			}
			T.TestTrue(TEXT("장착 가능 카드 클릭"), Click(EquipID));
			Wait(0.3);
			Step = 3;
			return false;
		}

		UTraitDetailPopupWidget* Popup() const { return Cast<UTraitDetailPopupWidget>(Panel->GetWidgetFromName(TEXT("TraitDetailPopup"))); }
		bool LayerOpen() const { return IsShown(Panel->GetWidgetFromName(TEXT("TraitModalLayer"))); }

		// 화면 밖 두 폭 렌더 ― 위치·겹침·글자 크기
		void MeasureState(const FString& State)
		{
			TSharedRef<SWidget> Slate = Panel->TakeWidget();
			for (const int32 Width : CaptureWidths)
			{
				const FString Label = FString::Printf(TEXT("%d %s"), Width, *State);
				FImage Img;
				const bool bDrawn = Draw(Slate, FIntPoint(Width, CaptureHeight), Img);
				T.TestTrue(Label + TEXT(": 렌더"), bDrawn);
				const FVector2D Origin = Panel->GetRootWidget() ? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
				const FBox2 Card = Measure(Popup(), Origin);
				const FBox2 Dock = Measure(Panel->GetWidgetFromName(TEXT("UIE_PanelBorder")), Origin);
				const FBox2 Tab = Measure(Panel->GetTraitTabButtonWidget(), Origin);
				const FBox2 Enter = Measure(Panel->GetEnterOfficeButtonWidget(), Origin);
				const FBox2 Info = Measure(Panel->GetWidgetFromName(TEXT("LeftPanelRightBox")), Origin);
				if (bDrawn) SavePng(Img, Dir() / FString::Printf(TEXT("dock_%d_%s.png"), Width, *State));

				TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
				J->SetObjectField(TEXT("card"), BoxJson(Card));
				J->SetObjectField(TEXT("dock"), BoxJson(Dock));
				J->SetObjectField(TEXT("tab"), BoxJson(Tab));
				J->SetObjectField(TEXT("enter"), BoxJson(Enter));
				Report->SetObjectField(Label, J);

				if (!T.TestTrue(Label + TEXT(": 상세 카드·패널 측정"), Card.bFound && Dock.bFound && Card.W > 0)) continue;
				const double Gap = Dock.L - Card.R();
				T.TestTrue(FString::Printf(TEXT("%s: 카드 오른쪽 → 패널 간격 %.1f (8~32)"), *Label, Gap), Gap >= 8.0 && Gap <= 32.0);
				// 아래를 패널 아래에 맞춰 붙인다 ― 내용 길이가 달라도 장착 버튼 자리가 그대로다
				T.TestTrue(FString::Printf(TEXT("%s: 카드 아래 %.1f == 패널 아래 %.1f - 20 (±%.0f)"), *Label, Card.B(), Dock.B(), Tol), FMath::Abs(Card.B() - (Dock.B() - 20.0)) <= Tol);
				T.TestTrue(FString::Printf(TEXT("%s: 카드 위 %.0f >= 탭 위 %.0f"), *Label, Card.T, Tab.T), Tab.bFound && Card.T >= Tab.T - 0.5);
				const FBox2 Btn = Measure(Popup()->GetWidgetFromName(TEXT("FooterRow")), Origin);
				J->SetObjectField(TEXT("footer"), BoxJson(Btn));
				double& FirstB = FooterBottomByWidth.FindOrAdd(Width, -1.0);
				if (Btn.bFound && FirstB < 0.0) FirstB = Btn.B();
				T.TestTrue(FString::Printf(TEXT("%s: 버튼 줄 아래 %.1f == 첫 상태 %.1f"), *Label, Btn.B(), FirstB), Btn.bFound && FMath::Abs(Btn.B() - FirstB) <= Tol);
				T.TestTrue(FString::Printf(TEXT("%s: 카드 폭 %.1f == 720"), *Label, Card.W), FMath::Abs(Card.W - 720.0) <= Tol);
				// 왼쪽 아래 사무실 입장 버튼과 겹치면 안 된다(가로가 겹칠 때만 세로를 본다)
				const bool bXOverlap = Enter.bFound && Card.L < Enter.R() && Enter.L < Card.R();
				T.TestTrue(FString::Printf(TEXT("%s: 입장 버튼과 겹침 없음 (카드 아래 %.0f / 버튼 위 %.0f, 가로겹침 %d)"), *Label, Card.B(), Enter.T, bXOverlap),
					!bXOverlap || Card.B() <= Enter.T - 8.0);
				T.TestTrue(FString::Printf(TEXT("%s: 카드 아래 %.0f <= 화면 %d"), *Label, Card.B(), CaptureHeight), Card.B() <= CaptureHeight);
				T.TestFalse(Label + TEXT(": 상세가 열린 동안 건물 정보 카드 숨김"), Info.bFound);

				// 우상단 X 와 머리 글자가 겹치면 안 된다
				const FBox2 CloseX = Measure(Popup()->GetWidgetFromName(TEXT("UIE_CloseButton")), Origin);
				T.TestTrue(Label + TEXT(": 상세 X 버튼 있음"), CloseX.bFound);
				J->SetObjectField(TEXT("close"), BoxJson(CloseX));
				// X 는 판 안쪽 ― 위·오른쪽 여백 16 이상(테두리에 걸치지 않음)
				T.TestTrue(FString::Printf(TEXT("%s: X 위 여백 %.0f >= 16"), *Label, CloseX.T - Card.T), CloseX.bFound && CloseX.T - Card.T >= 16.0 - 0.5);
				T.TestTrue(FString::Printf(TEXT("%s: X 오른쪽 여백 %.0f >= 16"), *Label, Card.R() - CloseX.R()), CloseX.bFound && Card.R() - CloseX.R() >= 16.0 - 0.5);
				for (const TCHAR* Name : { TEXT("RequirementText"), TEXT("RarityText"), TEXT("NameText") })
				{
					const FBox2 Tx = Measure(Popup()->GetWidgetFromName(FName(Name)), Origin);
					if (!Tx.bFound || !CloseX.bFound || Tx.T >= CloseX.B() || CloseX.T >= Tx.B()) continue;
					T.TestTrue(FString::Printf(TEXT("%s: %s 오른쪽 %.0f <= X 왼쪽 %.0f - 8"), *Label, Name, Tx.R(), CloseX.L), Tx.R() <= CloseX.L - 8.0 + 0.5);
				}

				// 글자가 카드 안쪽에 들어오는가 ― 말줄임 없는 한 줄 이름이 가장 위험하다
				for (const TCHAR* Name : { TEXT("NameText"), TEXT("RequirementText"), TEXT("EffectTargetText"), TEXT("EffectValueText"), TEXT("EffectDetailText"), TEXT("SetBonus2Text"), TEXT("SetBonus3Text"), TEXT("RestrictionText"), TEXT("TargetSlotHintText") })
				{
					const FBox2 Tx = Measure(Popup()->GetWidgetFromName(FName(Name)), Origin);
					if (!Tx.bFound) continue;
					T.TestTrue(FString::Printf(TEXT("%s: %s 오른쪽 %.0f <= 카드 안쪽 %.0f"), *Label, Name, Tx.R(), Card.R() - 30.0), Tx.R() <= Card.R() - 30.0 + 0.5);
				}
			}
		}

		bool StepMeasure()
		{
			if (!T.TestTrue(TEXT("카드 클릭 → 상세 열림"), LayerOpen() && Popup() != nullptr)) return true;
			UTraitDetailPopupWidget* P = Popup();

			// 글자 크기 = 설계서 §3
			const TPair<const TCHAR*, float> Sizes[] = {
				{ TEXT("NameText"), 44.f }, { TEXT("RarityText"), 28.f }, { TEXT("RequirementText"), 28.f },
				{ TEXT("EffectTargetText"), 32.f }, { TEXT("EffectValueText"), 56.f }, { TEXT("EffectDetailText"), 28.f },
				{ TEXT("SetBonusNameText"), 30.f }, { TEXT("SetBonus2Text"), 28.f }, { TEXT("TargetSlotHintText"), 30.f }, { TEXT("RestrictionText"), 30.f } };
			for (const auto& S : Sizes)
			{
				const float Got = FontSize(*P, S.Key);
				T.TestTrue(FString::Printf(TEXT("글자 %s %.0f == %.0f"), S.Key, Got, S.Value), FMath::IsNearlyEqual(Got, S.Value, 0.5f));
			}
			const UButtonWidget* Equip = Cast<UButtonWidget>(P->GetWidgetFromName(TEXT("EquipButton")));
			T.TestTrue(TEXT("장착 버튼 라벨 40 · 외곽선 없음"), Equip && FMath::IsNearlyEqual(Equip->GetTextSize(), 40.f) && !Equip->GetTextOutlineEnabled());
			T.TestFalse(TEXT("「부가 효과」 라벨이 화면에 없음"), IsShown(P->GetWidgetFromName(TEXT("SecLabelText"))));
			T.TestFalse(TEXT("등급 칩 판 없음"), IsShown(P->GetWidgetFromName(TEXT("RarityChipBorder"))));

			// 효과 문구 = 슬롯 캡션과 같은 함수
			FBuildingTraitTableRow Row;
			if (GI->GetSubsystem<UTableManagerSubsystem>()->GetBuildingTraitData(EquipID, Row))
			{
				const EBuildingTraitTarget Target = GetTargetForCategory(Row.Category);
				const UTextBlock* Tgt = Cast<UTextBlock>(P->GetWidgetFromName(TEXT("EffectTargetText")));
				const UTextBlock* Val = Cast<UTextBlock>(P->GetWidgetFromName(TEXT("EffectValueText")));
				T.TestTrue(TEXT("효과 대상 문구"), Tgt && Tgt->GetText().EqualTo(GetTraitTargetDisplayName(Target)));
				T.TestTrue(TEXT("효과 수치 문구"), Val && Val->GetText().EqualTo(FormatTraitEffectValueText(Target, Row.BaseEffect)));
			}

			// 선택 표시 = 보고 있는 카드 1장에만 바깥 링, 공용 카드 안쪽 테두리는 쓰지 않는다
			int32 Selected = 0;
			const UPanelWidget* Grid = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("TraitCardContainer")));
			for (int32 i = 0; Grid && i < Grid->GetChildrenCount(); ++i)
			{
				const UTraitCardSlotWidget* Card = Cast<UTraitCardSlotWidget>(Grid->GetChildAt(i));
				if (!Card) continue;
				if (IsShown(Card->GetWidgetFromName(TEXT("SelectRing"))))
				{
					++Selected;
					T.TestEqual(TEXT("선택 링 = 보고 있는 카드"), Card->GetTraitID(), EquipID);
				}
				T.TestFalse(TEXT("카드 안쪽 선택 테두리 안 씀"), Card->GetCardWidget() && Card->GetCardWidget()->IsSelected());
			}
			T.TestEqual(TEXT("선택 링 장수"), Selected, 1);

			// 버튼이 갈 슬롯을 말한다, 안내 줄은 평소 없음
			const UTextBlock* EquipLabel = Equip ? Cast<UTextBlock>(Equip->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
			T.TestTrue(TEXT("장착 버튼 = 「N번 슬롯에 장착」"), EquipLabel && EquipLabel->GetText().ToString().EndsWith(TEXT("번 슬롯에 장착")));
			T.TestFalse(TEXT("빈 슬롯이 있으면 안내 줄 없음"), IsShown(P->GetWidgetFromName(TEXT("TargetSlotHintText"))));

			MeasureState(TEXT("equip"));
			Click(LongID);
			MeasureState(TEXT("longname"));
			Click(BlockedID);
			T.TestFalse(TEXT("제한 특성: 장착 버튼 비활성"), Equip && Equip->GetIsEnabled());
			MeasureState(TEXT("blocked"));

			// 분야마다 세트 효과 이름 길이가 다르다 ― 분야별 1장씩 채워 글자가 창 안에 있는지 본다
			{
				TSet<EBuildingTraitCategory> Seen;
				const TSharedRef<SWidget> Slate = Panel->TakeWidget();
				for (const FBuildingTraitTableRow& CatRow : GI->GetSubsystem<UTableManagerSubsystem>()->GetAllBuildingTraits())
				{
					if (Seen.Contains(CatRow.Category)) continue;
					Seen.Add(CatRow.Category);
					P->ConfigureForTrait(CatRow.TraitID, Building->GetBuildingIndex(), INDEX_NONE);
					FImage Img;
					if (!Draw(Slate, FIntPoint(CaptureWidths[0], CaptureHeight), Img)) continue;
					const FVector2D Origin = Panel->GetRootWidget() ? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
					const FBox2 Card = Measure(P, Origin);
					for (const TCHAR* Name : { TEXT("SetBonusNameText"), TEXT("SetBonus2Text"), TEXT("SetBonus3Text"), TEXT("EffectTargetText"), TEXT("EffectDetailText"), TEXT("NameText") })
					{
						const FBox2 Tx = Measure(P->GetWidgetFromName(FName(Name)), Origin);
						if (!Tx.bFound || !Card.bFound) continue;
						T.TestTrue(FString::Printf(TEXT("분야 %d %s: %s 오른쪽 %.0f <= 카드 안쪽 %.0f"), static_cast<int32>(CatRow.Category), *CatRow.TraitID.ToString(), Name, Tx.R(), Card.R() - 30.0),
							Tx.R() <= Card.R() - 30.0 + 0.5);
					}
				}
				T.TestTrue(TEXT("분야 2개 이상 점검"), Seen.Num() >= 2);
			}
			Step = 4;
			Wait(0.2);
			return false;
		}

		bool StepBehaviour()
		{
			// 다른 카드 = 내용 교체(열린 채), 같은 카드 재탭 = 닫기
			Click(OtherID);
			T.TestTrue(TEXT("다른 카드 탭 → 열린 채 교체"), LayerOpen());
			Click(OtherID);
			T.TestFalse(TEXT("같은 카드 재탭 → 닫힘"), LayerOpen());
			T.TestTrue(TEXT("닫으면 건물 정보 카드 복원"), IsShown(Panel->GetWidgetFromName(TEXT("LeftPanelRightBox"))));

			// 빈 곳(배경 버튼) = 상세만 닫고 패널은 남는다
			Click(EquipID);
			if (UButton* Bg = Cast<UButton>(Panel->GetWidgetFromName(TEXT("BackgroundBtn"))))
			{
				Bg->OnClicked.Broadcast();
			}
			T.TestFalse(TEXT("빈 곳 클릭 → 상세 닫힘"), LayerOpen());
			T.TestFalse(TEXT("빈 곳 클릭 → 패널은 그대로"), Panel->IsCloseRequested());

			// ESC = 안쪽부터
			Click(EquipID);
			Panel->NativeOnHandleBackAction();
			T.TestFalse(TEXT("ESC → 상세 먼저 닫힘"), LayerOpen());
			T.TestFalse(TEXT("ESC 첫 번째 → 패널은 그대로"), Panel->IsCloseRequested());

			// 장착 = 닫히고 슬롯이 찬다
			Click(EquipID);
			UBuildingTraitManagerSubsystem* Traits = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
			const int32 Before = Traits->GetEquippedTraits(Building->GetBuildingIndex()).FilterByPredicate([](const FName& N) { return !N.IsNone(); }).Num();
			if (UTraitDetailPopupWidget* P = Popup()) P->OnEquipClicked.ExecuteIfBound(EquipID);
			const int32 After = Traits->GetEquippedTraits(Building->GetBuildingIndex()).FilterByPredicate([](const FName& N) { return !N.IsNone(); }).Num();
			T.TestFalse(TEXT("장착 → 상세 닫힘"), LayerOpen());
			T.TestEqual(TEXT("장착 → 장착 수 +1"), After, Before + 1);

			// 빈 슬롯 없음 ― 남은 해금 칸을 채우고, 닫았다 열어 기본 타겟을 「없음」으로 되돌린 뒤 본다
			const int32 BIdx = Building->GetBuildingIndex();
			int32 Fill = 0;
			for (int32 i = 0; i < 3; ++i)
			{
				const TArray<FName> Now = Traits->GetEquippedTraits(BIdx);
				if (Traits->IsSlotUnlocked(BIdx, i) && (!Now.IsValidIndex(i) || Now[i].IsNone()) && FillIDs.IsValidIndex(Fill))
				{
					Traits->EquipTrait(BIdx, i, FillIDs[Fill++]);
				}
			}
			Click(LongID);
			Click(LongID);
			Click(LongID);
			if (UTraitDetailPopupWidget* P = Popup())
			{
				const UButtonWidget* Equip = Cast<UButtonWidget>(P->GetWidgetFromName(TEXT("EquipButton")));
				const UTextBlock* EquipLabel = Equip ? Cast<UTextBlock>(Equip->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
				T.TestTrue(TEXT("빈 슬롯 없음: 상세 열림"), LayerOpen());
				T.TestFalse(TEXT("빈 슬롯 없음: 장착 버튼 비활성"), Equip && Equip->GetIsEnabled());
				T.TestTrue(TEXT("빈 슬롯 없음: 버튼 「빈 슬롯 없음」"), EquipLabel && EquipLabel->GetText().ToString() == TEXT("빈 슬롯 없음"));
				T.TestTrue(TEXT("빈 슬롯 없음: 안내 줄 보임"), IsShown(P->GetWidgetFromName(TEXT("TargetSlotHintText"))));
				MeasureState(TEXT("full"));
			}
			Panel->NativeOnHandleBackAction();
			T.TestFalse(TEXT("빈 슬롯 없음: ESC → 상세만 닫힘"), LayerOpen());

			Panel->NativeOnHandleBackAction();
			T.TestTrue(TEXT("ESC 두 번째(상세 없음) → 패널 닫기 요청"), Panel->IsCloseRequested());
			Step = 5;
			return false;
		}

		bool Finish()
		{
			FString Out;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
			FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
			IFileManager::Get().MakeDirectory(*Dir(), true);
			FFileHelper::SaveStringToFile(Out, *(Dir() / TEXT("dock.json")));
			UE_LOG(LogTemp, Display, TEXT("TraitDetailDock: 증거 = %s"), *Dir());
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTraitDetailDockTest, "CGR.Trait.DetailDock",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTraitDetailDockTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(TraitDetailDock::FDockCommand(this));
	return true;
}

#endif
