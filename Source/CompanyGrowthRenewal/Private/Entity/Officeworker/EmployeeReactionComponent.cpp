#include "Entity/Officeworker/EmployeeReactionComponent.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Entity/Officeworker/EmployeeAnimInstance.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Data/EmployeeTypes.h"
#include "Office/WorkstationActorBase.h"
#include "Office/OfficeInterior.h"
#include "GameMode/OfficeGameMode.h"
#include "Player/MainMapPlayerController.h"
#include "Global/GlobalUtilFunctions.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

// 수치 출처 = 승인 시안 OfficeDevJuice_2026-10-17 src.html(onEvent·poseChar, juice). 시안의 머리(반지름 36cm)가
// 실제 직원 머리(약 17cm)의 두 배라 머리 기준 효과 크기는 ×0.47 로 옮겼다. 거리 값은 BodyScale 1 기준 cm.
namespace EmployeeReactionTuning
{
	constexpr float SpringStiffness = 260.f;
	constexpr float SpringDamping = 13.f;
	constexpr float SpringStep = 1.f / 120.f;

	constexpr float WakeHopHeight = 25.f;
	constexpr float WakeHopDuration = 0.26f;
	constexpr float WakeStretch = 1.25f;
	constexpr float WakeSurprisedTime = 0.3f;
	constexpr float WakePanicEnd = 0.75f;
	constexpr float WakeWiggleDeg = 20.f;
	constexpr float WakeWiggleRate = 44.f;
	constexpr float WakeSweatTime = 0.35f;
	constexpr float WakeDeterminedTime = 2.5f;
	constexpr float WakeExclamationTime = 0.6f;
	constexpr float TypingBoostRate = 1.8f;
	constexpr float TypingBoostTime = 2.5f;

	constexpr float BoltHopHeight = 17.f;
	constexpr float BoltHopDuration = 0.2f;
	constexpr float BoltStretch = 1.18f;
	constexpr float BoltSurprisedTime = 0.25f;
	constexpr float BoltExclamationTime = 0.55f;
	constexpr float DashMaxDuration = 0.6f;
	constexpr float DashMinSpeed = 220.f;
	constexpr float DashMinLength = 40.f;
	constexpr float DashLeanDeg = 15.f;
	constexpr float DashSettleTime = 0.12f;
	constexpr float DashDustInterval = 0.1f;
	constexpr float DashRunAnimMin = 250.f;
	constexpr float DashRunAnimMax = 550.f;
	constexpr float ArriveSquash = 0.82f;
	constexpr float ArriveDeterminedTime = 0.9f;

	constexpr float PokeTime = 0.85f;
	constexpr float PokeHopHeight = 6.f;
	constexpr float PokeHopDuration = 0.22f;
	constexpr float PokeMaxYawDeg = 25.f;

	constexpr float CritTime = 0.6f;
	constexpr float CritSquash = 0.96f;

	constexpr float MilestoneTime = 1.0f;
	constexpr float MilestoneHopHeight = 11.f;
	constexpr float MilestoneHopDuration = 0.22f;
	constexpr float MilestoneHopGap = 0.24f;

	constexpr float ZFirstDelay = 0.9f;
	constexpr float ZIntervalNodding = 1.4f;
	constexpr float ZIntervalSleeping = 1.0f;
	constexpr float ZLife = 2.0f;
	constexpr float ZSpawnUp = 30.f;
	constexpr float ZSpawnSide = 8.f;
	constexpr float ZRiseSpeed = 40.f;
	constexpr float ZSwaySpeed = 12.f;
	constexpr float ZSizeMin = 18.f;
	constexpr float ZSizeMax = 23.f;

	constexpr float SweatUp = 8.f;
	constexpr float DropSize = 10.f;
	constexpr float DropLife = 0.55f;
	constexpr float DropSideMin = 50.f;
	constexpr float DropSideMax = 85.f;
	constexpr float DropUpMin = 90.f;
	constexpr float DropUpMax = 130.f;
	constexpr float DropGravity = 350.f;

	constexpr float SteamUp = 22.f;
	constexpr float SteamSide = 12.f;
	constexpr float SteamSize = 22.f;
	constexpr float SteamLife = 0.5f;
	constexpr float SteamRise = 35.f;
	constexpr float SteamDrift = 10.f;

	// 얼굴 판 프레임(SK_Body 컴포넌트 공간, +Y = 얼굴 앞): 눈 z 148.45 · 입 z 141 → 코끝 근처.
	constexpr float NoseX = 3.f;
	constexpr float NoseY = 21.5f;
	constexpr float NoseZ = 144.f;
	constexpr float BubbleMinRadius = 2.8f;
	constexpr float BubbleMaxRadius = 9.f;
	constexpr float BubblePulseHz = 0.9f;
	constexpr float BubbleOpacity = 0.72f;
	constexpr float BubblePopTime = 0.12f;
	constexpr float SphereMeshRadius = 50.f;

	constexpr int32 MaxSprites = 8;
	// 먼지 = 김 그림을 바닥에서 퍼뜨린다(가게 나이아가라 연기는 축척을 무시해 책상만큼 커졌다)
	constexpr float DustSize = 26.f;
	constexpr float DustLife = 0.42f;
	constexpr float DustTrailSpeed = 15.f;
	constexpr float DustLandSpeed = 90.f;
	constexpr int32 DustLandCount = 6;

	// 끌어서 데려오기(시안 grab·drag·drop·land·snap). 높이는 바닥 기준 cm, 기울기는 rad.
	constexpr float CarryLiftHeight = 55.f;
	constexpr float CarryStretch = 1.17f;
	constexpr float CarryStretchWobble = 0.03f;
	constexpr float CarryWobbleRate = 9.f;
	constexpr float CarryBounce = 1.5f;
	constexpr float CarryYawWiggleDeg = 8.f;
	constexpr float CarryTiltMax = 0.6f;
	constexpr float CarryTiltPerSpeed = 0.05f;
	constexpr float CarryTiltStiffness = 90.f;
	constexpr float CarryTiltDampingHeld = 7.f;
	constexpr float CarryTiltDampingFree = 9.f;
	constexpr float CarrySweatInterval = 0.28f;
	constexpr float CarryFaceRefresh = 0.2f;
	constexpr float CarryLegRunSpeed = 550.f;
	constexpr float CarryExclamationTime = 0.5f;
	constexpr float CarryBoundsMargin = 40.f;
	constexpr float CarryGravity = 2400.f;
	constexpr float CarryDropReference = 90.f;
	constexpr float CarryDizzyHeight = 70.f;
	constexpr float CarryDizzyTime = 0.55f;
	constexpr float CarrySulkyAfterDizzyTime = 0.7f;
	constexpr float CarrySulkyTime = 0.8f;
	constexpr float CarrySnapRadius = 125.f;
	constexpr float CarrySnapTime = 0.22f;
	constexpr float DropArriveSulkyTime = 0.9f;

	// 검증 명령 drag — 시안 대본: 0.3초 뒤 집어 1.5초 동안 옆으로 휘며(0.9m·0.4m) 35cm 더 솟았다 자리에 놓는다
	constexpr float DebugDragPressDelay = 0.3f;
	constexpr float DebugDragBoltWait = 2.4f;
	constexpr float DebugDragDuration = 1.5f;
	constexpr float DebugDragArc = 35.f;
	constexpr float DebugDragSide = 90.f;
	constexpr float DebugDragDepth = 40.f;
	constexpr float DebugDragFarDistance = 250.f;
}

namespace EmployeeReactionAssets
{
	const TCHAR* const SpriteMaterialPath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Reaction/M_EmployeeReactionSprite.M_EmployeeReactionSprite");
	const TCHAR* const BubbleMaterialPath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Reaction/M_EmployeeSnotBubble.M_EmployeeSnotBubble");
	const TCHAR* const ZTexturePath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Reaction/T_EmployeeReact_Z.T_EmployeeReact_Z");
	const TCHAR* const DropTexturePath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Reaction/T_EmployeeReact_Drop.T_EmployeeReact_Drop");
	const TCHAR* const SteamTexturePath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Reaction/T_EmployeeReact_Steam.T_EmployeeReact_Steam");
	const TCHAR* const SphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* const ExclamationPath = TEXT("/Game/CompanyGrowth/UI/Textures/WorkerEmote/emote_exclamation.emote_exclamation");
	const TCHAR* const HeadBone = TEXT("Head");

	static void WarnMissingOnce(const TCHAR* What)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogTemp, Warning, TEXT("[EmployeeReaction] 연출 에셋 없음 %s — Tools/EmployeeReaction/author_reaction_assets.py 실행 필요(몸·얼굴 반응만 동작)"), What);
		}
	}
}

UEmployeeReactionComponent::UEmployeeReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// 애님 평가 뒤에 메시를 움직여야 머리 본 위치가 이번 프레임 값이다
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UEmployeeReactionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AOfficeworker* Worker = GetWorker())
	{
		if (UEmployeeBehaviorComponent* Behavior = Worker->BehaviorComponent)
		{
			Behavior->OnFatigueSlackPhaseChanged.AddUObject(this, &UEmployeeReactionComponent::HandleSlackPhaseChanged);
			Behavior->OnCriticalContribution.AddUObject(this, &UEmployeeReactionComponent::HandleCriticalContribution);
		}
	}
}

void UEmployeeReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbortCarry();
	if (AOfficeworker* Worker = GetWorker())
	{
		if (UEmployeeBehaviorComponent* Behavior = Worker->BehaviorComponent)
		{
			Behavior->OnFatigueSlackPhaseChanged.RemoveAll(this);
			Behavior->OnCriticalContribution.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

AOfficeworker* UEmployeeReactionComponent::GetWorker() const
{
	return Cast<AOfficeworker>(GetOwner());
}

USkeletalMeshComponent* UEmployeeReactionComponent::GetBody() const
{
	const AOfficeworker* Worker = GetWorker();
	return Worker ? Worker->GetMesh() : nullptr;
}

bool UEmployeeReactionComponent::IsMotionReduced() const
{
	return USettingsManagerSubsystem::IsReduceMotion(this);
}

bool UEmployeeReactionComponent::IsBodyActBusy() const
{
	return WakeStartTime >= 0.0 || DashStartTime >= 0.0 || PokeStartTime >= 0.0 || CarryPhase != EEmployeeCarryPhase::None;
}

void UEmployeeReactionComponent::StartTicking()
{
	CaptureBaseTransforms();
	if (!IsComponentTickEnabled())
	{
		SetComponentTickEnabled(true);
	}
}

float UEmployeeReactionComponent::GetFxScale() const
{
	const AOfficeworker* Worker = GetWorker();
	return Worker ? FMath::Max(0.1f, static_cast<float>(Worker->GetActorScale3D().Z)) : 1.f;
}

FVector UEmployeeReactionComponent::GetHeadLocation() const
{
	const USkeletalMeshComponent* Body = GetBody();
	const FName HeadName(EmployeeReactionAssets::HeadBone);
	if (Body && Body->DoesSocketExist(HeadName))
	{
		return Body->GetSocketLocation(HeadName);
	}
	const AOfficeworker* Worker = GetWorker();
	return Worker ? Worker->GetActorLocation() + FVector(0.f, 0.f, 56.f * GetFxScale()) : FVector::ZeroVector;
}

void UEmployeeReactionComponent::GetCameraAxes(FVector& OutRight, FVector& OutForwardFlat) const
{
	FRotator CameraRotation = FRotator::ZeroRotator;
	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		CameraRotation = CameraManager->GetCameraRotation();
	}
	const FRotationMatrix CameraMatrix(CameraRotation);
	OutRight = CameraMatrix.GetScaledAxis(EAxis::Y);
	OutForwardFlat = CameraMatrix.GetScaledAxis(EAxis::X).GetSafeNormal2D();
}

FVector UEmployeeReactionComponent::GetCameraForward() const
{
	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		return CameraManager->GetCameraRotation().Vector();
	}
	return FVector::ForwardVector;
}

float UEmployeeReactionComponent::GetCapsuleHalfHeight() const
{
	const AOfficeworker* Worker = GetWorker();
	const UCapsuleComponent* Capsule = Worker ? Worker->GetCapsuleComponent() : nullptr;
	return Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 92.f * GetFxScale();
}

void UEmployeeReactionComponent::PlayFace(EWorkerFaceExpression Expression, float Duration)
{
	if (AFunnyOfficeworker* Funny = Cast<AFunnyOfficeworker>(GetOwner()))
	{
		Funny->PlayFaceReaction(Expression, Duration);
	}
}

void UEmployeeReactionComponent::ShowExclamation(float Seconds)
{
	AOfficeworker* Worker = GetWorker();
	UTexture2D* Emote = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(EmployeeReactionAssets::ExclamationPath)).LoadSynchronous();
	if (Worker && Emote)
	{
		Worker->PlayEmoteReaction(Emote, Seconds);
	}
}

void UEmployeeReactionComponent::AddHop(double StartTime, float Duration, float Height)
{
	FEmployeeReactionHop& Hop = Hops.AddDefaulted_GetRef();
	Hop.StartTime = StartTime;
	Hop.Duration = FMath::Max(0.05f, Duration);
	Hop.Height = Height;
}

void UEmployeeReactionComponent::KickSquash(float Value)
{
	Squash = Value;
	SquashVelocity = 0.f;
}

//========================================
// 반응 진입점
//========================================

void UEmployeeReactionComponent::HandleSlackPhaseChanged(EFatigueSlackPhase OldPhase, EFatigueSlackPhase NewPhase, bool bManualCatch, const FVector& PreviousLocation)
{
	const AOfficeworker* Worker = GetWorker();
	const UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
	if (!Behavior || Worker->bIsPortraitMode) return;

	// 들고 있는 동안 다른 경로(판 종료·자동 복귀)로 폭주가 풀리면 몸을 바닥에 내려놓고 끝낸다.
	// 자기 착지 잡기는 TryManualCatch 전에 단계를 비워 두므로 여기 걸리지 않는다.
	FVector ReturnFrom = PreviousLocation;
	if (OldPhase == EFatigueSlackPhase::Bolting && IsCarryActive())
	{
		if (IsCarryLifted())
		{
			ReturnFrom.Z = CarryFloorZ;
		}
		AbortCarry();
	}

	// 딴짓 사유(태평 자세 1)는 자는 게 아니라 Z·콧방울을 띄우지 않는다
	const bool bDrowsyReason = Behavior->SeatedPoseIndex != 1;
	if (NewPhase == EFatigueSlackPhase::Telegraph || NewPhase == EFatigueSlackPhase::Slumping)
	{
		SetDozeVisual(bDrowsyReason
			? (NewPhase == EFatigueSlackPhase::Slumping ? EEmployeeDozeVisual::Sleeping : EEmployeeDozeVisual::Nodding)
			: EEmployeeDozeVisual::None);
		return;
	}

	if (OldPhase == EFatigueSlackPhase::Telegraph || OldPhase == EFatigueSlackPhase::Slumping)
	{
		// 깨우기가 먼저 콧방울 터뜨림을 시작해야 졸음 해제가 콧방울을 그냥 지우지 않는다
		if (bManualCatch)
		{
			PlayWakeReaction(IsSnotBubbleShown());
		}
		SetDozeVisual(EEmployeeDozeVisual::None);
		return;
	}

	if (OldPhase == EFatigueSlackPhase::Bolting && NewPhase == EFatigueSlackPhase::None && bManualCatch)
	{
		PlayBoltReturn(ReturnFrom, bCarryDropped);
	}
}

void UEmployeeReactionComponent::HandleCriticalContribution()
{
	PlayCritReaction();
}

void UEmployeeReactionComponent::PlayWakeReaction(bool bPopBubble)
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || !GetBody()) return;
	using namespace EmployeeReactionTuning;

	const double Now = CurrentWorld->GetTimeSeconds();
	const bool bReduced = IsMotionReduced();
	StartTicking();

	PokeStartTime = -1.0;
	WakeStartTime = Now;
	bWakePanicShown = false;
	bWakeSweatShown = false;
	PlayFace(EWorkerFaceExpression::Surprised, WakeSurprisedTime);
	ShowExclamation(WakeExclamationTime);
	if (bPopBubble)
	{
		StartBubblePop(true);
	}
	if (!bReduced)
	{
		AddHop(Now, WakeHopDuration, WakeHopHeight);
		KickSquash(WakeStretch);
	}
}

void UEmployeeReactionComponent::PlayBoltReturn(const FVector& FromLocation, bool bDropped)
{
	AOfficeworker* Worker = GetWorker();
	UWorld* CurrentWorld = GetWorld();
	if (!Worker || !CurrentWorld || !GetBody()) return;
	using namespace EmployeeReactionTuning;

	const double Now = CurrentWorld->GetTimeSeconds();
	const bool bReduced = IsMotionReduced();
	const float FxScale = GetFxScale();
	StartTicking();

	WakeStartTime = -1.0;
	PokeStartTime = -1.0;
	DashStartTime = Now;
	bDashPanicShown = false;

	// 경로 = 이탈 위치 → 자리 앞 접근점 → 좌석. 접근점은 내비 바닥 높이라 달리는 높이(캡슐 중심)로 맞춘다.
	DashFrom = FromLocation;
	DashTo = Worker->GetActorLocation();
	// 끌어서 자리 1.25m 안에 놓았으면 접근점을 거치지 않고 0.22초에 미끄러져 앉는다
	DashStyle = !bDropped ? EEmployeeDashStyle::Caught
		: (FVector::Dist2D(DashFrom, DashTo) <= CarrySnapRadius * FxScale ? EEmployeeDashStyle::DropSnap : EEmployeeDashStyle::DropRun);
	DashVia = Worker->GetWorkstationApproachLocation();
	DashVia.Z = DashFrom.Z;
	if (DashStyle == EEmployeeDashStyle::DropSnap
		|| FVector::Dist2D(DashFrom, DashTo) <= FVector::Dist2D(DashVia, DashTo) || FVector::Dist2D(DashFrom, DashVia) < DashMinLength)
	{
		DashVia = DashFrom;
	}
	DashLength = static_cast<float>(FVector::Dist(DashFrom, DashVia) + FVector::Dist(DashVia, DashTo));
	NextDustTime = Now;
	if (DashStyle == EEmployeeDashStyle::DropSnap)
	{
		DashDuration = CarrySnapTime;
		bDashMoving = !bReduced && DashLength > 1.f;
		DashRunSpeed = DashRunAnimMin;
	}
	else
	{
		// 복귀는 0.6초로 고정 — 아주 가까우면 시안처럼 최저 속도로 더 일찍 도착한다
		DashDuration = FMath::Min(DashMaxDuration, DashLength / (DashMinSpeed * FxScale));
		bDashMoving = !bReduced && DashLength >= DashMinLength;
		DashRunSpeed = FMath::Clamp(DashLength / FMath::Max(0.05f, DashDuration), DashRunAnimMin, DashRunAnimMax);
		if (!bDashMoving)
		{
			DashDuration = DashMaxDuration;
		}
	}

	// 끌어서 놓은 복귀는 착지가 얼굴·눌림을 이미 걸었다 — 놀람·「!」·점프는 탭 잡기에만
	if (bDropped) return;

	PlayFace(EWorkerFaceExpression::Surprised, BoltSurprisedTime);
	ShowExclamation(BoltExclamationTime);
	if (!bReduced)
	{
		AddHop(Now, BoltHopDuration, BoltHopHeight);
		KickSquash(BoltStretch);
	}
}

bool UEmployeeReactionComponent::PlayWorkPoke(bool bIgnoreWorkState)
{
	AOfficeworker* Worker = GetWorker();
	UWorld* CurrentWorld = GetWorld();
	const UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
	if (!Worker || !CurrentWorld || !Behavior || !GetBody() || IsBodyActBusy()) return false;
	if (!bIgnoreWorkState && (Behavior->GetBehaviorMode() != EEmployeeBehaviorMode::Stage
		|| Behavior->GetState() != EEmployeeState::Typing || !Behavior->bIsSeated
		|| Behavior->GetFatigueSlackPhase() != EFatigueSlackPhase::None))
	{
		return false;
	}
	using namespace EmployeeReactionTuning;

	const double Now = CurrentWorld->GetTimeSeconds();
	StartTicking();
	PokeStartTime = Now;
	PlayFace(EWorkerFaceExpression::Wink, PokeTime);

	PokeYawTarget = 0.f;
	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = CameraManager->GetCameraLocation() - Worker->GetActorLocation();
		const float CameraYaw = static_cast<float>(ToCamera.Rotation().Yaw);
		PokeYawTarget = FMath::Clamp(FMath::FindDeltaAngleDegrees(static_cast<float>(Worker->GetActorRotation().Yaw), CameraYaw),
			-PokeMaxYawDeg, PokeMaxYawDeg);
	}
	if (!IsMotionReduced())
	{
		AddHop(Now, PokeHopDuration, PokeHopHeight);
	}
	return true;
}

