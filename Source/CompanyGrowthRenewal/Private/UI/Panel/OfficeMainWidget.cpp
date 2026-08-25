#include "UI/Panel/OfficeMainWidget.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "Manager/MissionManagerSubsystem.h"
#include "UI/HUD/MissionTrackerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Office/OfficeManager.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Cards/IconCardWidget.h"
#include "UI/Element/Trade/ProjectInfoBarWidget.h"
#include "UI/Element/Office/OfficeProjectCardWidget.h"
#include "UI/Panel/OfficeRecruitmentPanelWidget.h"
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
#include "Components/VerticalBox.h"
#include "Components/ProgressBar.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Effects/NiagaraTextEffectWidget.h"
#include "UI/Element/Effects/TextGlitchEffectWidget.h"
#include "UI/Panel/LaunchConfirmWidget.h"
#include "UI/Panel/LaunchRewardRevealWidget.h"
#include "UI/Element/Office/ReviewReactionToastWidget.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "UI/Panel/ProjectReportWidget.h"
#include "Data/ProjectReportData.h"
#include "UI/Element/Effects/ScoreOrbContainerWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Enum/VFXType.h"
#include "Table/VFXTable.h"
#include "Player/BuildingLandCameraShake.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Manager/EmployeeManager.h"
#include "Global/GlobalUtilFunctions.h"
#include "Data/StageProgressData.h"

namespace
{
	// 타이머 링 상태색 (linear) — 평시 골드 / 임박 레드.
	// 스트립 점수 바가 그린을 "목표 달성" 으로 쓰므로 시간에는 재사용하지 않는다(의미 충돌).
	const FLinearColor TimerRingGold(0.887923f, 0.533276f, 0.076185f, 1.0f);
	const FLinearColor TimerRingRed(0.701102f, 0.048172f, 0.048172f, 1.0f);   // #EF4444
	constexpr float TimerWarnSeconds = 5.0f;
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
	RefreshOfficeUI(NewState);
}

void UOfficeMainWidget::RefreshOfficeUI(EProjectLifecycle State)
{
	// 1. 스위처 3페이지: Idle=0 / {Developing,LaunchPending}=1 / {Operating,ReportPending}=2
	int32 PageIdx = 0;
	switch (State)
	{
	case EProjectLifecycle::Developing:
	case EProjectLifecycle::LaunchPending:
		PageIdx = 1; break;
	case EProjectLifecycle::Operating:
	case EProjectLifecycle::ReportPending:
		PageIdx = 2; break;
	default:
		PageIdx = 0; break;
	}
	if (ProjectWidgetSwitcher)
	{
		ProjectWidgetSwitcher->SetActiveWidgetIndex(PageIdx);
	}

	// 1-b. 스트립 식별(이름/배지) + 스테이지 셀 갱신 (신규 — 은퇴한 카드/네임바 대체)
	PopulateStripIdentity();
	UpdateStripStageCells();

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

	// 5. 하단 버튼 (개발중에만 숨김)
	const bool bFocusMode = (State == EProjectLifecycle::Developing || State == EProjectLifecycle::LaunchPending);
	if (bFocusMode)
	{
		PlayHideButtonsAnim();
	}
	else
	{
		PlayShowButtonsAnim();
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

	// 7. 상단 밴드(회사배너/트렌드칩/티어칩) = OfficeLayer 소유 — 라이프사이클 전환마다 트리거
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UOfficeLayerWidget* Layer = UIMgr->GetOfficeLayer())
		{
			Layer->RefreshTopBar();
		}
	}

	// 8. 판매 곡선 — 운영 중에만 표시 (idle/개발 중 빈 박스 방지)
	if (SalesCurve)
	{
		SalesCurve->SetVisibility(bIsInOperationMode
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 9. 우레일 대기존 — 힌트박스+착수 CTA 묶음은 대기에만 (스위처 밖으로 나가 자체 토글 필요)
	const bool bIdle = (State == EProjectLifecycle::Idle);
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
		TimerRing->SetVisibility(TimerVis);
	}
	if (ProcessTimeText)
	{
		ProcessTimeText->SetVisibility(TimerVis);
	}
}

void UOfficeMainWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

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
			const float Breathe = 0.45f + 0.45f * (0.5f + 0.5f * FMath::Sin(IdlePulseElapsed * 2.856f));
			CTAGlow->SetRenderOpacity(Breathe);
		}
		else if (IdlePulseElapsed != 0.0f)
		{
			// 대기 종료 시 글로우 소등 1회
			IdlePulseElapsed = 0.0f;
			CTAGlow->SetRenderOpacity(0.0f);
		}
	}

	// 스테이지 바 — 폴백 플러시 / 부드러운 채움(≈3.3/s) / 착지 연출(펀치+플래시)
	const double NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (int32 i = 0; i < 6; ++i)
	{
		if (bStripStagePendingDirty[i] && NowSec - StripStagePendingSince[i] > PendingFlushTimeout)
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
		if (!FMath::IsNearlyEqual(StripStageShownPct[i], StripStageTargetPct[i], PctEps))
		{
			StripStageShownPct[i] = FMath::FInterpConstantTo(StripStageShownPct[i], StripStageTargetPct[i], InDeltaTime, 3.3f);
			bVisualDirty = true;
		}
		if (bVisualDirty)
		{
			ApplyStripStageVisual(i, StripStageShownPct[i]);
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

	// 라스트 스퍼트 — 소리/셰이크뿐이던 잔여 40% 구간에 화면 긴장감 (골드 펄스 ~1.6Hz)
	if (bLastSpurtVisual)
	{
		LastSpurtElapsed += InDeltaTime;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(LastSpurtElapsed * 10.0f);
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

	// 상단 스트립 스테이지 셀 캐시 (BindWidget 대신 트리 이름으로 ― Stage{1..6}Cell/Name/Val/Bar/Done)
	CacheStripStageCells();

	// 폭 예약 없음 — 숫자가 링 중앙에 겹쳐 자리를 링이 소유하므로 자릿수가 변해도 레이아웃이 안 흔들린다.

	// 서브시스템 델리게이트는 위젯 수명 전체에 걸쳐 유지 (임시 비활성 중에도 수신 보장)
	// — OpenBoardButton → PushBottomClass 시 잠시 Deactivated 되어도 OnStepStartEffect 수신 유지
	BindStageProgressDelegates();
	BindBoardEventDelegates();
	BindOperationDelegates();

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
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] NativeConstruct - persistent delegates bound"));
}

void UOfficeMainWidget::NativeDestruct()
{
	// 배속을 켠 채 사무실을 떠나면 세계가 2배속으로 남는다 — 복원의 유일한 지점.
	// 활성/비활성 쌍이 아니라 파괴 시점인 이유: 모달이 뜰 때마다 Deactivate 되므로 거기서 리셋하면 배속이 조용히 풀린다.
	ApplyDevSpeed(1.0f);

	UnbindStageProgressDelegates();
	UnbindBoardEventDelegates();
	UnbindOperationDelegates();

	if (UOfficeManager* OfficeMgr = GetWorld()->GetSubsystem<UOfficeManager>())
	{
		OfficeMgr->OnWorkstationCountChanged.RemoveAll(this);
	}

	if (UEmployeeManager* EmpMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr)
	{
		EmpMgr->OnEmployeeRosterChanged.RemoveAll(this);
	}

	// 진행 중인 이펙트 위젯 정리
	RemoveStepStartEffect();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepEffectTimerHandle);
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

void UOfficeMainWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 버튼 이벤트 바인딩
	if (WorkstationOpenButton) WorkstationOpenButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnWorkstationButtonClicked);
	if (DecorationOpenButton) DecorationOpenButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnDecorationButtonClicked);
	if (EmployeeButton) EmployeeButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnEmployeeButtonClicked);
	if (OfficeUpgradeButton) OfficeUpgradeButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnOfficeUpgradeButtonClicked);
	if (RecruitmentButton) RecruitmentButton->OnClicked().AddUObject(this, &UOfficeMainWidget::OnRecruitmentButtonClicked);
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

void UOfficeMainWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// 스택이 이 위젯을 Collapsed 시키면 Slate 가 틱을 안 돌려 보간이 중간값에서 얼어붙는다 → 떠나기 전에 목표로 스냅
	DockSlideAlpha = bDockHidden ? 1.f : 0.f;
	ApplyDockSlide(DockSlideAlpha);

	if (WorkstationOpenButton) WorkstationOpenButton->OnClicked().Clear();
	if (DecorationOpenButton) DecorationOpenButton->OnClicked().Clear();
	if (EmployeeButton) EmployeeButton->OnClicked().Clear();
	if (OfficeUpgradeButton) OfficeUpgradeButton->OnClicked().Clear();
	if (RecruitmentButton) RecruitmentButton->OnClicked().Clear();
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

void UOfficeMainWidget::OnWorkstationButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Workstation button clicked"));

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();

	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> WorkstationPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeWorkstationPanel);
	if (WorkstationPanel)
	{
		UIManager->GetUIBase()->PushBottomClass(WorkstationPanel.Get());
	}

	// M7 미션 가이드 — [책상 배치]→배치 모드 전이
	if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		M->NotifyPlacementModeOpened(EOfficePlacementKind::Desk);
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

	// 직원창 = 풀스크린 UI_EmployeeWindow
	TSubclassOf<UUserWidget> EmployeeWindow = TableMgr->GetWidgetClass(EWidgetType::EmployeeWindow);
	if (!EmployeeWindow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] EmployeeWindow 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	UIManager->GetUIBase()->PushPromptClass(EmployeeWindow.Get());

	// 입력모드 쌍 — 닫힐 때 EmployeeWindow::NativeOnDeactivated 가 GoToNormalMode 복원
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
		{
			PC->GoToUIMode();
		}
	}
}

void UOfficeMainWidget::OnOfficeUpgradeButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] OfficeUpgrade button clicked"));

	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();

	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> UpgradePanel = TableMgr->GetWidgetClass(EWidgetType::OfficeUpgradePanel);
	if (UpgradePanel)
	{
		UIManager->GetUIBase()->PushBottomClass(UpgradePanel.Get());
	}
}

void UOfficeMainWidget::OnRecruitmentButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Recruitment button clicked"));

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();

	if (!GI || !TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> RecruitmentPanel = TableMgr->GetWidgetClass(EWidgetType::OfficeRecruitmentPanel);
	if (RecruitmentPanel)
	{
		UCommonActivatableWidget* Widget = UIManager->GetUIBase()->PushPromptClass(RecruitmentPanel.Get());

		if (UOfficeRecruitmentPanelWidget* RecruitmentWidget = Cast<UOfficeRecruitmentPanelWidget>(Widget))
		{
			int32 BuildingIndex = GI->GetCurrentManagedBuildingIndex();
			RecruitmentWidget->SetBuildingIndex(BuildingIndex);
		}
	}
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

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] StageProgressManager delegates unbound"));
}

void UOfficeMainWidget::OnTimerUpdated(float RemainingTime)
{
	UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	const float TotalTime = (StageMgr && StageMgr->GetTotalDuration() > 0.0f)
		? StageMgr->GetTotalDuration()
		: 15.0f;

	if (TimerRing)
	{
		TimerRing->SetPercent(TotalTime > 0.0f ? RemainingTime / TotalTime : 0.0f);
	}

	if (ProcessTimeText)
	{
		// 단위 "s" 생략 — 링이 "이건 시간"을 이미 말하고, 84px 링 안쪽(70px)에 44px 글자가 안 들어간다.
		FString TimeString;
		if (RemainingTime <= 0.0f)
		{
			TimeString = TEXT("");
		}
		else if (RemainingTime <= 5.0f)
		{
			TimeString = FString::Printf(TEXT("%.1f"), RemainingTime);
		}
		else
		{
			TimeString = FString::Printf(TEXT("%d"), FMath::CeilToInt(RemainingTime));
		}
		ProcessTimeText->SetText(FText::FromString(TimeString));
	}

	UpdateTimerRingColor(RemainingTime);

	// 후반 라스트 스퍼트 — 잔여 40% 진입 시 1회 큐 (사운드 + 가벼운 셰이크). 깊은 템포 가속은 추후.
	if (!bLateStageEscalated && TotalTime > 0.0f && RemainingTime > 0.0f
		&& (RemainingTime / TotalTime) <= 0.4f)
	{
		bLateStageEscalated = true;
		bLastSpurtVisual = true;
		if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
		{
			if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
			{
				SoundMgr->PlaySound(FName("Dev_LastSpurt"));
			}
		}
		if (!USettingsManagerSubsystem::IsReduceMotion(this))
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				// 막판 스퍼트 = 가벼운 타격 파형 (착지 thud 와 구분). 새 파형이 이미 가벼워 스케일 1.0
				PC->ClientStartCameraShake(ULightImpactCameraShake::StaticClass(), 1.0f);
			}
		}
	}
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
}

