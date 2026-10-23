#include "UI/Panel/OfficeMainWidget.h"
#include "Manager/LaunchReactionSubsystem.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "UI/HUD/MissionTrackerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ManufacturingDevelopmentManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SideContentSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Office/OfficeManager.h"
#include "UI/Panel/BusinessTripPanelWidget.h"
#include "UI/Panel/TrainingCenterPanelWidget.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/OfficeFeatureButtonWidget.h"
#include "UI/Element/Cards/IconCardWidget.h"
#include "UI/Element/Trade/ProjectInfoBarWidget.h"
#include "UI/Element/Office/OfficeProjectCardWidget.h"
#include "UI/Panel/OfficeRecruitmentModalWidget.h"
#include "UI/Panel/OfficeEmployeePanelWidget.h"
#include "UI/Panel/EquipmentGachaPanelWidget.h"
#include "Player/OfficePlayerController.h"
#include "Player/MainMapPlayerController.h"
#include "Table/ProjectDataTable.h"
#include "Table/ProjectGenreTable.h"
#include "Enum/ProjectStepType.h"
#include "Enum/OperationState.h"
#include "Enum/NotificationType.h"
#include "Enum/WidgetType.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "UI/Element/Common/RadialProgressWidget.h"
#include "Data/ProjectEventData.h"
#include "UI/Panel/EventChoicePanelWidget.h"
#include "UI/Panel/BoostGambleWidget.h"
#include "UI/Element/Office/OfficeEventRailWidget.h"
#include "UI/Element/Office/SalesCurveWidget.h"
#include "UI/Element/Cards/EventChoiceCardWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Data/OperationData.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/ProgressBar.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Effects/NiagaraTextEffectWidget.h"
#include "UI/Element/Effects/DevelopmentBannerWidget.h"
#include "UI/Panel/LaunchConfirmWidget.h"
#include "UI/Panel/PitchBoardWidget.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "UI/Panel/ProjectReportWidget.h"
#include "Data/ProjectReportData.h"
#include "UI/Element/Effects/ScoreOrbContainerWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Enum/VFXType.h"
#include "Table/VFXTable.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeReactionComponent.h"
#include "Office/WorkstationDevFxComponent.h"
#include "Player/OfficeCameraPawn.h"
#include "Manager/EmployeeManager.h"
#include "Global/GlobalUtilFunctions.h"
#include "Data/StageProgressData.h"
#include "UI/Element/Common/QualityGradeMedalUtils.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	// 개발 완료 강조색 (linear).
	// 스트립 점수 바가 그린을 "목표 달성" 으로 쓰므로 시간에는 재사용하지 않는다(의미 충돌).
	const FLinearColor TimerRingGold(0.887923f, 0.533276f, 0.076185f, 1.0f);

	FText CompanyFeatureLockReason(int32 RequiredLevel)
	{
		return FText::Format(NSLOCTEXT("Office", "CompanyFeatureLevelRequired", "회사 레벨 {0}부터 이용할 수 있어요."), FText::AsNumber(RequiredLevel));
	}

	FString FormatStripScore(int64 Score)
	{
		return UGlobalUtilFunctions::AbbreviateNumber(Score, ENumberAbbrevStyle::WesternSuffix, ENumberRoundMode::Floor).ToString();
	}

	// 자릿수가 늘 때마다 게이지가 넓어져 표시줄 전체가 들썩이지 않도록, 목표 2배까지의 가장 넓은 "점수/목표" 폭을 미리 잰다.
	float MeasureStripScoreWidth(const UTextBlock* ValT, int64 GoalScore)
	{
		if (!ValT || GoalScore <= 0 || !FSlateApplication::IsInitialized()) return 0.0f;
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateFontInfo& Font = ValT->GetFont();

		TCHAR WideDigit = TEXT('0');
		float WideDigitWidth = 0.0f;
		for (TCHAR Digit = TEXT('0'); Digit <= TEXT('9'); ++Digit)
		{
			const float DigitWidth = FontMeasure->Measure(FString::Chr(Digit), Font).X;
			if (DigitWidth > WideDigitWidth) { WideDigitWidth = DigitWidth; WideDigit = Digit; }
		}
		auto Widen = [WideDigit](FString Text)
		{
			for (TCHAR& Ch : Text) { if (FChar::IsDigit(Ch)) Ch = WideDigit; }
			return Text;
		};

		// 목표 쪽 글자는 개발 내내 그대로라 실제 글자로 잰다. 넓은 숫자로 바꾸는 건 변하는 획득 쪽뿐이다.
		const FString GoalText = FormatStripScore(GoalScore);
		float Width = 0.0f;
		for (const int64 Shown : { GoalScore, GoalScore * 2 })
		{
			Width = FMath::Max(Width, (float)FontMeasure->Measure(Widen(FormatStripScore(Shown)) + TEXT("/") + GoalText, Font).X);
		}
		return Width + Font.OutlineSettings.OutlineSize * 2.0f;
	}

	// 운영 카드 제목 칸은 표지 옆 좁은 칸이라 자동 줄바꿈에 맡기면 한글이 음절 사이에서 끊긴다.
	// 띄어쓰기에서만 줄을 나누고, 칸에 안 들어가면 한 단계씩 작게 다시 맞춘다(기준 27pt = 27 · 22.5 · 19.5).
	FText FitOpCardTitle(const FText& Name, FSlateFontInfo& InOutFont, float BaseSize, float WrapWidth)
	{
		InOutFont.Size = BaseSize;
		if (!FSlateApplication::IsInitialized() || WrapWidth <= 0.0f || Name.IsEmpty()) return Name;
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		TArray<FString> Words;
		Name.ToString().ParseIntoArrayWS(Words);

		struct FTitleTier { float Scale; int32 MaxLines; };
		static const FTitleTier Tiers[] = { { 1.0f, 2 }, { 22.5f / 27.0f, 2 }, { 19.5f / 27.0f, 3 } };
		FString Fitted;
		for (const FTitleTier& Tier : Tiers)
		{
			InOutFont.Size = BaseSize * Tier.Scale;
			TArray<FString> Lines;
			bool bEveryWordFits = true;
			for (const FString& Word : Words)
			{
				bEveryWordFits &= FontMeasure->Measure(Word, InOutFont).X <= WrapWidth;
				if (Lines.Num() > 0 && FontMeasure->Measure(Lines.Last() + TEXT(" ") + Word, InOutFont).X <= WrapWidth)
				{
					Lines.Last() += TEXT(" ") + Word;
				}
				else
				{
					Lines.Add(Word);
				}
			}
			Fitted = FString::Join(Lines, TEXT("\n"));
			if (bEveryWordFits && Lines.Num() <= Tier.MaxLines) break;
		}
		return FText::FromString(Fitted);
	}
}

void UOfficeMainWidget::PlayShowButtonsAnim()
{
	// 플래그만 세팅 — 실제 슬라이드는 NativeTick 이 DockSlideAlpha 를 보간해 구동(멱등).
	bDockHidden = false;
}

void UOfficeMainWidget::PlayHideButtonsAnim()
{
	bDockHidden = true;
}

void UOfficeMainWidget::ApplyDockSlide(float Alpha)
{
	if (!OfficeDockTray)
	{
		return;
	}
	OfficeDockTray->SetRenderTranslation(FVector2D(0.f, Alpha * DockSlideDistance));
	OfficeDockTray->SetRenderOpacity(1.f - Alpha);
}

// ===== Lifecycle FSM 단일 진입점 =====

void UOfficeMainWidget::OnLifecycleChangedHandler(EProjectLifecycle OldState, EProjectLifecycle NewState)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Lifecycle %s → %s"),
		*LexToString(OldState), *LexToString(NewState));
	if (ScoreOrbContainer && NewState != EProjectLifecycle::Developing && NewState != EProjectLifecycle::LaunchPending)
	{
		ScoreOrbContainer->ResetDevelopmentEffects();
	}
	// 책상 연출 — 개발 밖으로 나가면(결과 창 닫힘 등) 조각·메모지·빛을 거두고 완료 당김을 되돌린다
	if (NewState == EProjectLifecycle::Developing)
	{
		UWorkstationDevFxComponent::EnsureDevelopmentActive(GetWorld());
	}
	else if (NewState != EProjectLifecycle::LaunchPending)
	{
		UWorkstationDevFxComponent::NotifyDevelopmentReset(GetWorld());
		if (AOfficeCameraPawn* Camera = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Camera->ReleasePresentationPullIn(0.3f);
		}
	}
	RefreshOfficeUI(NewState);
}

