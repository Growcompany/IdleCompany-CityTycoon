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
#include "GameFramework/FloatingPawnMovement.h"
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
#include "Engine/LocalPlayer.h"
#include "SceneView.h"

namespace
{
// 실제 컴포넌트 갱신 오차가 외부 5% 계약을 넘지 않도록 탐색에는 1% 완충을 둔다.
constexpr float InitialOfficeFramingSafeInset = 0.06f;
constexpr float InitialFallbackArmLengthCm = 1200.f;
// 줌 커브의 거리 매핑을 보존하는 리그 보간 범위.
constexpr float OfficeMaximumZoomDistanceCm = 12500.f;
constexpr float OfficeAllowedZoomOutArmLengthCm = 5200.f;
constexpr float OfficeLocalZoomOutScale = 1.35f;
constexpr float OfficeZoomOutBaselineArmLengthCm = 1438.554f;
// 바닥 중심에 대한 오프셋. 앞쪽 바닥 모서리·파사드는 화면 아래로 잘릴 수 있다.
// 전역 const FVector 대신 constexpr 성분: Live Coding은 전역 초기값 변경을 반영하지 않아 CamPose 재베이크가 묻힌다.
constexpr float CalibratedInitialPawnOffsetX = 181.892f;
constexpr float CalibratedInitialPawnOffsetY = -42.777f;
constexpr float CalibratedInitialArmLengthCm = 1000.f;
constexpr float CalibratedInitialBaselineFloorSizeCm = 800.f;

void ConfigureOfficeZoomLimit(UMovementInputHandler* Handler, const float MaximumArmLength = OfficeAllowedZoomOutArmLengthCm)
{
	if (!Handler) { return; }
	// 이분법의 양방향 오차보다 작은 여유를 두어 실제 팔길이가 상한을 넘지 않게 한다.
	const float MaximumZoomValue = CameraFramingMath::SolveZoomForArmLength(
		Handler->GetZoomRigParams(), [Handler](float Value) { return Handler->EvaluateZoomKey(Value); },
		MaximumArmLength - 0.01f, 24, 0.001f);
	Handler->SetMaximumZoomValue(MaximumZoomValue);
}

FVector GetOfficeWallCornerWorld(const AOfficeInterior& Interior)
{
	FVector LeftMin, LeftMax, RightMin, RightMax;
	Interior.GetWallPlacementBoundsLocal(EWallSide::Left, LeftMin, LeftMax);
	Interior.GetWallPlacementBoundsLocal(EWallSide::Right, RightMin, RightMax);
	return Interior.GetActorTransform().TransformPosition(
		FVector(RightMin.X, LeftMin.Y, Interior.GetStructuralFloorZLocal()));
}

// 승인 구도가 화면비를 바꿔도 잃으면 안 되는 점 = 바닥 중심 + 카메라에서 먼 바닥 모서리 2개.
// 잘려 나가는 앞쪽 모서리·파사드(hero box)는 이 판정에 넣지 않는다.
TArray<FVector, TInlineAllocator<3>> MakeRoomAnchorPoints(
	const FBox2D& FloorBounds,
	const float StructuralFloorZ,
	const FTransform& InteriorTransform,
	const FVector& CameraLocation)
{
	TArray<FVector, TInlineAllocator<4>> Corners;
	Corners.Add(InteriorTransform.TransformPosition(FVector(FloorBounds.Min.X, FloorBounds.Min.Y, StructuralFloorZ)));
	Corners.Add(InteriorTransform.TransformPosition(FVector(FloorBounds.Max.X, FloorBounds.Min.Y, StructuralFloorZ)));
	Corners.Add(InteriorTransform.TransformPosition(FVector(FloorBounds.Min.X, FloorBounds.Max.Y, StructuralFloorZ)));
	Corners.Add(InteriorTransform.TransformPosition(FVector(FloorBounds.Max.X, FloorBounds.Max.Y, StructuralFloorZ)));
	Corners.Sort([&CameraLocation](const FVector& A, const FVector& B)
	{
		return FVector::DistSquared(A, CameraLocation) > FVector::DistSquared(B, CameraLocation);
	});

	const FVector2D FloorCenter = FloorBounds.GetCenter();
	TArray<FVector, TInlineAllocator<3>> Anchors;
	Anchors.Add(InteriorTransform.TransformPosition(FVector(FloorCenter.X, FloorCenter.Y, StructuralFloorZ)));
	Anchors.Add(Corners[0]);
	Anchors.Add(Corners[1]);
	return Anchors;
}
}

FVector2D AOfficeCameraPawn::GetSupportedYawRange()
{
	const AOfficeCameraPawn* Defaults = GetDefault<AOfficeCameraPawn>();
	return FVector2D(Defaults->MinYawRotation, Defaults->MaxYawRotation);
}

float AOfficeCameraPawn::GetDefaultViewYaw()
{
	return GetDefault<AOfficeCameraPawn>()->DefaultYawRotation;
}

bool AOfficeCameraPawn::TryGetInitialPhotoViewOffset(
	const FVector2D FloorSizeCm,
	const float PresentationScale,
	FVector& OutCameraOffset)
{
	if (!FMath::IsFinite(FloorSizeCm.X) || !FMath::IsFinite(FloorSizeCm.Y)
		|| FloorSizeCm.X <= 0.0 || FloorSizeCm.Y <= 0.0
		|| !FMath::IsFinite(PresentationScale) || PresentationScale <= 0.f)
	{
		return false;
	}

	const AOfficeCameraPawn* Defaults = GetDefault<AOfficeCameraPawn>();
	const UMovementInputHandler* DefaultHandler = Defaults->MovementInputHandler.Get();
	const USpringArmComponent* DefaultArm = Defaults->SpringArm.Get();
	const UCameraComponent* DefaultCamera = Defaults->CameraComponent.Get();
	if (!DefaultHandler || !DefaultArm || !DefaultCamera)
	{
		return false;
	}

	const double FloorDiagonalCm = FloorSizeCm.Size();
	const double BaselineDiagonalCm = static_cast<double>(CalibratedInitialBaselineFloorSizeCm) * FMath::Sqrt(2.0);
	const double FootprintScale = FMath::Max(1.0, FloorDiagonalCm / BaselineDiagonalCm) * PresentationScale;
	const double DesiredArmLength = static_cast<double>(CalibratedInitialArmLengthCm) * FootprintScale;
	if (!FMath::IsFinite(DesiredArmLength)
		|| DesiredArmLength > static_cast<double>(TNumericLimits<float>::Max()))
	{
		return false;
	}

	const float MaximumZoomValue = DefaultHandler->GetMaximumZoomValue();
	const float MaximumArmLength = DefaultHandler->EvaluateZoomRig(MaximumZoomValue).ArmLength;
	const float TargetArmLength = FMath::Min(static_cast<float>(DesiredArmLength), MaximumArmLength);
	const float ZoomValue = FMath::Min(
		CameraFramingMath::SolveZoomForArmLength(
			DefaultHandler->GetZoomRigParams(),
			[DefaultHandler](const float Value) { return DefaultHandler->EvaluateZoomKey(Value); },
			TargetArmLength,
			24,
			0.001f),
		MaximumZoomValue);

	const FZoomRigSample Rig = DefaultHandler->EvaluateZoomRig(ZoomValue);
	const FQuat PawnRotation = FRotator(0.f, GetDefaultViewYaw(), 0.f).Quaternion();
	const FQuat RigRotation = PawnRotation * FRotator(Rig.PitchDeg, 0.f, 0.f).Quaternion();
	const FVector PawnOffset(CalibratedInitialPawnOffsetX * PresentationScale,
		CalibratedInitialPawnOffsetY * PresentationScale, 0.f);
	const FVector ArmOrigin = PawnOffset + PawnRotation.RotateVector(DefaultArm->GetRelativeLocation());
	const FVector SocketLocation = ArmOrigin + DefaultArm->TargetOffset
		+ RigRotation.RotateVector(DefaultArm->SocketOffset - FVector(Rig.ArmLength, 0.f, 0.f));
	const FVector CameraOffset = (DefaultCamera->GetRelativeTransform()
		* FTransform(RigRotation, SocketLocation)).GetLocation();
	if (CameraOffset.ContainsNaN())
	{
		return false;
	}
	OutCameraOffset = CameraOffset;
	return true;
}

