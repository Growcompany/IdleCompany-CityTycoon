// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Officeworker/Officeworker.h"
#include "Global/CGDevSettings.h"
#include "GameMode/OfficeGameMode.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Office/WorkstationActorBase.h"
#include "Components/WidgetComponent.h"
#include "Components/Image.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Data/WorkerBubbleConfig.h"
#include "UI/Element/Office/FatigueBarWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/CGGameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

#include "Kismet/GameplayStatics.h"
#include "Manager/EntityManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Enum/ResourceType.h"
#include "Manager/ResourceItemManager.h"

// 사진찍는용
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Entity/Officeworker/PortraitOutline.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Engine/DirectionalLight.h"
#include "Components/MeshComponent.h"
#include "EngineUtils.h"
#include "Async/Async.h"
#include "RHIGPUReadback.h"
#include "RHISurfaceDataConversion.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"

struct FOfficeworkerPortraitCaptureState
{
    TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback;
    TAtomic<bool> bCopyEnqueued{false};
    TAtomic<bool> bCancelled{false};
    FString EmployeeID;
    FString FilePath;
    FString TemporaryPath;
    int32 Width = 0;
    int32 Height = 0;
    EPixelFormat Format = PF_Unknown;
};

namespace
{
    constexpr int32 PortraitOutputWidth = 288;
    constexpr int32 PortraitOutputHeight = 384;
    constexpr float OfficeworkerTapReactionSeconds = 1.5f;
    // 폰 터치 오차를 흡수하려고 이동 캡슐(반지름 30)보다 넓게 잡는다
    constexpr float OfficeworkerClickRadius = 40.f;
}

// Sets default values
AOfficeworker::AOfficeworker()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(30.f, 92.04f);

    GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.04f), FRotator(0.f, -90.f, 0.f));

    // 책상 BoxComponent 와 같은 전용 클릭 볼륨 — 이동 캡슐의 Pawn 프로필은 건드리지 않는다
    ClickVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("ClickVolume"));
    ClickVolume->SetupAttachment(GetCapsuleComponent());
    ClickVolume->InitCapsuleSize(OfficeworkerClickRadius, 92.04f);
    ClickVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ClickVolume->SetCollisionObjectType(ECC_WorldDynamic);
    ClickVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    ClickVolume->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    ClickVolume->SetGenerateOverlapEvents(false);
    ClickVolume->SetCanEverAffectNavigation(false);
    ClickVolume->CanCharacterStepUpOn = ECB_No;

    // 머리 위 상태 버블 (3D Widget Screen-space)
    BubbleWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("BubbleWidget"));
    BubbleWidgetComponent->SetupAttachment(GetCapsuleComponent());
    BubbleWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f)); // 머리 위
    BubbleWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
    BubbleWidgetComponent->SetDrawSize(FVector2D(96.f, 96.f));
    BubbleWidgetComponent->SetVisibility(false);
    BubbleWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
    // [Perf] Screen-space 위젯은 위치=페인트타임 스크린레이어, 값=소유자 Tick에서 푸시 → 컴포넌트 자체 틱 불필요(직원당 1개씩 제거).
    BubbleWidgetComponent->PrimaryComponentTick.bStartWithTickEnabled = false;

    // 머리 위 상시 피로 게이지 바 (전용 — 무드 버블과 독립적으로 항상 표시)
    FatigueBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("FatigueBarWidget"));
    FatigueBarComponent->SetupAttachment(GetCapsuleComponent());
    FatigueBarComponent->SetRelativeLocation(FVector(0.f, 0.f, FatigueBarHeightZ)); // 버블(110)보다 위 — 가로 바
    CurrentFatigueBarZ = FatigueBarHeightZ;
    FatigueBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
    FatigueBarComponent->SetDrawSize(FatigueBarDrawSize); // 실제 크기 권위 — BP 디테일에서 조절, OnConstruction 재적용
    FatigueBarComponent->SetPivot(FVector2D(0.5f, 0.5f));
    FatigueBarComponent->PrimaryComponentTick.bStartWithTickEnabled = false; // [Perf] 위와 동일 — 상시 표시 바의 매프레임 컴포넌트 틱 제거
    // WidgetClass 는 BeginPlay 에서 TableManager 로 로드 (프로젝트 컨벤션)

    // CDO에 BubbleConfig 자동 로드 — 모든 spawn 인스턴스가 상속
    static ConstructorHelpers::FObjectFinder<UWorkerBubbleConfig> ConfigFinder(
        TEXT("/Game/CompanyGrowth/Data/DA_WorkerBubbleConfig.DA_WorkerBubbleConfig"));
    if (ConfigFinder.Succeeded())
    {
        BubbleConfig = ConfigFinder.Object;
    }
    // BubbleWidget Class는 BeginPlay에서 TableManager로 로드 (프로젝트 컨벤션)

    // 모바일 최적화: 화면 밖 직원은 pose 갱신 스킵
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    // 화면 안 직원도 거리/화면크기에 따라 애님 평가를 건너뛴다. 코스메틱 10슬롯은 LeaderPose 라 바디만 제어하면 된다
    GetMesh()->bEnableUpdateRateOptimizations = true;

    GetCharacterMovement()->MaxWalkSpeed = 125.f;  // 걷기 애님 대비 이동속도 매칭(발 미끄러짐 완화)
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 180.f, 0.f);  // 부드러운 회전

    // AI Controller 자동 설정
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Portrait 캡처 카메라 - 생성만, 설정은 블루프린트에서
    FaceCaptureCamera = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FaceCaptureCamera"));
    FaceCaptureCamera->SetupAttachment(GetMesh());
    // 1컷 정지 촬영 전용 리그 — CaptureScene() 으로만 찍는다. 엔진 기본값(둘 다 true)이면 활성화된 순간부터 씬이 매 프레임 한 번 더 렌더된다
    FaceCaptureCamera->bCaptureEveryFrame = false;
    FaceCaptureCamera->bCaptureOnMovement = false;

    // Selection Overlay Material 로드
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> OverlayMaterialFinder(
        TEXT("/Game/KiAura/OverlayAuras/MI_AuraOutlinePulsingLowSpikes_White")
    );
    if (OverlayMaterialFinder.Succeeded())
    {
        SelectionOverlayMaterial = OverlayMaterialFinder.Object;
    }

    // Behavior Component 생성 (상태/피로도/수익 관리)
    BehaviorComponent = CreateDefaultSubobject<UEmployeeBehaviorComponent>(TEXT("BehaviorComponent"));
}

