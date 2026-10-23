#include "Office/WorkstationDevFxComponent.h"
#include "Office/WorkstationActorBase.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Entity/Officeworker/EmployeeReactionComponent.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SaveLoadManager.h"
#include "Core/CGGameInstance.h"
#include "Data/GameSaveData.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Player/OfficeCameraPawn.h"
#include "UI/UISoundTags.h"
#include "Global/GlobalUtilFunctions.h"
#include "Data/EmployeeTypes.h"
#include "Data/StageProgressData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

// 시안 OfficeDevJuice_2026-10-17 의 값(미터)을 cm 로 옮겼다. 길이는 좌석 축척(좌석→모니터 1m = 1)을 곱해 쓴다.
namespace DevFxTune
{
	constexpr float WaveFirstDelay = 0.15f;
	constexpr float WaveStep = 0.09f;
	constexpr float MonitorOnSeconds = 0.18f;
	constexpr float FlashSeconds = 0.25f;
	constexpr float FlashGain = 0.5f;
	constexpr float TierOpacity[3] = { 0.22f, 0.42f, 0.62f };
	constexpr float AsleepOpacity = 0.07f;
	constexpr float AwayOpacity = 0.1f;
	constexpr float GlowRatio = 0.55f;
	constexpr float OpacityFollow = 8.f;
	constexpr float HiddenOpacity = 0.003f;

	constexpr int32 FlowTier1Streak = 3;
	constexpr int32 FlowTier2Streak = 6;
	constexpr float FlowTypingRate = 1.33f;
	constexpr float FocusFaceSeconds = 2.4f;

	constexpr float KeycapInterval = 0.22f;
	constexpr float KeycapLife = 0.4f;
	constexpr int32 MaxKeycaps = 3;
	constexpr float KeycapGravity = 700.f;
	constexpr float KeycapRise = 130.f;
	constexpr float KeycapSpread = 30.f;
	constexpr float KeycapReach = 22.f;

	constexpr float TileFlight = 0.26f;
	constexpr float TileLand = 0.2f;
	constexpr float TileArc = 32.f;
	constexpr float TileSquash = 0.3f;
	constexpr float CritTileScale = 1.35f;
	constexpr float CritGoldMix = 0.55f;
	constexpr float StackBumpSeconds = 0.18f;
	constexpr float StackBump = 0.06f;
	constexpr int32 MaxStack = 7;
	constexpr int32 MaxTiles = 10;
	constexpr float TileBaseHeight = 4.f;
	constexpr float TileStepHeight = 7.5f;

	constexpr float NoteFold = 0.2f;
	constexpr float NoteFlight = 0.3f;
	constexpr float NoteTotal = 0.82f;
	constexpr float NoteArcUp = 70.f;
	constexpr float NoteRestRoll = -0.08f;
	constexpr float NoteFoldRise = 10.f;
	constexpr float CheerWobbleSeconds = 1.6f;

	constexpr float ChairPushCm = 42.f;
	constexpr float ChairSpinSeconds = 0.8f;
	constexpr float ChairFollow = 10.f;
	constexpr float ChairSpinStagger = 0.05f;
	constexpr float ChairSpinWaitSeconds = 1.2f;
	constexpr float CheerFaceSeconds = 6.f;
	constexpr float CheerTileSeconds = 0.25f;
	constexpr float CheerTileRise = 40.f;

	constexpr float ContributorMatchCm = 50.f;
	constexpr float OrbLift = 6.f;
	constexpr float WaveTickVolume = 0.55f;
	constexpr float WaveTickPitch = 1.15f;
	constexpr double PendingRestoreSeconds = 5.0;
}

namespace DevFxParam
{
	const FName Opacity(TEXT("Opacity"));
	const FName GlowColor(TEXT("GlowColor"));
	const FName TileColor(TEXT("TileColor"));
	const FName GoldMix(TEXT("GoldMix"));
	const FName GlyphCell(TEXT("GlyphCell"));
	const FName GlyphOn(TEXT("GlyphOn"));
	const FName NoteColor(TEXT("NoteColor"));
}

namespace DevFxUtil
{
	static bool IsDevelopmentRunning(UWorld* InWorld)
	{
		if (!InWorld) return false;
		const UOfficeStageProgressManager* StageMgr = InWorld->GetSubsystem<UOfficeStageProgressManager>();
		if (StageMgr && StageMgr->IsTimerRunning()) return true;
		const UCGGameInstance* GI = Cast<UCGGameInstance>(InWorld->GetGameInstance());
		if (!GI || GI->IsVisitMode() || !IsManufacturingType(GI->GetCurrentBuildingCompanyType())) return false;
		USaveLoadManager* Saves = GI->GetSubsystem<USaveLoadManager>();
		const auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
		const auto* Office = Save ? Save->GameData.OfficeDataMap.Find(GI->GetCurrentManagedBuildingIndex()) : nullptr;
		return Office && Office->ManufacturingResearch.ProjectIndex > 0;
	}

	const TCHAR* const ConfigPath = TEXT("/Game/CompanyGrowth/Data/DA_WorkstationDevFx.DA_WorkstationDevFx");
	// #24324A — 잠든 자리의 꺼질 듯한 화면빛(linear)
	const FLinearColor AsleepGlow(0.018f, 0.032f, 0.066f, 1.f);

	static float EaseIn(float T) { return T * T * T; }
	static float EaseInOut(float T) { return T < 0.5f ? 4.f * T * T * T : 1.f - FMath::Pow(-2.f * T + 2.f, 3.f) * 0.5f; }

	// 정육면체 1m → 조각 25(좌석 앞뒤)×34(좌우)×7(두께) cm
	static FVector TileScale(float LaneScale, float AlongForward, float AlongRight, float AlongUp)
	{
		return FVector(0.25f * LaneScale * AlongForward, 0.34f * LaneScale * AlongRight, 0.07f * LaneScale * AlongUp);
	}

	// 이름 끝 좌석 번호(MonitorMesh_1_Single → 1). 번호가 없으면 좌석 공용 = INDEX_NONE.
	static int32 SeatOfComponent(const UActorComponent* Comp)
	{
		const FString CompName = Comp ? Comp->GetName() : FString();
		const int32 At = CompName.Find(TEXT("Mesh_"), ESearchCase::CaseSensitive);
		if (At == INDEX_NONE || At + 5 >= CompName.Len()) return INDEX_NONE;
		const TCHAR Digit = CompName[At + 5];
		if (!FChar::IsDigit(Digit)) return INDEX_NONE;
		if (At + 6 < CompName.Len() && CompName[At + 6] != TEXT('_')) return INDEX_NONE;
		return static_cast<int32>(Digit - TEXT('0'));
	}

	static bool IsShownEquipment(const UStaticMeshComponent* Comp)
	{
		return Comp && Comp->GetStaticMesh() && Comp->IsVisible() && !Comp->ComponentHasTag(UWorkstationDevFxComponent::FxComponentTag);
	}

	// 좌석 축 공간(X = 좌석→모니터, Y = 오른쪽, Z = 월드 높이)의 상자 — 메시 로컬 상자 8 꼭짓점을 투영한다
	static void AccumulateLaneBox(const UStaticMeshComponent* Comp, const FVector& Origin, const FVector& Forward, const FVector& Right, FBox& InOutBox)
	{
		const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (!Mesh) return;
		const FBox Local = Mesh->GetBoundingBox();
		const FTransform& ToWorld = Comp->GetComponentTransform();
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Point((Corner & 1) ? Local.Max.X : Local.Min.X, (Corner & 2) ? Local.Max.Y : Local.Min.Y, (Corner & 4) ? Local.Max.Z : Local.Min.Z);
			const FVector Offset = ToWorld.TransformPosition(Point) - Origin;
			InOutBox += FVector(FVector::DotProduct(Offset, Forward), FVector::DotProduct(Offset, Right), Offset.Z + Origin.Z);
		}
	}

	static FBox LaneBox(const TArray<UStaticMeshComponent*>& Comps, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		FBox Out(ForceInit);
		for (const UStaticMeshComponent* Comp : Comps)
		{
			AccumulateLaneBox(Comp, Origin, Forward, Right, Out);
		}
		return Out;
	}

	static FVector LaneToWorld(const FVector& Origin, const FVector& Forward, const FVector& Right, double Along, double Side, double Height)
	{
		return FVector(Origin.X, Origin.Y, 0.0) + Forward * Along + Right * Side + FVector(0.0, 0.0, Height);
	}

	// 화면 왼쪽 → 오른쪽 순서(모니터 켜짐 물결·의자 회전 순서)
	static TArray<UWorkstationDevFxComponent*> GatherInScreenOrder(UWorld* InWorld)
	{
		TArray<UWorkstationDevFxComponent*> Result;
		if (!InWorld) return Result;
		for (TActorIterator<AWorkstationActorBase> It(InWorld); It; ++It)
		{
			if (UWorkstationDevFxComponent* Fx = It->GetDevFxComponent())
			{
				Result.Add(Fx);
			}
		}
		FVector CameraRight = FVector::RightVector;
		if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(InWorld, 0))
		{
			CameraRight = FRotationMatrix(CameraManager->GetCameraRotation()).GetScaledAxis(EAxis::Y);
		}
		Result.Sort([CameraRight](const UWorkstationDevFxComponent& A, const UWorkstationDevFxComponent& B)
		{
			const AActor* OwnerA = A.GetOwner();
			const AActor* OwnerB = B.GetOwner();
			const double KeyA = OwnerA ? FVector::DotProduct(OwnerA->GetActorLocation(), CameraRight) : 0.0;
			const double KeyB = OwnerB ? FVector::DotProduct(OwnerB->GetActorLocation(), CameraRight) : 0.0;
			return KeyA < KeyB;
		});
		return Result;
	}

	static bool IsReactionBusy(const AOfficeworker* InWorker)
	{
		const UEmployeeReactionComponent* Reaction = InWorker ? InWorker->FindComponentByClass<UEmployeeReactionComponent>() : nullptr;
		return Reaction && Reaction->IsComponentTickEnabled();
	}
}

