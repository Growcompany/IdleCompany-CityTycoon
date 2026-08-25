// Fill out your copyright notice in the Description page of Project Settings.

#include "MovementInputHandler.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SphereComponent.h"
#include "Player/PlayerCamera.h"
#include "Engine/EngineTypes.h"   
#include "Player/MainMapPlayerController.h"
#include "Player/Components/PlacementHandler.h"
#include "Util/CoordinateUtils.h"
#include "Input/InputPriorities.h"

// Sets default values for this component's properties
UMovementInputHandler::UMovementInputHandler()
{
    // TickComponent 미오버라이드 - MoveTracking은 BeginPlay의 SetTimer 0.0166s가 담당
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC
    (TEXT("/Script/EnhancedInput.InputMappingContext'/Game/CompanyGrowth/Input/IMC_BaseInput.IMC_BaseInput'"));
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> DragMoveIMC
    (TEXT("/Script/EnhancedInput.InputMappingContext'/Game/CompanyGrowth/Input/IMC_DragMove.IMC_DragMove'"));
    static ConstructorHelpers::FObjectFinder<UInputAction> MoveAction
    (TEXT("/Script/EnhancedInput.InputAction'/Game/CompanyGrowth/Input/IA_Move.IA_Move'"));
    static ConstructorHelpers::FObjectFinder<UInputAction> ZoomAction
    (TEXT("/Script/EnhancedInput.InputAction'/Game/CompanyGrowth/Input/IA_Zoom.IA_Zoom'"));
    static ConstructorHelpers::FObjectFinder<UInputAction> SpinAction
    (TEXT("/Script/EnhancedInput.InputAction'/Game/CompanyGrowth/Input/IA_Spin.IA_Spin'"));
    static ConstructorHelpers::FObjectFinder<UInputAction> DragMoveAction
    (TEXT("/Script/EnhancedInput.InputAction'/Game/CompanyGrowth/Input/IA_DragMove.IA_DragMove'"));
    static ConstructorHelpers::FObjectFinder<UCurveFloat> c_zoom
    (TEXT("/Script/Engine.CurveFloat'/Game/CompanyGrowth/Blueprint/Player/Curve/C_Zoom.C_Zoom'"));

    if (IMC.Succeeded()) InputMapping = IMC.Object;
    if (DragMoveIMC.Succeeded()) IMC_DragMoveContext = DragMoveIMC.Object;
    if (MoveAction.Succeeded()) IA_Move = MoveAction.Object;
    if (ZoomAction.Succeeded()) IA_Zoom = ZoomAction.Object;
    if (SpinAction.Succeeded()) IA_Spin = SpinAction.Object;
    if (DragMoveAction.Succeeded()) IA_DragMove = DragMoveAction.Object;
    if (c_zoom.Succeeded()) { ZoomCurve = c_zoom.Object; }
}

void UMovementInputHandler::Initialize(UFloatingPawnMovement* InMovement)
{
    Movement = InMovement;

    Owner = Cast<APlayerCamera>(GetOwner());
    UE_LOG(LogTemp, Log, TEXT("[UMovementInputHandler] Initialize"));
}

void UMovementInputHandler::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    UE_LOG(LogTemp, Log, TEXT("UMovementInputHandler::SetupPlayerInputComponent started."));
    if (Owner && Owner->GetController())
    {
        if (APlayerController* PlayerController = Cast<APlayerController>(Owner->GetController()))
        {
            if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
            {
                InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
                if (InputSubsystem)
                {
                    InputSubsystem->AddMappingContext(InputMapping, 0);

                    UE_LOG(LogTemp, Log, TEXT("InputMappingContext added."));

                    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
                    {
                        EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &UMovementInputHandler::Move);
                        EnhancedInputComponent->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &UMovementInputHandler::Zoom);
                        EnhancedInputComponent->BindAction(IA_Spin, ETriggerEvent::Triggered, this, &UMovementInputHandler::Spin);
                        EnhancedInputComponent->BindAction(IA_DragMove, ETriggerEvent::Started, this, &UMovementInputHandler::OnDragStarted);
                        EnhancedInputComponent->BindAction(IA_DragMove, ETriggerEvent::Triggered, this, &UMovementInputHandler::OnDragMove);
                    }
                }
            }
        }
    }

}