void AOfficeworker::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyOfficeLightingChannels();

    // 피로 바 크기/높이는 BP 디테일 값 권위 — 디폴트 변경이 뷰포트/스폰에 즉시 반영되게 여기서 재적용.
    if (FatigueBarComponent)
    {
        FatigueBarComponent->SetDrawSize(FatigueBarDrawSize);
        FatigueBarComponent->SetRelativeLocation(FVector(0.f, 0.f, FatigueBarHeightZ));
        CurrentFatigueBarZ = FatigueBarHeightZ;
        bFatigueBarZSettled = true;
    }
}

void AOfficeworker::ApplyOfficeLightingChannels(bool bGachaReveal)
{
    if (!GetWorld() || !GetWorld()->GetAuthGameMode<AOfficeGameMode>()) return;
    const bool bInteriorFill = !bIsPortraitMode && !bGachaReveal;
    TInlineComponentArray<UMeshComponent*> WorkerMeshes(this);
    for (UMeshComponent* WorkerMesh : WorkerMeshes)
    {
        WorkerMesh->SetLightingChannels(!bInteriorFill, bInteriorFill, false);
    }
}

void AOfficeworker::BeginPlay()
{
    ApplyOfficeLightingChannels();
    // 컴포넌트 BeginPlay의 지연 Idle 초기화가 촬영 리그 배회를 다시 켜지 않도록 먼저 차단한다.
    if (bIsPortraitMode && BehaviorComponent)
    {
        BehaviorComponent->bSkipAutoInit = true;
    }

    Super::BeginPlay();

    // 머리 위 버블 위젯 클래스 — TableManager에서 로드 (프로젝트 컨벤션)
    if (BubbleWidgetComponent)
    {
        if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance()))
        {
            if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
            {
                if (TSubclassOf<UUserWidget> BubbleWidgetClass = TableMgr->GetWidgetClass(EWidgetType::WorkerBubble))
                {
                    BubbleWidgetComponent->SetWidgetClass(BubbleWidgetClass);
                }

                // 피로 바도 같은 TableManager 경로로 로드 (상시 표시)
                if (FatigueBarComponent)
                {
                    if (TSubclassOf<UUserWidget> FatigueBarClass = TableMgr->GetWidgetClass(EWidgetType::WorkerFatigueBar))
                    {
                        FatigueBarComponent->SetWidgetClass(FatigueBarClass);
                    }
                }
            }
        }
    }

    // 블루프린트에서 추가한 FaceCaptureCamera 찾기
    if (FaceCaptureCamera)
    {
        FaceCaptureCamera->SetActive(false);
    }

    // Portrait 모드면 애니메이션/이동/중력 비활성화
    if (bIsPortraitMode)
    {
        // BeginPlay 전에 이미 무장된 배회 상태와 액터 타이머도 함께 정리한다.
        StopRandomRoaming();

        // 촬영 리그는 SceneCapture에는 남기되 플레이어 카메라에는 노출하지 않는다.
        TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
        GetComponents(PrimitiveComponents);
        for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
        {
            if (PrimitiveComponent)
            {
                PrimitiveComponent->SetVisibleInSceneCaptureOnly(true);
            }
        }

        // 촬영용 캐릭터에는 머리 위 피로 바 불필요 (혹시 모를 캡처 혼입 방지)
        if (FatigueBarComponent)
        {
            FatigueBarComponent->SetVisibility(false);
        }

        // 애니메이션 끄기
        if (GetMesh())
        {
            GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            GetMesh()->Stop();
        }

        // 이동/중력 끄기
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->GravityScale = 0.f;
            GetCharacterMovement()->SetMovementMode(MOVE_None);
            GetCharacterMovement()->StopMovementImmediately();
        }

        // AI 비활성화
        AAIController* AIC = Cast<AAIController>(GetController());
        if (AIC)
        {
            AIC->StopMovement();
        }

        // 행동 컴포넌트 틱 차단 — 안 끄면 촬영 리그가 OfficeMap 에 상주하면서 계속 Idle/Wander 로
        // 판정돼 방치 수익을 실제로 찍는다(NotifyEmployeesBehaviorMode 가 portrait 를 걸러내 영원히 Idle).
        // 화면 밖(Z≈-4864) 유령 수도꼭지 + EmployeeID<0 인 동안 10Hz O(N) FindEmployee 까지 돈다.
        if (BehaviorComponent)
        {
            BehaviorComponent->SetComponentTickEnabled(false);
        }

        UE_LOG(LogTemp, Log, TEXT("[Officeworker] Portrait mode enabled - Animation/Movement/Gravity disabled"));
    }

    // AssetManager가 초기화된 후 호출
    LoadAssetsAsync();
}

// Called every frame
void AOfficeworker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(PortraitReadbackTimer);
    if (PendingPortraitCapture)
    {
        PendingPortraitCapture->bCancelled = true;
        PendingPortraitCapture.Reset();
    }
    OnPortraitCaptured.Unbind();
    CanCommitPortrait.Unbind();
    Super::EndPlay(EndPlayReason);
}

