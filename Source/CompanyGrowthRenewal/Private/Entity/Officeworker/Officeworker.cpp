// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Officeworker/Officeworker.h"
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
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"

// Sets default values
AOfficeworker::AOfficeworker()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(30.f, 92.04f);

    GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.04f), FRotator(0.f, -90.f, 0.f));

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

    // 모바일 최적화: 화면 밖 직원은 pose 갱신 스킵 (모듈러 파츠는 AModularOfficeworker 가 처리)
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

    GetCharacterMovement()->MaxWalkSpeed = 125.f;  // 걷기 애님 대비 이동속도 매칭(발 미끄러짐 완화)
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 180.f, 0.f);  // 부드러운 회전

    // AI Controller 자동 설정
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Portrait 캡처 카메라 - 생성만, 설정은 블루프린트에서
    FaceCaptureCamera = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FaceCaptureCamera"));
    FaceCaptureCamera->SetupAttachment(GetMesh());

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

    // 피로 바 크기/높이는 BP 디테일 값 권위 — 디폴트 변경이 뷰포트/스폰에 즉시 반영되게 여기서 재적용.
    if (FatigueBarComponent)
    {
        FatigueBarComponent->SetDrawSize(FatigueBarDrawSize);
        FatigueBarComponent->SetRelativeLocation(FVector(0.f, 0.f, FatigueBarHeightZ));
        CurrentFatigueBarZ = FatigueBarHeightZ;
        bFatigueBarZSettled = true;
    }
}

void AOfficeworker::BeginPlay()
{
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
void AOfficeworker::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateFatigueBarHeight(DeltaTime);

    // 1초 주기로 머리 위 버블 + Glow 평가 + 이모트 변형 (성능 절약)
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
            else if (BehaviorComponent->EmployeeState == EEmployeeState::Sitting)
            {
                // Sitting(휴식) 상태 = 피로한 상태 — 체력 기반 사이클로 자연스럽게 진입
                Desired = EWorkerBubbleType::Tired;
            }
            else if (BehaviorComponent->CurrentBehaviorMode == EEmployeeBehaviorMode::Stage)
            {
                Desired = EWorkerBubbleType::Working;
            }

            // 같은 타입이어도 1초마다 이모트 변형 (다채로움)
            if (Desired == CurrentBubbleType && Desired != EWorkerBubbleType::None)
            {
                // 강제 재선택 — CurrentBubbleType을 None으로 잠깐 바꿔 캐시 무효화
                EWorkerBubbleType Same = CurrentBubbleType;
                CurrentBubbleType = EWorkerBubbleType::None;
                SetWorkerBubbleType(Same);
            }
            else
            {
                SetWorkerBubbleType(Desired);
            }

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

    // 바디(애니 정지)가 캡처에서 컬링되지 않도록 본/바운드 강제 갱신.
    if (USkeletalMeshComponent* Body = GetMesh())
    {
        Body->bHiddenInSceneCapture = false;
        Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
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
    FString EmployeeIDStr = FString::FromInt(this->EmployeeID);
    UE_LOG(LogTemp, Warning, TEXT("[Portrait] CaptureAndSaveFacePortrait called with EmployeeID: %d"), this->EmployeeID);

    if (!FaceCaptureCamera || !FaceCaptureCamera->TextureTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("CaptureAndSaveFacePortrait: Capture system not initialized!"));
        return;
    }

    // 1. 캡처
    FaceCaptureCamera->CaptureScene();

    // 2. RenderTarget에서 픽셀 읽기
    UTextureRenderTarget2D* RT = FaceCaptureCamera->TextureTarget;
    FTextureRenderTargetResource* RTResource = RT->GameThread_GetRenderTargetResource();

    TArray<FColor> Pixels;
    if (!RTResource->ReadPixels(Pixels))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to read pixels!"));
        return;
    }

    // 알파 채널을 먼저 처리 (OneMinus)
    for (FColor& Pixel : Pixels)
    {
        Pixel.A = 255 - Pixel.A;
    }

    // RGB와 알파를 분리해서 리사이즈
    TArray<FColor> RGBPixels;
    TArray<FColor> AlphaAsColor;  // uint8 배열 대신 FColor 사용
    RGBPixels.Reserve(Pixels.Num());
    AlphaAsColor.Reserve(Pixels.Num());

    for (const FColor& Pixel : Pixels)
    {
        RGBPixels.Add(FColor(Pixel.R, Pixel.G, Pixel.B, 255));
        AlphaAsColor.Add(FColor(Pixel.A, Pixel.A, Pixel.A, 255)); // 알파값을 RGB에 복사
    }

    // RGB 리사이즈 (3:4 비율)
    TArray<FColor> ResizedRGB;
    FImageUtils::ImageResize(
        RT->SizeX,
        RT->SizeY,
        RGBPixels,
        144,
        192,
        ResizedRGB,
        false
    );

    // 알파 채널도 Bilinear 보간으로 리사이즈
    TArray<FColor> ResizedAlphaColor;
    FImageUtils::ImageResize(
        RT->SizeX,
        RT->SizeY,
        AlphaAsColor,
        144,
        192,
        ResizedAlphaColor,
        false
    );

    // RGB와 알파 합치기
    TArray<FColor> ResizedPixels;
    ResizedPixels.SetNum(144 * 192);

    for (int32 i = 0; i < ResizedPixels.Num(); i++)
    {
        ResizedPixels[i] = FColor(
            ResizedRGB[i].R,
            ResizedRGB[i].G,
            ResizedRGB[i].B,
            ResizedAlphaColor[i].R  // 알파값을 R 채널에서 가져옴
        );
    }

    // 3. PNG로 저장
    FString FilePath = GetPortraitFilePath();
    FString Directory = FPaths::GetPath(FilePath);
    if (!FPaths::DirectoryExists(Directory))
    {
        IFileManager::Get().MakeDirectory(*Directory, true);
    }

    TArray<uint8, FDefaultAllocator64> CompressedData;
    FImageUtils::PNGCompressImageArray(144, 192, ResizedPixels, CompressedData);

    if (FFileHelper::SaveArrayToFile(CompressedData, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("Portrait saved: %s (%d bytes)"), *FilePath, CompressedData.Num());
    }

    FaceCaptureCamera->SetActive(false);

    // 캡처완료 알림
    OnPortraitCaptured.ExecuteIfBound(EmployeeIDStr);
}