void UOfficeMainWidget::RefreshOfficeUI(EProjectLifecycle State)
{
	const UCGGameInstance* OfficeGI = Cast<UCGGameInstance>(GetGameInstance());
	const bool bManufacturing = OfficeGI && IsManufacturingType(OfficeGI->GetCurrentBuildingCompanyType());
	const UManufacturingDevelopmentManager* ResearchMgr = bManufacturing ? OfficeGI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	const bool bManufacturingResearch = ResearchMgr && ResearchMgr->GetActiveResearchStatus(OfficeGI->GetCurrentManagedBuildingIndex()).bInProgress;
	if (OpenBoardButton)
	{
		const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
		OpenBoardButton->SetButtonText(GI && IsManufacturingType(GI->GetCurrentBuildingCompanyType())
			? NSLOCTEXT("Office", "SelectManufacturingProduct", "제품 선택")
			: NSLOCTEXT("Office", "OpenPitchBoard", "기획안 보기"));
		if (IdleHintText)
		{
			IdleHintText->SetText(GI && IsManufacturingType(GI->GetCurrentBuildingCompanyType())
				? NSLOCTEXT("Office", "ManufacturingIdleHint", "제품을 선택해 생산 수량을 확보하세요")
				: NSLOCTEXT("Office", "ProjectIdleHint", "기획안으로 새 프로젝트를 시작해주세요"));
		}
	}
	UpdateExpansionHotspots();
	RefreshCompanyFeatureButtons();
	RefreshEquipmentManualButton();
	// 1. 진행 줄 2페이지: {Developing,LaunchPending}=1 / 그 밖=0. 운영 중 내용은 오른쪽 열 운영 카드(OpCard)가 보여 준다
	const int32 PageIdx = (State == EProjectLifecycle::Developing || State == EProjectLifecycle::LaunchPending) ? 1 : 0;
	if (ProjectWidgetSwitcher)
	{
		ProjectWidgetSwitcher->SetActiveWidgetIndex(bManufacturing ? 1 : PageIdx);
	}

	// 1-b. 스트립 식별(이름/배지) + 스테이지 셀 갱신 (신규 — 은퇴한 카드/네임바 대체)
	PopulateStripIdentity();
	UpdateStripStageCells();
	if (State == EProjectLifecycle::Developing || State == EProjectLifecycle::LaunchPending)
	{
		if (const UOfficeStageProgressManager* TimerMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>())
		{
			OnTimerUpdated(TimerMgr->GetProgressData().RemainingTime);
		}
	}

	// 2. 카드 모드 (Stage / Operation)
	if (UI_OfficeProjectCardCurrent)
	{
		const bool bOperationMode = (State == EProjectLifecycle::Operating || State == EProjectLifecycle::ReportPending);
		if (bOperationMode)
		{
			if (FOperationData* CurrentOp = GetCurrentBuildingOperation())
			{
				UI_OfficeProjectCardCurrent->SwitchToOperationMode(
					CurrentOp->ActualRevenuePerSecond,
					static_cast<int64>(CurrentOp->TotalRevenueEarned),
					QualityGradeToAlphabetString(CurrentOp->QualityGrade));
			}
			else
			{
				UI_OfficeProjectCardCurrent->SwitchToOperationMode(0.0f, 0, TEXT("-"));
			}
		}
		else
		{
			UI_OfficeProjectCardCurrent->SwitchToStageMode();
		}
	}

	// 3. NameBar
	RefreshProjectNameBar();

	// 4. 카드 진행률 / 타이틀 갱신
	UpdateProjectCardProgress();
	UpdateIdleTierText();

	// 5. 개발 집중 구간에는 하단 도크를 숨긴다. 오른쪽 레일은 배치 상태와 함께 갱신한다.
	const bool bFocusMode = IsFocusLockedPhase(State) || bManufacturingResearch;
	if (bFocusMode)
	{
		PlayHideButtonsAnim();
	}
	else
	{
		PlayShowButtonsAnim();
	}

	// 5-a. 개발 배속 원복 — 배속은 개발 구간에서만 성립한다(운영 수익은 실시간 티커라 안 빨라진다).
	// 토글이 개발 스트립과 함께 사라지므로, 출시/폐기 때 여기서 안 풀면 사무실을 떠날 때까지 월드가 2배속으로 남는다.
	if (!bFocusMode && DevSpeedMultiplier > 1.5f)
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] 개발 구간 종료(%s) — 배속 x1 복원"), *LexToString(State));
		ApplyDevSpeed(1.0f);
	}

	// 5-b. 미션 트래커 — 집중 모드면 좌측 슬라이드아웃, 아니면 복귀. 트래커는 어느 레이어에 임베드됐든 해석.
	{
		UMissionTrackerWidget* Tracker = GetMissionTracker();
		if (!Tracker)
		{
			if (UMissionManagerSubsystem* MM = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
			{
				Tracker = MM->GetActiveTracker();
			}
		}
		if (Tracker)
		{
			Tracker->SetFocusHidden(bFocusMode);
		}
	}

	// 6. bIsInOperationMode 동기화
	bIsInOperationMode = (State == EProjectLifecycle::Operating || State == EProjectLifecycle::ReportPending);

	// 7. 상단 밴드(회사배너/트렌드칩/티어칩) = OfficeLayer 소유 — 라이프사이클 전환마다 트리거.
	// 개발 구간에는 진행 줄을 회사 블록 자리로 올리고 블록을 숨긴다. 판정은 나가기를 숨기는 OfficeLayer 잠금과 같아야 한다 ―
	// 제조 연구 중에는 나가기가 남아 있어 맨 위 전폭 진행 줄이 나가기와 겹치므로 블록 아래에 둔다
	const bool bStripOnTop = IsFocusLockedPhase(State);
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UOfficeLayerWidget* Layer = UIMgr->GetOfficeLayer())
		{
			Layer->RefreshTopBar();
			Layer->SetTopBlockHidden(bStripOnTop);
		}
	}
	ApplyStripRailPlacement(bStripOnTop);

	// 8. 판매 곡선 — 운영 중에만 표시 (idle/개발 중 빈 박스 방지)
	if (SalesCurve)
	{
		SalesCurve->SetVisibility(bIsInOperationMode
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 9. 우레일 대기존 — 힌트박스+착수 CTA 묶음은 대기에만 (스위처 밖으로 나가 자체 토글 필요)
	const bool bIdle = bManufacturing ? !bManufacturingResearch : State == EProjectLifecycle::Idle;
	if (UWidget* ProjectStrip = GetWidgetFromName(TEXT("StripRoot")))
	{
		ProjectStrip->SetVisibility(bManufacturing || (!bIdle && !bIsInOperationMode)
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshOpCardVisibility(State);
	if (OpCardStatusRow)
	{
		// 보고 대기에는 「운영 중」만 지우고 자리는 남긴다 ― 접으면 제목이 올라가 카드 높이가 바뀐다
		OpCardStatusRow->SetVisibility(State == EProjectLifecycle::Operating ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	for (const TCHAR* Name : {TEXT("TimerCluster"), TEXT("DevDiv2")})
	{
		if (UWidget* TimerPart = GetWidgetFromName(Name))
		{
			TimerPart->SetVisibility(bManufacturing && !bManufacturingResearch ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
	if (StripSpeedButton)
	{
		StripSpeedButton->SetVisibility(bManufacturing ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	bIdlePulse = bIdle;   // 대기에만 CTA 펄스 (NativeTick)
	if (RightIdleZone)
	{
		RightIdleZone->SetVisibility(bIdle ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	else if (OpenBoardButton)
	{
		// 존 미배치 WBP 폴백 — 버튼 단독 토글
		OpenBoardButton->SetVisibility(bIdle ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (IdleTierProgress)
	{
		IdleTierProgress->SetVisibility(bIdle ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 10. 타이머 — 대기 중엔 숨김 (개발/운영/출시대기만 표시)
	const ESlateVisibility TimerVis = bIdle ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	if (TimerRing)
	{
		TimerRing->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ProcessTimeText)
	{
		ProcessTimeText->SetVisibility(TimerVis);
	}
	RefreshManufacturingPointsStatus();
	RefreshPlacementEditControls();
}

void UOfficeMainWidget::RefreshManufacturingPointsStatus()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	const bool bManufacturing = GI && IsManufacturingType(GI->GetCurrentBuildingCompanyType());
	const UManufacturingDevelopmentManager* ResearchMgr = bManufacturing ? GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	const int32 BuildingID = bManufacturing ? GI->GetCurrentManagedBuildingIndex() : INDEX_NONE;
	const FManufacturingResearchStatus Research = ResearchMgr ? ResearchMgr->GetActiveResearchStatus(BuildingID) : FManufacturingResearchStatus();
	const bool bResearchActive = Research.bInProgress;
	if (UWidget* PointsHeading = GetWidgetFromName(TEXT("ManufacturingPointsHeading")))
	{
		PointsHeading->SetVisibility(bManufacturing && !bResearchActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* ProjectNameBox = GetWidgetFromName(TEXT("DevProjectNameBox")))
	{
		ProjectNameBox->SetVisibility(bManufacturing && !bResearchActive ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ManufacturingExperienceSummary)
	{
		ManufacturingExperienceSummary->SetVisibility(bManufacturing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* ExperienceBox = GetWidgetFromName(TEXT("ManufacturingExperienceSummaryBox")))
	{
		ExperienceBox->SetVisibility(bManufacturing ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bManufacturing) return;
	const UManufacturingDevelopmentManager* PointsMgr = ResearchMgr;
	const UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	const TArray<double> Balances = PointsMgr ? PointsMgr->GetDisciplinePoints(BuildingID) : TArray<double>();
	const int32 RestingCount = PointsMgr ? PointsMgr->GetRestingEmployeeCount(BuildingID) : 0;
	if (bResearchActive && TableMgr)
	{
		bool bFoundProduct = false;
		const FProjectData Product = TableMgr->GetProjectData(GI->GetCurrentBuildingCompanyType(), Research.ActiveProjectIndex, bFoundProduct);
		if (DevProjectName) DevProjectName->SetText(bFoundProduct ? Product.ProjectName : FText::GetEmpty());
		if (!bFoundProduct) UE_LOG(LogTemp, Warning, TEXT("[Office] 제조 연구 제품 %d DT 행 없음"), Research.ActiveProjectIndex);
		const float Remaining = FMath::Max(0.f, Research.RequiredWorkSeconds - Research.CompletedWorkSeconds);
		if (ProcessTimeLabel) ProcessTimeLabel->SetText(NSLOCTEXT("Office", "ManufacturingResearchWorkRemaining", "남은 근무"));
		if (ProcessTimeText) ProcessTimeText->SetText(FText::Format(NSLOCTEXT("Office", "ManufacturingResearchSeconds", "{0}초"), FText::AsNumber(FMath::CeilToInt(Remaining))));
	}
	if (DevGenreBadgeBox) DevGenreBadgeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (DevGenreBadge)
	{
		const int32 ProgressPercent = Research.RequiredWorkSeconds > 0.f
			? FMath::Clamp(FMath::RoundToInt(100.f * Research.CompletedWorkSeconds / Research.RequiredWorkSeconds), 0, 100) : 0;
		DevGenreBadge->SetText(bResearchActive
			? FText::Format(NSLOCTEXT("Office", "ManufacturingResearchProgressBadge", "개발 {0}% / 휴식 {1}명"),
				FText::AsNumber(ProgressPercent), FText::AsNumber(RestingCount))
			: FText::Format(NSLOCTEXT("Office", "ManufacturingWorkStatus", "휴식 {0}명"), FText::AsNumber(RestingCount)));
	}
	if (ManufacturingExperienceSummary)
	{
		const UEmployeeManager* Employees = GI->GetSubsystem<UEmployeeManager>();
		int32 ReadyCount = 0;
		if (Employees)
		{
			for (const FEmployeeInstance& Employee : Employees->GetAllEmployeesRef())
			{
				if (!GI->IsVisitMode() && Employee.AssignedBuildingIndex == BuildingID
					&& EmployeeTrainingRules::CanLevelUp(Employee)) ++ReadyCount;
			}
		}
		ManufacturingExperienceSummary->SetText(FText::Format(
			NSLOCTEXT("Office", "ManufacturingExperienceSummary", "근무 중 경험치 누적\n레벨업 가능 {0}명"),
			FText::AsNumber(ReadyCount)));
	}
	for (int32 FieldIndex = 0; FieldIndex < 6; ++FieldIndex)
	{
		const FText Name = FieldIndex < 3 && TableMgr
			? TableMgr->GetDisciplineDisplayName(GI->GetCurrentBuildingCompanyType(), FieldIndex) : FText::GetEmpty();
		if (UWidget* Cell = StripStageCells.IsValidIndex(FieldIndex) ? StripStageCells[FieldIndex] : nullptr)
		{
			Cell->SetVisibility(Name.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
		if (Name.IsEmpty())
		{
			if (FieldIndex < 3)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Office] 제조업 분야 %d 표시명 없음 — DT_DisciplineDisplay 확인"), FieldIndex);
			}
			continue;
		}
		if (UCommonTextBlock* NameText = StripStageNames.IsValidIndex(FieldIndex) ? StripStageNames[FieldIndex] : nullptr)
		{
			NameText->SetText(Name);
		}
		if (UImage* Icon = StripStageDisciplineIcons.IsValidIndex(FieldIndex) ? StripStageDisciplineIcons[FieldIndex] : nullptr)
		{
			const int32 IconIndex = FieldIndex == 0 ? 1 : FieldIndex == 1 ? 4 : 5;
			UTexture2D* Texture = UGlobalUtilFunctions::GetDisciplineIcon(IconIndex);
			Icon->SetBrushFromTexture(Texture, false);
			Icon->SetColorAndOpacity(UGlobalUtilFunctions::GetStepColor(FieldIndex + 1));
			Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		const int32 CurrentCapability = Research.TeamCapability.IsValidIndex(FieldIndex) ? Research.TeamCapability[FieldIndex] : 0;
		const int32 RequiredCapability = Research.RequiredCapability.IsValidIndex(FieldIndex) ? Research.RequiredCapability[FieldIndex] : 0;
		const float ResearchPct = bResearchActive && RequiredCapability > 0
			? static_cast<float>(CurrentCapability) / RequiredCapability : 0.f;
		StripStageTargetScore[FieldIndex] = bResearchActive ? RequiredCapability : 0.f;
		StripStageShownPct[FieldIndex] = ResearchPct;
		StripStageTargetPct[FieldIndex] = ResearchPct;
		bStripStagePendingDirty[FieldIndex] = false;
		ApplyStripStageVisual(FieldIndex, ResearchPct);
		if (UCommonTextBlock* ValueText = StripStageVals.IsValidIndex(FieldIndex) ? StripStageVals[FieldIndex] : nullptr)
		{
			const double Balance = Balances.IsValidIndex(FieldIndex) && FMath::IsFinite(Balances[FieldIndex])
				? FMath::Max(0.0, Balances[FieldIndex]) : 0.0;
			ValueText->SetMinDesiredWidth(0.0f);
			ValueText->SetText(bResearchActive
				? FText::Format(NSLOCTEXT("Office", "ManufacturingResearchCapability", "{0} / {1}"), FText::AsNumber(CurrentCapability), FText::AsNumber(RequiredCapability))
				: FText::Format(NSLOCTEXT("Office", "ManufacturingFieldBalance", "{0} P"),
					UGlobalUtilFunctions::AbbreviateNumberFloat(Balance, ENumberAbbrevStyle::WesternSuffix, ENumberRoundMode::Floor)));
		}
	}
}

void UOfficeMainWidget::OnManufacturingPointsChanged(int32 BuildingID)
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI || !IsManufacturingType(GI->GetCurrentBuildingCompanyType())
		|| GI->GetCurrentManagedBuildingIndex() != BuildingID) return;
	const UManufacturingDevelopmentManager* ResearchMgr = GI->GetSubsystem<UManufacturingDevelopmentManager>();
	if (!ResearchMgr) return;
	const bool bResearchActive = ResearchMgr->GetActiveResearchStatus(BuildingID).bInProgress;
	const int32 PendingProduct = ResearchMgr->GetPendingResearchResult(BuildingID);
	if (bManufacturingResearchObserved && bResearchActive != bManufacturingResearchWasActive)
	{
		if (bResearchActive)
		{
			OnStepStartEffectReceived();
		}
		else if (PendingProduct != INDEX_NONE && IsActivated())
		{
			PlayLaunchClimax(true);
			if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(ManufacturingResultTimer, this,
				&UOfficeMainWidget::TryShowManufacturingResearchResult, 0.9f, false);
		}
		if (const UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr)
		{
			RefreshOfficeUI(StageMgr->GetLifecycle());
		}
		if (bResearchActive) UWorkstationDevFxComponent::EnsureDevelopmentActive(GetWorld());
	}
	else
	{
		RefreshManufacturingPointsStatus();
		if (PendingProduct != INDEX_NONE && IsActivated() && !GetWorld()->GetTimerManager().IsTimerActive(ManufacturingResultTimer))
		{
			TryShowManufacturingResearchResult();
		}
	}
	bManufacturingResearchObserved = true;
	bManufacturingResearchWasActive = bResearchActive;
}

void UOfficeMainWidget::TryShowManufacturingResearchResult()
{
	if (bHandlingManufacturingResult || !IsActivated() || !GetWorld()) return;
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI || GI->IsVisitMode() || !IsManufacturingType(GI->GetCurrentBuildingCompanyType())) return;
	UManufacturingDevelopmentManager* ResearchMgr = GI->GetSubsystem<UManufacturingDevelopmentManager>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	const int32 BuildingID = GI->GetCurrentManagedBuildingIndex();
	const int32 ProjectIndex = ResearchMgr ? ResearchMgr->GetPendingResearchResult(BuildingID) : INDEX_NONE;
	if (ManufacturingResultProjectIndex != INDEX_NONE
		&& (!ActiveManufacturingResult.IsValid() || !ActiveManufacturingResult->IsActivated()
			|| ManufacturingResultBuildingID != BuildingID
			|| ManufacturingResultProjectIndex != ProjectIndex))
	{
		ManufacturingResultProjectIndex = INDEX_NONE;
		ManufacturingResultBuildingID = INDEX_NONE;
		ActiveManufacturingResult.Reset();
	}
	if (ProjectIndex == INDEX_NONE || !TableMgr || !UIBase) return;
	if (ManufacturingResultProjectIndex != INDEX_NONE || ManufacturingQuantityProjectIndex != INDEX_NONE
		|| UIBase->GetPromptStackCount() > 0
		|| UIBase->GetBottomStackCount() > 1 || (ManufacturingIntroOverlay.IsValid() && ManufacturingIntroOverlay->IsInViewport()))
	{
		GetWorld()->GetTimerManager().SetTimer(ManufacturingResultTimer, this,
			&UOfficeMainWidget::TryShowManufacturingResearchResult, 0.5f, false);
		return;
	}
	TSubclassOf<UUserWidget> ResultClass = TableMgr->GetWidgetClass(EWidgetType::LaunchConfirm);
	if (!ResultClass) return;
	ManufacturingResultBuildingID = BuildingID;
	ManufacturingResultProjectIndex = ProjectIndex;
	UCommonActivatableWidget* Result = UIBase->PushPromptClass(ResultClass.Get(), [this, GI, ProjectIndex](UCommonActivatableWidget& Widget)
	{
		if (ULaunchConfirmWidget* LaunchWidget = Cast<ULaunchConfirmWidget>(&Widget))
		{
			LaunchWidget->OnLaunchConfirmed.RemoveAll(this);
			LaunchWidget->OnLaunchCancelled.RemoveAll(this);
			LaunchWidget->OnLaunchRetryRequested.RemoveAll(this);
			LaunchWidget->OnManufacturingResearchResultClosed.RemoveAll(this);
			LaunchWidget->SetManufacturingResearchResult(GI->GetCurrentBuildingCompanyType(), ProjectIndex);
			LaunchWidget->OnManufacturingResearchResultClosed.AddUniqueDynamic(this, &UOfficeMainWidget::OnManufacturingResearchResultClosed);
		}
	});
	ActiveManufacturingResult = Cast<ULaunchConfirmWidget>(Result);
	if (!ActiveManufacturingResult.IsValid())
	{
		ManufacturingResultBuildingID = INDEX_NONE;
		ManufacturingResultProjectIndex = INDEX_NONE;
		ActiveManufacturingResult.Reset();
		UE_LOG(LogTemp, Error, TEXT("[Office] LaunchConfirm 등록 클래스가 제조 연구 결과 위젯이 아닙니다."));
		return;
	}
	if (ScoreOrbContainer) ScoreOrbContainer->FadeDevelopmentDim();
}

void UOfficeMainWidget::OnManufacturingResearchResultClosed(bool bOpenQuantity)
{
	if (bHandlingManufacturingResult) return;
	TGuardValue<bool> HandlingResultGuard(bHandlingManufacturingResult, true);
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UManufacturingDevelopmentManager* ResearchMgr = GI ? GI->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr;
	const int32 ProductIndex = ManufacturingResultProjectIndex;
	const int32 BuildingID = ManufacturingResultBuildingID;
	const bool bSaved = ResearchMgr && ProductIndex != INDEX_NONE
		&& ResearchMgr->AcknowledgeResearchResult(BuildingID, ProductIndex);
	ManufacturingResultProjectIndex = INDEX_NONE;
	ManufacturingResultBuildingID = INDEX_NONE;
	ActiveManufacturingResult.Reset();
	UWorkstationDevFxComponent::NotifyDevelopmentReset(GetWorld());
	if (AOfficeCameraPawn* Camera = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Camera->ReleasePresentationPullIn(0.3f);
	}
	if (ScoreOrbContainer) ScoreOrbContainer->ResetDevelopmentEffects();
	if (!bSaved)
	{
		if (UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			UIMgr->ShowNotification(NSLOCTEXT("Office", "ResearchResultSaveFailed", "연구 결과 저장에 실패했습니다. 다시 확인해주세요."), 3.f, ENotificationType::Failed);
		}
		if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(ManufacturingResultTimer, this,
			&UOfficeMainWidget::TryShowManufacturingResearchResult, 0.5f, false);
		return;
	}
	if (bOpenQuantity && GI && GI->GetCurrentManagedBuildingIndex() == BuildingID)
	{
		ManufacturingQuantityProjectIndex = ProductIndex;
		ManufacturingQuantityBuildingID = BuildingID;
		if (GetWorld()) GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UOfficeMainWidget::TryOpenManufacturingQuantity);
	}
	else if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ManufacturingResultTimer, this,
			&UOfficeMainWidget::TryShowManufacturingResearchResult, 0.5f, false);
	}
}

void UOfficeMainWidget::TryOpenManufacturingQuantity()
{
	if (ManufacturingQuantityProjectIndex == INDEX_NONE || !GetWorld()) return;
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIMgr ? UIMgr->GetUIBase() : nullptr;
	if (!GI || GI->IsVisitMode() || GI->GetCurrentManagedBuildingIndex() != ManufacturingQuantityBuildingID
		|| !IsManufacturingType(GI->GetCurrentBuildingCompanyType()) || !TableMgr || !UIBase)
	{
		ManufacturingQuantityProjectIndex = INDEX_NONE;
		ManufacturingQuantityBuildingID = INDEX_NONE;
		return;
	}
	if (UIBase->GetPromptStackCount() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(ManufacturingQuantityTimer, this,
			&UOfficeMainWidget::TryOpenManufacturingQuantity, 0.1f, false);
		return;
	}
	const TSubclassOf<UUserWidget> BoardClass = TableMgr->GetWidgetClass(EWidgetType::PitchBoard);
	if (!BoardClass) return;
	const int32 ProductIndex = ManufacturingQuantityProjectIndex;
	ManufacturingQuantityProjectIndex = INDEX_NONE;
	ManufacturingQuantityBuildingID = INDEX_NONE;
	if (UPitchBoardWidget* Board = Cast<UPitchBoardWidget>(UIBase->PushPromptClass(BoardClass.Get()))) Board->OpenProject(ProductIndex);
}

void UOfficeMainWidget::HandleEmployeeExperienceChanged(int32 EmployeeID)
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UEmployeeManager* Employees = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!GI || !Employees || !IsManufacturingType(GI->GetCurrentBuildingCompanyType())) return;
	const FEmployeeInstance* Employee = Employees->FindEmployee(EmployeeID);
	if (Employee && Employee->AssignedBuildingIndex == GI->GetCurrentManagedBuildingIndex())
	{
		RefreshManufacturingPointsStatus();
	}
}

void UOfficeMainWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateExpansionHotspots(true);
	UpdatePlacementWorkstationMarker(InDeltaTime);
	UpdateDevelopmentEffectArea();
	if (StepStartBannerWidget && GetWorld() && InDeltaTime > KINDA_SMALL_NUMBER)
	{
		StepStartBannerWidget->SetPresentationSpeed(GetWorld()->GetDeltaSeconds() / InDeltaTime);
	}

	// 하단 도크 슬라이드 — 개발중이면 아래로 밀어내고, 아니면 제자리로 보간(+페이드). 오프스크린이라 버튼 클릭 불가.
	if (OfficeDockTray)
	{
		const float Target = bDockHidden ? 1.f : 0.f;
		if (!FMath::IsNearlyEqual(DockSlideAlpha, Target, 0.001f))
		{
			DockSlideAlpha = FMath::FInterpTo(DockSlideAlpha, Target, InDeltaTime, DockSlideSpeed);
			// FInterpTo 는 목표에 정확히 안 닿음 — 도착 프레임만 스냅해야 알파가 실제 위치와 어긋나지 않는다
			if (FMath::IsNearlyEqual(DockSlideAlpha, Target, 0.001f))
			{
				DockSlideAlpha = Target;
			}
			ApplyDockSlide(DockSlideAlpha);
		}
	}

	// 대기 CTA 어텐션 — 스케일 바운스 대신 뒤 글로우 opacity 브리딩(세련). 버튼 크기 변화 0.
	if (CTAGlow)
	{
		if (bIdlePulse)
		{
			IdlePulseElapsed += InDeltaTime;
			// opacity 0.45~0.90, 주기 ~2.2s(2π/2.2≈2.856). 빛만 호흡 → 프리미엄 어텐션(튜토리얼보다 살짝 약).
			// 연출 감소: 호흡 대신 최대치 고정 — 어텐션은 정보라 끄지 않고 움직임만 끈다
			const float Breathe = FMotionGate::IsReduced()
				? 0.90f
				: 0.45f + 0.45f * (0.5f + 0.5f * FMath::Sin(IdlePulseElapsed * 2.856f));
			CTAGlow->SetRenderOpacity(Breathe);
		}
		else if (IdlePulseElapsed != 0.0f)
		{
			// 대기 종료 시 글로우 소등 1회
			IdlePulseElapsed = 0.0f;
			CTAGlow->SetRenderOpacity(0.0f);
		}
	}

	// 대기 CTA 샤인 스윕 — 버튼은 고정하고 광택 띠만 면을 가로지른다.
	// 버튼 자체를 상시 이동시키면 소수픽셀=지글거림 / 정수스냅=계단 중 하나를 피할 수 없다(2026-09-24 실측).
	// VFX 소스북 §2.5 "상시 반복되는 UI 흔들림 금지", 레퍼런스 24장 "강조는 링·글로우·선버스트·색 대비로만".
	if (CtaShineImage)
	{
		if (bIdlePulse)
		{
			const float Phase = FMath::Fmod(IdlePulseElapsed, CtaShinePeriod);
			if (Phase < CtaShineTravel && !FMotionGate::IsReduced())   // 샤인 스윕은 순수 장식 — 감소면 소등
			{
				const float T = Phase / CtaShineTravel;
				CtaShineImage->SetRenderTranslation(
					FVector2D(FMath::Lerp(-CtaShineSweepHalf, CtaShineSweepHalf, T), 0.0f));
				CtaShineImage->SetRenderOpacity(0.55f * FMath::Sin(T * PI));   // 진입/이탈 페이드
			}
			else
			{
				CtaShineImage->SetRenderOpacity(0.0f);
			}
		}
		else
		{
			CtaShineImage->SetRenderOpacity(0.0f);
		}
	}

	// 스테이지 바 — 폴백 플러시 / 부드러운 채움(≈3.3/s) / 착지 연출(펀치+플래시)
	const double NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const UOfficeStageProgressManager* PresentationMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	const bool bPresentationPaused = PresentationMgr && (PresentationMgr->IsEventPaused() || PresentationMgr->IsBoostGamblePending());
	const float PendingDelta = GetWorld() ? GetWorld()->GetDeltaSeconds() : InDeltaTime;
	const UCGGameInstance* OfficeGI = Cast<UCGGameInstance>(GetGameInstance());
	const bool bManufacturing = OfficeGI && IsManufacturingType(OfficeGI->GetCurrentBuildingCompanyType());
	for (int32 i = 0; !bManufacturing && i < 6; ++i)
	{
		// 오브와 같은 월드 시간만 제외해야 배속/이벤트 대기 중 폴백이 착지를 앞지르지 않는다.
		if (bPresentationPaused && bStripStagePendingDirty[i]) StripStagePendingSince[i] += PendingDelta;
		if (!bPresentationPaused && bStripStagePendingDirty[i] && NowSec - StripStagePendingSince[i] > PendingFlushTimeout)
		{
			FlushStripStagePending(i);   // orb 유실/스킵 — 연출 없이 값만
		}

		bool bVisualDirty = false;
		if (StripStageFlashAlpha[i] > 0.0f)
		{
			StripStageFlashAlpha[i] = FMath::Max(0.0f, StripStageFlashAlpha[i] - InDeltaTime / StripFlashDecayTime);
			bVisualDirty = true;   // 플래시 감쇠는 pct 정지 중에도 다시 그려야 함
		}
		// 문턱은 Pct 가 아니라 "표시 점수 0.5점" 환산 — 목표가 38만까지 커지면 고정 0.001Pct 가 381점이 되어
		// 초반 획득이 통째로 삼켜진다(숫자가 0에 눌러앉고 바도 안 움직임). 목표가 작을 땐 기존 0.001 을 상한으로.
		const float PctEps = (StripStageTargetScore[i] > 0.0f)
			? FMath::Min(0.001f, 0.5f / StripStageTargetScore[i]) : 0.001f;
		if (StripStageShownPct[i] != StripStageTargetPct[i])
		{
			StripStageShownPct[i] = FMath::IsNearlyEqual(StripStageShownPct[i], StripStageTargetPct[i], PctEps)
				? StripStageTargetPct[i]
				: FMath::FInterpConstantTo(StripStageShownPct[i], StripStageTargetPct[i], InDeltaTime, 3.3f);
			bVisualDirty = true;
		}
		if (bVisualDirty)
		{
			ApplyStripStageVisual(i, StripStageShownPct[i], true);
		}

		if (StripStageGoalElapsed[i] < StripGoalDuration)
		{
			StripStageGoalElapsed[i] = FMotionGate::IsReduced() ? StripGoalDuration
				: FMath::Min(StripGoalDuration, StripStageGoalElapsed[i] + InDeltaTime);
			const float GoalTime = StripStageGoalElapsed[i];
			// 원 크기를 넘지 않게 작았다가 돌아온다 — 1.0 초과 확대는 트레이 경계에서 잘리고 글자가 뭉개진다.
			const float PunchT = FMath::Clamp(GoalTime / 0.33f, 0.0f, 1.0f);
			const float PunchScale = FMath::Lerp(0.96f, 1.0f, 1.0f - FMath::Square(1.0f - PunchT));
			if (UWidget* Cell = StripStageCells.IsValidIndex(i) ? StripStageCells[i] : nullptr)
			{
				Cell->SetRenderScale(FVector2D(PunchScale, PunchScale));
			}
			// 초과로 넘어갈 때만 게이지 테두리가 금색으로 번쩍 — 점수 글자가 덮지 않는 신호라 광택보다 잘 보인다.
			if (UImage* Ring = StripStageGoalRings.IsValidIndex(i) ? StripStageGoalRings[i] : nullptr)
			{
				const float RingT = GoalTime / StripGoalDuration;
				const float RingAlpha = RingT < 0.35f ? 1.0f : 1.0f - (RingT - 0.35f) / 0.65f;
				Ring->SetRenderOpacity(GoalTime < StripGoalDuration && StripStageVisualState[i] == 3
					? FMath::Clamp(RingAlpha, 0.0f, 1.0f) : 0.0f);
			}
		}

		if (StripStagePunchElapsed[i] < StripPunchDuration)
		{
			StripStagePunchElapsed[i] += InDeltaTime;
			const float T = FMath::Clamp(StripStagePunchElapsed[i] / StripPunchDuration, 0.0f, 1.0f);
			float S;
			if (T < 0.3f)
			{
				S = FMath::Lerp(1.0f, StripStagePunchPeak[i], T / 0.3f);   // 빠른 팽창
			}
			else
			{
				const float D = (T - 0.3f) / 0.7f;
				S = FMath::Lerp(StripStagePunchPeak[i], 1.0f, 1.0f - FMath::Square(1.0f - D));   // EaseOut 복귀
			}
			if (T >= 1.0f) S = 1.0f;
			if (UCommonTextBlock* ValT = StripStageVals.IsValidIndex(i) ? StripStageVals[i] : nullptr)
			{
				ValT->SetRenderScale(FVector2D(S, S));
			}
		}
	}

	// 스트립 셸 SDF — Wpx/Hpx가 실제 위젯 크기와 일치해야 코너 정합 (크기 변화 시에만 재주입)
	UpdateStripShellMaterialSize();
	UpdateMissionAnchorBelowStrip(InDeltaTime);

	// 마지막 3초도 숫자와 단위는 고정하고 색만 부드럽게 강조한다.
	if (bPresentationPaused || FMotionGate::IsReduced()) ResetLastSpurtVisual();
	if (bLastSpurtVisual)
	{
		LastSpurtElapsed += InDeltaTime;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(LastSpurtElapsed * 5.2f);
		if (ProcessTimeText)
		{
			static const FLinearColor BaseCol(0.838799f, 0.854993f, 0.871367f, 1.0f);
			static const FLinearColor GoldCol(0.921582f, 0.679542f, 0.270498f, 1.0f);
			ProcessTimeText->SetColorAndOpacity(FSlateColor(FMath::Lerp(BaseCol, GoldCol, Pulse)));
		}
	}
}

// 상단 밴드(회사배너/트렌드칩/티어칩)는 OfficeLayerWidget 로 이관 — RefreshOfficeUI 가 Layer->RefreshTopBar() 트리거.

void UOfficeMainWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindExpansionHotspots();
	if (EquipmentManualButton) EquipmentManualButton->OnClicked().AddUObject(this, &ThisClass::ShowEquipmentManualEmployees);
	if (auto* Equipment = GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>()) Equipment->OnEquipmentChanged.AddUObject(this, &ThisClass::RefreshEquipmentManualButton);

	// 상단 스트립 스테이지 셀과 직능 아이콘/목표 광택을 트리 이름으로 캐시한다.
	CacheStripStageCells();

	// 폭 예약 없음 — 숫자가 링 중앙에 겹쳐 자리를 링이 소유하므로 자릿수가 변해도 레이아웃이 안 흔들린다.

	// 서브시스템 델리게이트는 위젯 수명 전체에 걸쳐 유지 (임시 비활성 중에도 수신 보장)
	// — OpenBoardButton → PushBottomClass 시 잠시 Deactivated 되어도 OnStepStartEffect 수신 유지
	BindStageProgressDelegates();
	BindBoardEventDelegates();
	BindOperationDelegates();
	if (UManufacturingDevelopmentManager* PointsMgr = GetGameInstance()->GetSubsystem<UManufacturingDevelopmentManager>())
	{
		PointsMgr->OnPointsChanged.AddUObject(this, &UOfficeMainWidget::OnManufacturingPointsChanged);
	}
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.AddUObject(this, &ThisClass::RefreshCompanyFeatureButtons);
	}

	// OfficeManager는 WorldSubsystem이라 위젯과 동일 수명
	if (UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>())
	{
		OfficeMgr->OnWorkstationCountChanged.AddUObject(
			this, &UOfficeMainWidget::UpdateWorkstationBadge);
	}

	// 벤치 적립/착석 순간 [책상 배치] 배지 즉시 재평가 (미배치 발생 = 책상 필요 신호)
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		EmpMgr->OnEmployeeRosterChanged.AddUObject(this, &UOfficeMainWidget::UpdateWorkstationBadge);
		// 근무 획득은 OnPointsChanged의 사무실별 정산 알림에서 한 번만 갱신한다.
		EmpMgr->OnEmployeeLevelUp.AddWeakLambda(this, [this](int32 EmployeeID, int32)
		{
			HandleEmployeeExperienceChanged(EmployeeID);
		});
	}

	// 잠금이 풀리는 순간 서버값으로 정정된 다이아 잔액으로 컨티뉴 슬롯 상태를 다시 계산한다
	if (UCloudEconomySubsystem* Cloud = GetGameInstance()->GetSubsystem<UCloudEconomySubsystem>())
	{
		Cloud->OnEconomyReady.AddUObject(this, &UOfficeMainWidget::HandleEconomyReady);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] NativeConstruct - persistent delegates bound"));
}

void UOfficeMainWidget::HandleEconomyReady(bool bReady)
{
	// 잠긴 동안에도 컨티뉴는 살려 둔다 — 조용히 죽은 버튼은 "다이아가 사라졌다" 로 읽힌다.
	// 탭하면 NotifyCloudOpFailure 가 3상태 중 하나를 말해준다(§7.2)
	RefreshLeftSlotState();
}

void UOfficeMainWidget::NativeDestruct()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ManufacturingIntroTimer); }
	if (UPanelIntroOverlayWidget* Intro = ManufacturingIntroOverlay.Get()) { Intro->RemoveFromParent(); }
	ManufacturingIntroOverlay.Reset();
	UnbindExpansionHotspots();
	UnbindPlacementEditControls();
	SetPlacementWorkstationMarker(nullptr, nullptr);
	CloseEquipmentManualDialog();
	if (EquipmentManualButton) EquipmentManualButton->OnClicked().RemoveAll(this);
	if (auto* Equipment = GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>()) Equipment->OnEquipmentChanged.RemoveAll(this);
	// 배속을 켠 채 사무실을 떠나면 세계가 2배속으로 남는다 — 복원의 유일한 지점.
	// 활성/비활성 쌍이 아니라 파괴 시점인 이유: 모달이 뜰 때마다 Deactivate 되므로 거기서 리셋하면 배속이 조용히 풀린다.
	ApplyDevSpeed(1.0f);

	UnbindStageProgressDelegates();
	UnbindBoardEventDelegates();
	UnbindOperationDelegates();
	if (UManufacturingDevelopmentManager* PointsMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UManufacturingDevelopmentManager>() : nullptr)
	{
		PointsMgr->OnPointsChanged.RemoveAll(this);
	}
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->OnGameDataLoaded.RemoveAll(this);
	}

	if (UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>())
	{
		OfficeMgr->OnWorkstationCountChanged.RemoveAll(this);
	}

	if (UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		EmpMgr->OnEmployeeRosterChanged.RemoveAll(this);
		EmpMgr->OnEmployeeLevelUp.RemoveAll(this);
	}

	if (UCloudEconomySubsystem* Cloud = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCloudEconomySubsystem>() : nullptr)
	{
		Cloud->OnEconomyReady.RemoveAll(this);
	}

	// 진행 중인 이펙트 위젯 정리
	RemoveStepStartEffect();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FeverOverlayTimerHandle);  // 페이드 중 파괴 시 반복 타이머 잔류 방지 (Construct↔Destruct 쌍)
	}

	if (ScoreOrbContainer)
	{
		ScoreOrbContainer->OnScoreOrbArrived.RemoveAll(this);
		ScoreOrbContainer->RemoveFromParent();
		ScoreOrbContainer = nullptr;
	}

	Super::NativeDestruct();
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] NativeDestruct"));
}

UWidget* UOfficeMainWidget::GetRecruitmentButtonWidget() const
{
	return RecruitmentButton;
}

UWidget* UOfficeMainWidget::GetOpenBoardButtonWidget() const
{
	return OpenBoardButton;
}

UWidget* UOfficeMainWidget::GetWorkstationOpenButtonWidget() const
{
	return WorkstationOpenButton;
}

UWidget* UOfficeMainWidget::GetDecorationOpenButtonWidget() const
{
	return DecorationOpenButton;
}

UWidget* UOfficeMainWidget::GetEmployeeButtonWidget() const
{
	return EmployeeButton;
}

UWidget* UOfficeMainWidget::GetEquipmentGachaButtonWidget() const
{
	return EquipmentGachaButton;
}

void UOfficeMainWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ManufacturingIntroTimer, this,
			&UOfficeMainWidget::TryPlayManufacturingIntro, 0.5f, false);
	}
	RefreshCompanyFeatureButtons();
	BuildPlacementEditControls();
	BindPlacementEditControls();

	// 버튼 이벤트 바인딩
	if (WorkstationOpenButton) WorkstationOpenButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnWorkstationButtonClicked);
	if (DecorationOpenButton) DecorationOpenButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnDecorationButtonClicked);
	if (TrainingCenterOpenButton) TrainingCenterOpenButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnTrainingCenterButtonClicked);
	if (BusinessTripOpenButton) BusinessTripOpenButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnBusinessTripButtonClicked);
	if (EmployeeButton) EmployeeButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnEmployeeButtonClicked);
	if (RecruitmentButton) RecruitmentButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnRecruitmentButtonClicked);
	if (EquipmentGachaButton) EquipmentGachaButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnEquipmentGachaButtonClicked);
	if (CollectionBtn) CollectionBtn->OnClicked().AddUObject(this, &UOfficeMainWidget::OnCollectionBtnClicked);
	// 포트폴리오/상단 밴드는 OfficeLayer 소관 — 여기서 배선/갱신 안 함 (RefreshOfficeUI 가 Layer 트리거).

	// M4 미션 가이드 — [채용] 버튼 하이라이트 타겟 등록
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->RegisterOfficeMainWidget(this);
	}

	// ProjectCard 델리게이트 바인딩
	if (UI_OfficeProjectCardCurrent)
	{
		UI_OfficeProjectCardCurrent->OnStartButtonClicked.AddDynamic(this, &UOfficeMainWidget::OnStartButtonClickedHandler);
		UI_OfficeProjectCardCurrent->OnStopOperationClicked.AddDynamic(this, &UOfficeMainWidget::OnStopOperationClickedHandler);
	}


	// 보드 열기 버튼 바인딩
	if (OpenBoardButton)
	{
		OpenBoardButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnOpenBoardButtonClicked);
	}

	// 운영종료 버튼(스트립) — 은퇴한 카드의 OnStopOperationClicked 대체
	if (StripStopButton)
	{
		StripStopButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnStopOperationClickedHandler);
	}

	// 배속 토글(스트립) — 사무실 재진입마다 ×1 로 시작한다(월드가 새로 만들어져 TimeDilation 도 1.0)
	if (StripSpeedButton)
	{
		StripSpeedButton->OnClicked.AddDynamic(this, &UOfficeMainWidget::OnSpeedToggleClickedHandler);
		// 재활성화 때 배속을 리셋하지 않는다 — 모달(픽칭/출시확인)이 뜨고 닫힐 때마다 켜둔 배속이 풀린다.
		// 아이콘만 현재 배속에 맞춰 되그린다.
		RefreshSpeedIcon();
	}

	// 라이프사이클 기반 UI 초기화 (단일 진입점)
	if (UOfficeStageProgressManager* SM = GetWorld()->GetSubsystem<UOfficeStageProgressManager>())
	{
		RefreshOfficeUI(SM->GetLifecycle());
	}
	if (const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (IsManufacturingType(GI->GetCurrentBuildingCompanyType()))
		{
			if (const UManufacturingDevelopmentManager* ResearchMgr = GI->GetSubsystem<UManufacturingDevelopmentManager>())
			{
				bManufacturingResearchWasActive = ResearchMgr->GetActiveResearchStatus(GI->GetCurrentManagedBuildingIndex()).bInProgress;
				bManufacturingResearchObserved = true;
				if (bManufacturingResearchWasActive) UWorkstationDevFxComponent::EnsureDevelopmentActive(GetWorld());
			}
		}
	}
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(ManufacturingResultTimer, this,
		&UOfficeMainWidget::TryShowManufacturingResearchResult, 0.9f, false);

	// 재활성 시점의 도크는 연출 대상이 아니라 결과 상태 — 알파를 믿지 말고 실제 트랜스폼을 단언
	DockSlideAlpha = bDockHidden ? 1.f : 0.f;
	ApplyDockSlide(DockSlideAlpha);

	// 업무공간 뱃지 갱신 (델리게이트는 NativeConstruct에서 영구 바인딩됨)
	UpdateWorkstationBadge();

	// ScoreOrb 컨테이너 생성 (TableManager 패턴 — WBP 클래스 로드)
	if (!ScoreOrbContainer)
	{
		UCGGameInstance* GI = UCGGameInstance::GetInstance();
		UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (TableMgr && PC)
		{
			TSubclassOf<UUserWidget> OrbClass = TableMgr->GetWidgetClass(EWidgetType::ScoreOrbContainer);
			if (OrbClass)
			{
				ScoreOrbContainer = CreateWidget<UScoreOrbContainerWidget>(PC, OrbClass);
				if (ScoreOrbContainer)
				{
					// 뷰포트 직접 부착은 UIBase(프롬프트 스택=이벤트 팝업) 위에 그려짐 →
					// OfficeMain 루트(Overlay) 마지막 자식으로 부착해 HUD 위·팝업 아래 유지
					ScoreOrbContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
					ScoreOrbContainer->SetDevelopmentPresentationEnabled(true);
					ScoreOrbContainer->OnScoreOrbArrived.AddUObject(this, &UOfficeMainWidget::OnScoreOrbArrivedReceived);
					bool bAttached = false;
					if (UOverlay* RootOverlay = Cast<UOverlay>(WidgetTree->RootWidget))
					{
						if (UOverlaySlot* OrbOverlaySlot = RootOverlay->AddChildToOverlay(ScoreOrbContainer))
						{
							OrbOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
							OrbOverlaySlot->SetVerticalAlignment(VAlign_Fill);
							bAttached = true;
						}
					}
					if (!bAttached)
					{
						ScoreOrbContainer->AddToViewport(500);   // 루트가 Overlay 가 아닐 때 폴백
					}
				}
			}
		}
	}

	// 현재 상태 확인
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	bIsInOperationMode = false;

	int32 CurrentBuildingID = GetCurrentBuildingIDAsInt();
	UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] NativeOnActivated - CurrentBuildingID: %d"), CurrentBuildingID);

	// 운영 중인지 확인
	FOperationData* CurrentOp = GetCurrentBuildingOperation();
	UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] NativeOnActivated - CurrentOp: %s"), CurrentOp ? TEXT("Found") : TEXT("NULL"));

	if (CurrentOp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] NativeOnActivated - Operation State: %d, Project: %s"),
			static_cast<int32>(CurrentOp->State), *CurrentOp->ProjectName);

		if (CurrentOp->State == EOperationState::Operating || CurrentOp->State == EOperationState::Paused)
		{
			bIsInOperationMode = true;
			UpdateUIForOperationMode(true);
			UpdateOperationUI(*CurrentOp);

			if (UI_OfficeProjectCardCurrent)
			{
				UI_OfficeProjectCardCurrent->SwitchToOperationMode(
					CurrentOp->ActualRevenuePerSecond,
					static_cast<int64>(CurrentOp->TotalRevenueEarned),
					QualityGradeToAlphabetString(CurrentOp->QualityGrade));
			}

			RefreshProjectNameBar();

					UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] NativeOnActivated - Operation mode restored"));
			return;
		}
	}

	// 오피스 밖에서 운영 완료된 Pending Report 체크 — 위젯 생애에 한 번만 (Collection 패널 등 여닫기 시 재트리거 방지)
	if (!bHasCheckedPendingReport)
	{
		bHasCheckedPendingReport = true;

		UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
		if (GI)
		{
			UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
			if (OpMgr && OpMgr->HasPendingReport(CurrentBuildingID))
			{
				ShowPendingReportModal();
			}
		}
	}

	// 기본 상태 (idle 또는 스테이지 진행 준비)
	bIsInOperationMode = false;
	UpdateUIForOperationMode(false);
	UpdateProjectCardProgress();

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] NativeOnActivated - Buttons bound"));
}

