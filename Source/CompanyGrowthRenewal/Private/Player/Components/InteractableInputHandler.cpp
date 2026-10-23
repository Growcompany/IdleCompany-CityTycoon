// Source/YourModule/Private/Components/FactoryInputHandler.cpp
#include "InteractableInputHandler.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Player/PlayerCamera.h"
#include "Player/MainMapPlayerController.h"
#include "Player/Components/MovementInputHandler.h"
#include "Core/CGGameInstance.h"
#include "Player/Components/PlacementHandler.h"
#include "Components/SphereComponent.h"
#include "Entity/InteractableBaseActor.h"
#include "Entity/Ambient/AmbientAircraftFlyby.h"
#include "Interfaces/IInputHandler.h"
#include "Entity/Factory/BrickFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Util/CoordinateUtils.h"
#include "Input/InputPriorities.h"
#include "Office/WorkstationActorBase.h"
#include "Player/OfficeCameraPawn.h"
#include "Office/DecorationActor.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/EmployeeReactionComponent.h"
#include "EngineUtils.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Enum/ProjectLifecycle.h"
#include "UI/UIBase.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "Player/Components/InteractableDragPolicy.h"
#include "UI/Panel/FactoryPanelWidget.h"
#include "UI/Panel/WorkstationInfoWidget.h"
#include "UI/Panel/OfficeEmployeePanelWidget.h"
#include "Enum/WidgetType.h"
#include "Global/GlobalUtilFunctions.h"
#include "TimerManager.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Blueprint/WidgetLayoutLibrary.h"

namespace
{
    // 32dp = 손끝 접촉면 반경 근사. 멀리서 본 건물은 화면에서 손가락보다 작아진다.
    constexpr float PlacementTouchSlopDp = 32.f;

    // 광선이 빗나가도 배치 대상의 화면 투영 사각형(+여유) 안이면 그 대상을 누른 것으로 본다.
    bool IsTouchNearPlacementTarget(const APlayerController* PC, const AActor* Target,
        const FVector2D& ScreenPos, float SlopPx)
    {
        if (!PC || !IsValid(Target)) { return false; }
        const FBox Bounds = Target->GetComponentsBoundingBox();
        if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN()) { return false; }

