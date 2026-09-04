#include "Player/PlayerCamera.h"

// Unreal Engine Headers
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

// Enhanced Input
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

// Project-Specific Headers
#include "Player/MainMapPlayerController.h"
#include "Player/Components/InteractableInputHandler.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/Components/PlacementHandler.h"
#include "Player/Components/FocusOcclusionHandler.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Entity/InteractableBaseActor.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Table/BuildingData.h"          // FBuildingData (footprint 칸수)
#include "Table/BuildableCardTable.h"    // FBuildableCardTable

DEFINE_LOG_CATEGORY(InputKey);

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

    // 건물 포커스를 위한 카메라 전환 처리
    if (bIsTransitioningCamera && MovementInputHandler)
    {
        // 히치 프레임(모바일 콜드 로드/최초 PSO 컴파일 등)의 큰 DeltaTime이 VInterpTo alpha를 포화(>=1)시켜 카메라가 순간이동하는 것 방지 — 보간에만 상한
        const float DT = FMath::Min(DeltaTime, 1.f / 30.f);

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
        MovementInputHandler->ApplyZoomSettings(); // ZoomValue를 SpringArm 등에 적용 (값 변경 안 함)

        // 전환 완료 확인
        float LocationDist = FVector::Dist(NewLocation, TargetCameraLocation);
        float ZoomValueDiff = FMath::Abs(NewZoomValue - TargetZoomValue);

        if (LocationDist < 0.1f && ZoomValueDiff < 0.001f)
        {
            SetActorLocation(TargetCameraLocation);
            MovementInputHandler->SetZoomValue(TargetZoomValue);
            MovementInputHandler->ApplyZoomSettings();
            bIsTransitioningCamera = false;

            UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] Camera transition complete"));
        }
    }
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
    AMainMapPlayerController* PC = GetPlayerController();
    if (PC)
    {
        PC->GoToBuildPlaceMode();
    }

    // Building 타입 체크
    ABuildingBaseActor* buildingActor = Cast<ABuildingBaseActor>(ExistingBuilding);
    if (!buildingActor)
    {
        UE_LOG(LogTemp, Error, TEXT("StartBuildingRelocation called with non-Building actor"));
        return;
    }

    // **기존 건물을 그대로 PlacementTarget으로 사용**
    PlacementHandler->SetExistingBuildingAsTarget(buildingActor);

    // 원래 위치 저장 (Cancel용)
    FVector originalPosition = ExistingBuilding->GetActorLocation();
    PlacementHandler->SetOriginalPosition(originalPosition);

    // 건물 떠오르기 효과
    UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] Calling StartFloating on %s"), *buildingActor->GetName());
    buildingActor->StartFloating(500.0f);

    // 건물 클릭 시 카메라 포커싱
    StopCameraTransition();
    FocusOnBuilding(buildingActor, 50.f, 50.f, 0.5f);
    // 재배치도 같은 부지 격자 세션 — FocusOnBuildingForPlacement 과 동일하게 고스트를 겹치지 않는다
    ClearFocusTarget();
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
    bIsTransitioningCamera = true;
    FocusGlideElapsed = 0.f;
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

void APlayerCamera::ClearFocusTarget()
{
    if (FocusOcclusionHandler)
    {
        FocusOcclusionHandler->ClearFocusTarget();
    }
}

void APlayerCamera::RestoreFocusTarget(AActor* Target)
{
    // null 을 넘기면 SetFocusTarget 이 해제로 동작한다 — 복원 호출이 조용히 해제가 되지 않도록 여기서 막는다
    if (FocusOcclusionHandler && Target)
    {
        FocusOcclusionHandler->SetFocusTarget(Target);
    }
}

// 영향권 반경 R 원을 좁은 FOV(~20도) 화면에 담는 데 필요한 팔길이 배수. FocusOnAreaRadius / FocusOnBuildingOrAura 공용.
static constexpr float KeystoneAuraZoomFactor = 5.5f;

