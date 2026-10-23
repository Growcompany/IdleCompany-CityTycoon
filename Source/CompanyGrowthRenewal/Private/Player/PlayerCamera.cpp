#include "Player/PlayerCamera.h"

// Unreal Engine Headers
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/ScopeExit.h"

// Enhanced Input
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

// Project-Specific Headers
#include "Player/MainMapPlayerController.h"
#include "Player/Components/InteractableInputHandler.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/Components/CameraFramingMath.h"
#include "Player/Components/PlacementHandler.h"
#include "Player/Components/FocusOcclusionHandler.h"
#include "Core/CGGameInstance.h"
#include "GameMode/CGGameModeBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Entity/InteractableBaseActor.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Factory/BrickFactory.h"
#include "Table/BuildingData.h"          // FBuildingData (footprint 칸수)
#include "Table/BuildableCardTable.h"    // FBuildableCardTable

DEFINE_LOG_CATEGORY(LogInputKey);

APlayerCamera::APlayerCamera()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(root);

	// Spring Arm Component + Specific Camera Moving and TargetArmLength is Set in MovementInputhandler
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(-47.5f, 0.f, 0.f));
    SpringArm->SocketOffset = FVector(-1200.f, 0.f, 320.f);
    SpringArm->bEnableCameraRotationLag = true;
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;
    SpringArm->bUsePawnControlRotation = false;
    // 랙 속도 ↑ (기존 10=tau 0.1s라 둔함). 긴 팔 탓에 회전도 위치랙에 걸리므로 둘 다 명시 설정
    SpringArm->CameraLagSpeed = 22.f;
    SpringArm->CameraRotationLagSpeed = 22.f;

	// Camera Component
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("MainCamera"));
    CameraComponent->SetupAttachment(SpringArm);
    CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
    CameraComponent->SetFieldOfView(25.0f);

    // Collision ��ġ�� Actorüũ
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->SetupAttachment(GetRootComponent());
    Collision->SetSphereRadius(720.0f);
    Collision->SetRelativeScale3D(FVector(0.666f, 0.666f, 1.f));
    Collision->SetCollisionObjectType(ECC_Pawn);
    Collision->OnComponentBeginOverlap.AddDynamic(this, &APlayerCamera::OnBeginOverlap);  
    Collision->OnComponentEndOverlap.AddDynamic(this, &APlayerCamera::OnEndOverlap);

	// Movement Component
    MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
    MovementInputHandler = CreateDefaultSubobject<UMovementInputHandler>(TEXT("MovementInputHandler"));
    MovementInputHandler->Initialize(MovementComponent);
    
    // 일단 Factory만 관리함 이건
    InteractableInputHandler = CreateDefaultSubobject<UInteractableInputHandler>(TEXT("InteractableInputHandler"));
    InteractableInputHandler->Initialize();

    PlacementHandler = CreateDefaultSubobject<UPlacementHandler>(TEXT("PlacementComponent"));
    PlacementHandler->Initialize();

    FocusOcclusionHandler = CreateDefaultSubobject<UFocusOcclusionHandler>(TEXT("FocusOcclusionHandler"));

}

