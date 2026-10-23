#include "UI/Panel/OfficeEmployeePanelWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Types/ReflectionMetadata.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Core/CGGameInstance.h"
#include "Data/EntityCardData.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Enum/NotificationType.h"
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "GameMode/OfficeGameMode.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Player/MainMapPlayerController.h"
#include "Player/OfficeCameraPawn.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "Utils/FWidgetAnimationUtils.h"

namespace OfficeEmployeePanel
{
	// 팔 길이 — 카메라 거리 ≈ 팔 + 약 1450(리그 소켓 후퇴)이라 팔로는 직원 키가 화면 ~28% 가 상한이다.
	// 0 = 줌 리그 최소 팔(현재 20)로 클램프. 40% 목표는 FOV·소켓 쪽 조정이 필요하다(A2b 실측)
	constexpr float EmployeeFocusArmLengthCm = 0.f;
	// 판 지오메트리·세운 직원의 착지를 기다리는 최대 틱. 넘으면 그 상태로(지오메트리가 없으면 화면 가운데로) 잡는다
	constexpr int32 MaxCameraFramingAttempts = 60;
	// 새로 앉힌 직원의 행동 초기화(스폰 0.1초 뒤 자리 텔레포트)가 끝난 뒤 세워야 덮어쓰이지 않는다
	constexpr float SeatRefocusDelaySeconds = 0.3f;
	// 직원이 바라볼 카메라 쪽 지점 거리 — 멀수록 이동 후 위치 오차가 방향에 덜 섞인다
	constexpr float FacingProbeDistanceCm = 100000.f;
	// 창 크기가 바뀐 뒤 판 지오메트리가 새 레이아웃으로 갱신될 때까지 기다리는 틱
	constexpr int32 ViewportReframeDelayTicks = 3;
	// 목록 모달 등장 — 시작 위치는 화면 밖(왼쪽 여백 28 + 판 372 + 그림자 40), 승인 목업 .2s cubic-bezier(.2,.8,.2,1)
	constexpr float RailSlideDistance = 440.f;
	constexpr float RailSlideDuration = 0.2f;
	constexpr float RailSlideMaxStep = 1.f / 30.f;
	// 스택 페이드가 끝나 패널이 다 보일 때(물려받은 불투명도) 시작한다. 못 재면 이만큼 기다린 뒤 시작
	constexpr float RailSlideVisibleOpacity = 0.98f;
	constexpr float RailSlideMaxWait = 1.5f;

	// CSS cubic-bezier(.2,.8,.2,1) 의 y(x). x(t) 가 단조라 이분법으로 t 를 찾는다
	static float RailSlideEaseOut(float X)
	{
		auto Bezier = [](float T, float P1, float P2)
		{
			const float U = 1.f - T;
			return 3.f * U * U * T * P1 + 3.f * U * T * T * P2 + T * T * T;
		};
		float Lo = 0.f;
		float Hi = 1.f;
		for (int32 Iter = 0; Iter < 24; ++Iter)
		{
			const float Mid = 0.5f * (Lo + Hi);
			(Bezier(Mid, 0.2f, 0.2f) < X ? Lo : Hi) = Mid;
		}
		return Bezier(0.5f * (Lo + Hi), 0.8f, 1.f);
	}
}

UOfficeEmployeePanelWidget::UOfficeEmployeePanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 뒤로가기(ESC/안드로이드 Back)로 닫혀야 한다
	bIsBackHandler = true;
}

// ===== 진입 헬퍼 =====

namespace OfficeEmployeePanel
{
	UClass* ResolvePanelClass(UTableManagerSubsystem* TableMgr)
	{
		const TSubclassOf<UUserWidget> PanelClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::OfficeEmployeePanel) : nullptr;
		return PanelClass && PanelClass->IsChildOf(UOfficeEmployeePanelWidget::StaticClass()) ? PanelClass.Get() : nullptr;
	}
}

bool UOfficeEmployeePanelWidget::IsPanelClassRegistered(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return OfficeEmployeePanel::ResolvePanelClass(GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr) != nullptr;
}

UOfficeEmployeePanelWidget* UOfficeEmployeePanelWidget::PushForEmployee(const UObject* WorldContextObject, int32 EmployeeId)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UCGGameInstance* GI = World ? Cast<UCGGameInstance>(World->GetGameInstance()) : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	if (!TableMgr || !UIBase)
	{
		return nullptr;
	}

	UClass* PanelClass = OfficeEmployeePanel::ResolvePanelClass(TableMgr);
	if (!PanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] OfficeEmployeePanel 클래스 미등록 — DT_WidgetClass 확인, 구 경로로 폴백"));
		return nullptr;
	}

	UOfficeEmployeePanelWidget* Panel = Cast<UOfficeEmployeePanelWidget>(UIBase->PushBottomClass(PanelClass));
	if (!Panel)
	{
		return nullptr;
	}
	Panel->OpenForEmployee(EmployeeId);

	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
	{
		PC->GoToUIMode();
	}
	return Panel;
}

UOfficeEmployeePanelWidget* UOfficeEmployeePanelWidget::OpenPanelForEmployee(const UObject* WorldContextObject, int32 EmployeeId)
{
	return PushForEmployee(WorldContextObject, EmployeeId);
}

void UOfficeEmployeePanelWidget::SortRosterByTodo(const UEmployeeManager& InEmployeeManager, TArray<FEmployeeInstance>& InOutRoster)
{
	TSet<int32> ReadyIds;
	for (const FEmployeeInstance& Employee : InOutRoster)
	{
		if (InEmployeeManager.CanLevelUpEmployee(Employee.EmployeeID))
		{
			ReadyIds.Add(Employee.EmployeeID);
		}
	}
	InOutRoster.StableSort([&ReadyIds](const FEmployeeInstance& A, const FEmployeeInstance& B)
	{
		const bool bReadyA = ReadyIds.Contains(A.EmployeeID);
		const bool bReadyB = ReadyIds.Contains(B.EmployeeID);
		if (bReadyA != bReadyB)
		{
			return bReadyA;
		}
		return A.Level > B.Level;
	});
}