        FBox2D ScreenRect(ForceInit);
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector Point(
                (Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
                (Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y,
                (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
            FVector2D Projected;
            if (PC->ProjectWorldLocationToScreen(Point, Projected, false)) { ScreenRect += Projected; }
        }
        return ScreenRect.bIsValid && ScreenRect.ExpandBy(SlopPx).IsInside(ScreenPos);
    }
}

UInteractableInputHandler::UInteractableInputHandler()
{
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> imc_Interactable
    (TEXT("EnhancedInput.InputMappingContext'/Game/CompanyGrowth/Input/IMC_Interactable_Mode.IMC_Interactable_Mode'"));
    if (imc_Interactable.Succeeded()) 
    {
        IMC_Interactable = imc_Interactable.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> ia_InteractableHold
    (TEXT("EnhancedInput.InputAction'/Game/CompanyGrowth/Input/IA_InteractableHold.IA_InteractableHold'"));
    if (ia_InteractableHold.Succeeded())
    {
        IA_InteractableHold = ia_InteractableHold.Object;
    }

    static ConstructorHelpers::FObjectFinder<UInputAction> ia_InteractableTap
    (TEXT("EnhancedInput.InputAction'/Game/CompanyGrowth/Input/IA_InteractableTap.IA_InteractableTap'"));
    if (ia_InteractableTap.Succeeded())
    {
        IA_InteractableTap = ia_InteractableTap.Object;
    }
}

void UInteractableInputHandler::Initialize()
{
    Owner = Cast<APlayerCamera>(GetOwner());
    UE_LOG(LogTemp, Log, TEXT("[UInteractableInputHandler] Initialize"));
}

void UInteractableInputHandler::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    if (Owner && Owner->GetController())
    {
        if (APlayerController* PlayerController = Cast<APlayerController>(Owner->GetController()))
        {
            PlayerController->bShowMouseCursor = true;
            PlayerController->bEnableClickEvents = true;
            PlayerController->bEnableMouseOverEvents = true;
            PlayerController->bEnableTouchEvents = true;
            PlayerController->bEnableTouchOverEvents = true;

            if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
            {
                InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
                if (InputSubsystem)
                {
                    InputSubsystem->AddMappingContext(IMC_Interactable, InputPriority::INTERACTION);
                    UE_LOG(LogTemp, Log, TEXT("[UInteractableInputHandler] InputMappingContext added."));

                    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
                    {
                        // 플랫폼별 바인딩 분기
                        AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(PlayerController);
                        if (MainPC && MainPC->GetCurrentInputType() == EInputType::KeyMouse)
                        {
                            // Hold
                            EnhancedInputComponent->BindAction(IA_InteractableHold,
                                ETriggerEvent::Started, this,
                                &UInteractableInputHandler::OnInteractionStarted);
                            EnhancedInputComponent->BindAction(IA_InteractableHold,
                                ETriggerEvent::Ongoing, this,
                                &UInteractableInputHandler::OnInteractionTriggered);
                            EnhancedInputComponent->BindAction(IA_InteractableHold,
                                ETriggerEvent::Triggered, this,
                                &UInteractableInputHandler::OnInteractionTriggered);
                            EnhancedInputComponent->BindAction(IA_InteractableHold,
                                ETriggerEvent::Completed, this,
                                &UInteractableInputHandler::OnInteractionFinish);
                            EnhancedInputComponent->BindAction(IA_InteractableHold,
                                ETriggerEvent::Canceled, this,
                                &UInteractableInputHandler::OnInteractionCanceled);

                            // Tap
                            EnhancedInputComponent->BindAction(IA_InteractableTap,
                                ETriggerEvent::Completed, this, &UInteractableInputHandler::OnTapCompleted);
                        }
                        else
                        {
                            // 모바일에서는 Enhanced Input 바인딩 안 함
                        }
                    }
                }
            }
        }
    }
}

void UInteractableInputHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishEmployeeCarryPress();
    ResetFactoryPressState();
    Super::EndPlay(EndPlayReason);
}

void UInteractableInputHandler::OnInteractionStarted(const FInputActionValue& Value)
{
    if (!CoordinateUtils::SingleTouchCheck())
        return;

    SavedTouchAbsolutePos = UGlobalUtilFunctions::GetPointerAbsolutePosition();

    // 터치 위치 저장
    FVector2D screenPos = CoordinateUtils::GetLastTouchEventPosition();
    FVector worldPos = FVector::ZeroVector;
    bHasPressScreenPosition = CoordinateUtils::ProjectTouchToGroundPlane(screenPos, worldPos);
    if (!bHasPressScreenPosition)
    {
        // 짧은 탭은 처리 시점에 이미 뗀 상태라 위 조회가 실패한다 — 이번 누름 이벤트 위치로 투영한다.
        bHasPressScreenPosition = CoordinateUtils::ProjectScreenPosToGroundPlane(screenPos, worldPos);
    }
    if (bHasPressScreenPosition)
    {
        SavedTouchScreenPos = screenPos;
        SavedTouchWorldPos = worldPos;

        // 월드 탭 지점 링 리플 (UI 클릭은 이 핸들러까지 안 옴 — 버튼은 자체 펀치 피드백)
        // 위치는 위젯이 직접 취득 — screenPos 는 물리픽셀이라 DPI 불일치 (CLAUDE.md 좌표 규칙)
        if (UUIManagerSubsystem* UIMgr = GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
        {
            if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
            {
                InGame->SpawnTapRing();
            }
        }
    }

    bIsDragging = false;
    EInputMode currentMode = Owner->GetPlayerController()->GetCurrentInputMode();

    // Collision으로 감지된 액터 확인 (저장된 터치 위치 사용)
    AActor* collisionActor = GetOverlappingActor(screenPos);
    LastHitActor = collisionActor;
	if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Owner))
	{
		bOfficeEditSelectionPress = OfficePawn->IsOfficePlacementEditing();
		if (bOfficeEditSelectionPress && !OfficePawn->GetOfficeEditSelection())
		{
			if (!OfficePawn->SelectOfficeEditActor(collisionActor)) return;
			// 선택이 BuildPlace로 전환한 뒤 같은 누름을 아래 배치 드래그 경로로 넘긴다.
			currentMode = Owner->GetPlayerController()->GetCurrentInputMode();
		}
	}
    BeginEmployeeCarryPress(currentMode == EInputMode::Normal ? collisionActor : nullptr);