void UEmployeeReactionComponent::PlayCritReaction()
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || !GetBody() || WakeStartTime >= 0.0 || DashStartTime >= 0.0) return;
	using namespace EmployeeReactionTuning;

	StartTicking();
	PlayFace(EWorkerFaceExpression::StarEyes, CritTime);
	if (!IsMotionReduced())
	{
		KickSquash(CritSquash);
	}
}

void UEmployeeReactionComponent::PlayMilestoneReaction()
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || !GetBody() || WakeStartTime >= 0.0 || DashStartTime >= 0.0) return;
	using namespace EmployeeReactionTuning;

	const double Now = CurrentWorld->GetTimeSeconds();
	StartTicking();
	PlayFace(EWorkerFaceExpression::Happy, MilestoneTime);
	if (!IsMotionReduced())
	{
		AddHop(Now, MilestoneHopDuration, MilestoneHopHeight);
		AddHop(Now + MilestoneHopGap, MilestoneHopDuration, MilestoneHopHeight);
	}
}

void UEmployeeReactionComponent::PreviewDoze(float Seconds)
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || !GetBody()) return;
	SetDozeVisual(EEmployeeDozeVisual::Sleeping, true);
	DozePreviewEndTime = CurrentWorld->GetTimeSeconds() + FMath::Max(0.5f, Seconds);
}

bool UEmployeeReactionComponent::PlayDebugReaction(const FString& Kind, const FString& Option)
{
	AOfficeworker* Worker = GetWorker();
	if (!Worker) return false;

	if (Kind.Equals(TEXT("drag"), ESearchCase::IgnoreCase))
	{
		return StartDebugDrag(Option.Equals(TEXT("far"), ESearchCase::IgnoreCase));
	}

	if (Kind.Equals(TEXT("wake"), ESearchCase::IgnoreCase))
	{
		PlayWakeReaction(IsSnotBubbleShown());
		SetDozeVisual(EEmployeeDozeVisual::None);
		return true;
	}
	if (Kind.Equals(TEXT("bolt"), ESearchCase::IgnoreCase))
	{
		const float FxScale = GetFxScale();
		PlayBoltReturn(Worker->GetActorLocation() + (Worker->GetActorRightVector() * 220.f - Worker->GetActorForwardVector() * 180.f) * FxScale);
		return true;
	}
	if (Kind.Equals(TEXT("poke"), ESearchCase::IgnoreCase))
	{
		return PlayWorkPoke(true);
	}
	if (Kind.Equals(TEXT("crit"), ESearchCase::IgnoreCase))
	{
		PlayCritReaction();
		return true;
	}
	if (Kind.Equals(TEXT("mile"), ESearchCase::IgnoreCase))
	{
		PlayMilestoneReaction();
		return true;
	}
	if (Kind.Equals(TEXT("doze"), ESearchCase::IgnoreCase))
	{
		PreviewDoze(6.f);
		return true;
	}
	return false;
}

int32 UEmployeeReactionComponent::NotifyDisciplineMilestone(UWorld* InWorld, int32 DisciplineSlot)
{
	if (!InWorld || DisciplineSlot == INDEX_NONE) return 0;
	UGameInstance* GameInstance = InWorld->GetGameInstance();
	UEmployeeManager* Employees = GameInstance ? GameInstance->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!Employees) return 0;

	int32 Reacted = 0;
	for (TActorIterator<AOfficeworker> It(InWorld); It; ++It)
	{
		AOfficeworker* Worker = *It;
		const UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
		if (!Behavior || Worker->bIsPortraitMode || Behavior->GetBehaviorMode() != EEmployeeBehaviorMode::Stage
			|| Behavior->GetFatigueSlackPhase() != EFatigueSlackPhase::None)
		{
			continue;
		}
		const FEmployeeInstance* Employee = Employees->FindEmployee(Worker->GetEmployeeID());
		if (!Employee || GetPrimaryDisciplineSlot(*Employee) != DisciplineSlot) continue;
		if (UEmployeeReactionComponent* Reaction = Worker->FindComponentByClass<UEmployeeReactionComponent>())
		{
			Reaction->PlayMilestoneReaction();
			++Reacted;
		}
	}
	return Reacted;
}

//========================================
// 틱
//========================================

void UEmployeeReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	using namespace EmployeeReactionTuning;

	AOfficeworker* Worker = GetWorker();
	USkeletalMeshComponent* Body = GetBody();
	UWorld* CurrentWorld = GetWorld();
	if (!Worker || !Body || !CurrentWorld)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const double Now = CurrentWorld->GetTimeSeconds();
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.05f);
	const bool bReduced = IsMotionReduced();
	const UEmployeeBehaviorComponent* Behavior = Worker->BehaviorComponent;
	const float FxScale = GetFxScale();

	FVector BodyOffset = FVector::ZeroVector;
	float YawDeg = 0.f;
	float LeanDeg = 0.f;
	float RunSpeed = -1.f;
	float CarryStretchValue = -1.f;

	if (DebugDragArmTime >= 0.0 || DebugDragStartTime >= 0.0)
	{
		UpdateDebugDrag(Now);
	}
	if (CarryPhase != EEmployeeCarryPhase::None)
	{
		UpdateCarry(Dt, Now, BodyOffset, YawDeg, CarryStretchValue, RunSpeed);
	}
	if (AfterDizzyTime >= 0.0 && Now >= AfterDizzyTime)
	{
		AfterDizzyTime = -1.0;
		PlayFace(EWorkerFaceExpression::Sulky, CarrySulkyAfterDizzyTime);
	}

	if (WakeStartTime >= 0.0)
	{
		const float T = static_cast<float>(Now - WakeStartTime);
		if (T >= WakeSurprisedTime && !bWakePanicShown)
		{
			bWakePanicShown = true;
			PlayFace(EWorkerFaceExpression::Panic, WakePanicEnd - WakeSurprisedTime);
		}
		if (T >= WakeSweatTime && !bWakeSweatShown)
		{
			bWakeSweatShown = true;
			if (!bReduced)
			{
				const FVector SweatOrigin = GetHeadLocation() + FVector(0.f, 0.f, SweatUp * FxScale);
				SpawnDrops(SweatOrigin, 3, 1.f);
				SpawnDrops(SweatOrigin, 2, -1.f);
			}
		}
		if (!bReduced && T >= WakeSurprisedTime && T < WakePanicEnd)
		{
			const float U = T - WakeSurprisedTime;
			YawDeg += FMath::Sin(U * WakeWiggleRate) * WakeWiggleDeg * (1.f - U / (WakePanicEnd - WakeSurprisedTime));
		}
		if (T >= WakePanicEnd)
		{
			WakeStartTime = -1.0;
			PlayFace(EWorkerFaceExpression::Determined, WakeDeterminedTime);
			SpawnSteamPuffs(bReduced);
			TypingBoostEndTime = bReduced ? -1.0 : Now + TypingBoostTime;
		}
	}

	if (DashStartTime >= 0.0)
	{
		const float T = static_cast<float>(Now - DashStartTime);
		if (DashStyle == EEmployeeDashStyle::Caught && T >= BoltSurprisedTime && !bDashPanicShown)
		{
			bDashPanicShown = true;
			PlayFace(EWorkerFaceExpression::Panic, FMath::Max(0.05f, DashDuration - BoltSurprisedTime));
		}
		// 판이 끝나는 등 자리 상태가 바뀌면 달리기를 접는다
		const bool bAbort = !Behavior || Behavior->GetState() != EEmployeeState::Typing
			|| Behavior->GetFatigueSlackPhase() != EFatigueSlackPhase::None;
		if (bAbort || T >= DashDuration)
		{
			FinishDash(!bAbort);
		}
		else if (bDashMoving && !bReduced)
		{
			FVector PathLocation, PathDirection;
			const float DashAlpha = T / DashDuration;
			// 자리 가까이 놓은 몸은 시안 snap 처럼 빠르게 붙고 끝에서 잦아든다(eOut)
			EvaluateDash(DashStyle == EEmployeeDashStyle::DropSnap ? 1.f - FMath::Pow(1.f - DashAlpha, 3.f) : DashAlpha, PathLocation, PathDirection);
			BodyOffset = Worker->GetActorTransform().InverseTransformVector(PathLocation - DashTo);
			const float Settle = FMath::Clamp((DashDuration - T) / DashSettleTime, 0.f, 1.f);
			if (!PathDirection.IsNearlyZero())
			{
				YawDeg += FMath::FindDeltaAngleDegrees(static_cast<float>(Worker->GetActorRotation().Yaw),
					static_cast<float>(PathDirection.Rotation().Yaw)) * Settle;
			}
			LeanDeg += DashLeanDeg * Settle;
			RunSpeed = DashRunSpeed;
			if (DashStyle != EEmployeeDashStyle::DropSnap && Now >= NextDustTime)
			{
				NextDustTime = Now + DashDustInterval;
				const float HalfHeight = Worker->GetCapsuleComponent() ? Worker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 92.f * FxScale;
				SpawnDust(PathLocation - FVector(0.f, 0.f, HalfHeight), 1, DustTrailSpeed);
			}
		}
	}

	if (PokeStartTime >= 0.0)
	{
		const float T = static_cast<float>(Now - PokeStartTime);
		if (T >= PokeTime)
		{
			PokeStartTime = -1.0;
		}
		else if (!bReduced)
		{
			// 시안 그대로 0.65초 안에 돌아갔다 돌아온다(sin(π·min(1, 1.3t)))
			YawDeg += PokeYawTarget * FMath::Sin(PI * FMath::Min(1.f, T / PokeTime * 1.3f));
		}
	}

	float HopZ = 0.f;
	for (int32 Index = Hops.Num() - 1; Index >= 0; --Index)
	{
		const float U = static_cast<float>((Now - Hops[Index].StartTime) / Hops[Index].Duration);
		if (U >= 1.f)
		{
			Hops.RemoveAt(Index);
			continue;
		}
		if (U > 0.f)
		{
			HopZ += FMath::Sin(PI * U) * Hops[Index].Height;
		}
	}

	// 반암시적 오일러 + 잘게 나눈 걸음 — 저프레임에서도 스프링이 튀지 않는다
	for (float Remaining = Dt; Remaining > KINDA_SMALL_NUMBER; Remaining -= SpringStep)
	{
		const float Step = FMath::Min(Remaining, SpringStep);
		SquashVelocity += (-(Squash - 1.f) * SpringStiffness - SquashVelocity * SpringDamping) * Step;
		Squash += SquashVelocity * Step;
	}
	// 들린 동안은 늘어남을 직접 쥔다 — 놓으면 그 값에서 스프링이 이어받는다
	if (CarryStretchValue > 0.f)
	{
		Squash = CarryStretchValue;
		SquashVelocity = 0.f;
	}
	if (bReduced)
	{
		Squash = 1.f;
		SquashVelocity = 0.f;
	}
	const FQuat Swing = UpdateCarrySwing(Dt);

	const bool bBoost = !bReduced && Now < TypingBoostEndTime && Behavior && Behavior->GetState() == EEmployeeState::Typing;
	const float TargetRate = BaseAnimRateScale * (bBoost ? TypingBoostRate : 1.f);
	if (!FMath::IsNearlyEqual(Body->GlobalAnimRateScale, TargetRate))
	{
		Body->GlobalAnimRateScale = TargetRate;
	}
	SetRunOverride(RunSpeed);

	const bool bPose = !bReduced && (!BodyOffset.IsNearlyZero(0.01) || FMath::Abs(HopZ) > 0.01f || FMath::Abs(YawDeg) > 0.01f
		|| FMath::Abs(LeanDeg) > 0.01f || FMath::Abs(Squash - 1.f) > 0.0005f || !Swing.IsIdentity(KINDA_SMALL_NUMBER));
	if (bPose)
	{
		ApplyBodyPose(BodyOffset + FVector(0.f, 0.f, HopZ), YawDeg, LeanDeg, Squash, BodyOffset, Swing);
	}
	else if (bPoseApplied)
	{
		RestoreBodyPose(false);
	}

	if (DozeVisual != EEmployeeDozeVisual::None)
	{
		// 모드 전환처럼 알림 없이 슬랙이 풀리는 경로에서도 졸음 표시가 남지 않게 매 틱 대조한다
		const EFatigueSlackPhase Phase = Behavior ? Behavior->GetFatigueSlackPhase() : EFatigueSlackPhase::None;
		const bool bStillDozing = bDozePreview ? Now < DozePreviewEndTime
			: (Phase == EFatigueSlackPhase::Telegraph || Phase == EFatigueSlackPhase::Slumping);
		if (bStillDozing)
		{
			UpdateDoze(Now, bReduced);
		}
		else
		{
			SetDozeVisual(EEmployeeDozeVisual::None);
		}
	}
	if (BubblePopStartTime >= 0.0)
	{
		UpdateBubblePop(Now);
	}
	UpdateSprites(Dt, Now);

	if (!HasPendingWork(Now))
	{
		RestoreBodyPose(true);
		SetComponentTickEnabled(false);
	}
}