bool UOfficeEmployeePanelWidget::TryGetGapCenterRatioX(const FSlateRect& ViewportRect, float LeftPanelRight, float RightPanelLeft, float& OutRatio)
{
	const float ViewportWidth = ViewportRect.Right - ViewportRect.Left;
	if (!FMath::IsFinite(ViewportRect.Left) || !FMath::IsFinite(ViewportRect.Right)
		|| !FMath::IsFinite(LeftPanelRight) || !FMath::IsFinite(RightPanelLeft) || ViewportWidth <= 0.f)
	{
		return false;
	}
	const float GapLeft = FMath::Clamp(LeftPanelRight, ViewportRect.Left, ViewportRect.Right);
	const float GapRight = FMath::Clamp(RightPanelLeft, ViewportRect.Left, ViewportRect.Right);
	if (GapRight <= GapLeft)
	{
		return false;
	}
	OutRatio = ((GapLeft + GapRight) * 0.5f - ViewportRect.Left) / ViewportWidth;
	return FMath::IsFinite(OutRatio);
}

// ===== 수명 주기 =====

void UOfficeEmployeePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	}

	if (PrevButton) PrevButton->OnClicked().AddUObject(this, &ThisClass::HandlePrevClicked);
	if (NextButton) NextButton->OnClicked().AddUObject(this, &ThisClass::HandleNextClicked);
	if (FireButton) FireButton->OnClicked().AddUObject(this, &ThisClass::HandleFireClicked);
	if (EnhanceButton) EnhanceButton->OnClicked().AddUObject(this, &ThisClass::HandleEnhanceClicked);
	if (BenchSeatButton) BenchSeatButton->OnClicked().AddUObject(this, &ThisClass::HandleBenchSeatClicked);
	if (CloseButton) CloseButton->OnCloseClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	if (GapCloseButton)
	{
		// 카메라를 끌다 놓은 것은 닫기가 아니다 — 끌기 거리를 넘으면 눌림이 풀린다
		GapCloseButton->SetClickMethod(EButtonClickMethod::PreciseClick);
		GapCloseButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		GapCloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleGapClicked);
	}

	BindRosterControls();

	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);
	for (UWidget* TabWidget : { TabGrowthButton.Get(), TabPotentialButton.Get(), TabEquipmentButton.Get() })
	{
		UTabButtonWidget* FolderTab = Cast<UTabButtonWidget>(TabWidget);
		if (UCommonButtonBase* Tab = FolderTab ? FolderTab->GetButton() : Cast<UCommonButtonBase>(TabWidget))
		{
			TabButtonGroup->AddWidget(Tab);
			Tab->SetIsSelectable(true);
		}
	}
	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &ThisClass::HandleTabSelectionChanged);
	TabButtonGroup->SelectButtonAtIndex(0, /*bAllowSound*/ false);
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(0);
	}
	BuildGrowthRows();
	BindEquipmentControls();
}

void UOfficeEmployeePanelWidget::NativeDestruct()
{
	if (PrevButton) PrevButton->OnClicked().RemoveAll(this);
	if (NextButton) NextButton->OnClicked().RemoveAll(this);
	if (FireButton) FireButton->OnClicked().RemoveAll(this);
	if (EnhanceButton) EnhanceButton->OnClicked().RemoveAll(this);
	if (BenchSeatButton) BenchSeatButton->OnClicked().RemoveAll(this);
	if (CloseButton) CloseButton->OnCloseClicked.RemoveAll(this);
	if (GapCloseButton) GapCloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleGapClicked);
	UnbindRosterControls();
	UnbindEquipmentControls();

	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		// 선택 필수인 채로 비우면 엔진이 남은 버튼을 재선택한다
		TabButtonGroup->SetSelectionRequired(false);
		TabButtonGroup->RemoveAll();
		TabButtonGroup = nullptr;
	}

	// 비활성화 없이 해제되는 경로(레벨 전환) 대비 — 직원을 선 채로 두지 않는다 (미리 입혀 보기도 먼저 걷는다)
	CloseEquipDialog();
	CloseEquipDetail();
	EndEquipMoment();
	EndWorldFocus();
	// 뷰포트 오버레이·카드도 같은 이유로 걷는다
	CloseOddsTable();
	CloseEnhanceCard();
	EndRailSlideIn();

	Super::NativeDestruct();
}

void UOfficeEmployeePanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	BeginRailSlideIn();

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeRosterChanged.AddUObject(this, &ThisClass::HandleRosterChanged);
		EmployeeManager->OnEmployeeStatsChanged.AddUObject(this, &ThisClass::HandleEmployeeStatsChanged);
		EmployeeManager->OnEmployeeLevelUp.AddUObject(this, &ThisClass::HandleEmployeeLevelUp);
		EmployeeManager->OnEmployeeExperienceChanged.AddUObject(this, &ThisClass::HandleEmployeeExperienceChanged);
		EmployeeManager->OnEmployeePortraitUpdated.AddUObject(this, &ThisClass::HandleEmployeePortraitUpdated);
	}
	if (UResourceItemManager* ResourceMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr)
	{
		ResourceMgr->OnResourceChanged.AddUObject(this, &ThisClass::HandleResourceChanged);
	}
	if (UEmployeeEquipmentManagerSubsystem* EquipmentMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr)
	{
		EquipmentMgr->OnEquipmentChanged.AddUObject(this, &ThisClass::HandleEquipmentChanged);
	}

	SetOfficeHUDSuppressed(true);
	SyncFooterForTab();
	if (AOfficeCameraPawn* Camera = GetOfficeCamera())
	{
		SavedCameraView = Camera->CaptureViewState();
	}
	if (auto* PC = Cast<AMainMapPlayerController>(GetOwningPlayer()))
	{
		PC->GoToUIMode();
	}
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->NotifyOfficeEmployeePanelOpened(this);
	}

	PendingSelectAfterFireId = INDEX_NONE;
	LastViewportSize = FVector2D::ZeroVector;
	ViewportReframeDelayTicks = 0;
	RefreshWallet();
	ResetRosterView();
	RefreshRoster();

	if (PendingOpenEmployeeId != INDEX_NONE)
	{
		const int32 OpenId = PendingOpenEmployeeId;
		PendingOpenEmployeeId = INDEX_NONE;
		SelectEmployee(OpenId);
	}
	else
	{
		bNeedsDefaultSelection = true;
	}
}