AOfficeCameraPawn::AOfficeCameraPawn()
	: Super() // PlayerCamera의 생성자 호출
{
	// PlayerCamera의 생성자가 모든 컴포넌트를 생성하므로
	// 여기서는 오피스 전용 설정만 추가
	// 오피스 전용 설정
	if (MovementInputHandler)
	{
		// 줌 범위를 오피스용으로 설정
		MovementInputHandler->MinZoomDistance = 20.0f;  // 최소 줌 (가장 가까이)
		MovementInputHandler->MaxZoomDistance = OfficeMaximumZoomDistanceCm;

		// 이동 경계를 오피스용으로 작게 제한 (MainMap: 100000, OfficeMap: 1000)
		MovementInputHandler->BoundaryRadius = 3000.0f;

		// 줌할 때 방의 비율이 흔들리지 않도록 각도와 화각은 고정하고 거리만 바꾼다.
		MovementInputHandler->ZoomInPitch = -35.0f;
		MovementInputHandler->ZoomOutPitch = -35.0f;
		MovementInputHandler->ZoomInFOV = 35.0f;
		MovementInputHandler->ZoomOutFOV = 35.0f;
		ConfigureOfficeZoomLimit(MovementInputHandler);

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
	ConfigureOfficeZoomLimit(MovementInputHandler);

	// 초기 위치와 회전 설정
	SetActorLocation(FVector(574.906541f, -538.555362f, 0.0f));
	SetActorRotation(FRotator(0.f, DefaultYawRotation, 0.f));

	// 세이브 복원 전 잠깐 쓰이는 기본 리그도 실제 줌 커브를 따라 1200cm로 맞춘다.
	if (MovementInputHandler)
	{
		float InitialZoomValue = 0.f;
		float ClampedInitialArmLength = 0.f;
		if (ResolveZoomValueForArmLength(
			InitialFallbackArmLengthCm,
			InitialZoomValue,
			ClampedInitialArmLength))
		{
			const bool bSavedCameraLag = SpringArm->bEnableCameraLag;
			const bool bSavedCameraRotationLag = SpringArm->bEnableCameraRotationLag;
			SpringArm->bEnableCameraLag = false;
			SpringArm->bEnableCameraRotationLag = false;

			MovementInputHandler->SetZoomValue(InitialZoomValue);
			MovementInputHandler->ApplyZoomSettings();
			RefreshCameraRigTransforms();

			SpringArm->bEnableCameraLag = bSavedCameraLag;
			SpringArm->bEnableCameraRotationLag = bSavedCameraRotationLag;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("  Initial Zoom set to: %.1f (ZoomValue: %.3f, ActualArm: %.1f)"),
				ClampedInitialArmLength,
				InitialZoomValue,
				SpringArm->TargetArmLength);
		}
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

	int32 InteriorCount = 0;
	if (AOfficeInterior* UniqueInterior = FindUniqueOfficeInterior(InteriorCount))
	{
		BindOfficeBoundaryInterior(UniqueInterior);
	}

	const FVector InitialActorLocation = GetActorLocation();
	UE_LOG(LogTemp, Warning, TEXT("  Initial Position: %s"), *InitialActorLocation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  Initial Rotation: %.1f degrees"), DefaultYawRotation);
	UE_LOG(LogTemp, Warning, TEXT("  Rotation Range: %.1f ~ %.1f degrees"), MinYawRotation, MaxYawRotation);
	UE_LOG(LogTemp, Warning, TEXT("  Controller: %s"), GetController() ? *GetController()->GetName() : TEXT("NULL"));
	UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] BeginPlay END"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

void AOfficeCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndOfficePlacementEdit();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USaveLoadManager* SaveLoadManager = GameInstance->GetSubsystem<USaveLoadManager>())
		{
			SaveLoadManager->OnGameDataLoaded.RemoveAll(this);
		}
	}

	if (AOfficeInterior* Interior = BoundOfficeInterior.Get())
	{
		Interior->OnFootprintChanged.RemoveAll(this);
	}
	BoundOfficeInterior.Reset();

	Super::EndPlay(EndPlayReason);
}

void AOfficeCameraPawn::HandleGameDataLoaded()
{
	bInitialOfficeFramingPending = true;
	InitialOfficeFramingFailureCount = 0;
	TryApplyPendingInitialOfficeFraming();
}