    if (currentMode == EInputMode::BuildPlace)
    {
        // BuildPlace 모드 처리 - 건물, 업무공간, 장식품 배치 대상 확인
        AActor* PlacementTarget = nullptr;
        bool bCityPlacement = false;

        if (Owner->PlacementHandler->IsWorkstationMode())
        {
            PlacementTarget = Cast<AActor>(Owner->PlacementHandler->GetPlacementWorkstation());
        }
        else if (Owner->PlacementHandler->IsDecorationMode() || Owner->PlacementHandler->IsWallDecorationMode())
        {
            PlacementTarget = Cast<AActor>(Owner->PlacementHandler->GetPlacementDecoration());
        }
        else
        {
            PlacementTarget = Cast<AActor>(Owner->PlacementHandler->GetPlacementTargetEntity());
            bCityPlacement = true;
        }

        if (bCityPlacement && PlacementTarget && collisionActor != PlacementTarget
            && IsTouchNearPlacementTarget(Owner->GetPlayerController(), PlacementTarget, screenPos,
                PlacementTouchSlopDp * UWidgetLayoutLibrary::GetViewportScale(this)))
        {
            UE_LOG(LogTemp, Log, TEXT("[PlacementTouch] near-miss grabbed %s (ray hit %s)"),
                *PlacementTarget->GetName(), collisionActor ? *collisionActor->GetName() : TEXT("None"));
            collisionActor = PlacementTarget;
            LastHitActor = PlacementTarget;
        }

        if (collisionActor && collisionActor == PlacementTarget)
        {
            // 배치 대상 클릭 - 이동 모드
            Owner->MovementInputHandler->ReleaseDragMoveIMC();  // 카메라 드래그 해제
            Owner->PlacementHandler->InitPlacementIMC();         // 배치 활성화
            Owner->PlacementHandler->StartDragging();            // 드래그 시작
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Placement target clicked"));
            UE_LOG(LogTemp, Log, TEXT("Placement target clicked"));
        }
        else
        {
            // 다른 곳 클릭 - 카메라 드래그 모드로 전환
            Owner->PlacementHandler->ReleasePlacementIMC();
            Owner->MovementInputHandler->InitDragMoveIMC();
            UE_LOG(LogTemp, Log, TEXT("Non-target clicked - switching to camera drag"));
        }
    }
    else if (currentMode == EInputMode::Normal && collisionActor)
    {
        // Normal 모드에서 Interactable 클릭
        if (collisionActor->ActorHasTag("Interactable") &&
            collisionActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass()))
        {
            // 방문 모드에서는 모든 인터랙션 차단
            if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
            {
                if (GI->IsVisitMode()) return;
            }

            UFactoryPanelWidget* TopFactoryPanel = GetTopFactoryPanel();
            if (collisionActor->ActorHasTag("Factory") && IsValid(TopFactoryPanel))
            {
                if (TopFactoryPanel->IsCloseRequested())
                {
                    ResetFactoryPressState();
                    return;
                }

                FactoryPressPhase = CGRFactoryTapHoldPolicy::EPhase::Pending;
                GetWorld()->GetTimerManager().SetTimer(
                    FactoryHoldDelayTimerHandle,
                    this,
                    &UInteractableInputHandler::BeginFactoryHoldAfterDelay,
                    CGRFactoryTapHoldPolicy::HoldThresholdSeconds,
                    false);
                return;
            }

            if (collisionActor->ActorHasTag("Factory"))
            {
                FactoryPressPhase = CGRFactoryTapHoldPolicy::EPhase::Holding;
            }

            APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
            IInputHandler::Execute_OnInteract(LastHitActor, PC);
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Interactable clicked"));
            UE_LOG(LogTemp, Log, TEXT("Interactable clicked"));
        }
    }
}

bool UInteractableInputHandler::TryGetFactoryHoldAnchor(FVector2D& OutAbsolutePosition) const
{
    if (FactoryPressPhase != CGRFactoryTapHoldPolicy::EPhase::Holding)
    {
        return false;
    }

    OutAbsolutePosition = SavedTouchAbsolutePos;
    return true;
}

void UInteractableInputHandler::OnInteractionTriggered(const FInputActionValue& Value)
{
    if (bEmployeeCarryPress)
    {
        UpdateEmployeeCarryPress();
    }

    // 실제 드래그 거리로 판단
    FVector2D currentScreenPos;
    FVector currentWorldPos;

    if (CoordinateUtils::ProjectTouchToGroundPlane(currentScreenPos, currentWorldPos))
    {
        float dragDistance = FVector2D::Distance(SavedTouchScreenPos, currentScreenPos);
        const float DragThreshold = CGRInteractableDragPolicy::DragThresholdPixels;
        const EInputMode currentMode = Owner->GetPlayerController()->GetCurrentInputMode();
        const bool bShouldCancelFactoryHold = CGRInteractableDragPolicy::ShouldCancelFactoryHold(
            bIsDragging,
            dragDistance,
            DragThreshold,
            currentMode,
            IsValid(LastHitActor) && LastHitActor->ActorHasTag("Factory"));

        if (dragDistance > DragThreshold && !bIsDragging)
        {
            const CGRFactoryTapHoldPolicy::EDragAction FactoryDragAction =
                CGRFactoryTapHoldPolicy::ResolveDrag(FactoryPressPhase);

            if (FactoryDragAction == CGRFactoryTapHoldPolicy::EDragAction::CancelPending)
            {
                ResetFactoryPressState();
            }

            bIsDragging = true;

            // KeepProduction 은 여기 들어오면 안 된다 — 모드가 Factory 인 동안은 이 경로가 무조건 생산을 끊는다.
            if (FactoryDragAction == CGRFactoryTapHoldPolicy::EDragAction::None)
            {
                const bool bImplementsInputHandler = IsValid(LastHitActor)
                    && LastHitActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass());
                CGRInteractableDragPolicy::TryCancelFactoryHold(
                    bShouldCancelFactoryHold,
                    LastHitActor,
                    bImplementsInputHandler,
                    [this](AActor* FactoryActor)
                    {
                        APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
                        IInputHandler::Execute_OnEndInteract(FactoryActor, PC);
                    });
            }

            // 드래그 시작시 상호작용 취소 (시각적 피드백 중단)
            if (LastHitActor && LastHitActor->ActorHasTag("Interactable"))
            {
                // 여기서 흔들림 중단하거나 원래대로 돌리는 로직 추가 가능
                UE_LOG(LogTemp, Log, TEXT("Drag started - interaction canceled"));
            }
        }
    }
}