void UOfficeEmployeePanelWidget::NativeOnDeactivated()
{
	EndRailSlideIn();
	bNeedsDefaultSelection = false;
	PendingOpenEmployeeId = INDEX_NONE;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SeatRefocusTimer);
	}

	// 미리 입혀 보기는 직원 살펴보기가 끝나기 전에 걷는다
	CloseEquipDialog();
	CloseEquipDetail();
	EndEquipMoment();
	SetWorldSlotsShown(false);
	HideHeadChevron();
	EndWorldFocus();
	SuspendPotentialWorkspace();
	CloseOddsTable();
	CloseEnhanceCard();
	if (AOfficeCameraPawn* Camera = GetOfficeCamera())
	{
		Camera->StopFollowingEmployee();
		Camera->ClearFocusTarget();
		Camera->RestoreViewState(SavedCameraView);
	}
	SavedCameraView = FOfficeCameraViewState();
	SetOfficeHUDSuppressed(false);

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeRosterChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeStatsChanged.RemoveAll(this);
		EmployeeManager->OnEmployeeLevelUp.RemoveAll(this);
		EmployeeManager->OnEmployeeExperienceChanged.RemoveAll(this);
		EmployeeManager->OnEmployeePortraitUpdated.RemoveAll(this);
	}
	if (UResourceItemManager* ResourceMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr)
	{
		ResourceMgr->OnResourceChanged.RemoveAll(this);
	}
	if (UEmployeeEquipmentManagerSubsystem* EquipmentMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr)
	{
		EquipmentMgr->OnEquipmentChanged.RemoveAll(this);
	}

	if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
	{
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}

	Super::NativeOnDeactivated();
}

void UOfficeEmployeePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// 이번 프레임 배치에 쓰인 패널 등장 오프셋 — 베이스 트윈이 Super 에서 다음 프레임 값으로 바꾸기 전에 읽는다
	const FVector2D PanelOffsetThisFrame = GetRenderTransform().Translation;
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickRailSlideIn(InDeltaTime, PanelOffsetThisFrame);

	if (bNeedsDefaultSelection)
	{
		bNeedsDefaultSelection = false;
		const int32 DefaultId = RosterOrder.Contains(SelectedEmployeeId)
			? SelectedEmployeeId : (RosterOrder.Num() > 0 ? RosterOrder[0] : INDEX_NONE);
		if (DefaultId != INDEX_NONE)
		{
			SelectEmployee(DefaultId);
		}
	}
	if (bCameraFramingPending)
	{
		TryApplyCameraFraming();
	}
	if (bBenchLensShiftPending)
	{
		TryApplyBenchLensShift();
	}
	TickViewportReframe();
	TickEquipmentWorld(InDeltaTime);
	// 장착 순간 여부가 이번 틱에 정해진 뒤에 판정한다
	TickHeadChevron(InDeltaTime);
}

bool UOfficeEmployeePanelWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	// 위에 뜬 것부터 하나씩 닫는다 — 확률표 → 장비 강화 다이얼로그 → 장비 상세·★ 강화 카드 → 패널
	if (CloseOddsTable() || CloseEquipDialog() || CloseEquipDetail() || CloseEnhanceCard())
	{
		return true;
	}
	if (!IsCloseRequested())
	{
		CloseWithAnimation();
	}
	return true;
}

