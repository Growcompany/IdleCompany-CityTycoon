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
#include "Office/OfficeRecruitmentFlowSubsystem.h"
#include "Player/CameraViewBounds.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"

namespace
{
    void GetBoundaryProjectionContext(const APlayerCamera* CameraPawn, FIntRect& OutRect,
        EAspectRatioAxisConstraint& OutAxisConstraint)
    {
        int32 Width = 1920;
        int32 Height = 1080;
        const APlayerController* ViewController = CameraPawn ? Cast<APlayerController>(CameraPawn->GetController()) : nullptr;
        const ULocalPlayer* LocalPlayer = ViewController ? ViewController->GetLocalPlayer() : nullptr;
        if (ViewController)
        {
            int32 ViewWidth = 0;
            int32 ViewHeight = 0;
            ViewController->GetViewportSize(ViewWidth, ViewHeight);
            if (ViewWidth > 0 && ViewHeight > 0)
            {
                Width = ViewWidth;
                Height = ViewHeight;
            }
        }
        OutRect = FIntRect(0, 0, Width, Height);
        OutAxisConstraint = LocalPlayer ? LocalPlayer->AspectRatioAxisConstraint.GetValue()
            : GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint.GetValue();
    }

    FVector ClampLocationInBounds(const FVector& WorldLocation, const FBox2D& Bounds, const FTransform& Transform)
    {
        FVector LocalLocation = Transform.InverseTransformPosition(WorldLocation);
        LocalLocation.X = FMath::Clamp(LocalLocation.X, Bounds.Min.X, Bounds.Max.X);
        LocalLocation.Y = FMath::Clamp(LocalLocation.Y, Bounds.Min.Y, Bounds.Max.Y);
        FVector BoundedLocation = Transform.TransformPosition(LocalLocation);
        BoundedLocation.Z = WorldLocation.Z;
        return BoundedLocation;
    }

    bool IsRecruitmentInputBlocked(const UObject* Context)
    {
        const UWorld* InputWorld = Context ? Context->GetWorld() : nullptr;
        const UOfficeRecruitmentFlowSubsystem* RecruitmentFlow = InputWorld
            ? InputWorld->GetSubsystem<UOfficeRecruitmentFlowSubsystem>() : nullptr;
        return RecruitmentFlow && RecruitmentFlow->IsBusy();
    }
}

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

void UMovementInputHandler::SetBoundaryCenterWorld(const FVector& InBoundaryCenterWorld)
{
    BoundaryCenterWorld = InBoundaryCenterWorld;
}

FVector UMovementInputHandler::CalculatePlanarBoundaryOffset(const FVector& WorldLocation) const
{
    FVector BoundaryOffset = WorldLocation - BoundaryCenterWorld;
    BoundaryOffset.Z = 0.f;
    return BoundaryOffset;
}

bool UMovementInputHandler::SetPlanarMovementBounds(const FBox2D& InLocalBounds, const FTransform& InBoundsTransform,
    bool bInConstrainCameraView)
{
    const FVector BoundsScale = InBoundsTransform.GetScale3D();
    if (!InLocalBounds.bIsValid
        || !FMath::IsFinite(InLocalBounds.Min.X) || !FMath::IsFinite(InLocalBounds.Min.Y)
        || !FMath::IsFinite(InLocalBounds.Max.X) || !FMath::IsFinite(InLocalBounds.Max.Y)
        || InLocalBounds.Min.X > InLocalBounds.Max.X || InLocalBounds.Min.Y > InLocalBounds.Max.Y
        || InBoundsTransform.ContainsNaN()
        || FMath::IsNearlyZero(BoundsScale.X) || FMath::IsNearlyZero(BoundsScale.Y)
        || FMath::IsNearlyZero(BoundsScale.Z)
        || (bInConstrainCameraView && (BoundsScale.Z < 0.f
            || !InBoundsTransform.GetRotation().GetUpVector().Equals(FVector::UpVector, 0.0001f))))
    {
        return false;
    }
    PlanarMovementBounds = InLocalBounds;
    MovementBoundsTransform = InBoundsTransform;
    bConstrainCameraView = bInConstrainCameraView;
    bReportedViewBoundsFailure = false;
    return true;
}