bool UInteractableInputHandler::FinishOfficeEditSelectionPress()
{
	if (!bOfficeEditSelectionPress) return false;
	bOfficeEditSelectionPress = false;
	Owner->PlacementHandler->StopDragging();
	Owner->PlacementHandler->ReleasePlacementIMC();
	LastHitActor = nullptr;
	bIsDragging = false;
	Owner->MovementInputHandler->InitDragMoveIMC();
	return true;
}

void UInteractableInputHandler::OnInteractionFinish(const FInputActionValue& Value)
{
	if (FinishOfficeEditSelectionPress()) return;
	if (FinishEmployeeCarryPress()) return;
    UE_LOG(LogTemp, Verbose, TEXT("[UInteractableInputHandler] Click-Release"));

    // 배치 드래그 상태 해제
    Owner->PlacementHandler->StopDragging();

    if (FinishFactoryPress())
    {
        Owner->MovementInputHandler->InitDragMoveIMC();
        bIsDragging = false;
        LastHitActor = nullptr;
        return;
    }

    if (ShouldCancelAircraftTap())
    {
        LastHitActor = nullptr;
        bIsDragging = true;
    }
    EInputMode currentMode = Owner->GetPlayerController()->GetCurrentInputMode();

    if (LastHitActor && LastHitActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass()))
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);

        // Factory는 항상 OnEndInteract 호출 (드래그 무관)
        if (currentMode == EInputMode::Factory && LastHitActor->ActorHasTag("Factory"))
        {
            IInputHandler::Execute_OnEndInteract(LastHitActor, PC);
        }
        // Building: 드래그 중이 아닌 경우에만 건물 관리 패널 표시
        if (!bIsDragging && currentMode == EInputMode::Normal && LastHitActor->ActorHasTag("Building"))
        {
            IInputHandler::Execute_OnEndInteract(LastHitActor, PC);
            bIsFirstClickBuilding = true;
        }
        // 일반 인터랙터블(Building 태그 없는 IInputHandler — 도시 인수 클릭 프록시 등): 느린 릴리즈에서도 동작 (Canceled 경로와 일관)
        else if (!bIsDragging && currentMode == EInputMode::Normal && LastHitActor->ActorHasTag("Interactable"))
        {
            IInputHandler::Execute_OnEndInteract(LastHitActor, PC);
        }
    }
    // Workstation: IInputHandler 미구현/무태그 — 클릭으로 직접 우측 도킹 패널 표시 (드래그 아닐 때만)
    else if (!bIsDragging && currentMode == EInputMode::Normal)
    {
        if (AWorkstationActorBase* Workstation = Cast<AWorkstationActorBase>(LastHitActor))
        {
            OpenWorkstationPanel(Workstation);
        }
        else if (AOfficeworker* Worker = Cast<AOfficeworker>(LastHitActor))
        {
            HandleEmployeeTap(Worker);
        }
    }
    // 정리
    Owner->MovementInputHandler->InitDragMoveIMC();
    bIsDragging = false;
    LastHitActor = nullptr;
}

void UInteractableInputHandler::OnInteractionCanceled(const FInputActionValue& Value)
{
	if (FinishOfficeEditSelectionPress()) return;
	if (FinishEmployeeCarryPress()) return;
    UE_LOG(LogTemp, Verbose, TEXT("[OnInteractionCanceled] Quick release - 0.2 second"));

    if (FinishFactoryPress())
    {
        bIsDragging = false;
        LastHitActor = nullptr;
        return;
    }

    if (ShouldCancelAircraftTap())
    {
        LastHitActor = nullptr;
        bIsDragging = true;
    }
    EInputMode currentMode = Owner->GetPlayerController()->GetCurrentInputMode();

    // Interactable 객체 처리
    if (LastHitActor && LastHitActor->ActorHasTag("Interactable"))
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);

        if (currentMode == EInputMode::Normal)
        {
            if (LastHitActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass()))
            {
                IInputHandler::Execute_OnEndInteract(LastHitActor, PC);
                bIsFirstClickBuilding = true;
                UE_LOG(LogTemp, Log, TEXT("Interactable tapped in Normal mode"));
            }
        }
        else
        {
            // Factory는 항상 OnEndInteract 호출 (드래그 무관)
            if (currentMode == EInputMode::Factory && LastHitActor->ActorHasTag("Factory"))
            {
                IInputHandler::Execute_OnEndInteract(LastHitActor, PC);
            }
        }
    }
    // Workstation: 무태그라 위 Interactable 분기에 안 잡힘 — 빠른 탭으로 우측 도킹 패널 표시
    else if (currentMode == EInputMode::Normal)
    {
        if (AWorkstationActorBase* Workstation = Cast<AWorkstationActorBase>(LastHitActor))
        {
            OpenWorkstationPanel(Workstation);
        }
        else if (AOfficeworker* Worker = Cast<AOfficeworker>(LastHitActor))
        {
            HandleEmployeeTap(Worker);
        }
    }

    // 정리
    bIsDragging = false;
    LastHitActor = nullptr;
}