void UOfficeMainWidget::OnProjectSelectedReceived()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Project selected, updating card UI"));

	// 새 프로젝트 선택 → 다음 개발 대비 1회성 플래그 리셋 (StepStart 이펙트 없이 시작되는 경로 대비)
	for (bool& bReached : bCategoryMilestoneReached) { bReached = false; }
	bLateStageEscalated = false;
	ResetLastSpurtVisual();

	UpdateProjectCardProgress();
	RefreshLeftSlotState();
}

void UOfficeMainWidget::OnScoreOrbRequestedReceived(
	FVector WorldPos, int32 StepNumber, float Score, bool bIsCritical, int32 OrbCount)
{
	if (!ScoreOrbContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] ScoreOrb - Container is NULL!"));
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

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
	ScoreOrbContainer->SpawnOrbBurst(StartLocal, TargetLocal, StepNumber, bIsCritical, bIsCritical ? 5 : OrbCount);

	// 점수 숫자 "+N"도 같은 시작점에서 orb와 함께 (스크린스페이스, C++ 애니)
	ScoreOrbContainer->SpawnScoreNumber(StartLocal, static_cast<int64>(Score), StepNumber, bIsCritical);

	// 4. 크리 때만 타격 스펙터클 (월드 Niagara + 셰이크 + 음) — 1초 드립에선 매 틱 재생 = 스팸
	if (bIsCritical)
	{
		PlayDevSpectacleFeedback(WorldPos, true);
	}
}

void UOfficeMainWidget::PlayDevSpectacleFeedback(const FVector& WorldPos, bool bIsCritical)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	// 월드 Niagara — 평타=알록달록 루트 튐(DevScoreHit), 크리=흰 스파이크(DevCrit).
	// GetVFXAsset 은 행/에셋 미배치 시 nullptr 반환 → 스폰 자체를 건너뛰어 안전(DT_VFX 채워지기 전 단계 대비).
	if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
	{
		const EVFXType HitType = bIsCritical ? EVFXType::DevCrit : EVFXType::DevScoreHit;

		bool bHasRow = false;
		const FVFXTableRow RowData = TableMgr->GetVFXData(HitType, bHasRow);
		if (UNiagaraSystem* HitSystem = TableMgr->GetVFXAsset(HitType))
		{
			// 좌석 직원 기준 위치는 DT_VFX LocationOffset 으로 조정(라이브 튜닝, 리빌드 불필요)
			const FVector SpawnPos = WorldPos + (bHasRow ? RowData.LocationOffset : FVector(0.f, 0.f, 60.f));
			const float SpawnScale = bHasRow ? RowData.DefaultScale : 1.0f;
			UNiagaraComponent* HitComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), HitSystem, SpawnPos, FRotator::ZeroRotator,
				FVector(SpawnScale), /*bAutoDestroy*/ true);

			// KiAura 는 Infinite 루프라 bAutoDestroy(완료 시 정리)나 Deactivate 만으론 안 사라질 수 있음.
			// 2단계 종료: BurstDuration 에 Deactivate(방출 중단, 잔여 페이드) → 꼬리 뒤 DestroyComponent 로 확실히 회수.
			// 평타는 매 기여마다 떠서 길면 누적·뭉개짐 → 짧은 팝(0.3s). 크리는 드무니 길게(1.2s) 머무름.
			if (HitComp)
			{
				const float BurstDuration = bIsCritical ? 1.2f : 0.3f;
				const float FadeTail = 0.6f;
				TWeakObjectPtr<UNiagaraComponent> WeakComp(HitComp);
				FTimerManager& TimerMgr = GetWorld()->GetTimerManager();

				FTimerHandle DeactivateHandle;
				TimerMgr.SetTimer(DeactivateHandle, [WeakComp]()
				{
					if (WeakComp.IsValid()) { WeakComp->Deactivate(); }
				}, BurstDuration, false);

				FTimerHandle DestroyHandle;
				TimerMgr.SetTimer(DestroyHandle, [WeakComp]()
				{
					if (WeakComp.IsValid()) { WeakComp->DestroyComponent(); }
				}, BurstDuration + FadeTail, false);
			}
		}
	}

	// 사운드 — 한 액션 한 사운드. 짧고 굵은 percussive(클립 미배정이면 무음, 죽은 태그 아님).
	if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
	{
		SoundMgr->PlaySound(bIsCritical ? FName("Dev_Crit") : FName("Dev_ScoreHit"));
	}

	// 크리 전용 — 라이트 카메라 셰이크(기존 건물 착지 패턴 재사용, 작은 Scale 로 "쿵" 정도).
	if (bIsCritical && !USettingsManagerSubsystem::IsReduceMotion(this))
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			// 개발 크리티컬 = 가벼운 타격 파형 (착지 thud 축소판이 아닌 독립 응답). 스케일 1.0
			PC->ClientStartCameraShake(ULightImpactCameraShake::StaticClass(), 1.0f);
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
	UProgressBar* Bar = StripStageBars.IsValidIndex(CategoryIndex) ? StripStageBars[CategoryIndex] : nullptr;

	// 비파괴 팝 — RenderScale 로 톡 부풀렸다 0.18s 뒤 원복 (WBP 스타일 색은 안 건드림)
	if (Bar)
	{
		Bar->SetRenderScale(FVector2D(1.12f, 1.12f));
		TWeakObjectPtr<UProgressBar> WeakBar(Bar);
		if (UWorld* World = GetWorld())
		{
			FTimerHandle RevertHandle;
			World->GetTimerManager().SetTimer(RevertHandle, [WeakBar]()
			{
				if (WeakBar.IsValid()) { WeakBar->SetRenderScale(FVector2D(1.0f, 1.0f)); }
			}, 0.18f, false);
		}
	}

	// "달성!" 도장 — 셀 중앙 스크린스페이스 팝. 폰트는 WBP 소유(Val) 복사 (경로 하드코딩 금지)
	UWidget* Cell = StripStageCells.IsValidIndex(CategoryIndex) ? StripStageCells[CategoryIndex] : nullptr;
	UCommonTextBlock* FontSrc = StripStageVals.IsValidIndex(CategoryIndex) ? StripStageVals[CategoryIndex] : nullptr;
	if (ScoreOrbContainer && Cell && FontSrc)
	{
		const FGeometry CanvasGeo = ScoreOrbContainer->GetCanvasGeometry();
		const FGeometry& CellGeo = Cell->GetCachedGeometry();
		const FVector2D CenterLocal = CanvasGeo.AbsoluteToLocal(CellGeo.LocalToAbsolute(CellGeo.GetLocalSize() * 0.5f));

		FSlateFontInfo StampFont = FontSrc->GetFont();
		StampFont.Size = 40;
		ScoreOrbContainer->SpawnStampText(CenterLocal,
			NSLOCTEXT("StageProgress", "StageStamp", "달성!"),
			UGlobalUtilFunctions::GetStepColor(CategoryIndex + 1), StampFont);
	}

	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlaySound(FName("Dev_Milestone"));
		}
	}
}