void AOfficeworker::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateFatigueBarHeight(DeltaTime);

    // 1초 주기로 머리 위 상태 버블과 Glow를 평가한다.
    BubbleCheckAccum += DeltaTime;
    if (BubbleCheckAccum >= 1.0f)
    {
        BubbleCheckAccum = 0.0f;
        if (BehaviorComponent)
        {
            const bool bBuffActive = BehaviorComponent->HasActiveBuff();
            EWorkerBubbleType Desired = EWorkerBubbleType::None;

            // 우선순위: Buff종류별 > Tired > Working > None
            if (bBuffActive)
            {
                // ScoreMultiplier = Boosted, CritChance = Happy, WorkSpeed = Angry(분노의 작업속도)
                if (BehaviorComponent->GetTotalCritChanceBonus() > 0.0f)
                {
                    Desired = EWorkerBubbleType::Happy;
                }
                else if (BehaviorComponent->GetWorkSpeedMultiplier() < 1.0f)
                {
                    Desired = EWorkerBubbleType::Angry;  // 빨리 일하는 분노의 의지
                }
                else
                {
                    Desired = EWorkerBubbleType::Boosted;
                }
            }
            // 졸음 연출(Z 글자·콧방울)이 이미 같은 뜻을 보여 주면 피로 버블은 겹치지 않게 뺀다
            else if (BehaviorComponent->EmployeeState == EEmployeeState::Sitting && !IsDozeVisualActive())
            {
                // Sitting(휴식) 상태 = 피로한 상태 — 체력 기반 사이클로 자연스럽게 진입
                Desired = EWorkerBubbleType::Tired;
            }
            else if (BehaviorComponent->CurrentBehaviorMode == EEmployeeBehaviorMode::Stage
                && BehaviorComponent->EmployeeState == EEmployeeState::Typing)
            {
                Desired = EWorkerBubbleType::Working;
            }

            SetWorkerBubbleType(Desired);

            SetGlowOverlay(bBuffActive);

            // 머리 위 상시 피로 바 갱신 (값 변화 없으면 위젯이 no-op) — 화면 밖 워커는
            // BehaviorComponent 가 피로 동결하므로 값이 안 변해 자연히 갱신 0
            if (FatigueBarComponent)
            {
                if (UFatigueBarWidget* FatigueWidget = Cast<UFatigueBarWidget>(FatigueBarComponent->GetUserWidgetObject()))
                {
                    FatigueWidget->SetFatigue01(BehaviorComponent->GetFatigue01());
                }
            }
        }
    }

    // 랜덤 배회 중일 때 이동 완료 체크
    if (bIsRoaming)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            // 이동 중이 아니고 타이머도 없으면 → 이동 완료 상태
            UPathFollowingComponent* PathComp = AIC->GetPathFollowingComponent();
            bool bIsMoving = PathComp && PathComp->GetStatus() != EPathFollowingStatus::Idle;
            if (!bIsMoving && !GetWorld()->GetTimerManager().IsTimerActive(RoamTimerHandle))
            {
                OnMoveCompleted();
            }
        }
    }
}

void AOfficeworker::UpdateFatigueBarHeight(float DeltaTime)
{
    if (!FatigueBarComponent)
    {
        return;
    }

    const bool bSeated = BehaviorComponent && BehaviorComponent->bIsSeated;
    const float TargetZ = bSeated ? FatigueBarSeatedHeightZ : FatigueBarHeightZ;

    if (FMath::IsNearlyEqual(CurrentFatigueBarZ, TargetZ, 0.05f))
    {
        if (bFatigueBarZSettled)
        {
            return;
        }
        CurrentFatigueBarZ = TargetZ;
        bFatigueBarZSettled = true;
    }
    else
    {
        CurrentFatigueBarZ = FMath::FInterpTo(CurrentFatigueBarZ, TargetZ, DeltaTime, FatigueBarHeightInterpSpeed);
        bFatigueBarZSettled = false;
    }

    FatigueBarComponent->SetRelativeLocation(FVector(0.f, 0.f, CurrentFatigueBarZ));
}

void AOfficeworker::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
}

FVector AOfficeworker::GetCenterLocation() const
{
    // 발 위치(GetActorLocation) + 오프셋 = 대략 허리/배꼽 높이
    FVector Location = GetActorLocation();
    Location.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f;  // HalfHeight의 절반 (약 46)
    return Location;
}

// ========== 랜덤 배회 AI ==========

void AOfficeworker::StartRandomRoaming(float InRoamRadius, float InMinWaitTime, float InMaxWaitTime)
{
    StandUpFromWorkstation();

    // 이미 배회 중이면 무시가 아니라 재무장 — early-return 이던 시절, 잘못된 위치에서 걸린 첫 호출이
    // 뒤이은 올바른 호출(새 원점)을 통째로 삼켜 워커가 제자리에 갇혔다(2026-08-05 채용 직원 정지).
    if (bIsRoaming)
    {
        GetWorld()->GetTimerManager().ClearTimer(RoamTimerHandle);
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            AIC->StopMovement();
        }
    }

    bIsRoaming = true;
    RoamRadius = InRoamRadius;
    MinWaitTime = InMinWaitTime;
    MaxWaitTime = InMaxWaitTime;

    // 원점은 반드시 NavMesh 위여야 한다 — 좌석/책상 같은 구멍 안에서 시작하면
    // GetRandomReachablePointInRadius 가 영구 실패해 배회가 통째로 죽는다.
    // "투영 후 호출" 을 호출자 규약으로 두던 동안 두 번 깨졌으므로(2026-07-08, 2026-08-05) 여기서 직접 보정한다.
    RoamOrigin = GetActorLocation();
    if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation Projected;
        if (NavSys->ProjectPointToNavigation(RoamOrigin, Projected, FVector(1000.f, 1000.f, 1000.f)))
        {
            RoamOrigin = Projected.Location;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] StartRandomRoaming: NavMesh 투영 실패 — 배회 불가 (%.1f, %.1f, %.1f)"),
                EmployeeID, RoamOrigin.X, RoamOrigin.Y, RoamOrigin.Z);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Started roaming (RoamOrigin: %.1f, %.1f, %.1f / Radius: %.1f, Wait: %.1f~%.1f)"),
        EmployeeID, RoamOrigin.X, RoamOrigin.Y, RoamOrigin.Z, RoamRadius, MinWaitTime, MaxWaitTime);

    // 첫 이동 시작
    MoveToRandomLocation();
}