// Called when the game starts
void UMovementInputHandler::BeginPlay()
{
    Super::BeginPlay();

    // ZoomValue 계산
    ZoomValue = 0.38f;

    UpdateZoom();


    FTimerHandle MoveTrackingHandle;
    GetWorld()->GetTimerManager().SetTimer(MoveTrackingHandle, this,
        &UMovementInputHandler::MoveTracking, 0.016666f,
        true, 0.0f);
}

void UMovementInputHandler::MoveTracking()
{
    auto actorLocation = Owner->GetActorLocation();
    float locationLength = actorLocation.Length();
    auto normalizedVec = actorLocation.Normalize();

    actorLocation.Z = 0.f;
    actorLocation = -actorLocation;

    // 범위 시각화 - BoundaryRadius 반지름 원 그리기 (디버그용)
    //DrawDebugCircle(GetWorld(), FVector::ZeroVector, BoundaryRadius, 128, FColor::Red, false, 0.1f, 0, 200.f,
    //    FVector(0, 0, 1), FVector(1, 0, 0));

    // 범위 밖이면 강제로 중앙쪽으로 당기기
    locationLength = FMath::Max(0.f, (locationLength - BoundaryRadius) / 2.f);
    Owner->AddMovementInput(actorLocation, locationLength);

    // 입력 타입 검사
    EInputType inputType = Owner->GetPlayerController()->GetCurrentInputType();
    if (inputType == EInputType::KeyMouse)
    {
        FVector2D ScreenPos;
        FVector intersection;
        CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, intersection);
        intersection.Z += 10.f;
        Owner->Collision->SetWorldLocation(intersection);
    }

}

void UMovementInputHandler::TrackMove()
{
    FVector2D ScreenPos;
    FVector IntersectionPos;
    if (CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, IntersectionPos))
    {
        FVector offset;
        FVector forwardVec = Owner->SpringArm->GetForwardVector();
        FVector upVec = Owner->SpringArm->GetUpVector();
        FVector socketOffset = Owner->SpringArm->SocketOffset;
        FVector springArmWorldLoc = Owner->SpringArm->GetComponentLocation();
        FVector cameraWorldLoc = Owner->CameraComponent->GetComponentLocation();

        FVector offsetApplyForwardVec = -(forwardVec * (Owner->SpringArm->TargetArmLength - socketOffset.X));
        offsetApplyForwardVec = offsetApplyForwardVec + (upVec * socketOffset.Z) + springArmWorldLoc;
        offset = offsetApplyForwardVec - cameraWorldLoc;

        FVector newMove = TargetHandle - IntersectionPos - offset;

        // 터치 떨림 방지: 이동 거리가 너무 작으면 무시 (작을수록 느린 드래그도 즉시 반응)
        float moveDistance = FVector(newMove.X, newMove.Y, 0.f).Size();
        if (moveDistance < 8.0f)
        {
            return;
        }

        // 스무딩 alpha: 높을수록 손가락 추종 밀착(잔상↓), 낮을수록 떨림억제↑. 0.8 = 밀착 위주
        StoredMove = FMath::Lerp(StoredMove, newMove, 0.8f);

        // 범위제한 및 점진적 저항 적용
        FVector currentLocation = Owner->GetActorLocation();
        float currentDistance = currentLocation.Length();

        float resistanceFactor = 1.0f;
        if (currentDistance > BoundaryRadius)
        {
            // 현재 위치와 이동 방향 확인
            FVector currentPos = currentLocation;
            FVector moveDirection = FVector(StoredMove.X, StoredMove.Y, 0.f).GetSafeNormal();
            FVector centerDirection = -currentPos.GetSafeNormal();

            // 중앙으로 향하는지 확인 (내적으로 방향 비교)
            float dotProduct = FVector::DotProduct(moveDirection, centerDirection);

            if (dotProduct > 0.3f)  // 중앙 방향으로 이동하면 저항 없음
            {
                resistanceFactor = 1.0f;  // 저항 없음
            }
            else  // 중앙에서 멀어지면 저항 적용
            {
                // 경계 근처에서 점진적으로 저항 증가 (BoundaryRadius 기준 ±10% 범위)
                float softZoneStart = BoundaryRadius * 0.9f;
                float softZoneRange = BoundaryRadius * 0.2f;
                float distanceRatio = (currentDistance - softZoneStart) / softZoneRange;  // 0~1 범위
                resistanceFactor = FMath::Max(0.01f, 1.0f - distanceRatio * 0.99f);
            }
        }
        FVector adjustedMove = FVector(StoredMove.X, StoredMove.Y, 0.f) * resistanceFactor;
        Owner->AddActorWorldOffset(adjustedMove);
    }
}