FMinimalViewInfo UMovementInputHandler::BuildBoundaryView(const FVector& PawnLocation, float Zoom, float ProjectionOffset) const
{
    FMinimalViewInfo View;
    Owner->CameraComponent->GetCameraView(0.f, View);
    const FZoomRigSample Rig = EvaluateZoomRig(Zoom);
    const FQuat PawnRotation = Owner->GetActorQuat();
    const FQuat RigRotation = PawnRotation * FRotator(Rig.PitchDeg, 0.f, 0.f).Quaternion();
    const FVector ArmOrigin = PawnLocation + PawnRotation.RotateVector(Owner->SpringArm->GetRelativeLocation());
    const FVector SocketLocation = ArmOrigin + Owner->SpringArm->TargetOffset
        + RigRotation.RotateVector(Owner->SpringArm->SocketOffset - FVector(Rig.ArmLength, 0.f, 0.f));
    const FTransform CameraTransform = Owner->CameraComponent->GetRelativeTransform() * FTransform(RigRotation, SocketLocation);
    View.Location = CameraTransform.GetLocation();
    View.Rotation = CameraTransform.Rotator();
    View.FOV = Rig.HorizontalFOVDeg;
    View.OffCenterProjectionOffset.X = ProjectionOffset;
    return View;
}

bool UMovementInputHandler::GetCameraMovementBounds(const FVector& PawnLocation, float Zoom,
    float ProjectionOffset, FBox2D& OutBounds) const
{
    if (!PlanarMovementBounds.bIsValid) return false;
    OutBounds = PlanarMovementBounds;
    if (!bConstrainCameraView) return true;
    if (!Owner || !Owner->CameraComponent || !Owner->SpringArm) return false;

    FIntRect ViewRect;
    EAspectRatioAxisConstraint AxisConstraint;
    GetBoundaryProjectionContext(Owner, ViewRect, AxisConstraint);
    FBox2D Footprint;
    FBox2D Translation;
    if (!CameraViewBounds::GetGroundFootprint(BuildBoundaryView(PawnLocation, Zoom, ProjectionOffset),
        ViewRect, AxisConstraint, MovementBoundsTransform, Footprint)
        || !CameraViewBounds::GetTranslationBounds(PlanarMovementBounds, Footprint, Translation)) return false;

    const FVector2D LocalPivot(MovementBoundsTransform.InverseTransformPosition(PawnLocation));
    const FVector2D Minimum = FVector2D::Max(PlanarMovementBounds.Min, LocalPivot + Translation.Min);
    const FVector2D Maximum = FVector2D::Min(PlanarMovementBounds.Max, LocalPivot + Translation.Max);
    if (Minimum.X > Maximum.X || Minimum.Y > Maximum.Y) return false;
    OutBounds = FBox2D(Minimum, Maximum);
    return true;
}

bool UMovementInputHandler::TryConstrainCameraPose(FVector& InOutLocation, float& InOutZoom, float ProjectionOffset) const
{
    if (!bConstrainCameraView)
    {
        InOutLocation = ClampToMovementBoundary(InOutLocation);
        return true;
    }
    const float RequestedZoom = FMath::Clamp(InOutZoom, 0.f, MaximumZoomValue);
    FBox2D AllowedBounds;
    float AllowedZoom = RequestedZoom;
    if (!GetCameraMovementBounds(InOutLocation, AllowedZoom, ProjectionOffset, AllowedBounds))
    {
        // Curves can change pitch and FOV non-monotonically; refine only the first fitting interval.
        bool bFound = false;
        for (int32 Step = 1; Step <= 32; ++Step)
        {
            const float Candidate = RequestedZoom * (1.f - Step / 32.f);
            if (!GetCameraMovementBounds(InOutLocation, Candidate, ProjectionOffset, AllowedBounds)) continue;
            float Fit = Candidate;
            float Outside = RequestedZoom * (1.f - (Step - 1) / 32.f);
            for (int32 Refine = 0; Refine < 8; ++Refine)
            {
                const float Midpoint = (Fit + Outside) * 0.5f;
                FBox2D TrialBounds;
                if (GetCameraMovementBounds(InOutLocation, Midpoint, ProjectionOffset, TrialBounds))
                {
                    Fit = Midpoint;
                    AllowedBounds = TrialBounds;
                }
                else Outside = Midpoint;
            }
            AllowedZoom = Fit;
            bFound = true;
            break;
        }
        if (!bFound) return false;
    }
    InOutZoom = AllowedZoom;
    InOutLocation = ClampLocationInBounds(InOutLocation, AllowedBounds, MovementBoundsTransform);
    return true;
}

