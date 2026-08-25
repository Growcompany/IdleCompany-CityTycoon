// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/OfficeCameraPawn.h"
#include "Player/OfficeCameraFraming.h"
#include "Player/Components/MovementInputHandler.h"
#include "Player/Components/PlacementHandler.h"
#include "Player/Components/InteractableInputHandler.h"
#include "Player/OfficePlayerController.h"
#include "Components/SphereComponent.h"
#include "Entity/Officeworker/Officeworker.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Office/WorkstationActorBase.h"
#include "Office/DecorationActor.h"
#include "Office/OfficeInterior.h"
#include "GameMode/OfficeGameMode.h"
#include "Manager/SaveLoadManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

namespace
{
// 실제 컴포넌트 갱신 오차가 외부 5% 계약을 넘지 않도록 탐색에는 1% 완충을 둔다.
constexpr float InitialOfficeFramingSafeInset = 0.06f;
}

AOfficeCameraPawn::AOfficeCameraPawn()
	: Super() // PlayerCamera의 생성자 호출
{
	// PlayerCamera의 생성자가 모든 컴포넌트를 생성하므로
	// 여기서는 오피스 전용 설정만 추가
	// 오피스 전용 설정
	if (MovementInputHandler)
	{
		// 줌 범위를 오피스용으로 설정 (50 ~ 5000)
		MovementInputHandler->MinZoomDistance = 20.0f;  // 최소 줌 (가장 가까이)
		MovementInputHandler->MaxZoomDistance = 5000.0f; // 최대 줌 (가장 멀리)

		// 이동 경계를 오피스용으로 작게 제한 (MainMap: 100000, OfficeMap: 1000)
		MovementInputHandler->BoundaryRadius = 3000.0f;

		// 코지 디오라마: 도시맵보다 낮은 각(방 안을 들여다봄) + 넓은 화각(원근 깊이). 에디터 Details에서 미세조정 가능.
		MovementInputHandler->ZoomInPitch = -32.0f;
		MovementInputHandler->ZoomOutPitch = -48.0f;
		MovementInputHandler->ZoomInFOV = 40.0f;
		MovementInputHandler->ZoomOutFOV = 30.0f;

		UE_LOG(LogTemp, Warning, TEXT("  Zoom range set: %.1f ~ %.1f"),
			MovementInputHandler->MinZoomDistance, MovementInputHandler->MaxZoomDistance);
		UE_LOG(LogTemp, Warning, TEXT("  Boundary radius set: %.1f"), MovementInputHandler->BoundaryRadius);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  [ERROR] MovementInputHandler is NULL in constructor!"));
	}

	// InteractableInputHandler는 업무공간 클릭/드래그를 위해 유지 (부모 클래스에서 생성됨)
	// PlacementHandler는 업무공간 배치를 위해 유지 (부모 클래스에서 생성됨)

	// Collision Sphere를 오피스용으로 작게 조정
	if (Collision)
	{
		Collision->SetSphereRadius(72.0f);  // MainMap: 720 → OfficeMap: 72
		UE_LOG(LogTemp, Warning, TEXT("  Collision sphere radius set: 72.0f"));
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Constructor END"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void AOfficeCameraPawn::BeginPlay()
{
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] BeginPlay START"));

	Super::BeginPlay(); // PlayerCamera의 BeginPlay 호출 (드래그 모드 초기화 포함)

	// 초기 위치와 회전 설정
	SetActorLocation(FVector(574.906541f, -538.555362f, 0.0f));
	SetActorRotation(FRotator(0.0f, 150.0f, 0.0f)); // Yaw 150도

	// 초기 줌을 3000으로 설정
	if (MovementInputHandler)
	{
		// 3000이 MinZoomDistance와 MaxZoomDistance 사이에서 어느 위치인지 계산
		float zoomRange = MovementInputHandler->MaxZoomDistance - MovementInputHandler->MinZoomDistance;
		// 넓힌 화각(코지) 보정 — 살짝 당겨 책상 크기 유지
		float targetZoom = 1200.0f;
		float zoomRatio = (targetZoom - MovementInputHandler->MinZoomDistance) / zoomRange;

		MovementInputHandler->SetZoomValue(zoomRatio);
		MovementInputHandler->ApplyZoomSettings();

		UE_LOG(LogTemp, Warning, TEXT("  Initial Zoom set to: %.1f (ZoomValue: %.2f)"), targetZoom, zoomRatio);
		UE_LOG(LogTemp, Warning, TEXT("  Zoom Range: %.1f ~ %.1f"),
			MovementInputHandler->MinZoomDistance, MovementInputHandler->MaxZoomDistance);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  [ERROR] MovementInputHandler is NULL in BeginPlay!"));
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USaveLoadManager* SaveLoadManager = GameInstance->GetSubsystem<USaveLoadManager>())
		{
			SaveLoadManager->OnGameDataLoaded.AddUObject(this, &AOfficeCameraPawn::HandleGameDataLoaded);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("  Initial Position: (400, -300, 0)"));
	UE_LOG(LogTemp, Warning, TEXT("  Initial Rotation: 150 degrees"));
	UE_LOG(LogTemp, Warning, TEXT("  Rotation Range: %.1f ~ %.1f degrees"), MinYawRotation, MaxYawRotation);
	UE_LOG(LogTemp, Warning, TEXT("  Controller: %s"), GetController() ? *GetController()->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] BeginPlay END"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void AOfficeCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USaveLoadManager* SaveLoadManager = GameInstance->GetSubsystem<USaveLoadManager>())
		{
			SaveLoadManager->OnGameDataLoaded.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AOfficeCameraPawn::HandleGameDataLoaded()
{
	ApplyInitialOfficeFraming();
}

bool AOfficeCameraPawn::ApplyInitialOfficeFraming(bool bForce)
{
	if (bInitialOfficeFramingApplied && !bForce)
	{
		return true;
	}

	if (!MovementInputHandler || !SpringArm || !CameraComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing requires valid camera components."));
		return false;
	}

	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing requires a valid world."));
		return false;
	}

	AOfficeInterior* UniqueInterior = nullptr;
	int32 InteriorCount = 0;
	for (TActorIterator<AOfficeInterior> InteriorIt(ActorWorld); InteriorIt; ++InteriorIt)
	{
		++InteriorCount;
		UniqueInterior = InteriorCount == 1 ? *InteriorIt : nullptr;
	}
	if (InteriorCount != 1 || !UniqueInterior)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeCameraPawn] Initial framing expected exactly one OfficeInterior, but found %d."),
			InteriorCount);
		return false;
	}

	const FBox2D FloorBounds = UniqueInterior->GetCurrentFloorBoundsLocal();
	if (!FloorBounds.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing rejected invalid office floor bounds."));
		return false;
	}

	const FBox LocalHeroBounds = FOfficeCameraFraming::MakeHeroBounds(
		FloorBounds,
		UniqueInterior->GetStructuralFloorZLocal(),
		InitialFacadeVisibleDepthCm);
	if (!LocalHeroBounds.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing could not build hero bounds."));
		return false;
	}

	const TStaticArray<FVector, 8> LocalHeroCorners = FOfficeCameraFraming::MakeBoxCorners(LocalHeroBounds);
	TStaticArray<FVector, 8> WorldHeroCorners;
	const FTransform InteriorTransform = UniqueInterior->GetActorTransform();
	for (int32 CornerIndex = 0; CornerIndex < LocalHeroCorners.Num(); ++CornerIndex)
	{
		WorldHeroCorners[CornerIndex] = InteriorTransform.TransformPosition(LocalHeroCorners[CornerIndex]);
	}
	const FVector WorldHeroCenter = InteriorTransform.TransformPosition(LocalHeroBounds.GetCenter());

	APlayerController* OfficePlayerController = Cast<APlayerController>(GetController());
	if (!OfficePlayerController)
	{
		OfficePlayerController = ActorWorld->GetFirstPlayerController();
	}
	if (!OfficePlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing requires a player controller."));
		return false;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	OfficePlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeCameraPawn] Initial framing rejected invalid viewport size %dx%d."),
			ViewportWidth,
			ViewportHeight);
		return false;
	}
	const float ViewportAspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);

	FOfficeCameraFitResult FitResult;
	{
		const float SavedZoomValue = MovementInputHandler->GetZoomValue();
		const bool bSavedCameraLag = SpringArm->bEnableCameraLag;
		const bool bSavedCameraRotationLag = SpringArm->bEnableCameraRotationLag;
		SpringArm->bEnableCameraLag = false;
		SpringArm->bEnableCameraRotationLag = false;

		auto RefreshCameraTransforms = [this]()
		{
			SpringArm->UpdateComponentToWorld();
			SpringArm->TickComponent(0.f, LEVELTICK_All, nullptr);
			CameraComponent->UpdateComponentToWorld();
		};

		ON_SCOPE_EXIT
		{
			MovementInputHandler->SetZoomValue(SavedZoomValue);
			MovementInputHandler->ApplyZoomSettings();
			RefreshCameraTransforms();
			SpringArm->bEnableCameraLag = bSavedCameraLag;
			SpringArm->bEnableCameraRotationLag = bSavedCameraRotationLag;
		};

		const FVector CurrentPawnLocation = GetActorLocation();
		FitResult = FOfficeCameraFraming::FindClosestFit(
			[this, WorldHeroCenter, CurrentPawnLocation, ViewportAspectRatio, &RefreshCameraTransforms](const float ZoomValue)
			{
				MovementInputHandler->SetZoomValue(ZoomValue);
				MovementInputHandler->ApplyZoomSettings();
				RefreshCameraTransforms();

				const FVector ProspectivePawnLocation = CalculatePawnLocationForTarget(WorldHeroCenter);
				FTransform ProspectiveCameraTransform = CameraComponent->GetComponentTransform();
				ProspectiveCameraTransform.AddToTranslation(ProspectivePawnLocation - CurrentPawnLocation);

				FOfficeCameraSample Sample;
				Sample.ZoomValue = ZoomValue;
				Sample.ArmLength = SpringArm->TargetArmLength;
				Sample.CameraTransform = ProspectiveCameraTransform;
				Sample.HorizontalFOVDegrees = CameraComponent->FieldOfView;
				Sample.AspectRatio = ViewportAspectRatio;
				return Sample;
			},
			TConstArrayView<FVector>(WorldHeroCorners.GetData(), WorldHeroCorners.Num()),
			FVector2D(InitialOfficeFramingSafeInset, InitialOfficeFramingSafeInset),
			FVector2D(1.f - InitialOfficeFramingSafeInset, 1.f - InitialOfficeFramingSafeInset),
			32,
			8);
	}

	float SelectedArmLength = FitResult.ArmLength;
	if (!FitResult.bFits)
	{
		SelectedArmLength = 5000.f;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeCameraPawn] Hero bounds do not fit at maximum zoom; using 5000cm fallback."));
	}

	FocusOnLocation(WorldHeroCenter, SelectedArmLength);
	bInitialOfficeFramingApplied = true;
	return true;
}