void UOfficeMainWidget::PlayLaunchClimax()
{
	UWorld* World = GetWorld();
	UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
	if (!GI) return;

	UOfficeStageProgressManager* StageMgr = World->GetSubsystem<UOfficeStageProgressManager>();
	const EQualityGrade Grade = StageMgr ? StageMgr->GetCurrentQualityGrade() : EQualityGrade::C;

	// 등급 비례 강도 (C=1.0 ~ S=2.0)
	float Intensity = 1.0f;
	switch (Grade)
	{
	case EQualityGrade::S: Intensity = 2.0f; break;
	case EQualityGrade::A: Intensity = 1.6f; break;
	case EQualityGrade::B: Intensity = 1.3f; break;
	default: Intensity = 1.0f; break;
	}

	// 1) 좋은 결과(B 이상)만 전원 기립 환호 — 그 외는 매니저의 CheerSitting 을 그대로 둠(전 등급 기립 방지 + 매니저 호출 안 죽임)
	const bool bGoodResult = static_cast<uint8>(Grade) >= static_cast<uint8>(EQualityGrade::B);
	if (StageMgr && bGoodResult) { StageMgr->NotifyEmployeesCheerStandUp(); }

	// 2) 스크린 컨페티 팝퍼 — 축하 언어. KiAura(전투 오라)는 피버 전용으로 은퇴 (직원 부착 연출 기각, 2026-07-11)
	if (ScoreOrbContainer)
	{
		int32 Pieces = 24;
		float GoldRatio = 0.1f;
		switch (Grade)
		{
		case EQualityGrade::S: Pieces = 90; GoldRatio = 0.5f;  break;
		case EQualityGrade::A: Pieces = 60; GoldRatio = 0.35f; break;
		case EQualityGrade::B: Pieces = 40; GoldRatio = 0.2f;  break;
		default: break;
		}
		if (USettingsManagerSubsystem::IsReduceMotion(this)) Pieces /= 2;
		ScoreOrbContainer->PlayConfettiBurst(Pieces, GoldRatio);
	}

	// 3) 화면 플래시 — FeverScreenTint 재사용(흰빛 강하게 → 0으로 페이드). 등급 높을수록 강함.
	if (FeverScreenTint)
	{
		FeverOverlayAlpha = FMath::Clamp(0.45f + 0.2f * Intensity, 0.0f, 0.9f);
		FeverOverlayTargetAlpha = 0.0f;
		FeverScreenTint->SetColorAndOpacity(FLinearColor(1.0f, 0.96f, 0.85f, 1.0f));
		FeverScreenTint->SetRenderOpacity(FeverOverlayAlpha);
		FeverScreenTint->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!World->GetTimerManager().IsTimerActive(FeverOverlayTimerHandle))
		{
			World->GetTimerManager().SetTimer(FeverOverlayTimerHandle, this,
				&UOfficeMainWidget::TickFeverOverlay, 0.03f, true);
		}
	}

	// 4) 출시 사운드
	if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
	{
		SoundMgr->PlaySound(FName("Dev_LaunchClimax"));
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Launch climax (grade %s, intensity %.1f)"),
		*QualityGradeToAlphabetString(Grade), Intensity);
}

void UOfficeMainWidget::OnStepStartEffectReceived()
{
	// 새 개발 시작 — 마일스톤/escalate 1회성 플래그 리셋
	for (bool& bReached : bCategoryMilestoneReached) { bReached = false; }
	bLateStageEscalated = false;
	ResetLastSpurtVisual();

	ShowStepStartEffect(1);

	// NameBar를 모드별 "개발 중" 상태로 전환 + 버튼 숨김
	RefreshProjectNameBar();
}