void UMovementInputHandler::ConstrainCameraView(FMinimalViewInfo& InOutView) const
{
    if (!bConstrainCameraView || !Owner) return;
    FIntRect ViewRect;
    EAspectRatioAxisConstraint AxisConstraint;
    GetBoundaryProjectionContext(Owner, ViewRect, AxisConstraint);
    const auto TryCorrectView = [&](FMinimalViewInfo& View)
    {
        FBox2D Footprint;
        FBox2D Translation;
        if (!CameraViewBounds::GetGroundFootprint(View, ViewRect, AxisConstraint, MovementBoundsTransform, Footprint)
            || !CameraViewBounds::GetTranslationBounds(PlanarMovementBounds, Footprint, Translation)) return false;
        const FVector LocalDelta(FMath::Clamp(0.0, Translation.Min.X, Translation.Max.X),
            FMath::Clamp(0.0, Translation.Min.Y, Translation.Max.Y), 0.0);
        View.Location += MovementBoundsTransform.TransformVector(LocalDelta);
        return true;
    };

    if (TryCorrectView(InOutView))
    {
        bReportedViewBoundsFailure = false;
        return;
    }

    // Rotation lag can momentarily need more room than either endpoint. Use the safe target rig in that frame.
    FVector SafeLocation = Owner->GetActorLocation();
    float SafeZoom = ZoomValue;
    if (TryConstrainCameraPose(SafeLocation, SafeZoom, Owner->HorizontalProjectionOffset))
    {
        FMinimalViewInfo SafeView = BuildBoundaryView(SafeLocation, SafeZoom, Owner->HorizontalProjectionOffset);
        if (TryCorrectView(SafeView))
        {
            InOutView.Location = SafeView.Location;
            InOutView.Rotation = SafeView.Rotation;
            InOutView.FOV = SafeView.FOV;
            bReportedViewBoundsFailure = false;
            return;
        }
    }
    if (!bReportedViewBoundsFailure)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CityCamera] Authored bounds cannot contain the minimum camera rig."));
        bReportedViewBoundsFailure = true;
    }
}

FVector UMovementInputHandler::ClampToMovementBoundary(const FVector& WorldLocation) const
{
    if (!PlanarMovementBounds.bIsValid || WorldLocation.ContainsNaN()) { return WorldLocation; }
    FBox2D AllowedBounds = PlanarMovementBounds;
    if (bConstrainCameraView && Owner)
        GetCameraMovementBounds(WorldLocation, ZoomValue, Owner->HorizontalProjectionOffset, AllowedBounds);
    return ClampLocationInBounds(WorldLocation, AllowedBounds, MovementBoundsTransform);
}