float APlayerCamera::FocusOnBuilding(ABuildingBaseActor* Building, float TopPadding, float BottomPadding, float ScreenCenterRatioX, float ScreenHeightRatioScale)
{
    if (!Building || !SpringArm || !CameraComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] FocusOnBuilding: Invalid parameters"));
        return 0.f;
    }

    // 건물 경계 (Base와 Top 위치) 가져오기
    FVector BuildingLocation = Building->GetActorLocation();
    float BuildingHeight = Building->GetTotalBuildingHeight();  // Top Module 포함한 전체 높이
    FVector BasePosition = BuildingLocation;
    FVector TopPosition = BuildingLocation + FVector(0, 0, BuildingHeight);

    // 뷰포트 크기 가져오기
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCamera] FocusOnBuilding: No PlayerController"));
        return 0.f;
    }

    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

    // 목표 화면 위치 계산
    float TargetScreenX = ViewportSizeX * ScreenCenterRatioX; // 화면 가로 위치 (0.0=왼쪽, 0.5=중앙, 1.0=오른쪽)

    // 1단계: 목표 화면 비율 결정
    // BuildingHeight는 이미 위에서 GetTotalBuildingHeight()로 계산됨 (건물 바닥~꼭대기 높이)
    // 뷰포트 0x0(로딩 중 호출) → 0 나눗셈 NaN 방지, 폴백 16:9 (FocusOnActor와 동일 가드)
    float AspectRatio = (ViewportSizeY > 0) ? (float)ViewportSizeX / (float)ViewportSizeY : (16.f / 9.f);

    // 건물이 화면에서 차지하는 위치를 직접 정의하는 방식
    // 화면 좌표: 0.0 = 하단, 0.5 = 중앙, 1.0 = 상단
    float MidHeight = 20000.0f; // 전환 기준 높이 (40000까지는 괜찮았음)

    // 건물 Top/Bottom의 화면 위치 결정
    float TopScreenPosition;    // 건물 꼭대기가 화면에서 위치
    float BottomScreenPosition; // 건물 하단이 화면에서 위치

    if (BuildingHeight <= MidHeight)
    {
        // 낮은 건물: Top 0.55, Bottom 0.45에 위치
        // → 건물이 화면 중간에 작게 배치 (화면의 10% 차지)
        // 높아질수록: Top이 상단 1/10, Bottom이 하단 1/10으로 이동
        // → 건물이 화면의 80% 차지
        float Factor = BuildingHeight / MidHeight;
        TopScreenPosition = FMath::Lerp(0.6f, 0.9f, Factor);    // 중간 상단 → 상단 1/10
        BottomScreenPosition = FMath::Lerp(0.4f, 0.1f, Factor); // 중간 하단 → 하단 1/10
    }
    else
    {
        // 높은 건물: Bottom을 하단 1/10에 고정, Top은 화면 밖으로 나가도 OK
        TopScreenPosition = 0.9f;    // 상단 1/10 (하지만 실제론 화면 밖)
        BottomScreenPosition = 0.1f; // 하단 1/10 고정
    }

    // 건물이 화면에서 차지하는 비율 계산 (스케일이 작을수록 카메라가 멀리 = 줌인 약함)
    float DesiredScreenHeightRatio = (TopScreenPosition - BottomScreenPosition) * FMath::Clamp(ScreenHeightRatioScale, 0.1f, 1.0f);

    // 건물의 실제 월드 좌표
    FVector BuildingBottomWorld = BasePosition;
    FVector BuildingTopWorld = BasePosition + FVector(0.f, 0.f, BuildingHeight);

    UE_LOG(LogTemp, Warning, TEXT("[FocusOnBuilding] Height=%.1f, Ratio=%.1f%%, TopScreen=%.3f, BottomScreen=%.3f, BottomWorld=(%.1f,%.1f,%.1f), TopWorld=(%.1f,%.1f,%.1f)"),
        BuildingHeight, DesiredScreenHeightRatio * 100.0f, TopScreenPosition, BottomScreenPosition,
        BuildingBottomWorld.X, BuildingBottomWorld.Y, BuildingBottomWorld.Z,
        BuildingTopWorld.X, BuildingTopWorld.Y, BuildingTopWorld.Z);

    // 2단계: 목표 ZoomValue 반복 계산
    // ZoomValue가 변하면 FOV와 카메라 각도도 함께 변하므로
    // 반복적으로 계산하여 최적의 ZoomValue를 수렴시킴

    // 줌값 하나가 팔 길이·FOV·피치 동시 변경 → 닫힌 식 없음
    // → 값 대입 후 결과 측정하는 탐색으로만 목표 줌값 산출
    // 현재 ZoomValue 저장 (계산 후 복원용)
    float OriginalZoomValue = 0.5f;
    if (MovementInputHandler)
    {
        OriginalZoomValue = MovementInputHandler->GetZoomValue();

        // 반복적으로 ZoomValue 수렴 (5회 반복으로 정확한 값 도출)
        TargetZoomValue = 0.5f; // 초기 추정값

        for (int32 Iteration = 0; Iteration < 5; ++Iteration)
        {
            // 추정 ZoomValue → FOV (0 = 30도 가까움, 1 = 20도 멀리)
            float EstimatedFOV = FMath::Lerp(30.f, 20.f, TargetZoomValue);
            float HalfFOVRadians = FMath::DegreesToRadians(EstimatedFOV * 0.5f);

            // ZoomValue → 피치 (0 = -40도, 1 = -55도, ApplyZoomSettings와 동일)
            float TargetCameraPitch = FMath::Lerp(-40.0f, -55.0f, TargetZoomValue);

            // 피치 보정: 내려다보는 각도만큼 건물이 수직 압축돼 보임 → PitchCorrection = cos(각도)
            // (-40도 ≈ 0.766, -55도 ≈ 0.574. 가파를수록 더 가까이)
            float PitchRadians = FMath::DegreesToRadians(FMath::Abs(TargetCameraPitch));
            float PitchCorrection = FMath::Cos(PitchRadians);

            // 필요 거리: 보이는 높이 = 건물 높이 × cos(각도) → Distance = 높이 × cos / (tan(FOV/2) × DesiredRatio)
            // (PitchCorrection은 분자에. 건물 높이만 사용)
            float RequiredDistance = (BuildingHeight * PitchCorrection) / (FMath::Tan(HalfFOVRadians) * DesiredScreenHeightRatio);

            // RequiredDistance에 맞는 ZoomValue 이진 탐색 (ZoomCurve 때문에 직접 계산 불가)
            float MinVal = 0.0f;
            float MaxVal = 1.0f;
            float TestZoomValue = TargetZoomValue;

            for (int32 i = 0; i < 15; ++i)
            {
                // 테스트 ZoomValue 적용 (팔 길이 측정에 실제 줌 변경 필요 → 끝에서 원복)
                MovementInputHandler->SetZoomValue(TestZoomValue);
                MovementInputHandler->ApplyZoomSettings();
                float TestArmLength = SpringArm->TargetArmLength;

                // 목표 거리와 차이 계산
                float Diff = TestArmLength - RequiredDistance;

                // 충분히 가까우면 종료 (오차 10 이하)
                if (FMath::Abs(Diff) < 10.0f)
                {
                    break;
                }

                // 이진 탐색: 범위를 절반씩 좁혀감
                if (Diff > 0.0f)  // 거리가 너무 멀면
                {
                    MaxVal = TestZoomValue;  // 상한 낮추기
                }
                else  // 거리가 너무 가까우면
                {
                    MinVal = TestZoomValue;  // 하한 높이기
                }

                TestZoomValue = (MinVal + MaxVal) * 0.5f;
            }

            TargetZoomValue = TestZoomValue;
        }

        // 원래 ZoomValue 복원 (실제 전환은 Tick). 계산 중 줌 잔류 시 카메라 점프
        MovementInputHandler->SetZoomValue(OriginalZoomValue);
        MovementInputHandler->ApplyZoomSettings();
    }

    // 3단계: 카메라 위치 계산 (수평 오프셋)
    FVector CameraRight = GetActorRightVector();

    // 화면 수평 오프셋 비율 계산
    // ScreenCenterRatioX = 0.5면 중앙 (오프셋 없음)
    // ScreenCenterRatioX = 0.25면 왼쪽으로 25% (오프셋 -0.25)
    // ScreenCenterRatioX = 0.75면 오른쪽으로 25% (오프셋 +0.25)
    float ScreenOffsetRatio = ScreenCenterRatioX - 0.5f;

    // 최종 목표 상태의 FOV로 수평 오프셋 계산
    // 이진 탐색으로 구한 TargetZoomValue 적용 시 실제 SpringArm 거리 사용
    float ActualDistance = 0.0f;
    if (MovementInputHandler)
    {
        MovementInputHandler->SetZoomValue(TargetZoomValue);
        MovementInputHandler->ApplyZoomSettings();
        ActualDistance = SpringArm->TargetArmLength;  // 실제 카메라 거리 (ZoomCurve 반영)

        // 원래 ZoomValue로 복원
        MovementInputHandler->SetZoomValue(OriginalZoomValue);
        MovementInputHandler->ApplyZoomSettings();
    }
    else
    {
        ActualDistance = SpringArm->TargetArmLength;  // fallback
    }

    // 수평 오프셋 월드 거리 계산
    // 원리: tan(HorizontalFOV/2) = (화면 가로에 보이는 거리 / 2) / Distance
    // 따라서: HorizontalOffset = ScreenOffsetRatio * Distance * tan(HorizontalFOV/2)
    float FinalFOV = FMath::Lerp(30.f, 20.f, TargetZoomValue);
    float HorizontalFOV = FinalFOV * AspectRatio;
    float HalfHorizontalFOVRadians = FMath::DegreesToRadians(HorizontalFOV * 0.5f);
    float HorizontalOffset = ScreenOffsetRatio * ActualDistance * FMath::Tan(HalfHorizontalFOVRadians);

    // 4단계: 카메라 위치 계산
    // 카메라가 실제로 보는 세로 높이
    float VisibleHeight = ActualDistance * 2.0f * FMath::Tan(FMath::DegreesToRadians(FMath::Lerp(30.f, 20.f, TargetZoomValue) * 0.5f));

    // 카메라 위치 계산: 건물 중심이 화면의 적절한 위치에 오도록
    // 건물 중심이 화면의 (TopScreen + BottomScreen) / 2 위치에 오도록 계산
    float BuildingCenterScreenPosition = (TopScreenPosition + BottomScreenPosition) * 0.5f;
    FVector BuildingCenterWorld = BasePosition + FVector(0.f, 0.f, BuildingHeight * 0.5f);

    // 카메라 중심(0.5)에서 건물 중심이 BuildingCenterScreenPosition에 오도록 오프셋 계산
    float CameraHeightOffset = VisibleHeight * (0.5f - BuildingCenterScreenPosition);
    TargetCameraLocation = BuildingCenterWorld + FVector(0.f, 0.f, CameraHeightOffset) - CameraRight * HorizontalOffset;

    // Tick()에서 부드러운 보간 시작
    BeginCameraTransition();

    if (FocusOcclusionHandler)
    {
        FocusOcclusionHandler->SetFocusTarget(Building);
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] FocusOnBuilding: Height=%f, ScreenRatio=%.1f%%, ZoomValue=%f, Pitch=%f, Distance=%f"),
        BuildingHeight, DesiredScreenHeightRatio * 100.0f, TargetZoomValue,
        FMath::Lerp(-40.0f, -55.0f, TargetZoomValue), ActualDistance);

    return ActualDistance;
}