void UOfficeMainWidget::TryPlayManufacturingIntro()
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UPanelIntroSubsystem* IntroMgr = GI ? GI->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
	static const FName IntroKey(TEXT("ManufacturingOffice"));
	if (!IsActivated() || !GI || !IsManufacturingType(GI->GetCurrentBuildingCompanyType())
		|| !IntroMgr || !IntroMgr->ShouldPlay(IntroKey)) { return; }
	if (ManufacturingIntroOverlay.IsValid() && ManufacturingIntroOverlay->IsInViewport()) { return; }
	ManufacturingIntroOverlay = IntroMgr->PlayIntro(this, IntroKey, false);
	if (!ManufacturingIntroOverlay.IsValid() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ManufacturingIntroTimer, this,
			&UOfficeMainWidget::TryPlayManufacturingIntro, 0.5f, false);
	}
}

void UOfficeMainWidget::NativeOnDeactivated()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ManufacturingIntroTimer);
		GetWorld()->GetTimerManager().ClearTimer(ManufacturingResultTimer);
	}
	if (UPanelIntroOverlayWidget* Intro = ManufacturingIntroOverlay.Get()) { Intro->RemoveFromParent(); }
	ManufacturingIntroOverlay.Reset();
	if (ScoreOrbContainer) ScoreOrbContainer->ResetDevelopmentEffects();
	ResetLastSpurtVisual();
	UnbindPlacementEditControls();
	CloseEquipmentManualDialog();
	Super::NativeOnDeactivated();
	UpdateExpansionHotspots();

	// 스택이 이 위젯을 Collapsed 시키면 Slate 가 틱을 안 돌려 보간이 중간값에서 얼어붙는다 → 떠나기 전에 목표로 스냅
	DockSlideAlpha = bDockHidden ? 1.f : 0.f;
	ApplyDockSlide(DockSlideAlpha);

	if (WorkstationOpenButton) WorkstationOpenButton->OnClicked().Clear();
	if (DecorationOpenButton) DecorationOpenButton->OnClicked().Clear();
	if (TrainingCenterOpenButton) TrainingCenterOpenButton->OnClicked().Clear();
	if (BusinessTripOpenButton) BusinessTripOpenButton->OnClicked().Clear();
	if (EmployeeButton) EmployeeButton->OnClicked().Clear();
	if (RecruitmentButton) RecruitmentButton->OnClicked().Clear();
	if (EquipmentGachaButton) EquipmentGachaButton->OnClicked().Clear();
	if (CollectionBtn) CollectionBtn->OnClicked().Clear();

	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->UnregisterOfficeMainWidget(this);
	}

	// ProjectCard 델리게이트 언바인딩
	if (UI_OfficeProjectCardCurrent)
	{
		UI_OfficeProjectCardCurrent->OnStartButtonClicked.RemoveAll(this);
		UI_OfficeProjectCardCurrent->OnStopOperationClicked.RemoveAll(this);
	}

	if (OpenBoardButton)
	{
		OpenBoardButton->OnClicked().RemoveAll(this);
	}

	if (StripStopButton)
	{
		StripStopButton->OnClicked().RemoveAll(this);
	}

	if (StripSpeedButton)
	{
		StripSpeedButton->OnClicked.RemoveDynamic(this, &UOfficeMainWidget::OnSpeedToggleClickedHandler);
	}

	// 서브시스템 델리게이트 / 이벤트 팝업 / 이펙트는 Destruct에서 정리 (일시 비활성 중 이벤트 수신 유지 목적)
	// 버튼 가시성은 비활성 시점에 강제 변경하지 않음 → 재활성 시 RefreshOfficeUI 가 라이프사이클 기준으로 결정

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] NativeOnDeactivated"));
}

// ========== 버튼 핸들러 ==========