bool AOfficeCameraPawn::TryBuildCameraPoseLog(
	const AOfficeInterior* Interior,
	const FIntPoint ViewportSize,
	FString& OutLine) const
{
	if (!IsValid(Interior)
		|| !IsValid(MovementInputHandler.Get())
		|| !IsValid(SpringArm.Get())
		|| !IsValid(CameraComponent.Get())
		|| ViewportSize.X <= 0
		|| ViewportSize.Y <= 0)
	{
		return false;
	}

	const FBox2D FloorBounds = Interior->GetCurrentFloorBoundsLocal();
	const float StructuralFloorZLocal = Interior->GetStructuralFloorZLocal();
	if (!FloorBounds.bIsValid
		|| !FMath::IsFinite(FloorBounds.Min.X)
		|| !FMath::IsFinite(FloorBounds.Min.Y)
		|| !FMath::IsFinite(FloorBounds.Max.X)
		|| !FMath::IsFinite(FloorBounds.Max.Y)
		|| !FMath::IsFinite(StructuralFloorZLocal))
	{
		return false;
	}

	const FIntPoint TileCount = Interior->GetTileCount();
	const FVector2D LocalFloorCenter = FloorBounds.GetCenter();
	const FVector FloorCenterWorld = Interior->GetActorTransform().TransformPosition(
		FVector(LocalFloorCenter.X, LocalFloorCenter.Y, StructuralFloorZLocal));
	const FVector PawnWorldLocation = GetActorLocation();
	const FVector PawnRelativeToFloorCenter = PawnWorldLocation - FloorCenterWorld;
	const FRotator PawnWorldRotation = GetActorRotation();
	const float ZoomValue = MovementInputHandler->GetZoomValue();
	const float ActualArmLength = SpringArm->TargetArmLength;
	const FRotator ArmRotation = SpringArm->GetRelativeRotation();
	const FVector SocketOffset = SpringArm->SocketOffset;
	const FVector CameraWorldLocation = CameraComponent->GetComponentLocation();
	const FRotator CameraWorldRotation = CameraComponent->GetComponentRotation();
	const float HorizontalFieldOfView = CameraComponent->FieldOfView;

	if (FloorCenterWorld.ContainsNaN()
		|| PawnWorldLocation.ContainsNaN()
		|| PawnRelativeToFloorCenter.ContainsNaN()
		|| PawnWorldRotation.ContainsNaN()
		|| !FMath::IsFinite(ZoomValue)
		|| !FMath::IsFinite(ActualArmLength)
		|| ArmRotation.ContainsNaN()
		|| SocketOffset.ContainsNaN()
		|| CameraWorldLocation.ContainsNaN()
		|| CameraWorldRotation.ContainsNaN()
		|| !FMath::IsFinite(HorizontalFieldOfView))
	{
		return false;
	}

	const FString PoseLine = FString::Printf(
		TEXT("[OfficeCameraCalibration] Tile=%dx%d Viewport=%dx%d ")
		TEXT("FloorCenterWorld=(%.3f,%.3f,%.3f) ")
		TEXT("PawnRelativeToFloorCenter=(%.3f,%.3f,%.3f) ")
		TEXT("PawnWorldLocation=(%.3f,%.3f,%.3f) PawnYaw=%.3f ")
		TEXT("PawnWorldRotation=(%.3f,%.3f,%.3f) ZoomValue=%.6f ")
		TEXT("ActualArmLength=%.3f ArmPitch=%.3f ArmRotation=(%.3f,%.3f,%.3f) ")
		TEXT("SocketOffset=(%.3f,%.3f,%.3f) CameraWorldLocation=(%.3f,%.3f,%.3f) ")
		TEXT("CameraWorldRotation=(%.3f,%.3f,%.3f) HFOV=%.3f"),
		TileCount.X,
		TileCount.Y,
		ViewportSize.X,
		ViewportSize.Y,
		FloorCenterWorld.X,
		FloorCenterWorld.Y,
		FloorCenterWorld.Z,
		PawnRelativeToFloorCenter.X,
		PawnRelativeToFloorCenter.Y,
		PawnRelativeToFloorCenter.Z,
		PawnWorldLocation.X,
		PawnWorldLocation.Y,
		PawnWorldLocation.Z,
		PawnWorldRotation.Yaw,
		PawnWorldRotation.Pitch,
		PawnWorldRotation.Yaw,
		PawnWorldRotation.Roll,
		ZoomValue,
		ActualArmLength,
		ArmRotation.Pitch,
		ArmRotation.Pitch,
		ArmRotation.Yaw,
		ArmRotation.Roll,
		SocketOffset.X,
		SocketOffset.Y,
		SocketOffset.Z,
		CameraWorldLocation.X,
		CameraWorldLocation.Y,
		CameraWorldLocation.Z,
		CameraWorldRotation.Pitch,
		CameraWorldRotation.Yaw,
		CameraWorldRotation.Roll,
		HorizontalFieldOfView);
	OutLine = PoseLine;
	return true;
}

bool AOfficeCameraPawn::TryGetInitialFramingViewport(
	float& OutAspectRatio,
	int32& OutViewportWidth,
	int32& OutViewportHeight) const
{
	OutAspectRatio = 0.f;
	OutViewportWidth = 0;
	OutViewportHeight = 0;

	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		return false;
	}

	APlayerController* OfficePlayerController = Cast<APlayerController>(GetController());
	if (!OfficePlayerController)
	{
		OfficePlayerController = ActorWorld->GetFirstPlayerController();
	}
	if (!OfficePlayerController)
	{
		return false;
	}

	OfficePlayerController->GetViewportSize(OutViewportWidth, OutViewportHeight);
	if (OutViewportWidth <= 0 || OutViewportHeight <= 0)
	{
		return false;
	}

	OutAspectRatio = static_cast<float>(OutViewportWidth) / static_cast<float>(OutViewportHeight);
	return FMath::IsFinite(OutAspectRatio) && OutAspectRatio > 0.f;
}

void AOfficeCameraPawn::TryApplyPendingInitialOfficeFraming()
{
	if (!bInitialOfficeFramingPending)
	{
		return;
	}
	if (bInitialOfficeFramingApplied)
	{
		bInitialOfficeFramingPending = false;
		return;
	}

	float ViewportAspectRatio = 0.f;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	if (!TryGetInitialFramingViewport(ViewportAspectRatio, ViewportWidth, ViewportHeight))
	{
		return;
	}

	if (ApplyInitialOfficeFraming())
	{
		bInitialOfficeFramingPending = false;
		InitialOfficeFramingFailureCount = 0;
		return;
	}

	// 뷰포트는 유효한데 적용이 실패했다 = 대개 영구 실패(OfficeInterior 0개/2개 이상, 바닥 바운즈 무효).
	// 재시도 예산을 넘기면 포기한다 — 안 그러면 매 프레임 TActorIterator 월드 순회 + 경고가 계속된다.
	if (++InitialOfficeFramingFailureCount >= MaxInitialOfficeFramingFailures)
	{
		bInitialOfficeFramingPending = false;
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[OfficeCameraPawn] Initial framing gave up after %d attempts; camera keeps its default pose."),
			InitialOfficeFramingFailureCount);
	}
}

bool AOfficeCameraPawn::ResolveZoomValueForArmLength(
	float DesiredArmLength,
	float& OutZoomValue,
	float& OutClampedArmLength)
{
	if (!MovementInputHandler || !SpringArm)
	{
		return false;
	}

	const auto SampleArmLength = [this](const float ZoomValue)
	{
		MovementInputHandler->SetZoomValue(ZoomValue);
		MovementInputHandler->ApplyZoomSettings();
		return SpringArm->TargetArmLength;
	};

	const float MinimumArmLength = SampleArmLength(0.f);
	const float MaximumZoomValue = MovementInputHandler->GetMaximumZoomValue();
	const float MaximumArmLength = SampleArmLength(MaximumZoomValue);
	const float FiniteDesiredArmLength = FMath::IsFinite(DesiredArmLength)
		? DesiredArmLength
		: MinimumArmLength;
	OutClampedArmLength = FMath::Clamp(
		FiniteDesiredArmLength,
		MinimumArmLength,
		MaximumArmLength);

	OutZoomValue = 0.f;
	if (OutClampedArmLength >= MaximumArmLength)
	{
		OutZoomValue = MaximumZoomValue;
		return true;
	}
	if (OutClampedArmLength <= MinimumArmLength)
	{
		return true;
	}

	float LowerZoomValue = 0.f;
	float UpperZoomValue = MaximumZoomValue;
	for (int32 Iteration = 0; Iteration < 24; ++Iteration)
	{
		const float CandidateZoomValue = (LowerZoomValue + UpperZoomValue) * 0.5f;
		const float CandidateArmLength = SampleArmLength(CandidateZoomValue);
		if (CandidateArmLength <= OutClampedArmLength)
		{
			LowerZoomValue = CandidateZoomValue;
			OutZoomValue = CandidateZoomValue;
		}
		else
		{
			UpperZoomValue = CandidateZoomValue;
		}
	}

	return true;
}