void UMovementInputHandler::ApplyMovementBoundary()
{
    if (!Owner || !PlanarMovementBounds.bIsValid) { return; }
    const FVector CurrentLocation = Owner->GetActorLocation();
    FBox2D AllowedBounds = PlanarMovementBounds;
    if (bConstrainCameraView)
        GetCameraMovementBounds(CurrentLocation, ZoomValue, Owner->HorizontalProjectionOffset, AllowedBounds);
    const FVector BoundedLocation = ClampLocationInBounds(CurrentLocation, AllowedBounds, MovementBoundsTransform);
    if (!CurrentLocation.Equals(BoundedLocation, 0.001f))
    {
        Owner->SetActorLocation(BoundedLocation);
        StoredMove = FVector::ZeroVector;
    }
    if (Movement)
    {
        const FVector LocalLocation = MovementBoundsTransform.InverseTransformPosition(BoundedLocation);
        FVector LocalVelocity = MovementBoundsTransform.InverseTransformVectorNoScale(Movement->Velocity);
        if ((LocalLocation.X >= AllowedBounds.Max.X - 0.01 && LocalVelocity.X > 0.f)
            || (LocalLocation.X <= AllowedBounds.Min.X + 0.01 && LocalVelocity.X < 0.f))
        {
            LocalVelocity.X = 0.f;
        }
        if ((LocalLocation.Y >= AllowedBounds.Max.Y - 0.01 && LocalVelocity.Y > 0.f)
            || (LocalLocation.Y <= AllowedBounds.Min.Y + 0.01 && LocalVelocity.Y < 0.f))
        {
            LocalVelocity.Y = 0.f;
        }
        Movement->Velocity = MovementBoundsTransform.TransformVectorNoScale(LocalVelocity);
    }
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
    if (!Owner) { return; }
    if (IsRecruitmentInputBlocked(this))
    {
        StoredMove = FVector::ZeroVector;
        bWasSingleTouch = false;
        Owner->ConsumeMovementInputVector();
        if (Movement) Movement->StopMovementImmediately();
        return;
    }
    if (PlanarMovementBounds.bIsValid)
    {
        ApplyMovementBoundary();
    }
    else
    {
        const FVector BoundaryOffset = CalculatePlanarBoundaryOffset(Owner->GetActorLocation());
        float BoundaryDistance = BoundaryOffset.Size();
        const FVector DirectionToCenter = -BoundaryOffset.GetSafeNormal();

        // 범위 시각화 - BoundaryRadius 반지름 원 그리기 (디버그용)
        //DrawDebugCircle(GetWorld(), FVector::ZeroVector, BoundaryRadius, 128, FColor::Red, false, 0.1f, 0, 200.f,
        //    FVector(0, 0, 1), FVector(1, 0, 0));

        // 범위 밖이면 강제로 중앙쪽으로 당기기
        BoundaryDistance = FMath::Max(0.f, (BoundaryDistance - BoundaryRadius) / 2.f);
        Owner->AddMovementInput(DirectionToCenter, BoundaryDistance);
    }

    // 입력 타입 검사
    if (!Owner->GetPlayerController()) { return; }
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
        if (bConstrainCameraView)
        {
            const APlayerController* ViewController = Cast<APlayerController>(Owner->GetController());
            if (ViewController && ViewController->PlayerCameraManager)
                cameraWorldLoc = ViewController->PlayerCameraManager->GetCameraLocation();
        }

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
        const FVector BoundaryOffset = CalculatePlanarBoundaryOffset(Owner->GetActorLocation());
        const float currentDistance = BoundaryOffset.Size();

        float resistanceFactor = 1.0f;
        if (!PlanarMovementBounds.bIsValid && currentDistance > BoundaryRadius)
        {
            // 현재 위치와 이동 방향 확인
            FVector moveDirection = FVector(StoredMove.X, StoredMove.Y, 0.f).GetSafeNormal();
            FVector centerDirection = -BoundaryOffset.GetSafeNormal();

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
        const FVector CurrentLocation = Owner->GetActorLocation();
        const FVector BoundedLocation = ClampToMovementBoundary(CurrentLocation + adjustedMove);
        const FVector ActualMove = BoundedLocation - CurrentLocation;
        Owner->AddActorWorldOffset(ActualMove);
        if (!ActualMove.Equals(adjustedMove, 0.001f))
        {
            // 손가락 기준점도 실제 이동량을 따라가야 경계에서 반대로 드래그할 때 즉시 반응한다.
            TargetHandle += ActualMove - FVector(newMove.X, newMove.Y, 0.f);
            StoredMove = FVector::ZeroVector;
        }
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
    SetZoomValue(ZoomValue + ZoomDirection * 0.01f);

    ApplyZoomSettings();
}

void UMovementInputHandler::SetMaximumZoomValue(float InMaximumZoomValue)
{
    MaximumZoomValue = FMath::IsFinite(InMaximumZoomValue) ? FMath::Clamp(InMaximumZoomValue, 0.f, 1.f) : 1.f;
    SetZoomValue(ZoomValue);
}

FZoomRigParams UMovementInputHandler::GetZoomRigParams() const
{
    FZoomRigParams Params;
    Params.MinArmLength = MinZoomDistance;
    Params.MaxArmLength = MaxZoomDistance;
    Params.PitchInDeg = ZoomInPitch;
    Params.PitchOutDeg = ZoomOutPitch;
    Params.FOVInDeg = ZoomInFOV;
    Params.FOVOutDeg = ZoomOutFOV;
    if (Owner && Owner->CameraComponent)
    {
        Params.CameraAspectRatio = Owner->CameraComponent->AspectRatio;
    }
    if (Owner && Owner->SpringArm)
    {
        // SocketOffset.X 는 음수(뒤로)라 부호를 뒤집어 "후퇴량"으로 넘긴다
        Params.CameraPullBackCm = -Owner->SpringArm->SocketOffset.X;
    }
    return Params;
}

float UMovementInputHandler::EvaluateZoomKey(float InZoomValue) const
{
    const float Clamped = FMath::Clamp(InZoomValue, 0.f, 1.f);
    return ZoomCurve != nullptr ? ZoomCurve->GetFloatValue(Clamped) : 0.5f;
}

FZoomRigSample UMovementInputHandler::EvaluateZoomRig(float InZoomValue) const
{
    return CameraFramingMath::EvaluateRig(GetZoomRigParams(), EvaluateZoomKey(InZoomValue));
}

void UMovementInputHandler::ApplyZoomSettings()
{
    if (!Owner || !Owner->SpringArm || !Owner->CameraComponent || !Owner->MovementComponent) return;
    if (bConstrainCameraView)
    {
        FVector SafeLocation = Owner->GetActorLocation();
        float SafeZoom = ZoomValue;
        if (TryConstrainCameraPose(SafeLocation, SafeZoom, Owner->HorizontalProjectionOffset))
        {
            ZoomValue = SafeZoom;
        }
    }
    const float lerpKey = EvaluateZoomKey(ZoomValue);
    const FZoomRigSample Rig = CameraFramingMath::EvaluateRig(GetZoomRigParams(), lerpKey);

    Owner->SpringArm->TargetArmLength = Rig.ArmLength;
    Owner->SpringArm->SetRelativeRotation(FRotator(Rig.PitchDeg, 0.f, 0.f));

    const float NewMaxSpeed = FMath::Lerp(8000.f, 48000.f, lerpKey);
    Owner->MovementComponent->MaxSpeed = NewMaxSpeed;
    // 가속/감속을 MaxSpeed에 비례(줌 무관 일정 체감). 배수 = 이즈 정도: 6 ≈ 0.17s 이즈인/아웃. ↓=더 부드럽게, ↑=더 칼각
    Owner->MovementComponent->Acceleration = NewMaxSpeed * 6.f;
    Owner->MovementComponent->Deceleration = NewMaxSpeed * 6.f;

    Owner->CameraComponent->FieldOfView = Rig.HorizontalFOVDeg;
    if (bConstrainCameraView) ApplyMovementBoundary();
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
    if (IsRecruitmentInputBlocked(this)) return;
    Owner->StopCameraTransition();

    const FVector BoundaryOffset = CalculatePlanarBoundaryOffset(Owner->GetActorLocation());
    const float currentDistance = BoundaryOffset.Size();

    // 현재 범위 내에 있을 때만 이동 허용
    // 점진적 저항 계산
    float resistanceFactor = 1.0f;
    float softZoneStart = BoundaryRadius * 0.9f;
    if (!PlanarMovementBounds.bIsValid && currentDistance > softZoneStart)
    {
        FVector moveDirection = (Owner->GetActorForwardVector() * Value[1] + Owner->GetActorRightVector()
            * Value[0]).GetSafeNormal();
        FVector centerDirection = -BoundaryOffset.GetSafeNormal();
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
    if (IsRecruitmentInputBlocked(this)) return;
    Owner->StopCameraTransition();

    ZoomDirection = Value[0];
    UpdateZoom();
}

void UMovementInputHandler::Spin(const FInputActionValue& Value)
{
    if (IsRecruitmentInputBlocked(this)) return;
    Owner->StopCameraTransition();

    // 리그를 돌리면 화면(월드)은 반대로 보여 제스처와 어긋남 → yaw 부호 반전
    Owner->AddActorLocalRotation(FRotator(0, -Value[0], 0));
    if (bConstrainCameraView) ApplyZoomSettings();
}

bool UMovementInputHandler::GetTouchIntersection(FVector& OutIntersection) const
{
    FVector2D ScreenPos;
    return CoordinateUtils::ProjectTouchToGroundPlane(ScreenPos, OutIntersection);
}

void UMovementInputHandler::OnDragStarted(const FInputActionValue& Value)
{
    if (IsRecruitmentInputBlocked(this)) return;
    Owner->StopCameraTransition();

    UE_LOG(LogTemp, Log, TEXT("OnDragStarted"));
    PositionCheck();
}

void UMovementInputHandler::OnDragMove(const FInputActionValue& Value)
{
    if (IsRecruitmentInputBlocked(this)) return;
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