void UOfficeMainWidget::RefreshCompanyFeatureButtons()
{
	USideContentSubsystem* Side = GetGameInstance() ? GetGameInstance()->GetSubsystem<USideContentSubsystem>() : nullptr;
	const int32 BuildingIndex = GetCurrentBuildingIDAsInt();
	const auto RefreshButton = [Side, BuildingIndex](UOfficeFeatureButtonWidget* FeatureButton, FName FeatureKey)
	{
		if (!FeatureButton) return;
		int32 RequiredLevel = 0;
		FText DisplayName;
		const bool bUnlocked = Side && Side->GetCompanyFeatureUnlock(BuildingIndex, FeatureKey, RequiredLevel, DisplayName);
		FeatureButton->SetVisibility(DisplayName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		FeatureButton->SetButtonText(DisplayName);
		if (RequiredLevel > 0)
		{
			FeatureButton->SetFeatureLocked(!bUnlocked,
				FText::Format(NSLOCTEXT("Office", "CompanyFeatureLevel", "Lv.{0}"), FText::AsNumber(RequiredLevel)),
				CompanyFeatureLockReason(RequiredLevel));
		}
	};
	RefreshButton(TrainingCenterOpenButton, TEXT("TrainingCenter"));
	RefreshButton(BusinessTripOpenButton, TEXT("BusinessTrip"));
}

void UOfficeMainWidget::OnCompanyTierUnlocked(int32 NewTier)
{
	RefreshCompanyFeatureButtons();
}

bool UOfficeMainWidget::CanOpenCompanyFeature(FName FeatureKey)
{
	USideContentSubsystem* Side = GetGameInstance() ? GetGameInstance()->GetSubsystem<USideContentSubsystem>() : nullptr;
	int32 RequiredLevel = 0;
	FText DisplayName;
	if (Side && Side->GetCompanyFeatureUnlock(GetCurrentBuildingIDAsInt(), FeatureKey, RequiredLevel, DisplayName)) return true;
	RefreshCompanyFeatureButtons();
	if (RequiredLevel > 0)
	{
		if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
		{
			UIManager->ShowRejectNotification(CompanyFeatureLockReason(RequiredLevel));
		}
	}
	return false;
}

void UOfficeMainWidget::OnBusinessTripButtonClicked()
{
	if (!CanOpenCompanyFeature(TEXT("BusinessTrip"))) return;
	const int32 BuildingIndex = GetCurrentBuildingIDAsInt();
	if (BuildingIndex == INDEX_NONE) return;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	USaveLoadManager* SaveMgr = GetWorld()->GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase() || !SaveMgr) return;

	TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::BusinessTripPanel);
	if (!WidgetClass) return;

	// 오피스는 이미 한 건물 안이라 허브를 거치지 않는다 — 그 건물의 출장으로 바로 간다
	UCommonActivatableWidget* Pushed = UIManager->GetUIBase()->PushPromptClass(
		TSubclassOf<UCommonActivatableWidget>(WidgetClass));
	if (UBusinessTripPanelWidget* Panel = Cast<UBusinessTripPanelWidget>(Pushed))
	{
		Panel->Configure(BuildingIndex, SaveMgr->GetBuildingCompanyType(BuildingIndex));
	}
}

void UOfficeMainWidget::OnTrainingCenterButtonClicked()
{
	if (!CanOpenCompanyFeature(TEXT("TrainingCenter"))) return;
	const int32 BuildingIndex = GetCurrentBuildingIDAsInt();
	if (BuildingIndex == INDEX_NONE) return;

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	USaveLoadManager* SaveMgr = GetWorld()->GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase() || !SaveMgr) return;

	TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::TrainingCenterPanel);
	if (!WidgetClass) return;

	// 오피스는 이미 한 건물 안이라 허브를 거치지 않는다 — 그 건물의 기술 연구소로 바로 간다
	UCommonActivatableWidget* Pushed = UIManager->GetUIBase()->PushPromptClass(
		TSubclassOf<UCommonActivatableWidget>(WidgetClass));
	if (UTrainingCenterPanelWidget* Panel = Cast<UTrainingCenterPanelWidget>(Pushed))
	{
		Panel->Configure(BuildingIndex, SaveMgr->GetBuildingCompanyType(BuildingIndex));
	}
}

void UOfficeMainWidget::OnWorkstationButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Workstation button clicked"));

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();

	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> WorkstationPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeWorkstationPanel);
	if (WorkstationPanel)
	{
		if (UIManager->GetUIBase()->PushBottomClass(WorkstationPanel.Get()))
		{
			if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
			{
				MissionMgr->NotifyPlacementModeOpened(EOfficePlacementKind::Desk);
			}
		}
	}
}

void UOfficeMainWidget::UpdateWorkstationBadge()
{
	if (!WorkstationOpenButton) return;

	// 미배치 직원 존재 = "책상 필요" 신호 (스펙 §2.5). 책상 배치는 상한이 없으므로 벤치 유무가 유일한 조건이다.
	bool bHasBench = false;
	if (UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		UCGGameInstance* CGI = Cast<UCGGameInstance>(GetGameInstance());
		const int32 BuildingIdx = CGI ? CGI->GetCurrentManagedBuildingIndex() : INDEX_NONE;
		bHasBench = EmpMgr->GetUnassignedEmployeesInBuilding(BuildingIdx).Num() > 0;
	}

	if (bHasBench)
	{
		WorkstationOpenButton->ShowBadge();
	}
	else
	{
		WorkstationOpenButton->HideBadge();
	}
}

void UOfficeMainWidget::OnDecorationButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Decoration button clicked"));

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();

	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> DecorationPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeDecorationPanel);
	if (DecorationPanel)
	{
		UIManager->GetUIBase()->PushBottomClass(DecorationPanel.Get());
	}
}

void UOfficeMainWidget::OnEmployeeButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Employee button clicked"));

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();

	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	// 사무실 직원 패널(할 일 순 첫 직원) — 위젯 클래스 미등록이면 아래 풀스크린 직원창으로 폴백
	if (UOfficeEmployeePanelWidget::PushForEmployee(this, UOfficeEmployeePanelWidget::FindFirstTodoEmployeeId(this)))
	{
		return;
	}

	// 직원창 = 풀스크린 UI_EmployeeWindow
	TSubclassOf<UUserWidget> EmployeeWindow = TableMgr->GetWidgetClass(EWidgetType::EmployeeWindow);
	if (!EmployeeWindow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] EmployeeWindow 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	if (!UIManager->GetUIBase()->PushPromptClass(EmployeeWindow.Get())) return;

	// 입력모드 쌍 — 닫힐 때 EmployeeWindow::NativeOnDeactivated 가 GoToNormalMode 복원
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
		{
			PC->GoToUIMode();
		}
	}
}

void UOfficeMainWidget::OnRecruitmentButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Recruitment button clicked"));

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();

	if (!GI || !TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> RecruitmentPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeRecruitmentModal);
	if (RecruitmentPanel)
	{
		UCommonActivatableWidget* Widget = UIManager->GetUIBase()->PushPromptClass(RecruitmentPanel.Get());

		if (UOfficeRecruitmentModalWidget* RecruitmentWidget = Cast<UOfficeRecruitmentModalWidget>(Widget))
		{
			int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
			RecruitmentWidget->SetBuildingIndex(BuildingIndex);
		}
	}
}

void UOfficeMainWidget::OnEquipmentGachaButtonClicked()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	const TSubclassOf<UUserWidget> PanelClass = TableMgr->GetWidgetClass(EWidgetType::EquipmentGachaPanel);
	if (!PanelClass) return;
	UIManager->GetUIBase()->PushPromptClass(PanelClass.Get(), [](UCommonActivatableWidget& Widget)
	{
		if (auto* Panel = Cast<UEquipmentGachaPanelWidget>(&Widget)) Panel->Open();
	});
}

void UOfficeMainWidget::OnCollectionBtnClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Collection button clicked"));

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> CollectionPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeCollectionPanel);
	if (CollectionPanel)
	{
		UIManager->GetUIBase()->PushBottomClass(CollectionPanel.Get());
	}
}

void UOfficeMainWidget::OnStartButtonClickedHandler(int32 ProjectIndex)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!StageMgr || !TableMgr) return;

	// 운영 모드 중에는 무시
	if (bIsInOperationMode)
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StartButton ignored - operation mode active"));
		return;
	}

	// 타이머 진행 중이면 무시 (동시 진행은 중단 불가)
	if (StageMgr->IsTimerRunning())
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StartButton ignored - timer is running"));
		return;
	}

	// 스테이지 미진행 → 선택된 프로젝트로 시작 (풀 시작)
	if (!StageMgr->IsStageInProgress())
	{
		if (ProjectIndex <= 0) ProjectIndex = 1;

		bool bSuccess = false;
		FProjectData ProjectData = TableMgr->ResolveProjectData(ProjectIndex, bSuccess);

		if (bSuccess)
		{
			StageMgr->StartNewStage(ProjectData, ProjectIndex, 1);
					UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StartButton - Starting project %d"), ProjectIndex);
		}
		return;
	}

	// idle 상태(CurrentStep==0)에서 도전하기 → 동시 타이머 시작
	// (SelectProject로 선택 후 / 세이브 복원 후 재개 / 포기 후 재도전 모두 여기서 처리)
	if (StageMgr->GetCurrentStep() == 0)
	{
		StageMgr->StartSimultaneousTimer(true);
			UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StartButton - Starting simultaneous timer for project %d"), ProjectIndex);
	}
}

// ========== Stage Progress UI (동시 진행) ==========

void UOfficeMainWidget::BindStageProgressDelegates()
{
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr) return;

	StageMgr->OnTimeUpdated.AddDynamic(this, &UOfficeMainWidget::OnTimerUpdated);
	StageMgr->OnDisciplineScoresUpdated.AddDynamic(this, &UOfficeMainWidget::OnDisciplineScoresUpdatedReceived);
	StageMgr->OnStageResultEffect.AddDynamic(this, &UOfficeMainWidget::OnStageResultEffectReceived);
	StageMgr->OnSimultaneousTimerEnd.AddDynamic(this, &UOfficeMainWidget::OnSimultaneousTimerEndReceived);
	StageMgr->OnProgressDataRestored.AddDynamic(this, &UOfficeMainWidget::OnProgressDataRestoredReceived);
	StageMgr->OnStepStartEffect.AddDynamic(this, &UOfficeMainWidget::OnStepStartEffectReceived);
	StageMgr->OnProjectSelected.AddDynamic(this, &UOfficeMainWidget::OnProjectSelectedReceived);
	StageMgr->OnScoreOrbRequested.AddDynamic(this, &UOfficeMainWidget::OnScoreOrbRequestedReceived);
	StageMgr->OnLifecycleChanged.AddDynamic(this, &UOfficeMainWidget::OnLifecycleChangedHandler);
	StageMgr->OnFeverChanged.AddDynamic(this, &UOfficeMainWidget::OnFeverChangedReceived);
	StageMgr->OnTierUnlocked.AddDynamic(this, &UOfficeMainWidget::OnCompanyTierUnlocked);

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StageProgressManager delegates bound"));
}

void UOfficeMainWidget::UnbindStageProgressDelegates()
{
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr) return;

	StageMgr->OnTimeUpdated.RemoveAll(this);
	StageMgr->OnDisciplineScoresUpdated.RemoveAll(this);
	StageMgr->OnStageResultEffect.RemoveAll(this);
	StageMgr->OnSimultaneousTimerEnd.RemoveAll(this);
	StageMgr->OnProgressDataRestored.RemoveAll(this);
	StageMgr->OnStepStartEffect.RemoveAll(this);
	StageMgr->OnProjectSelected.RemoveAll(this);
	StageMgr->OnScoreOrbRequested.RemoveAll(this);
	StageMgr->OnLifecycleChanged.RemoveAll(this);
	StageMgr->OnFeverChanged.RemoveAll(this);
	StageMgr->OnTierUnlocked.RemoveAll(this);

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StageProgressManager delegates unbound"));
}

void UOfficeMainWidget::OnTimerUpdated(float RemainingTime)
{
	const UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	const bool bComplete = RemainingTime <= 0.f;
	if (TimerRing) TimerRing->SetVisibility(ESlateVisibility::Collapsed);
	if (ProcessTimeLabel)
	{
		ProcessTimeLabel->SetText(NSLOCTEXT("Office", "TimeUntilComplete", "완료까지"));
		ProcessTimeLabel->SetVisibility(bComplete ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (ProcessTimeText)
	{
		ProcessTimeText->SetText(bComplete ? NSLOCTEXT("Office", "DevelopmentComplete", "개발 완료")
			: FText::Format(NSLOCTEXT("Office", "DevelopmentSeconds", "{0}초"), FText::AsNumber(FMath::CeilToInt(RemainingTime))));
	}
	const bool bPaused = StageMgr && (StageMgr->IsEventPaused() || StageMgr->IsBoostGamblePending());
	const bool bEmphasize = !bComplete && RemainingTime <= 3.f && !bPaused && !FMotionGate::IsReduced();
	if (!bEmphasize) ResetLastSpurtVisual();
	else bLastSpurtVisual = true;
}

void UOfficeMainWidget::UpdateDevelopmentEffectArea()
{
	// 막은 화면 전체에 칠하고 상단 바 쪽만 옅게 둔다 — 바 판 영역만 넘긴다
	if (!ScoreOrbContainer || !StripBG) return;
	const FGeometry CanvasGeo = ScoreOrbContainer->GetCanvasGeometry();
	const FGeometry StripGeo = StripBG->GetCachedGeometry();
	ScoreOrbContainer->SetDevelopmentStripRect(
		CanvasGeo.AbsoluteToLocal(StripGeo.LocalToAbsolute(FVector2D::ZeroVector)),
		CanvasGeo.AbsoluteToLocal(StripGeo.LocalToAbsolute(StripGeo.GetLocalSize())));
}

void UOfficeMainWidget::OnDisciplineScoresUpdatedReceived(
	const TArray<float>& Scores, const TArray<float>& Targets)
{
	// 마일스톤 판정은 착지 플러시(FlushStripStagePending)로 이관 — 바가 시각적으로 100%에 닿는 순간과 일치

	// 프로젝트 카드(은퇴) 갱신
	UpdateProjectCardProgress();

	// 스트립 스테이지 바 — 활성 직능 스텝별 라이브 채움 (착지 정산은 orb 도착이 flush)
	const int32 Count = FMath::Min(Scores.Num(), Targets.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		UpdateStripStageLive(i, Scores[i], Targets[i]);
	}
}

void UOfficeMainWidget::OnStageResultEffectReceived()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Stage result effect received"));

	// 팝업 지연 타이머가 설치되기 전에 월드 배속을 복원해야 0.9초가 실시간과 일치한다.
	ApplyDevSpeed(1.0f);
	OnTimerUpdated(0.f);

	// 라운드 종료 — 라스트 스퍼트 펄스 원복. 실패든 성공이든 반드시 돈다
	ResetLastSpurtVisual();

	// 수준 미달은 여기서 끝 — 축하 연출 없이 실패 화면으로 넘긴다
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (StageMgr && StageMgr->IsLaunchBlocked())
	{
		return;
	}

	// 출시 클라이맥스 한 방 (등급 비례) — "프로젝트 완료!" 텍스트 이펙트는 출시확인 패널과 중복이라 제거 (2026-07-11)
	PlayLaunchClimax();
}

void UOfficeMainWidget::OnSimultaneousTimerEndReceived()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Simultaneous timer end - showing LaunchConfirmWidget"));

	RefreshLeftSlotState();

	// 자동 LaunchConfirmWidget 팝업
	ShowLaunchConfirmModal();
}

void UOfficeMainWidget::OnProgressDataRestoredReceived()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Progress data restored from save, updating UI"));
	UpdateProjectCardProgress();
	RefreshLeftSlotState();
	const UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (StageMgr && StageMgr->GetLifecycle() == EProjectLifecycle::LaunchPending)
	{
		ShowLaunchConfirmModal();
	}
}

void UOfficeMainWidget::OnProjectSelectedReceived()
{
	if (ScoreOrbContainer) ScoreOrbContainer->ResetDevelopmentEffects();
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Project selected, updating card UI"));

	// 새 프로젝트 선택 → 다음 개발 대비 1회성 플래그 리셋 (StepStart 이펙트 없이 시작되는 경로 대비)
	for (bool& bReached : bCategoryMilestoneReached) { bReached = false; }
	bLateStageEscalated = false;
	ResetLastSpurtVisual();

	UpdateProjectCardProgress();
	RefreshLeftSlotState();
}

void UOfficeMainWidget::OnScoreOrbRequestedReceived(
	FVector WorldPos, int32 StepNumber, float Score, bool bIsCritical, int32 OrbCount, bool bPlaySpectacle)
{
	if (!ScoreOrbContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] ScoreOrb - Container is NULL!"));
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// 사무실 개발 = 구슬은 모니터 위에서 출발하고(책상 작업물 조각도 함께 난다), "+N" 숫자는 그대로 머리 위에서 뜬다
	FVector OrbWorldPos = WorldPos;
	UWorkstationDevFxComponent::HandleScoreOrb(GetWorld(), WorldPos, StepNumber, bIsCritical, bPlaySpectacle, OrbWorldPos);

	// OrbCanvas의 Geometry (모든 좌표를 이 Canvas 로컬로 변환)
	FGeometry CanvasGeo = ScoreOrbContainer->GetCanvasGeometry();

	// 1. 시작점: 3D 월드 → 뷰포트 위젯 로컬 → Canvas 로컬
	// (BrickFactory + InGameLayerWidget::SpawnBrickCollectAt 패턴)
	FVector2D ViewportPos;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC, WorldPos, ViewportPos, true))
	{
		return;
	}

	// 뷰포트 위젯 로컬 → Absolute → Canvas 로컬
	FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	FVector2D StartAbsolute = ViewportGeo.LocalToAbsolute(ViewportPos);
	FVector2D StartLocal = CanvasGeo.AbsoluteToLocal(StartAbsolute);

	// 2. 타겟: ProgressBar 위젯 Absolute 중앙 → Canvas 로컬. 은퇴 카드 대신 새 스트립 Stage{n}Bar 로 폴백.
	UWidget* TargetWidget = nullptr;
	if (UI_OfficeProjectCardCurrent)
	{
		TargetWidget = UI_OfficeProjectCardCurrent->GetStepWidget(StepNumber);
	}
	if (!TargetWidget)
	{
		TargetWidget = GetStripStepBar(StepNumber);
	}

	if (!TargetWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] ScoreOrb - TargetWidget is NULL for Step%d"), StepNumber);
		return;
	}

	const FGeometry& TargetGeo = TargetWidget->GetCachedGeometry();
	FVector2D TargetAbsCenter = TargetGeo.LocalToAbsolute(TargetGeo.GetLocalSize() * 0.5f);
	FVector2D TargetLocal = CanvasGeo.AbsoluteToLocal(TargetAbsCenter);

	// 3. 구슬 다중 스폰 (둘 다 Canvas 로컬 좌표) — 루트 느낌, 많을수록 좋아 기본 개수 유지
	// 정상 틱 = orb 1개 조용한 드립 / 크리 = 버스트
	FVector2D OrbStartLocal = StartLocal;
	FVector2D OrbViewportPos;
	if (!OrbWorldPos.Equals(WorldPos) && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, OrbWorldPos, OrbViewportPos, true))
	{
		OrbStartLocal = CanvasGeo.AbsoluteToLocal(ViewportGeo.LocalToAbsolute(OrbViewportPos));
	}
	ScoreOrbContainer->SpawnOrbBurst(OrbStartLocal, TargetLocal, StepNumber, bIsCritical, OrbCount);

	// 점수 숫자 "+N"도 같은 시작점에서 orb와 함께 (스크린스페이스, C++ 애니)
	ScoreOrbContainer->SpawnScoreNumber(StartLocal, static_cast<int64>(Score), StepNumber, bIsCritical);

	// 분할 목적지는 같은 크리 색을 유지하고 원본 기여에서만 크리 음을 재생한다.
	if (bIsCritical && bPlaySpectacle)
	{
		if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
		{
			SoundMgr->PlaySound(FName("Dev_Crit"));
		}
	}
}

// ===== 피버타임 연출 (Phase 2 Task 6) =====