bool AOfficeCameraPawn::ResolveInitialZoomValueForArmRange(
	float DesiredArmLength,
	float MinimumArmLength,
	float MaximumArmLength,
	float& OutZoomValue,
	float& OutRepresentedArmLength)
{
	if (!MovementInputHandler || !SpringArm
		|| !FMath::IsFinite(DesiredArmLength)
		|| !FMath::IsFinite(MinimumArmLength)
		|| !FMath::IsFinite(MaximumArmLength)
		|| DesiredArmLength <= 0.f
		|| MinimumArmLength <= 0.f
		|| MaximumArmLength <= 0.f
		|| MinimumArmLength > MaximumArmLength)
	{
		return false;
	}

	const float SavedZoomValue = MovementInputHandler->GetZoomValue();
	ON_SCOPE_EXIT
	{
		MovementInputHandler->SetZoomValue(SavedZoomValue);
		MovementInputHandler->ApplyZoomSettings();
	};

	float LowerZoomValue = 0.f;
	float ClampedDesiredArmLength = 0.f;
	if (!ResolveZoomValueForArmLength(
		FMath::Clamp(DesiredArmLength, MinimumArmLength, MaximumArmLength),
		LowerZoomValue,
		ClampedDesiredArmLength))
	{
		return false;
	}
	if (!FMath::IsFinite(ClampedDesiredArmLength)
		|| ClampedDesiredArmLength < MinimumArmLength
		|| ClampedDesiredArmLength > MaximumArmLength)
	{
		return false;
	}

	const auto SampleArmLength = [this](const float ZoomValue)
	{
		MovementInputHandler->SetZoomValue(ZoomValue);
		MovementInputHandler->ApplyZoomSettings();
		return SpringArm->TargetArmLength;
	};

	const float LowerArmLength = SampleArmLength(LowerZoomValue);
	if (!FMath::IsFinite(LowerArmLength))
	{
		return false;
	}
	if (LowerArmLength >= MinimumArmLength && LowerArmLength <= MaximumArmLength)
	{
		OutZoomValue = LowerZoomValue;
		OutRepresentedArmLength = LowerArmLength;
		return true;
	}
	if (LowerArmLength > MaximumArmLength)
	{
		return false;
	}

	float UpperZoomValue = MovementInputHandler->GetMaximumZoomValue();
	float UpperArmLength = SampleArmLength(UpperZoomValue);
	if (!FMath::IsFinite(UpperArmLength) || UpperArmLength < MinimumArmLength)
	{
		return false;
	}

	for (int32 Iteration = 0; Iteration < 24; ++Iteration)
	{
		const float CandidateZoomValue = (LowerZoomValue + UpperZoomValue) * 0.5f;
		const float CandidateArmLength = SampleArmLength(CandidateZoomValue);
		if (!FMath::IsFinite(CandidateArmLength))
		{
			return false;
		}
		if (CandidateArmLength < MinimumArmLength)
		{
			LowerZoomValue = CandidateZoomValue;
		}
		else
		{
			UpperZoomValue = CandidateZoomValue;
			UpperArmLength = CandidateArmLength;
		}
	}

	if (UpperArmLength < MinimumArmLength || UpperArmLength > MaximumArmLength)
	{
		return false;
	}
	OutZoomValue = UpperZoomValue;
	OutRepresentedArmLength = UpperArmLength;
	return true;
}

bool AOfficeCameraPawn::TrySettleCalibratedInitialPose(AOfficeInterior* Interior, const FBox2D* FramingBounds)
{
	if (!IsValid(Interior)
		|| !IsValid(MovementInputHandler.Get())
		|| !IsValid(SpringArm.Get())
		|| !IsValid(CameraComponent.Get()))
	{
		return false;
	}

	const FBox2D FloorBounds = Interior->GetCurrentFloorBoundsLocal();
	const FBox2D FocusBounds = FramingBounds ? *FramingBounds : FloorBounds;
	const float StructuralFloorZLocal = Interior->GetStructuralFloorZLocal();
	if (!FloorBounds.bIsValid
		|| !FocusBounds.bIsValid
		|| !FMath::IsFinite(FloorBounds.Min.X)
		|| !FMath::IsFinite(FloorBounds.Min.Y)
		|| !FMath::IsFinite(FloorBounds.Max.X)
		|| !FMath::IsFinite(FloorBounds.Max.Y)
		|| !FMath::IsFinite(FocusBounds.Min.X)
		|| !FMath::IsFinite(FocusBounds.Min.Y)
		|| !FMath::IsFinite(FocusBounds.Max.X)
		|| !FMath::IsFinite(FocusBounds.Max.Y)
		|| !FMath::IsFinite(StructuralFloorZLocal))
	{
		return false;
	}

	const FVector2D FloorSize = FloorBounds.GetSize();
	const double FloorDiagonalCm = FloorSize.Size();
	const double BaselineFloorDiagonalCm =
		static_cast<double>(CalibratedInitialBaselineFloorSizeCm) * FMath::Sqrt(2.0);
	if (!FMath::IsFinite(FloorDiagonalCm) || FloorDiagonalCm <= 0.f)
	{
		return false;
	}

	const FVector2D LocalFloorCenter = FocusBounds.GetCenter();
	const FVector FloorCenterWorld = Interior->GetActorTransform().TransformPosition(
		FVector(LocalFloorCenter.X, LocalFloorCenter.Y, StructuralFloorZLocal));
	FVector CalibratedPawnLocation = FloorCenterWorld + FVector(CalibratedInitialPawnOffsetX, CalibratedInitialPawnOffsetY, 0.f)
		* Interior->GetActorScale3D().X;
	if (FloorCenterWorld.ContainsNaN() || CalibratedPawnLocation.ContainsNaN())
	{
		return false;
	}

	const double FootprintScale = FMath::Max(1.0, FloorDiagonalCm / BaselineFloorDiagonalCm)
		* Interior->GetActorScale3D().X;
	const double DesiredArmLength = static_cast<double>(CalibratedInitialArmLengthCm) * FootprintScale;
	if (!FMath::IsFinite(DesiredArmLength)
		|| DesiredArmLength > static_cast<double>(TNumericLimits<float>::Max()))
	{
		return false;
	}

	float SettledZoomValue = 0.f;
	{
		const float SavedZoomValue = MovementInputHandler->GetZoomValue();
		float ResolvedArmLength = 0.f;
		{
			ON_SCOPE_EXIT
			{
				MovementInputHandler->SetZoomValue(SavedZoomValue);
				MovementInputHandler->ApplyZoomSettings();
			};

			if (!ResolveZoomValueForArmLength(
				static_cast<float>(DesiredArmLength),
				SettledZoomValue,
				ResolvedArmLength))
			{
				return false;
			}
		}

		if (!FMath::IsFinite(SettledZoomValue)
			|| !FMath::IsFinite(ResolvedArmLength)
			|| SettledZoomValue < 0.f
			|| SettledZoomValue > 1.f
			|| ResolvedArmLength <= 0.f)
		{
			return false;
		}
	}

	const bool bSavedCameraLag = SpringArm->bEnableCameraLag;
	const bool bSavedCameraRotationLag = SpringArm->bEnableCameraRotationLag;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;
	ON_SCOPE_EXIT
	{
		SpringArm->bEnableCameraLag = bSavedCameraLag;
		SpringArm->bEnableCameraRotationLag = bSavedCameraRotationLag;
	};

	SetActorLocationAndRotation(CalibratedPawnLocation, FRotator(0.f, DefaultYawRotation, 0.f));
	MovementInputHandler->SetZoomValue(SettledZoomValue);
	MovementInputHandler->ApplyZoomSettings();
	RefreshCameraRigTransforms();

	// 벽의 수직 이음선만 가로 중앙에 맞추고 기본 높이·깊이 구도는 유지한다.
	const FVector CameraRight = CameraComponent->GetRightVector();
	const FVector CenteredPawnLocation = CalculatePawnLocationForTarget(GetOfficeWallCornerWorld(*Interior));
	CalibratedPawnLocation += CameraRight * FVector::DotProduct(
		CenteredPawnLocation - CalibratedPawnLocation, CameraRight);
	SetActorLocation(CalibratedPawnLocation);
	RefreshCameraRigTransforms();

	TargetCameraLocation = CalibratedPawnLocation;
	TargetZoomValue = SettledZoomValue;
	bIsZooming = false;
	StopCameraTransition();
	return true;
}