void AOfficeCameraPawn::ApplyCameraBand(float InPitch, float OutPitch, float InFOV, float OutFOV)
{
	if (!MovementInputHandler) return;
	MovementInputHandler->ZoomInPitch = InPitch;
	MovementInputHandler->ZoomOutPitch = OutPitch;
	MovementInputHandler->ZoomInFOV = InFOV;
	MovementInputHandler->ZoomOutFOV = OutFOV;
	MovementInputHandler->ApplyZoomSettings(); // 현재 줌값 기준 즉시 반영
}

void AOfficeCameraPawn::Tick(float DeltaTime)
{
	// 드래그 중이면 카메라 위치 보간 중지 (Edge Panning과 충돌 방지)
	if (PlacementHandler && PlacementHandler->IsDraggingPlacement())
	{
		bIsTransitioningCamera = false;
	}

	Super::Tick(DeltaTime); // PlayerCamera의 Tick 호출 (MovementInputHandler의 MoveTracking이 경계 처리함)

	// 벽 편집 모드에서 카메라 회전 보간
	UpdateCameraRotation(DeltaTime);

	// 줌 보간 업데이트
	UpdateCameraZoom(DeltaTime);

	// 직원 추적 업데이트
	UpdateEmployeeFollow(DeltaTime);

	// 회전 각도 제한 (벽 편집 모드가 아닐 때만)
	ClampRotation();
}

void AOfficeCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// MovementInputHandler 설정
	if (MovementInputHandler)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Calling MovementInputHandler->SetupPlayerInputComponent..."));
		MovementInputHandler->SetupPlayerInputComponent(PlayerInputComponent);
		UE_LOG(LogTemp, Warning, TEXT("  MovementInputHandler setup complete!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  [ERROR] MovementInputHandler is NULL!"));
	}

	// InteractableInputHandler 설정 (업무공간 클릭/드래그용)
	if (InteractableInputHandler)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Calling InteractableInputHandler->SetupPlayerInputComponent..."));
		InteractableInputHandler->SetupPlayerInputComponent(PlayerInputComponent);
		UE_LOG(LogTemp, Warning, TEXT("  InteractableInputHandler setup complete!"));
	}

	// PlacementHandler 설정 (업무공간 배치용)
	if (PlacementHandler)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Calling PlacementHandler->SetupPlayerInputComponent..."));
		PlacementHandler->SetupPlayerInputComponent(PlayerInputComponent);
		UE_LOG(LogTemp, Warning, TEXT("  PlacementHandler setup complete!"));
	}

	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] SetupPlayerInputComponent END"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void AOfficeCameraPawn::ClampRotation()
{
	// 벽 편집 모드에서는 회전 제한하지 않음
	if (bIsInWallEditMode)
	{
		return;
	}

	FRotator CurrentRotation = GetActorRotation();
	float CurrentYaw = CurrentRotation.Yaw;

	// Yaw를 0 ~ 360 범위로 정규화 (90~180 범위가 연속이므로)
	while (CurrentYaw < 0.0f) CurrentYaw += 360.0f;
	while (CurrentYaw >= 360.0f) CurrentYaw -= 360.0f;

	// 90도 ~ 180도 범위로 제한 (연속 범위)
	float ClampedYaw = FMath::Clamp(CurrentYaw, MinYawRotation, MaxYawRotation);

	// 범위를 벗어났으면 강제로 제한
	if (!FMath::IsNearlyEqual(CurrentYaw, ClampedYaw, 0.1f))
	{
		CurrentRotation.Yaw = ClampedYaw;
		SetActorRotation(CurrentRotation);

		UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] Rotation clamped: %.1f -> %.1f"),
			CurrentYaw, ClampedYaw);
	}
}