void AOfficeworker::StopRandomRoaming()
{
    if (!bIsRoaming)
    {
        return;
    }

    bIsRoaming = false;
    GetWorld()->GetTimerManager().ClearTimer(RoamTimerHandle);

    // AI 이동 중지
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->StopMovement();
    }

    UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Stopped roaming"), EmployeeID);
}

void AOfficeworker::ResumeRandomRoaming()
{
    StartRandomRoaming(RoamRadius, MinWaitTime, MaxWaitTime);
}

void AOfficeworker::MoveToRandomLocation()
{
    if (!bIsRoaming)
    {
        return;
    }

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] No navigation system found"), EmployeeID);
        return;
    }

    // NavMesh 위에서 랜덤 위치 찾기
    FNavLocation RandomLocation;
    bool bFound = NavSys->GetRandomReachablePointInRadius(RoamOrigin, RoamRadius, RandomLocation);

    if (bFound)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            // 이동 시작
            AIC->MoveToLocation(RandomLocation.Location, 50.f);

            UE_LOG(LogTemp, Verbose, TEXT("[Officeworker %d] Moving to (%.1f, %.1f, %.1f)"),
                EmployeeID, RandomLocation.Location.X, RandomLocation.Location.Y, RandomLocation.Location.Z);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] Failed to find random location"), EmployeeID);
    }
}

void AOfficeworker::OnMoveCompleted()
{
    if (!bIsRoaming)
    {
        return;
    }

    // 랜덤 대기 시간 후 다음 이동
    float WaitTime = FMath::RandRange(MinWaitTime, MaxWaitTime);
    GetWorld()->GetTimerManager().SetTimer(RoamTimerHandle, this, &AOfficeworker::MoveToRandomLocation, WaitTime, false);
}

void AOfficeworker::SetSelected(bool bSelected)
{
    // 상태만 관리 — 시각(오버레이)은 자식 override 담당 (모듈러=파츠 오버레이, 스틱=현재 시각 없음)
    bIsSelected = bSelected;
}

void AOfficeworker::SetupFaceCapture()
{
    if (!FaceCaptureCamera)
    {
        UE_LOG(LogTemp, Error, TEXT("[Portrait] FaceCaptureCamera not set! Actor: %s"), *GetName());
        return;
    }

    if (!FaceCaptureCamera->TextureTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("[Portrait] TextureTarget not set! Actor: %s, FaceCaptureCamera: %s"),
            *GetName(), *FaceCaptureCamera->GetName());
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Portrait] SetupFaceCapture OK - Actor: %s, TextureTarget: %s"),
        *GetName(), *FaceCaptureCamera->TextureTarget->GetName());

    // 2배 해상도에서 촬영·외곽선을 계산한 뒤 축소해 작은 머리카락과 안경의 계단을 줄인다.
    UTextureRenderTarget2D* PortraitTarget = FaceCaptureCamera->TextureTarget;
    if (PortraitTarget->SizeX != PortraitOutputWidth * 2 || PortraitTarget->SizeY != PortraitOutputHeight * 2)
    {
        PortraitTarget->ResizeTarget(PortraitOutputWidth * 2, PortraitOutputHeight * 2);
    }

    // 이 캐릭터만 캡처 (배경 제외) — 구 모듈러 워커에서 검증된 원래 동작
    FaceCaptureCamera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    FaceCaptureCamera->ShowOnlyActors.Empty();
    FaceCaptureCamera->ShowOnlyActors.Add(this);
    FaceCaptureCamera->ShowOnlyComponents.Empty();  // 이전 실험 잔재(BubbleWidget 흰 평면) 제거 — 매 캡처 초기화

    // 깔끔한 컷아웃: SceneColor(HDR) + 인버스 오파시티 알파 → 워커 픽셀만 불투명, 배경은 투명.
    // (CaptureAndSaveFacePortrait 의 A=255-A 알파 코드가 이 소스를 전제로 함.) 새 RT/카메라가
    // FinalColor 류면 하늘/조명/그림자가 통째로 구워지므로 코드에서 결정적으로 고정.
    FaceCaptureCamera->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
    // 릿 캡처 — 언릿 플랫 알베도는 "2D 종이" 느낌(조형광 0). 카메라 부착 키라이트의 N·L 셰이딩으로 입체감.
    // 알파는 커버리지 기반이라 셰이딩 밝기와 무관(컷아웃 안전). 하드 섀도만 차단.
    FaceCaptureCamera->ShowFlags.SetLighting(true);
    FaceCaptureCamera->ShowFlags.SetDynamicShadows(false);
    FaceCaptureCamera->ShowFlags.SetFog(false);
    FaceCaptureCamera->ShowFlags.SetAtmosphere(false);

    // 증명사진 키라이트(좌상단 플래시) — 리그는 맵 밖 격리 + 디렉셔널은 SceneCapture 에 기여 안 함(무대 실측)
    // → 자체 광원 필수. Unitless = 무대 FillLight 와 동일한 결정적 스케일.
    static const FName PortraitFlashName(TEXT("PortraitFlash"));
    UPointLightComponent* Flash = nullptr;
    TInlineComponentArray<UPointLightComponent*> RigLights(this);
    for (UPointLightComponent* L : RigLights)
    {
        if (L->GetFName() == PortraitFlashName)
        {
            Flash = L;
            break;
        }
    }
    if (!Flash)
    {
        Flash = NewObject<UPointLightComponent>(this, PortraitFlashName);
        Flash->RegisterComponent();
        Flash->AttachToComponent(FaceCaptureCamera, FAttachmentTransformRules::KeepRelativeTransform);
    }
    // 세팅은 생성 가드 밖 — Live Coding 튜닝이 기존 컴포넌트에도 즉시 반영되게
    Flash->SetRelativeLocation(FVector(-20.f, -35.f, 45.f));  // 카메라 기준 좌상단 — 스튜디오 키 방향
    Flash->SetIntensityUnits(ELightUnits::Unitless);
    Flash->SetIntensity(15000.f);                             // ⚠ 밝기 노브 (6000=어두움 실측 → 2.5배)
    Flash->SetAttenuationRadius(2000.f);
    Flash->SetCastShadows(false);
    Flash->SetVisibility(true);

    // 바디(애니 정지)가 캡처에서 컬링되지 않도록 본/바운드 강제 갱신.
    if (USkeletalMeshComponent* Body = GetMesh())
    {
        Body->bHiddenInSceneCapture = false;
        Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Body->bEnableUpdateRateOptimizations = false;  // 평가 스킵 프레임에 촬영되면 포즈가 어긋난다
        Body->RefreshBoneTransforms();
        Body->UpdateBounds();
        Body->MarkRenderStateDirty();

        // 자동 프레이밍(옵션): 카메라를 바디 헤드에 바운드 기준으로 정렬. 0.09 스케일/오프셋 무관 안전망.
        // bAutoFramePortraitCamera=false(기본)면 BP/RT 에서 직접 잡은 카메라 위치·거리·FOV 를 그대로 둔다.
        if (bAutoFramePortraitCamera)
        {
            const FBoxSphereBounds B = Body->Bounds;
            const float Radius = FMath::Max(B.SphereRadius, 1.0f);
            const FVector Head = Body->DoesSocketExist(FName("head")) ? Body->GetSocketLocation(FName("head")) : B.Origin;
            const FVector Fwd = GetActorForwardVector();
            const FVector CamPos = Head + Fwd * (Radius * 2.4f) + FVector(0.f, 0.f, Radius * 0.10f);
            FaceCaptureCamera->SetWorldLocation(CamPos);
            FaceCaptureCamera->SetWorldRotation((Head - CamPos).Rotation());
            FaceCaptureCamera->ProjectionType = ECameraProjectionMode::Perspective;
            FaceCaptureCamera->FOVAngle = 35.f;
        }
    }

    FaceCaptureCamera->SetActive(true);
}