bool AOfficeCameraPawn::SettleFocusOnLocationImmediately(
	const FVector& Location,
	float DesiredArmLength,
	float MinimumArmLength,
	float MaximumArmLength)
{
	if (!MovementInputHandler || !SpringArm || !CameraComponent)
	{
		return false;
	}

	const bool bSavedCameraLag = SpringArm->bEnableCameraLag;
	const bool bSavedCameraRotationLag = SpringArm->bEnableCameraRotationLag;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;
	ON_SCOPE_EXIT
	{
		SpringArm->bEnableCameraLag = bSavedCameraLag;
		SpringArm->bEnableCameraRotationLag = bSavedCameraRotationLag;
	};

	float RepresentedArmLength = 0.f;
	if (!ResolveInitialZoomValueForArmRange(
		DesiredArmLength,
		MinimumArmLength,
		MaximumArmLength,
		TargetZoomValue,
		RepresentedArmLength))
	{
		return false;
	}

	MovementInputHandler->SetZoomValue(TargetZoomValue);
	MovementInputHandler->ApplyZoomSettings();
	if (!FMath::IsFinite(SpringArm->TargetArmLength)
		|| SpringArm->TargetArmLength < MinimumArmLength
		|| SpringArm->TargetArmLength > MaximumArmLength)
	{
		return false;
	}
	RefreshCameraRigTransforms();

	TargetCameraLocation = CalculatePawnLocationForTarget(Location);
	SetActorLocation(TargetCameraLocation);
	RefreshCameraRigTransforms();

	bIsZooming = false;
	StopCameraTransition();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[OfficeCameraPawn] Initial focus settled: %s, Distance: %.1f, TargetZoom: %.3f"),
		*Location.ToString(),
		RepresentedArmLength,
		TargetZoomValue);
	return true;
}

AOfficeInterior* AOfficeCameraPawn::FindUniqueOfficeInterior(int32& OutInteriorCount) const
{
	OutInteriorCount = 0;
	AOfficeInterior* UniqueInterior = nullptr;
	UWorld* ActorWorld = GetWorld();
	if (!ActorWorld)
	{
		return nullptr;
	}

	for (TActorIterator<AOfficeInterior> InteriorIt(ActorWorld); InteriorIt; ++InteriorIt)
	{
		++OutInteriorCount;
		UniqueInterior = OutInteriorCount == 1 ? *InteriorIt : nullptr;
	}

	return OutInteriorCount == 1 ? UniqueInterior : nullptr;
}

void AOfficeCameraPawn::BindOfficeBoundaryInterior(AOfficeInterior* Interior)
{
	if (BoundOfficeInterior.Get() == Interior)
	{
		UpdateOfficeBoundaryCenter(Interior);
		return;
	}

	if (AOfficeInterior* PreviouslyBoundInterior = BoundOfficeInterior.Get())
	{
		PreviouslyBoundInterior->OnFootprintChanged.RemoveAll(this);
	}

	BoundOfficeInterior = Interior;
	if (Interior)
	{
		Interior->OnFootprintChanged.AddUObject(this, &AOfficeCameraPawn::HandleOfficeFootprintChanged);
		UpdateOfficeBoundaryCenter(Interior);
	}
}

void AOfficeCameraPawn::UpdateOfficeBoundaryCenter(AOfficeInterior* Interior)
{
	if (!MovementInputHandler || !Interior)
	{
		return;
	}

	const FBox2D FloorBounds = Interior->GetCurrentFloorBoundsLocal();
	if (!FloorBounds.bIsValid
		|| !FMath::IsFinite(FloorBounds.Min.X)
		|| !FMath::IsFinite(FloorBounds.Min.Y)
		|| !FMath::IsFinite(FloorBounds.Max.X)
		|| !FMath::IsFinite(FloorBounds.Max.Y))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Cannot update movement boundary from invalid floor bounds."));
		return;
	}

	const FVector2D LocalFloorCenter = FloorBounds.GetCenter();
	const FVector WorldFloorCenter = Interior->GetActorTransform().TransformPosition(
		FVector(LocalFloorCenter.X, LocalFloorCenter.Y, Interior->GetStructuralFloorZLocal()));
	MovementInputHandler->SetBoundaryCenterWorld(WorldFloorCenter);
	const FIntPoint TileCount = Interior->GetTileCount();
	if (TileCount.X <= 0) { return; }
	const double TileMargin = FloorBounds.GetSize().X / TileCount.X;
	if (!MovementInputHandler->SetPlanarMovementBounds(FloorBounds.ExpandBy(TileMargin), Interior->GetActorTransform()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Cannot apply local movement bounds."));
		return;
	}
	const double FootprintScale = FMath::Max(1.0, FloorBounds.GetSize().Size()
		/ (CalibratedInitialBaselineFloorSizeCm * FMath::Sqrt(2.0))) * Interior->GetActorScale3D().X;
	const double MaximumArmLength = FMath::Clamp(
		OfficeZoomOutBaselineArmLengthCm * FootprintScale * OfficeLocalZoomOutScale,
		static_cast<double>(InitialMinimumArmLengthCm), static_cast<double>(OfficeAllowedZoomOutArmLengthCm));
	const float PreviousZoom = MovementInputHandler->GetZoomValue();
	ConfigureOfficeZoomLimit(MovementInputHandler, static_cast<float>(MaximumArmLength));
	if (PreviousZoom != MovementInputHandler->GetZoomValue()) { MovementInputHandler->ApplyZoomSettings(); }
}

bool AOfficeCameraPawn::IsAtAutomaticFramingPose() const
{
	return MovementInputHandler && LastAutomaticFramingZoom >= 0.f
		&& !IsTransitioningCamera() && !bIsZooming && !ProjectionFocusTarget.IsValid()
		&& GetPendingMovementInputVector().IsNearlyZero()
		&& (!MovementComponent || MovementComponent->Velocity.IsNearlyZero())
		&& FMath::IsNearlyZero(HorizontalProjectionOffset) && FMath::IsNearlyZero(TargetHorizontalProjectionOffset)
		&& GetActorTransform().Equals(LastAutomaticFramingPose, 0.1)
		&& FMath::IsNearlyEqual(MovementInputHandler->GetZoomValue(), LastAutomaticFramingZoom, 0.0001f);
}

void AOfficeCameraPawn::HandleOfficeFootprintChanged(const FIntPoint TileCount)
{
	(void)TileCount;
	const bool bStillAutomatic = IsAtAutomaticFramingPose();
	UpdateOfficeBoundaryCenter(BoundOfficeInterior.Get());
	if (bStillAutomatic) ApplyExpansionFramingSlack(BoundOfficeInterior.Get());
}