void APlayerCamera::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const float DT = FMath::Min(DeltaTime, 1.f / 10.f);
    HorizontalProjectionOffset = FMath::FInterpTo(
        HorizontalProjectionOffset, TargetHorizontalProjectionOffset, DT, CameraTransitionSpeed);

    if (MovementInputHandler && MovementInputHandler->bConstrainCameraView)
    {
        if (bIsTransitioningCamera) ConstrainFocusDestination();
        if (!bIsTransitioningCamera) MovementInputHandler->ApplyZoomSettings();
    }

    // 건물 포커스를 위한 카메라 전환 처리
    if (bIsTransitioningCamera && MovementInputHandler)
    {
        // 히치 프레임(모바일 콜드 로드/최초 PSO 컴파일 등)의 큰 DeltaTime이 VInterpTo alpha를 포화(>=1)시켜 카메라가 순간이동하는 것 방지 — 보간에만 상한
        // 30fps 상한은 상시 19fps 기기에서 매 프레임 걸려 글라이드가 1.58배 느려진다.
        // 10fps 로 낮춰도 0.1 * CameraTransitionSpeed(3.0) = 0.3 이라 alpha 포화 방어는 유지된다.
        // 경과는 벽시계여야 한다 — 상한 DT 로 누적하면 히치 프레임에서 도착 판정이 그만큼 늦어진다
        FocusGlideElapsed += DeltaTime;

        // 카메라 위치 부드럽게 전환
        FVector CurrentLocation = GetActorLocation();
        FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetCameraLocation, DT, CameraTransitionSpeed);
        SetActorLocation(NewLocation);

        // ZoomValue 부드럽게 전환
        float CurrentZoomValue = MovementInputHandler->GetZoomValue();
        float NewZoomValue = FMath::FInterpTo(CurrentZoomValue, TargetZoomValue, DT, CameraTransitionSpeed);
        MovementInputHandler->SetZoomValue(NewZoomValue);
        MovementInputHandler->ApplyZoomSettings();

        // 전환 완료 확인
        float LocationDist = FVector::Dist(GetActorLocation(), TargetCameraLocation);
        float ZoomValueDiff = FMath::Abs(MovementInputHandler->GetZoomValue() - TargetZoomValue);

        if (LocationDist < 0.1f && ZoomValueDiff < 0.001f)
        {
            SetActorLocation(TargetCameraLocation);
            MovementInputHandler->SetZoomValue(TargetZoomValue);
            MovementInputHandler->ApplyZoomSettings();
            HorizontalProjectionOffset = TargetHorizontalProjectionOffset;
            bIsTransitioningCamera = false;

            UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] Camera transition complete"));
        }
    }
}

void APlayerCamera::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime, OutResult);
    OutResult.OffCenterProjectionOffset.X = HorizontalProjectionOffset;
    if (const ACGGameModeBase* CityMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameModeBase>() : nullptr)
    {
        if (CityMode->TryGetCityOpeningView(OutResult)) return;
    }
    if (MovementInputHandler) MovementInputHandler->ConstrainCameraView(OutResult);
}

void APlayerCamera::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
    const FHitResult& SweepResult)
{
}

void APlayerCamera::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
}

void APlayerCamera::BeginPlay()
{
    Super::BeginPlay();

    // 안전하게 다음 프레임에 실행
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            OnChangedInputType();
        });

    // 기본 상태: 카메라 드래그 모드
    MovementInputHandler->InitDragMoveIMC();
    UE_LOG(LogTemp, Log, TEXT("Game started - Camera drag mode initialized"));
}

void APlayerCamera::OnChangedInputType()
{
    AMainMapPlayerController* PC = GetPlayerController();
    if (!PC)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("GetPlayerController() is NULL!"));
        }
        return;
    }

    auto currentInputType = PC->GetCurrentInputType();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
            FString::Printf(TEXT("OnChangedInputType: %s"),
                *StaticEnum<EInputType>()->GetNameStringByValue((int32)currentInputType)));
    }

    if (currentInputType == EInputType::Touch)
    {
        // 모바일: collision 완전 비활성화
        Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Collision->SetVisibility(false);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Touch Mode - Collision DISABLED"));
        }
        return;
    }

    if (currentInputType == EInputType::KeyMouse)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Blue, TEXT("SETTING COLLISION TO 10"));
        }

        Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Collision->SetRelativeLocation(FVector(0, 0, 10));
    }
}

void APlayerCamera::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    MovementInputHandler->SetupPlayerInputComponent(PlayerInputComponent);
    InteractableInputHandler->SetupPlayerInputComponent(PlayerInputComponent);
    PlacementHandler->SetupPlayerInputComponent(PlayerInputComponent);
}

AMainMapPlayerController* APlayerCamera::GetPlayerController()
{
    return Cast<AMainMapPlayerController>(GetController());
}

