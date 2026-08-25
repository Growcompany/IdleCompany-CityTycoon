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
#include "Interfaces/IInputHandler.h"
#include "Entity/Factory/BrickFactory.h"
#include "Kismet/GameplayStatics.h"
#include "Util/CoordinateUtils.h"
#include "Input/InputPriorities.h"
#include "Office/WorkstationActorBase.h"
#include "Office/DecorationActor.h"
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
#include "Enum/WidgetType.h"
#include "TimerManager.h"

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
    ResetFactoryPressState();
    Super::EndPlay(EndPlayReason);
}

void UInteractableInputHandler::OnInteractionStarted(const FInputActionValue& Value)
{
    if (!CoordinateUtils::SingleTouchCheck())
        return;

    // 터치 위치 저장
    FVector2D screenPos;
    FVector worldPos;
    if (CoordinateUtils::ProjectTouchToGroundPlane(screenPos, worldPos))
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

    if (currentMode == EInputMode::BuildPlace)
    {
        // BuildPlace 모드 처리 - 건물, 업무공간, 장식품 배치 대상 확인
        AActor* PlacementTarget = nullptr;

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

            APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
            IInputHandler::Execute_OnInteract(LastHitActor, PC);
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Interactable clicked"));
            UE_LOG(LogTemp, Log, TEXT("Interactable clicked"));
        }
    }
}

void UInteractableInputHandler::OnInteractionTriggered(const FInputActionValue& Value)
{
    // 실제 드래그 거리로 판단
    FVector2D currentScreenPos;
    FVector currentWorldPos;

    if (CoordinateUtils::ProjectTouchToGroundPlane(currentScreenPos, currentWorldPos))
    {
        float dragDistance = FVector2D::Distance(SavedTouchScreenPos, currentScreenPos);
        const float DragThreshold = 30.0f; // 작게 설정해서 민감하게 반응
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
            else if (FactoryDragAction == CGRFactoryTapHoldPolicy::EDragAction::StopProduction)
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
                ResetFactoryPressState();
            }

            bIsDragging = true;

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

void UInteractableInputHandler::OnInteractionFinish(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Error, TEXT("[UInteractableInputHandler] Click-Release"));

    // 배치 드래그 상태 해제
    Owner->PlacementHandler->StopDragging();

    if (FinishFactoryPress())
    {
        Owner->MovementInputHandler->InitDragMoveIMC();
        bIsDragging = false;
        LastHitActor = nullptr;
        return;
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
    }
    // 정리
    Owner->MovementInputHandler->InitDragMoveIMC();
    bIsDragging = false;
    LastHitActor = nullptr;
}

void UInteractableInputHandler::OnInteractionCanceled(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Error, TEXT("[OnInteractionCanceled] Quick release - 0.2 second"));

    if (FinishFactoryPress())
    {
        bIsDragging = false;
        LastHitActor = nullptr;
        return;
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
    }

    // 정리
    bIsDragging = false;
    LastHitActor = nullptr;
}

void UInteractableInputHandler::OnTapCompleted(const FInputActionValue& Value)
{
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

    // 일반 모드: Single 레이캐스트
    FHitResult HitResult;
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        WorldLocation,
        TraceEnd,
        ECC_GameTraceChannel1,
        QueryParams
    );

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

        // 인터페이스 기반 일반 인터랙터블 (AInteractableBaseActor 비파생 — 예: 도시 인수 클릭 프록시)
        if (HitActor->ActorHasTag("Interactable") &&
            HitActor->GetClass()->ImplementsInterface(UInputHandler::StaticClass()))
        {
            return HitActor;
        }
    }

    return nullptr;
}

void UInteractableInputHandler::OpenWorkstationPanel(AWorkstationActorBase* Workstation)
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
        // v1 = seat 0 기준 (더블 책상 좌석별 선택은 보류)
        InfoWidget->SetWorkstationData(Workstation);
        InfoWidget->SetEmployeeData(Workstation->GetAssignedEmployeeID(0));

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