void APlayerCamera::FocusOnBuildingForPlacement(ABuildingBaseActor* Building, float TopPadding, float BottomPadding, float ScreenHeightRatioScale)
{
    // 1. 진행 중인 카메라 전환 중단
    StopCameraTransition();

    // 2. 건물에 카메라 포커싱
    if (Building)
    {
        FocusOnBuilding(Building, TopPadding, BottomPadding, 0.5f, ScreenHeightRatioScale);
        // 배치 중에는 부지 격자 하이라이트가 화면을 쓰므로 고스트를 겹치지 않는다
        ClearFocusTarget();
        UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] FocusOnBuildingForPlacement: Building=%s"), *Building->GetName());
    }

    // 3. PlacementHandler 입력 매핑 복원
    if (PlacementHandler)
    {
        PlacementHandler->InitPlacementIMC();
        UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] PlacementHandler input restored"));
    }
}

void APlayerCamera::FocusOnActor(AActor* Actor, float ScreenCenterRatioX, bool bTrackOcclusion)
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
    // 모바일 시작 포커스는 로딩 화면 중(StopLoadingScreen 전) 뷰포트가 0x0 → 0 나눗셈 NaN → 카메라 못 도달. 폴백 16:9.
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

    float DesiredScreenHeightRatio = TopScreenPosition - BottomScreenPosition;

    // 현재 ZoomValue 저장
    float OriginalZoomValue = 0.5f;
    if (MovementInputHandler)
    {
        OriginalZoomValue = MovementInputHandler->GetZoomValue();

        // 반복적으로 ZoomValue 수렴
        TargetZoomValue = 0.5f;

        for (int32 Iteration = 0; Iteration < 5; ++Iteration)
        {
            float EstimatedFOV = FMath::Lerp(30.f, 20.f, TargetZoomValue);
            float HalfFOVRadians = FMath::DegreesToRadians(EstimatedFOV * 0.5f);
            float TargetCameraPitch = FMath::Lerp(-40.0f, -55.0f, TargetZoomValue);
            float PitchRadians = FMath::DegreesToRadians(FMath::Abs(TargetCameraPitch));
            float PitchCorrection = FMath::Cos(PitchRadians);

            float RequiredDistance = (ActorHeight * PitchCorrection) / (FMath::Tan(HalfFOVRadians) * DesiredScreenHeightRatio);

            // 이진 탐색으로 ZoomValue 역계산
            float MinVal = 0.0f;
            float MaxVal = 1.0f;
            float TestZoomValue = TargetZoomValue;

            for (int32 i = 0; i < 15; ++i)
            {
                MovementInputHandler->SetZoomValue(TestZoomValue);
                MovementInputHandler->ApplyZoomSettings();
                float TestArmLength = SpringArm->TargetArmLength;

                float Diff = TestArmLength - RequiredDistance;

                if (FMath::Abs(Diff) < 10.0f)
                {
                    break;
                }

                if (Diff > 0.0f)
                {
                    MaxVal = TestZoomValue;
                }
                else
                {
                    MinVal = TestZoomValue;
                }

                TestZoomValue = (MinVal + MaxVal) * 0.5f;
            }

            TargetZoomValue = TestZoomValue;
        }

        // 원래 ZoomValue로 복원
        MovementInputHandler->SetZoomValue(OriginalZoomValue);
        MovementInputHandler->ApplyZoomSettings();
    }

    // 카메라 위치 계산
    FVector CameraRight = GetActorRightVector();
    float ScreenOffsetRatio = ScreenCenterRatioX - 0.5f;

    float ActualDistance = 0.0f;
    if (MovementInputHandler)
    {
        MovementInputHandler->SetZoomValue(TargetZoomValue);
        MovementInputHandler->ApplyZoomSettings();
        ActualDistance = SpringArm->TargetArmLength;

        MovementInputHandler->SetZoomValue(OriginalZoomValue);
        MovementInputHandler->ApplyZoomSettings();
    }
    else
    {
        ActualDistance = SpringArm->TargetArmLength;
    }

    float FinalFOV = FMath::Lerp(30.f, 20.f, TargetZoomValue);
    float HorizontalFOV = FinalFOV * AspectRatio;
    float HalfHorizontalFOVRadians = FMath::DegreesToRadians(HorizontalFOV * 0.5f);
    float HorizontalOffset = ScreenOffsetRatio * ActualDistance * FMath::Tan(HalfHorizontalFOVRadians);

    float VisibleHeight = ActualDistance * 2.0f * FMath::Tan(FMath::DegreesToRadians(FinalFOV * 0.5f));
    float BuildingCenterScreenPosition = (TopScreenPosition + BottomScreenPosition) * 0.5f;
    FVector ActorCenterWorld = BasePosition + FVector(0.f, 0.f, ActorHeight * 0.5f);

    float CameraHeightOffset = VisibleHeight * (0.5f - BuildingCenterScreenPosition);
    TargetCameraLocation = ActorCenterWorld + FVector(0.f, 0.f, CameraHeightOffset) - CameraRight * HorizontalOffset;

    BeginCameraTransition();

    if (bTrackOcclusion && FocusOcclusionHandler)
    {
        FocusOcclusionHandler->SetFocusTarget(Actor);
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] FocusOnActor: %s, Height=%f, ZoomValue=%f"),
        *Actor->GetName(), ActorHeight, TargetZoomValue);
}