const FName UWorkstationDevFxComponent::FxComponentTag(TEXT("WorkstationDevFx"));

UWorkstationDevFxComponent::UWorkstationDevFxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

AWorkstationActorBase* UWorkstationDevFxComponent::GetDesk() const
{
	return Cast<AWorkstationActorBase>(GetOwner());
}

bool UWorkstationDevFxComponent::LoadConfig()
{
	if (FxConfig) return true;
	if (bConfigTried) return false;
	bConfigTried = true;

	UWorkstationDevFxConfig* Loaded = TSoftObjectPtr<UWorkstationDevFxConfig>(FSoftObjectPath(DevFxUtil::ConfigPath)).LoadSynchronous();
	if (Loaded && Loaded->TileMesh && Loaded->PlaneMesh && Loaded->TileMaterial && Loaded->GlowMaterial && Loaded->NoteMaterial)
	{
		FxConfig = Loaded;
		return true;
	}
	static bool bWarned = false;
	if (!bWarned)
	{
		bWarned = true;
		UE_LOG(LogTemp, Warning, TEXT("[WorkstationDevFx] %s 가 없거나 비어 있다 — 책상 연출 그림 없이 구슬 출발점만 옮긴다(Tools/WorkstationDevFx/author_assets.py run_all)"),
			DevFxUtil::ConfigPath);
	}
	return false;
}

void UWorkstationDevFxComponent::StartTicking()
{
	if (!IsComponentTickEnabled())
	{
		SetComponentTickEnabled(true);
	}
}

void UWorkstationDevFxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FWorkstationDevFxLane& Lane : Lanes)
	{
		UnbindLaneWorker(Lane);
		RestoreChair(Lane);
		RestoreFlowRate(Lane, true);
		DestroyLaneVisuals(Lane);
	}
	Lanes.Reset();
	Super::EndPlay(EndPlayReason);
}

//========================================
// 정적 진입점
//========================================

void UWorkstationDevFxComponent::NotifyDevelopmentStart(UWorld* InWorld)
{
	if (!InWorld) return;
	const bool bReduced = USettingsManagerSubsystem::IsReduceMotion(InWorld);
	float Delay = DevFxTune::WaveFirstDelay;
	bool bTick = true;
	for (UWorkstationDevFxComponent* Fx : DevFxUtil::GatherInScreenOrder(InWorld))
	{
		const int32 LaneCount = Fx->BeginDevelopment(Delay, bTick, false);
		if (LaneCount > 0)
		{
			bTick = false;
			if (!bReduced) Delay += DevFxTune::WaveStep * LaneCount;
		}
	}
}

void UWorkstationDevFxComponent::EnsureDevelopmentActive(UWorld* InWorld)
{
	if (!DevFxUtil::IsDevelopmentRunning(InWorld)) return;
	for (UWorkstationDevFxComponent* Fx : DevFxUtil::GatherInScreenOrder(InWorld))
	{
		if (!Fx->IsDevelopmentActive())
		{
			Fx->BeginDevelopment(0.f, false, true);
		}
	}
}

bool UWorkstationDevFxComponent::HandleScoreOrb(UWorld* InWorld, const FVector& HeadWorldPos, int32 StepNumber, bool bCritical, bool bNewContribution, FVector& OutOrbStart)
{
	OutOrbStart = HeadWorldPos;
	if (!InWorld) return false;

	// 구슬 요청엔 직원이 실려 오지 않는다 — 요청 위치(직원 머리 위) 바로 아래 선 직원이 기여자다
	AOfficeworker* Contributor = nullptr;
	double BestDistSq = FMath::Square(static_cast<double>(DevFxTune::ContributorMatchCm));
	for (TActorIterator<AOfficeworker> It(InWorld); It; ++It)
	{
		if (It->bIsPortraitMode) continue;
		const double DistSq = FVector::DistSquared2D(It->GetActorLocation(), HeadWorldPos);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Contributor = *It;
		}
	}
	AWorkstationActorBase* Desk = Contributor ? Contributor->GetAssignedWorkstation() : nullptr;
	UWorkstationDevFxComponent* Fx = Desk ? Desk->GetDevFxComponent() : nullptr;
	return Fx && Fx->HandleContribution(*Contributor, StepNumber, bCritical, bNewContribution, OutOrbStart);
}

int32 UWorkstationDevFxComponent::NotifyDisciplineMilestone(UWorld* InWorld, int32 DisciplineSlot, int32 StepNumber)
{
	if (!InWorld || DisciplineSlot == INDEX_NONE) return 0;
	UGameInstance* GameInstance = InWorld->GetGameInstance();
	UEmployeeManager* Employees = GameInstance ? GameInstance->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!Employees) return 0;

	int32 Played = 0;
	for (UWorkstationDevFxComponent* Fx : DevFxUtil::GatherInScreenOrder(InWorld))
	{
		for (int32 LaneIndex = 0; LaneIndex < Fx->Lanes.Num(); ++LaneIndex)
		{
			const AOfficeworker* LaneWorker = Fx->Lanes[LaneIndex].Worker.Get();
			const UEmployeeBehaviorComponent* Behavior = LaneWorker ? LaneWorker->BehaviorComponent : nullptr;
			if (!Behavior || Behavior->GetBehaviorMode() != EEmployeeBehaviorMode::Stage
				|| Behavior->GetFatigueSlackPhase() != EFatigueSlackPhase::None)
			{
				continue;
			}
			const FEmployeeInstance* Employee = Employees->FindEmployee(LaneWorker->GetEmployeeID());
			if (!Employee || GetPrimaryDisciplineSlot(*Employee) != DisciplineSlot) continue;
			Fx->PlayMilestoneNote(LaneIndex, StepNumber);
			++Played;
		}
	}
	return Played;
}

void UWorkstationDevFxComponent::NotifyLaunchCheer(UWorld* InWorld)
{
	if (!InWorld) return;
	const TArray<UWorkstationDevFxComponent*> Ordered = DevFxUtil::GatherInScreenOrder(InWorld);
	for (int32 Order = 0; Order < Ordered.Num(); ++Order)
	{
		Ordered[Order]->DeskOrder = Order;
		Ordered[Order]->PlayCheer(DevFxTune::ChairSpinStagger * static_cast<float>(Order + 1));
	}

	// 두 번 폴짝은 반응 컴포넌트의 분야 달성 동작을 그대로 쓰고, 웃음만 환호 내내 이어 준다
	for (TActorIterator<AOfficeworker> It(InWorld); It; ++It)
	{
		AOfficeworker* Cheerer = *It;
		if (!Cheerer || Cheerer->bIsPortraitMode) continue;
		if (UEmployeeReactionComponent* Reaction = Cheerer->FindComponentByClass<UEmployeeReactionComponent>())
		{
			Reaction->PlayMilestoneReaction();
		}
		if (AFunnyOfficeworker* Funny = Cast<AFunnyOfficeworker>(Cheerer))
		{
			Funny->PlayFaceReaction(EWorkerFaceExpression::Happy, DevFxTune::CheerFaceSeconds);
		}
	}
}

void UWorkstationDevFxComponent::NotifyDevelopmentReset(UWorld* InWorld)
{
	for (UWorkstationDevFxComponent* Fx : DevFxUtil::GatherInScreenOrder(InWorld))
	{
		Fx->ResetDevelopment();
	}
}

bool UWorkstationDevFxComponent::PlayDebug(UWorld* InWorld, const FString& Kind, int32 WorkerIndex)
{
	if (!InWorld) return false;
	if (Kind.Equals(TEXT("start"), ESearchCase::IgnoreCase))
	{
		NotifyDevelopmentStart(InWorld);
		for (UWorkstationDevFxComponent* Fx : DevFxUtil::GatherInScreenOrder(InWorld))
		{
			Fx->bDebugRunning = Fx->bActive;
		}
		return true;
	}
	AOfficeCameraPawn* Camera = Cast<AOfficeCameraPawn>(UGameplayStatics::GetPlayerPawn(InWorld, 0));
	if (Kind.Equals(TEXT("climax"), ESearchCase::IgnoreCase))
	{
		// 실제 B 이상 결과(OfficeMainWidget::PlayLaunchClimax)와 같은 순서 — 기립 없이 의자만 돌리면 실물과 다른 그림이 찍힌다
		if (UOfficeStageProgressManager* StageMgr = InWorld->GetSubsystem<UOfficeStageProgressManager>())
		{
			StageMgr->NotifyEmployeesCheerStandUp();
		}
		NotifyLaunchCheer(InWorld);
		if (Camera) Camera->PlayPresentationPullIn(0.04f, 0.5f);
		return true;
	}
	if (Kind.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
	{
		if (Camera) Camera->ReleasePresentationPullIn(0.3f);
		NotifyDevelopmentReset(InWorld);
		return true;
	}

	const bool bTile = Kind.Equals(TEXT("tile"), ESearchCase::IgnoreCase);
	const bool bCrit = Kind.Equals(TEXT("crit"), ESearchCase::IgnoreCase);
	const bool bMile = Kind.Equals(TEXT("mile"), ESearchCase::IgnoreCase);
	const bool bFlow = Kind.Equals(TEXT("flow"), ESearchCase::IgnoreCase);
	if (!bTile && !bCrit && !bMile && !bFlow) return false;

	TArray<AOfficeworker*> Workers;
	for (TActorIterator<AOfficeworker> It(InWorld); It; ++It)
	{
		if (!It->bIsPortraitMode && It->GetAssignedWorkstation())
		{
			Workers.Add(*It);
		}
	}
	Workers.Sort([](const AOfficeworker& A, const AOfficeworker& B) { return A.GetEmployeeID() < B.GetEmployeeID(); });

	bool bPlayed = false;
	for (int32 Index = 0; Index < Workers.Num(); ++Index)
	{
		if (WorkerIndex >= 0 && Index != WorkerIndex) continue;
		AOfficeworker* Target = Workers[Index];
		UWorkstationDevFxComponent* Fx = Target->GetAssignedWorkstation()->GetDevFxComponent();
		if (!Fx) continue;
		if (!Fx->bActive)
		{
			Fx->BeginDevelopment(0.f, false, true);
			Fx->bDebugRunning = true;
		}
		const int32 LaneIndex = Fx->FindLaneForWorker(Target);
		if (!Fx->Lanes.IsValidIndex(LaneIndex)) continue;
		const int32 LaneStep = Fx->Lanes[LaneIndex].StepNumber;
		if (bTile || bCrit)
		{
			FVector UnusedStart = FVector::ZeroVector;
			bPlayed = Fx->HandleContribution(*Target, LaneStep, bCrit, true, UnusedStart) || bPlayed;
		}
		else if (bMile)
		{
			Fx->PlayMilestoneNote(LaneIndex, LaneStep);
			bPlayed = true;
		}
		else
		{
			Fx->SetDebugFlow(LaneIndex);
			bPlayed = true;
		}
	}
	return bPlayed;
}

//========================================
// 좌석 구성
//========================================

int32 UWorkstationDevFxComponent::BeginDevelopment(float WaveDelay, bool bPlayTick, bool bImmediate)
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld) return 0;

	ResetDevelopment();
	BuildLanes();
	if (Lanes.Num() == 0) return 0;

	bActive = true;
	LoadConfig();
	const double Now = CurrentWorld->GetTimeSeconds();
	const bool bReduced = USettingsManagerSubsystem::IsReduceMotion(this);
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		FWorkstationDevFxLane& Lane = Lanes[LaneIndex];
		EnsureLaneVisuals(Lane);
		const float LaneDelay = bReduced ? WaveDelay : WaveDelay + DevFxTune::WaveStep * static_cast<float>(LaneIndex);
		Lane.OnTime = bImmediate ? Now - DevFxTune::MonitorOnSeconds : Now + LaneDelay;
		Lane.bOn = bImmediate;
		Lane.bPlayTickOnTurnOn = bPlayTick && LaneIndex == 0;
		Lane.PoolOpacity = bImmediate ? DevFxTune::TierOpacity[0] : 0.f;
	}
	StartTicking();
	return Lanes.Num();
}