void APlayerCamera::BeginBuild(const FBuildableCardTable& buildableInfo, ECompanyType CompanyType, FName TargetPlotId)
{
    AMainMapPlayerController* PC = GetPlayerController();
    if (PC)
    {
        PC->GoToBuildPlaceMode();
    }

    UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
    UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();

    // BuildableTable과 InteractableDataTable의 RowName이 동일하므로 그대로 사용 (예: "B1", "B2", "B3")
    FName InteractableRowName = buildableInfo.RowName;

    bool bFoundBuildingName = false;
    const FInteractableInfo interactableInfo = TableManager->GetInteractableInfo(InteractableRowName, bFoundBuildingName);

    if (!bFoundBuildingName) {
        UE_LOG(LogTemp, Warning, TEXT("[APlayerCamera::BeginBuild] : Can't Find Building (RowName: %s)"), *InteractableRowName.ToString());
    }

    // 배치 확정 시 건물에 베이크할 산업 — 프리뷰 스폰 전에 PlacementHandler 에 전달
    PlacementHandler->SetPendingCompanyType(CompanyType);

    // 대상 부지 + 건물 footprint 칸수(DT) 전달 — 부지 격자 스냅/점유에 사용 (SetPendingCompanyType 미러)
    int32 FootW = 1;
    int32 FootD = 1;
    bool bBuildingDataOk = false;
    const FBuildingData BData = TableManager->GetBuildingData(InteractableRowName, bBuildingDataOk);
    if (bBuildingDataOk)
    {
        FootW = FMath::Max(1, BData.FootprintWidthCells);
        FootD = FMath::Max(1, BData.FootprintDepthCells);
    }
    PlacementHandler->SetPendingPlotId(TargetPlotId, FootW, FootD);

    if (PlacementHandler->CheckEntityAgainstPlacementTarget(interactableInfo) == false)
    {
        PlacementHandler->SetPlacementTargetEntity(interactableInfo);
    }

}

void APlayerCamera::EndBuild()
{
    UE_LOG(LogTemp, Log, TEXT("[APlayerCamera]: EndBuild()"));

    AMainMapPlayerController* PC = GetPlayerController();
    if (PC)
    {
        PC->GoToNormalMode();
    }

    PlacementHandler->ReleasePlacementTargetEntity();
    PlacementHandler->ReleasePlacementIMC();
}

void APlayerCamera::StartBuildingRelocation(AInteractableBaseActor* ExistingBuilding)
{
    ABuildingBaseActor* buildingActor = Cast<ABuildingBaseActor>(ExistingBuilding);
    ABrickFactory* Factory = Cast<ABrickFactory>(ExistingBuilding);
    if (!IsValid(ExistingBuilding) || !PlacementHandler || (!buildingActor && !Factory))
    {
        UE_LOG(LogTemp, Error, TEXT("StartBuildingRelocation requires a building or factory"));
        return;
    }
    if (Factory && !Factory->BeginRelocation()) return;

    AMainMapPlayerController* PC = GetPlayerController();
    if (PC) PC->GoToBuildPlaceMode();

    // 원래 위치 저장 (Cancel용)
    FVector originalPosition = ExistingBuilding->GetActorLocation();
    PlacementHandler->SetOriginalPosition(originalPosition);

    // **기존 건물을 그대로 PlacementTarget으로 사용**
    PlacementHandler->SetExistingBuildingAsTarget(ExistingBuilding);

    // 건물 떠오르기 효과
    if (buildingActor) buildingActor->StartFloating(500.0f);

    // 건물 클릭 시 카메라 포커싱
    StopCameraTransition();
    // 재배치도 새 배치와 같이 앞 건물을 고스트로 비켜 준다 — 해제는 Normal 복귀 시 안전망이 맡는다
    if (buildingActor) FocusOnBuilding(buildingActor, 50.f, 50.f, 0.5f);
    else FocusOnActor(Factory, 0.5f, /*bTrackOcclusion=*/true, 15000.f);
    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] StartBuildingRelocation - Camera focusing on building"));
}

void APlayerCamera::EndBuildingRelocation()
{
    UE_LOG(LogTemp, Log, TEXT("[APlayerCamera]: EndBuildingRelocation()"));

    // 건물 떠오르기 효과 종료
    if (ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(PlacementHandler->GetPlacementTargetEntity()))
    {
        Building->StopFloating();
    }

    AMainMapPlayerController* PC = GetPlayerController();
    if (PC)
    {
        PC->GoToNormalMode();
    }

    // PlacementTarget 해제 (중요! 이게 없으면 다음 건설이 안 됨)
    PlacementHandler->ReleasePlacementTargetEntity();
    PlacementHandler->ReleasePlacementIMC();
}