void UOfficeMainWidget::OnFeverChangedReceived(bool bActive)
{
	// 화염 틴트 색은 트리거 시 1회만 지정(따뜻한 주황) — 알파 페이드는 TickFeverOverlay 가 RenderOpacity 로 처리.
	if (FeverScreenTint && bActive)
	{
		FeverScreenTint->SetColorAndOpacity(FLinearColor(1.0f, 0.32f, 0.08f, 1.0f));
	}
	FeverOverlayTargetAlpha = bActive ? 0.28f : 0.0f;

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(FeverOverlayTimerHandle))
		{
			World->GetTimerManager().SetTimer(FeverOverlayTimerHandle, this,
				&UOfficeMainWidget::TickFeverOverlay, 0.03f, true);
		}
	}

	if (bActive)
	{
		SpawnFeverBurst();
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
		{
			if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
			{
				SoundMgr->PlaySound(FName("Dev_Fever"));
			}
		}
	}
}

void UOfficeMainWidget::TickFeverOverlay()
{
	FeverOverlayAlpha = FMath::FInterpConstantTo(FeverOverlayAlpha, FeverOverlayTargetAlpha, 0.03f, 1.0f);

	if (FeverScreenTint)
	{
		FeverScreenTint->SetRenderOpacity(FeverOverlayAlpha);
		FeverScreenTint->SetVisibility(FeverOverlayAlpha > 0.01f
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 목표 도달 → 보간 타이머 정지
	if (FMath::IsNearlyEqual(FeverOverlayAlpha, FeverOverlayTargetAlpha, 0.005f))
	{
		FeverOverlayAlpha = FeverOverlayTargetAlpha;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FeverOverlayTimerHandle);
		}
	}
}

void UOfficeMainWidget::SpawnFeverBurst(int32 MaxWorkers)
{
	UWorld* World = GetWorld();
	UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	bool bHasRow = false;
	const FVFXTableRow RowData = TableMgr->GetVFXData(EVFXType::DevFever, bHasRow);
	UNiagaraSystem* FeverSystem = TableMgr->GetVFXAsset(EVFXType::DevFever);
	if (!FeverSystem) return;  // DT_VFX DevFever 미배치면 안전하게 스킵

	const FVector Offset = bHasRow ? RowData.LocationOffset : FVector(0.f, 0.f, 60.f);
	const float Scale = (bHasRow ? RowData.DefaultScale : 1.0f) * 1.5f;  // 레전드는 크게

	// 직원 최대 MaxWorkers 명에게 레전드 번개 — "전원 폭주" 한 방. 2단계 종료로 무한 루프 잔존 방지.
	int32 Count = 0;
	for (TActorIterator<AOfficeworker> It(World); It && Count < MaxWorkers; ++It)
	{
		AOfficeworker* Worker = *It;
		if (!Worker) continue;

		const FVector Pos = Worker->GetActorLocation() + Offset;
		UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, FeverSystem, Pos, FRotator::ZeroRotator, FVector(Scale), true);
		if (Comp)
		{
			TWeakObjectPtr<UNiagaraComponent> WeakComp(Comp);
			FTimerManager& TimerMgr = World->GetTimerManager();
			FTimerHandle StopHandle;
			TimerMgr.SetTimer(StopHandle, [WeakComp]()
			{
				if (WeakComp.IsValid()) { WeakComp->Deactivate(); }
			}, 1.8f, false);
			FTimerHandle KillHandle;
			TimerMgr.SetTimer(KillHandle, [WeakComp]()
			{
				if (WeakComp.IsValid()) { WeakComp->DestroyComponent(); }
			}, 2.6f, false);
		}
		++Count;
	}
}

// ===== 마일스톤 / 출시 클라이맥스 (Phase 3 Task 7/8) =====

void UOfficeMainWidget::PlayCategoryMilestone(int32 CategoryIndex)
{
	const UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr || !StageMgr->IsTimerRunning()) return;

	// 그 분야가 주 직능인 직원이 같은 순간 기뻐한다(칸 순서 = 스텝 순서)
	const FStageProgressData& MilestoneProgress = StageMgr->GetProgressData();
	if (MilestoneProgress.Steps.IsValidIndex(CategoryIndex))
	{
		// 책상 메모지가 먼저 — 몰입 가속을 거둔 뒤에 반응 연출이 기준 타자 속도를 읽는다
		UWorkstationDevFxComponent::NotifyDisciplineMilestone(GetWorld(), MilestoneProgress.Steps[CategoryIndex].DisciplineSlot, CategoryIndex + 1);
		UEmployeeReactionComponent::NotifyDisciplineMilestone(GetWorld(), MilestoneProgress.Steps[CategoryIndex].DisciplineSlot);
	}

	// 화면 가운데 배너 대신 목표에 닿은 칸 아래에서 탭이 빠져나온다 — 폭은 칸 게이지, 위치는 상단 바 아래 가장자리
	UWidget* Gauge = StripStageBars.IsValidIndex(CategoryIndex) ? StripStageBars[CategoryIndex] : nullptr;
	if (!Gauge) Gauge = StripStageCells.IsValidIndex(CategoryIndex) ? StripStageCells[CategoryIndex] : nullptr;
	if (ScoreOrbContainer && Gauge && StripBG)
	{
		const FGeometry CanvasGeo = ScoreOrbContainer->GetCanvasGeometry();
		const FGeometry GaugeGeo = Gauge->GetCachedGeometry();
		const FGeometry StripGeo = StripBG->GetCachedGeometry();
		const FVector2D GaugeMin = CanvasGeo.AbsoluteToLocal(GaugeGeo.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D GaugeMax = CanvasGeo.AbsoluteToLocal(GaugeGeo.LocalToAbsolute(GaugeGeo.GetLocalSize()));
		const float StripBottom = CanvasGeo.AbsoluteToLocal(StripGeo.LocalToAbsolute(StripGeo.GetLocalSize())).Y;
		ScoreOrbContainer->PlayDevelopmentGoalTab(CategoryIndex, FVector2D(GaugeMin.X, StripBottom), GaugeMax.X - GaugeMin.X);
	}

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlaySound(FName("Dev_Milestone"));
		}
	}
}

void UOfficeMainWidget::PlayLaunchClimax(bool bManufacturingSuccess)
{
	UWorld* World = GetWorld();
	UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
	if (!GI) return;

	UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>();
	const EQualityGrade Grade = !bManufacturingSuccess && StageMgr ? StageMgr->GetCurrentQualityGrade() : EQualityGrade::Max;

	constexpr float Intensity = 1.0f;

	// 1) 좋은 결과(B 이상)만 전원 기립 환호 — 그 외는 매니저의 CheerSitting 을 그대로 둠(전 등급 기립 방지 + 매니저 호출 안 죽임)
	const bool bGoodResult = bManufacturingSuccess || static_cast<uint8>(Grade) >= static_cast<uint8>(EQualityGrade::B);
	if (bGoodResult)
	{
		if (StageMgr && !bManufacturingSuccess) StageMgr->NotifyEmployeesCheerStandUp();
		// 의자 밀림·회전 + 두 번 폴짝·웃음 + 메모지 팔랑, 카메라는 0.5초에 4% 당김(흔들림 없음 — 결과 창이 닫히면 원복)
		UWorkstationDevFxComponent::NotifyLaunchCheer(World);
		if (!USettingsManagerSubsystem::IsReduceMotion(this))
		{
			if (AOfficeCameraPawn* Camera = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				Camera->PlayPresentationPullIn(0.04f, 0.5f);
			}
		}
	}

	// 「개발 종료!」 판은 두지 않는다 — 0.9초 뒤 결과 창 제목이 같은 말을 하므로 축하는 막과 불꽃만 맡는다
	UpdateDevelopmentEffectArea();
	if (ScoreOrbContainer) ScoreOrbContainer->PlayDevelopmentFireworks(Intensity);
	// 불꽃의 가는 밝은 꼬리가 보이도록 기존 전체 화면 흰빛을 끈다.
	FeverOverlayAlpha = FeverOverlayTargetAlpha = 0.f;
	World->GetTimerManager().ClearTimer(FeverOverlayTimerHandle);
	if (FeverScreenTint) FeverScreenTint->SetVisibility(ESlateVisibility::Collapsed);

	// 4) 출시 사운드
	if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
	{
		SoundMgr->PlaySound(FName("Dev_LaunchClimax"));
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Completion climax (manufacturing=%d, intensity %.1f)"),
		bManufacturingSuccess, Intensity);
}

void UOfficeMainWidget::OnStepStartEffectReceived()
{
	if (ScoreOrbContainer) ScoreOrbContainer->ResetDevelopmentEffects();
	// 새 개발 시작 — 마일스톤/escalate 1회성 플래그 리셋
	for (bool& bReached : bCategoryMilestoneReached) { bReached = false; }
	bLateStageEscalated = false;
	ResetLastSpurtVisual();

	// 책상 — 지난 판을 거두고 모니터가 화면 왼쪽부터 차례로 켜진다
	if (AOfficeCameraPawn* Camera = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Camera->ReleasePresentationPullIn(0.3f);
	}
	UWorkstationDevFxComponent::NotifyDevelopmentStart(GetWorld());

	ShowStepStartEffect();

	// NameBar를 모드별 "개발 중" 상태로 전환 + 버튼 숨김
	RefreshProjectNameBar();
}


void UOfficeMainWidget::UpdateProjectCardProgress()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();

	if (!StageMgr || !TableMgr) return;

	const FStageProgressData& Progress = StageMgr->GetProgressData();
	if (Progress.ProjectID > 0)
	{
		// NameBar는 상태 기반 단일 갱신 경로 사용
		if (!StageMgr->IsTimerRunning() && !bIsInOperationMode)
		{
			RefreshProjectNameBar();
		}

		if (UI_OfficeProjectCardCurrent)
		{
			// 현재 빌딩 CompanyType(GI) 우선. Progress.CompanyType은 SaveGame 직렬화 값이라 stale일 수 있음.
			bool bSuccess = false;
			FProjectData ProjectData = TableMgr->ResolveProjectData(Progress.ProjectID, bSuccess);

			if (bSuccess)
			{
				UI_OfficeProjectCardCurrent->SetProjectData(Progress.ProjectID);
				UI_OfficeProjectCardCurrent->SetTitle(FText::FromString(TEXT("현재 프로젝트")));

				// 보드 선택 시 트레이트/숙련도 배율이 적용된 Progress.Steps 값을 단일 진실로 사용
				int32 Step1Current = 0, Step1Max = ProjectData.RequiredScore_Step1;
				int32 Step2Current = 0, Step2Max = ProjectData.RequiredScore_Step2;
				int32 Step3Current = 0, Step3Max = ProjectData.RequiredScore_Step3;
				int32 Step4Current = 0, Step4Max = ProjectData.RequiredScore_Step4;

				if (Progress.Steps.Num() >= 3)
				{
					Step1Current = static_cast<int32>(Progress.Steps[0].AcquiredScore);
					Step1Max     = static_cast<int32>(Progress.Steps[0].TargetScore);
					Step2Current = static_cast<int32>(Progress.Steps[1].AcquiredScore);
					Step2Max     = static_cast<int32>(Progress.Steps[1].TargetScore);
					Step3Current = static_cast<int32>(Progress.Steps[2].AcquiredScore);
					Step3Max     = static_cast<int32>(Progress.Steps[2].TargetScore);
				}
				if (Progress.Steps.Num() >= 4)
				{
					Step4Current = static_cast<int32>(Progress.Steps[3].AcquiredScore);
					Step4Max     = static_cast<int32>(Progress.Steps[3].TargetScore);
				}

				UI_OfficeProjectCardCurrent->UpdateStepProgress(
					Step1Current, Step1Max,
					Step2Current, Step2Max,
					Step3Current, Step3Max,
					Step4Current, Step4Max
				);
			}
		}
	}
	else
	{
		RefreshProjectNameBar();

		if (UI_OfficeProjectCardCurrent)
		{
			bool bSuccess = false;
			FProjectData ProjectData = TableMgr->ResolveProjectData(1, bSuccess);

			if (bSuccess)
			{
				UI_OfficeProjectCardCurrent->SetProjectData(1);
				// 대기 상태: 타이틀 리셋 (이전 클라이언트명 잔존 방지)
				UI_OfficeProjectCardCurrent->SetTitle(FText::FromString(TEXT("현재 프로젝트")));
				UI_OfficeProjectCardCurrent->UpdateStepProgress(
					0, ProjectData.RequiredScore_Step1,
					0, ProjectData.RequiredScore_Step2,
					0, ProjectData.RequiredScore_Step3,
					0, ProjectData.RequiredScore_Step4
				);
			}
		}
	}
}

// ========== Operation UI ==========

void UOfficeMainWidget::BindOperationDelegates()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UProjectOperationManager* OperationMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (OperationMgr)
	{
		OperationMgr->OnOperationStarted.AddDynamic(this, &UOfficeMainWidget::OnOperationStartedReceived);
		OperationMgr->OnOperationUpdated.AddDynamic(this, &UOfficeMainWidget::OnOperationUpdatedReceived);
		OperationMgr->OnOperationCompleted.AddDynamic(this, &UOfficeMainWidget::OnOperationCompletedReceived);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Operation delegates bound"));
}

void UOfficeMainWidget::UnbindOperationDelegates()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UProjectOperationManager* OperationMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (OperationMgr)
	{
		OperationMgr->OnOperationStarted.RemoveAll(this);
		OperationMgr->OnOperationUpdated.RemoveAll(this);
		OperationMgr->OnOperationCompleted.RemoveAll(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Operation delegates unbound"));
}

void UOfficeMainWidget::OnOperationStartedReceived(int32 BuildingID)
{
	if (BuildingID != GetCurrentBuildingIDAsInt()) return;

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Operation started for building %d"), BuildingID);

	bIsInOperationMode = true;

	// CardSwitcher를 Operation 모드로 전환하며 실제 값으로 초기화 (placeholder "0, 0, -" 방지)
	if (FOperationData* CurrentOp = GetCurrentBuildingOperation())
	{
		if (UI_OfficeProjectCardCurrent)
		{
			UI_OfficeProjectCardCurrent->SwitchToOperationMode(
				CurrentOp->ActualRevenuePerSecond,
				static_cast<int64>(CurrentOp->TotalRevenueEarned),
				QualityGradeToAlphabetString(CurrentOp->QualityGrade));
		}
		UpdateOperationUI(*CurrentOp);
	}
	else
	{
		// fallback: placeholder (이후 OnOperationUpdated에서 실제값으로 보정됨)
		UpdateUIForOperationMode(true);
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] OnOperationStarted: CurrentOp null, using placeholder"));
	}

	RefreshLeftSlotState();
}

void UOfficeMainWidget::OnOperationUpdatedReceived(int32 BuildingID, const FOperationData& Data)
{
	int32 CurrentBuildingID = GetCurrentBuildingIDAsInt();
	if (BuildingID != CurrentBuildingID) return;

	const bool bIsOperatingState = (Data.State == EOperationState::Operating || Data.State == EOperationState::Paused);

	// 운영 모드가 아닌데 운영 중인 경우 (늦은 복원) — 일관된 경로로 초기화
	if (!bIsInOperationMode && bIsOperatingState)
	{
		bIsInOperationMode = true;
		if (UI_OfficeProjectCardCurrent)
		{
			UI_OfficeProjectCardCurrent->SwitchToOperationMode(
				Data.ActualRevenuePerSecond,
				static_cast<int64>(Data.TotalRevenueEarned),
				QualityGradeToAlphabetString(Data.QualityGrade));
		}
			RefreshProjectNameBar();
		RefreshLeftSlotState();
	}

	// State가 운영 중이면 매 틱 UI 갱신 (bIsInOperationMode 누락 대비)
	if (bIsOperatingState)
	{
		UpdateOperationUI(Data);
		if (SalesCurve)
		{
			SalesCurve->PushSample(Data);  // 라이브 감쇠 곡선 피드 (새 운영은 위젯이 자동 리셋)
		}
	}
}

void UOfficeMainWidget::OnOperationCompletedReceived(int32 BuildingID, const FOperationData& Data)
{
	if (BuildingID != GetCurrentBuildingIDAsInt()) return;

	// 운영 종료 = 남은 반응은 버린다 (스펙 §4.1) — 레벨 전환은 서브시스템 Deinitialize 가 맡는다
	if (ULaunchReactionSubsystem* Reactions = GetWorld() ? GetWorld()->GetSubsystem<ULaunchReactionSubsystem>() : nullptr)
	{
		Reactions->CancelDrip(BuildingID);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Operation completed for building %d, Project: %s"),
		BuildingID, *Data.ProjectName);

	if (UI_OfficeProjectCardCurrent)
	{
		UI_OfficeProjectCardCurrent->SwitchToStageMode();
	}

	ShowProjectReportModal(Data);

	// 결산서가 이미 「운영 완료」를 말한다 ― 토스트는 결산서 위에 겹치므로 결산서를 못 띄웠을 때만 대신 알린다
	if (!ActiveReportWidget)
	{
		UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
		if (UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			FText NotificationMessage = NSLOCTEXT("Operation", "OperationCompleteNotify", "프로젝트 운영이 완료되었습니다");
			UIManager->ShowNotification(NotificationMessage, 3.0f, ENotificationType::Success);
		}
	}
}

void UOfficeMainWidget::OnSpeedToggleClickedHandler()
{
	const UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr || !StageMgr->IsTimerRunning()) return;
	ApplyDevSpeed(DevSpeedMultiplier > 1.5f ? 1.0f : 2.0f);
}

void UOfficeMainWidget::ApplyDevSpeed(float NewSpeed)
{
	DevSpeedMultiplier = NewSpeed;

	// 운영 매니저는 FTSTicker(실시간)라 여기 영향을 안 받는다 — 배속으로 수익이 늘지 않는 건 구조가 보장한다
	if (UWorld* SpeedWorld = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(SpeedWorld, NewSpeed);
	}
	if (StepStartBannerWidget)
	{
		StepStartBannerWidget->SetPresentationSpeed(NewSpeed);
	}

	RefreshSpeedIcon();
}

// 삼각형 1개(▶) ↔ 2개(▶▶) — 현재 배속을 그대로 반영. 위젯 재활성화 때도 쓰인다.
void UOfficeMainWidget::RefreshSpeedIcon()
{
	if (!StripSpeedIcon) { return; }
	if (UTexture2D* Icon = (DevSpeedMultiplier > 1.5f) ? SpeedIcon_x2.Get() : SpeedIcon_x1.Get())
	{
		StripSpeedIcon->SetBrushFromTexture(Icon, /*bMatchSize*/false);
	}
}

void UOfficeMainWidget::OnStopOperationClickedHandler()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StopOperation button clicked"));

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;

	// 운영종료 = 남은 수익 포기(파괴적) → 확인 다이얼로그. UI 접근 실패 시 폴백으로 즉시 종료.
	TSubclassOf<UUserWidget> Cls = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::ConfirmCancel) : nullptr;
	UConfirmCancelWidget* Confirm = (Cls && UIManager && UIManager->GetUIBase())
		? Cast<UConfirmCancelWidget>(UIManager->GetUIBase()->PushPromptClass(Cls.Get()))
		: nullptr;

	if (!Confirm)
	{
		if (GI)
		{
			if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
			{
				OpMgr->EndAllOperationsByBuilding(GetCurrentBuildingIDAsInt());
			}
		}
		return;
	}

	// 프롬프트 스택은 위젯 풀 재사용 — 이전 소비자(부지 인수/슬롯 개방 등)의 잔류 바인딩 해제 후 소유
	Confirm->OnConfirm.Clear();
	Confirm->OnCancel.Clear();

	Confirm->SetTitle(FText::FromString(TEXT("운영 종료")));
	Confirm->SetMessage(FText::FromString(TEXT("남은 운영 수익을 포기하고 지금 종료할까요?")));
	Confirm->SetConfirmButtonText(FText::FromString(TEXT("종료")));
	Confirm->SetCancelButtonText(FText::FromString(TEXT("취소")));

	// 확인 시에만 실제 종료 (취소는 다이얼로그만 닫힘). 위젯 수명 안전을 위해 WeakThis.
	TWeakObjectPtr<UOfficeMainWidget> WeakThis(this);
	Confirm->OnConfirm.AddLambda([WeakThis]()
	{
		UOfficeMainWidget* Self = WeakThis.Get();
		if (!Self || !Self->GetWorld()) return;
		UCGGameInstance* GI2 = Cast<UCGGameInstance>(Self->GetWorld()->GetGameInstance());
		if (!GI2) return;
		if (UProjectOperationManager* OpMgr = GI2->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->EndAllOperationsByBuilding(Self->GetCurrentBuildingIDAsInt());
		}
	});
}