bool UEmployeeReactionComponent::HasPendingWork(double Now) const
{
	return IsBodyActBusy() || Hops.Num() > 0 || Now < TypingBoostEndTime || DozeVisual != EEmployeeDozeVisual::None
		|| BubblePopStartTime >= 0.0 || HasActiveSprites()
		|| FMath::Abs(Squash - 1.f) > 0.001f || FMath::Abs(SquashVelocity) > 0.01f
		|| DebugDragArmTime >= 0.0 || DebugDragStartTime >= 0.0 || AfterDizzyTime >= 0.0
		|| FMath::Abs(CarryTilt) > 0.001f || FMath::Abs(CarryTiltVelocity) > 0.01f;
}

//========================================
// 바디 메시 변환
//========================================

void UEmployeeReactionComponent::CaptureBaseTransforms()
{
	if (bBaseCaptured) return;
	AOfficeworker* Worker = GetWorker();
	USkeletalMeshComponent* Body = GetBody();
	if (!Worker || !Body) return;

	BaseBodyLocation = Body->GetRelativeLocation();
	BaseBodyRotation = Body->GetRelativeRotation().Quaternion();
	BaseBodyScale = Body->GetRelativeScale3D();
	// 책상 몰입 연출이 같은 값을 1.33배로 올려 둘 수 있어 잴 때마다 달라진다 — 직원 애니 기본 속도는 1 이다.
	BaseAnimRateScale = 1.f;
	if (Worker->BubbleWidgetComponent)
	{
		BaseOverheadLocation = Worker->BubbleWidgetComponent->GetRelativeLocation();
	}
	bBaseCaptured = true;
}

void UEmployeeReactionComponent::ApplyBodyPose(const FVector& Offset, float YawDeg, float LeanDeg, float Stretch, const FVector& OverheadOffset, const FQuat& Swing)
{
	AOfficeworker* Worker = GetWorker();
	USkeletalMeshComponent* Body = GetBody();
	if (!Worker || !Body || !bBaseCaptured) return;

	// 메시 원점 = 발바닥이라 늘어남·기울기가 바닥을 축으로 일어난다. 부피 보존: 세로 s, 가로 1/√s.
	const float SafeStretch = FMath::Clamp(Stretch, 0.6f, 1.5f);
	const float Thin = 1.f / FMath::Sqrt(FMath::Max(0.4f, SafeStretch));
	// 들린 몸의 흔들림은 캡슐 중심(손가락이 쥔 자리)을 축으로 돌아 발이 끄는 반대쪽으로 처진다
	const FVector PoseLocation = Swing.RotateVector(BaseBodyLocation + Offset);
	const FQuat PoseRotation = Swing * FRotator(-LeanDeg, YawDeg, 0.f).Quaternion() * BaseBodyRotation;
	Body->SetRelativeLocationAndRotation(PoseLocation, PoseRotation);
	Body->SetRelativeScale3D(BaseBodyScale * FVector(Thin, Thin, SafeStretch));
	// 「!」 말풍선도 달리는 몸을 따라간다
	if (Worker->BubbleWidgetComponent)
	{
		Worker->BubbleWidgetComponent->SetRelativeLocation(BaseOverheadLocation + OverheadOffset);
	}
	bPoseApplied = true;
}

void UEmployeeReactionComponent::RestoreBodyPose(bool bResetAnimRate)
{
	AOfficeworker* Worker = GetWorker();
	USkeletalMeshComponent* Body = GetBody();
	SetRunOverride(-1.f);
	if (!Worker || !Body || !bBaseCaptured) return;

	if (bPoseApplied)
	{
		Body->SetRelativeLocationAndRotation(BaseBodyLocation, BaseBodyRotation);
		Body->SetRelativeScale3D(BaseBodyScale);
		if (Worker->BubbleWidgetComponent)
		{
			Worker->BubbleWidgetComponent->SetRelativeLocation(BaseOverheadLocation);
		}
		bPoseApplied = false;
	}
	// 타자 가속은 틱이 매 프레임 맞추므로 쉬러 갈 때만 원래 값으로 되돌린다
	if (bResetAnimRate && !FMath::IsNearlyEqual(Body->GlobalAnimRateScale, BaseAnimRateScale))
	{
		Body->GlobalAnimRateScale = BaseAnimRateScale;
	}
}

void UEmployeeReactionComponent::SetRunOverride(float RunSpeed)
{
	const bool bWant = RunSpeed >= 0.f;
	if (!bWant && !bRunOverrideApplied) return;
	const USkeletalMeshComponent* Body = GetBody();
	if (UEmployeeAnimInstance* Anim = Body ? Cast<UEmployeeAnimInstance>(Body->GetAnimInstance()) : nullptr)
	{
		Anim->ReactionRunSpeed = bWant ? RunSpeed : -1.f;
	}
	bRunOverrideApplied = bWant;
}

void UEmployeeReactionComponent::EvaluateDash(float Alpha, FVector& OutLocation, FVector& OutDirection) const
{
	const float FirstLength = static_cast<float>(FVector::Dist(DashFrom, DashVia));
	const float Distance = FMath::Clamp(Alpha, 0.f, 1.f) * DashLength;
	if (Distance < FirstLength && FirstLength > KINDA_SMALL_NUMBER)
	{
		OutLocation = FMath::Lerp(DashFrom, DashVia, Distance / FirstLength);
		OutDirection = (DashVia - DashFrom).GetSafeNormal2D();
	}
	else
	{
		const float SecondLength = FMath::Max(KINDA_SMALL_NUMBER, DashLength - FirstLength);
		OutLocation = FMath::Lerp(DashVia, DashTo, FMath::Clamp((Distance - FirstLength) / SecondLength, 0.f, 1.f));
		OutDirection = (DashTo - DashVia).GetSafeNormal2D();
	}
}

void UEmployeeReactionComponent::FinishDash(bool bArrived)
{
	using namespace EmployeeReactionTuning;
	DashStartTime = -1.0;
	SetRunOverride(-1.f);
	if (!bArrived) return;

	// 끌려온 직원은 앉고 나서도 잠깐 삐쳐 있다(시안 seated·drop). 어지러움이 남았으면 그 뒤 삐침이 이어받는다.
	if (DashStyle == EEmployeeDashStyle::Caught)
	{
		PlayFace(EWorkerFaceExpression::Determined, ArriveDeterminedTime);
	}
	else if (AfterDizzyTime < 0.0)
	{
		PlayFace(EWorkerFaceExpression::Sulky, DropArriveSulkyTime);
	}
	if (!IsMotionReduced())
	{
		KickSquash(ArriveSquash);
		if (const AOfficeworker* Worker = GetWorker())
		{
			const float HalfHeight = Worker->GetCapsuleComponent() ? Worker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 92.f * GetFxScale();
			SpawnDust(Worker->GetActorLocation() - FVector(0.f, 0.f, HalfHeight), EmployeeReactionTuning::DustLandCount, EmployeeReactionTuning::DustLandSpeed);
		}
	}
}

//========================================
// 끌어서 데려오기
//========================================