void UWorkstationDevFxComponent::ResetDevelopment()
{
	bool bWaitForRate = false;
	for (FWorkstationDevFxLane& Lane : Lanes)
	{
		UnbindLaneWorker(Lane);
		RestoreChair(Lane);
		RestoreFlowRate(Lane);
		bWaitForRate = bWaitForRate || Lane.bRateApplied;
		DestroyLaneVisuals(Lane);
		Lane.Streak = 0;
		Lane.Tier = 0;
		Lane.bOn = false;
		Lane.OnTime = -1.0;
		Lane.bDebugFlow = false;
		Lane.PoolOpacity = 0.f;
	}
	bActive = false;
	bDebugRunning = false;
	CheerStartTime = -1.0;

	// 반응 연출이 아직 타자 속도를 쥐고 있으면 그게 끝날 때까지 기다렸다 되돌린다
	UWorld* CurrentWorld = GetWorld();
	if (bWaitForRate && CurrentWorld)
	{
		bPendingRestore = true;
		PendingRestoreDeadline = CurrentWorld->GetTimeSeconds() + DevFxTune::PendingRestoreSeconds;
		StartTicking();
	}
	else
	{
		bPendingRestore = false;
		Lanes.Reset();
	}
}

void UWorkstationDevFxComponent::BuildLanes()
{
	for (FWorkstationDevFxLane& Old : Lanes)
	{
		UnbindLaneWorker(Old);
		RestoreFlowRate(Old, true);
		DestroyLaneVisuals(Old);
	}
	Lanes.Reset();
	bPendingRestore = false;

	AWorkstationActorBase* Desk = GetDesk();
	UWorld* CurrentWorld = GetWorld();
	if (!Desk || !CurrentWorld) return;

	const int32 SeatCount = Desk->GetChairCount();
	for (int32 Seat = 0; Seat < SeatCount; ++Seat)
	{
		AOfficeworker* SeatWorker = Desk->GetOccupantAt(Seat);
		const int32 AssignedID = Desk->GetAssignedEmployeeID(Seat);
		if (!SeatWorker && AssignedID != INDEX_NONE)
		{
			for (TActorIterator<AOfficeworker> It(CurrentWorld); It; ++It)
			{
				if (!It->bIsPortraitMode && It->GetEmployeeID() == AssignedID)
				{
					SeatWorker = *It;
					break;
				}
			}
		}
		if (!SeatWorker || SeatWorker->bIsPortraitMode) continue;

		FWorkstationDevFxLane NewLane;
		NewLane.SeatIndex = Seat;
		NewLane.Worker = SeatWorker;
		NewLane.EmployeeID = SeatWorker->GetEmployeeID();
		if (!ResolveLaneAnchors(NewLane)) continue;
		ResolveLaneDiscipline(NewLane);
		// 몰입 가속의 기준 속도는 아무 반응도 없을 때 한 번 읽어 둔다
		if (USkeletalMeshComponent* Body = SeatWorker->GetMesh())
		{
			if (!DevFxUtil::IsReactionBusy(SeatWorker))
			{
				NewLane.TrueRate = Body->GlobalAnimRateScale;
				NewLane.bTrueRateKnown = true;
			}
		}
		Lanes.Add(MoveTemp(NewLane));
	}
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		BindLaneWorker(LaneIndex);
	}
}

bool UWorkstationDevFxComponent::ResolveLaneAnchors(FWorkstationDevFxLane& Lane) const
{
	const AWorkstationActorBase* Desk = GetDesk();
	if (!Desk) return false;

	TArray<UStaticMeshComponent*> Meshes;
	Desk->GetComponents<UStaticMeshComponent>(Meshes);

	const int32 LaneSeat = Lane.SeatIndex;
	auto InLane = [LaneSeat](const UStaticMeshComponent* Comp)
	{
		const int32 CompSeat = DevFxUtil::SeatOfComponent(Comp);
		return CompSeat == INDEX_NONE || CompSeat == LaneSeat;
	};

	TArray<UStaticMeshComponent*> Monitors, Laptops, Keyboards, Clutter;
	UStaticMeshComponent* ChairComp = nullptr;
	const FString ChairName = FString::Printf(TEXT("ChairMesh_%d"), LaneSeat);
	for (UStaticMeshComponent* Comp : Meshes)
	{
		if (!Comp) continue;
		const FString CompName = Comp->GetName();
		if (CompName == ChairName)
		{
			ChairComp = Comp;
			continue;
		}
		if (!DevFxUtil::IsShownEquipment(Comp) || !InLane(Comp)) continue;
		if (CompName.StartsWith(TEXT("MonitorMesh"))) Monitors.Add(Comp);
		else if (CompName.StartsWith(TEXT("LaptopMesh"))) Laptops.Add(Comp);
		else if (CompName.StartsWith(TEXT("KeyboardMesh"))) Keyboards.Add(Comp);
		else if (CompName.StartsWith(TEXT("MouseMesh")) || CompName.StartsWith(TEXT("ComputerMesh")) || CompName.StartsWith(TEXT("AccessoryMesh")))
		{
			Clutter.Add(Comp);
		}
	}
	const bool bHasMonitor = Monitors.Num() > 0;
	const TArray<UStaticMeshComponent*>& Screens = bHasMonitor ? Monitors : Laptops;
	if (Screens.Num() == 0) return false;
	if (bHasMonitor) Clutter.Append(Laptops);   // 옆 노트북은 조각 더미 자리를 막는 물건

	const FVector SeatLocation = Desk->GetSeatLocation(LaneSeat);
	FBox ScreenWorld(ForceInit);
	for (const UStaticMeshComponent* Comp : Screens)
	{
		ScreenWorld += Comp->Bounds.GetBox();
	}
	FVector ToScreen = ScreenWorld.GetCenter() - SeatLocation;
	ToScreen.Z = 0.0;
	Lane.Forward = ToScreen.GetSafeNormal(UE_SMALL_NUMBER, Desk->GetActorForwardVector().GetSafeNormal2D());
	if (Lane.Forward.IsNearlyZero()) Lane.Forward = FVector::ForwardVector;
	Lane.Right = FVector::CrossProduct(FVector::UpVector, Lane.Forward);
	Lane.Scale = FMath::Clamp(static_cast<float>(ToScreen.Size()) / 100.f, 0.6f, 2.5f);
	const float S = Lane.Scale;
	const FVector& F = Lane.Forward;
	const FVector& R = Lane.Right;

	const FBox Screen = DevFxUtil::LaneBox(Screens, SeatLocation, F, R);
	if (!Screen.IsValid) return false;
	const FBox Keys = DevFxUtil::LaneBox(Keyboards, SeatLocation, F, R);

	const FVector ScreenCenter = Screen.GetCenter();
	const double ScreenHalfWidth = Screen.GetExtent().Y;
	const double ScreenHeight = FMath::Max(1.0, Screen.Max.Z - Screen.Min.Z);
	double DeskTop = Screen.Min.Z;
	if (Keys.IsValid) DeskTop = FMath::Min(DeskTop, Keys.Min.Z);

	Lane.ScreenTop = DevFxUtil::LaneToWorld(SeatLocation, F, R, ScreenCenter.X, ScreenCenter.Y, Screen.Max.Z);
	Lane.TileStart = DevFxUtil::LaneToWorld(SeatLocation, F, R, ScreenCenter.X, ScreenCenter.Y, Screen.Max.Z - 0.24 * ScreenHeight);
	Lane.GlowLocation = DevFxUtil::LaneToWorld(SeatLocation, F, R, ScreenCenter.X + 3.0 * S, ScreenCenter.Y, Screen.Max.Z + 3.0 * S);
	Lane.PoolCenter = DevFxUtil::LaneToWorld(SeatLocation, F, R, ScreenCenter.X - 26.0 * S, ScreenCenter.Y, DeskTop + 0.5);
	Lane.PoolWidth = 150.f * S;
	Lane.PoolDepth = 95.f * S;

	if (Keys.IsValid)
	{
		const FVector KeysCenter = Keys.GetCenter();
		Lane.KeyboardTop = DevFxUtil::LaneToWorld(SeatLocation, F, R, KeysCenter.X, KeysCenter.Y, Keys.Max.Z + 1.0);
	}
	else if (!bHasMonitor)
	{
		// 노트북만 있는 자리 — 노트북 앞쪽(자판)에서 튄다
		Lane.KeyboardTop = DevFxUtil::LaneToWorld(SeatLocation, F, R, Screen.Min.X + 8.0 * S, ScreenCenter.Y, DeskTop + 3.0);
	}
	else
	{
		Lane.KeyboardTop = DevFxUtil::LaneToWorld(SeatLocation, F, R, ScreenCenter.X - 30.0 * S, ScreenCenter.Y, DeskTop + 2.0);
	}

	// 메모지 = 모니터 뒷면(좌석 반대쪽) 왼편 위. 사무실 거리에서 노트북 뚜껑 안에만 있으면 안 보여 윗변 위로 삐죽 나오게 붙인다.
	Lane.NoteSize = FMath::Max(8.f, FMath::Min3(30.f * S, static_cast<float>(ScreenHalfWidth) * 1.1f, static_cast<float>(ScreenHeight) * 0.75f));
	const double NoteHalf = Lane.NoteSize * 0.5;
	const double NoteSide = ScreenCenter.Y - FMath::Clamp(ScreenHalfWidth - NoteHalf * 0.6, 0.0, 22.0 * S);
	Lane.NoteAnchor = DevFxUtil::LaneToWorld(SeatLocation, F, R, Screen.Max.X + 0.4, NoteSide, Screen.Max.Z - 0.1 * ScreenHeight);

	// 조각 더미 = 모니터 옆(기본 왼쪽). 옆 노트북·마우스·본체가 막고 있으면 반대쪽으로.
	TArray<FBox> ClutterBoxes;
	for (const UStaticMeshComponent* Comp : Clutter)
	{
		FBox Item(ForceInit);
		DevFxUtil::AccumulateLaneBox(Comp, SeatLocation, F, R, Item);
		if (Item.IsValid) ClutterBoxes.Add(Item);
	}
	const double StackAlong = ScreenCenter.X - 17.0 * S;
	const double StackReach = FMath::Max(55.0 * S, ScreenHalfWidth + 16.0 * S);
	const double Radius = 18.0 * S;
	auto CountBlocked = [&ClutterBoxes, StackAlong, Radius](double Side)
	{
		int32 Hits = 0;
		for (const FBox& Item : ClutterBoxes)
		{
			if (StackAlong > Item.Min.X - Radius && StackAlong < Item.Max.X + Radius && Side > Item.Min.Y - Radius && Side < Item.Max.Y + Radius)
			{
				++Hits;
			}
		}
		return Hits;
	};
	const bool bSharedScreen = DevFxUtil::SeatOfComponent(Screens[0]) == INDEX_NONE && Desk->GetChairCount() > 1;
	const double Preferred = (bSharedScreen && LaneSeat % 2 == 1) ? 1.0 : -1.0;
	const double FirstSide = ScreenCenter.Y + Preferred * StackReach;
	const double OtherSide = ScreenCenter.Y - Preferred * StackReach;
	double StackSide = CountBlocked(OtherSide) < CountBlocked(FirstSide) ? OtherSide : FirstSide;
	if (Desk->DeskMesh && Desk->DeskMesh->GetStaticMesh())
	{
		FBox DeskBox(ForceInit);
		DevFxUtil::AccumulateLaneBox(Desk->DeskMesh, SeatLocation, F, R, DeskBox);
		if (DeskBox.IsValid && DeskBox.Max.Y - DeskBox.Min.Y > 2.0 * Radius)
		{
			StackSide = FMath::Clamp(StackSide, DeskBox.Min.Y + Radius, DeskBox.Max.Y - Radius);
		}
	}
	Lane.StackBase = DevFxUtil::LaneToWorld(SeatLocation, F, R, StackAlong, StackSide, DeskTop + 0.2);

	Lane.Chair = ChairComp;
	return true;
}