void UOfficeMainWidget::UpdateTimerRingColor(float RemainingTime)
{
	if (!TimerRing) return;

	const float T = FMath::Clamp(1.0f - RemainingTime / TimerWarnSeconds, 0.0f, 1.0f);
	TimerRing->SetFillColor(FMath::Lerp(TimerRingGold, TimerRingRed, T));
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

	if (ULaunchReactionSubsystem* Reactions = GetWorld() ? GetWorld()->GetSubsystem<ULaunchReactionSubsystem>() : nullptr)
	{
		Reactions->OnReactionReady.AddUniqueDynamic(this, &UOfficeMainWidget::OnLaunchReactionReady);
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

	if (ULaunchReactionSubsystem* Reactions = GetWorld() ? GetWorld()->GetSubsystem<ULaunchReactionSubsystem>() : nullptr)
	{
		Reactions->OnReactionReady.RemoveAll(this);
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

	// 완료 안내 = 토스트만 (가운데 NiagaraTextEffect 는 결산서에 가려 안 보이고 과잉이라 제거 2026-07-08)
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (GI)
	{
		UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
		if (UIManager)
		{
			FText NotificationMessage = NSLOCTEXT("Operation", "OperationCompleteNotify", "프로젝트 운영이 완료되었습니다");
			UIManager->ShowNotification(NotificationMessage, 3.0f, ENotificationType::Success);
		}
	}

	if (UI_OfficeProjectCardCurrent)
	{
		UI_OfficeProjectCardCurrent->SwitchToStageMode();
	}

	ShowProjectReportModal(Data);
}

void UOfficeMainWidget::OnSpeedToggleClickedHandler()
{
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

void UOfficeMainWidget::ShowStepStartEffect(int32 StepNumber)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	// 1. BlindMaskSweep 위젯
	TSubclassOf<UUserWidget> BlindClass = TableMgr->GetWidgetClass(EWidgetType::BlindMaskSweep);
	if (BlindClass)
	{
		BlindMaskSweepWidget = CreateWidget<UUserWidget>(GetWorld(), BlindClass);
		if (BlindMaskSweepWidget)
		{
			BlindMaskSweepWidget->AddToViewport(999);

			if (UImage* SweepImage = Cast<UImage>(BlindMaskSweepWidget->GetWidgetFromName(TEXT("BlindMask_Sweep_2"))))
			{
				BlindMaskMID = SweepImage->GetDynamicMaterial();
				if (BlindMaskMID)
				{
					BlindMaskMID->SetScalarParameterValue(TEXT("Progress"), 0.0f);
					BlindMaskAnimElapsed = 0.0f;

					GetWorld()->GetTimerManager().SetTimer(
						BlindMaskAnimTimerHandle,
						this,
						&UOfficeMainWidget::UpdateBlindMaskAnimation,
						0.016f,
						true
					);
				}
			}
		}
	}

	// 2. TextGlitch 위젯
	TSubclassOf<UUserWidget> GlitchClass = TableMgr->GetWidgetClass(EWidgetType::TextGlitch);
	if (GlitchClass)
	{
		StepStartGlitchWidget = CreateWidget<UTextGlitchEffectWidget>(GetWorld(), GlitchClass);
		if (StepStartGlitchWidget)
		{
			StepStartGlitchWidget->SetTextFromString(TEXT("START!"));
			StepStartGlitchWidget->AddToViewport(1000);
		}
	}

	// 3. 2초 후 이펙트 제거
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StepEffectTimerHandle,
			this,
			&UOfficeMainWidget::RemoveStepStartEffect,
			2.0f,
			false
		);

		// BlindMaskSweep 동안 swoosh looping + sin envelope (가운데 max, 양 끝 0)
		if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
		{
			BlindSweepVariantIdx = 0;
			BlindSweepStartTime = World->GetTimeSeconds();
			TWeakObjectPtr<USoundManagerSubsystem> WeakMgr(SoundMgr);
			TWeakObjectPtr<UOfficeMainWidget> WeakSelf(this);
			World->GetTimerManager().SetTimer(BlindSweepSoundHandle, FTimerDelegate::CreateLambda(
				[WeakMgr, WeakSelf]()
				{
					if (!WeakMgr.IsValid() || !WeakSelf.IsValid()) return;
					UWorld* W = WeakSelf->GetWorld();
					if (!W) return;
					float Elapsed = W->GetTimeSeconds() - WeakSelf->BlindSweepStartTime;
					// 사운드는 visual(2.0s)보다 짧게 끝 — peak 도 visual 30% 시점으로 앞당김
					constexpr float SoundDuration = 1.2f;
					float T = FMath::Clamp(Elapsed / SoundDuration, 0.0f, 1.0f);
					float Envelope = FMath::Sin(PI * T);  // 0→1→0 bell curve, peak at 0.6s
					FName SoundID = FName(*FString::Printf(TEXT("BlindSweep_%d"), (WeakSelf->BlindSweepVariantIdx % 4) + 1));
					WeakMgr->PlaySoundWithVolume(SoundID, Envelope);
					WeakSelf->BlindSweepVariantIdx++;
				}), 0.12f, true);
		}
	}
}

void UOfficeMainWidget::RemoveStepStartEffect()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(BlindMaskAnimTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(BlindSweepSoundHandle);
	}
	BlindMaskMID = nullptr;

	if (BlindMaskSweepWidget)
	{
		BlindMaskSweepWidget->RemoveFromParent();
		BlindMaskSweepWidget = nullptr;
	}

	if (StepStartGlitchWidget)
	{
		StepStartGlitchWidget->RemoveFromParent();
		StepStartGlitchWidget = nullptr;
	}

	if (TimerRing)
	{
		// 색까지 되돌린다 — percent 만 리셋하면 직전 라운드가 레드로 끝난 경우
		// 첫 타이머 틱이 올 때까지 "빨간 꽉 찬 링" 이 남는다.
		TimerRing->SetPercent(1.0f);
		TimerRing->SetFillColor(TimerRingGold);
	}

	if (ProcessTimeText)
	{
		ProcessTimeText->SetText(FText::FromString(TEXT("15")));
	}
}

void UOfficeMainWidget::UpdateBlindMaskAnimation()
{
	BlindMaskAnimElapsed += 0.016f;
	float Progress = FMath::Clamp(BlindMaskAnimElapsed / BlindMaskAnimDuration, 0.0f, 1.0f);

	if (BlindMaskMID)
	{
		BlindMaskMID->SetScalarParameterValue(TEXT("Progress"), Progress);
	}

	if (Progress >= 1.0f)
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(BlindMaskAnimTimerHandle);
		}
	}
}