void APlayerCamera::FocusOnLocation(const FVector& TargetLocation, float DesiredDistance)
{
    StopCameraTransition();

    TargetCameraLocation = TargetLocation;
    BeginCameraTransition();

    if (MovementInputHandler)
    {
        // DesiredDistance가 지정되면 줌 값 조절, 아니면 현재 줌 유지
        if (DesiredDistance > 0.f && SpringArm)
        {
            float SavedZoomValue = MovementInputHandler->GetZoomValue();

            // 이진 탐색으로 DesiredDistance에 맞는 ZoomValue 찾기
            float MinVal = 0.0f;
            float MaxVal = 1.0f;
            TargetZoomValue = 0.5f;

            for (int32 i = 0; i < 15; ++i)
            {
                MovementInputHandler->SetZoomValue(TargetZoomValue);
                MovementInputHandler->ApplyZoomSettings();
                float TestArmLength = SpringArm->TargetArmLength;

                float Diff = TestArmLength - DesiredDistance;

                if (FMath::Abs(Diff) < 10.0f)
                {
                    break;
                }

                if (Diff > 0.0f)
                {
                    MaxVal = TargetZoomValue;
                }
                else
                {
                    MinVal = TargetZoomValue;
                }

                TargetZoomValue = (MinVal + MaxVal) * 0.5f;
            }

            // 원래 ZoomValue로 복원 (Tick에서 보간 시작)
            MovementInputHandler->SetZoomValue(SavedZoomValue);
            MovementInputHandler->ApplyZoomSettings();
        }
        else
        {
            // 줌 변경 없이 현재 값 유지
            TargetZoomValue = MovementInputHandler->GetZoomValue();
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerCamera] FocusOnLocation: Target=(%.1f, %.1f, %.1f), Distance=%.1f"),
        TargetLocation.X, TargetLocation.Y, TargetLocation.Z, DesiredDistance);
}