TOptional<FUIInputConfig> UOfficeEmployeePanelWidget::GetDesiredInputConfig() const
{
	// 게임 입력을 막아 카메라 드래그·줌과 다른 3D 직원 탭을 끈다. 커서는 잡지 않는다 — 레일·패널은 눌려야 한다
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

// ===== 공개 API =====

void UOfficeEmployeePanelWidget::OpenForEmployee(int32 EmployeeId)
{
	if (!IsActivated())
	{
		PendingOpenEmployeeId = EmployeeId;
		return;
	}
	bNeedsDefaultSelection = false;
	if (EmployeeManager && EmployeeManager->FindEmployee(EmployeeId))
	{
		SelectEmployee(EmployeeId);
	}
	else if (RosterOrder.Num() > 0)
	{
		SelectEmployee(RosterOrder[0]);
	}
}

void UOfficeEmployeePanelWidget::SelectEmployee(int32 EmployeeId)
{
	if (!EmployeeManager || !EmployeeManager->FindEmployee(EmployeeId))
	{
		return;
	}
	bNeedsDefaultSelection = false;
	const bool bChanged = SelectedEmployeeId != EmployeeId;
	SelectedEmployeeId = EmployeeId;

	RefreshHeader();
	RefreshGrowth();
	SyncPotentialWorkspace();
	SyncEnhanceCard();
	SyncEquipmentTab(bChanged);
	SyncRosterSelection(/*bScrollIntoView*/ bChanged);
	if (bChanged || !FocusWorker.IsValid())
	{
		ApplyWorldFocus();
	}
}

UWidget* UOfficeEmployeePanelWidget::GetEnhanceButtonWidget() const
{
	if (!IsActivated() || SelectedEmployeeId == INDEX_NONE)
	{
		return nullptr;
	}
	// 푸터는 성장 탭에만 있다 — 다른 탭이면 안내가 「성장」 탭을 먼저 가리키게 한다
	const bool bGrowth = !TabSwitcher || TabSwitcher->GetActiveWidgetIndex() == 0;
	return bGrowth ? EnhanceButton.Get() : TabGrowthButton.Get();
}

// ===== 표시 =====

void UOfficeEmployeePanelWidget::RefreshHeader()
{
	const FEmployeeInstance* Employee = (EmployeeManager && SelectedEmployeeId != INDEX_NONE)
		? EmployeeManager->FindEmployee(SelectedEmployeeId) : nullptr;

	if (NameText)
	{
		NameText->SetText(Employee ? FText::FromString(Employee->EmployeeName) : FText::GetEmpty());
	}
	if (RankText)
	{
		RankText->SetText(Employee
			? FText::FromString(UEmployeeTypeHelper::GetJobTitleDisplayName(UEmployeeTypeHelper::GetJobTitleFromLevel(Employee->Level)))
			: FText::GetEmpty());
	}
	if (StarText)
	{
		StarText->SetText(Employee
			? FText::Format(NSLOCTEXT("OfficeEmployeePanel", "StarLevel", "★ {0}"), FText::AsNumber(Employee->EnhancementLevel))
			: FText::GetEmpty());
	}

	// 주 직능 = 직원창 직능 칩과 같은 argmax, 표시명 SOT = DT_DisciplineDisplay (산업별 재해석)
	FText DisciplineName;
	if (Employee)
	{
		const int32 BestIdx = GetPrimaryDisciplineSlot(*Employee);
		UCGGameInstance* CGGI = Cast<UCGGameInstance>(GetGameInstance());
		UTableManagerSubsystem* TableMgr = CGGI ? CGGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (BestIdx != INDEX_NONE && TableMgr)
		{
			DisciplineName = TableMgr->GetDisciplineDisplayName(CGGI->GetCurrentBuildingCompanyType(), BestIdx);
		}
	}
	const ESlateVisibility DisciplineVisibility = DisciplineName.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	if (DeptText)
	{
		DeptText->SetText(DisciplineName);
		DeptText->SetVisibility(DisciplineVisibility);
	}
	if (DeptIcon)
	{
		DeptIcon->SetVisibility(DisciplineVisibility);
	}
	if (DeptSeparator)
	{
		DeptSeparator->SetVisibility(DisciplineVisibility);
	}

	const bool bCanCycle = RosterOrder.Num() > 1;
	if (PrevButton) PrevButton->SetIsEnabled(bCanCycle);
	if (NextButton) NextButton->SetIsEnabled(bCanCycle);
	if (FireButton) FireButton->SetIsEnabled(Employee != nullptr);
	if (EnhanceButton) EnhanceButton->SetIsEnabled(Employee != nullptr);
}

void UOfficeEmployeePanelWidget::RefreshWallet()
{
	UResourceItemManager* ResourceMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!ResourceMgr)
	{
		return;
	}
	if (WalletMoneyChip)
	{
		WalletMoneyChip->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Money));
	}
	if (WalletDiamondChip)
	{
		WalletDiamondChip->SetValue(ResourceMgr->GetResourceAmount(EResourceType::Diamond));
	}
}

// ===== 월드 포커스 (카메라 + 직원 살펴보기) =====

void UOfficeEmployeePanelWidget::ApplyWorldFocus()
{
	AOfficeworker* Worker = FindWorkerActor(SelectedEmployeeId);
	if (!Worker)
	{
		ShowBenchView();
		return;
	}

	if (BenchNotice)
	{
		BenchNotice->SetVisibility(ESlateVisibility::Collapsed);
	}
	bBenchLensShiftPending = false;
	FocusWorker = Worker;

	FVector ViewLocation = Worker->GetActorLocation() - Worker->GetActorForwardVector() * OfficeEmployeePanel::FacingProbeDistanceCm;
	if (const AOfficeCameraPawn* Camera = GetOfficeCamera())
	{
		// 시선 축(리그 요)의 반대쪽 = 글라이드가 끝난 뒤 카메라가 있을 방향
		const FVector ViewForward = FRotator(0.f, Camera->GetActorRotation().Yaw, 0.f).Vector();
		ViewLocation = Worker->GetActorLocation() - ViewForward * OfficeEmployeePanel::FacingProbeDistanceCm;
	}
	ResetWorkerRotation();
	Inspect.Begin(Worker, ViewLocation);
	CaptureRotateBase(Worker);

	bCameraFramingPending = true;
	CameraFramingAttempts = 0;
	TryApplyCameraFraming();
}

void UOfficeEmployeePanelWidget::ShowBenchView()
{
	ResetWorkerRotation();
	Inspect.End();
	FocusWorker.Reset();
	bCameraFramingPending = false;
	if (BenchNotice)
	{
		// 판을 눌러도 뒤의 빈칸 닫기 버튼으로 새지 않게 카드가 히트를 받는다
		BenchNotice->SetVisibility(ESlateVisibility::Visible);
	}
	if (AOfficeCameraPawn* Camera = GetOfficeCamera())
	{
		Camera->StopFollowingEmployee();
		Camera->ClearFocusTarget();
		Camera->ApplyInitialOfficeFraming(true);
		// 전경도 직원 선택과 같은 렌즈 시프트 — 사무실 가운데가 패널 뒤로 들어가지 않게 두 판 사이 빈칸에 둔다
		bBenchLensShiftPending = true;
		BenchLensShiftAttempts = 0;
		TryApplyBenchLensShift();
	}
}