bool UInteractableInputHandler::ShouldCancelAircraftTap() const
{
    if (!Cast<AAmbientAircraftFlyby>(LastHitActor))
    {
        return false;
    }
    const AMainMapPlayerController* MainMapPC = Owner ? Owner->GetPlayerController() : nullptr;
    FVector2D ReleasePosition = SavedTouchScreenPos;
    bool bValidReleasePosition = false;
    if (MainMapPC)
    {
        if (MainMapPC->GetCurrentInputType() == EInputType::Touch)
        {
            double TouchX = 0.0, TouchY = 0.0;
            bool bTouchPressed = false;
            // PlayerInput는 Ended 이벤트에서도 마지막 X/Y를 유지한다.
            MainMapPC->GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bTouchPressed);
            ReleasePosition = FVector2D(TouchX, TouchY);
            bValidReleasePosition = MainMapPC->PlayerInput != nullptr;
        }
        else if (MainMapPC->GetCurrentInputType() == EInputType::KeyMouse)
        {
            double MouseX = 0.0, MouseY = 0.0;
            bValidReleasePosition = MainMapPC->GetMousePosition(MouseX, MouseY);
            ReleasePosition = FVector2D(MouseX, MouseY);
        }
        else
        {
            bValidReleasePosition = true;
        }
    }
    return CGRInteractableDragPolicy::ShouldCancelAircraftTap(
        bIsDragging, bHasPressScreenPosition && bValidReleasePosition, SavedTouchScreenPos, ReleasePosition);
}

void UInteractableInputHandler::OnTapCompleted(const FInputActionValue& Value)
{
	if (FinishOfficeEditSelectionPress()) return;
	if (FinishEmployeeCarryPress()) return;
	if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Owner))
	{
		if (OfficePawn->IsOfficePlacementEditing() && !LastHitActor) return;
	}
    EInputMode currentMode = Owner->GetPlayerController()->GetCurrentInputMode();

    // BuildPlace 모드: 탭하면 해당 위치로 이동
    if (currentMode == EInputMode::BuildPlace)
    {
        // 빌딩 처음 클릭하고 뗄 떼는 이동을 막기위함
        if (bIsFirstClickBuilding)
        {
            bIsFirstClickBuilding = false;
            return;
        }

        // WallDecoration 모드는 스크린 위치 버전 호출 (저장된 터치 위치 사용)
        if (Owner->PlacementHandler->IsWallDecorationMode())
        {
            Owner->PlacementHandler->TrackMovePlacement(SavedTouchScreenPos);
        }
        else
        {
            Owner->PlacementHandler->TrackMovePlacement(SavedTouchWorldPos);
        }
        Owner->PlacementHandler->ReleasePlacementIMC();
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Building moved by tap!"));
        UE_LOG(LogTemp, Error, TEXT("Building moved by tap!"));
    }

    // 정리
    Owner->MovementInputHandler->InitDragMoveIMC();
    bIsDragging = false;
    LastHitActor = nullptr;
}