void UOfficeMainWidget::UpdateUIForOperationMode(bool bOperating)
{
	if (UI_OfficeProjectCardCurrent)
	{
		if (bOperating)
		{
			UI_OfficeProjectCardCurrent->SwitchToOperationMode(0.0f, 0, TEXT("-"));
		}
		else
		{
			UI_OfficeProjectCardCurrent->SwitchToStageMode();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] UpdateUIForOperationMode: %s"),
		bOperating ? TEXT("Operation") : TEXT("Stage"));
}

void UOfficeMainWidget::UpdateOperationUI(const FOperationData& Data)
{
	if (ProcessTimeText)
	{
		FString TimeString = Data.GetRemainingTimeString();
		ProcessTimeText->SetText(FText::FromString(TimeString));
	}

	UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>();
	float StoredRevenue = OfficeMgr ? OfficeMgr->GetStoredRevenue() : 0.0f;

	// 프로젝트 카드 운영 정보 갱신
	if (UI_OfficeProjectCardCurrent)
	{
		UI_OfficeProjectCardCurrent->UpdateOperationInfo(
			Data.ActualRevenuePerSecond,
			static_cast<int64>(Data.TotalRevenueEarned),
			QualityGradeToAlphabetString(Data.QualityGrade));
	}

	// 신규 스트립 수익 메트릭(+등급 배지) — 운영 중 실시간
	UpdateStripOperationMetrics(Data);
}

FOperationData* UOfficeMainWidget::GetCurrentBuildingOperation()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return nullptr;

	UProjectOperationManager* OperationMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OperationMgr) return nullptr;

	int32 BuildingID = GetCurrentBuildingIDAsInt();
	return OperationMgr->GetOperationByBuildingID(BuildingID);
}

int32 UOfficeMainWidget::GetCurrentBuildingIDAsInt() const
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return INDEX_NONE;
	return GI->GetCurrentManagedBuildingIndex();
}

// ========== Step 시작 이펙트 ==========

void UOfficeMainWidget::ShowStepStartEffect()
{
	RemoveStepStartEffect();
	UWorld* EffectWorld = GetWorld();
	UCGGameInstance* GI = GetGameInstance<UCGGameInstance>();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!EffectWorld || !TableMgr) return;

	const TSubclassOf<UUserWidget> BannerClass = TableMgr->GetWidgetClass(EWidgetType::DevelopmentStart);
	if (!BannerClass || !BannerClass->IsChildOf(UDevelopmentBannerWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] DevelopmentStart banner is missing or incompatible in DT_WidgetClass."));
		return;
	}
	StepStartBannerWidget = CreateWidget<UDevelopmentBannerWidget>(EffectWorld, BannerClass);
	if (!StepStartBannerWidget) return;

	StepStartBannerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	StepStartBannerWidget->SetPresentationSpeed(DevSpeedMultiplier);
	StepStartBannerWidget->AddToViewport(1000);
	StepStartBannerWidget->StartPresentation(USettingsManagerSubsystem::IsReduceMotion(this));
	const float Duration = StepStartBannerWidget->GetPresentationDuration();
	if (Duration <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] DevelopmentStart Presentation animation is not bound."));
		RemoveStepStartEffect();
		return;
	}
	EffectWorld->GetTimerManager().SetTimer(StepEffectTimerHandle,
		this, &UOfficeMainWidget::RemoveStepStartEffect, Duration, false);
}

void UOfficeMainWidget::RemoveStepStartEffect()
{
	if (UWorld* EffectWorld = GetWorld())
	{
		EffectWorld->GetTimerManager().ClearTimer(StepEffectTimerHandle);
	}
	if (StepStartBannerWidget)
	{
		StepStartBannerWidget->RemoveFromParent();
		StepStartBannerWidget = nullptr;
	}

	if (TimerRing)
	{
		// 색까지 되돌린다 — percent 만 리셋하면 직전 라운드가 레드로 끝난 경우
		// 첫 타이머 틱이 올 때까지 "빨간 꽉 찬 링" 이 남는다.
		TimerRing->SetPercent(1.0f);
		TimerRing->SetFillColor(TimerRingGold);
	}

	const UOfficeStageProgressManager* TimerMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (TimerMgr) OnTimerUpdated(TimerMgr->GetProgressData().RemainingTime);
}

// ========== 출시 확인 모달 ==========

void UOfficeMainWidget::ShowLaunchConfirmModal()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;
	if (Cast<ULaunchConfirmWidget>(UIManager->GetUIBase()->GetActivePromptWidget())) return;

	TSubclassOf<UUserWidget> LaunchConfirmClass = TableMgr->GetWidgetClass(EWidgetType::LaunchConfirm);
	if (!LaunchConfirmClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeMainWidget] LaunchConfirm widget not registered in DataTable!"));
		return;
	}

	UCommonActivatableWidget* PushedWidget = UIManager->GetUIBase()->PushPromptClass(LaunchConfirmClass.Get());
	// 결과 창 바탕이 페이드인하는 동안 개발 종료 막을 같이 거둬 화면이 두 번 밝아지지 않게 한다
	if (ScoreOrbContainer) ScoreOrbContainer->FadeDevelopmentDim();
	ULaunchConfirmWidget* LaunchWidget = Cast<ULaunchConfirmWidget>(PushedWidget);
	if (LaunchWidget)
	{
		LaunchWidget->OnManufacturingResearchResultClosed.RemoveAll(this);
		UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
		if (StageMgr)
		{
			StageMgr->PrepareLaunchLoot();
			const FStageProgressData& StageData = StageMgr->GetProgressData();
			LaunchWidget->SetPreviewData(StageData, StageData.ProjectNumber);
		}

		// 프롬프트 스택 풀 재사용 대비 — 같은 인스턴스가 재push 되면 바인딩이 누적된다
		LaunchWidget->OnLaunchConfirmed.AddUniqueDynamic(this, &UOfficeMainWidget::OnLaunchConfirmedHandler);
		LaunchWidget->OnLaunchCancelled.AddUniqueDynamic(this, &UOfficeMainWidget::OnLaunchCancelledHandler);
		LaunchWidget->OnLaunchRetryRequested.AddUniqueDynamic(this, &UOfficeMainWidget::OnLaunchRetryRequestedHandler);
	}
}

void UOfficeMainWidget::OnLaunchConfirmedHandler()
{
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr) { return; }

	const EProjectLifecycle LifecycleBefore = StageMgr->GetLifecycle();
	UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
	if (MissionMgr)
	{
		MissionMgr->SuspendGuideUntilLaunchRewardConfirmed();
	}

	StageMgr->StartLaunch();
	if (MissionMgr) { MissionMgr->ResumeGuideAfterLaunchRewardConfirmed(); }
	UpdateProjectCardProgress();
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Launch confirmed"));

	// 확정 2회 탭은 FSM에서 무시한다. 반응도 실제 출시 후 한 번만 시작한다.
	if (StageMgr->GetLifecycle() == LifecycleBefore)
	{
		return;
	}
	if (StageMgr->GetLifecycle() == EProjectLifecycle::Operating)
	{
		if (ULaunchReactionSubsystem* Reactions = GetWorld()->GetSubsystem<ULaunchReactionSubsystem>())
		{
			Reactions->BeginDrip(GetCurrentBuildingIDAsInt());
		}
	}
}

void UOfficeMainWidget::OnLaunchCancelledHandler()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Launch dismissed - returning to idle"));

	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (StageMgr)
	{
		StageMgr->HandleLaunchDismissed();
	}

	UpdateProjectCardProgress();
}

void UOfficeMainWidget::OnLaunchRetryRequestedHandler()
{
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] 컨티뉴 실패 - StageMgr 없음, 결과 모달 재표시"));
		ShowLaunchConfirmModal();
		return;
	}

	// 차감은 서버가 하고 타이머 재개는 성공 응답 뒤다 — 실패면 상태가 그대로이므로 결과 모달을 되살린다
	StageMgr->RequestRetryDevelopment(
		FOnCloudEconomyOpDone::CreateWeakLambda(this, [this](bool bOk, FName Code)
	{
		if (!bOk)
		{
			// 게이트/잔액/서버 미충족 — 상태를 바꾸지 않았으므로 결과 모달이 다시 뜨도록 되돌린다
			UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] 컨티뉴 실패 - 결과 모달 재표시. Code=%s"), *Code.ToString());

			// 모달만 되살리면 왜 안 됐는지 알 길이 없다. 원인은 코드로 갈린다 —
			// 잔액 부족만 부족액 토스트, 잠금/네트워크/서버는 3상태·DT 문구(1-B-4c)
			UOfficeStageProgressManager* Mgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
			if (Mgr && Mgr->CanRetryDevelopment())
			{
				if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
				{
					UIMgr->NotifyCloudOpFailure(Code, EResourceType::Diamond, Mgr->GetRetryDiamondCost());
				}
			}

			ShowLaunchConfirmModal();
			return;
		}

		RefreshLeftSlotState();
		UpdateProjectCardProgress();
		UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] 컨티뉴 - 개발 재개"));
	}));
}

// ========== 프로젝트 결산서 모달 ==========

void UOfficeMainWidget::ShowProjectReportModal(const FOperationData& CompletedData)
{
	// 이미 결산서가 떠 있으면 중복 푸시 차단
	if (ActiveReportWidget)
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] ShowProjectReportModal skipped - already showing"));
		return;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> ReportClass = TableMgr->GetWidgetClass(EWidgetType::ProjectReport);
	if (!ReportClass) return;

	UCommonActivatableWidget* PushedWidget = UIManager->GetUIBase()->PushPromptClass(ReportClass.Get());
	UProjectReportWidget* ReportWidget = Cast<UProjectReportWidget>(PushedWidget);
	if (ReportWidget)
	{
		ActiveReportWidget = ReportWidget;
		FProjectReportData ReportData;
		ReportData.ProjectID = CompletedData.ProjectID;
		ReportData.ProjectNumber = CompletedData.ProjectNumber;
		ReportData.StageNumber = CompletedData.StageNumber;
		ReportData.ProjectName = CompletedData.ProjectName;
		ReportData.BuildingID = CompletedData.BuildingID;
		ReportData.QualityScore = CompletedData.QualityScore;
		ReportData.QualityGrade = CompletedData.QualityGrade;
		ReportData.DisciplineScores = CompletedData.DisciplineScores;
		ReportData.DisciplineTargets = CompletedData.DisciplineTargets;
		ReportData.DisciplineSlots = CompletedData.DisciplineSlots;
		ReportData.TotalRevenueEarned = CompletedData.TotalRevenueEarned;
		ReportData.TotalOperationTime = CompletedData.TotalOperationTime;

		// MarketCapGained/CompanyType 은 FOperationData 에 없음 — pending report 에서 보완 (GenerateReport 가 설정).
		// CompanyType 이 None 으로 남으면 분야 축 라벨(GetDisciplineDisplayName)이 통째로 빈다.
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			if (const FProjectReportData* Pending = OpMgr->GetPendingReport(CompletedData.BuildingID))
			{
				ReportData.MarketCapGained = Pending->MarketCapGained;
				ReportData.CompanyType = Pending->CompanyType;
			}
		}

		ReportWidget->SetReportData(ReportData);
		ReportWidget->OnReportClosed.AddDynamic(this, &UOfficeMainWidget::OnReportClosedHandler);
	}
}

void UOfficeMainWidget::ShowPendingReportModal()
{
	if (ActiveReportWidget)
	{
		UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] ShowPendingReportModal skipped - already showing"));
		return;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	const int32 BuildingID = GetCurrentBuildingIDAsInt();

	UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>();
	if (!OpMgr) return;

	const FProjectReportData* Pending = OpMgr->GetPendingReport(BuildingID);
	if (!Pending) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> ReportClass = TableMgr->GetWidgetClass(EWidgetType::ProjectReport);
	if (!ReportClass) return;

	// 아래 ConsumeReport 가 원본을 지우므로 값으로 복사해 둔다
	const FProjectReportData Report = *Pending;

	UCommonActivatableWidget* PushedWidget = UIManager->GetUIBase()->PushPromptClass(ReportClass.Get());
	UProjectReportWidget* ReportWidget = Cast<UProjectReportWidget>(PushedWidget);
	if (ReportWidget)
	{
		ActiveReportWidget = ReportWidget;
		ReportWidget->SetReportData(Report);
		ReportWidget->OnReportClosed.AddDynamic(this, &UOfficeMainWidget::OnReportClosedHandler);

		// modal 표시 즉시 PendingReport 소비 — 어떤 경로로 닫혀도 재출현 차단
		OpMgr->ConsumeReport(BuildingID);
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}
}

void UOfficeMainWidget::OnReportClosedHandler()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Report closed - returning to idle"));

	// 활성 결산서 참조 해제
	if (ActiveReportWidget)
	{
		ActiveReportWidget->OnReportClosed.RemoveAll(this);
		ActiveReportWidget = nullptr;
	}

	// FSM 인텐트 — ReportPending → Idle (UI는 OnLifecycleChanged로 자동 갱신)
	if (UOfficeStageProgressManager* SM = GetWorld()->GetSubsystem<UOfficeStageProgressManager>())
	{
		SM->NotifyReportClosed();
	}

	// PendingReport 플래그 해제 + 디스크 저장 (재진입 시 재등장 방지)
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (GI)
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->ConsumeReport(GetCurrentBuildingIDAsInt());
		}
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			SaveMgr->SaveGameData();
		}
	}

	bIsInOperationMode = false;
	UpdateUIForOperationMode(false);
	RefreshLeftSlotState();
	UpdateProjectCardProgress();

	if (TimerRing)
	{
		TimerRing->SetPercent(0.0f);
	}
}

// ========== 프로젝트 보드 이벤트 시스템 ==========

void UOfficeMainWidget::BindBoardEventDelegates()
{
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr) return;

	StageMgr->OnProjectEventTriggered.AddDynamic(this, &UOfficeMainWidget::OnProjectEventTriggeredReceived);
	StageMgr->OnProjectModeChanged.AddDynamic(this, &UOfficeMainWidget::OnProjectModeChangedReceived);
	StageMgr->OnBoostGambleRequested.AddDynamic(this, &UOfficeMainWidget::OnBoostGambleRequestedReceived);

	// 직원 상태 토스트(지침/졸음) → 이벤트 레일. EmployeeManager 는 GameInstance 서브시스템이라 GI 경유.
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>())
		{
			EmpMgr->OnEmployeeStatusToast.AddDynamic(this, &UOfficeMainWidget::OnEmployeeStatusToastReceived);
			EmpMgr->OnEmployeeStatusToastCleared.AddDynamic(this, &UOfficeMainWidget::OnEmployeeStatusToastClearedReceived);
		}
	}

	// 트렌드 로테이션 = 상단 밴드 소관(OfficeLayer 가 직접 구독) — 여기선 구독 안 함.

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Board/Event delegates bound"));
}

void UOfficeMainWidget::UnbindBoardEventDelegates()
{
	// 직원 상태 토스트 언바인드 — StageMgr 유무와 독립(GameInstance 서브시스템)이라 조기 return 전에 먼저.
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>())
		{
			EmpMgr->OnEmployeeStatusToast.RemoveAll(this);
			EmpMgr->OnEmployeeStatusToastCleared.RemoveAll(this);
		}
	}

	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr) return;

	StageMgr->OnProjectEventTriggered.RemoveAll(this);
	StageMgr->OnProjectModeChanged.RemoveAll(this);
	StageMgr->OnBoostGambleRequested.RemoveAll(this);

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Board/Event delegates unbound"));
}

void UOfficeMainWidget::OnProjectEventTriggeredReceived(const FProjectEventData& EventData)
{
	// 선택 결과 팝업이 선택된 Choice 효과를 표시할 수 있도록 보관 (카드에 뿌린 것과 동일 데이터)
	PendingEventData = EventData;

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIMgr || !UIMgr->GetUIBase()) return;

	// 신규: 3선택지 EventChoicePanel
	TSubclassOf<UUserWidget> PanelClassRaw = TableMgr->GetWidgetClass(EWidgetType::EventChoicePanel);
	if (!PanelClassRaw) return;

	TSubclassOf<UCommonActivatableWidget> PanelClass = *PanelClassRaw;
	if (!PanelClass) return;

	UCommonActivatableWidget* Pushed = UIMgr->GetUIBase()->PushPromptClass(PanelClass);
	UEventChoicePanelWidget* Panel = Cast<UEventChoicePanelWidget>(Pushed);
	if (!Panel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] EventChoicePanel cast failed"));
		return;
	}

	Panel->SetEventData(EventData);
	Panel->OnChoiceMade.AddDynamic(this, &UOfficeMainWidget::OnEventChoiceMadeHandler);

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] EventChoicePanel shown: %s"), *EventData.EventTitle.ToString());
}

void UOfficeMainWidget::OnBoostGambleRequestedReceived()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	// 레일은 OfficeMain 자기 소유(개발 스트립 바로 아래 StripRailColumn) — OfficeLayer 경유/좌표추적 폐기.
	UOfficeEventRailWidget* Rail = EventRail;

	TSubclassOf<UUserWidget> BoostClassRaw = TableMgr->GetWidgetClass(EWidgetType::BoostGamble);

	UBoostGambleWidget* Card = (Rail && BoostClassRaw)
		? CreateWidget<UBoostGambleWidget>(Rail, BoostClassRaw.Get())
		: nullptr;

	if (!Card)
	{
		// 카드를 못 띄우면 매니저의 pending 이 남아 개발 타이머가 멈춘 채 방치된다 — 즉시 안전으로 정산해 진행 재개
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] DevEvent 카드 생성 실패(레일=%d, 클래스=%d) — 안전 처리로 정산"),
			Rail != nullptr, BoostClassRaw != nullptr);
		if (UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr)
		{
			StageMgr->ResolveBoostGamble(false);
		}
		return;
	}

	// 카드가 자체 카운트다운을 돌고 버튼/타임아웃 시 StageMgr->ResolveBoostGamble 를 직접 호출(자체 구동).
	// 스택 모달이 아니므로 해결 후 스스로 사라지려면 소유 레일의 제거 함수를 물려줘야 한다.
	Card->OnRailRemoveRequested.BindUObject(Rail, &UOfficeEventRailWidget::RemoveEventCard, static_cast<UUserWidget*>(Card));
	Rail->AddEventCard(Card);

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] DevEvent 카드 레일 추가"));
}

void UOfficeMainWidget::OnEmployeeStatusToastReceived(int32 EmployeeID, const FText& Message, float Duration)
{
	// 겜블 카드와 같은 우측 레일에 자동만료 상태 토스트(웜앰버). 레일 없으면 조용히 무시(Optional).
	// Duration = 손댈 수 있는 구간 길이(매니저가 DA 에서 계산) — 상황이 끝나면 알림도 끝나게.
	if (EventRail)
	{
		EventRail->AddStatusToast(Message, Duration, FLinearColor(1.0f, 0.867f, 0.478f, 1.0f), EmployeeID);
	}
}

void UOfficeMainWidget::OnEmployeeStatusToastClearedReceived(int32 EmployeeID)
{
	if (EventRail)
	{
		EventRail->DismissStatusToast(EmployeeID);
	}
}