void AOfficeCameraPawn::ApplyExpansionFramingSlack(AOfficeInterior* Interior)
{
	if (!Interior || !MovementInputHandler || !SpringArm || !CameraComponent) return;
	float Aspect = 0.f;
	int32 ViewWidth = 0, ViewHeight = 0;
	if (!TryGetInitialFramingViewport(Aspect, ViewWidth, ViewHeight)) return;
	const FBox2D Floor = Interior->GetCurrentFloorBoundsLocal();
	if (!Floor.bIsValid) return;
	const double Tile = Interior->GetTileSize();
	const FTransform InteriorTransform = Interior->GetActorTransform();
	const double PresentationScale = Interior->GetActorScale3D().X;
	const double FloorScale = FMath::Max(1.0, Floor.GetSize().Size()
		/ (CalibratedInitialBaselineFloorSizeCm * FMath::Sqrt(2.0))) * PresentationScale;
	const double VirtualWidth = 1440.0 * Aspect;
	// 투명 터치154와 비용 칩까지의 전체 높이166을 확보한다.
	const FVector2D SafeMin(162.0 / VirtualWidth, 227.0 / 1440.0);
	const FVector2D SafeMax(1.0 - 162.0 / VirtualWidth, (1440.0 - 196.0 - 89.0) / 1440.0);
	FMinimalViewInfo BaseView;
	CalcCamera(0.f, BaseView);
	const APlayerController* ViewController = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = ViewController ? ViewController->GetLocalPlayer() : nullptr;
	const EAspectRatioAxisConstraint AxisConstraint = LocalPlayer
		? LocalPlayer->AspectRatioAxisConstraint.GetValue() : GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint.GetValue();
	auto BoundsForSlack = [Floor, Tile](FIntPoint Slack)
	{
		return FBox2D(Floor.Min - FVector2D(0.0, Slack.Y * Tile),
			Floor.Max + FVector2D(Slack.X * Tile, 0.0));
	};
	const FIntPoint Slack = FOfficeCameraFraming::ResolveFramingSlack(
		Interior->HasExpansionCell(EOfficeExpandDirection::Left),
		Interior->HasExpansionCell(EOfficeExpandDirection::Right),
		[&](FIntPoint TrialSlack, bool bAxisX)
		{
			FTransform SampleTransform = CameraComponent->GetComponentTransform();
			float SampleFOV = CameraComponent->FieldOfView;
			if (TrialSlack != FIntPoint::ZeroValue)
			{
				const FBox2D Trial = BoundsForSlack(TrialSlack);
				const float MaxZoom = MovementInputHandler->GetMaximumZoomValue();
				const float DesiredArm = FMath::Min(static_cast<float>(CalibratedInitialArmLengthCm * FloorScale),
					MovementInputHandler->EvaluateZoomRig(MaxZoom).ArmLength);
				const float Zoom = FMath::Min(MaxZoom, CameraFramingMath::SolveZoomForArmLength(
					MovementInputHandler->GetZoomRigParams(),
					[this](float Value) { return MovementInputHandler->EvaluateZoomKey(Value); },
					DesiredArm, 24, 0.001f));
				const FZoomRigSample Rig = MovementInputHandler->EvaluateZoomRig(Zoom);
				const FVector2D Center = Trial.GetCenter();
				const FVector PawnLocation = InteriorTransform.TransformPosition(
					FVector(Center.X, Center.Y, Interior->GetStructuralFloorZLocal()))
					+ FVector(CalibratedInitialPawnOffsetX, CalibratedInitialPawnOffsetY, 0.0) * PresentationScale;
				const FQuat PawnRotation = FRotator(0.f, DefaultYawRotation, 0.f).Quaternion();
				const FQuat RigRotation = PawnRotation * FRotator(Rig.PitchDeg, 0.f, 0.f).Quaternion();
				const FVector ArmOrigin = PawnLocation + PawnRotation.RotateVector(SpringArm->GetRelativeLocation());
				const FVector SocketLocation = ArmOrigin + SpringArm->TargetOffset
					+ RigRotation.RotateVector(SpringArm->SocketOffset - FVector(Rig.ArmLength, 0.f, 0.f));
				SampleTransform = CameraComponent->GetRelativeTransform() * FTransform(RigRotation, SocketLocation);
				const FVector SampleRight = SampleTransform.GetUnitAxis(EAxis::Y);
				SampleTransform.AddToTranslation(SampleRight * FVector::DotProduct(
					GetOfficeWallCornerWorld(*Interior) - SampleTransform.GetLocation(), SampleRight));
				SampleFOV = Rig.HorizontalFOVDeg;
			}
			FMinimalViewInfo View = BaseView;
			View.Location = SampleTransform.GetLocation();
			View.Rotation = SampleTransform.Rotator();
			View.FOV = SampleFOV;
			const FIntRect ViewRect(0, 0, ViewWidth, ViewHeight);
			FSceneViewProjectionData Projection;
			Projection.SetViewRectangle(ViewRect);
			Projection.ViewOrigin = View.Location;
			Projection.ViewRotationMatrix = FInverseRotationMatrix(View.Rotation) * FMatrix(
				FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
			FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View, AxisConstraint, ViewRect, Projection);
			auto IsInside = [&](const FVector& Point, FVector2D Minimum, FVector2D Maximum)
			{
				FVector2D Screen;
				if (!FSceneView::ProjectWorldToScreen(Point, Projection.GetConstrainedViewRect(),
					Projection.ComputeViewProjectionMatrix(), Screen)) return false;
				const FVector2D Normalized(Screen.X / ViewWidth, Screen.Y / ViewHeight);
				return Normalized.X >= Minimum.X && Normalized.X <= Maximum.X
					&& Normalized.Y >= Minimum.Y && Normalized.Y <= Maximum.Y;
			};
			if (TrialSlack != FIntPoint::ZeroValue)
				for (const FVector& Point : MakeRoomAnchorPoints(Floor, Interior->GetStructuralFloorZLocal(),
					InteriorTransform, SampleTransform.GetLocation()))
					if (!IsInside(Point, FVector2D(0.06, 0.06), FVector2D(0.94, 0.94))) return false;
			return IsInside(Interior->GetExpansionAnchorWorld(
				bAxisX ? EOfficeExpandDirection::Left : EOfficeExpandDirection::Right), SafeMin, SafeMax);
		});
	if (Slack != FIntPoint::ZeroValue)
	{
		const FBox2D SelectedBounds = BoundsForSlack(Slack);
		if (!TrySettleCalibratedInitialPose(Interior, &SelectedBounds))
			UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Expansion framing candidate could not settle."));
	}
	LastAutomaticFramingPose = GetActorTransform();
	LastAutomaticFramingZoom = MovementInputHandler->GetZoomValue();
}