void UWorkstationDevFxComponent::ResolveLaneDiscipline(FWorkstationDevFxLane& Lane) const
{
	UWorld* CurrentWorld = GetWorld();
	const UOfficeStageProgressManager* StageMgr = CurrentWorld ? CurrentWorld->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	UGameInstance* GameInstance = CurrentWorld ? CurrentWorld->GetGameInstance() : nullptr;
	UEmployeeManager* Employees = GameInstance ? GameInstance->GetSubsystem<UEmployeeManager>() : nullptr;
	const FEmployeeInstance* Employee = Employees ? Employees->FindEmployee(Lane.EmployeeID) : nullptr;
	const int32 Primary = Employee ? GetPrimaryDisciplineSlot(*Employee) : INDEX_NONE;

	Lane.GlyphCell = FMath::Max(0, Primary);
	Lane.StepNumber = 1;
	if (StageMgr)
	{
		const int32 StepIndex = StageMgr->GetProgressData().Steps.IndexOfByPredicate(
			[Primary](const FStepRoundData& Step) { return Step.DisciplineSlot == Primary; });
		if (StepIndex != INDEX_NONE) Lane.StepNumber = StepIndex + 1;
	}
	// 색은 구슬과 같은 출처(스트립 칸 위치색)라 조각·빛·구슬이 한 색으로 읽힌다
	Lane.Color = UGlobalUtilFunctions::GetStepColor(Lane.StepNumber);
}

void UWorkstationDevFxComponent::BindLaneWorker(int32 LaneIndex)
{
	if (!Lanes.IsValidIndex(LaneIndex)) return;
	FWorkstationDevFxLane& Lane = Lanes[LaneIndex];
	AOfficeworker* LaneWorker = Lane.Worker.Get();
	UEmployeeBehaviorComponent* Behavior = LaneWorker ? LaneWorker->BehaviorComponent : nullptr;
	if (!Behavior) return;
	// 반응 컴포넌트(BeginPlay 에서 먼저 묶임)보다 늦게 묶어 먼저 불린다 — 반응이 기준 속도를 읽기 전에 몰입 가속을 거둔다
	Lane.BoundBehavior = Behavior;
	Lane.SlackHandle = Behavior->OnFatigueSlackPhaseChanged.AddUObject(this, &UWorkstationDevFxComponent::HandleSlackPhaseChanged, LaneIndex);
	Lane.CritHandle = Behavior->OnCriticalContribution.AddUObject(this, &UWorkstationDevFxComponent::HandleCriticalContribution, LaneIndex);
}

void UWorkstationDevFxComponent::UnbindLaneWorker(FWorkstationDevFxLane& Lane)
{
	if (UEmployeeBehaviorComponent* Behavior = Lane.BoundBehavior.Get())
	{
		Behavior->OnFatigueSlackPhaseChanged.Remove(Lane.SlackHandle);
		Behavior->OnCriticalContribution.Remove(Lane.CritHandle);
	}
	Lane.BoundBehavior.Reset();
	Lane.SlackHandle.Reset();
	Lane.CritHandle.Reset();
}

int32 UWorkstationDevFxComponent::FindLaneForWorker(const AOfficeworker* InWorker) const
{
	if (!InWorker) return INDEX_NONE;
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		if (Lanes[LaneIndex].Worker.Get() == InWorker || Lanes[LaneIndex].EmployeeID == InWorker->GetEmployeeID())
		{
			return LaneIndex;
		}
	}
	return INDEX_NONE;
}

void UWorkstationDevFxComponent::HandleSlackPhaseChanged(EFatigueSlackPhase OldPhase, EFatigueSlackPhase NewPhase, bool bManualCatch, const FVector& PreviousLocation, int32 LaneIndex)
{
	if (!Lanes.IsValidIndex(LaneIndex) || NewPhase == EFatigueSlackPhase::None) return;
	FWorkstationDevFxLane& Lane = Lanes[LaneIndex];
	Lane.Streak = 0;
	RestoreFlowRate(Lane);

	// 집중 표정이 졸음·딴짓 얼굴을 덮지 않게 바로 거둔다
	UWorld* CurrentWorld = GetWorld();
	if (CurrentWorld && Lane.FocusFaceEndTime > CurrentWorld->GetTimeSeconds())
	{
		Lane.FocusFaceEndTime = 0.0;
		if (AFunnyOfficeworker* Funny = Cast<AFunnyOfficeworker>(Lane.Worker.Get()))
		{
			Funny->PlayFaceReaction(Funny->ResolveFaceExpression(), 0.02f);
		}
	}
}

void UWorkstationDevFxComponent::HandleCriticalContribution(int32 LaneIndex)
{
	if (!Lanes.IsValidIndex(LaneIndex)) return;
	RestoreFlowRate(Lanes[LaneIndex]);
}

//========================================
// 기여 · 분야 달성 · 환호
//========================================

bool UWorkstationDevFxComponent::HandleContribution(const AOfficeworker& InWorker, int32 InStepNumber, bool bCritical, bool bNewContribution, FVector& OutOrbStart)
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld) return false;
	if (!bActive)
	{
		// 시작 연출 없이 들어온 개발(세이브 복원·컨티뉴) — 물결 없이 바로 켠다
		BeginDevelopment(0.f, false, true);
	}
	const int32 LaneIndex = FindLaneForWorker(&InWorker);
	if (!Lanes.IsValidIndex(LaneIndex)) return false;
	FWorkstationDevFxLane& Lane = Lanes[LaneIndex];
	OutOrbStart = Lane.ScreenTop + FVector(0.0, 0.0, DevFxTune::OrbLift * Lane.Scale);
	if (!bNewContribution) return true;

	const double Now = CurrentWorld->GetTimeSeconds();
	const bool bReduced = USettingsManagerSubsystem::IsReduceMotion(this);
	int32 Cell = Lane.GlyphCell;
	if (const UOfficeStageProgressManager* StageMgr = CurrentWorld->GetSubsystem<UOfficeStageProgressManager>())
	{
		const TArray<FStepRoundData>& Steps = StageMgr->GetProgressData().Steps;
		if (Steps.IsValidIndex(InStepNumber - 1))
		{
			Cell = FMath::Max(0, Steps[InStepNumber - 1].DisciplineSlot);
		}
	}
	Lane.StepNumber = InStepNumber;
	Lane.Color = UGlobalUtilFunctions::GetStepColor(InStepNumber);

	++Lane.Streak;
	if (Lane.Streak >= DevFxTune::FlowTier1Streak)
	{
		// 몰입 1단계 = 집중 얼굴. 다음 기여까지 이어지게 길게 걸고, 크리 별눈·달성 웃음은 뒤에 와서 덮는다.
		if (AFunnyOfficeworker* Funny = Cast<AFunnyOfficeworker>(Lane.Worker.Get()))
		{
			Funny->PlayFaceReaction(EWorkerFaceExpression::Focused, DevFxTune::FocusFaceSeconds);
			Lane.FocusFaceEndTime = Now + DevFxTune::FocusFaceSeconds;
		}
	}
	SpawnTile(Lane, InStepNumber, Cell, bCritical, bReduced);
	StartTicking();
	return true;
}