void APlayerCamera::StopCameraTransition()
{
    bIsTransitioningCamera = false;
    // ZoomValue는 이미 동기화되어 있으므로 추가 작업 불필요
}

void APlayerCamera::BeginCameraTransition()
{
    if (!ConstrainFocusDestination()) return;
    bIsTransitioningCamera = true;
    FocusGlideElapsed = 0.f;
}

bool APlayerCamera::ConstrainFocusDestination()
{
    if (!MovementInputHandler || MovementInputHandler->TryConstrainCameraPose(
        TargetCameraLocation, TargetZoomValue, TargetHorizontalProjectionOffset)) return true;

    TargetCameraLocation = GetActorLocation();
    TargetZoomValue = MovementInputHandler->GetZoomValue();
    TargetHorizontalProjectionOffset = HorizontalProjectionOffset;
    bIsTransitioningCamera = false;
    UE_LOG(LogTemp, Warning, TEXT("[CityCamera] Focus rejected: destination height and lens cannot fit the playable area."));
    return false;
}

bool APlayerCamera::IsFocusVisuallySettled(float LocationTolerance, float ZoomTolerance) const
{
    if (!bIsTransitioningCamera)
    {
        return true;
    }
    // 핸들러가 없으면 Tick 의 전환 블록이 아예 안 돌아 카메라가 영영 안 움직인다 —
    // 위치 게이트 뒤에 두면 목표가 멀 때 false 로 갇혀 "기다리지 않는다" 는 의도와 반대가 된다
    if (MovementInputHandler == nullptr)
    {
        return true;
    }
    return ShouldTreatFocusAsSettled(
        bIsTransitioningCamera, FocusGlideElapsed, FocusVisualSettleSeconds,
        static_cast<float>(FVector::Dist(GetActorLocation(), TargetCameraLocation)), LocationTolerance,
        FMath::Abs(MovementInputHandler->GetZoomValue() - TargetZoomValue), ZoomTolerance);
}

FOfficeCameraViewState APlayerCamera::CaptureViewState() const
{
    FOfficeCameraViewState State;
    if (!MovementInputHandler)
    {
        return State;
    }
    State.Location = bIsTransitioningCamera ? TargetCameraLocation : GetActorLocation();
    State.Zoom = bIsTransitioningCamera ? TargetZoomValue : MovementInputHandler->GetZoomValue();
    State.Yaw = GetActorRotation().Yaw;
    State.bValid = true;
    return State;
}

void APlayerCamera::RestoreViewState(const FOfficeCameraViewState& State)
{
    if (!State.bValid || !MovementInputHandler)
    {
        return;
    }
    if (!FMath::IsNearlyEqual(FRotator::NormalizeAxis(GetActorRotation().Yaw - State.Yaw), 0.f, 0.1f))
    {
        SetActorRotation(FRotator(0.f, State.Yaw, 0.f));
    }
    TargetCameraLocation = State.Location;
    TargetZoomValue = State.Zoom;
    BeginCameraTransition();
}

void APlayerCamera::SetFramingCenterRatioX(float Ratio)
{
    // FocusOnBuilding 과 같은 부호 규약 — 엔진이 NDC X 에서 오프셋을 빼므로 화면 비율 q 는 1-2q
    TargetHorizontalProjectionOffset = 1.f - 2.f * FMath::Clamp(Ratio, 0.f, 1.f);
}

void APlayerCamera::ClearFocusTarget()
{
    TargetHorizontalProjectionOffset = 0.f;
    if (FocusOcclusionHandler)
    {
        FocusOcclusionHandler->ClearFocusTarget();
    }
}