void UMovementInputHandler::GetEdgeMove(FVector& Direction, float& Strength)
{
    FVector2D ScreenPos;
    FVector IntersectionPos;
    CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, IntersectionPos);


    FVector2D viewportCenter = CoordinateUtils::GetViewportCenter();

    FVector temDirection;

    CursorDistFromViewportCenter(ScreenPos - viewportCenter, temDirection, Strength);

    Direction = Owner->GetActorTransform().TransformVectorNoScale(temDirection);
}

void UMovementInputHandler::CursorDistFromViewportCenter(FVector2D mousePosFromViewportCenter, FVector& Direction,
    float& Strength) const
{
    FVector2D edgeDetectDistance = CoordinateUtils::GetViewportCenter();

    switch (Owner->GetPlayerController()->GetCurrentInputType())
    {
    case EInputType::KeyMouse:
        edgeDetectDistance.X -= EdgeMoveDistance;
        edgeDetectDistance.Y -= EdgeMoveDistance;
        break;
    case EInputType::GamePad:
        edgeDetectDistance.X -= EdgeMoveDistance * 2.f;
        edgeDetectDistance.Y -= EdgeMoveDistance * 2.f;
        break;
    case EInputType::Touch:
        edgeDetectDistance.X -= EdgeMoveDistance * 2.f;
        edgeDetectDistance.Y -= EdgeMoveDistance * 2.f;
        break;
    }

    FVector2D absPos = mousePosFromViewportCenter.GetAbs();
    absPos -= edgeDetectDistance;
    absPos = FVector2D::Max(absPos, FVector2D::ZeroVector);
    absPos /= EdgeMoveDistance;

    FVector2D signVec = FVector2D(FMath::Sign(mousePosFromViewportCenter.X) * absPos.X,
        FMath::Sign(mousePosFromViewportCenter.Y) * absPos.Y * -1.f);
    Direction = FVector(signVec.Y, signVec.X, 0.f);
    Strength = 1.f;
}


void UMovementInputHandler::UpdateZoom()
{
    ZoomValue += ZoomDirection * 0.01f;
    ZoomValue = FMath::Clamp(ZoomValue, 0.0f, 1.f);

    ApplyZoomSettings();
}

void UMovementInputHandler::ApplyZoomSettings()
{
    float lerpKey = ZoomCurve != nullptr ? ZoomCurve->GetFloatValue(ZoomValue) : 0.5f;

    Owner->SpringArm->TargetArmLength = FMath::Lerp(MinZoomDistance, MaxZoomDistance, lerpKey);
    Owner->SpringArm->SetRelativeRotation(FRotator(FMath::Lerp(ZoomInPitch, ZoomOutPitch, lerpKey), 0.f, 0.f));

    const float NewMaxSpeed = FMath::Lerp(8000.f, 48000.f, lerpKey);
    Owner->MovementComponent->MaxSpeed = NewMaxSpeed;
    // 가속/감속을 MaxSpeed에 비례(줌 무관 일정 체감). 배수 = 이즈 정도: 6 ≈ 0.17s 이즈인/아웃. ↓=더 부드럽게, ↑=더 칼각
    Owner->MovementComponent->Acceleration = NewMaxSpeed * 6.f;
    Owner->MovementComponent->Deceleration = NewMaxSpeed * 6.f;

    Owner->CameraComponent->FieldOfView = FMath::Lerp(ZoomInFOV, ZoomOutFOV, lerpKey);
}

void UMovementInputHandler::UpdateDof() const
{
    Owner->CameraComponent->PostProcessSettings.bOverride_DepthOfFieldFstop = 3.f;
    Owner->CameraComponent->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = 150.f;
    Owner->CameraComponent->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = Owner->SpringArm->TargetArmLength;
}