void UWorkstationDevFxComponent::SetDebugFlow(int32 LaneIndex)
{
	if (!Lanes.IsValidIndex(LaneIndex)) return;
	FWorkstationDevFxLane& Lane = Lanes[LaneIndex];
	Lane.Streak = FMath::Max(Lane.Streak, DevFxTune::FlowTier2Streak);
	Lane.bDebugFlow = true;
	bDebugRunning = true;
	if (AFunnyOfficeworker* Funny = Cast<AFunnyOfficeworker>(Lane.Worker.Get()))
	{
		Funny->PlayFaceReaction(EWorkerFaceExpression::Focused, 8.f);
	}
	StartTicking();
}

void UWorkstationDevFxComponent::PlayMilestoneNote(int32 LaneIndex, int32 InStepNumber)
{
	if (!Lanes.IsValidIndex(LaneIndex) || !LoadConfig()) return;
	FWorkstationDevFxLane& Lane = Lanes[LaneIndex];
	RestoreFlowRate(Lane);
	EnsureLaneVisuals(Lane);
	const bool bReduced = USettingsManagerSubsystem::IsReduceMotion(this);
	const float S = Lane.Scale;
	const FVector StackTop = Lane.StackBase + FVector(0.0, 0.0, (DevFxTune::TileBaseHeight + Lane.StackOrder.Num() * DevFxTune::TileStepHeight) * S);

	// 쌓인 조각이 한 장으로 접힌다
	for (const int32 TileIndex : Lane.StackOrder)
	{
		FWorkstationDevFxTile& Tile = Lane.Tiles[TileIndex];
		if (bReduced || !Tile.Mesh)
		{
			ReleaseTile(Tile);
			continue;
		}
		Tile.Phase = 3;
		Tile.Age = 0.f;
		Tile.From = Tile.Mesh->GetComponentLocation();
		Tile.LeaveTo = Lane.StackBase + FVector(0.0, 0.0, DevFxTune::NoteFoldRise * S);
		Tile.LeaveDuration = DevFxTune::NoteFold;
		Tile.LeaveEndScale = 0.3f;
	}
	Lane.StackOrder.Reset();

	if (!Lane.Note)
	{
		UMaterialInstanceDynamic* NewMID = nullptr;
		Lane.Note = CreateFxMesh(FxConfig->PlaneMesh, FxConfig->NoteMaterial, NewMID);
		Lane.NoteMID = NewMID;
	}
	if (!Lane.Note) return;
	if (Lane.NoteMID)
	{
		Lane.NoteMID->SetVectorParameterValue(DevFxParam::NoteColor, UGlobalUtilFunctions::GetStepColor(InStepNumber));
	}
	Lane.NoteFrom = StackTop;
	Lane.NoteControl = FMath::Lerp(StackTop, Lane.NoteAnchor, 0.5f) + FVector(0.0, 0.0, DevFxTune::NoteArcUp * S);
	Lane.NotePhase = bReduced ? 2 : 1;
	Lane.NoteAge = bReduced ? DevFxTune::NoteTotal : 0.f;
	TickNote(Lane, LaneIndex, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, 0.f);
	StartTicking();
}

void UWorkstationDevFxComponent::PlayCheer(float SpinDelay)
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || Lanes.Num() == 0) return;
	const double Now = CurrentWorld->GetTimeSeconds();
	const bool bReduced = USettingsManagerSubsystem::IsReduceMotion(this);
	CheerStartTime = bReduced ? -1.0 : Now;

	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		FWorkstationDevFxLane& Lane = Lanes[LaneIndex];
		Lane.Streak = 0;
		Lane.bDebugFlow = false;
		RestoreFlowRate(Lane);

		// 조각은 위로 떠오르며 사라진다(완성된 작업물을 들고 일어선다)
		for (FWorkstationDevFxTile& Tile : Lane.Tiles)
		{
			if (Tile.Phase != 1 && Tile.Phase != 2) continue;
			if (bReduced || !Tile.Mesh)
			{
				ReleaseTile(Tile);
				continue;
			}
			Tile.Phase = 3;
			Tile.Age = 0.f;
			Tile.From = Tile.Mesh->GetComponentLocation();
			Tile.LeaveTo = Tile.From + FVector(0.0, 0.0, DevFxTune::CheerTileRise * Lane.Scale);
			Tile.LeaveDuration = DevFxTune::CheerTileSeconds;
			Tile.LeaveEndScale = 0.f;
		}
		Lane.StackOrder.Reset();

		if (!bReduced)
		{
			CaptureChair(Lane);
			Lane.SpinStartTime = Now + SpinDelay + DevFxTune::ChairSpinStagger * static_cast<float>(LaneIndex);
		}
	}
	StartTicking();
}

//========================================
// 연출 메시
//========================================

UStaticMeshComponent* UWorkstationDevFxComponent::CreateFxMesh(UStaticMesh* Mesh, UMaterialInterface* Material, UMaterialInstanceDynamic*& OutMID)
{
	OutMID = nullptr;
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !Mesh || !Material) return nullptr;

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(OwnerActor, NAME_None, RF_Transient);
	Comp->ComponentTags.Add(FxComponentTag);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetCastShadow(false);
	Comp->SetReceivesDecals(false);
	Comp->SetUsingAbsoluteLocation(true);
	Comp->SetUsingAbsoluteRotation(true);
	Comp->SetUsingAbsoluteScale(true);
	Comp->SetStaticMesh(Mesh);
	OutMID = UMaterialInstanceDynamic::Create(Material, this);
	Comp->SetMaterial(0, OutMID ? static_cast<UMaterialInterface*>(OutMID) : Material);
	Comp->SetVisibility(false);
	if (USceneComponent* AttachRoot = OwnerActor->GetRootComponent())
	{
		Comp->SetupAttachment(AttachRoot);
	}
	Comp->RegisterComponent();
	return Comp;
}

void UWorkstationDevFxComponent::EnsureLaneVisuals(FWorkstationDevFxLane& Lane)
{
	if (!LoadConfig()) return;
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	if (!Lane.Pool)
	{
		UMaterialInstanceDynamic* NewMID = nullptr;
		Lane.Pool = CreateFxMesh(FxConfig->PlaneMesh, FxConfig->GlowMaterial, NewMID);
		Lane.PoolMID = NewMID;
		if (Lane.Pool)
		{
			Lane.Pool->SetTranslucentSortPriority(1);
			Lane.Pool->SetWorldLocationAndRotation(Lane.PoolCenter, FRotationMatrix::MakeFromZX(FVector::UpVector, Lane.Forward).ToQuat());
			Lane.Pool->SetWorldScale3D(FVector(Lane.PoolDepth / 100.f, Lane.PoolWidth / 100.f, 1.f));
		}
		if (Lane.PoolMID)
		{
			Lane.PoolMID->SetScalarParameterValue(DevFxParam::Opacity, 0.f);
			Lane.PoolMID->SetVectorParameterValue(DevFxParam::GlowColor, Lane.Color);
		}
	}

	if (!Lane.Glow)
	{
		UMaterialBillboardComponent* Billboard = NewObject<UMaterialBillboardComponent>(OwnerActor, NAME_None, RF_Transient);
		Billboard->ComponentTags.Add(FxComponentTag);
		Billboard->SetMobility(EComponentMobility::Movable);
		Billboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Billboard->SetGenerateOverlapEvents(false);
		Billboard->SetCanEverAffectNavigation(false);
		Billboard->SetCastShadow(false);
		Billboard->SetReceivesDecals(false);
		Billboard->SetTranslucentSortPriority(2);
		Billboard->SetUsingAbsoluteLocation(true);
		Billboard->SetUsingAbsoluteRotation(true);
		Billboard->SetUsingAbsoluteScale(true);
		if (USceneComponent* AttachRoot = OwnerActor->GetRootComponent())
		{
			Billboard->SetupAttachment(AttachRoot);
		}
		Billboard->RegisterComponent();

		UMaterialInstanceDynamic* NewGlowMID = UMaterialInstanceDynamic::Create(FxConfig->GlowMaterial, this);
		FMaterialSpriteElement Element;
		Element.Material = NewGlowMID ? static_cast<UMaterialInterface*>(NewGlowMID) : FxConfig->GlowMaterial.Get();
		Element.bSizeIsInScreenSpace = false;
		Element.BaseSizeX = 100.f * Lane.Scale;
		Element.BaseSizeY = 70.f * Lane.Scale;
		Billboard->SetElements({ Element });
		Billboard->SetWorldLocation(Lane.GlowLocation);
		Billboard->SetVisibility(false);
		if (NewGlowMID)
		{
			NewGlowMID->SetScalarParameterValue(DevFxParam::Opacity, 0.f);
			NewGlowMID->SetVectorParameterValue(DevFxParam::GlowColor, Lane.Color);
		}
		Lane.Glow = Billboard;
		Lane.GlowMID = NewGlowMID;
	}
}