// ========== 출시 확인 모달 ==========

void UOfficeMainWidget::ShowLaunchConfirmModal()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) return;

	TSubclassOf<UUserWidget> LaunchConfirmClass = TableMgr->GetWidgetClass(EWidgetType::LaunchConfirm);
	if (!LaunchConfirmClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeMainWidget] LaunchConfirm widget not registered in DataTable!"));
		return;
	}

	UCommonActivatableWidget* PushedWidget = UIManager->GetUIBase()->PushPromptClass(LaunchConfirmClass.Get());
	ULaunchConfirmWidget* LaunchWidget = Cast<ULaunchConfirmWidget>(PushedWidget);
	if (LaunchWidget)
	{
		UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
		if (StageMgr)
		{
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

	// ⚠ StartLaunch 가 FSM 전이로 ProgressData 를 갱신/리셋할 수 있다 — 리빌에 필요한 값은 전이 전에 복사
	const FStageProgressData Snapshot = StageMgr->GetProgressData();
	const bool bHasReview = (Snapshot.ReviewScore > 0 && Snapshot.CriticScores.Num() >= 4);
	const EProjectLifecycle LifecycleBefore = StageMgr->GetLifecycle();

	StageMgr->StartLaunch();
	UpdateProjectCardProgress();
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] Launch confirmed"));

	// 확정 2회 탭 = StartLaunch no-op → 리빌 재push 방지
	if (StageMgr->GetLifecycle() == LifecycleBefore) { return; }

	if (!bHasReview) { return; }   // 수주/제조 = 리뷰·전리품 없음

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	ULaunchLootManagerSubsystem* LootMgr = GI ? GI->GetSubsystem<ULaunchLootManagerSubsystem>() : nullptr;

	FLaunchRewardRevealData Data;
	Data.ProjectName = FText::FromString(Snapshot.ProjectName);
	Data.ReviewScore = Snapshot.ReviewScore;
	Data.BandKey = ULaunchLootManagerSubsystem::ReviewScoreToTableKey(Snapshot.ReviewScore);
	Data.Loot = LootMgr ? LootMgr->GetLastLaunchLoot() : TArray<FMissionReward>();
	Data.bFirstDiscovery = Snapshot.bFirstDiscovery;
	ShowLaunchRewardReveal(Data);
}

void UOfficeMainWidget::ShowLaunchRewardReveal(const FLaunchRewardRevealData& Data)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) { return; }
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIManager || !UIManager->GetUIBase()) { return; }

	TSubclassOf<UUserWidget> RevealClass = TableMgr->GetWidgetClass(EWidgetType::LaunchRewardReveal);
	if (!RevealClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeMainWidget] LaunchRewardReveal widget not registered in DataTable! - 리빌 생략, 드립 즉시 시작"));
		OnLaunchRevealClosed();
		return;
	}

	ULaunchRewardRevealWidget* Reveal = Cast<ULaunchRewardRevealWidget>(UIManager->GetUIBase()->PushPromptClass(RevealClass.Get()));
	if (!Reveal)
	{
		OnLaunchRevealClosed();
		return;
	}
	// 프롬프트 스택 풀 재사용 대비 — 같은 인스턴스 재push 시 바인딩 누적 방지
	Reveal->OnRevealClosed.AddUniqueDynamic(this, &UOfficeMainWidget::OnLaunchRevealClosed);
	Reveal->Setup(Data);
}

void UOfficeMainWidget::OnLaunchRevealClosed()
{
	if (ULaunchReactionSubsystem* Reactions = GetWorld() ? GetWorld()->GetSubsystem<ULaunchReactionSubsystem>() : nullptr)
	{
		Reactions->BeginDrip(GetCurrentBuildingIDAsInt());
	}
}

void UOfficeMainWidget::OnLaunchReactionReady(int32 BuildingID, const FLaunchReaction& Reaction)
{
	if (!EventRail || BuildingID != GetCurrentBuildingIDAsInt()) { return; }   // 다른 건물/오피스 밖 = 소실(의도)

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> ToastClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::ReviewReactionToast) : nullptr;
	if (!ToastClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] ReviewReactionToast widget class not registered - 반응 생략"));
		return;
	}

	UReviewReactionToastWidget* Toast = CreateWidget<UReviewReactionToastWidget>(this, ToastClass);
	if (!Toast) { return; }

	UOfficeEventRailWidget* Rail = EventRail;
	Toast->OnRailRemoveRequested.BindUObject(Rail, &UOfficeEventRailWidget::RemoveEventCard, static_cast<UUserWidget*>(Toast));
	// ⚠ Setup 을 Add 보다 먼저 — 레일이 가득 차 있으면 AddTransientCard 가 이 토스트를 즉시 트림할 수 있다
	Toast->SetupReaction(Reaction);
	Rail->AddTransientCard(Toast);
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
	if (!StageMgr || !StageMgr->TryRetryDevelopment())
	{
		// 게이트/잔액 미충족 — 상태를 바꾸지 않았으므로 결과 모달이 다시 뜨도록 되돌린다
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] 컨티뉴 실패 - 결과 모달 재표시"));

		// 게이트를 통과했는데 실패했다면 남은 원인은 잔액뿐 — 모달만 되살리면 왜 안 됐는지 알 길이 없다.
		if (StageMgr && StageMgr->CanRetryDevelopment())
		{
			if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
			{
				UIMgr->NotifyInsufficientResource(EResourceType::Diamond, StageMgr->GetRetryDiamondCost());
			}
		}

		ShowLaunchConfirmModal();
		return;
	}

	RefreshLeftSlotState();
	UpdateProjectCardProgress();
	UE_LOG(LogTemp, Log, TEXT("[OfficeMainWidget] 컨티뉴 - 개발 재개"));
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
	StripStageCells.Reset();
	StripStageNames.Reset();
	StripStageVals.Reset();
	StripStageBars.Reset();
	StripStageDoneBadges.Reset();
	for (int32 i = 1; i <= 6; ++i)
	{
		StripStageCells.Add(GetWidgetFromName(*FString::Printf(TEXT("Stage%dCell"), i)));
		StripStageNames.Add(Cast<UCommonTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dName"), i))));
		StripStageVals.Add(Cast<UCommonTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dVal"), i))));
		StripStageBars.Add(Cast<UProgressBar>(GetWidgetFromName(*FString::Printf(TEXT("Stage%dBar"), i))));
		StripStageDoneBadges.Add(GetWidgetFromName(*FString::Printf(TEXT("Stage%dDone"), i)));
	}

	// 값은 바 위에 겹친 labeled meter — 중앙 정렬이라 펀치 피봇도 중앙(정렬 기준점 일치, 좌우 대칭 확장).
	for (UCommonTextBlock* ValT : StripStageVals)
	{
		if (!ValT) continue;
		ValT->SetJustification(ETextJustify::Center);
		ValT->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	StripRow = GetWidgetFromName(TEXT("StageRow"));
	if (!StripRow)
	{
		// StageRow 이름 없으면 M10 WatchStrip 하이라이트 타겟이 조용히 null이 되어 관찰 4초가 무연출로 지나간다 — 여기서만 1회 로그.
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] StageRow 위젯을 찾을 수 없습니다 ― M10 스트립 관찰 가이드의 하이라이트 타겟이 사라집니다"));
	}

	// 배지 이름은 외부 스크립트가 만드는 유일한 캐시라 트리를 재paste 하면 6개가 통째로 사라지고
	// ApplyStripStageVisual 의 토글이 조용히 no-op 이 된다(육안으로는 "달성해도 배지가 없다"로만 보인다) ― 여기서만 1회 로그.
	const bool bAnyDoneBadge = StripStageDoneBadges.ContainsByPredicate([](const UWidget* W) { return W != nullptr; });
	if (!bAnyDoneBadge)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeMainWidget] Stage{1..6}Done 배지를 하나도 찾을 수 없습니다 ― 달성 배지가 표시되지 않습니다. Tools/StageBadge/inject_stage_done.py 재실행 필요"));
	}

	// 썸네일 프레임/크기 스타일은 WBP 소유 (슬림 키라인 트리 2026-07-11 paste) — C++ 스타일링 금지 규칙
}