// ========== 벽 편집 모드 구현 ==========

void AOfficeCameraPawn::EnterWallEditMode()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] EnterWallEditMode called"));

	bIsInWallEditMode = true;

	// 현재 줌 값 저장 (나중에 복귀할 때 사용)
	if (MovementInputHandler)
	{
		OriginalZoomValue = MovementInputHandler->GetZoomValue();

		// 벽 편집용 줌 계산 (WallEditZoomDistance를 0~1 값으로 변환)
		float ZoomRange = MovementInputHandler->MaxZoomDistance - MovementInputHandler->MinZoomDistance;
		float WallEditZoomRatio = (WallEditZoomDistance - MovementInputHandler->MinZoomDistance) / ZoomRange;
		WallEditZoomRatio = FMath::Clamp(WallEditZoomRatio, 0.f, 1.f);

		// 줌 보간 시작
		TargetZoomValue = WallEditZoomRatio;
		bIsZooming = true;

		UE_LOG(LogTemp, Warning, TEXT("  Original zoom saved: %.2f, Target zoom: %.2f (distance: %.1f)"),
			OriginalZoomValue, TargetZoomValue, WallEditZoomDistance);
	}
}

void AOfficeCameraPawn::FocusOnWall(EWallSide WallSide, const FVector& FocusLocation)
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] FocusOnWall called - WallSide: %s, FocusLocation: %s"),
		WallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"),
		*FocusLocation.ToString());

	// 목표 회전 각도 설정
	FRotator CurrentRotation = GetActorRotation();
	TargetRotation = CurrentRotation;

	// 왼쪽 벽: Yaw 90도 (왼쪽 벽을 정면으로)
	// 오른쪽 벽: Yaw 180도 (오른쪽 벽을 정면으로)
	if (WallSide == EWallSide::Left)
	{
		TargetRotation.Yaw = 90.0f;
		UE_LOG(LogTemp, Warning, TEXT("  Target rotation set to Left wall: Yaw=90.0"));
	}
	else // Right
	{
		TargetRotation.Yaw = 180.0f;
		UE_LOG(LogTemp, Warning, TEXT("  Target rotation set to Right wall: Yaw=180.0"));
	}

	// 회전 보간 시작
	bIsRotating = true;

	// 카메라 이동 위치 계산 (SpringArm, Pitch 고려)
	AOfficeGameMode* GameMode = Cast<AOfficeGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode && SpringArm && CameraComponent && MovementInputHandler)
	{
		AOfficeInterior* OfficeInterior = GameMode->GetOfficeInterior();
		if (OfficeInterior)
		{
			FVector TargetLocation;

			// FocusLocation이 지정되었으면 그 위치 사용, 아니면 벽 중앙
			if (!FocusLocation.IsZero())
			{
				TargetLocation = FocusLocation;
			}
			else
			{
				TargetLocation = OfficeInterior->GetWallLocation(WallSide);
				// 벽 중앙 높이로 조정 (벽 높이의 절반)
				TargetLocation.Z += OfficeInterior->GetWallHeight() * 0.5f;
			}

			// 현재 회전 저장 후 목표 회전으로 일시 변경하여 위치 계산
			FRotator SavedRotation = GetActorRotation();
			SetActorRotation(TargetRotation);

			TargetCameraLocation = CalculatePawnLocationForTarget(TargetLocation);

			// 원래 회전으로 복원 (보간이 처리함)
			SetActorRotation(SavedRotation);

			BeginCameraTransition();

			UE_LOG(LogTemp, Warning, TEXT("  Target Location: %s, Target Camera: %s, TargetYaw: %.1f"),
				*TargetLocation.ToString(), *TargetCameraLocation.ToString(), TargetRotation.Yaw);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("  Current Yaw: %.1f -> Target Yaw: %.1f"),
		CurrentRotation.Yaw, TargetRotation.Yaw);
}