int32 UWorkstationDevFxComponent::AcquireTile(FWorkstationDevFxLane& Lane)
{
	for (int32 TileIndex = 0; TileIndex < Lane.Tiles.Num(); ++TileIndex)
	{
		if (Lane.Tiles[TileIndex].Phase == 0 && Lane.Tiles[TileIndex].Mesh) return TileIndex;
	}
	if (Lane.Tiles.Num() < DevFxTune::MaxTiles && FxConfig)
	{
		UMaterialInstanceDynamic* NewMID = nullptr;
		UStaticMeshComponent* NewMesh = CreateFxMesh(FxConfig->TileMesh, FxConfig->TileMaterial, NewMID);
		if (!NewMesh) return INDEX_NONE;
		FWorkstationDevFxTile NewTile;
		NewTile.Mesh = NewMesh;
		NewTile.MID = NewMID;
		return Lane.Tiles.Add(NewTile);
	}
	// 칸이 다 차면 가장 오래 쌓인 조각을 다시 쓴다
	if (Lane.StackOrder.Num() > 0)
	{
		const int32 Oldest = Lane.StackOrder[0];
		Lane.StackOrder.RemoveAt(0);
		ReleaseTile(Lane.Tiles[Oldest]);
		LayoutStack(Lane, 1.f);
		return Oldest;
	}
	return INDEX_NONE;
}

void UWorkstationDevFxComponent::ReleaseTile(FWorkstationDevFxTile& Tile)
{
	Tile.Phase = 0;
	Tile.Age = 0.f;
	Tile.bLanded = false;
	if (Tile.Mesh) Tile.Mesh->SetVisibility(false);
}

void UWorkstationDevFxComponent::SpawnTile(FWorkstationDevFxLane& Lane, int32 InStepNumber, int32 InGlyphCell, bool bCritical, bool bReduced)
{
	if (!LoadConfig()) return;
	const int32 TileIndex = AcquireTile(Lane);
	if (!Lane.Tiles.IsValidIndex(TileIndex)) return;

	int32 Flying = 0;
	for (const FWorkstationDevFxTile& Other : Lane.Tiles)
	{
		if (Other.Phase == 1) ++Flying;
	}
	const int32 StackSlot = FMath::Min(Lane.StackOrder.Num() + Flying, DevFxTune::MaxStack - 1);

	FWorkstationDevFxTile& Tile = Lane.Tiles[TileIndex];
	Tile.Phase = 1;
	Tile.Age = bReduced ? DevFxTune::TileFlight + DevFxTune::TileLand : 0.f;
	Tile.bLanded = bReduced;
	Tile.BaseScale = bCritical ? DevFxTune::CritTileScale : 1.f;
	Tile.From = Lane.TileStart;
	Tile.To = Lane.StackBase + FVector(0.0, 0.0, (DevFxTune::TileBaseHeight + StackSlot * DevFxTune::TileStepHeight) * Lane.Scale);
	if (Tile.MID)
	{
		Tile.MID->SetVectorParameterValue(DevFxParam::TileColor, UGlobalUtilFunctions::GetStepColor(InStepNumber));
		Tile.MID->SetScalarParameterValue(DevFxParam::GoldMix, bCritical ? DevFxTune::CritGoldMix : 0.f);
		Tile.MID->SetScalarParameterValue(DevFxParam::GlyphCell, static_cast<float>(InGlyphCell));
		Tile.MID->SetScalarParameterValue(DevFxParam::GlyphOn, 1.f);
	}
	if (Tile.Mesh)
	{
		const float StartSize = Tile.BaseScale * 0.7f;
		Tile.Mesh->SetWorldLocationAndRotation(bReduced ? Tile.To : Tile.From, GetLaneQuat(Lane));
		Tile.Mesh->SetWorldScale3D(DevFxUtil::TileScale(Lane.Scale, StartSize, StartSize, StartSize));
		Tile.Mesh->SetVisibility(true);
	}
}

void UWorkstationDevFxComponent::LayoutStack(FWorkstationDevFxLane& Lane, float Bump)
{
	const FQuat LaneQuat = GetLaneQuat(Lane);
	for (int32 Order = 0; Order < Lane.StackOrder.Num(); ++Order)
	{
		FWorkstationDevFxTile& Tile = Lane.Tiles[Lane.StackOrder[Order]];
		if (!Tile.Mesh) continue;
		const float Height = (DevFxTune::TileBaseHeight + Order * DevFxTune::TileStepHeight) * Lane.Scale * Bump;
		Tile.Mesh->SetWorldLocationAndRotation(Lane.StackBase + FVector(0.0, 0.0, Height), LaneQuat);
		Tile.Mesh->SetWorldScale3D(DevFxUtil::TileScale(Lane.Scale, Tile.BaseScale, Tile.BaseScale, Tile.BaseScale * Bump));
	}
}

void UWorkstationDevFxComponent::SpawnKeycap(FWorkstationDevFxLane& Lane)
{
	if (!LoadConfig()) return;
	FWorkstationDevFxKeycap* Cap = Lane.Keycaps.FindByPredicate([](const FWorkstationDevFxKeycap& Item) { return !Item.bAlive && Item.Mesh; });
	if (!Cap && Lane.Keycaps.Num() < DevFxTune::MaxKeycaps)
	{
		UMaterialInstanceDynamic* NewMID = nullptr;
		UStaticMeshComponent* NewMesh = CreateFxMesh(FxConfig->TileMesh, FxConfig->TileMaterial, NewMID);
		if (!NewMesh) return;
		FWorkstationDevFxKeycap NewCap;
		NewCap.Mesh = NewMesh;
		NewCap.MID = NewMID;
		Cap = &Lane.Keycaps[Lane.Keycaps.Add(NewCap)];
	}
	if (!Cap || !Cap->Mesh) return;

	const float S = Lane.Scale;
	if (Cap->MID)
	{
		Cap->MID->SetVectorParameterValue(DevFxParam::TileColor, Lane.Color);
		Cap->MID->SetScalarParameterValue(DevFxParam::GoldMix, 0.f);
		Cap->MID->SetScalarParameterValue(DevFxParam::GlyphOn, 0.f);
	}
	Cap->bAlive = true;
	Cap->Age = 0.f;
	Cap->Spin = 0.f;
	Cap->Location = Lane.KeyboardTop + Lane.Right * (FMath::FRandRange(-DevFxTune::KeycapReach, DevFxTune::KeycapReach) * S);
	Cap->Velocity = Lane.Right * (FMath::FRandRange(-DevFxTune::KeycapSpread, DevFxTune::KeycapSpread) * S)
		+ Lane.Forward * (FMath::FRandRange(-DevFxTune::KeycapSpread, DevFxTune::KeycapSpread) * S)
		+ FVector(0.0, 0.0, DevFxTune::KeycapRise * S);
	Cap->Mesh->SetWorldLocationAndRotation(Cap->Location, GetLaneQuat(Lane));
	Cap->Mesh->SetWorldScale3D(FVector(0.05f, 0.05f, 0.03f) * S);
	Cap->Mesh->SetVisibility(true);
}

void UWorkstationDevFxComponent::DestroyLaneVisuals(FWorkstationDevFxLane& Lane)
{
	auto Kill = [](UPrimitiveComponent* Prim)
	{
		if (Prim) Prim->DestroyComponent();
	};
	Kill(Lane.Pool.Get());
	Kill(Lane.Glow.Get());
	Kill(Lane.Note.Get());
	for (FWorkstationDevFxTile& Tile : Lane.Tiles) Kill(Tile.Mesh.Get());
	for (FWorkstationDevFxKeycap& Cap : Lane.Keycaps) Kill(Cap.Mesh.Get());
	Lane.Pool = nullptr;
	Lane.PoolMID = nullptr;
	Lane.Glow = nullptr;
	Lane.GlowMID = nullptr;
	Lane.Note = nullptr;
	Lane.NoteMID = nullptr;
	Lane.NotePhase = 0;
	Lane.Tiles.Reset();
	Lane.Keycaps.Reset();
	Lane.StackOrder.Reset();
}

FQuat UWorkstationDevFxComponent::GetLaneQuat(const FWorkstationDevFxLane& Lane) const
{
	// 로컬 X = 좌석→모니터, Y = 오른쪽, Z = 위 — 조각 재질의 글리프 UV 가 이 축을 전제한다
	return FRotationMatrix::MakeFromZX(FVector::UpVector, Lane.Forward).ToQuat();
}

//========================================
// 틱
//========================================

void UWorkstationDevFxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		SetComponentTickEnabled(false);
		return;
	}
	const double Now = CurrentWorld->GetTimeSeconds();
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.05f);

	if (bPendingRestore && !bActive)
	{
		const bool bForce = Now >= PendingRestoreDeadline;
		bool bAllRestored = true;
		for (FWorkstationDevFxLane& Lane : Lanes)
		{
			RestoreFlowRate(Lane, bForce);
			bAllRestored = bAllRestored && !Lane.bRateApplied;
		}
		if (bAllRestored)
		{
			bPendingRestore = false;
			Lanes.Reset();
			SetComponentTickEnabled(false);
		}
		return;
	}

	const bool bReduced = USettingsManagerSubsystem::IsReduceMotion(this);
	const bool bRunning = bDebugRunning || DevFxUtil::IsDevelopmentRunning(CurrentWorld);
	bool bBusy = bActive;
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		bBusy = TickLane(Lanes[LaneIndex], LaneIndex, Now, Dt, bReduced, bRunning) || bBusy;
	}
	if (!bBusy)
	{
		SetComponentTickEnabled(false);
	}
}