UWidget* UOfficeMainWidget::GetStripStepBar(int32 StepNumber) const
{
	const int32 Idx = StepNumber - 1;
	return StripStageBars.IsValidIndex(Idx) ? StripStageBars[Idx] : nullptr;
}

UWidget* UOfficeMainWidget::GetStripRowWidget() const
{
	return StripRow;
}

UWidget* UOfficeMainWidget::GetStripSpeedButtonWidget() const
{
	return StripSpeedButton;
}

void UOfficeMainWidget::PopulateStripIdentity()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
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
	FString GradeStr;
	if (FOperationData* Op = GetCurrentBuildingOperation())
	{
		if (!Op->ProjectName.IsEmpty()) NameText = FText::FromString(Op->ProjectName);
		GradeStr = QualityGradeToAlphabetString(Op->QualityGrade);
	}

	if (DevProjectName) DevProjectName->SetText(NameText);
	if (OpProjectName)  OpProjectName->SetText(NameText);

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
	// 커버는 출시 후(운영)만 공개 — 개발 중 DevThumb 은 "?" 미스터리 유지 (스포일러 방지, 사용자 확정 2026-07-11)
	// 인셋 2px 기준 내부 크기 — WBP OpThumb SizeBox 160x120 과 계약
	ApplyThumbCover(OpThumbInner, OpThumbQ, 156.0f, 116.0f);

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

	// 등급 배지(운영) — 등급 없으면 Collapsed
	if (OpGradeBadgeBox)
	{
		if (!GradeStr.IsEmpty() && GradeStr != TEXT("-"))
		{
			if (OpGradeBadge) OpGradeBadge->SetText(FText::FromString(GradeStr));
			OpGradeBadgeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			OpGradeBadgeBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UOfficeMainWidget::UpdateStripStageCells()
{
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

		// 상태 전환/리프레시 = 스냅 (라운드 진행이 아니므로 애니메이션 없이 즉시). 이후 라이브 점수는 NativeTick 이 lerp.
		StripStageTargetScore[i] = Step.TargetScore;
		StripStagePendingPct[i] = Pct;
		bStripStagePendingDirty[i] = false;
		StripStageFlashAlpha[i] = 0.0f;
		StripStagePunchElapsed[i] = 99.0f;
		if (UCommonTextBlock* PunchValT = StripStageVals.IsValidIndex(i) ? StripStageVals[i] : nullptr)
		{
			PunchValT->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
		StripStageShownPct[i] = Pct;
		StripStageTargetPct[i] = Pct;
		ApplyStripStageVisual(i, Pct);

		// 폭 예약 없음 — 값 줄이 셀 전폭(WellBox 170)을 채우고 우측 정렬하므로 자릿수가 늘어도 폭이 안 흔들린다.
		// 크기 권위는 WellBox 하나 (카탈로그 공통 마감 §8). 여기서 MinDesiredWidth 를 주장하면
		// 칸이 평균폭보다 넓어지길 요구하게 되고, StageRow 는 Fill 균등분배라 그만큼 이름이 잘린다.
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

	// 마일스톤 — 바가 시각적으로 100%에 닿는 순간(착지/폴백)과 일치하도록 이 시점에서 판정
	if (!bCategoryMilestoneReached[Index]
		&& StripStageTargetScore[Index] > 0.0f && IsStageGoalMet(Pct))
	{
		bCategoryMilestoneReached[Index] = true;
		PlayCategoryMilestone(Index);
	}
	return true;
}

void UOfficeMainWidget::UpdateStripShellMaterialSize()
{
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

	StripStagePunchElapsed[Index] = 0.0f;
	StripStagePunchPeak[Index] = bIsCritical ? 1.2f : 1.12f;
	// 크리 착지 = 골드 플래시로 차별화 (일반 = 화이트)
	StripStageFlashColor[Index] = bIsCritical
		? FLinearColor(0.791298f, 0.421407f, 0.086500f, 1.0f) : FLinearColor::White;
	StripStageFlashAlpha[Index] = 1.0f;
}

void UOfficeMainWidget::ApplyStripStageVisual(int32 Index, float Pct)
{
	if (Index < 0 || Index >= 6) return;

	static const FLinearColor BarBlue(0.046665f, 0.327778f, 0.745404f, 1.0f);
	// 이 그린은 실제로 3벌이다 ― 여기 + Tools/StageBadge/inject_stage_done.py 의 GR/GG/GB + 그 스크립트가
	// .uasset 에 베이크한 배지 브러시. 여기만 고치면 배지가 조용히 갈라진다(스크립트 재실행이 있어야 따라온다).
	static const FLinearColor BarGreen(0.104616f, 0.630757f, 0.254152f, 1.0f);
	static const FLinearColor BarGold(0.887923f, 0.533276f, 0.076185f, 1.0f);
	// 과락 미달 레드 — 결과 모달 분야 행(UStatRowWidget::GetProgressBarColor)이 쓰는 값 그대로. 두 화면의 사다리를 맞춘다.
	static const FLinearColor BarRed(0.9f, 0.25f, 0.2f, 1.0f);
	// 값 글자는 바 위에 겹치므로 상태색을 쓰지 않는다 — 블루 채움 위 블루 글자가 되고, 상태는 바 채움색이 이미 말한다.
	static const FLinearColor ValInk(1.0f, 0.941f, 0.871f, 1.0f);   // #FFF8F0 — 컬러 면 위 라벨 표준(카탈로그 §6)

	const bool bOver = Pct > 1.001f;        // 목표 초과(오버필) → 골드 강조
	const bool bFull = IsStageGoalMet(Pct); // 목표 달성 → 그린 (배지와 같은 문턱)
	// 과락 = 이 분야 하나만 미달해도 출시가 막힌다(MeetsMinimumClearScore 는 AND). 문턱은 게이트와 같은 상수를 쓴다.
	const bool bBelowMin = Pct < FStageProgressData::MinimumScoreRatio;

	const FLinearColor BaseBarCol = bOver ? BarGold : (bFull ? BarGreen : (bBelowMin ? BarRed : BarBlue));
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
		// "획득 / 목표" — 목표가 프로젝트별로 30~38만까지 벌어져 획득 숫자만으로는 진척을 읽을 수 없다.
		// 축약은 WesternSuffix(K/M) 고정 — 이 셀에 꽂히는 orb 가 같은 표기라 단위계가 갈리면 안 된다(카탈로그 §9 스코어 예외).
		// Pct 미클램프라 목표 초과분도 그대로 상승하고, 그때는 색(골드)이 초과를 알린다.
		const float Tgt = StripStageTargetScore[Index];
		ValT->SetText(FText::FromString(FString::Printf(TEXT("%s / %s"),
			*UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(FMath::RoundToInt(Pct * Tgt)),
				ENumberAbbrevStyle::WesternSuffix, ENumberRoundMode::Floor).ToString(),
			*UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(Tgt),
				ENumberAbbrevStyle::WesternSuffix, ENumberRoundMode::Floor).ToString())));
		ValT->SetColorAndOpacity(FSlateColor(ValInk));
	}
	if (UCommonTextBlock* NameT = StripStageNames.IsValidIndex(Index) ? StripStageNames[Index] : nullptr)
	{
		// 이름 라벨 = 직능 슬롯 위치색(1파랑/2초록/3주황/4보라/5청록/6로즈 ― orb·+N 숫자와 단일 출처 GetStepColor).
		// 진행 상태색(바 fill)과 분리 → 색=직능 정체성, 바=진행도로 역할 분담.
		NameT->SetColorAndOpacity(FSlateColor(UGlobalUtilFunctions::GetStepColor(Index + 1)));
	}
	// 달성 배지 ― 도장(1회성 팝)이 사라진 뒤에도 남는 상태 표시. 오버필이어도 그린 유지(초과는 바 색이 말한다).
	// HitTestInvisible(자기+자식 전부): 스트립은 상시 HUD 라 배지가 입력을 먹으면 안 된다.
	// SelfHitTestInvisible 로는 부족하다 ― 자식 Stage{N}DoneIcon(UImage)은 생성자에서 가시성을 안 건드려
	// UWidget 기본값 Visible 로 남는다(HorizontalBox/SizeBox/CommonTextBlock 은 생성자가 SelfHitTestInvisible 을 건다).
	if (UWidget* Done = StripStageDoneBadges.IsValidIndex(Index) ? StripStageDoneBadges[Index] : nullptr)
	{
		Done->SetVisibility(bFull ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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

	// 등급 배지도 운영 중 실시간 갱신 (RefreshOfficeUI 밖 per-tick 경로)
	if (OpGradeBadge)
	{
		OpGradeBadge->SetText(FText::FromString(QualityGradeToAlphabetString(Data.QualityGrade)));
	}
	if (OpGradeBadgeBox)
	{
		OpGradeBadgeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// 운영 수명 카운트다운 — "운영 중" 배지(OpModeBadgeText)를 남은시간으로 재사용 (GetWidgetFromName, 신규 위젯/바인딩 불필요)
	if (UCommonTextBlock* ModeBadge = Cast<UCommonTextBlock>(GetWidgetFromName(TEXT("OpModeBadgeText"))))
	{
		const int32 Rem = FMath::Max(0, FMath::CeilToInt(Data.RemainingTime));
		ModeBadge->SetText(FText::FromString(FString::Printf(TEXT("종료 %d:%02d"), Rem / 60, Rem % 60)));
		// 15초 이하 = 종료 임박 → 골드 경고
		static const FLinearColor OpGreen(0.327778f, 0.806952f, 0.508881f, 1.0f);
		static const FLinearColor OpWarn(0.921582f, 0.679542f, 0.270498f, 1.0f);
		ModeBadge->SetColorAndOpacity(FSlateColor(Rem <= 15 ? OpWarn : OpGreen));
	}
}