void UOfficeMainWidget::OnEventChoiceMadeHandler(int32 ChoiceIndex)
{
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!StageMgr)
	{
		return;
	}

	// 결과 팝업/Orb 표시용으로 선택된 Choice 캡처 (효과 적용 전 — 카드에 뿌린 것과 동일 데이터)
	const bool bHasChoice = PendingEventData.Choices.IsValidIndex(ChoiceIndex);
	const FProjectEventChoice Choice = bHasChoice ? PendingEventData.Choices[ChoiceIndex] : FProjectEventChoice();

	// 선택지 적용 전 각 Step 점수 캡처 (Orb 비행용 — 실제 적용 델타 기준)
	const FStageProgressData& ProgBefore = StageMgr->GetProgressData();
	float BeforePlan = 0.0f, BeforeDev = 0.0f, BeforeQA = 0.0f;
	if (ProgBefore.Steps.Num() >= 3)
	{
		BeforePlan = ProgBefore.Steps[0].AcquiredScore;
		BeforeDev  = ProgBefore.Steps[1].AcquiredScore;
		BeforeQA   = ProgBefore.Steps[2].AcquiredScore;
	}

	StageMgr->HandleEventChoice(ChoiceIndex);

	// 텍스트 팝업 = 선택된 Choice 효과 요약(점수/시간/버프/정지/보상) — 카드와 동일 포맷.
	// 점수/시간 델타만 보던 구 방식의 가짜 "변화 없음"(버프/정지만 있는 선택지) 오판을 제거.
	if (bHasChoice)
	{
		ShowEventEffectPopup(Choice);
	}

	// 양수 점수 보너스 → 화면 중앙에서 해당 Step ProgressBar로 Orb burst 비행 (실제 적용 델타 기준)
	const FStageProgressData& ProgAfter = StageMgr->GetProgressData();
	if (ProgAfter.Steps.Num() >= 3 && UI_OfficeProjectCardCurrent)
	{
		const int32 PlanDelta = FMath::RoundToInt(ProgAfter.Steps[0].AcquiredScore - BeforePlan);
		const int32 DevDelta  = FMath::RoundToInt(ProgAfter.Steps[1].AcquiredScore - BeforeDev);
		const int32 QADelta   = FMath::RoundToInt(ProgAfter.Steps[2].AcquiredScore - BeforeQA);

		if (PlanDelta > 0) SpawnEventScoreOrbs(1, PlanDelta);
		if (DevDelta  > 0) SpawnEventScoreOrbs(2, DevDelta);
		if (QADelta   > 0) SpawnEventScoreOrbs(3, QADelta);

		// UpdateStepProgressInstant는 호출하지 않음
		// → OnSimultaneousScoresUpdated 경로의 SetCurrentProgressAnimated가 보간 처리 → Orb 비행과 시각적으로 동기화
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Event choice made: %d"), ChoiceIndex);
}

void UOfficeMainWidget::SpawnEventScoreOrbs(int32 StepNumber, int32 BonusAmount)
{
	if (!ScoreOrbContainer || BonusAmount <= 0) return;

	UWidget* TargetWidget = UI_OfficeProjectCardCurrent ? UI_OfficeProjectCardCurrent->GetStepWidget(StepNumber) : nullptr;
	if (!TargetWidget) TargetWidget = GetStripStepBar(StepNumber);
	if (!TargetWidget) return;

	FGeometry CanvasGeo = ScoreOrbContainer->GetCanvasGeometry();
	if (CanvasGeo.GetLocalSize().IsNearlyZero()) return;

	// 시작점: Canvas 로컬 좌표 기준 화면 중앙
	FVector2D ScreenCenter = CanvasGeo.GetLocalSize() * 0.5f;

	// 타겟: 해당 Step ProgressBar 중앙 → Canvas 로컬 (기존 OnScoreOrbRequested 패턴)
	const FGeometry& TargetGeo = TargetWidget->GetCachedGeometry();
	FVector2D TargetAbsCenter = TargetGeo.LocalToAbsolute(TargetGeo.GetLocalSize() * 0.5f);
	FVector2D TargetLocal = CanvasGeo.AbsoluteToLocal(TargetAbsCenter);

	// 보너스 크기 비례 Orb 개수 — 5점당 1개, 5~15개 cap
	int32 OrbCount = FMath::Clamp(BonusAmount / 5, 5, 15);

	ScoreOrbContainer->SpawnEventOrbBurst(ScreenCenter, TargetLocal, StepNumber, OrbCount);
}

void UOfficeMainWidget::ShowEventEffectPopup(const FProjectEventChoice& Choice)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr);
	if (!GI) return;
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	// 선택 카드와 동일한 효과 요약(점수/시간/버프/정지/보상) — 비면 진짜 무효과
	const int32 WorkerCount = UEventChoiceCardWidget::CountWorkersInWorld(GetWorld());
	const EProjectMode Mode = UEventChoiceCardWidget::GetActiveProjectMode(GetWorld());
	FString Message = UEventChoiceCardWidget::BuildEffectSummaryText(Choice, WorkerCount, Mode);
	if (Message.IsEmpty())
	{
		Message = TEXT("변화 없음");
	}

	// 부정(점수-/시간+/랜덤페널티/보상감소) 있으면 Red, 아니면 Green. 정지-only 등 중립도 Green(카드 중립 톤과 정합, 경고색 회피)
	const bool bAnyNegative = (Choice.PlanningScoreBonus < 0.f || Choice.DevScoreBonus < 0.f || Choice.QAScoreBonus < 0.f
		|| Choice.TimerAdjustment > 0.f || Choice.bRandomCategoryPenalty || Choice.RewardMultiplier < 1.0f);

	EWidgetType EffectType = bAnyNegative
		? EWidgetType::NiagaraTextEffect_Red
		: EWidgetType::NiagaraTextEffect_Green;

	TSubclassOf<UUserWidget> EffectClass = TableMgr->GetWidgetClass(EffectType);
	if (!EffectClass) return;

	UNiagaraTextEffectWidget* TextEffect = CreateWidget<UNiagaraTextEffectWidget>(GetWorld(), EffectClass);
	if (TextEffect)
	{
		TextEffect->SetEffectText(FText::FromString(Message));
		TextEffect->AddToViewport(1001);
		TextEffect->PlayEffectAndRemove();
	}
}

void UOfficeMainWidget::OnProjectModeChangedReceived(EProjectMode NewMode)
{
	// 상태 기반 재동기화 (델리게이트 순서 무관, 일관성 보장)
	RefreshLeftSlotState();
	UpdateIdleTierText();

	// 착수 경로가 SelectProject(OnProjectSelected 발행) 이후 TargetScore를 조정할 수 있으므로 카드를 다시 갱신
	UpdateProjectCardProgress();


	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Project mode changed: %s"),
		*ProjectModeToString(NewMode));
}

void UOfficeMainWidget::OnOpenBoardButtonClicked()
{
	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr) return;

	UUIBase* UIBase = UIMgr->GetUIBase();
	if (!UIBase) return;

	// 운영 중에는 새 프로젝트 시작 금지
	if (GetCurrentBuildingOperation() != nullptr)
	{
		UIMgr->ShowNotification(NSLOCTEXT("Office", "OperationActive", "운영 중에는 새 프로젝트를 시작할 수 없습니다"), 2.5f, ENotificationType::Failed);
		UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] New project blocked - operation active"));
		return;
	}

	// GDS 착수 = 기획 보드(현재 티어 전부 + 추천 카드 1장). 보드가 자체적으로 StartSelfDevelopFromProject 호출 →
	// FSM Developing 전환을 RefreshOfficeUI 가 받아 우 레일 갱신.
	TSubclassOf<UUserWidget> PitchClassRaw = TableMgr->GetWidgetClass(EWidgetType::PitchBoard);
	if (!PitchClassRaw)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] PitchBoard 위젯 미등록(DT_WidgetClass) - 기획 보드 열기 실패"));
		return;
	}

	UIBase->PushPromptClass(PitchClassRaw.Get());
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Pitch board opened"));
}

void UOfficeMainWidget::RefreshLeftSlotState()
{
	// 레거시 — FSM의 Lifecycle을 단일 진실로 사용
	if (UOfficeStageProgressManager* SM = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr)
	{
		RefreshOfficeUI(SM->GetLifecycle());
	}
}

void UOfficeMainWidget::RefreshProjectNameBar()
{
	if (!UIE_ProjectNameBar) return;

	// 1. 운영 중 → 운영 상태
	if (FOperationData* CurrentOp = GetCurrentBuildingOperation())
	{
		if (CurrentOp->State == EOperationState::Operating || CurrentOp->State == EOperationState::Paused)
		{
			UIE_ProjectNameBar->SetOperationStatus(
				CurrentOp->ProjectNumber,
				FText::FromString(CurrentOp->ProjectName),
				QualityGradeToAlphabetString(CurrentOp->QualityGrade)
			);
			return;
		}
	}

	// 2. 스테이지 진행 중 또는 모드 활성화 → 모드별 개발 중
	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (StageMgr)
	{
		const FStageProgressData& Progress = StageMgr->GetProgressData();
		if (Progress.ActiveMode != EProjectMode::None && Progress.ProjectID > 0)
		{
			UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
			UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
			if (TableMgr)
			{
				// 현재 빌딩 CompanyType(GI) 우선 — stale Progress.CompanyType 방어
				bool bSuccess = false;
				FProjectData ProjData = TableMgr->ResolveProjectData(Progress.ProjectID, bSuccess);
				if (bSuccess)
				{
					UIE_ProjectNameBar->SetDevelopmentStatus(
						Progress.ProjectNumber,
						ProjData.ProjectName,
						Progress.ActiveMode
					);
					return;
				}
			}
		}
	}

	// 3. 그 외 → 대기 상태
	UIE_ProjectNameBar->SetIdleStatus();
}

void UOfficeMainWidget::UpdateIdleTierText()
{
	// 티어 칩은 상단 밴드(OfficeLayer)로 이관 — 여긴 우레일 IdleTierProgress StatRow 만(배치 시).
	if (!IdleTierProgress) return;

	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!StageMgr) return;

	const FProjectTierProgress& Progress = StageMgr->GetTierProgress();
	int32 Cleared = Progress.GetTierClearedCount(Progress.CurrentTier);

	FString TierName = FString::Printf(TEXT("%d단계"), Progress.CurrentTier);
	FString TierValue = FString::Printf(TEXT("%d/%d"), Cleared, TierConstants::CLEAR_TO_UNLOCK);

	IdleTierProgress->SetStatInfo(TierName, TierValue);
}

// ============================================================================
// 상단 프로젝트 스트립 populate (신규 — 은퇴한 UI_OfficeProjectCardCurrent/UIE_ProjectNameBar 역할 흡수)
// ============================================================================