AActor* UInteractableInputHandler::GetOverlappingActor(const FVector2D& ScreenPos)
{
    AMainMapPlayerController* PC = Owner->GetPlayerController();
    if (!PC) return nullptr;

    FVector WorldLocation, WorldDirection;

    // 전달받은 스크린 좌표로 월드 좌표 변환
    PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldLocation, WorldDirection);

    // 레이캐스트로 Interactable 액터 감지
    FVector TraceEnd = WorldLocation + (WorldDirection * 500000.0f);
    FCollisionQueryParams QueryParams(FName(TEXT("InteractableTrace")), false, Owner);

    // 배치 모드 확인
    EPlacementMode PlacementMode = Owner->PlacementHandler->GetCurrentPlacementMode();
    bool bIsPlacementMode = (PlacementMode == EPlacementMode::Workstation ||
                             PlacementMode == EPlacementMode::Decoration ||
                             PlacementMode == EPlacementMode::WallDecoration);

    // 개발중 집중 모드: 배치 모드가 아니면 월드 오브젝트 탐지를 소스에서 차단.
    // → LastHitActor 가 null 이 되어 업무공간/빌딩/장식/롱프레스 다운스트림 분기가 전부 no-op.
    if (!bIsPlacementMode && IsOfficeFocusLocked())
    {
        // 직원만은 맞힌다 — 일하는 직원 찌르기 연출용. 패널은 HandleEmployeeTap 이 계속 막는다.
        FHitResult WorkerHit;
        if (GetWorld()->LineTraceSingleByChannel(WorkerHit, WorldLocation, TraceEnd, ECC_GameTraceChannel1, QueryParams))
        {
            return Cast<AOfficeworker>(WorkerHit.GetActor());
        }
        return nullptr;
    }

    // 배치 모드일 때는 전용 채널(ECC_GameTraceChannel2)로 미리보기 액터만 감지
    if (bIsPlacementMode)
    {
        FHitResult HitResult;
        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            WorldLocation,
            TraceEnd,
            ECC_GameTraceChannel2,
            QueryParams
        );

        if (bHit && HitResult.GetActor())
        {
            return HitResult.GetActor();
        }
        return nullptr;
    }

    // 배치 편집 중엔 앉은 직원이 책상 클릭을 가리지 않게 광선에서 뺀다
    if (const AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Owner))
    {
        if (OfficePawn->IsOfficePlacementEditing())
        {
            for (TActorIterator<AOfficeworker> It(GetWorld()); It; ++It)
            {
                QueryParams.AddIgnoredActor(*It);
            }
        }
    }

    // 일반 모드: Single 레이캐스트
    FHitResult HitResult;
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        WorldLocation,
        TraceEnd,
        ECC_GameTraceChannel1,
        QueryParams
    );

    // 부지 상자는 지면 위 5m 까지 솟아 있어, 그 위에 선 건물 아래쪽을 누르면 부지가 먼저 맞는다.
    if (bHit && Cast<ACityPlotActor>(HitResult.GetActor()))
    {
        FCollisionQueryParams BehindPlotParams = QueryParams;
        BehindPlotParams.AddIgnoredActor(HitResult.GetActor());
        FHitResult BehindHit;
        if (GetWorld()->LineTraceSingleByChannel(BehindHit, WorldLocation, TraceEnd, ECC_GameTraceChannel1, BehindPlotParams))
        {
            const AActor* BehindActor = BehindHit.GetActor();
            if (Cast<AInteractableBaseActor>(BehindActor) && !Cast<ACityPlotActor>(BehindActor))
            {
                HitResult = BehindHit;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[GetOverlappingActor] bHit=%d, Actor=%s, Component=%s"),
        bHit,
        bHit && HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("None"),
        bHit && HitResult.GetComponent() ? *HitResult.GetComponent()->GetName() : TEXT("None"));

    if (bHit && HitResult.bBlockingHit && HitResult.GetActor())
    {
        AActor* HitActor = HitResult.GetActor();

        // Interactable 액터 체크
        if (AInteractableBaseActor* InteractableActor = Cast<AInteractableBaseActor>(HitActor))
        {
            if (InteractableActor->ActorHasTag("Interactable"))
            {
                return InteractableActor;
            }
            return InteractableActor;
        }

        // 업무공간 액터 체크
        if (AWorkstationActorBase* WorkstationActor = Cast<AWorkstationActorBase>(HitActor))
        {
            if (WorkstationActor->ActorHasTag("Workstation"))
            {
                return WorkstationActor;
            }
            return WorkstationActor;
        }

        // 장식품 액터 체크
        if (ADecorationActor* DecorationActor = Cast<ADecorationActor>(HitActor))
        {
            if (DecorationActor->ActorHasTag("Decoration"))
            {
                return DecorationActor;
            }
            return DecorationActor;
        }

        // 직원 — 맞은 컴포넌트는 전용 ClickVolume 뿐이다 (이동 캡슐은 클릭 채널 Ignore)
        if (AOfficeworker* Worker = Cast<AOfficeworker>(HitActor))
        {
            return Worker;
        }

        // 인터페이스 기반 일반 인터랙터블 (AInteractableBaseActor 비파생 — 예: 도시 인수 클릭 프록시)
        if (HitActor->ActorHasTag("Interactable") &&
            HitActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass()))
        {
            return HitActor;
        }
    }

    return nullptr;
}