void AOfficeCameraPawn::ExitWallEditMode()
{
	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] ExitWallEditMode called"));

	bIsInWallEditMode = false;

	// 기본 회전 각도(150도)로 복귀 (보간 사용)
	FRotator CurrentRotation = GetActorRotation();
	TargetRotation = CurrentRotation;
	TargetRotation.Yaw = DefaultYawRotation;
	bIsRotating = true;

	// 원래 줌 값으로 복귀
	if (MovementInputHandler)
	{
		TargetZoomValue = OriginalZoomValue;
		bIsZooming = true;

		UE_LOG(LogTemp, Warning, TEXT("  Returning to original zoom: %.2f"), OriginalZoomValue);
	}

	UE_LOG(LogTemp, Warning, TEXT("  Returning to default rotation: Yaw=%.1f"), DefaultYawRotation);
}

void AOfficeCameraPawn::UpdateCameraRotation(float DeltaTime)
{
	if (!bIsRotating)
	{
		return;
	}

	// 현재 회전에서 목표 회전으로 부드럽게 보간
	FRotator CurrentRotation = GetActorRotation();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationInterpSpeed);

	SetActorRotation(NewRotation);

	// 목표에 도달했으면 보간 중지
	if (CurrentRotation.Equals(TargetRotation, 0.5f))
	{
		bIsRotating = false;
		SetActorRotation(TargetRotation); // 정확한 각도로 설정

		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Rotation interpolation completed: Yaw=%.1f"),
			TargetRotation.Yaw);
	}
}