void APlayerCamera::RestoreFocusTarget(AActor* Target)
{
    if (Target)
    {
        TargetHorizontalProjectionOffset = ProjectionFocusTarget.Get() == Target
            ? FocusHorizontalProjectionOffset : 0.f;
    }
    // null 을 넘기면 SetFocusTarget 이 해제로 동작한다 — 복원 호출이 조용히 해제가 되지 않도록 여기서 막는다
    if (FocusOcclusionHandler && Target)
    {
        FocusOcclusionHandler->SetFocusTarget(Target);
    }
}

float APlayerCamera::FocusOnBuilding(ABuildingBaseActor* Building, float TopPadding, float BottomPadding, float ScreenCenterRatioX, float ScreenHeightRatioScale)
{
    if (!Building || !SpringArm || !CameraComponent || !MovementInputHandler)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] FocusOnBuilding: Invalid parameters"));
        return 0.f;
    }

    const FVector BasePosition = Building->GetPlacementFootprintCenter();
    const float BuildingHeight = Building->GetTotalBuildingHeightWorld();
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] FocusOnBuilding: No PlayerController"));
        return 0.f;
    }

    // 화면 좌표는 아래 0, 위 1. 저층도 지정한 상·하 여백 사이의 중심에 배치한다.
    constexpr float FitScreenTop = 0.80f;
    constexpr float FitScreenBottom = 0.10f;
    constexpr float FitScreenCenter = (FitScreenTop + FitScreenBottom) * 0.5f;
    const float FitRatio = (FitScreenTop - FitScreenBottom) * FMath::Clamp(ScreenHeightRatioScale, 0.1f, 1.0f);
    const FZoomRigParams Rig = MovementInputHandler->GetZoomRigParams();
    const auto CurveEval = [this](float Zoom) { return MovementInputHandler->EvaluateZoomKey(Zoom); };

    // 기본 줌 범위 안에서만 탐색한다. 상한에서 넘치는 건물은 꼭대기를 상단 여백에 건다.
    TargetZoomValue = CameraFramingMath::SolveZoomForHeight(Rig, CurveEval, BuildingHeight, FitRatio, FitScreenCenter);
    const FZoomRigSample Target = MovementInputHandler->EvaluateZoomRig(TargetZoomValue);
    const float CameraDistance = Target.ArmLength + Rig.CameraPullBackCm;
    const FVerticalFramingResult Framing = CameraFramingMath::SolveVerticalFraming(
        BuildingHeight, CameraDistance, Target.PitchDeg, Target.HorizontalFOVDeg,
        Rig.CameraAspectRatio, FitRatio, FitScreenTop, /*bAnchorOverflowToTop=*/true, FitScreenCenter);

    const FVector FocusPivotWorld = BasePosition + FVector(0.f, 0.f, Framing.PivotHeight);

    // 소켓의 수직 오프셋만큼 시선 중심선이 올라가므로 피벗에서 회전된 오프셋을 뺀다.
    const FQuat RigRotation = FRotator(Target.PitchDeg, GetActorRotation().Yaw, 0.f).Quaternion();
    const FVector SocketRiseWorld = RigRotation.RotateVector(FVector(0.f, 0.f, SpringArm->SocketOffset.Z));
    TargetCameraLocation = FocusPivotWorld - SocketRiseWorld;
    // 엔진의 투영 행렬은 NDC X에서 OffCenterProjectionOffset.X를 빼므로 화면 비율 q는 1-2q다.
    TargetHorizontalProjectionOffset = 1.f - 2.f * FMath::Clamp(ScreenCenterRatioX, 0.f, 1.f);
    BeginCameraTransition();
    if (!bIsTransitioningCamera) return 0.f;
    FocusHorizontalProjectionOffset = TargetHorizontalProjectionOffset;
    ProjectionFocusTarget = Building;

    if (FocusOcclusionHandler)
    {
        FocusOcclusionHandler->SetFocusTarget(Building);
    }

    UE_LOG(LogTemp, Log, TEXT("[FocusOnBuilding] Height=%.1f, Fit=%s, Ratio=%.1f%%(목표 %.1f%%), Top=%.3f, Bottom=%.3f, Zoom=%.4f, Pitch=%.1f, hFOV=%.1f, Arm=%.1f"),
        BuildingHeight, Framing.bFitsInFrame ? TEXT("전체") : TEXT("위기준"),
        (Framing.TopScreenPosition - Framing.BottomScreenPosition) * 100.f, FitRatio * 100.f,
        Framing.TopScreenPosition, Framing.BottomScreenPosition,
        TargetZoomValue, Target.PitchDeg, Target.HorizontalFOVDeg, Target.ArmLength);
    return MovementInputHandler->EvaluateZoomRig(TargetZoomValue).ArmLength;
}