bool UEmployeeReactionComponent::CanBeginCarry() const
{
	const AOfficeworker* Worker = GetWorker();
	const UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
	if (!Behavior || !GetBody() || Worker->bIsPortraitMode || Worker->IsHidden() || IsCarryActive() || DashStartTime >= 0.0)
	{
		return false;
	}
	if (Behavior->GetBehaviorMode() != EEmployeeBehaviorMode::Stage || Behavior->GetFatigueSlackPhase() != EFatigueSlackPhase::Bolting)
	{
		return false;
	}
	// 패널이 열린 동안(UI 모드)은 월드 조작을 받지 않는다
	const AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	return !PC || PC->GetCurrentInputMode() != EInputMode::UI;
}

bool UEmployeeReactionComponent::BeginCarry(bool bFollowPointer)
{
	using namespace EmployeeReactionTuning;
	AOfficeworker* Worker = GetWorker();
	UWorld* CurrentWorld = GetWorld();
	if (!Worker || !CurrentWorld || !CanBeginCarry()) return false;

	if (IsMotionReduced())
	{
		CarryPhase = EEmployeeCarryPhase::Reduced;
		return true;
	}

	const double Now = CurrentWorld->GetTimeSeconds();
	const float FxScale = GetFxScale();
	StartTicking();
	PauseWorkerForCarry();

	CarryPhase = EEmployeeCarryPhase::Held;
	bCarryFollowPointer = bFollowPointer;
	CarryFloorZ = static_cast<float>(Worker->GetActorLocation().Z);
	CarryTarget = Worker->GetActorLocation() + FVector(0.f, 0.f, CarryLiftHeight * FxScale);
	if (bFollowPointer)
	{
		FVector PointerTarget;
		if (ResolveCarryPointerTarget(static_cast<float>(CarryTarget.Z), PointerTarget))
		{
			CarryTarget = ClampCarryTarget(PointerTarget);
		}
	}
	// 집는 프레임에 바로 들어 올린다 — 첫 틱 이동량이 흔들림 속도로 잡히지 않게 여기서 옮긴다
	Worker->SetActorLocation(CarryTarget, false, nullptr, ETeleportType::TeleportPhysics);
	CarryLastLocation = Worker->GetActorLocation();
	CarryVelocity = FVector::ZeroVector;
	CarryFallSpeed = 0.f;
	CarryTilt = 0.f;
	CarryTiltVelocity = 0.f;
	AfterDizzyTime = -1.0;
	NextCarrySweatTime = Now + CarrySweatInterval * 0.5f;
	NextCarryFaceTime = Now + CarryFaceRefresh;
	Hops.Reset();
	WakeStartTime = -1.0;
	PokeStartTime = -1.0;

	PlayFace(EWorkerFaceExpression::Panic, CarryFaceRefresh * 2.f);
	ShowExclamation(CarryExclamationTime);
	return true;
}

void UEmployeeReactionComponent::ReleaseCarry()
{
	if (CarryPhase == EEmployeeCarryPhase::Reduced)
	{
		CarryPhase = EEmployeeCarryPhase::None;
		CatchFromCarry(false);
		return;
	}
	if (CarryPhase != EEmployeeCarryPhase::Held) return;

	CarryPhase = EEmployeeCarryPhase::Falling;
	CarryFallSpeed = 0.f;
	CarryDropHeight = FMath::Max(0.f, static_cast<float>(CarryLastLocation.Z) - CarryFloorZ);
	DebugDragStartTime = -1.0;
	StartTicking();
}

void UEmployeeReactionComponent::UpdateCarry(float DeltaTime, double Now, FVector& InOutBodyOffset, float& InOutYawDeg, float& OutStretch, float& OutRunSpeed)
{
	using namespace EmployeeReactionTuning;
	AOfficeworker* Worker = GetWorker();
	const UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
	// 알림 없이 조건이 깨져도(초상화 전환·모드 이탈) 들고 있던 몸이 공중에 남지 않게 매 틱 대조한다
	if (!Behavior || Worker->bIsPortraitMode || Behavior->GetBehaviorMode() != EEmployeeBehaviorMode::Stage
		|| Behavior->GetFatigueSlackPhase() != EFatigueSlackPhase::Bolting)
	{
		AbortCarry();
		return;
	}
	if (CarryPhase == EEmployeeCarryPhase::Reduced) return;

	const float FxScale = GetFxScale();
	FVector NewLocation = CarryLastLocation;
	if (CarryPhase == EEmployeeCarryPhase::Held)
	{
		if (bCarryFollowPointer)
		{
			FVector PointerTarget;
			if (ResolveCarryPointerTarget(CarryFloorZ + CarryLiftHeight * FxScale, PointerTarget))
			{
				CarryTarget = ClampCarryTarget(PointerTarget);
			}
		}
		// 끄는 동안은 보간 없이 손가락에 붙는다
		NewLocation = CarryTarget;
		const float Wobble = FMath::Sin(static_cast<float>(Now) * CarryWobbleRate);
		OutStretch = CarryStretch + CarryStretchWobble * Wobble;
		InOutBodyOffset.Z += CarryBounce * FxScale * Wobble;
		if (Now >= NextCarrySweatTime)
		{
			NextCarrySweatTime = Now + CarrySweatInterval;
			SpawnDrops(GetHeadLocation() + FVector(0.f, 0.f, SweatUp * FxScale), 1, FMath::RandBool() ? 1.f : -1.f);
		}
	}
	else
	{
		CarryFallSpeed += CarryGravity * FxScale * DeltaTime;
		NewLocation.Z -= CarryFallSpeed * DeltaTime;
		if (NewLocation.Z <= CarryFloorZ)
		{
			LandCarry();
			return;
		}
	}

	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		CarryVelocity = (NewLocation - CarryLastLocation) / DeltaTime;
	}
	Worker->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	CarryLastLocation = Worker->GetActorLocation();

	// 다리는 ABP 를 안 고치고 달리기 동작을 공중에서 돌려 버둥거리게 한다
	OutRunSpeed = CarryLegRunSpeed;
	InOutYawDeg += CarryYawWiggleDeg * FMath::Sin(static_cast<float>(Now) * CarryWobbleRate);
	if (Now >= NextCarryFaceTime)
	{
		NextCarryFaceTime = Now + CarryFaceRefresh;
		PlayFace(EWorkerFaceExpression::Panic, CarryFaceRefresh * 2.f);
	}
}

FQuat UEmployeeReactionComponent::UpdateCarrySwing(float DeltaTime)
{
	using namespace EmployeeReactionTuning;
	const bool bHeld = CarryPhase == EEmployeeCarryPhase::Held;
	if (!IsCarryLifted() && FMath::Abs(CarryTilt) < 0.0005f && FMath::Abs(CarryTiltVelocity) < 0.005f)
	{
		CarryTilt = 0.f;
		CarryTiltVelocity = 0.f;
		return FQuat::Identity;
	}

	FVector Right, ForwardFlat;
	GetCameraAxes(Right, ForwardFlat);
	float TiltTarget = 0.f;
	if (bHeld)
	{
		// 시안: 화면 가로 속도(m/s)의 0.05배, ±0.6rad — 끄는 쪽 반대로 발이 처진다
		const float RightSpeed = static_cast<float>(FVector::DotProduct(CarryVelocity, Right)) / (100.f * GetFxScale());
		TiltTarget = -FMath::Clamp(RightSpeed * CarryTiltPerSpeed, -CarryTiltMax, CarryTiltMax);
	}
	const float Damping = bHeld ? CarryTiltDampingHeld : CarryTiltDampingFree;
	for (float Remaining = DeltaTime; Remaining > KINDA_SMALL_NUMBER; Remaining -= SpringStep)
	{
		const float Step = FMath::Min(Remaining, SpringStep);
		CarryTiltVelocity += (-(CarryTilt - TiltTarget) * CarryTiltStiffness - CarryTiltVelocity * Damping) * Step;
		CarryTilt += CarryTiltVelocity * Step;
	}
	CarryTilt = FMath::Clamp(CarryTilt, -CarryTiltMax * 1.3f, CarryTiltMax * 1.3f);

	const AOfficeworker* Worker = GetWorker();
	if (!Worker || FMath::Abs(CarryTilt) < 0.0005f) return FQuat::Identity;
	// 회전축 = 카메라 시선 — 화면 안에서 좌우로 흔들린다
	const FVector LocalAxis = Worker->GetActorTransform().InverseTransformVectorNoScale(GetCameraForward()).GetSafeNormal();
	return LocalAxis.IsNearlyZero() ? FQuat::Identity : FQuat(LocalAxis, CarryTilt);
}