void AOfficeCameraPawn::UpdateCameraZoom(float DeltaTime)
{
	if (!bIsZooming || bIsTransitioningCamera || !MovementInputHandler)
	{
		return;
	}

	// 현재 줌에서 목표 줌으로 부드럽게 보간
	float CurrentZoom = MovementInputHandler->GetZoomValue();
	float NewZoom = FMath::FInterpTo(CurrentZoom, TargetZoomValue, DeltaTime, RotationInterpSpeed);

	MovementInputHandler->SetZoomValue(NewZoom);
	MovementInputHandler->ApplyZoomSettings();

	// 목표에 도달했으면 보간 중지
	if (FMath::IsNearlyEqual(CurrentZoom, TargetZoomValue, 0.01f))
	{
		bIsZooming = false;
		MovementInputHandler->SetZoomValue(TargetZoomValue);
		MovementInputHandler->ApplyZoomSettings();

		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Zoom interpolation completed: %.2f"), TargetZoomValue);
	}
}

// ========== 직원 포커스 구현 ==========

FVector AOfficeCameraPawn::CalculatePawnLocationForTarget(const FVector& TargetLocation) const
{
	if (!SpringArm || !CameraComponent)
	{
		return GetActorLocation();
	}

	// 현재 Pawn 위치 및 카메라 위치
	FVector CurrentPawnLocation = GetActorLocation();
	FVector CameraWorldLocation = CameraComponent->GetComponentLocation();

	// Pawn의 Forward 방향 (수평)
	FRotator PawnRotation = GetActorRotation();
	FVector PawnForward = FRotationMatrix(FRotator(0.f, PawnRotation.Yaw, 0.f)).GetUnitAxis(EAxis::X);

	// SpringArm Pitch 각도
	FRotator SpringArmRotation = SpringArm->GetRelativeRotation();
	float AbsPitchRad = FMath::DegreesToRadians(FMath::Abs(SpringArmRotation.Pitch));

	// 카메라와 타겟의 높이 차이
	float HeightDiff = CameraWorldLocation.Z - TargetLocation.Z;

	// 카메라가 타겟 높이를 볼 때의 수평 거리
	float HorizontalDistToLookPoint = HeightDiff / FMath::Tan(AbsPitchRad);

	// 카메라가 현재 보고 있는 지점 (타겟 높이에서)
	FVector CurrentLookPoint = CameraWorldLocation + PawnForward * HorizontalDistToLookPoint;
	CurrentLookPoint.Z = TargetLocation.Z;

	// 타겟이 LookPoint에 오도록 Pawn 이동량 계산
	FVector RequiredPawnOffset = TargetLocation - CurrentLookPoint;

	return CurrentPawnLocation + RequiredPawnOffset;
}