bool AOfficeCameraPawn::ApplyInitialOfficeFraming(bool bForce)
{
	if (bInitialOfficeFramingApplied && !bForce)
	{
		bInitialOfficeFramingPending = false;
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

	int32 InteriorCount = 0;
	AOfficeInterior* UniqueInterior = FindUniqueOfficeInterior(InteriorCount);
	if (InteriorCount != 1 || !UniqueInterior)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeCameraPawn] Initial framing expected exactly one OfficeInterior, but found %d."),
			InteriorCount);
		return false;
	}
	BindOfficeBoundaryInterior(UniqueInterior);

	float ViewportAspectRatio = 0.f;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	if (!TryGetInitialFramingViewport(ViewportAspectRatio, ViewportWidth, ViewportHeight))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeCameraPawn] Initial framing rejected invalid viewport size %dx%d."),
			ViewportWidth,
			ViewportHeight);
		return false;
	}

	const FBox2D FloorBounds = UniqueInterior->GetCurrentFloorBoundsLocal();
	if (!FloorBounds.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing rejected invalid office floor bounds."));
		return false;
	}

	const float StructuralFloorZ = UniqueInterior->GetStructuralFloorZLocal();
	FVector LocalFloorFocusPoint = FVector::ZeroVector;
	if (!FOfficeCameraFraming::TryMakeFloorFocusPoint(
		FloorBounds,
		StructuralFloorZ,
		InitialFocusBelowFloorCm,
		LocalFloorFocusPoint))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing could not build the floor focus point."));
		return false;
	}

	const FBox LocalHeroBounds = FOfficeCameraFraming::MakeHeroBounds(
		FloorBounds,
		StructuralFloorZ,
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
	const FVector WorldFloorFocusPoint = InteriorTransform.TransformPosition(LocalFloorFocusPoint);

	const FVector2D FramingSafeMin(InitialOfficeFramingSafeInset, InitialOfficeFramingSafeInset);
	const FVector2D FramingSafeMax(1.f - InitialOfficeFramingSafeInset, 1.f - InitialOfficeFramingSafeInset);
	const TConstArrayView<FVector> WorldHeroCornerView(WorldHeroCorners.GetData(), WorldHeroCorners.Num());

	// 두 벽의 이음선을 가로 중앙에 둔다. 바깥 모서리가 잘린다는 이유로
	// 외벽 전체를 담는 원거리 구도로 되돌리지 않는다.
	if (TrySettleCalibratedInitialPose(UniqueInterior))
	{
		TArray<FVector, TInlineAllocator<2>> RoomAnchors;
		RoomAnchors.Add(GetOfficeWallCornerWorld(*UniqueInterior));
		if (FOfficeCameraFraming::DoesViewFit(
			MakeArrayView(RoomAnchors),
			CameraComponent->GetComponentTransform(),
			CameraComponent->FieldOfView,
			ViewportAspectRatio,
			FramingSafeMin,
			FramingSafeMax))
		{
			bInitialOfficeFramingApplied = true;
			bInitialOfficeFramingPending = false;
			ApplyExpansionFramingSlack(UniqueInterior);
			return true;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeCameraPawn] Calibrated pose room anchors do not fit at aspect %.3f (%dx%d); using dynamic framing."),
			ViewportAspectRatio,
			ViewportWidth,
			ViewportHeight);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Calibrated initial pose unavailable; using dynamic framing fallback."));
	}

	FOfficeCameraFitResult FitResult;
	{
		const float SavedZoomValue = MovementInputHandler->GetZoomValue();
		const bool bSavedCameraLag = SpringArm->bEnableCameraLag;
		const bool bSavedCameraRotationLag = SpringArm->bEnableCameraRotationLag;
		SpringArm->bEnableCameraLag = false;
		SpringArm->bEnableCameraRotationLag = false;

		ON_SCOPE_EXIT
		{
			MovementInputHandler->SetZoomValue(SavedZoomValue);
			MovementInputHandler->ApplyZoomSettings();
			RefreshCameraRigTransforms();
			SpringArm->bEnableCameraLag = bSavedCameraLag;
			SpringArm->bEnableCameraRotationLag = bSavedCameraRotationLag;
		};

		const FVector CurrentPawnLocation = GetActorLocation();
		FitResult = FOfficeCameraFraming::FindClosestFit(
			[this, WorldFloorFocusPoint, CurrentPawnLocation, ViewportAspectRatio](const float ZoomValue)
			{
				MovementInputHandler->SetZoomValue(ZoomValue);
				MovementInputHandler->ApplyZoomSettings();
				RefreshCameraRigTransforms();

				const FVector ProspectivePawnLocation = CalculatePawnLocationForTarget(WorldFloorFocusPoint);
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
			WorldHeroCornerView,
			FramingSafeMin,
			FramingSafeMax,
			32,
			8);
	}

	float SelectedArmLength = FitResult.ArmLength;
	if (!FitResult.bFits)
	{
		SelectedArmLength = MovementInputHandler->MaxZoomDistance;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[OfficeCameraPawn] Hero bounds do not fit at maximum zoom; using configured %.1fcm fallback."),
			SelectedArmLength);
	}
	float BoundedArmLength = 0.f;
	if (!FOfficeCameraFraming::TryClampInitialArmLength(
		SelectedArmLength,
		InitialMinimumArmLengthCm,
		InitialMaximumArmLengthCm,
		BoundedArmLength))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing selected an invalid arm length."));
		return false;
	}
	SelectedArmLength = BoundedArmLength;

	if (!SettleFocusOnLocationImmediately(
		WorldFloorFocusPoint,
		SelectedArmLength,
		InitialMinimumArmLengthCm,
		InitialMaximumArmLengthCm))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeCameraPawn] Initial framing could not settle the camera pose."));
		return false;
	}
	bInitialOfficeFramingApplied = true;
	bInitialOfficeFramingPending = false;
	ApplyExpansionFramingSlack(UniqueInterior);
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

FSkylineClimbCameraOffset FSkylineClimbCameraOffset::Compute(const int32 ClearedCount)
{
	FSkylineClimbCameraOffset Out;
	const int32 Count = FMath::Max(0, ClearedCount);
	Out.LiftCm = FMath::Min(Count * LiftPerFloorCm, MaxLiftCm);
	Out.PullBackCm = FMath::Min(Count * PullBackPerFloorCm, MaxPullBackCm);
	return Out;
}

void AOfficeCameraPawn::SetSkylineClimbFloors(const int32 ClearedCount)
{
	TargetClimbOffset = FSkylineClimbCameraOffset::Compute(ClearedCount);
}

void AOfficeCameraPawn::UpdateSkylineClimbOffset(const float DeltaTime)
{
	if (FMath::IsNearlyEqual(AppliedClimbOffset.LiftCm, TargetClimbOffset.LiftCm, 0.5f)
		&& FMath::IsNearlyEqual(AppliedClimbOffset.PullBackCm, TargetClimbOffset.PullBackCm, 0.5f))
	{
		return;
	}

	const float PreviousLift = AppliedClimbOffset.LiftCm;
	AppliedClimbOffset.LiftCm =
		FMath::FInterpTo(AppliedClimbOffset.LiftCm, TargetClimbOffset.LiftCm, DeltaTime, 3.0f);
	AppliedClimbOffset.PullBackCm =
		FMath::FInterpTo(AppliedClimbOffset.PullBackCm, TargetClimbOffset.PullBackCm, DeltaTime, 3.0f);

	// 절대 위치가 아니라 증분만 준다 — 초기 프레이밍이 정한 자리를 덮어쓰지 않는다
	AddActorWorldOffset(FVector(0.0f, 0.0f, AppliedClimbOffset.LiftCm - PreviousLift));

	// 후퇴는 절대값으로 쓴다 — SocketOffset 은 생성자 이후 아무도 안 건드려 기준이 안정적이고,
	// 줌이 모는 TargetArmLength 를 피해야 보간끼리 싸우지 않는다
	if (SpringArm)
	{
		if (!bClimbSocketBaseCaptured)
		{
			ClimbSocketBaseX = SpringArm->SocketOffset.X;
			bClimbSocketBaseCaptured = true;
		}
		SpringArm->SocketOffset.X = ClimbSocketBaseX - AppliedClimbOffset.PullBackCm + AppliedPresentationPullCm;
	}
}

void AOfficeCameraPawn::PlayPresentationPullIn(const float Fraction, const float Duration)
{
	if (!SpringArm || !GetWorld()) return;
	if (!bClimbSocketBaseCaptured)
	{
		ClimbSocketBaseX = SpringArm->SocketOffset.X;
		bClimbSocketBaseCaptured = true;
	}
	// 카메라~초점 거리 = 팔 길이 − 소켓 전진량(연출 당김은 빼고 잰다)
	const float BaseOffsetX = ClimbSocketBaseX - AppliedClimbOffset.PullBackCm;
	const float ViewDistance = FMath::Max(0.f, SpringArm->TargetArmLength - BaseOffsetX);
	PresentationPullFromCm = AppliedPresentationPullCm;
	PresentationPullToCm = ViewDistance * FMath::Clamp(Fraction, 0.f, 0.5f);
	PresentationPullDuration = FMath::Max(0.01f, Duration);
	PresentationPullStartTime = GetWorld()->GetRealTimeSeconds();
}