FString AOfficeworker::GetPortraitFilePath() const
{
    FString BasePath;
    FString EmployeeIDStr = FString::FromInt(this->EmployeeID);

#if PLATFORM_ANDROID
    // Android: External Storage 사용 (접근 가능)
    extern FString GExternalFilePath;
    BasePath = GExternalFilePath;
    UE_LOG(LogTemp, Warning, TEXT("Android External Path: %s"), *GExternalFilePath);
#elif PLATFORM_IOS
    // iOS: Documents 폴더
    BasePath = FPaths::ProjectSavedDir();
    UE_LOG(LogTemp, Warning, TEXT("iOS Saved Path: %s"), *BasePath);
#else
    // PC
    BasePath = FPaths::ProjectSavedDir();
    UE_LOG(LogTemp, Warning, TEXT("PC Saved Path: %s"), *BasePath);
#endif

    FString FullPath = BasePath / TEXT("Portraits") / (EmployeeIDStr + TEXT(".png"));
    UE_LOG(LogTemp, Warning, TEXT("Full Portrait Path: %s"), *FullPath);

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

    // 좌석 위치와 회전값 가져오기
    FVector SeatLocation = Workstation->GetSeatLocation(AssignedSeatIndex);
    FRotator SeatRotation = Workstation->GetSeatRotation(AssignedSeatIndex);

    // 순간이동
    SetActorLocation(SeatLocation);
    SetActorRotation(SeatRotation);

    // Workstation에 착석 등록
    Workstation->SitDownAt(AssignedSeatIndex, this);

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
    if (!AssignedWorkstation.IsValid() || AssignedSeatIndex < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] No assigned workstation for move"), EmployeeID);
        return;
    }

    AWorkstationActorBase* Workstation = AssignedWorkstation.Get();

    // 좌석 위치 가져오기
    FVector SeatLocation = Workstation->GetSeatLocation(AssignedSeatIndex);

    // 좌석 앞 1m 지점을 목표로 설정
    FVector TargetLocation = SeatLocation + Workstation->GetActorForwardVector() * 100.f;

    AAIController* AIC = Cast<AAIController>(GetController());
    if (AIC)
    {
        // AI 이동 시작
        AIC->MoveToLocation(TargetLocation, 50.f);

        // 이동 완료 감지를 위한 콜백 설정
        // Tick에서 이동 완료 시 텔레포트 처리
        UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Moving to workstation area at %s"),
            EmployeeID, *TargetLocation.ToString());
    }
}

void AOfficeworker::StandUpFromWorkstation()
{
    if (!AssignedWorkstation.IsValid() || AssignedSeatIndex < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Officeworker %d] Not sitting at workstation"), EmployeeID);
        return;
    }

    AWorkstationActorBase* Workstation = AssignedWorkstation.Get();

    // Workstation에서 기립 등록
    Workstation->StandUp(this);

    // BehaviorComponent 상태 업데이트
    if (BehaviorComponent)
    {
        BehaviorComponent->bIsSeated = false;
    }

    UE_LOG(LogTemp, Log, TEXT("[Officeworker %d] Stood up from workstation seat %d"),
        EmployeeID, AssignedSeatIndex);
}

// ===== 머리 위 상태 버블 =====

void AOfficeworker::SetWorkerBubbleType(EWorkerBubbleType NewType)
{
    if (CurrentBubbleType == NewType) return;

    CurrentBubbleType = NewType;
    if (!BubbleWidgetComponent) return;

    if (NewType == EWorkerBubbleType::None)
    {
        BubbleWidgetComponent->SetVisibility(false);
        return;
    }

    BubbleWidgetComponent->SetVisibility(true);

    // Config에서 상태별 이모트 풀 → 랜덤 1장 → WBP의 EmoteImage에 적용
    UWorkerBubbleConfig* Config = BubbleConfig.IsNull() ? nullptr : BubbleConfig.LoadSynchronous();
    if (!Config) return;

    UTexture2D* PickedEmote = Config->PickEmoteForState(NewType);
    if (!PickedEmote) return;

    UUserWidget* BubbleUserWidget = BubbleWidgetComponent->GetUserWidgetObject();
    if (!BubbleUserWidget) return;

    if (UImage* EmoteImage = Cast<UImage>(BubbleUserWidget->GetWidgetFromName(TEXT("EmoteImage"))))
    {
        // bMatchSize=true → 텍스처 원본 해상도(가로/세로비) 그대로 적용. 슬롯이 Center면 stretch 안 됨
        EmoteImage->SetBrushFromTexture(PickedEmote, true);

        // 너무 크면 96px로 fit (긴 변 기준)
        if (PickedEmote)
        {
            const float MaxDim = 96.0f;
            const float W = static_cast<float>(PickedEmote->GetSizeX());
            const float H = static_cast<float>(PickedEmote->GetSizeY());
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
        }
    }
}

void AOfficeworker::SetGlowOverlay(bool bEnabled, FLinearColor Color)
{
    // 베이스는 no-op — 시각(오버레이)은 자식 override 담당 (모듈러=파츠 오버레이, 스틱=현재 시각 없음)
}