void UOfficeEmployeePanelWidget::EndWorldFocus()
{
	ResetWorkerRotation();
	Inspect.End();
	FocusWorker.Reset();
	bCameraFramingPending = false;
	CameraFramingAttempts = 0;
	bBenchLensShiftPending = false;
	BenchLensShiftAttempts = 0;
}

void UOfficeEmployeePanelWidget::TryApplyBenchLensShift()
{
	AOfficeCameraPawn* Camera = GetOfficeCamera();
	if (!Camera)
	{
		bBenchLensShiftPending = false;
		return;
	}
	float CenterRatioX = 0.5f;
	if (!TryComputeFramingCenterRatioX(CenterRatioX))
	{
		if (++BenchLensShiftAttempts < OfficeEmployeePanel::MaxCameraFramingAttempts)
		{
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] RailPanel/DetailPanel 지오메트리를 못 읽어 대기 전경을 화면 가운데로 둡니다."));
		CenterRatioX = 0.5f;
	}
	Camera->SetFramingCenterRatioX(CenterRatioX);
	bBenchLensShiftPending = false;
	UE_LOG(LogTemp, Log, TEXT("[OfficeEmployeePanel] Bench overview CenterX=%.4f"), CenterRatioX);
}

void UOfficeEmployeePanelWidget::TickViewportReframe()
{
	// 닫히는 애니메이션 동안에도 틱은 돈다 — 복원한 카메라를 다시 덮지 않게 활성일 때만
	if (!IsActivated())
	{
		return;
	}
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	if (ViewportSize.X < 1.0 || ViewportSize.Y < 1.0)
	{
		return;
	}
	if (!ViewportSize.Equals(LastViewportSize, 0.5))
	{
		// 첫 틱은 기준만 잡는다 — 열 때의 프레이밍은 선택 경로가 맡는다
		if (!LastViewportSize.IsZero())
		{
			ViewportReframeDelayTicks = OfficeEmployeePanel::ViewportReframeDelayTicks;
		}
		LastViewportSize = ViewportSize;
		return;
	}
	if (ViewportReframeDelayTicks <= 0 || --ViewportReframeDelayTicks > 0)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[OfficeEmployeePanel] Viewport resized to %.0fx%.0f - reframe"), ViewportSize.X, ViewportSize.Y);
	if (FocusWorker.IsValid())
	{
		// 착지 대기 중이면 그 시도가 새 지오메트리로 잡는다
		if (!bCameraFramingPending)
		{
			bCameraFramingPending = true;
			CameraFramingAttempts = 1; // 이미 선 직원 — 첫 시도 미룸(세운 그 프레임 보호)을 건너뛴다
			TryApplyCameraFraming();
		}
	}
	else if (SelectedEmployeeId != INDEX_NONE)
	{
		ShowBenchView();
	}
}

int32 UOfficeEmployeePanelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// 스택 전이 페이드·패널 자신의 불투명도까지 곱해진 값 — 전이가 끝나기 전엔 패널이 그려지지 않거나 옅다
	LastPaintedOpacity = InWidgetStyle.GetColorAndOpacityTint().A;
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void UOfficeEmployeePanelWidget::BeginRailSlideIn()
{
	EndRailSlideIn();
	if (!RailPanel || FMotionGate::IsReduced())
	{
		return;
	}
	bRailSlidingIn = true;
	bRailSlideStarted = false;
	RailSlideWaitElapsed = 0.f;
	LastPaintedOpacity = 0.f;
	// 첫 프레임부터 화면 밖에 둔다 — 사원증이 아직 배치 전인 프레임이 보이지 않는다
	RailPanel->SetRenderTranslation(FVector2D(-OfficeEmployeePanel::RailSlideDistance, 0.f) - GetRenderTransform().Translation);
}

void UOfficeEmployeePanelWidget::TickRailSlideIn(float InDeltaTime, const FVector2D& PanelOffsetThisFrame)
{
	if (!bRailSlidingIn || !RailPanel)
	{
		return;
	}
	// BottomStack 전이(페이드)·패널 등장 중엔 패널이 아직 안 보인다 — 다 보인 다음 프레임부터 0.2초를 잰다
	if (!bRailSlideStarted)
	{
		RailSlideWaitElapsed += InDeltaTime;
		if (LastPaintedOpacity < OfficeEmployeePanel::RailSlideVisibleOpacity && RailSlideWaitElapsed < OfficeEmployeePanel::RailSlideMaxWait)
		{
			RailPanel->SetRenderTranslation(FVector2D(-OfficeEmployeePanel::RailSlideDistance, 0.f) - PanelOffsetThisFrame);
			return;
		}
		bRailSlideStarted = true;
		RailSlideElapsed = 0.f;
	}
	// 여는 순간의 긴 프레임(사원증·카메라 준비)이 0.2초를 한 번에 삼키지 않게 한 틱 진행은 1/30초로 묶는다
	RailSlideElapsed += FMath::Min(InDeltaTime, OfficeEmployeePanel::RailSlideMaxStep);
	const float T = FMath::Clamp(RailSlideElapsed / OfficeEmployeePanel::RailSlideDuration, 0.f, 1.f);
	const float SlideX = -OfficeEmployeePanel::RailSlideDistance * (1.f - OfficeEmployeePanel::RailSlideEaseOut(T));
	// 패널 전체 등장(베이스의 아래→위 슬라이드)을 상쇄해 모달은 가로로만 움직인다. 위치 계산은 레이아웃 좌표라 프레이밍과 무관
	RailPanel->SetRenderTranslation(FVector2D(SlideX, 0.f) - PanelOffsetThisFrame);
	const bool bPanelSettled = PanelOffsetThisFrame.IsNearlyZero(0.01) || RailSlideElapsed > OfficeEmployeePanel::RailSlideDuration + 1.f;
	if (T >= 1.f && bPanelSettled)
	{
		EndRailSlideIn();
	}
}