void UInteractableInputHandler::HandleEmployeeTap(AOfficeworker* Worker)
{
    // 초상화 촬영용·로딩 중 숨긴 직원은 클릭 대상이 아니다
    if (!IsValid(Worker) || Worker->bIsPortraitMode || Worker->IsHidden())
    {
        return;
    }
    // 개발 중 잠금은 그대로 — 패널 대신 일하는 직원의 짧은 반응만 보여 준다
    if (IsOfficeFocusLocked())
    {
        if (UEmployeeReactionComponent* Reaction = Worker->FindComponentByClass<UEmployeeReactionComponent>())
        {
            Reaction->PlayWorkPoke();
        }
        return;
    }
    if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
    {
        if (GI->IsVisitMode()) return;
    }

    Worker->PlayTapReaction();

    // 사무실 직원 패널 — 위젯 클래스 미등록(DT 행 없음)이면 아래 책상 패널 경로로 폴백
    if (UOfficeEmployeePanelWidget::PushForEmployee(this, Worker->GetEmployeeID()))
    {
        return;
    }

    if (AWorkstationActorBase* Desk = Worker->GetAssignedWorkstation())
    {
        OpenWorkstationPanel(Desk, Worker->GetEmployeeID());
    }
}

void UInteractableInputHandler::BeginEmployeeCarryPress(AActor* PressedActor)
{
    bEmployeeCarryPress = false;
    CarryPressReaction.Reset();

    AOfficeworker* Worker = Cast<AOfficeworker>(PressedActor);
    UEmployeeReactionComponent* Reaction = Worker ? Worker->FindComponentByClass<UEmployeeReactionComponent>() : nullptr;
    if (!Reaction || !Owner || !Owner->MovementInputHandler || !IsOfficeFocusLocked() || !Reaction->CanBeginCarry())
    {
        return;
    }
    bEmployeeCarryPress = true;
    CarryPressReaction = Reaction;
    // 직원 위에서 시작한 누름은 카메라를 끌지 않는다 — 떼는 쪽(FinishEmployeeCarryPress)이 다시 켠다
    Owner->MovementInputHandler->ReleaseDragMoveIMC();
}

void UInteractableInputHandler::UpdateEmployeeCarryPress()
{
    UEmployeeReactionComponent* Reaction = CarryPressReaction.Get();
    if (!Reaction || Reaction->IsCarryActive())
    {
        return;
    }
    // 문턱은 UI 단위 — 절대(픽셀) 이동량을 뷰포트 DPI 배율로 나눈다
    const FVector2D Pointer = UGlobalUtilFunctions::GetPointerAbsolutePosition();
    if (Pointer.IsNearlyZero() || SavedTouchAbsolutePos.IsNearlyZero())
    {
        return;
    }
    const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
    if ((Pointer - SavedTouchAbsolutePos).Size() / ViewportScale < UEmployeeReactionComponent::CarryDragThresholdUnits)
    {
        return;
    }
    Reaction->BeginCarry(true);
}

bool UInteractableInputHandler::FinishEmployeeCarryPress()
{
    if (!bEmployeeCarryPress)
    {
        return false;
    }
    bEmployeeCarryPress = false;
    UEmployeeReactionComponent* Reaction = CarryPressReaction.Get();
    CarryPressReaction.Reset();
    if (Owner && Owner->MovementInputHandler)
    {
        Owner->MovementInputHandler->InitDragMoveIMC();
    }

    // 끌지 않은 짧은 탭은 기존 해제 경로가 처리한다(폭주 직원 탭 = 지금처럼 무반응, 잡기는 캐치 링)
    if (!Reaction || !Reaction->IsCarryActive())
    {
        return false;
    }
    Reaction->ReleaseCarry();
    bIsDragging = false;
    LastHitActor = nullptr;
    return true;
}

void UInteractableInputHandler::OpenWorkstationPanel(AWorkstationActorBase* Workstation, int32 FocusEmployeeID)
{
    if (!Workstation || !Owner)
    {
        return;
    }

    // 개발중 집중 모드 — 패널 오픈 차단(GetOverlappingActor 소스 차단의 방어선)
    if (IsOfficeFocusLocked())
    {
        return;
    }

    // 방문 모드에서는 인터랙션 차단
    if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
    {
        if (GI->IsVisitMode())
        {
            return;
        }
    }

    UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
    if (!GameInstance)
    {
        return;
    }

    UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
    UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();
    if (!TableManager || !UIManager)
    {
        return;
    }

    TSubclassOf<UUserWidget> PanelClass = TableManager->GetWidgetClass(EWidgetType::WorkstationInfo);
    if (!PanelClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[InteractableInputHandler] WorkstationInfo widget class not found in table"));
        return;
    }

    UUIBase* UIBase = UIManager->GetUIBase();
    if (!UIBase)
    {
        return;
    }

    UCommonActivatableWidget* PushedWidget = UIBase->PushBottomClass(PanelClass.Get());
    if (UWorkstationInfoWidget* InfoWidget = Cast<UWorkstationInfoWidget>(PushedWidget))
    {
        // 책상 클릭은 0번 좌석 기준, 직원 클릭은 그 직원 좌석 기준
        InfoWidget->SetWorkstationData(Workstation);
        InfoWidget->SetEmployeeData(FocusEmployeeID != INDEX_NONE ? FocusEmployeeID : Workstation->GetAssignedEmployeeID(0));

        if (AMainMapPlayerController* MainPC = Owner->GetPlayerController())
        {
            MainPC->GoToUIMode();
        }
    }
}