void AOfficeCameraPawn::FocusOnEmployee(AOfficeworker* Employee, float DesiredDistance, bool bEnableFollow)
{
	if (!Employee || !SpringArm || !MovementInputHandler || !CameraComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] FocusOnEmployee: Invalid parameters"));
		return;
	}

	// 추적 상태 설정
	FollowingEmployee = Employee;
	bIsFollowingEmployee = bEnableFollow;
	FollowDistance = DesiredDistance;

	// 직원의 중심 위치 (GetCenterLocation 사용)
	FVector EmployeeLocation = Employee->GetCenterLocation();

	// 현재 ZoomValue 저장
	float SavedZoomValue = MovementInputHandler->GetZoomValue();

	// DesiredDistance에 해당하는 ZoomValue 계산 (이진 탐색)
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

	// 목표 ZoomValue 적용 상태에서 Pawn 위치 계산
	MovementInputHandler->SetZoomValue(TargetZoomValue);
	MovementInputHandler->ApplyZoomSettings();

	TargetCameraLocation = CalculatePawnLocationForTarget(EmployeeLocation);

	// 원래 ZoomValue로 복원 (Tick에서 보간 시작)
	MovementInputHandler->SetZoomValue(SavedZoomValue);
	MovementInputHandler->ApplyZoomSettings();

	// 부드러운 전환 시작
	BeginCameraTransition();

	UE_LOG(LogTemp, Log, TEXT("[FocusOnEmployee] %s at (%.1f, %.1f, %.1f), TargetZoom=%.3f"),
		*Employee->GetName(), EmployeeLocation.X, EmployeeLocation.Y, EmployeeLocation.Z, TargetZoomValue);
}

void AOfficeCameraPawn::StopFollowingEmployee()
{
	bIsFollowingEmployee = false;
	FollowingEmployee = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] Stopped following employee"));
}

void AOfficeCameraPawn::FocusOnLocation(const FVector& Location, float DesiredDistance)
{
	if (!SpringArm || !MovementInputHandler || !CameraComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] FocusOnLocation: Invalid components"));
		return;
	}

	// 직원 추적 중이면 중지
	if (bIsFollowingEmployee)
	{
		StopFollowingEmployee();
	}

	// 현재 ZoomValue 저장
	float SavedZoomValue = MovementInputHandler->GetZoomValue();

	// DesiredDistance에 해당하는 ZoomValue 계산 (이진 탐색)
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

	// 목표 ZoomValue 적용 상태에서 Pawn 위치 계산
	MovementInputHandler->SetZoomValue(TargetZoomValue);
	MovementInputHandler->ApplyZoomSettings();

	TargetCameraLocation = CalculatePawnLocationForTarget(Location);

	// 원래 ZoomValue로 복원 (Tick에서 보간 시작)
	MovementInputHandler->SetZoomValue(SavedZoomValue);
	MovementInputHandler->ApplyZoomSettings();

	// 위치와 줌의 공통 부모 전환 시작
	BeginCameraTransition();

	UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] FocusOnLocation: %s, Distance: %.1f, TargetZoom: %.3f"),
		*Location.ToString(), DesiredDistance, TargetZoomValue);
}

void AOfficeCameraPawn::UpdateEmployeeFollow(float DeltaTime)
{
	if (!bIsFollowingEmployee || !FollowingEmployee || !CameraComponent || !SpringArm)
	{
		return;
	}

	// 직원이 유효하지 않으면 추적 중지
	if (!IsValid(FollowingEmployee))
	{
		StopFollowingEmployee();
		return;
	}

	// 직원의 중심 위치 (GetCenterLocation 사용)
	FVector EmployeeLocation = FollowingEmployee->GetCenterLocation();

	// 공통 함수로 목표 위치 계산
	FVector DesiredLocation = CalculatePawnLocationForTarget(EmployeeLocation);

	// 부드럽게 카메라 이동
	FVector CurrentLocation = GetActorLocation();
	FVector NewLocation = FMath::VInterpTo(CurrentLocation, DesiredLocation, DeltaTime, 5.0f);
	SetActorLocation(NewLocation);
}

// ========== 업무공간 배치 구현 ==========

void AOfficeCameraPawn::BeginWorkstationPlacement(const FWorkstationCardTable& WorkstationInfo)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] BeginWorkstationPlacement: %s"), *WorkstationInfo.DisplayName.ToString());

	// 업무공간 정보 저장
	CurrentWorkstationInfo = WorkstationInfo;
	bIsInWorkstationPlacementMode = true;

	// OfficePlayerController에서 배치 모드로 전환
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetController());
	if (PC)
	{
		PC->GoToBuildPlaceMode();
	}

	// PlacementHandler에 업무공간 블루프린트 클래스 설정
	if (PlacementHandler)
	{
		// BlueprintClass 로드
		UClass* WorkstationClass = WorkstationInfo.BlueprintClass.LoadSynchronous();
		if (WorkstationClass)
		{
			PlacementHandler->SetWorkstationPlacementTarget(WorkstationClass, WorkstationInfo.RowName);
			UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] PlacementHandler workstation target set: %s (TypeID: %s)"), *WorkstationClass->GetName(), *WorkstationInfo.RowName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeCameraPawn] Failed to load BlueprintClass for: %s"), *WorkstationInfo.RowName.ToString());
		}
	}
}