bool UEmployeeReactionComponent::ResolveCarryPointerTarget(float PlaneZ, FVector& OutTarget)
{
	// 손을 막 뗀 프레임엔 터치 좌표가 비어 (0,0) 이 온다 — 그 프레임은 지난 위치를 유지한다
	const FVector2D Absolute = UGlobalUtilFunctions::GetPointerAbsolutePosition();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (Absolute.IsNearlyZero() || !PC) return false;

	FVector2D PixelPosition, ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(GetWorld(), Absolute, PixelPosition, ViewportPosition);
	FVector RayOrigin, RayDirection;
	if (!PC->DeprojectScreenPositionToWorld(static_cast<float>(PixelPosition.X), static_cast<float>(PixelPosition.Y), RayOrigin, RayDirection)
		|| RayDirection.Z > -KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// 손가락 광선 ∩ 들린 높이의 수평면 = 몸 중심이 손가락 아래에 온다
	const double Distance = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (Distance <= 0.0) return false;
	OutTarget = RayOrigin + RayDirection * Distance;
	return true;
}

FVector UEmployeeReactionComponent::ClampCarryTarget(const FVector& Target) const
{
	const UWorld* CurrentWorld = GetWorld();
	const AOfficeGameMode* OfficeGM = CurrentWorld ? CurrentWorld->GetAuthGameMode<AOfficeGameMode>() : nullptr;
	const AOfficeInterior* Interior = OfficeGM ? OfficeGM->GetOfficeInterior() : nullptr;
	if (!Interior) return Target;

	// 바닥 밖으로 끌어내 벽 뒤로 사라지지 않게 사무실 바닥(내비 범위) 안에 둔다
	const FBox Bounds = Interior->GetNavMeshBoundsBox();
	const double Margin = EmployeeReactionTuning::CarryBoundsMargin * GetFxScale();
	const FVector Size = Bounds.GetSize();
	if (!Bounds.IsValid || Size.X <= Margin * 2.0 || Size.Y <= Margin * 2.0) return Target;
	return FVector(FMath::Clamp(Target.X, Bounds.Min.X + Margin, Bounds.Max.X - Margin),
		FMath::Clamp(Target.Y, Bounds.Min.Y + Margin, Bounds.Max.Y - Margin), Target.Z);
}

void UEmployeeReactionComponent::LandCarry()
{
	using namespace EmployeeReactionTuning;
	AOfficeworker* Worker = GetWorker();
	UWorld* CurrentWorld = GetWorld();
	if (!Worker || !CurrentWorld)
	{
		AbortCarry();
		return;
	}

	const double Now = CurrentWorld->GetTimeSeconds();
	const float FxScale = GetFxScale();
	const FVector Landing(CarryLastLocation.X, CarryLastLocation.Y, CarryFloorZ);
	Worker->SetActorLocation(Landing, false, nullptr, ETeleportType::TeleportPhysics);
	CarryLastLocation = Worker->GetActorLocation();
	// 잡기가 단계를 바꾸며 알림을 쏘기 전에 끌기를 닫는다 — 알림 쪽 중단 분기가 자기 착지를 끊지 않게
	CarryPhase = EEmployeeCarryPhase::None;
	ResumeWorkerAfterCarry();

	// 시안 land: 높이 비례 눌림(1−0.3h) + 바닥 먼지 링 + 70cm 넘게 떨어지면 어지러움 뒤 삐침, 아니면 삐침
	const float DropCm = CarryDropHeight / FxScale;
	const float DropScale = FMath::Clamp(DropCm / CarryDropReference, 0.3f, 1.2f);
	if (!IsMotionReduced())
	{
		KickSquash(1.f - 0.3f * DropScale);
		SpawnDust(Landing - FVector(0.f, 0.f, GetCapsuleHalfHeight()), DustLandCount, DustLandSpeed * DropScale / 0.6f);
	}
	if (DropCm > CarryDizzyHeight)
	{
		PlayFace(EWorkerFaceExpression::Dizzy, CarryDizzyTime);
		AfterDizzyTime = Now + CarryDizzyTime;
	}
	else
	{
		PlayFace(EWorkerFaceExpression::Sulky, CarrySulkyTime);
		AfterDizzyTime = -1.0;
	}
	CatchFromCarry(true);
}

void UEmployeeReactionComponent::CatchFromCarry(bool bDropped)
{
	AOfficeworker* Worker = GetWorker();
	UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
	if (!Behavior) return;

	// 잡기 효과(피로·장비 기록·자리 복귀)는 탭 잡기와 같은 한 경로로 — 복귀 모양만 bCarryDropped 로 갈린다
	const EFatigueSlackPhase CaughtPhase = Behavior->GetFatigueSlackPhase();
	bCarryDropped = bDropped;
	const bool bCaught = Behavior->TryManualCatch();
	bCarryDropped = false;
	if (bCaught)
	{
		OnCarryCaught.Broadcast(CaughtPhase);
		return;
	}
	// 잡기가 거절되면(장비 판정) 폭주를 그대로 이어 간다 — 들 때 멈춘 배회를 다시 건다
	if (bDropped && Behavior->GetFatigueSlackPhase() == EFatigueSlackPhase::Bolting)
	{
		Worker->ResumeRandomRoaming();
	}
}

void UEmployeeReactionComponent::AbortCarry()
{
	if (CarryPhase == EEmployeeCarryPhase::None) return;
	const bool bLifted = IsCarryLifted();
	CarryPhase = EEmployeeCarryPhase::None;
	DebugDragStartTime = -1.0;

	// 다른 경로가 이미 자리로 옮겼으면 그대로 두고, 아니면 들고 있던 자리 바닥에 내려놓는다
	AOfficeworker* Worker = GetWorker();
	if (bLifted && Worker && FVector::DistSquared(Worker->GetActorLocation(), CarryLastLocation) < 1.0)
	{
		Worker->SetActorLocation(FVector(CarryLastLocation.X, CarryLastLocation.Y, CarryFloorZ), false, nullptr, ETeleportType::TeleportPhysics);
	}
	ResumeWorkerAfterCarry();
}

void UEmployeeReactionComponent::PauseWorkerForCarry()
{
	AOfficeworker* Worker = GetWorker();
	UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
	if (!Behavior || bCarryPaused) return;

	// 행동 틱이 돌면 폭주 시간이 다 돼 들고 있는 몸을 자리로 순간 이동시킨다 — 직원 살펴보기(FOfficeEmployeeInspect)와 같은 정지
	Worker->StopRandomRoaming();
	if (AAIController* AIC = Cast<AAIController>(Worker->GetController()))
	{
		AIC->StopMovement();
	}
	// 이동 모드는 그대로 두고 틱만 끈다 — MOVE_None 으로 두면 착석 텔레포트가 좌석 잠금을 안 걸어 나중에 못 걷는다
	if (UCharacterMovementComponent* Movement = Worker->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		bCarryMoveTickWasEnabled = Movement->IsComponentTickEnabled();
		Movement->SetComponentTickEnabled(false);
	}
	bCarryBehaviorTickWasEnabled = Behavior->IsComponentTickEnabled();
	Behavior->SetComponentTickEnabled(false);
	bCarryPaused = true;
}

void UEmployeeReactionComponent::ResumeWorkerAfterCarry()
{
	if (!bCarryPaused) return;
	bCarryPaused = false;
	AOfficeworker* Worker = GetWorker();
	if (!Worker) return;
	if (UCharacterMovementComponent* Movement = Worker->GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(bCarryMoveTickWasEnabled);
	}
	if (UEmployeeBehaviorComponent* Behavior = Worker->BehaviorComponent)
	{
		Behavior->SetComponentTickEnabled(bCarryBehaviorTickWasEnabled);
	}
}

bool UEmployeeReactionComponent::StartDebugDrag(bool bFar)
{
	using namespace EmployeeReactionTuning;
	AOfficeworker* Worker = GetWorker();
	UEmployeeBehaviorComponent* Behavior = Worker ? Worker->BehaviorComponent : nullptr;
	UWorld* CurrentWorld = GetWorld();
	if (!Behavior || !CurrentWorld || IsCarryActive() || DebugDragArmTime >= 0.0
		|| Behavior->GetBehaviorMode() != EEmployeeBehaviorMode::Stage)
	{
		return false;
	}

	float Wait = DebugDragPressDelay;
	if (Behavior->GetFatigueSlackPhase() == EFatigueSlackPhase::None)
	{
		// 시안 대본처럼 먼저 자리를 박차고 잠깐 돌아다니게 한 뒤 집는다
		Behavior->ForceSlackBolt();
		Wait += DebugDragBoltWait;
	}
	if (Behavior->GetFatigueSlackPhase() != EFatigueSlackPhase::Bolting) return false;

	bDebugDragFar = bFar;
	DebugDragArmTime = CurrentWorld->GetTimeSeconds() + Wait;
	DebugDragStartTime = -1.0;
	StartTicking();
	return true;
}

void UEmployeeReactionComponent::UpdateDebugDrag(double Now)
{
	using namespace EmployeeReactionTuning;
	AOfficeworker* Worker = GetWorker();
	if (!Worker)
	{
		DebugDragArmTime = -1.0;
		DebugDragStartTime = -1.0;
		return;
	}
	const float FxScale = GetFxScale();

	if (DebugDragArmTime >= 0.0)
	{
		if (Now < DebugDragArmTime) return;
		DebugDragArmTime = -1.0;
		if (!BeginCarry(false))
		{
			UE_LOG(LogTemp, Warning, TEXT("[EmployeeReaction] drag: 직원 %d 를 들 수 없다(폭주 중이 아니거나 패널이 열림)"), Worker->GetEmployeeID());
			return;
		}
		if (CarryPhase == EEmployeeCarryPhase::Reduced)
		{
			ReleaseCarry();
			return;
		}

		FVector Right, ForwardFlat;
		GetCameraAxes(Right, ForwardFlat);
		DebugDragFrom = CarryTarget;
		const AWorkstationActorBase* Desk = Worker->GetAssignedWorkstation();
		const int32 SeatIndex = Worker->GetAssignedSeatIndex();
		DebugDragTo = bDebugDragFar ? DebugDragFrom + Right * (DebugDragFarDistance * FxScale)
			: (Desk && SeatIndex >= 0 ? Desk->GetSeatLocation(SeatIndex) : Worker->GetWorkstationApproachLocation());
		DebugDragTo.Z = DebugDragFrom.Z;
		DebugDragTo = ClampCarryTarget(DebugDragTo);
		DebugDragMid = (DebugDragFrom + DebugDragTo) * 0.5 + Right * (DebugDragSide * FxScale) + ForwardFlat * (DebugDragDepth * FxScale);
		DebugDragStartTime = Now;
		return;
	}

	if (DebugDragStartTime < 0.0) return;
	if (CarryPhase != EEmployeeCarryPhase::Held)
	{
		DebugDragStartTime = -1.0;
		return;
	}
	// 시안 dragHome: eInOut 으로 휜 2차 곡선 + 위로 솟는 호, 끝에서 놓는다
	const float K = FMath::Clamp(static_cast<float>((Now - DebugDragStartTime) / DebugDragDuration), 0.f, 1.f);
	const float Ease = K < 0.5f ? 4.f * K * K * K : 1.f - FMath::Pow(-2.f * K + 2.f, 3.f) * 0.5f;
	const FVector First = FMath::Lerp(DebugDragFrom, DebugDragMid, Ease);
	const FVector Second = FMath::Lerp(DebugDragMid, DebugDragTo, Ease);
	CarryTarget = FMath::Lerp(First, Second, Ease);
	CarryTarget.Z = CarryFloorZ + (CarryLiftHeight + FMath::Sin(PI * K) * DebugDragArc) * FxScale;
	if (K >= 1.f)
	{
		DebugDragStartTime = -1.0;
		ReleaseCarry();
	}
}

//========================================
// 졸음 표시 (콧방울 + Z)
//========================================

void UEmployeeReactionComponent::SetDozeVisual(EEmployeeDozeVisual NewVisual, bool bPreview)
{
	using namespace EmployeeReactionTuning;
	const UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld) return;

	const EEmployeeDozeVisual OldVisual = DozeVisual;
	DozeVisual = NewVisual;
	bDozePreview = bPreview && NewVisual != EEmployeeDozeVisual::None;
	if (NewVisual == EEmployeeDozeVisual::None)
	{
		// 터지는 중인 콧방울은 UpdateBubblePop 이 거둔다
		if (BubblePopStartTime < 0.0)
		{
			HideSnotBubble();
		}
		return;
	}

	StartTicking();
	if (OldVisual == EEmployeeDozeVisual::None)
	{
		NextZTime = CurrentWorld->GetTimeSeconds() + ZFirstDelay;
	}
	if (NewVisual == EEmployeeDozeVisual::Sleeping)
	{
		ShowSnotBubble();
	}
	else
	{
		HideSnotBubble();
	}
}