void UInteractableInputHandler::BeginFactoryHoldAfterDelay()
{
    UFactoryPanelWidget* FactoryPanel = GetActiveFactoryPanel();
    const bool bFactoryValid = IsValid(LastHitActor)
        && LastHitActor->ActorHasTag("Factory")
        && LastHitActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass());

    if (!CGRFactoryTapHoldPolicy::ShouldStartHold(
            FactoryPressPhase,
            bIsDragging,
            IsValid(FactoryPanel),
            bFactoryValid))
    {
        ResetFactoryPressState();
        return;
    }

    FactoryPressPhase = CGRFactoryTapHoldPolicy::EPhase::Holding;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    IInputHandler::Execute_OnInteract(LastHitActor, PC);
}

bool UInteractableInputHandler::FinishFactoryPress()
{
    if (FactoryPressPhase == CGRFactoryTapHoldPolicy::EPhase::None)
    {
        return false;
    }

    if (UWorld* HandlerWorld = GetWorld())
    {
        HandlerWorld->GetTimerManager().ClearTimer(FactoryHoldDelayTimerHandle);
    }

    const CGRFactoryTapHoldPolicy::EReleaseAction ReleaseAction =
        CGRFactoryTapHoldPolicy::ResolveRelease(FactoryPressPhase, bIsDragging);

    if (ReleaseAction == CGRFactoryTapHoldPolicy::EReleaseAction::ClosePanel)
    {
        if (UFactoryPanelWidget* FactoryPanel = GetActiveFactoryPanel())
        {
            FactoryPanel->RequestClose();
        }
        else
        {
            HandleFactoryTapOpen();
        }
    }
    else if (ReleaseAction == CGRFactoryTapHoldPolicy::EReleaseAction::StopProduction)
    {
        const bool bImplementsInputHandler = IsValid(LastHitActor)
            && LastHitActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass());
        CGRInteractableDragPolicy::TryCancelFactoryHold(
            true,
            LastHitActor,
            bImplementsInputHandler,
            [this](AActor* FactoryActor)
            {
                APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
                IInputHandler::Execute_OnEndInteract(FactoryActor, PC);
            });
    }

    ResetFactoryPressState();
    return true;
}

void UInteractableInputHandler::HandleFactoryTapOpen()
{
    if (!IsValid(Cast<ABrickFactory>(LastHitActor)))
    {
        return;
    }

    UUIManagerSubsystem* UIMgr = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    if (!UIMgr)
    {
        return;
    }

    // 오픈 절차는 UBuildOpenWidget::OnFactoryOpenButtonClicked 과 같다
    UTableManagerSubsystem* TableMgr = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    UUIBase* UIBase = UIMgr->GetUIBase();
    if (!TableMgr || !UIBase)
    {
        return;
    }
    if (TSubclassOf<UUserWidget> PanelClass = TableMgr->GetWidgetClass(EWidgetType::FactoryPanel))
    {
        UIBase->PushBottomClass(PanelClass.Get());
    }
}

void UInteractableInputHandler::ResetFactoryPressState()
{
    if (UWorld* HandlerWorld = GetWorld())
    {
        HandlerWorld->GetTimerManager().ClearTimer(FactoryHoldDelayTimerHandle);
    }
    else
    {
        FactoryHoldDelayTimerHandle.Invalidate();
    }

    FactoryPressPhase = CGRFactoryTapHoldPolicy::EPhase::None;
    SavedTouchAbsolutePos = FVector2D::ZeroVector;
    LastHitActor = nullptr;
}

UFactoryPanelWidget* UInteractableInputHandler::GetTopFactoryPanel() const
{
    UWorld* HandlerWorld = GetWorld();
    if (!HandlerWorld)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = HandlerWorld->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }

    UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();
    if (!UIManager)
    {
        return nullptr;
    }

    UUIBase* UIBase = UIManager->GetUIBase();
    if (!UIBase)
    {
        return nullptr;
    }

    return Cast<UFactoryPanelWidget>(UIBase->GetActiveBottomWidget());
}

UFactoryPanelWidget* UInteractableInputHandler::GetActiveFactoryPanel() const
{
    UFactoryPanelWidget* FactoryPanel = GetTopFactoryPanel();
    if (!IsValid(FactoryPanel) || FactoryPanel->IsCloseRequested())
    {
        return nullptr;
    }

    return FactoryPanel;
}

bool UInteractableInputHandler::IsOfficeFocusLocked() const
{
    if (const UWorld* World = GetWorld())
    {
        if (UOfficeStageProgressManager* SM = World->GetSubsystem<UOfficeStageProgressManager>())
        {
            return SM->IsFocusLocked();
        }
    }
    return false;
}