void AOfficeCameraPawn::EndWorkstationPlacement()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] EndWorkstationPlacement"));

	bIsInWorkstationPlacementMode = false;
	CurrentWorkstationInfo = FWorkstationCardTable();

	// OfficePlayerController를 UI 모드로 전환
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetController());
	if (PC)
	{
		PC->SetGameMode();
	}

	// PlacementHandler 정리
	if (PlacementHandler)
	{
		PlacementHandler->ReleaseWorkstationPlacementTarget();
		PlacementHandler->ReleasePlacementIMC();
	}
}

// ========== 장식품 배치 ==========

void AOfficeCameraPawn::BeginDecorationPlacement(const FDecorationCardTable& DecorationInfo)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] BeginDecorationPlacement: %s"), *DecorationInfo.Name.ToString());

	if (DecorationInfo.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] DecorationInfo.RowName is None!"));
		return;
	}

	// 장식품 정보 저장
	CurrentDecorationInfo = DecorationInfo;
	bIsInDecorationPlacementMode = true;

	// OfficePlayerController를 BuildPlace 모드로 전환
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetController());
	if (PC)
	{
		PC->GoToBuildPlaceMode();
	}

	// PlacementHandler로 배치 시작
	if (PlacementHandler)
	{
		PlacementHandler->SetDecorationPlacementTarget(DecorationInfo);
		UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] PlacementHandler decoration target set: %s"), *DecorationInfo.RowName.ToString());
	}
}

void AOfficeCameraPawn::BeginWallDecorationPlacement(const FDecorationCardTable& DecorationInfo, EWallSide WallSide)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] BeginWallDecorationPlacement: %s on %s wall"),
		*DecorationInfo.Name.ToString(),
		WallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"));

	if (DecorationInfo.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] DecorationInfo.RowName is None!"));
		return;
	}

	// 장식품 정보 저장
	CurrentDecorationInfo = DecorationInfo;
	bIsInDecorationPlacementMode = true;

	// OfficePlayerController를 BuildPlace 모드로 전환
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetController());
	if (PC)
	{
		PC->GoToBuildPlaceMode();
	}

	// PlacementHandler로 벽 장식품 배치 시작
	if (PlacementHandler)
	{
		PlacementHandler->SetWallDecorationPlacementTarget(DecorationInfo, WallSide);
		UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] PlacementHandler wall decoration target set: %s"),
			*DecorationInfo.RowName.ToString());
	}
}

void AOfficeCameraPawn::EndDecorationPlacement()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeCameraPawn] EndDecorationPlacement"));

	// 벽 편집 모드였다면 카메라 복원
	if (bIsInWallEditMode)
	{
		ExitWallEditMode();
	}

	bIsInDecorationPlacementMode = false;
	CurrentDecorationInfo = FDecorationCardTable();

	// OfficePlayerController를 Game 모드로 전환
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetController());
	if (PC)
	{
		PC->SetGameMode();
	}

	// PlacementHandler 정리 (Grid, Wall 둘 다 처리)
	if (PlacementHandler)
	{
		if (PlacementHandler->IsWallDecorationMode())
		{
			PlacementHandler->ReleaseWallDecorationPlacementTarget();
		}
		else if (PlacementHandler->IsDecorationMode())
		{
			PlacementHandler->ReleaseDecorationPlacementTarget();
		}
		PlacementHandler->ReleasePlacementIMC();
	}
}