void UOfficeMainWidget::CacheStripStageCells()
{
	StripRequirementRow = GetWidgetFromName(TEXT("StageRow"));
	StageTray = Cast<UBorder>(GetWidgetFromName(TEXT("StageTray")));
	LastStageTrayMatSize = FVector2D::ZeroVector;
	StripStageCells.Reset();
	StripStageNames.Reset();
	StripStageVals.Reset();
	StripStageBars.Reset();
	StripStageDisciplineIcons.Reset();
	StripStageGoalRings.Reset();
	StripRootWidget = GetWidgetFromName(TEXT("StripRoot"));
	// 체인 카드와 성장 목표판이 함께 붙는 칼럼을 옮긴다(목표판은 MissionManager가 이 칼럼에 형제로 붙인다).
	MissionAnchor = MissionTracker && MissionTracker->GetParent() ? Cast<UWidget>(MissionTracker->GetParent()) : GetWidgetFromName(TEXT("MissionAnchorBox"));
	MissionAnchorShiftY = 0.0f;
	for (int32 i = 1; i <= 6; ++i)
	{
		StripStageCells.Add(GetWidgetFromName(*FString::Printf(TEXT("Stage%dCell"), i)));
		StripStageNames.Add(Cast<UCommonTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dName"), i))));
		StripStageVals.Add(Cast<UCommonTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dVal"), i))));
		StripStageBars.Add(Cast<UProgressBar>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dBar"), i))));
		StripStageDisciplineIcons.Add(Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dDisciplineIcon"), i))));
		StripStageGoalRings.Add(Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dGoalRing"), i))));
		StripStageRingMatSize[i - 1] = FVector2D::ZeroVector;
		if (UWidget* Cell = StripStageCells.Last()) Cell->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	// 값은 바 위에 겹친 labeled meter — 중앙 정렬이라 펀치 피봇도 중앙(정렬 기준점 일치, 좌우 대칭 확장).
	for (UCommonTextBlock* ValT : StripStageVals)
	{
		if (!ValT) continue;
		ValT->SetJustification(ETextJustify::Center);
		ValT->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	if (StripStageDisciplineIcons.Contains(nullptr) || StripStageGoalRings.Contains(nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] Development strip icon or goal ring is missing. Run Tools/OfficeStrip/apply_hud_finish.py run(fit_only=True)."));
	}

	// 썸네일 프레임/크기 스타일은 WBP 소유 (슬림 키라인 트리 2026-07-11 paste) — C++ 스타일링 금지 규칙
}

UWidget* UOfficeMainWidget::GetStripStepBar(int32 StepNumber) const
{
	const int32 Idx = StepNumber - 1;
	return StripStageBars.IsValidIndex(Idx) ? StripStageBars[Idx] : nullptr;
}

UWidget* UOfficeMainWidget::GetStripSpeedButtonWidget() const
{
	return StripSpeedButton;
}

UWidget* UOfficeMainWidget::GetStripRequirementRowWidget() const
{
	return StripRequirementRow;
}

UWidget* UOfficeMainWidget::GetStripOperationTimeWidget() const
{
	// 운영 띠의 시간 숫자는 갱신 경로도 이름으로 찾는다(UpdateStripOperationMetrics) ― 같은 위젯을 짚는다
	return GetWidgetFromName(TEXT("OpModeBadgeText"));
}

void UOfficeMainWidget::PopulateStripIdentity()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (GI && IsManufacturingType(GI->GetCurrentBuildingCompanyType())) return;
	UOfficeStageProgressManager* SM = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!SM || !TableMgr) return;

	const FStageProgressData& Progress = SM->GetProgressData();

	// 이름/장르/아이콘 = 현재 프로젝트 (DT). Progress.ProjectID 로 resolve.
	FText NameText;
	FName Genre = NAME_None;
	TSoftObjectPtr<UTexture2D> IconPtr;
	if (Progress.ProjectID > 0)
	{
		bool bOk = false;
		FProjectData PD = TableMgr->ResolveProjectData(Progress.ProjectID, bOk);
		if (bOk) { NameText = PD.ProjectName; Genre = PD.Genre; IconPtr = PD.Icon; }
	}

	// 정체성은 ProgressData 우선(커스텀명/합성명 반영 — "조합=정체성" 규칙) — DT는 폴백+커버 소스
	if (!Progress.ProjectName.IsEmpty()) NameText = FText::FromString(Progress.ProjectName);
	if (!Progress.Genre.IsNone()) Genre = Progress.Genre;

	// 운영 중이면 운영 이름/등급으로 덮음
	EQualityGrade OpGrade = EQualityGrade::Max;
	if (FOperationData* Op = GetCurrentBuildingOperation())
	{
		if (!Op->ProjectName.IsEmpty()) NameText = FText::FromString(Op->ProjectName);
		OpGrade = Op->QualityGrade;
	}

	if (DevProjectName) DevProjectName->SetText(NameText);
	if (OpProjectName)
	{
		if (OpTitleBaseFontSize <= 0.0f) OpTitleBaseFontSize = OpProjectName->GetFont().Size;
		FSlateFontInfo TitleFont = OpProjectName->GetFont();
		const USizeBox* TitleBox = Cast<USizeBox>(GetWidgetFromName(TEXT("OpProjectNameBox")));
		OpProjectName->SetText(FitOpCardTitle(NameText, TitleFont, OpTitleBaseFontSize, TitleBox ? TitleBox->GetWidthOverride() : 0.0f));
		OpProjectName->SetFont(TitleFont);
	}

	// 썸네일 커버 — 운영/개발 공용. 매번 새 브러시로 구성: SetBrushFromMaterial 은 이전 폴백 다크브러시의
	// TintColor/DrawAs 를 보존해 커버가 다크퍼플과 곱해짐(보라 섞임 사고 — SetBrushColor 는 별개 프로퍼티라 무효였음).
	UTexture2D* IconTex = IconPtr.IsNull() ? nullptr : IconPtr.LoadSynchronous();
	auto ApplyThumbCover = [this, IconTex](UBorder* Inner, UCommonTextBlock* QText, float InnerW, float InnerH)
	{
		if (!Inner) return;
		if (IconTex)
		{
			// M_UI_RoundedThumb(Tex/AspectX/Radius) MID — 코너 정합: 프레임 라인 r16 − 인셋 2px = r14 (Radius=높이 정규화)
			static TSoftObjectPtr<UMaterialInterface> RoundMatPtr(
				FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_RoundedThumb.M_UI_RoundedThumb")));
			UMaterialInterface* RoundMat = RoundMatPtr.LoadSynchronous();

			FSlateBrush CoverBrush;
			CoverBrush.DrawAs = ESlateBrushDrawType::Image;
			CoverBrush.TintColor = FSlateColor(FLinearColor::White);
			if (RoundMat)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(RoundMat, this);
				MID->SetTextureParameterValue(TEXT("Tex"), IconTex);
				MID->SetScalarParameterValue(TEXT("AspectX"), InnerW / InnerH);
				MID->SetScalarParameterValue(TEXT("Radius"), 14.0f / InnerH);
				MID->SetVectorParameterValue(TEXT("BgFill"), FLinearColor(0.009134f, 0.014444f, 0.035601f, 1.0f));   // 크림 레터박스 → 셸 네이비
				CoverBrush.SetResourceObject(MID);
			}
			else
			{
				CoverBrush.SetResourceObject(IconTex);   // 머티리얼 로드 실패 시 폴백(각짐)
			}
			Inner->SetBrush(CoverBrush);
			if (QText) QText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			// 플레이스홀더 다크판은 프레임 BG(MI_UI_ThumbFrame*BG)가 담당 — Inner는 비움
			FSlateBrush EmptyBrush;
			EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			Inner->SetBrush(EmptyBrush);
			if (QText) QText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	};
	// 커버는 출시 후(운영)만 공개한다. 개발 중 표지 자리는 WBP에서 접어 분야 칸 폭으로 쓴다(사용자 확정 2026-10-13).
	// WBP의 표지 크기에서 프레임 인셋 2px를 제외해 재질 비율과 모서리를 맞춘다.
	if (const USizeBox* OpCoverSize = Cast<USizeBox>(GetWidgetFromName(TEXT("OpThumb"))))
	{
		for (const TCHAR* FrameName : { TEXT("OpThumbBG"), TEXT("OpThumbLine") })
		{
			if (UImage* FrameImage = Cast<UImage>(GetWidgetFromName(FrameName)))
			{
				if (UMaterialInstanceDynamic* FrameMID = FrameImage->GetDynamicMaterial())
				{
					FrameMID->SetScalarParameterValue(TEXT("Wpx"), OpCoverSize->GetWidthOverride());
					FrameMID->SetScalarParameterValue(TEXT("Hpx"), OpCoverSize->GetHeightOverride());
				}
			}
		}
		ApplyThumbCover(OpThumbInner, OpThumbQ,
			FMath::Max(1.f, OpCoverSize->GetWidthOverride() - 4.f),
			FMath::Max(1.f, OpCoverSize->GetHeightOverride() - 4.f));
	}

	// 개발 썸네일 키라인 = 장르색 (정체성 액센트 — 기획 보드 카드와 동일 출처 DT_ProjectGenre.Color)
	if (DevThumbLine)
	{
		FLinearColor GenreCol = FLinearColor::White;
		FProjectGenreRow GenreRow;
		if (!Genre.IsNone() && TableMgr->GetGenreInfo(Progress.CompanyType, Genre, GenreRow))
		{
			GenreCol = GenreRow.Color;
		}
		if (UMaterialInstanceDynamic* LineMID = DevThumbLine->GetDynamicMaterial())
		{
			LineMID->SetVectorParameterValue(TEXT("LineCol"), GenreCol);
		}
	}

	// 장르 배지(개발) — 빈 장르면 Box Collapsed (데이터주도, 코드 폴백 금지)
	if (DevGenreBadgeBox)
	{
		if (!Genre.IsNone())
		{
			if (DevGenreBadge) DevGenreBadge->SetText(FText::FromName(Genre));
			DevGenreBadgeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			DevGenreBadgeBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	ApplyOpGradeEmblem(OpGrade);
}

void UOfficeMainWidget::ApplyOpGradeEmblem(EQualityGrade Grade)
{
	if (!OpGradeBadgeBox) return;

	ECompanyType Company = ECompanyType::None;
	if (USaveLoadManager* SaveMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr)
	{
		const int32 BuildingIndex = GetCurrentBuildingIDAsInt();
		if (BuildingIndex != INDEX_NONE) Company = SaveMgr->GetBuildingCompanyType(BuildingIndex);
	}
	if (Grade == AppliedOpGrade && Company == AppliedOpGradeCompany) return;
	AppliedOpGrade = Grade;
	AppliedOpGradeCompany = Company;

	if (Grade == EQualityGrade::Max)
	{
		OpGradeBadgeBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (OpGradeBadge) OpGradeBadge->SetText(FText::FromString(QualityGradeToAlphabetString(Grade)));
	OpGradeBadgeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	CGQualityGradeMedal::Apply(GetGameInstance(), Company, Grade, OpGradeBadgeBox, OpGradeBadge);
}

void UOfficeMainWidget::UpdateStripStageCells()
{
	const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (GI && IsManufacturingType(GI->GetCurrentBuildingCompanyType())) return;
	UOfficeStageProgressManager* SM = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (!SM) return;
	const FStageProgressData& Progress = SM->GetProgressData();

	for (int32 i = 0; i < 6; ++i)
	{
		UWidget* Cell = StripStageCells.IsValidIndex(i) ? StripStageCells[i] : nullptr;
		const bool bHasStep = Progress.Steps.IsValidIndex(i) && !Progress.Steps[i].StepName.IsEmpty();

		// 스텝명 비면 셀 통째 Collapsed (활성 직능 수만큼만 표시, 나머지 칸 숨김)
		if (Cell) Cell->SetVisibility(bHasStep ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (!bHasStep) { bStripStagePendingDirty[i] = false; continue; }

		const FStepRoundData& Step = Progress.Steps[i];
		const float Pct = Step.TargetScore > 0.0f ? (Step.AcquiredScore / Step.TargetScore) : 0.0f;   // 미클램프 → 오버필 표시

		if (UCommonTextBlock* NameT = StripStageNames.IsValidIndex(i) ? StripStageNames[i] : nullptr)
		{
			NameT->SetText(FText::FromString(Step.StepName));
		}
		if (UImage* Icon = StripStageDisciplineIcons.IsValidIndex(i) ? StripStageDisciplineIcons[i] : nullptr)
		{
			UTexture2D* Texture = UGlobalUtilFunctions::GetDisciplineIcon(Step.DisciplineSlot);
			Icon->SetBrushFromTexture(Texture, false);
			// 흰 글리프를 분야 고유색으로 물들여 여섯 칸이 한눈에 갈리게 한다(결과창 막대와 같은 출처).
			Icon->SetColorAndOpacity(UGlobalUtilFunctions::GetStepColor(Step.DisciplineSlot + 1));
			Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}

		// 상태 전환/리프레시 = 스냅 (라운드 진행이 아니므로 애니메이션 없이 즉시). 이후 라이브 점수는 NativeTick 이 lerp.
		StripStageTargetScore[i] = Step.TargetScore;
		StripStagePendingPct[i] = Pct;
		bStripStagePendingDirty[i] = false;
		StripStageFlashAlpha[i] = 0.0f;
		StripStagePunchElapsed[i] = 99.0f;
		StripStageGoalElapsed[i] = StripGoalDuration;
		StripStageVisualState[i] = 0;
		if (Cell) Cell->SetRenderScale(FVector2D(1.0f, 1.0f));
		if (UImage* Ring = StripStageGoalRings.IsValidIndex(i) ? StripStageGoalRings[i] : nullptr)
		{
			Ring->SetRenderOpacity(0.0f);
		}
		if (UCommonTextBlock* ValT = StripStageVals.IsValidIndex(i) ? StripStageVals[i] : nullptr)
		{
			ValT->SetRenderScale(FVector2D(1.0f, 1.0f));
			// 글자 최소 폭은 점수 줄 ScaleBox 안쪽이라 좁은 화면에서는 칸을 넓히지 않고 글자 배율만 함께 낮춘다.
			ValT->SetMinDesiredWidth(MeasureStripScoreWidth(ValT, FMath::RoundToInt64(Step.TargetScore)));
		}
		StripStageShownPct[i] = Pct;
		StripStageTargetPct[i] = Pct;
		ApplyStripStageVisual(i, Pct);
	}
}

void UOfficeMainWidget::UpdateStripStageLive(int32 Index, float Score, float Target)
{
	if (Index < 0 || Index >= 6) return;
	// 권위값은 pending에만 — 시각 목표는 orb 착지가 플러시("구슬이 점수를 나른다"). 미클램프 → 오버필(>1)도 표시.
	StripStageTargetScore[Index] = Target;
	StripStagePendingPct[Index] = (Target > 0.0f) ? (Score / Target) : 0.0f;
	if (!bStripStagePendingDirty[Index])
	{
		bStripStagePendingDirty[Index] = true;
		StripStagePendingSince[Index] = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
}

bool UOfficeMainWidget::FlushStripStagePending(int32 Index)
{
	if (Index < 0 || Index >= 6 || !bStripStagePendingDirty[Index]) return false;
	bStripStagePendingDirty[Index] = false;

	const float Pct = StripStagePendingPct[Index];
	StripStageTargetPct[Index] = Pct;   // NativeTick lerp가 여기서부터 카운트업

	return true;
}

void UOfficeMainWidget::UpdateStripShellMaterialSize()
{
	if (StageTray)
	{
		const FVector2D TraySize = StageTray->GetCachedGeometry().GetLocalSize();
		if (TraySize.X >= 1.f && TraySize.Y >= 1.f && !TraySize.Equals(LastStageTrayMatSize, 0.5f))
		{
			if (UMaterialInstanceDynamic* TrayMID = StageTray->GetDynamicMaterial())
			{
				TrayMID->SetScalarParameterValue(TEXT("Wpx"), TraySize.X);
				TrayMID->SetScalarParameterValue(TEXT("Hpx"), TraySize.Y);
				LastStageTrayMatSize = TraySize;
			}
		}
	}
	// 금색 테두리는 칩 라인 계열이라 게이지 실치수를 받아야 코너가 맞는다. 테두리는 평소 투명이라 같은 칸을 채우는 바 크기를 쓴다.
	for (int32 i = 0; i < 6; ++i)
	{
		UImage* Ring = StripStageGoalRings.IsValidIndex(i) ? StripStageGoalRings[i] : nullptr;
		const UProgressBar* Bar = StripStageBars.IsValidIndex(i) ? StripStageBars[i] : nullptr;
		if (!Ring || !Bar) continue;
		const FVector2D BarSize = Bar->GetCachedGeometry().GetLocalSize();
		if (BarSize.X < 1.f || BarSize.Y < 1.f || BarSize.Equals(StripStageRingMatSize[i], 0.5f)) continue;
		if (UMaterialInstanceDynamic* RingMID = Ring->GetDynamicMaterial())
		{
			RingMID->SetScalarParameterValue(TEXT("Wpx"), BarSize.X);
			RingMID->SetScalarParameterValue(TEXT("Hpx"), BarSize.Y);
			StripStageRingMatSize[i] = BarSize;
		}
	}
	if (!StripBG && !StripLine) return;
	const FVector2D LocalSize = (StripBG ? StripBG : StripLine)->GetCachedGeometry().GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastStripMatSize, 0.5f)) return;
	LastStripMatSize = LocalSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (UImage* ShellImage : { StripBG, StripLine })
	{
		if (!ShellImage) continue;
		if (UMaterialInstanceDynamic* MID = ShellImage->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(WpxParam, LocalSize.X);
			MID->SetScalarParameterValue(HpxParam, LocalSize.Y);
		}
	}
}

void UOfficeMainWidget::UpdateMissionAnchorBelowStrip(float DeltaTime)
{
	if (!MissionAnchor) return;
	float TargetY = 0.0f;
	if (StripRootWidget && StripRootWidget->IsVisible())
	{
		// 같은 화면 좌표로 바닥/윗변을 재고, 이미 준 이동량은 빼서 원래 자리 기준으로 계산한다.
		const FGeometry& Own = GetCachedGeometry();
		const FGeometry& StripGeo = StripRootWidget->GetCachedGeometry();
		const FGeometry& AnchorGeo = MissionAnchor->GetCachedGeometry();
		if (StripGeo.GetLocalSize().Y > 1.0f && AnchorGeo.GetLocalSize().Y > 1.0f)
		{
			const float StripBottom = Own.AbsoluteToLocal(StripGeo.LocalToAbsolute(FVector2D(0.0f, StripGeo.GetLocalSize().Y))).Y;
			const float AnchorTop = Own.AbsoluteToLocal(AnchorGeo.LocalToAbsolute(FVector2D::ZeroVector)).Y - MissionAnchorShiftY;
			TargetY = FMath::Max(0.0f, StripBottom + MissionAnchorGap - AnchorTop);
		}
	}
	MissionAnchorShiftY = FMotionGate::IsReduced() || FMath::IsNearlyEqual(MissionAnchorShiftY, TargetY, 0.5f)
		? TargetY : FMath::FInterpTo(MissionAnchorShiftY, TargetY, DeltaTime, 12.0f);
	MissionAnchor->SetRenderTranslation(FVector2D(0.0f, MissionAnchorShiftY));
}

void UOfficeMainWidget::ApplyStripRailPlacement(bool bStripOnTop)
{
	UWidget* Rail = GetWidgetFromName(TEXT("StripRailColumn"));
	UOverlaySlot* RailSlot = Rail ? Cast<UOverlaySlot>(Rail->Slot) : nullptr;
	if (!RailSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] StripRailColumn(Overlay 칸) 없음 ― 진행 줄 자리를 못 옮긴다"));
		return;
	}
	FMargin Pad = RailSlot->GetPadding();
	if (StripRailTopBelowBlock < 0.0f)
	{
		StripRailTopBelowBlock = Pad.Top;
	}
	const float Top = bStripOnTop ? StripRailTopFocus : StripRailTopBelowBlock;
	if (!FMath::IsNearlyEqual(Pad.Top, Top))
	{
		Pad.Top = Top;
		RailSlot->SetPadding(Pad);
	}
}

void UOfficeMainWidget::RefreshOpCardVisibility(EProjectLifecycle State)
{
	if (!OpCard) return;
	const UCGGameInstance* OfficeGI = Cast<UCGGameInstance>(GetGameInstance());
	const AOfficeCameraPawn* Camera = PlacementEditPawn.Get();
	const bool bOperating = State == EProjectLifecycle::Operating || State == EProjectLifecycle::ReportPending;
	const bool bManufacturing = OfficeGI && IsManufacturingType(OfficeGI->GetCurrentBuildingCompanyType());
	const bool bEditing = Camera && Camera->IsOfficePlacementEditing();
	OpCard->SetVisibility(bOperating && !bManufacturing && !bEditing ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UOfficeMainWidget::ResetLastSpurtVisual()
{
	if (!bLastSpurtVisual && LastSpurtElapsed == 0.0f) return;
	bLastSpurtVisual = false;
	LastSpurtElapsed = 0.0f;
	if (ProcessTimeText)
	{
		ProcessTimeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.838799f, 0.854993f, 0.871367f, 1.0f)));
	}
}

void UOfficeMainWidget::OnScoreOrbArrivedReceived(int32 StepNumber, bool bIsCritical)
{
	if (FlushStripStagePending(StepNumber - 1))
	{
		StartStripStageJuice(StepNumber - 1, bIsCritical);
	}
}

void UOfficeMainWidget::StartStripStageJuice(int32 Index, bool bIsCritical)
{
	if (Index < 0 || Index >= 6) return;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - StripStageLastJuiceTime[Index] < StripPunchThrottle) return;
	StripStageLastJuiceTime[Index] = Now;

	// 착지 틱 — 연속 감쇠(rapid-fire decay): 조용하다 첫 착지=또렷(1.0), 연타마다 ×0.7(바닥 0.3), 0.8s 조용하면 리셋.
	// 피치 ±6% 랜덤 = 기계적 반복감 제거. 함수 로컬 static = 스트립 단일 인스턴스라 공유 무해.
	static double LastLandSoundTime = -10.0;
	static float LandSoundVolume = 1.0f;
	if (Now - LastLandSoundTime >= 0.2)
	{
		LandSoundVolume = (Now - LastLandSoundTime >= 0.8) ? 1.0f : FMath::Max(0.3f, LandSoundVolume * 0.7f);
		LastLandSoundTime = Now;
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
		{
			if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
			{
				SM->PlayUISoundWithParams(CGUISoundTags::BrickPop, LandSoundVolume, FMath::FRandRange(0.94f, 1.06f));
			}
		}
	}

	if (FMotionGate::IsReduced()) return;
	StripStagePunchElapsed[Index] = 0.0f;
	StripStagePunchPeak[Index] = bIsCritical ? 1.2f : 1.12f;
	// 크리 착지 = 골드 플래시로 차별화 (일반 = 화이트)
	StripStageFlashColor[Index] = bIsCritical
		? FLinearColor(0.791298f, 0.421407f, 0.086500f, 1.0f) : FLinearColor::White;
	StripStageFlashAlpha[Index] = 1.0f;
}

void UOfficeMainWidget::ApplyStripStageVisual(int32 Index, float Pct, bool bAnimateGoalTransition)
{
	if (Index < 0 || Index >= 6) return;

	static const FLinearColor BarBlue(0.046665f, 0.327778f, 0.745404f, 1.0f);
	static const FLinearColor BarGreen(0.104616f, 0.630757f, 0.254152f, 1.0f);
	static const FLinearColor BarGold(0.887923f, 0.533276f, 0.076185f, 1.0f);
	static const FLinearColor ValInk(1.0f, 0.982251f, 0.921582f, 1.0f);
	// 같은 계열의 진한 외곽선 — 금색·초록 위 흰 글자는 대비가 3:1 미만이라 외곽선이 받쳐야 하고, 검정보다 덜 뭉쳐 보인다.
	static const FLinearColor ValOutlineBlue(0.004391f, 0.023153f, 0.057805f, 1.0f);
	static const FLinearColor ValOutlineGreen(0.004777f, 0.042311f, 0.015996f, 1.0f);
	static const FLinearColor ValOutlineGold(0.068478f, 0.029557f, 0.001821f, 1.0f);
	static const FLinearColor NameInk(0.887923f, 0.913099f, 0.930111f, 1.0f);
	static const FLinearColor NameGreen(0.564712f, 0.871367f, 0.62396f, 1.0f);
	static const FLinearColor NameGold(1.0f, 0.799103f, 0.434154f, 1.0f);

	// 색과 숫자가 서로 다른 문턱을 보이지 않도록 획득·목표를 같은 반올림 정수로 비교한다.
	// 초과(금색)는 목표의 110%부터다 — 점수가 덩어리로 들어와도 도달(초록) 구간이 실제로 보이게 한다.
	// 출시 가능 여부는 StageProgressData의 최소 점수 판정이 별도로 담당한다.
	static constexpr int64 OverPercent = 110;
	const float Target = StripStageTargetScore[Index];
	const int64 ShownScore = FMath::RoundToInt64(Pct * Target);
	const int64 GoalScore = FMath::RoundToInt64(Target);
	const bool bFull = GoalScore > 0 && ShownScore >= GoalScore;
	const bool bOver = bFull && ShownScore * 100 >= GoalScore * OverPercent;
	const uint8 VisualState = bOver ? 3 : (bFull ? 2 : 1);
	// 목표 배너도 보간된 숫자가 문턱을 넘은 프레임에 재생해 셀 색보다 앞서지 않게 한다.
	if (bAnimateGoalTransition && bFull && !bCategoryMilestoneReached[Index])
	{
		bCategoryMilestoneReached[Index] = true;
		PlayCategoryMilestone(Index);
	}
	if (bAnimateGoalTransition && StripStageVisualState[Index] != 0
		&& VisualState > StripStageVisualState[Index] && bFull && !FMotionGate::IsReduced())
	{
		StripStageGoalElapsed[Index] = 0.0f;
		StripStageFlashAlpha[Index] = 1.0f;
		StripStageFlashColor[Index] = FLinearColor::White;
	}
	StripStageVisualState[Index] = VisualState;

	const FLinearColor BaseBarCol = bOver ? BarGold : (bFull ? BarGreen : BarBlue);
	// 착지 플래시 — 화이트 순간 블렌드 후 감쇠 (상태색 로직 위에 얹힘)
	const float Flash = StripStageFlashAlpha[Index] * 0.6f;
	const FLinearColor BarCol = (Flash > 0.0f)
		? FMath::Lerp(BaseBarCol, StripStageFlashColor[Index], Flash) : BaseBarCol;

	if (UProgressBar* Bar = StripStageBars.IsValidIndex(Index) ? StripStageBars[Index] : nullptr)
	{
		Bar->SetPercent(FMath::Clamp(Pct, 0.0f, 1.0f));   // 바는 100% 캡 — 오버필은 숫자/색으로 표현
		Bar->SetFillColorAndOpacity(BarCol);
	}
	if (UCommonTextBlock* ValT = StripStageVals.IsValidIndex(Index) ? StripStageVals[Index] : nullptr)
	{
		ValT->SetText(FText::FromString(FString::Printf(TEXT("%s/%s"), *FormatStripScore(ShownScore), *FormatStripScore(GoalScore))));
		ValT->SetColorAndOpacity(FSlateColor(ValInk));
		const FLinearColor& OutlineCol = bOver ? ValOutlineGold : (bFull ? ValOutlineGreen : ValOutlineBlue);
		if (!ValT->GetFont().OutlineSettings.OutlineColor.Equals(OutlineCol))
		{
			FSlateFontInfo ValFont = ValT->GetFont();
			ValFont.OutlineSettings.OutlineColor = OutlineCol;
			ValT->SetFont(ValFont);
		}
	}
	if (UCommonTextBlock* NameT = StripStageNames.IsValidIndex(Index) ? StripStageNames[Index] : nullptr)
	{
		NameT->SetColorAndOpacity(FSlateColor(bOver ? NameGold : (bFull ? NameGreen : NameInk)));
	}
}

void UOfficeMainWidget::UpdateStripOperationMetrics(const FOperationData& Data)
{
	if (RevPerSecText)
	{
		// 표시 순수익(감쇠O/진동X) 단일 정의 — 매초 출렁이는 진동 스냅샷 대신 (포맷 "+N"은 스트립 폭 관용 유지)
		float DisplayRate = Data.ActualRevenuePerSecond;
		if (UProjectOperationManager* OpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProjectOperationManager>() : nullptr)
		{
			DisplayRate = OpMgr->GetBuildingDisplayNetPerSec(Data.BuildingID);
		}
		const FText Abbrev = UGlobalUtilFunctions::AbbreviateNumber(
			static_cast<int64>(DisplayRate), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor);
		RevPerSecText->SetText(FText::FromString(FString::Printf(TEXT("+%s"), *Abbrev.ToString())));
	}
	if (TotalRevText)
	{
		TotalRevText->SetText(UGlobalUtilFunctions::AbbreviateNumber(
			static_cast<int64>(Data.TotalRevenueEarned), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	}

	ApplyOpGradeEmblem(Data.QualityGrade);

	// 종료 행동 옆에 남은 시간을 고정해 수익 정보와 분리한다.
	if (UCommonTextBlock* ModeBadge = Cast<UCommonTextBlock>(GetWidgetFromName(TEXT("OpModeBadgeText"))))
	{
		const int32 Rem = FMath::Max(0, FMath::CeilToInt(Data.RemainingTime));
		ModeBadge->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Rem / 60, Rem % 60)));
		// 15초 이하 = 종료 임박 → 골드 경고
		static const FLinearColor OpTimeInk(0.887923f, 0.913099f, 0.930111f, 1.0f);
		static const FLinearColor OpWarn(0.921582f, 0.679542f, 0.270498f, 1.0f);
		static const FLinearColor OpLabelMute = FLinearColor::FromSRGBColor(FColor(0x9A, 0xA4, 0xB2));   // 옆 칸 「누적 수익」 라벨과 같은 회색
		ModeBadge->SetColorAndOpacity(FSlateColor(Rem <= 15 ? OpWarn : OpTimeInk));
		if (UCommonTextBlock* TimeLabel = Cast<UCommonTextBlock>(GetWidgetFromName(TEXT("OpTimeLabel"))))
		{
			TimeLabel->SetText(Rem <= 15 ? NSLOCTEXT("Office", "OperationEndingSoon", "곧 종료")
				: NSLOCTEXT("Office", "OperationRemainingTime", "남은 시간"));
			TimeLabel->SetColorAndOpacity(FSlateColor(Rem <= 15 ? OpWarn : OpLabelMute));
		}
	}
}