void UOfficeEmployeePanelWidget::EndRailSlideIn()
{
	bRailSlidingIn = false;
	bRailSlideStarted = false;
	RailSlideElapsed = 0.f;
	RailSlideWaitElapsed = 0.f;
	if (RailPanel)
	{
		RailPanel->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UOfficeEmployeePanelWidget::TryApplyCameraFraming()
{
	AOfficeworker* Worker = FocusWorker.Get();
	AOfficeCameraPawn* Camera = GetOfficeCamera();
	if (!Worker || !Camera)
	{
		bCameraFramingPending = false;
		return;
	}

	// 좌석에서 세운 직후의 직원은 바닥으로 내려앉는 중이라 허리 높이가 ~1m 높게 잡힌다 — 착지한 뒤에 잡는다.
	// 첫 시도(세운 그 프레임)는 이동 컴포넌트가 아직 바닥 판정 전이라 낙하 여부를 모른다
	const UCharacterMovementComponent* WorkerMovement = Worker->GetCharacterMovement();
	const bool bWorkerSettling = CameraFramingAttempts == 0 || (WorkerMovement && WorkerMovement->IsFalling());
	float CenterRatioX = 0.5f;
	const bool bHasGeometry = TryComputeFramingCenterRatioX(CenterRatioX);
	if (bWorkerSettling || !bHasGeometry)
	{
		if (++CameraFramingAttempts < OfficeEmployeePanel::MaxCameraFramingAttempts)
		{
			return;
		}
		if (!bHasGeometry)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] RailPanel/DetailPanel 지오메트리를 못 읽어 화면 가운데로 잡습니다."));
			CenterRatioX = 0.5f;
		}
	}

	Camera->FocusOnEmployee(Worker, OfficeEmployeePanel::EmployeeFocusArmLengthCm, /*bEnableFollow*/ false);
	Camera->SetFramingCenterRatioX(CenterRatioX);
	bCameraFramingPending = false;
	UE_LOG(LogTemp, Log, TEXT("[OfficeEmployeePanel] Focus Employee=%d CenterX=%.4f Arm=%.0f"),
		SelectedEmployeeId, CenterRatioX, OfficeEmployeePanel::EmployeeFocusArmLengthCm);
}

bool UOfficeEmployeePanelWidget::TryComputeFramingCenterRatioX(float& OutRatio)
{
	if (!RailPanel || !DetailPanel)
	{
		return false;
	}
	// 레이아웃 좌표로 읽어 등장 슬라이드의 render translation 을 프레이밍에 섞지 않는다 (BuildingManage 선례)
	const FSlateRect ViewportRect = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).GetLayoutBoundingRect();
	const FSlateRect RightRect = DetailPanel->GetCachedGeometry().GetLayoutBoundingRect();
	if (RightRect.Right <= RightRect.Left || RightRect.Bottom <= RightRect.Top)
	{
		return false;
	}
	float LeftEdge = ViewportRect.Left;
	if (RailPanel->IsVisible())
	{
		const FSlateRect LeftRect = RailPanel->GetCachedGeometry().GetLayoutBoundingRect();
		if (LeftRect.Right <= LeftRect.Left || LeftRect.Bottom <= LeftRect.Top)
		{
			return false;
		}
		LeftEdge = LeftRect.Right;
	}
	return TryGetGapCenterRatioX(ViewportRect, LeftEdge, RightRect.Left, OutRatio);
}

AOfficeworker* UOfficeEmployeePanelWidget::FindWorkerActor(int32 EmployeeId) const
{
	const AOfficeGameMode* OfficeGM = GetWorld() ? GetWorld()->GetAuthGameMode<AOfficeGameMode>() : nullptr;
	AOfficeworker* Worker = (OfficeGM && EmployeeId != INDEX_NONE) ? OfficeGM->FindEmployeeActorByID(EmployeeId) : nullptr;
	return IsValid(Worker) && !Worker->bIsPortraitMode ? Worker : nullptr;
}

AOfficeCameraPawn* UOfficeEmployeePanelWidget::GetOfficeCamera() const
{
	return Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}

void UOfficeEmployeePanelWidget::SetOfficeHUDSuppressed(bool bSuppressed)
{
	UUIManagerSubsystem* UIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (UOfficeLayerWidget* OfficeLayer = UIManager ? UIManager->GetOfficeLayer() : nullptr)
	{
		OfficeLayer->SetSuppressedForEmployeePanel(bSuppressed);
	}
}

void UOfficeEmployeePanelWidget::CycleSelection(int32 Direction)
{
	if (RosterOrder.Num() == 0)
	{
		return;
	}
	const int32 CurrentIndex = RosterOrder.IndexOfByKey(SelectedEmployeeId);
	const int32 BaseIndex = CurrentIndex == INDEX_NONE ? 0 : CurrentIndex;
	const int32 NextIndex = (BaseIndex + Direction + RosterOrder.Num()) % RosterOrder.Num();
	SelectEmployee(RosterOrder[NextIndex]);
}

// ===== 핸들러 =====

void UOfficeEmployeePanelWidget::HandlePrevClicked()
{
	CycleSelection(-1);
}

void UOfficeEmployeePanelWidget::HandleNextClicked()
{
	CycleSelection(1);
}

void UOfficeEmployeePanelWidget::HandleCloseClicked()
{
	if (!IsCloseRequested())
	{
		CloseWithAnimation();
	}
}