bool UWorkstationDevFxComponent::TickLane(FWorkstationDevFxLane& Lane, int32 LaneIndex, double Now, float Dt, bool bReduced, bool bRunning)
{
	AOfficeworker* LaneWorker = Lane.Worker.Get();
	const UEmployeeBehaviorComponent* Behavior = LaneWorker ? LaneWorker->BehaviorComponent : nullptr;
	const EFatigueSlackPhase SlackPhase = Behavior ? Behavior->GetFatigueSlackPhase() : EFatigueSlackPhase::None;
	const bool bAsleep = SlackPhase == EFatigueSlackPhase::Telegraph || SlackPhase == EFatigueSlackPhase::Slumping;
	const bool bAway = SlackPhase == EFatigueSlackPhase::Bolting;
	const bool bTyping = Lane.bDebugFlow || (Behavior && Behavior->GetState() == EEmployeeState::Typing);

	// 모니터 켜짐(물결) — 첫 화면만 「톡」
	float OnAlpha = 0.f;
	if (bActive && Lane.OnTime >= 0.0)
	{
		OnAlpha = FMath::Clamp(static_cast<float>((Now - Lane.OnTime) / DevFxTune::MonitorOnSeconds), 0.f, 1.f);
		if (OnAlpha > 0.f && !Lane.bOn)
		{
			Lane.bOn = true;
			Lane.FlashTime = Now;
			if (Lane.bPlayTickOnTurnOn)
			{
				Lane.bPlayTickOnTurnOn = false;
				UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
				if (USoundManagerSubsystem* SoundMgr = GameInstance ? GameInstance->GetSubsystem<USoundManagerSubsystem>() : nullptr)
				{
					SoundMgr->PlayUISoundWithParams(CGUISoundTags::ToggleOn, DevFxTune::WaveTickVolume, DevFxTune::WaveTickPitch);
				}
			}
		}
	}

	// 몰입 단계 — 연속 기여 3회 = 1, 6회 = 2. 졸음·이탈 중엔 0.
	const int32 NewTier = (bRunning && !bAsleep && !bAway)
		? (Lane.Streak >= DevFxTune::FlowTier2Streak ? 2 : (Lane.Streak >= DevFxTune::FlowTier1Streak ? 1 : 0)) : 0;
	if (NewTier > Lane.Tier) Lane.FlashTime = Now;
	Lane.Tier = NewTier;

	const float SinceFlash = static_cast<float>(Now - Lane.FlashTime);
	const float Flash = (!bReduced && SinceFlash < DevFxTune::FlashSeconds) ? (1.f - SinceFlash / DevFxTune::FlashSeconds) * DevFxTune::FlashGain : 0.f;
	const float BaseOpacity = OnAlpha * (bAsleep ? DevFxTune::AsleepOpacity : (bAway ? DevFxTune::AwayOpacity : DevFxTune::TierOpacity[NewTier]));
	Lane.PoolOpacity = FMath::Lerp(Lane.PoolOpacity, BaseOpacity + Flash, FMath::Clamp(Dt * DevFxTune::OpacityFollow, 0.f, 1.f));
	const bool bLit = Lane.PoolOpacity > DevFxTune::HiddenOpacity;
	if (Lane.PoolMID)
	{
		Lane.PoolMID->SetScalarParameterValue(DevFxParam::Opacity, Lane.PoolOpacity);
		Lane.PoolMID->SetVectorParameterValue(DevFxParam::GlowColor, Lane.Color);
	}
	if (Lane.Pool && Lane.Pool->IsVisible() != bLit) Lane.Pool->SetVisibility(bLit);
	if (Lane.GlowMID)
	{
		Lane.GlowMID->SetScalarParameterValue(DevFxParam::Opacity, Lane.PoolOpacity * DevFxTune::GlowRatio);
		Lane.GlowMID->SetVectorParameterValue(DevFxParam::GlowColor, bAsleep ? DevFxUtil::AsleepGlow : Lane.Color);
	}
	if (Lane.Glow && Lane.Glow->IsVisible() != bLit) Lane.Glow->SetVisibility(bLit);

	// 몰입 2단계 — 타자 빨리 + 키캡 튐
	const bool bFlowPeak = bActive && !bReduced && NewTier >= 2 && bTyping && bRunning;
	ApplyFlowRate(Lane, bFlowPeak);
	if (bFlowPeak && Now >= Lane.NextKeycapTime)
	{
		Lane.NextKeycapTime = Now + DevFxTune::KeycapInterval;
		SpawnKeycap(Lane);
	}

	const bool bKeycaps = TickKeycaps(Lane, Dt);
	TickTiles(Lane, Now, Dt);
	TickNote(Lane, LaneIndex, Now, Dt);
	const bool bChair = TickChair(Lane, Now, Dt, bReduced);

	bool bMoving = bKeycaps || bChair || Lane.bRateApplied || Lane.NotePhase == 1
		|| Now - Lane.BumpTime < DevFxTune::StackBumpSeconds
		|| (CheerStartTime >= 0.0 && Now - CheerStartTime < DevFxTune::CheerWobbleSeconds);
	for (const FWorkstationDevFxTile& Tile : Lane.Tiles)
	{
		if (Tile.Phase == 1 || Tile.Phase == 3)
		{
			bMoving = true;
			break;
		}
	}
	return bMoving;
}

void UWorkstationDevFxComponent::TickTiles(FWorkstationDevFxLane& Lane, double Now, float Dt)
{
	const FQuat LaneQuat = GetLaneQuat(Lane);
	const float S = Lane.Scale;
	bool bLayoutDirty = false;
	for (int32 TileIndex = 0; TileIndex < Lane.Tiles.Num(); ++TileIndex)
	{
		FWorkstationDevFxTile& Tile = Lane.Tiles[TileIndex];
		if (!Tile.Mesh || Tile.Phase == 0 || Tile.Phase == 2) continue;
		Tile.Age += Dt;

		if (Tile.Phase == 1)
		{
			if (Tile.Age < DevFxTune::TileFlight)
			{
				// 모니터에서 0.26초 가속 비행, 32cm 포물선
				const float FlightU = Tile.Age / DevFxTune::TileFlight;
				const float FlightK = DevFxUtil::EaseIn(FlightU);
				const FVector Location = FMath::Lerp(Tile.From, Tile.To, FlightK) + FVector(0.0, 0.0, FMath::Sin(UE_PI * FlightU) * DevFxTune::TileArc * S);
				const float Size = Tile.BaseScale * (0.7f + 0.3f * FlightK);
				Tile.Mesh->SetWorldLocationAndRotation(Location, FQuat(FVector::UpVector, (1.f - FlightK) * 0.6f) * LaneQuat);
				Tile.Mesh->SetWorldScale3D(DevFxUtil::TileScale(S, Size, Size, Size));
			}
			else if (Tile.Age < DevFxTune::TileFlight + DevFxTune::TileLand)
			{
				// 찰싹 — 0.2초 눌림
				if (!Tile.bLanded)
				{
					Tile.bLanded = true;
					Lane.BumpTime = Now;
				}
				const float LandK = (Tile.Age - DevFxTune::TileFlight) / DevFxTune::TileLand;
				const float Squash = 1.f - DevFxTune::TileSquash * FMath::Sin(UE_PI * LandK);
				const float Wide = Tile.BaseScale * (2.f - Squash);
				Tile.Mesh->SetWorldLocationAndRotation(Tile.To, LaneQuat);
				Tile.Mesh->SetWorldScale3D(DevFxUtil::TileScale(S, Wide, Wide, Tile.BaseScale * Squash));
			}
			else
			{
				Tile.Phase = 2;
				Lane.StackOrder.Add(TileIndex);
				while (Lane.StackOrder.Num() > DevFxTune::MaxStack)
				{
					const int32 Oldest = Lane.StackOrder[0];
					Lane.StackOrder.RemoveAt(0);
					ReleaseTile(Lane.Tiles[Oldest]);
				}
				bLayoutDirty = true;
			}
		}
		else if (Tile.Phase == 3)
		{
			const float LeaveK = FMath::Clamp(Tile.Age / FMath::Max(0.01f, Tile.LeaveDuration), 0.f, 1.f);
			if (LeaveK >= 1.f)
			{
				ReleaseTile(Tile);
				continue;
			}
			// 접힘은 가속, 환호 퇴장은 등속(시안 그대로)
			const float Move = Tile.LeaveEndScale > 0.f ? DevFxUtil::EaseIn(LeaveK) : LeaveK;
			const float Size = Tile.BaseScale * FMath::Lerp(1.f, Tile.LeaveEndScale, LeaveK);
			Tile.Mesh->SetWorldLocationAndRotation(FMath::Lerp(Tile.From, Tile.LeaveTo, Move), LaneQuat);
			Tile.Mesh->SetWorldScale3D(DevFxUtil::TileScale(S, Size, Size, Size));
		}
	}

	// 더미 0.94 눌림(착지마다)
	const float SinceBump = static_cast<float>(Now - Lane.BumpTime);
	const bool bBumping = SinceBump < DevFxTune::StackBumpSeconds;
	const float Bump = bBumping ? 1.f - DevFxTune::StackBump * FMath::Sin(UE_PI * SinceBump / DevFxTune::StackBumpSeconds) : 1.f;
	if (bLayoutDirty || bBumping || SinceBump < DevFxTune::StackBumpSeconds + 0.1f)
	{
		LayoutStack(Lane, Bump);
	}
}