void UEmployeeReactionComponent::UpdateDoze(double Now, bool bReduced)
{
	using namespace EmployeeReactionTuning;
	if (Now >= NextZTime)
	{
		NextZTime = Now + (DozeVisual == EEmployeeDozeVisual::Sleeping ? ZIntervalSleeping : ZIntervalNodding);
		SpawnZ(bReduced);
	}

	if (DozeVisual != EEmployeeDozeVisual::Sleeping || !IsSnotBubbleShown() || BubblePopStartTime >= 0.0) return;

	const float Pulse = bReduced ? 0.5f : FMath::Square(FMath::Sin(PI * FMath::Frac(static_cast<float>(Now) * BubblePulseHz)));
	const float Radius = FMath::Lerp(BubbleMinRadius, BubbleMaxRadius, Pulse);
	// 콧구멍에서 부푸는 모양 — 반지름만큼 앞·아래로 밀어 얼굴을 파고들지 않게 한다
	const FVector NoseSpace(NoseX, NoseY + Radius * 0.75f, NoseZ - Radius * 0.25f);
	SnotBubble->SetRelativeLocation(HeadReferencePose.InverseTransformPosition(NoseSpace));
	SnotBubble->SetRelativeScale3D(BubbleWorldScale(Radius));
}

bool UEmployeeReactionComponent::ResolveHeadReference()
{
	if (bHeadReferenceResolved) return true;
	const USkeletalMeshComponent* Body = GetBody();
	const USkeletalMesh* BodyMesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!BodyMesh) return false;

	const FReferenceSkeleton& RefSkeleton = BodyMesh->GetRefSkeleton();
	const int32 HeadIndex = RefSkeleton.FindBoneIndex(FName(EmployeeReactionAssets::HeadBone));
	if (HeadIndex == INDEX_NONE) return false;

	// 기준 포즈의 머리 본 컴포넌트 공간 변환 — 얼굴 판 좌표(코끝)를 본 공간으로 옮기는 데 쓴다
	const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
	FTransform ComponentSpace = FTransform::Identity;
	for (int32 BoneIndex = HeadIndex; BoneIndex != INDEX_NONE; BoneIndex = RefSkeleton.GetParentIndex(BoneIndex))
	{
		ComponentSpace = ComponentSpace * RefPose[BoneIndex];
	}
	HeadReferencePose = ComponentSpace;
	bHeadReferenceResolved = true;
	return true;
}

FVector UEmployeeReactionComponent::BubbleWorldScale(float Radius) const
{
	const USkeletalMeshComponent* Body = GetBody();
	const float BodyScale = Body ? Body->GetComponentScale().X : 1.f;
	return FVector(Radius / EmployeeReactionTuning::SphereMeshRadius * BodyScale);
}

bool UEmployeeReactionComponent::ShowSnotBubble()
{
	USkeletalMeshComponent* Body = GetBody();
	if (!Body || !ResolveHeadReference()) return false;

	if (!SnotBubble)
	{
		UStaticMesh* Sphere = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(EmployeeReactionAssets::SphereMeshPath)).LoadSynchronous();
		UMaterialInterface* BubbleMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(EmployeeReactionAssets::BubbleMaterialPath)).LoadSynchronous();
		if (!Sphere || !BubbleMaterial)
		{
			EmployeeReactionAssets::WarnMissingOnce(EmployeeReactionAssets::BubbleMaterialPath);
			return false;
		}
		SnotBubble = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
		SnotBubble->SetStaticMesh(Sphere);
		SnotBubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SnotBubble->SetGenerateOverlapEvents(false);
		SnotBubble->SetCastShadow(false);
		SnotBubble->SetReceivesDecals(false);
		SnotBubble->SetCanEverAffectNavigation(false);
		SnotBubble->SetupAttachment(Body, FName(EmployeeReactionAssets::HeadBone));
		// 머리 본은 스켈레톤 축척을 물려받아 상대 크기로 두면 방 절반만큼 커진다 — 크기만 월드 기준으로 잡는다
		SnotBubble->SetUsingAbsoluteScale(true);
		SnotBubble->RegisterComponent();
		SnotBubbleMID = UMaterialInstanceDynamic::Create(BubbleMaterial, this);
		SnotBubble->SetMaterial(0, SnotBubbleMID);
	}
	if (SnotBubbleMID)
	{
		SnotBubbleMID->SetScalarParameterValue(TEXT("Opacity"), EmployeeReactionTuning::BubbleOpacity);
	}
	SnotBubble->SetRelativeScale3D(BubbleWorldScale(EmployeeReactionTuning::BubbleMinRadius));
	SnotBubble->SetVisibility(true);
	return true;
}

void UEmployeeReactionComponent::HideSnotBubble()
{
	if (SnotBubble && SnotBubble->IsVisible())
	{
		SnotBubble->SetVisibility(false);
	}
}

bool UEmployeeReactionComponent::IsSnotBubbleShown() const
{
	return SnotBubble && SnotBubble->IsVisible();
}

void UEmployeeReactionComponent::StartBubblePop(bool bWithDrops)
{
	const UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || !IsSnotBubbleShown()) return;
	StartTicking();
	if (BubblePopStartTime < 0.0)
	{
		BubblePopStartTime = CurrentWorld->GetTimeSeconds();
	}
	if (bWithDrops && !IsMotionReduced())
	{
		SpawnDrops(SnotBubble->GetComponentLocation(), 6, 1.f);
	}
}

void UEmployeeReactionComponent::UpdateBubblePop(double Now)
{
	using namespace EmployeeReactionTuning;
	const float T = static_cast<float>((Now - BubblePopStartTime) / BubblePopTime);
	if (T >= 1.f || !SnotBubble)
	{
		BubblePopStartTime = -1.0;
		HideSnotBubble();
		return;
	}
	if (SnotBubbleMID)
	{
		SnotBubbleMID->SetScalarParameterValue(TEXT("Opacity"), BubbleOpacity * (1.f - T));
	}
	if (!IsMotionReduced())
	{
		// 시안: 0.12초 동안 1.7배로 부풀며 사라진다
		const float PopRadius = FMath::Lerp(BubbleMinRadius, BubbleMaxRadius, 0.5f) * (1.f + 0.7f * T);
		SnotBubble->SetRelativeScale3D(BubbleWorldScale(PopRadius));
	}
}

//========================================
// 빌보드 효과 (Z · 땀 · 김)
//========================================

bool UEmployeeReactionComponent::LoadFxAssets()
{
	if (bFxAssetsTried) return SpriteMaterial != nullptr && SpriteTextures.Num() == 3;
	bFxAssetsTried = true;

	SpriteMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(EmployeeReactionAssets::SpriteMaterialPath)).LoadSynchronous();
	SpriteTextures.Reset();
	for (const TCHAR* TexturePath : { EmployeeReactionAssets::ZTexturePath, EmployeeReactionAssets::DropTexturePath, EmployeeReactionAssets::SteamTexturePath })
	{
		if (UTexture2D* Texture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TexturePath)).LoadSynchronous())
		{
			SpriteTextures.Add(Texture);
		}
	}
	if (!SpriteMaterial || SpriteTextures.Num() != 3)
	{
		EmployeeReactionAssets::WarnMissingOnce(EmployeeReactionAssets::SpriteMaterialPath);
		return false;
	}
	return true;
}

int32 UEmployeeReactionComponent::AcquireSprite()
{
	using namespace EmployeeReactionTuning;
	for (int32 Index = 0; Index < SpriteStates.Num(); ++Index)
	{
		if (SpriteStates[Index].Kind == EEmployeeReactionSprite::None) return Index;
	}
	if (SpritePool.Num() < MaxSprites)
	{
		UMaterialBillboardComponent* Sprite = NewObject<UMaterialBillboardComponent>(GetOwner(), NAME_None, RF_Transient);
		Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sprite->SetGenerateOverlapEvents(false);
		Sprite->SetCastShadow(false);
		Sprite->SetReceivesDecals(false);
		Sprite->SetCanEverAffectNavigation(false);
		Sprite->SetTranslucentSortPriority(10);
		Sprite->SetUsingAbsoluteLocation(true);
		Sprite->SetUsingAbsoluteRotation(true);
		Sprite->SetUsingAbsoluteScale(true);
		Sprite->RegisterComponent();
		SpritePool.Add(Sprite);
		SpriteMIDs.Add(UMaterialInstanceDynamic::Create(SpriteMaterial, this));
		SpriteStates.AddDefaulted();
		return SpriteStates.Num() - 1;
	}
	// 칸이 다 차면 가장 오래 산 효과를 재사용한다
	int32 Oldest = 0;
	float OldestAge = -1.f;
	for (int32 Index = 0; Index < SpriteStates.Num(); ++Index)
	{
		const float Age01 = SpriteStates[Index].Age / FMath::Max(0.01f, SpriteStates[Index].Life);
		if (Age01 > OldestAge)
		{
			OldestAge = Age01;
			Oldest = Index;
		}
	}
	return Oldest;
}