FString UOfficeEmployeePanelWidget::TestClickAtAbsolute(FVector2D AbsolutePosition, float DragPixels)
{
	if (!FSlateApplication::IsInitialized())
	{
		return TEXT("no slate");
	}
	FSlateApplication& App = FSlateApplication::Get();
	const FWidgetPath HitPath = App.LocateWindowUnderMouse(AbsolutePosition, App.GetInteractiveTopLevelWindows(), false,
		FSlateApplicationBase::CursorUserIndex);
	FString Hit;
	for (int32 Index = FMath::Max(0, HitPath.Widgets.Num() - 4); Index < HitPath.Widgets.Num(); ++Index)
	{
		Hit += FReflectionMetaData::GetWidgetDebugInfo(&HitPath.Widgets[Index].Widget.Get()) + TEXT(" > ");
	}
	// 터치로 흘린다 — 모바일 경로이고, 마우스 합성은 호버 상태가 없어 SButton 이 뗌을 클릭으로 치지 않는다
	constexpr uint32 TouchIndex = 0;
	const FVector2D EndPosition = AbsolutePosition + FVector2D(DragPixels, 0.f);
	App.ProcessTouchStartedEvent(nullptr, FPointerEvent(FSlateApplicationBase::CursorUserIndex, TouchIndex,
		AbsolutePosition, AbsolutePosition, 1.f, true));
	if (DragPixels > 0.f)
	{
		App.ProcessTouchMovedEvent(FPointerEvent(FSlateApplicationBase::CursorUserIndex, TouchIndex,
			EndPosition, AbsolutePosition, 1.f, true));
	}
	App.ProcessTouchEndedEvent(FPointerEvent(FSlateApplicationBase::CursorUserIndex, TouchIndex,
		EndPosition, EndPosition, 0.f, true));
	return Hit;
}

FString UOfficeEmployeePanelWidget::TestTapWidget(UWidget* Target, FVector2D Fraction, float DragPixels)
{
	if (!Target)
	{
		return TEXT("no target");
	}
	const FGeometry Geo = Target->GetCachedGeometry();
	return TestClickAtAbsolute(Geo.LocalToAbsolute(Geo.GetLocalSize() * Fraction), DragPixels);
}

FString UOfficeEmployeePanelWidget::TestTouchPhase(UWidget* Target, FVector2D Fraction, bool bBegin)
{
	if (!Target || !FSlateApplication::IsInitialized())
	{
		return TEXT("no target");
	}
	FSlateApplication& App = FSlateApplication::Get();
	const FGeometry Geo = Target->GetCachedGeometry();
	const FVector2D Position = Geo.LocalToAbsolute(Geo.GetLocalSize() * Fraction);
	const FWidgetPath HitPath = App.LocateWindowUnderMouse(Position, App.GetInteractiveTopLevelWindows(), false,
		FSlateApplicationBase::CursorUserIndex);
	FString Report = bBegin ? TEXT("begin ") : TEXT("end ");
	for (int32 Index = FMath::Max(0, HitPath.Widgets.Num() - 3); Index < HitPath.Widgets.Num(); ++Index)
	{
		Report += FReflectionMetaData::GetWidgetDebugInfo(&HitPath.Widgets[Index].Widget.Get()) + TEXT(" > ");
	}
	constexpr uint32 TouchIndex = 0;
	const FPointerEvent TouchEvent(FSlateApplicationBase::CursorUserIndex, TouchIndex, Position, Position, bBegin ? 1.f : 0.f, true);
	if (bBegin)
	{
		App.ProcessTouchStartedEvent(nullptr, TouchEvent);
	}
	else
	{
		App.ProcessTouchEndedEvent(TouchEvent);
	}
	auto Describe = [](const TSharedPtr<SWidget>& Widget) -> FString
	{
		return Widget.IsValid() ? FReflectionMetaData::GetWidgetDebugInfo(Widget.Get()) : FString(TEXT("none"));
	};
	const TSharedPtr<FSlateUser> User = App.GetUser(static_cast<int32>(FSlateApplicationBase::CursorUserIndex));
	const TSharedPtr<SWidget> Captor = User.IsValid() ? User->GetPointerCaptor(TouchIndex) : TSharedPtr<SWidget>();
	const TSharedPtr<SWidget> Focused = User.IsValid() ? User->GetFocusedWidget() : TSharedPtr<SWidget>();
	Report += FString::Printf(TEXT("| captor=%s focus=%s"), *Describe(Captor), *Describe(Focused));
	return Report;
}

FVector4 UOfficeEmployeePanelWidget::TestGetAbsoluteRect(UWidget* Target) const
{
	if (!Target)
	{
		return FVector4(0.f, 0.f, 0.f, 0.f);
	}
	const FGeometry Geo = Target->GetCachedGeometry();
	const FVector2D TopLeft = Geo.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geo.LocalToAbsolute(Geo.GetLocalSize());
	return FVector4(TopLeft.X, TopLeft.Y, BottomRight.X, BottomRight.Y);
}

void UOfficeEmployeePanelWidget::HandleGapClicked()
{
	// 카드가 떠 있으면 첫 탭은 카드만 내린다 — 패널은 그다음 탭에 X 와 같은 경로로 닫힌다
	if (CloseEquipDetail() || CloseEnhanceCard())
	{
		return;
	}
	HandleCloseClicked();
}

void UOfficeEmployeePanelWidget::HandleTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (TabSwitcher && ButtonIndex >= 0 && ButtonIndex < TabSwitcher->GetNumWidgets())
	{
		TabSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}
	if (IsPotentialTabShown())
	{
		SyncPotentialWorkspace();
	}
	else
	{
		SuspendPotentialWorkspace();
		CloseOddsTable();
	}
	SyncEquipmentTab(/*bEmployeeChanged*/ false);
	SyncFooterForTab();
}