void UWorkstationDevFxComponent::TickNote(FWorkstationDevFxLane& Lane, int32 LaneIndex, double Now, float Dt)
{
	if (!Lane.Note || Lane.NotePhase == 0) return;

	// 로컬 Z = 모니터 뒷면 바깥(좌석 반대), X = 위 — 메모지 재질 UV 가 이 축을 전제한다
	const FQuat BaseQuat = FRotationMatrix::MakeFromZX(Lane.Forward, FVector::UpVector).ToQuat();
	float Roll = DevFxTune::NoteRestRoll;
	float Size = 1.f;
	FVector Location = Lane.NoteAnchor;
	bool bShow = true;

	if (Lane.NotePhase == 1)
	{
		Lane.NoteAge += Dt;
		const float T = Lane.NoteAge;
		if (T < DevFxTune::NoteFold)
		{
			bShow = false;
		}
		else if (T < DevFxTune::NoteFold + DevFxTune::NoteFlight)
		{
			const float FlyK = DevFxUtil::EaseIn((T - DevFxTune::NoteFold) / DevFxTune::NoteFlight);
			const FVector ToControl = FMath::Lerp(Lane.NoteFrom, Lane.NoteControl, FlyK);
			const FVector ToAnchor = FMath::Lerp(Lane.NoteControl, Lane.NoteAnchor, FlyK);
			Location = FMath::Lerp(ToControl, ToAnchor, FlyK);
			Roll = (1.f - FlyK) * 2.2f + DevFxTune::NoteRestRoll;
			Size = 0.5f + 0.5f * FlyK;
		}
		else if (T < DevFxTune::NoteTotal)
		{
			const float StickK = (T - DevFxTune::NoteFold - DevFxTune::NoteFlight) / (DevFxTune::NoteTotal - DevFxTune::NoteFold - DevFxTune::NoteFlight);
			Roll = DevFxTune::NoteRestRoll + FMath::Sin(StickK * 14.f) * 0.14f * (1.f - StickK);
			Size = 1.f + 0.12f * (1.f - StickK) * FMath::Cos(StickK * 10.f);
		}
		else
		{
			Lane.NotePhase = 2;
		}
	}
	if (Lane.NotePhase == 2 && CheerStartTime >= 0.0)
	{
		// 완료 환호 — 1.6초 동안 팔랑
		const float SinceCheer = static_cast<float>(Now - CheerStartTime);
		if (SinceCheer < DevFxTune::CheerWobbleSeconds)
		{
			Roll = DevFxTune::NoteRestRoll + FMath::Sin(SinceCheer * 16.f + static_cast<float>(DeskOrder + LaneIndex)) * 0.18f
				* (1.f - SinceCheer / DevFxTune::CheerWobbleSeconds);
		}
	}

	const float Extent = Lane.NoteSize / 100.f * Size;
	Lane.Note->SetWorldLocationAndRotation(Location, FQuat(Lane.Forward, Roll) * BaseQuat);
	Lane.Note->SetWorldScale3D(FVector(Extent, Extent, 1.f));
	if (Lane.Note->IsVisible() != bShow) Lane.Note->SetVisibility(bShow);
}

bool UWorkstationDevFxComponent::TickKeycaps(FWorkstationDevFxLane& Lane, float Dt)
{
	bool bAny = false;
	const FQuat LaneQuat = GetLaneQuat(Lane);
	for (FWorkstationDevFxKeycap& Cap : Lane.Keycaps)
	{
		if (!Cap.bAlive || !Cap.Mesh) continue;
		Cap.Age += Dt;
		if (Cap.Age >= DevFxTune::KeycapLife)
		{
			Cap.bAlive = false;
			Cap.Mesh->SetVisibility(false);
			continue;
		}
		bAny = true;
		Cap.Velocity.Z -= DevFxTune::KeycapGravity * Lane.Scale * Dt;
		Cap.Location += Cap.Velocity * Dt;
		Cap.Spin += Dt * 9.f;
		const float Shrink = 1.f - Cap.Age / DevFxTune::KeycapLife * 0.5f;
		Cap.Mesh->SetWorldLocationAndRotation(Cap.Location, FQuat(Lane.Right, Cap.Spin) * LaneQuat);
		Cap.Mesh->SetWorldScale3D(FVector(0.05f, 0.05f, 0.03f) * (Lane.Scale * Shrink));
	}
	return bAny;
}

void UWorkstationDevFxComponent::ApplyFlowRate(FWorkstationDevFxLane& Lane, bool bWantFlowRate)
{
	if (!bWantFlowRate)
	{
		RestoreFlowRate(Lane);
		return;
	}
	AOfficeworker* LaneWorker = Lane.Worker.Get();
	USkeletalMeshComponent* Body = LaneWorker ? LaneWorker->GetMesh() : nullptr;
	// 반응 연출(깨우기 가속·크리 등)이 매 틱 속도를 맞추는 동안엔 비켜 준다
	if (!Body || DevFxUtil::IsReactionBusy(LaneWorker)) return;
	if (!Lane.bTrueRateKnown)
	{
		Lane.TrueRate = Body->GlobalAnimRateScale;
		Lane.bTrueRateKnown = true;
	}
	const float TargetRate = Lane.TrueRate * DevFxTune::FlowTypingRate;
	if (!FMath::IsNearlyEqual(Body->GlobalAnimRateScale, TargetRate))
	{
		Body->GlobalAnimRateScale = TargetRate;
	}
	Lane.bRateApplied = true;
}

void UWorkstationDevFxComponent::RestoreFlowRate(FWorkstationDevFxLane& Lane, bool bForce)
{
	if (!Lane.bRateApplied) return;
	AOfficeworker* LaneWorker = Lane.Worker.Get();
	USkeletalMeshComponent* Body = LaneWorker ? LaneWorker->GetMesh() : nullptr;
	if (!Body)
	{
		Lane.bRateApplied = false;
		return;
	}
	if (!bForce && DevFxUtil::IsReactionBusy(LaneWorker)) return;
	Body->GlobalAnimRateScale = Lane.TrueRate;
	Lane.bRateApplied = false;
}

void UWorkstationDevFxComponent::CaptureChair(FWorkstationDevFxLane& Lane)
{
	UStaticMeshComponent* ChairComp = Lane.Chair.Get();
	if (!ChairComp || Lane.bChairCaptured || ChairComp->Mobility != EComponentMobility::Movable) return;
	Lane.ChairBaseRelLocation = ChairComp->GetRelativeLocation();
	Lane.ChairBaseRelRotation = ChairComp->GetRelativeRotation();
	Lane.ChairBaseWorldLocation = ChairComp->GetComponentLocation();
	Lane.ChairBaseWorldRotation = ChairComp->GetComponentQuat();
	const FVector Center = ChairComp->Bounds.Origin;
	Lane.ChairPivot = FVector(Center.X, Center.Y, Lane.ChairBaseWorldLocation.Z);
	Lane.ChairPush = 0.f;
	Lane.bChairCaptured = true;
}

void UWorkstationDevFxComponent::RestoreChair(FWorkstationDevFxLane& Lane)
{
	UStaticMeshComponent* ChairComp = Lane.Chair.Get();
	if (ChairComp && Lane.bChairCaptured)
	{
		ChairComp->SetRelativeLocationAndRotation(Lane.ChairBaseRelLocation, Lane.ChairBaseRelRotation);
	}
	Lane.bChairCaptured = false;
	Lane.ChairPush = 0.f;
	Lane.SpinStartTime = -1.0;
}

bool UWorkstationDevFxComponent::TickChair(FWorkstationDevFxLane& Lane, double Now, float Dt, bool bReduced)
{
	UStaticMeshComponent* ChairComp = Lane.Chair.Get();
	if (!ChairComp || !Lane.bChairCaptured) return false;
	if (bReduced)
	{
		RestoreChair(Lane);
		return false;
	}

	// 일어서 있는 동안 의자가 뒤로 밀려 있고, 다시 앉으면 돌아온다
	const AOfficeworker* LaneWorker = Lane.Worker.Get();
	const UEmployeeBehaviorComponent* Behavior = LaneWorker ? LaneWorker->BehaviorComponent : nullptr;
	const EEmployeeState State = Behavior ? Behavior->GetState() : EEmployeeState::Sitting;
	const bool bSeatedState = State == EEmployeeState::Typing || State == EEmployeeState::Sitting || State == EEmployeeState::TypeToSit
		|| State == EEmployeeState::StandToSit || State == EEmployeeState::CheerSitting;
	const float PushTarget = bSeatedState ? 0.f : 1.f;
	Lane.ChairPush = FMath::Lerp(Lane.ChairPush, PushTarget, FMath::Clamp(Dt * DevFxTune::ChairFollow, 0.f, 1.f));
	if (bSeatedState && Lane.SpinStartTime >= 0.0 && Now >= Lane.SpinStartTime)
	{
		// 앉은 채로 의자만 돌면 몸을 뚫는다 — 일어설 때까지 미루고, 끝내 앉아 있으면 돌리지 않는다
		Lane.SpinStartTime = (CheerStartTime >= 0.0 && Now - CheerStartTime < DevFxTune::ChairSpinWaitSeconds) ? Now : -1.0;
	}

	const float SpinT = Lane.SpinStartTime >= 0.0
		? FMath::Clamp(static_cast<float>((Now - Lane.SpinStartTime) / DevFxTune::ChairSpinSeconds), 0.f, 1.f) : 1.f;
	const FQuat SpinQuat(FVector::UpVector, DevFxUtil::EaseInOut(SpinT) * 2.f * UE_PI);
	const FVector Location = Lane.ChairPivot + SpinQuat.RotateVector(Lane.ChairBaseWorldLocation - Lane.ChairPivot)
		- Lane.Forward * (DevFxTune::ChairPushCm * Lane.Scale * Lane.ChairPush);
	ChairComp->SetWorldLocationAndRotation(Location, SpinQuat * Lane.ChairBaseWorldRotation);

	if (SpinT >= 1.f && PushTarget <= 0.f && Lane.ChairPush < 0.002f)
	{
		RestoreChair(Lane);
		return false;
	}
	return true;
}

//========================================
// 검증 콘솔 명령
//========================================

#if !UE_BUILD_SHIPPING
namespace WorkstationDevFxConsole
{
	static FAutoConsoleCommandWithWorldAndArgs GWorkstationDevFxCommand(
		TEXT("CGR.Office.DevFx"),
		TEXT("책상 개발 연출을 강제로 재생한다. CGR.Office.DevFx <start|tile|crit|mile|flow|climax|reset> [직원 순번(직원ID 순, 0부터, -1 = 전원)]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* InWorld)
		{
			if (Args.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[WorkstationDevFx] 사용법: CGR.Office.DevFx <start|tile|crit|mile|flow|climax|reset> [순번]"));
				return;
			}
			const int32 TargetIndex = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
			const bool bPlayed = UWorkstationDevFxComponent::PlayDebug(InWorld, Args[0], TargetIndex);
			UE_LOG(LogTemp, Log, TEXT("[WorkstationDevFx] %s %d : %s"), *Args[0], TargetIndex,
				bPlayed ? TEXT("재생") : TEXT("재생 안 함(종류 오타이거나 책상에 앉은 직원 없음)"));
		}));
}
#endif