void UMovementInputHandler::PositionCheck()
{
    FVector2D ScreenPos;
    FVector   IntersectionPos;
    if (!CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, IntersectionPos))
        return;

    TargetHandle = IntersectionPos;

    // 입력 타입 검사
    EInputType inputType = Owner->GetPlayerController()->GetCurrentInputType();
    if (inputType == EInputType::KeyMouse)
    {
        Owner->Collision->SetWorldLocation(TargetHandle);
    }
}

void UMovementInputHandler::Move(const FInputActionValue& Value)
{
    Owner->StopCameraTransition();

    FVector currentLocation = Owner->GetActorLocation();
    float currentDistance = currentLocation.Length();

    // 현재 범위 내에 있을 때만 이동 허용
    // 점진적 저항 계산
    float resistanceFactor = 1.0f;
    float softZoneStart = BoundaryRadius * 0.9f;
    if (currentDistance > softZoneStart)
    {
        FVector moveDirection = (Owner->GetActorForwardVector() * Value[1] + Owner->GetActorRightVector()
            * Value[0]).GetSafeNormal();
        FVector centerDirection = -currentLocation.GetSafeNormal();
        float dotProduct = FVector::DotProduct(moveDirection, centerDirection);

        if (dotProduct > 0.3f)  // 중앙 방향으로 이동하면 저항 없음
        {
            resistanceFactor = 1.0f;  // 저항 없음
        }
        else  // 중앙에서 멀어지면 저항 적용
        {
            // 경계 근처에서 점진적으로 저항 증가 (BoundaryRadius 기준 ±10% 범위)
            float softZoneRange = BoundaryRadius * 0.2f;
            float distanceRatio = (currentDistance - softZoneStart) / softZoneRange;  // 0~1 범위
            resistanceFactor = FMath::Max(0.01f, 1.0f - distanceRatio * 0.99f);
        }
    }

    Owner->AddMovementInput(Owner->GetActorForwardVector(), Value[1] * resistanceFactor);
    Owner->AddMovementInput(Owner->GetActorRightVector(), Value[0] * resistanceFactor);

    Owner->PlacementHandler->UpdateTrackMovePlacement();
}

void UMovementInputHandler::Zoom(const FInputActionValue& Value)
{
    Owner->StopCameraTransition();

    ZoomDirection = Value[0];
    UpdateZoom();
    UpdateDof();
    UE_LOG(LogTemp, Log, TEXT("Zoom value = %f"), Value[0]);
}

void UMovementInputHandler::Spin(const FInputActionValue& Value)
{
    Owner->StopCameraTransition();

    // 리그를 돌리면 화면(월드)은 반대로 보여 제스처와 어긋남 → yaw 부호 반전
    Owner->AddActorLocalRotation(FRotator(0, -Value[0], 0));
    UE_LOG(LogTemp, Log, TEXT("Spin value = %f"), Value[0]);
}

bool UMovementInputHandler::GetTouchIntersection(FVector& OutIntersection) const
{
    FVector2D ScreenPos;
    return CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, OutIntersection);
}

void UMovementInputHandler::OnDragStarted(const FInputActionValue& Value)
{
    Owner->StopCameraTransition();

    UE_LOG(LogTemp, Log, TEXT("OnDragStarted"));
    PositionCheck();
}

void UMovementInputHandler::OnDragMove(const FInputActionValue& Value)
{
    bool bSingle = CoordinateUtils::SingleTouchCheck();

    // 멀티터치(Zoom/Rotate) 상태에서 → 싱글터치로 바뀌면
    if (bSingle && !bWasSingleTouch)
    {
        // 터치 시작 위치 갱신
        PositionCheck();
    }

    bWasSingleTouch = bSingle;

    if (bSingle)
    {
        TrackMove();
    }

}


void UMovementInputHandler::InitDragMoveIMC() const
{
    if (InputSubsystem)
    {
        InputSubsystem->AddMappingContext(IMC_DragMoveContext, InputPriority::MOVEMENT);
    }
}


void UMovementInputHandler::ReleaseDragMoveIMC() const
{
    if (InputSubsystem)
    {
        FModifyContextOptions options;
        options.bIgnoreAllPressedKeysUntilRelease = false;
        options.bForceImmediately = true;
        options.bNotifyUserSettings = false;

        InputSubsystem->RemoveMappingContext(IMC_DragMoveContext, options);
    }
}