void AOfficeworker::CaptureAndSaveFacePortrait()
{
    const FString CapturedID = FString::FromInt(EmployeeID);
    if (PendingPortraitCapture)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Portrait] Capture already in progress: %s"), *PendingPortraitCapture->EmployeeID);
        return;
    }
    UTextureRenderTarget2D* Target = FaceCaptureCamera ? FaceCaptureCamera->TextureTarget : nullptr;
    if (!Target || !Target->GameThread_GetRenderTargetResource())
    {
        if (FaceCaptureCamera) { FaceCaptureCamera->SetActive(false); }
        OnPortraitCaptured.ExecuteIfBound(CapturedID, false);
        return;
    }
    const EPixelFormat TargetFormat = Target->GetFormat();
    if (TargetFormat != PF_FloatRGBA && TargetFormat != PF_B8G8R8A8 && TargetFormat != PF_R8G8B8A8)
    {
        UE_LOG(LogTemp, Error, TEXT("[Portrait] Unsupported render target format: %s"), GetPixelFormatString(TargetFormat));
        FaceCaptureCamera->SetActive(false);
        OnPortraitCaptured.ExecuteIfBound(CapturedID, false);
        return;
    }
    // ImageUtils cannot load ImageWrapper from its worker-thread compression path.
    if (!FModuleManager::Get().LoadModule(FName(TEXT("ImageWrapper"))))
    {
        FaceCaptureCamera->SetActive(false);
        OnPortraitCaptured.ExecuteIfBound(CapturedID, false);
        return;
    }

    TSharedPtr<FOfficeworkerPortraitCaptureState, ESPMode::ThreadSafe> State =
        MakeShared<FOfficeworkerPortraitCaptureState, ESPMode::ThreadSafe>();
    State->Readback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("OfficeworkerPortrait"));
    State->EmployeeID = CapturedID;
    State->FilePath = GetPortraitFilePathForEmployee(EmployeeID);
    State->TemporaryPath = State->FilePath + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
    State->Width = Target->SizeX;
    State->Height = Target->SizeY;
    State->Format = TargetFormat;
    PendingPortraitCapture = State;

    FaceCaptureCamera->CaptureScene();
    // 캡처는 위에서 렌더 명령으로 이미 큐에 들어갔다 — 이후 끄면 촬영엔 남고 본 씬의 동적 광원 비용만 사라진다
    TInlineComponentArray<UPointLightComponent*> RigLights(this);
    for (UPointLightComponent* RigLight : RigLights)
    {
        if (RigLight->GetFName() == TEXT("PortraitFlash")) { RigLight->SetVisibility(false); }
    }
    FTextureRenderTargetResource* TargetResource = Target->GameThread_GetRenderTargetResource();
    ENQUEUE_RENDER_COMMAND(EnqueueOfficeworkerPortraitReadback)(
        [TargetResource, State](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* SourceTexture = TargetResource->GetShaderResourceTexture();
            if (SourceTexture && SourceTexture->GetFormat() == State->Format && !SourceTexture->IsMultisampled())
            {
                State->Readback->EnqueueCopy(RHICmdList, SourceTexture);
            }
            else
            {
                State->bCancelled = true;
            }
            State->bCopyEnqueued = true;
        });
    GetWorldTimerManager().SetTimer(PortraitReadbackTimer, this, &AOfficeworker::PollPortraitReadback, 0.01f, true);
}