void APlayerCamera::FocusOnAreaRadius(const FVector& Center, float WorldRadiusCm)
{
    if (!MovementInputHandler)
    {
        return;
    }

    // FOV가 좁아(~20도) 반경 R 원을 다 담으려면 팔길이 ≈ R * KeystoneAuraZoomFactor
    const float DesiredArm = WorldRadiusCm * KeystoneAuraZoomFactor;

    // 평소 MaxZoomDistance는 1회만 저장 (이미 확장 상태면 덮어쓰지 않음)
    if (SavedMaxZoomDistance < 0.f)
    {
        SavedMaxZoomDistance = MovementInputHandler->MaxZoomDistance;
    }

    // 줌 트랜지션(이진 탐색)이 DesiredArm까지 도달하도록 상한을 일시 확장 (여유 10%)
    MovementInputHandler->MaxZoomDistance = FMath::Max(SavedMaxZoomDistance, DesiredArm * 1.1f);

    // 오른쪽 Manage 패널에 가리지 않게 — 시점 중심을 카메라 우측으로 옮겨 건물이 화면 좌측(~1/4)에 오도록.
    const float ScreenShiftFactor = 0.08f; // 일반 건물 클릭 위치(화면 ~0.3)에 맞춤 — 좁은 FOV라 작은 값이 적당 (튜닝)
    FVector RightGround = CameraComponent ? CameraComponent->GetRightVector() : FVector::RightVector;
    RightGround.Z = 0.f;
    RightGround.Normalize();
    const FVector ShiftedCenter = Center + RightGround * (DesiredArm * ScreenShiftFactor);

    FocusOnLocation(ShiftedCenter, DesiredArm);
}

void APlayerCamera::FocusOnBuildingOrAura(ABuildingBaseActor* Building, float TopPadding, float BottomPadding, float ScreenCenterRatioX, float AuraRadiusCm)
{
    // 기본은 건물 층수 비례 포커싱. 단, 영향권 반경이 그보다 더 줌아웃을 요구하면 반경이 다 보이게 더 넓게(둘 중 큰 줌).
    const float BuildingArm = FocusOnBuilding(Building, TopPadding, BottomPadding, ScreenCenterRatioX);
    if (Building && AuraRadiusCm > 0.f && AuraRadiusCm * KeystoneAuraZoomFactor > BuildingArm)
    {
        FocusOnAreaRadius(Building->GetActorLocation(), AuraRadiusCm);
    }
}

void APlayerCamera::RestoreZoomRange()
{
    if (!MovementInputHandler)
    {
        return;
    }

    if (SavedMaxZoomDistance >= 0.f)
    {
        MovementInputHandler->MaxZoomDistance = SavedMaxZoomDistance;
        SavedMaxZoomDistance = -1.f;
    }
}