void APlayerCamera::FocusOnBuildingForPlacement(ABuildingBaseActor* Building, float DesiredDistanceCm)
{
    // 1. 진행 중인 카메라 전환 중단
    StopCameraTransition();

    // 2. 건물에 카메라 포커싱
    if (Building)
    {
        // 배치 중인 건물을 가리는 앞 건물은 고스트로 비켜 준다 — 끌어서 옮기면 주기 갱신이 따라간다
        FocusOnActor(Building, 0.5f, /*bTrackOcclusion=*/true, DesiredDistanceCm);
        UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] FocusOnBuildingForPlacement: Building=%s"), *Building->GetName());
    }

    // 3. PlacementHandler 입력 매핑 복원
    if (PlacementHandler)
    {
        PlacementHandler->InitPlacementIMC();
        UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] PlacementHandler input restored"));
    }
}

void APlayerCamera::RefreshCameraRigTransforms()
{
    if (!SpringArm || !CameraComponent)
    {
        return;
    }

    SpringArm->UpdateComponentToWorld();
    SpringArm->TickComponent(0.f, LEVELTICK_All, nullptr);
    CameraComponent->UpdateComponentToWorld();
}

void APlayerCamera::SettleFocusImmediately()
{
    // 패널을 열지 않는 포커스라 가림 고스트는 켜지 않는다.
    // 로딩 직후면 뷰포트가 0x0 일 수 있어 가림 트레이스 자체가 무의미하다.
    if (FocusOcclusionHandler)
    {
        FocusOcclusionHandler->ClearFocusTarget();
    }
    StopCameraTransition();
    HorizontalProjectionOffset = TargetHorizontalProjectionOffset;

    // 랙이 켜진 채 순간이동하면 스폰 지점에서 목표까지 카메라가 끌려오는 몇 프레임이 보인다 —
    // 정착 중에만 끄고, 최종 pose 에서 랙 캐시를 갱신한 뒤 원래 설정으로 되돌린다
    const bool bSavedCameraLag = SpringArm->bEnableCameraLag;
    const bool bSavedCameraRotationLag = SpringArm->bEnableCameraRotationLag;
    SpringArm->bEnableCameraLag = false;
    SpringArm->bEnableCameraRotationLag = false;
    ON_SCOPE_EXIT
    {
        SpringArm->bEnableCameraLag = bSavedCameraLag;
        SpringArm->bEnableCameraRotationLag = bSavedCameraRotationLag;
    };

    SetActorLocation(TargetCameraLocation);
    MovementInputHandler->SetZoomValue(TargetZoomValue);
    MovementInputHandler->ApplyZoomSettings();
    RefreshCameraRigTransforms();
}

bool APlayerCamera::SnapFocusOnBuilding(ABuildingBaseActor* Building, float ScreenCenterRatioX)
{
    if (!Building || !MovementInputHandler || !SpringArm || !CameraComponent)
    {
        return false;
    }

    // 프레이밍 계산은 글라이드 경로와 공유 — 목표 pose(TargetCameraLocation/TargetZoomValue)만 받아 쓴다
    if (FocusOnBuilding(Building, 50.f, 50.f, ScreenCenterRatioX) <= 0.f)
    {
        return false;
    }

    SettleFocusImmediately();

    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] SnapFocusOnBuilding: %s, Loc=(%.1f, %.1f, %.1f), Zoom=%.4f"),
        *Building->GetName(), TargetCameraLocation.X, TargetCameraLocation.Y, TargetCameraLocation.Z, TargetZoomValue);
    return true;
}