void AOfficeworker::PollPortraitReadback()
{
    TSharedPtr<FOfficeworkerPortraitCaptureState, ESPMode::ThreadSafe> State = PendingPortraitCapture;
    if (!State || !State->bCopyEnqueued) { return; }
    if (State->bCancelled)
    {
        FinishPortraitCapture(State, false);
        return;
    }
    if (!State->Readback->IsReady()) { return; }
    GetWorldTimerManager().ClearTimer(PortraitReadbackTimer);

    const TWeakObjectPtr<AOfficeworker> WeakWorker(this);
    ENQUEUE_RENDER_COMMAND(CopyOfficeworkerPortraitReadback)(
        [State, WeakWorker](FRHICommandListImmediate&)
        {
            int32 RowPitchInPixels = 0;
            int32 BufferHeight = 0;
            void* MappedData = State->Readback->Lock(RowPitchInPixels, &BufferHeight);
            const int32 BytesPerPixel = State->Format == PF_FloatRGBA ? 8 : 4;
            const bool bValid = MappedData && RowPitchInPixels >= State->Width && BufferHeight >= State->Height;
            TArray<uint8> RawPixels;
            if (bValid)
            {
                RawPixels.SetNumUninitialized(State->Width * State->Height * BytesPerPixel);
                for (int32 Y = 0; Y < State->Height; ++Y)
                {
                    FMemory::Memcpy(RawPixels.GetData() + Y * State->Width * BytesPerPixel,
                        static_cast<const uint8*>(MappedData) + Y * RowPitchInPixels * BytesPerPixel,
                        State->Width * BytesPerPixel);
                }
            }
            if (MappedData) { State->Readback->Unlock(); }
            if (!bValid || State->bCancelled)
            {
                AsyncTask(ENamedThreads::GameThread, [State, WeakWorker]()
                {
                    if (WeakWorker.IsValid()) { WeakWorker->FinishPortraitCapture(State, false); }
                });
                return;
            }

            Async(EAsyncExecution::ThreadPool, [State, WeakWorker, RawPixels = MoveTemp(RawPixels)]() mutable
            {
                TArray<FColor> Pixels;
                Pixels.SetNumUninitialized(State->Width * State->Height);
                const uint32 SourcePitch = State->Width * (State->Format == PF_FloatRGBA ? 8 : 4);
                if (State->Format == PF_FloatRGBA)
                {
                    // ReadPixels uses RCM_UNorm and gamma conversion for half-float targets.
                    ConvertRawR16G16B16A16FDataToFColor(State->Width, State->Height,
                        RawPixels.GetData(), SourcePitch, Pixels.GetData(), true);
                }
                else if (State->Format == PF_B8G8R8A8)
                {
                    ConvertRawB8G8R8A8DataToFColor(State->Width, State->Height,
                        RawPixels.GetData(), SourcePitch, Pixels.GetData());
                }
                else
                {
                    ConvertRawR8G8B8A8DataToFColor(State->Width, State->Height,
                        RawPixels.GetData(), SourcePitch, Pixels.GetData());
                }

                for (FColor& Pixel : Pixels) { Pixel.A = 255 - Pixel.A; }
                FPortraitOutlineParams Outline;
                Outline.ThicknessPx = 3.f * static_cast<float>(State->Width) / PortraitOutputWidth;
                PortraitOutline::Apply(Pixels, State->Width, State->Height, Outline);
                TArray<FColor> RGBPixels;
                TArray<FColor> AlphaPixels;
                RGBPixels.Reserve(Pixels.Num());
                AlphaPixels.Reserve(Pixels.Num());
                for (const FColor& Pixel : Pixels)
                {
                    RGBPixels.Add(FColor(Pixel.R, Pixel.G, Pixel.B, 255));
                    AlphaPixels.Add(FColor(Pixel.A, Pixel.A, Pixel.A, 255));
                }
                TArray<FColor> ResizedRGB;
                TArray<FColor> ResizedAlpha;
                FImageUtils::ImageResize(State->Width, State->Height, RGBPixels,
                    PortraitOutputWidth, PortraitOutputHeight, ResizedRGB, false);
                FImageUtils::ImageResize(State->Width, State->Height, AlphaPixels,
                    PortraitOutputWidth, PortraitOutputHeight, ResizedAlpha, false);
                TArray<FColor> OutputPixels;
                OutputPixels.SetNumUninitialized(PortraitOutputWidth * PortraitOutputHeight);
                for (int32 Index = 0; Index < OutputPixels.Num(); ++Index)
                {
                    OutputPixels[Index] = FColor(ResizedRGB[Index].R, ResizedRGB[Index].G,
                        ResizedRGB[Index].B, ResizedAlpha[Index].R);
                }

                TArray<uint8, FDefaultAllocator64> CompressedData;
                FImageUtils::PNGCompressImageArray(PortraitOutputWidth, PortraitOutputHeight,
                    OutputPixels, CompressedData);
                const FString Directory = FPaths::GetPath(State->FilePath);
                const bool bTemporarySaved = !State->bCancelled && CompressedData.Num() > 0
                    && IFileManager::Get().MakeDirectory(*Directory, true)
                    && FFileHelper::SaveArrayToFile(CompressedData, *State->TemporaryPath);
                AsyncTask(ENamedThreads::GameThread, [State, WeakWorker, bTemporarySaved]()
                {
                    if (WeakWorker.IsValid()) { WeakWorker->FinishPortraitCapture(State, bTemporarySaved); }
                    else { IFileManager::Get().Delete(*State->TemporaryPath); }
                });
            });
        });
}

void AOfficeworker::FinishPortraitCapture(
    const TSharedPtr<FOfficeworkerPortraitCaptureState, ESPMode::ThreadSafe>& State, bool bTemporarySaved)
{
    if (PendingPortraitCapture != State)
    {
        IFileManager::Get().Delete(*State->TemporaryPath);
        return;
    }
    GetWorldTimerManager().ClearTimer(PortraitReadbackTimer);
    const bool bCanCommit = !State->bCancelled && EmployeeID == FCString::Atoi(*State->EmployeeID)
        && (!CanCommitPortrait.IsBound() || CanCommitPortrait.Execute());
    const bool bSaved = bTemporarySaved && bCanCommit
        && IFileManager::Get().Move(*State->FilePath, *State->TemporaryPath, true, true);
    if (!bSaved) { IFileManager::Get().Delete(*State->TemporaryPath); }
    if (FaceCaptureCamera) { FaceCaptureCamera->SetActive(false); }
    PendingPortraitCapture.Reset();
    OnPortraitCaptured.ExecuteIfBound(State->EmployeeID, bSaved);
}
FString AOfficeworker::GetPortraitFilePath() const
{
    return GetPortraitFilePathForEmployee(EmployeeID);
}