void UOfficeEmployeePanelWidget::SyncFooterForTab()
{
	const bool bGrowth = !TabSwitcher || TabSwitcher->GetActiveWidgetIndex() == 0;
	if (FooterRow)
	{
		FooterRow->SetVisibility(bGrowth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// ★ 강화 버튼이 성장 탭에만 있으니 그 카드도 성장 탭에서만 산다
	if (!bGrowth)
	{
		CloseEnhanceCard();
	}
}

void UOfficeEmployeePanelWidget::SelectTab(int32 TabIndex)
{
	if (TabButtonGroup && TabIndex >= 0 && TabIndex < TabButtonGroup->GetButtonCount())
	{
		TabButtonGroup->SelectButtonAtIndex(TabIndex, /*bAllowSound*/ false);
	}
}

void UOfficeEmployeePanelWidget::HandleRosterCardClicked(int32 EmployeeId)
{
	SelectEmployee(EmployeeId);
}

void UOfficeEmployeePanelWidget::HandleFireClicked()
{
	const FEmployeeInstance* Employee = (EmployeeManager && SelectedEmployeeId != INDEX_NONE)
		? EmployeeManager->FindEmployee(SelectedEmployeeId) : nullptr;
	UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!Employee || !UIMgr)
	{
		return;
	}

	// 대상을 여는 시점에 고정 — 모달이 떠 있는 동안 선택이 옮겨갈 수 있다
	const int32 TargetId = Employee->EmployeeID;
	// 뷰포트 오버레이(Z 10000)라 패널은 그대로 활성 상태로 남는다
	UIMgr->ShowConfirmDialog(
		NSLOCTEXT("OfficeEmployeePanel", "FireTitle", "직원 해고"),
		FText::Format(NSLOCTEXT("OfficeEmployeePanel", "FireMessage", "{0} 직원을 해고합니다. 되돌릴 수 없습니다."),
			FText::FromString(Employee->EmployeeName)),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleFireConfirmed, TargetId),
		FSimpleDelegate(), EConfirmDialogIcon::Delete);
}

void UOfficeEmployeePanelWidget::HandleFireConfirmed(int32 EmployeeId)
{
	if (!EmployeeManager || !IsActivated())
	{
		return;
	}

	// 해고 후 고를 이웃 — 표시 순서의 다음, 없으면 이전
	const int32 FiredIndex = RosterOrder.IndexOfByKey(EmployeeId);
	PendingSelectAfterFireId = INDEX_NONE;
	if (FiredIndex != INDEX_NONE)
	{
		if (RosterOrder.IsValidIndex(FiredIndex + 1)) PendingSelectAfterFireId = RosterOrder[FiredIndex + 1];
		else if (RosterOrder.IsValidIndex(FiredIndex - 1)) PendingSelectAfterFireId = RosterOrder[FiredIndex - 1];
	}

	// 해고는 워커 액터를 지우므로 살펴보기를 먼저 정리한다
	const bool bFiringSelected = EmployeeId == SelectedEmployeeId;
	if (bFiringSelected)
	{
		EndWorldFocus();
	}
	// 로스터 재구성/재선택은 FireEmployee 의 OnEmployeeRosterChanged 브로드캐스트가 담당
	if (!EmployeeManager->FireEmployee(EmployeeId))
	{
		PendingSelectAfterFireId = INDEX_NONE;
		if (bFiringSelected)
		{
			ApplyWorldFocus();
		}
	}
}

void UOfficeEmployeePanelWidget::HandleEnhanceClicked()
{
	if (SelectedEmployeeId != INDEX_NONE)
	{
		ShowEnhanceCard();
	}
}

void UOfficeEmployeePanelWidget::HandleBenchSeatClicked()
{
	UOfficeManager* OfficeMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr;
	if (!OfficeMgr || SelectedEmployeeId == INDEX_NONE)
	{
		return;
	}
	if (!OfficeMgr->AutoSeatEmployee(SelectedEmployeeId))
	{
		if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
		{
			UIMgr->ShowRejectNotification(NSLOCTEXT("OfficeEmployeePanel", "NoEmptyDesk", "빈 책상이 없습니다"));
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SeatRefocusTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActivated())
			{
				ApplyWorldFocus();
			}
		}), OfficeEmployeePanel::SeatRefocusDelaySeconds, false);
	}
}

void UOfficeEmployeePanelWidget::HandleRosterChanged()
{
	RefreshRoster();

	if (EmployeeManager && EmployeeManager->FindEmployee(SelectedEmployeeId))
	{
		RefreshHeader();
		return;
	}

	// 선택 직원이 사라졌다(해고 등) — 이웃으로 옮기고, 아무도 없으면 닫는다
	const int32 NextId = RosterOrder.Contains(PendingSelectAfterFireId)
		? PendingSelectAfterFireId : (RosterOrder.Num() > 0 ? RosterOrder[0] : INDEX_NONE);
	PendingSelectAfterFireId = INDEX_NONE;
	EndWorldFocus();
	SelectedEmployeeId = INDEX_NONE;
	if (NextId != INDEX_NONE)
	{
		SelectEmployee(NextId);
		return;
	}
	RefreshHeader();
	if (!IsCloseRequested())
	{
		CloseWithAnimation();
	}
}

void UOfficeEmployeePanelWidget::HandleEmployeeStatsChanged(int32 EmployeeId)
{
	if (EmployeeId == SelectedEmployeeId)
	{
		RefreshHeader();
		RefreshGrowth();
	}
}

void UOfficeEmployeePanelWidget::HandleEmployeeLevelUp(int32 EmployeeId, int32 NewLevel)
{
	if (bBulkLevelingUp)
	{
		return;
	}
	// 레벨은 할 일 순 정렬 키라 순서가 바뀐다
	RefreshRoster();
	if (EmployeeId == SelectedEmployeeId)
	{
		RefreshHeader();
	}
}

void UOfficeEmployeePanelWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type == EResourceType::Money && WalletMoneyChip)
	{
		WalletMoneyChip->SetValue(NewValue);
	}
	else if (Type == EResourceType::Diamond && WalletDiamondChip)
	{
		WalletDiamondChip->SetValue(NewValue);
	}
}