bool APlayerCamera::SnapFocusOnActor(AActor* Actor, float ScreenCenterRatioX, float DesiredDistanceCm, float DesiredScreenHeightRatio)
{
    if (!Actor || !MovementInputHandler || !SpringArm || !CameraComponent)
    {
        return false;
    }

    // FocusOnActor 는 void 라 성공 여부를 안 돌려준다 — 전환 플래그를 내려놓고 부른 뒤
    // 그게 켜졌는지로 "목표 pose 가 실제로 계산됐는가" 를 읽는다(조기 반환 경로는 플래그를 안 켠다)
    StopCameraTransition();
    FocusOnActor(Actor, ScreenCenterRatioX, /*bTrackOcclusion=*/false, DesiredDistanceCm, DesiredScreenHeightRatio);
    if (!bIsTransitioningCamera)
    {
        return false;
    }

    SettleFocusImmediately();

    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] SnapFocusOnActor: %s, Loc=(%.1f, %.1f, %.1f), Zoom=%.4f"),
        *Actor->GetName(), TargetCameraLocation.X, TargetCameraLocation.Y, TargetCameraLocation.Z, TargetZoomValue);
    return true;
}

void APlayerCamera::FocusOnActor(AActor* Actor, float ScreenCenterRatioX, bool bTrackOcclusion, float DesiredDistanceCm, float DesiredScreenHeightRatio)
{
    if (!Actor || !SpringArm || !CameraComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] FocusOnActor: Invalid parameters"));
        return;
    }

    // Actor 바운딩 박스로 높이 계산
    FBox BoundingBox = Actor->GetComponentsBoundingBox();
    FVector ActorLocation = Actor->GetActorLocation();
    float ActorHeight = BoundingBox.GetSize().Z;
    FVector BasePosition = FVector(ActorLocation.X, ActorLocation.Y, BoundingBox.Min.Z);

    // 뷰포트 크기 가져오기
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] FocusOnActor: No PlayerController"));
        return;
    }

    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
    // 모바일 시작 포커스는 맵 로딩 중 뷰포트가 0x0 → 0 나눗셈 NaN → 카메라 못 도달. 폴백 16:9.
    float AspectRatio = (ViewportSizeY > 0) ? (float)ViewportSizeX / (float)ViewportSizeY : (16.f / 9.f);

    // 화면에서 Actor가 차지할 비율 계산
    float MidHeight = 20000.0f;
    float TopScreenPosition;
    float BottomScreenPosition;

    if (ActorHeight <= MidHeight)
    {
        float Factor = ActorHeight / MidHeight;
        TopScreenPosition = FMath::Lerp(0.6f, 0.9f, Factor);
        BottomScreenPosition = FMath::Lerp(0.4f, 0.1f, Factor);
    }
    else
    {
        TopScreenPosition = 0.9f;
        BottomScreenPosition = 0.1f;
    }

    // 호출부가 점유율을 지정하면 내부 곡선을 덮는다 — 그 곡선은 200m 초고층 기준이라 짧은 액터에 안 맞는다
    if (DesiredScreenHeightRatio > 0.f)
    {
        const float Clamped = FMath::Clamp(DesiredScreenHeightRatio, 0.05f, 1.0f);
        const float Margin = (1.f - Clamped) * 0.5f;
        TopScreenPosition = 1.f - Margin;
        BottomScreenPosition = Margin;
    }
    const float TargetScreenHeightRatio = TopScreenPosition - BottomScreenPosition;

    if (!MovementInputHandler)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] FocusOnActor: No MovementInputHandler"));
        return;
    }
    const FZoomRigParams Rig = MovementInputHandler->GetZoomRigParams();
    const auto CurveEval = [this](float Zoom) { return MovementInputHandler->EvaluateZoomKey(Zoom); };
    // DesiredDistanceCm 은 **카메라~피사체 거리**다 — 팔길이로 역산하면 SocketOffset 후퇴만큼 실제로 더 멀어진다.
    TargetHorizontalProjectionOffset = 0.f;
    ProjectionFocusTarget.Reset();
    TargetZoomValue = DesiredDistanceCm > 0.f
        ? CameraFramingMath::SolveZoomForArmLength(Rig, CurveEval, FMath::Max(1.f, DesiredDistanceCm - Rig.CameraPullBackCm))
        : CameraFramingMath::SolveZoomForHeight(Rig, CurveEval, ActorHeight, TargetScreenHeightRatio);
    const FZoomRigSample Target = MovementInputHandler->EvaluateZoomRig(TargetZoomValue);
    const float ActualDistance = Target.ArmLength;
    const float CameraDistance = ActualDistance + Rig.CameraPullBackCm;

    const FVector CameraRight = GetActorRightVector();
    const float ScreenOffsetRatio = ScreenCenterRatioX - 0.5f;
    const float HorizontalHalfTan = CameraFramingMath::HorizontalHalfTangent(Target.HorizontalFOVDeg, Rig.CameraAspectRatio, AspectRatio);
    const float HorizontalOffset = ScreenOffsetRatio * 2.f * CameraDistance * HorizontalHalfTan;

    const FVerticalFramingResult Framing = CameraFramingMath::SolveVerticalFraming(
        ActorHeight, CameraDistance, Target.PitchDeg, Target.HorizontalFOVDeg,
        Rig.CameraAspectRatio, TargetScreenHeightRatio, TopScreenPosition, /*bAnchorOverflowToTop=*/false);
    // 공장 단계별 외형의 바닥 외곽 중심을 시선 중심에 둔다. 예약 부지는 외형보다 넓을 수 있다.
    const ABrickFactory* Factory = Cast<ABrickFactory>(Actor);
    FVector FocusPivotWorld = BasePosition + FVector(0.f, 0.f, Framing.PivotHeight);
    if (Factory)
    {
        FocusPivotWorld = Factory->GetPlacementFootprintCenter();
        if (const UStaticMeshComponent* Body = Cast<UStaticMeshComponent>(Factory->GetMainMeshComponent()))
        {
            if (const UStaticMesh* Mesh = Body->GetStaticMesh())
            {
                const FBox LocalBounds = Mesh->GetBoundingBox();
                const FVector LocalGroundCenter(LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, LocalBounds.Min.Z);
                FocusPivotWorld = Body->GetComponentTransform().TransformPosition(LocalGroundCenter);
            }
        }
    }

    // FocusOnBuilding 과 같은 이유 — 시선 중심선이 폰 위치가 아니라 SocketOffset.Z 만큼 위를 지난다
    const FQuat RigRotation = FRotator(Target.PitchDeg, GetActorRotation().Yaw, 0.f).Quaternion();
    const FVector SocketRiseWorld = RigRotation.RotateVector(FVector(0.f, 0.f, SpringArm->SocketOffset.Z));

    TargetCameraLocation = FocusPivotWorld
        - SocketRiseWorld - CameraRight * HorizontalOffset;

    BeginCameraTransition();
    if (!bIsTransitioningCamera) return;

    if (bTrackOcclusion && FocusOcclusionHandler)
    {
        FocusOcclusionHandler->SetFocusTarget(Actor);
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] FocusOnActor: %s, Height=%.1f, Zoom=%.4f, Arm=%.1f"),
        *Actor->GetName(), ActorHeight, TargetZoomValue, ActualDistance);
}

void APlayerCamera::FocusOnLocation(const FVector& TargetLocation, float DesiredDistance)
{
    StopCameraTransition();
    TargetHorizontalProjectionOffset = 0.f;
    ProjectionFocusTarget.Reset();

    TargetCameraLocation = TargetLocation;

    if (MovementInputHandler)
    {
        if (DesiredDistance > 0.f)
        {
            const FZoomRigParams Rig = MovementInputHandler->GetZoomRigParams();
            const auto CurveEval = [this](float Zoom) { return MovementInputHandler->EvaluateZoomKey(Zoom); };
            TargetZoomValue = CameraFramingMath::SolveZoomForArmLength(Rig, CurveEval, DesiredDistance);
        }
        else
        {
            TargetZoomValue = MovementInputHandler->GetZoomValue(); // 거리 미지정 = 현재 줌 유지
        }
    }

    BeginCameraTransition();

    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] FocusOnLocation: Target=(%.1f, %.1f, %.1f), Distance=%.1f"),
        TargetLocation.X, TargetLocation.Y, TargetLocation.Z, DesiredDistance);
}