FString AOfficeworker::GetPortraitFilePathForEmployee(int32 InEmployeeID)
{
    FString BasePath;
    FString EmployeeIDStr = FString::FromInt(InEmployeeID);

#if PLATFORM_ANDROID
    // Android: External Storage 사용 (접근 가능)
    extern FString GExternalFilePath;
    BasePath = GExternalFilePath;
#elif PLATFORM_IOS
    // iOS: Documents 폴더
    BasePath = FPaths::ProjectSavedDir();
#else
    // PC
    BasePath = FPaths::ProjectSavedDir();
#endif

    FString FullPath = BasePath / UCGDevSettings::GetPortraitFolderName() / (EmployeeIDStr + TEXT(".png"));

    return FullPath;
}

// ========== ABP 연동 Getter ==========

EEmployeeState AOfficeworker::GetEmployeeState() const
{
    return BehaviorComponent ? BehaviorComponent->EmployeeState : EEmployeeState::Sitting;
}

bool AOfficeworker::IsSeated() const
{
    return BehaviorComponent ? BehaviorComponent->bIsSeated : true;
}

bool AOfficeworker::IsWalking() const
{
    return BehaviorComponent ? BehaviorComponent->Walking : false;
}

float AOfficeworker::GetSpeed() const
{
    return BehaviorComponent ? BehaviorComponent->Speed : 0.0f;
}

// ========== Workstation 할당 ==========

void AOfficeworker::SetAssignedWorkstation(AWorkstationActorBase* Workstation, int32 SeatIndex)
{
    if (AssignedWorkstation.Get() != Workstation || AssignedSeatIndex != SeatIndex)
    {
        StandUpFromWorkstation();
    }

    AssignedWorkstation = Workstation;
    AssignedSeatIndex = SeatIndex;

    if (Workstation)
    {
        UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Assigned to Workstation: %s, SeatIndex: %d"),
            EmployeeID, *Workstation->GetName(), SeatIndex);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Workstation assignment cleared"), EmployeeID);
    }
}

bool AOfficeworker::TeleportToWorkstationAndSit()
{
    if (!AssignedWorkstation.IsValid() || AssignedSeatIndex < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] No assigned workstation for teleport"), EmployeeID);
        return false;
    }

    AWorkstationActorBase* Workstation = AssignedWorkstation.Get();

    const int32 OccupiedSeatIndex = Workstation->FindWorkerSeatIndex(this);
    if ((OccupiedSeatIndex >= 0 && OccupiedSeatIndex != AssignedSeatIndex)
        || (OccupiedSeatIndex < 0
            && (!Workstation->CanSitAt(AssignedSeatIndex) || !Workstation->SitDownAt(AssignedSeatIndex, this))))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] Cannot sit at workstation seat %d"), EmployeeID, AssignedSeatIndex);
        return false;
    }

    StopRandomRoaming();
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->StopMovement();
    }
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    // 좌석 위치와 회전값 가져오기
    FVector SeatLocation = Workstation->GetSeatLocation(AssignedSeatIndex);
    FRotator SeatRotation = Workstation->GetSeatRotation(AssignedSeatIndex);

    // 순간이동
    SetActorLocation(SeatLocation);
    SetActorRotation(SeatRotation);

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        if (Movement->MovementMode != MOVE_None)
        {
            Movement->DisableMovement();
            bMovementLockedBySeat = true;
        }
    }

    // BehaviorComponent 상태 업데이트
    if (BehaviorComponent)
    {
        BehaviorComponent->bIsSeated = true;
        BehaviorComponent->Walking = false;
        BehaviorComponent->Speed = 0.0f;
    }

    UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Teleported to workstation and sitting at seat %d"),
        EmployeeID, AssignedSeatIndex);
    return true;
}

void AOfficeworker::MoveToWorkstationArea()
{
    StandUpFromWorkstation();

    if (!AssignedWorkstation.IsValid() || AssignedSeatIndex < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] No assigned workstation for move"), EmployeeID);
        return;
    }

    const FVector TargetLocation = GetWorkstationApproachLocation();

    AAIController* AIC = Cast<AAIController>(GetController());
    if (AIC)
    {
        // AI 이동 시작
        const bool bStopAtCapsuleEdge = !BehaviorComponent || !BehaviorComponent->IsRecruitmentArrivalActive();
        AIC->MoveToLocation(TargetLocation, 50.f, bStopAtCapsuleEdge);

        // 이동 완료 감지를 위한 콜백 설정
        // Tick에서 이동 완료 시 텔레포트 처리
        UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Moving to workstation area at %s"),
            EmployeeID, *TargetLocation.ToString());
    }
}

FVector AOfficeworker::GetWorkstationApproachLocation() const
{
    const AWorkstationActorBase* Workstation = AssignedWorkstation.Get();
    if (!Workstation || AssignedSeatIndex < 0) return GetActorLocation();
    FVector Approach = Workstation->GetSeatLocation(AssignedSeatIndex)
        + Workstation->GetActorForwardVector() * 100.f;
    if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation ProjectedApproach;
        if (NavSys->ProjectPointToNavigation(Approach, ProjectedApproach, FVector(180.f, 180.f, 250.f)))
        {
            Approach = ProjectedApproach.Location;
        }
    }
    return Approach;
}

void AOfficeworker::StandUpFromWorkstation()
{
    AWorkstationActorBase* Workstation = AssignedWorkstation.Get();
    const bool bOccupiedSeat = Workstation && Workstation->FindWorkerSeatIndex(this) >= 0;
    if (!bOccupiedSeat && !bMovementLockedBySeat)
    {
        return;
    }

    if (bOccupiedSeat)
    {
        Workstation->StandUp(this);
    }

    if (bMovementLockedBySeat)
    {
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->SetMovementMode(Movement->DefaultLandMovementMode);
        }
        bMovementLockedBySeat = false;
    }

    // BehaviorComponent 상태 업데이트
    if (BehaviorComponent)
    {
        BehaviorComponent->bIsSeated = false;
    }

    UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Stood up from workstation seat %d"), EmployeeID, AssignedSeatIndex);
}

// ===== 머리 위 상태 버블 =====