void UEmployeeReactionComponent::SpawnSprite(EEmployeeReactionSprite Kind, const FVector& Location, const FVector& Velocity, float Life, bool bStill)
{
	using namespace EmployeeReactionTuning;
	if (Kind == EEmployeeReactionSprite::None || !LoadFxAssets()) return;

	const int32 Index = AcquireSprite();
	if (!SpritePool.IsValidIndex(Index) || !SpriteMIDs.IsValidIndex(Index) || !SpriteStates.IsValidIndex(Index)) return;
	UMaterialBillboardComponent* Sprite = SpritePool[Index];
	UMaterialInstanceDynamic* SpriteMID = SpriteMIDs[Index];
	if (!Sprite || !SpriteMID) return;

	const float FxScale = GetFxScale();
	float QuadSize = DropSize;
	int32 TextureIndex = 1;
	switch (Kind)
	{
	case EEmployeeReactionSprite::Z:     QuadSize = FMath::FRandRange(ZSizeMin, ZSizeMax) * 1.5f; TextureIndex = 0; break;
	case EEmployeeReactionSprite::Drop:  QuadSize = DropSize; TextureIndex = 1; break;
	case EEmployeeReactionSprite::Steam: QuadSize = SteamSize; TextureIndex = 2; break;
	case EEmployeeReactionSprite::Dust:  QuadSize = DustSize; TextureIndex = 2; break;
	default: break;
	}

	FEmployeeReactionSpriteState& State = SpriteStates[Index];
	State.Kind = Kind;
	State.Location = Location;
	State.Velocity = Velocity;
	State.Age = 0.f;
	State.Life = FMath::Max(0.05f, Life);
	State.Phase = FMath::FRandRange(0.f, 2.f * PI);
	State.bStill = bStill;

	SpriteMID->SetTextureParameterValue(TEXT("SpriteTex"), SpriteTextures[TextureIndex]);
	SpriteMID->SetScalarParameterValue(TEXT("Opacity"), 0.f);
	SpriteMID->SetScalarParameterValue(TEXT("SpriteScale"), 1.f);
	SpriteMID->SetScalarParameterValue(TEXT("SpriteRotation"), 0.f);

	// 크기는 칸을 처음 쓸 때만 바꾼다(SetElements = 렌더 상태 재생성). 자라는 크기는 재질 UV 배율이 맡는다.
	FMaterialSpriteElement Element;
	Element.Material = SpriteMID;
	Element.bSizeIsInScreenSpace = false;
	Element.BaseSizeX = QuadSize * FxScale;
	Element.BaseSizeY = QuadSize * FxScale;
	Sprite->SetElements({ Element });
	Sprite->SetWorldLocation(Location);
	Sprite->SetVisibility(true);
	StartTicking();
}

void UEmployeeReactionComponent::SpawnDrops(const FVector& Origin, int32 Count, float Side)
{
	using namespace EmployeeReactionTuning;
	FVector Right, ForwardFlat;
	GetCameraAxes(Right, ForwardFlat);
	const float FxScale = GetFxScale();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = (Side >= 0.f ? 0.f : PI) + FMath::FRandRange(-0.8f, 0.8f);
		const FVector Horizontal = (Right * FMath::Cos(Angle) + ForwardFlat * (FMath::Sin(Angle) * 0.5f)).GetSafeNormal();
		const FVector Velocity = Horizontal * FMath::FRandRange(DropSideMin, DropSideMax) * FxScale
			+ FVector(0.f, 0.f, FMath::FRandRange(DropUpMin, DropUpMax) * FxScale);
		SpawnSprite(EEmployeeReactionSprite::Drop, Origin, Velocity, DropLife, false);
	}
}

void UEmployeeReactionComponent::SpawnSteamPuffs(bool bStill)
{
	using namespace EmployeeReactionTuning;
	FVector Right, ForwardFlat;
	GetCameraAxes(Right, ForwardFlat);
	const float FxScale = GetFxScale();
	const FVector Center = GetHeadLocation() + FVector(0.f, 0.f, SteamUp * FxScale);
	for (const float Side : { -1.f, 1.f })
	{
		const FVector Velocity = bStill ? FVector::ZeroVector
			: FVector(0.f, 0.f, SteamRise * FxScale) + Right * (Side * SteamDrift * FxScale);
		SpawnSprite(EEmployeeReactionSprite::Steam, Center + Right * (Side * SteamSide * FxScale), Velocity, SteamLife, bStill);
	}
}

void UEmployeeReactionComponent::SpawnZ(bool bStill)
{
	using namespace EmployeeReactionTuning;
	FVector Right, ForwardFlat;
	GetCameraAxes(Right, ForwardFlat);
	const float FxScale = GetFxScale();
	const FVector Location = GetHeadLocation() + FVector(0.f, 0.f, ZSpawnUp * FxScale) + Right * (ZSpawnSide * FxScale);
	SpawnSprite(EEmployeeReactionSprite::Z, Location, FVector::ZeroVector, ZLife, bStill);
}

void UEmployeeReactionComponent::UpdateSprites(float DeltaTime, double Now)
{
	using namespace EmployeeReactionTuning;
	if (!HasActiveSprites()) return;

	FVector Right, ForwardFlat;
	GetCameraAxes(Right, ForwardFlat);
	const float FxScale = GetFxScale();
	for (int32 Index = 0; Index < SpriteStates.Num(); ++Index)
	{
		FEmployeeReactionSpriteState& State = SpriteStates[Index];
		if (State.Kind == EEmployeeReactionSprite::None) continue;
		UMaterialBillboardComponent* Sprite = SpritePool.IsValidIndex(Index) ? SpritePool[Index].Get() : nullptr;
		UMaterialInstanceDynamic* SpriteMID = SpriteMIDs.IsValidIndex(Index) ? SpriteMIDs[Index].Get() : nullptr;
		State.Age += DeltaTime;
		const float T = State.Age / State.Life;
		if (T >= 1.f || !Sprite || !SpriteMID)
		{
			State.Kind = EEmployeeReactionSprite::None;
			if (Sprite)
			{
				Sprite->SetVisibility(false);
			}
			continue;
		}

		float Opacity = 1.f;
		float Scale01 = 1.f;
		float Rotation = 0.f;
		switch (State.Kind)
		{
		case EEmployeeReactionSprite::Z:
			// 시안: 위로 떠오르며 좌우로 흔들리고 0.6→1.5배로 자란다. 처음 15% 페이드 인, 이후 페이드 아웃.
			if (!State.bStill)
			{
				State.Location.Z += ZRiseSpeed * FxScale * DeltaTime;
				State.Location += Right * (FMath::Cos(static_cast<float>(Now) * 3.f + State.Phase) * ZSwaySpeed * FxScale * DeltaTime);
				Scale01 = (0.6f + 0.9f * T) / 1.5f;
				Rotation = FMath::Sin(T * 6.f + State.Phase) * 0.3f;
			}
			else
			{
				Scale01 = 0.7f;
			}
			Opacity = T < 0.15f ? T / 0.15f : (1.f - T) / 0.85f;
			break;
		case EEmployeeReactionSprite::Drop:
			State.Velocity.Z -= DropGravity * FxScale * DeltaTime;
			State.Location += State.Velocity * DeltaTime;
			Opacity = T < 0.7f ? 1.f : (1.f - T) / 0.3f;
			break;
		case EEmployeeReactionSprite::Steam:
			State.Location += State.Velocity * DeltaTime;
			Scale01 = State.bStill ? 0.75f : (0.22f + 0.2f * T) / 0.42f;
			Opacity = 1.f - T;
			break;
		case EEmployeeReactionSprite::Dust:
			State.Location += State.Velocity * ((1.f - T) * DeltaTime);
			State.Location.Z += 25.f * FxScale * DeltaTime;
			Scale01 = 0.55f + 0.45f * T;
			Opacity = FMath::Pow(1.f - T, 1.4f) * 0.95f;
			break;
		default:
			break;
		}

		Sprite->SetWorldLocation(State.Location);
		SpriteMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Opacity, 0.f, 1.f));
		SpriteMID->SetScalarParameterValue(TEXT("SpriteScale"), FMath::Max(0.05f, Scale01));
		SpriteMID->SetScalarParameterValue(TEXT("SpriteRotation"), Rotation);
	}
}

bool UEmployeeReactionComponent::HasActiveSprites() const
{
	for (const FEmployeeReactionSpriteState& State : SpriteStates)
	{
		if (State.Kind != EEmployeeReactionSprite::None) return true;
	}
	return false;
}

void UEmployeeReactionComponent::SpawnDust(const FVector& FloorLocation, int32 Count, float Speed)
{
	using namespace EmployeeReactionTuning;
	const float FxScale = GetFxScale();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = (static_cast<float>(Index) / FMath::Max(1, Count) + FMath::FRandRange(0.f, 0.15f)) * 2.f * PI;
		const FVector Velocity = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (Speed * FMath::FRandRange(0.7f, 1.3f) * FxScale);
		SpawnSprite(EEmployeeReactionSprite::Dust, FloorLocation + FVector(0.f, 0.f, 6.f * FxScale), Velocity, DustLife, false);
	}
}

//========================================
// 검증 콘솔 명령
//========================================

#if !UE_BUILD_SHIPPING
namespace EmployeeReactionConsole
{
	static AOfficeworker* FindReactionTarget(UWorld* InWorld, int32 Index)
	{
		TArray<AOfficeworker*> Workers;
		if (InWorld)
		{
			for (TActorIterator<AOfficeworker> It(InWorld); It; ++It)
			{
				if (!It->bIsPortraitMode && It->FindComponentByClass<UEmployeeReactionComponent>())
				{
					Workers.Add(*It);
				}
			}
		}
		Workers.Sort([](const AOfficeworker& A, const AOfficeworker& B) { return A.GetEmployeeID() < B.GetEmployeeID(); });
		return Workers.IsValidIndex(Index) ? Workers[Index] : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs GEmployeeReactCommand(
		TEXT("CGR.Employee.React"),
		TEXT("직원 몸 반응을 강제로 재생한다. CGR.Employee.React <wake|bolt|poke|crit|mile|doze|drag> [직원 순번(직원ID 순, 0부터)] [drag: far = 자리에서 먼 곳에 놓기]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* InWorld)
		{
			if (Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[EmployeeReaction] 사용법: CGR.Employee.React <wake|bolt|poke|crit|mile|doze|drag> [순번] [far]"));
				return;
			}
			const int32 TargetIndex = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
			AOfficeworker* Worker = FindReactionTarget(InWorld, TargetIndex);
			UEmployeeReactionComponent* Reaction = Worker ? Worker->FindComponentByClass<UEmployeeReactionComponent>() : nullptr;
			if (!Reaction)
			{
				UE_LOG(LogTemp, Warning, TEXT("[EmployeeReaction] %d 번째 사무실 직원이 없다"), TargetIndex);
				return;
			}
			const bool bPlayed = Reaction->PlayDebugReaction(Args[0], Args.Num() > 2 ? Args[2] : FString());
			UE_LOG(LogTemp, Log, TEXT("[EmployeeReaction] %s → 직원 %d : %s"), *Args[0], Worker->GetEmployeeID(),
				bPlayed ? TEXT("재생") : TEXT("재생 안 함(종류 오타이거나 다른 반응 중)"));
		}));
}
#endif