void AOfficeCameraPawn::ReleasePresentationPullIn(const float Duration)
{
	if (!GetWorld() || (FMath::IsNearlyZero(AppliedPresentationPullCm) && FMath::IsNearlyZero(PresentationPullToCm))) return;
	PresentationPullFromCm = AppliedPresentationPullCm;
	PresentationPullToCm = 0.f;
	PresentationPullDuration = FMath::Max(0.01f, Duration);
	PresentationPullStartTime = GetWorld()->GetRealTimeSeconds();
}

void AOfficeCameraPawn::UpdatePresentationPull()
{
	if (PresentationPullStartTime < 0.0 || !SpringArm || !GetWorld()) return;
	// 배속(시간 팽창)과 무관하게 실시간으로 당긴다
	const float PullAlpha = FMath::Clamp(static_cast<float>((GetWorld()->GetRealTimeSeconds() - PresentationPullStartTime) / PresentationPullDuration), 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - PullAlpha, 3.f);
	AppliedPresentationPullCm = FMath::Lerp(PresentationPullFromCm, PresentationPullToCm, Eased);
	SpringArm->SocketOffset.X = ClimbSocketBaseX - AppliedClimbOffset.PullBackCm + AppliedPresentationPullCm;
	if (PullAlpha >= 1.f) PresentationPullStartTime = -1.0;
}

void AOfficeCameraPawn::Tick(float DeltaTime)
{
	TryApplyPendingInitialOfficeFraming();

	// 드래그 중이면 카메라 위치 보간 중지 (Edge Panning과 충돌 방지)
	if (PlacementHandler && PlacementHandler->IsDraggingPlacement())
	{
		bIsTransitioningCamera = false;
	}
	if (MovementInputHandler && (bIsTransitioningCamera || bIsZooming))
	{
		TargetZoomValue = FMath::Clamp(TargetZoomValue, 0.f, MovementInputHandler->GetMaximumZoomValue());
		TargetCameraLocation = MovementInputHandler->ClampToMovementBoundary(TargetCameraLocation);
	}

	Super::Tick(DeltaTime); // PlayerCamera의 Tick 호출 (MovementInputHandler의 MoveTracking이 경계 처리함)

	// 등반 고도 보간 — 배치/벽편집 중에는 사용자 조작이 우선이다
	if (!bIsInWallEditMode && !bIsInWorkstationPlacementMode && !bIsInDecorationPlacementMode)
	{
		UpdateSkylineClimbOffset(DeltaTime);
	}
	UpdatePresentationPull();

	// 벽 편집 모드에서 카메라 회전 보간
	UpdateCameraRotation(DeltaTime);

	// 줌 보간 업데이트
	UpdateCameraZoom(DeltaTime);

	// 직원 추적 업데이트
	UpdateEmployeeFollow(DeltaTime);
	if (MovementInputHandler) { MovementInputHandler->ApplyMovementBoundary(); }

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

	if (!bIsInWallEditMode)
	{
		WallEditReturnView = CaptureViewState();
	}
	bIsInWallEditMode = true;

	if (MovementInputHandler)
	{
		// 벽 편집용 줌 계산 (WallEditZoomDistance를 0~1 값으로 변환)
		float ZoomRange = MovementInputHandler->MaxZoomDistance - MovementInputHandler->MinZoomDistance;
		float WallEditZoomRatio = (WallEditZoomDistance - MovementInputHandler->MinZoomDistance) / ZoomRange;
		WallEditZoomRatio = FMath::Clamp(WallEditZoomRatio, 0.f, 1.f);

		// 줌 보간 시작
		TargetZoomValue = WallEditZoomRatio;
		bIsZooming = true;

		UE_LOG(LogTemp, Warning, TEXT("  Original zoom saved: %.2f, Target zoom: %.2f (distance: %.1f)"),
			WallEditReturnView.Zoom, TargetZoomValue, WallEditZoomDistance);
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
				TargetLocation += OfficeInterior->GetActorTransform().TransformVector(
					FVector(0.f, 0.f, OfficeInterior->GetWallHeight() * 0.5f));
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

	// 진입 전 각도로 복귀 (보간 사용)
	FRotator CurrentRotation = GetActorRotation();
	TargetRotation = CurrentRotation;
	TargetRotation.Yaw = WallEditReturnView.bValid ? WallEditReturnView.Yaw : DefaultYawRotation;
	bIsRotating = true;

	// 진입 전 위치·줌으로 미끄러져 돌아간다 ― 위치를 두면 벽 구석을 비춘 채 남는다
	if (MovementInputHandler && WallEditReturnView.bValid)
	{
		TargetZoomValue = WallEditReturnView.Zoom;
		bIsZooming = true;
		TargetCameraLocation = WallEditReturnView.Location;
		BeginCameraTransition();

		UE_LOG(LogTemp, Warning, TEXT("  Returning to original view: zoom %.2f, location %s"),
			WallEditReturnView.Zoom, *WallEditReturnView.Location.ToString());
	}
	WallEditReturnView = FOfficeCameraViewState();

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

	// 보간 중인 실제 카메라를 목표 계산에 되먹임하지 않고 SpringArm의 목표 리그를 사용한다.
	const FQuat RigRotation = SpringArm->GetTargetRotation().Quaternion();
	const FVector SocketWorldLocation = SpringArm->GetComponentLocation() + SpringArm->TargetOffset
		+ RigRotation.RotateVector(SpringArm->SocketOffset - FVector(SpringArm->TargetArmLength, 0.f, 0.f));
	const FTransform TargetViewTransform = CameraComponent->GetRelativeTransform()
		* FTransform(RigRotation, SocketWorldLocation);
	const FVector CameraForward = TargetViewTransform.GetUnitAxis(EAxis::X);
	if (FMath::IsNearlyZero(CameraForward.Z)) { return GetActorLocation(); }
	const FVector CameraWorldLocation = TargetViewTransform.GetLocation();
	const FVector TargetHeightLookPoint = CameraWorldLocation + CameraForward
		* ((TargetLocation.Z - CameraWorldLocation.Z) / CameraForward.Z);
	FVector RequiredPawnOffset = TargetLocation - TargetHeightLookPoint;
	RequiredPawnOffset.Z = 0.f;
	return GetActorLocation() + RequiredPawnOffset;
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

	float ClampedDesiredDistance = 0.f;
	if (!ResolveZoomValueForArmLength(DesiredDistance, TargetZoomValue, ClampedDesiredDistance)) { return; }
	FollowDistance = ClampedDesiredDistance;

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

	const float SavedZoomValue = MovementInputHandler->GetZoomValue();
	float ClampedDesiredDistance = 0.f;
	if (!ResolveZoomValueForArmLength(
		DesiredDistance,
		TargetZoomValue,
		ClampedDesiredDistance))
	{
		return;
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
		*Location.ToString(), ClampedDesiredDistance, TargetZoomValue);
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
	EndOfficePlacementEdit();
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

	// 뷰포트 입력과 월드 상호작용 모드를 함께 복원한다.
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetController());
	if (PC)
	{
		PC->SetGameMode();
		PC->GoToNormalMode();
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
	EndOfficePlacementEdit();
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
	EndOfficePlacementEdit();
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

	// 뷰포트 입력과 월드 상호작용 모드를 함께 복원한다.
	AOfficePlayerController* PC = Cast<AOfficePlayerController>(GetController());
	if (PC)
	{
		PC->SetGameMode();
		PC->GoToNormalMode();
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