void AOfficeworker::SetWorkerBubbleType(EWorkerBubbleType NewType)
{
    if (CurrentBubbleType == NewType) return;

    CurrentBubbleType = NewType;
    // 클릭 반응 중에는 상태만 기억해 두고 화면은 반응이 끝날 때 맞춘다
    if (bTapReactionActive) return;

    ApplyWorkerBubbleState();
}

void AOfficeworker::SetOverheadUISuppressed(bool bSuppressed)
{
    if (bOverheadUISuppressed == bSuppressed) return;
    bOverheadUISuppressed = bSuppressed;

    if (FatigueBarComponent && !bIsPortraitMode)
    {
        FatigueBarComponent->SetVisibility(!bSuppressed);
    }
    // 클릭 반응 중이면 EndTapReaction 이 상태 버블로 돌아갈 때 이 플래그를 따른다
    if (!bTapReactionActive)
    {
        ApplyWorkerBubbleState();
    }
}

void AOfficeworker::ApplyWorkerBubbleState()
{
    GetWorldTimerManager().ClearTimer(WorkingBubbleTimerHandle);
    if (!BubbleWidgetComponent) return;

    if (bOverheadUISuppressed)
    {
        BubbleWidgetComponent->SetVisibility(false);
        return;
    }

    // 작업 시작 시에도 직원마다 시차를 둬 모든 머리 위에 동시에 뜨지 않게 한다.
    if (CurrentBubbleType == EWorkerBubbleType::Working)
    {
        BubbleWidgetComponent->SetVisibility(false);
        GetWorldTimerManager().SetTimer(WorkingBubbleTimerHandle, this,
            &AOfficeworker::ShowWorkingBubble, FMath::FRandRange(2.0f, 6.0f), false);
        return;
    }

    if (CurrentBubbleType == EWorkerBubbleType::None)
    {
        BubbleWidgetComponent->SetVisibility(false);
        return;
    }

    RefreshWorkerBubbleVisual();
}

void AOfficeworker::PlayTapReaction()
{
    UWorkerBubbleConfig* Config = BubbleConfig.IsNull() ? nullptr : BubbleConfig.LoadSynchronous();
    PlayEmoteReaction(Config ? Config->PickEmoteForState(EWorkerBubbleType::Happy) : nullptr, OfficeworkerTapReactionSeconds);
}

bool AOfficeworker::PlayEmoteReaction(UTexture2D* Emote, float Seconds)
{
    if (!Emote) return false;

    // 작업 중 버블 타이머가 반응 도중 이모트를 바꿔치기하지 않게 먼저 멈춘다
    GetWorldTimerManager().ClearTimer(WorkingBubbleTimerHandle);
    if (!ShowBubbleEmote(Emote))
    {
        ApplyWorkerBubbleState();
        return false;
    }

    bTapReactionActive = true;
    GetWorldTimerManager().SetTimer(TapReactionTimerHandle, this,
        &AOfficeworker::EndTapReaction, FMath::Max(0.05f, Seconds), false);
    return true;
}

void AOfficeworker::EndTapReaction()
{
    bTapReactionActive = false;
    ApplyWorkerBubbleState();
}

void AOfficeworker::ShowWorkingBubble()
{
    if (CurrentBubbleType != EWorkerBubbleType::Working || bOverheadUISuppressed) return;

    RefreshWorkerBubbleVisual();
    GetWorldTimerManager().SetTimer(WorkingBubbleTimerHandle, this,
        &AOfficeworker::HideWorkingBubble, 2.0f, false);
}

void AOfficeworker::HideWorkingBubble()
{
    if (CurrentBubbleType != EWorkerBubbleType::Working) return;

    if (BubbleWidgetComponent)
    {
        BubbleWidgetComponent->SetVisibility(false);
    }
    GetWorldTimerManager().SetTimer(WorkingBubbleTimerHandle, this,
        &AOfficeworker::ShowWorkingBubble, FMath::FRandRange(10.0f, 14.0f), false);
}

void AOfficeworker::RefreshWorkerBubbleVisual()
{
    if (!BubbleWidgetComponent) return;
    BubbleWidgetComponent->SetVisibility(false);

    // Config에서 상태별 이모트 풀 → 랜덤 1장 → WBP의 EmoteImage에 적용
    UWorkerBubbleConfig* Config = BubbleConfig.IsNull() ? nullptr : BubbleConfig.LoadSynchronous();
    if (!Config) return;

    ShowBubbleEmote(Config->PickEmoteForState(CurrentBubbleType));
}

bool AOfficeworker::ShowBubbleEmote(UTexture2D* Emote)
{
    if (!Emote || !BubbleWidgetComponent) return false;

    UUserWidget* BubbleUserWidget = BubbleWidgetComponent->GetUserWidgetObject();
    if (!BubbleUserWidget) return false;

    UImage* EmoteImage = Cast<UImage>(BubbleUserWidget->GetWidgetFromName(TEXT("EmoteImage")));
    if (!EmoteImage) return false;

    // bMatchSize=true → 텍스처 원본 해상도(가로/세로비) 그대로 적용. 슬롯이 Center면 stretch 안 됨
    EmoteImage->SetBrushFromTexture(Emote, true);
    BubbleWidgetComponent->SetVisibility(true);

    // 너무 크면 96px로 fit (긴 변 기준)
    const float MaxDim = 96.0f;
    const float W = static_cast<float>(Emote->GetSizeX());
    const float H = static_cast<float>(Emote->GetSizeY());
    const float Longer = FMath::Max(W, H);
    if (Longer > MaxDim)
    {
        const float Scale = MaxDim / Longer;
        EmoteImage->SetDesiredSizeOverride(FVector2D(W * Scale, H * Scale));
    }
    else
    {
        EmoteImage->SetDesiredSizeOverride(FVector2D(W, H));
    }
    return true;
}

void AOfficeworker::SetGlowOverlay(bool bEnabled, FLinearColor Color)
{
    // 베이스는 no-op — 시각(오버레이)은 자식 override 담당 (모듈러=파츠 오버레이, 스틱=현재 시각 없음)
}
