#include "Office/OfficeTVNewsDesk.h"

#include "Core/CGGameInstance.h"
#include "Data/EmployeeTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Manager/EmployeeManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "UI/Panel/EventChoicePanelWidget.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr int32 RecentWorkLimit = 3;
	// Orbs spawn 80 units above the worker that produced them.
	constexpr float OrbHeightOffset = 80.f;
	constexpr float WorkerMatchRadius = 250.f;
}

void UOfficeTVNewsDesk::Start(UWorld* InWorld)
{
	Stop();
	World = InWorld;
	if (!InWorld)
	{
		return;
	}
	if (UOfficeStageProgressManager* StageManager = InWorld->GetSubsystem<UOfficeStageProgressManager>())
	{
		Stage = StageManager;
		StageManager->OnScoreOrbRequested.AddUniqueDynamic(this, &ThisClass::HandleScoreOrb);
		StageManager->OnFeverChanged.AddUniqueDynamic(this, &ThisClass::HandleFever);
		StageManager->OnProjectEventTriggered.AddUniqueDynamic(this, &ThisClass::HandleProjectEvent);
		StageManager->OnTierUnlocked.AddUniqueDynamic(this, &ThisClass::HandleTierUnlocked);
		StageManager->OnLifecycleChanged.AddUniqueDynamic(this, &ThisClass::HandleLifecycle);
	}
	if (ULaunchReactionSubsystem* ReactionSource = InWorld->GetSubsystem<ULaunchReactionSubsystem>())
	{
		Reactions = ReactionSource;
		ReactionSource->OnReactionReady.AddUniqueDynamic(this, &ThisClass::HandleReaction);
	}
	if (UGameInstance* Game = InWorld->GetGameInstance())
	{
		if (UEmployeeManager* EmployeeSource = Game->GetSubsystem<UEmployeeManager>())
		{
			Employees = EmployeeSource;
			EmployeeSource->OnEmployeeLevelUp.AddUObject(this, &ThisClass::HandleLevelUp);
			EmployeeSource->OnEmployeeHireCompleted.AddUObject(this, &ThisClass::HandleHire);
		}
	}
	RememberLevels();
}

void UOfficeTVNewsDesk::Stop()
{
	UnbindChoicePanel();
	if (UOfficeStageProgressManager* StageManager = Stage.Get())
	{
		StageManager->OnScoreOrbRequested.RemoveAll(this);
		StageManager->OnFeverChanged.RemoveAll(this);
		StageManager->OnProjectEventTriggered.RemoveAll(this);
		StageManager->OnTierUnlocked.RemoveAll(this);
		StageManager->OnLifecycleChanged.RemoveAll(this);
	}
	if (ULaunchReactionSubsystem* ReactionSource = Reactions.Get())
	{
		ReactionSource->OnReactionReady.RemoveAll(this);
	}
	if (UEmployeeManager* EmployeeSource = Employees.Get())
	{
		EmployeeSource->OnEmployeeLevelUp.RemoveAll(this);
		EmployeeSource->OnEmployeeHireCompleted.RemoveAll(this);
	}
	Stage.Reset();
	Reactions.Reset();
	Employees.Reset();
	World.Reset();
	bEventPending = false;
	LaunchRecord = FOfficeTVLaunchRecord();
	KnownLevels.Reset();
	Buffers = FOfficeTVNewsBuffers();
	Interrupts.Reset();
	LaunchSequence.Reset();
	TierUps.Reset();
}

void UOfficeTVNewsDesk::Tick()
{
	if (bEventPending && !ChoicePanel.IsValid())
	{
		TryBindChoicePanel();
	}
	const UOfficeStageProgressManager* StageManager = Stage.Get();
	if (!LaunchRecord.bValid && StageManager && StageManager->GetLifecycle() == EProjectLifecycle::LaunchPending)
	{
		OfficeTVBroadcastSource::CaptureLaunchRecord(World.Get(), LaunchRecord);
	}
}

void UOfficeTVNewsDesk::NoteOperationTick(int32 BuildingID, double TotalRevenue)
{
	Buffers.TickBuilding = BuildingID;
	Buffers.TickTotalRevenue = TotalRevenue;
}

void UOfficeTVNewsDesk::TakeNews(TArray<FOfficeTVBroadcastFrame>& OutInterrupts, TArray<FOfficeTVBroadcastFrame>& OutPriority)
{
	OutInterrupts = MoveTemp(Interrupts);
	OutPriority = MoveTemp(LaunchSequence);
	OutPriority.Append(MoveTemp(TierUps));
	Interrupts.Reset();
	LaunchSequence.Reset();
	TierUps.Reset();
}

void UOfficeTVNewsDesk::HandleScoreOrb(FVector WorldPosition, int32 StepNumber, float Score, bool bIsCritical,
	int32 /*OrbCount*/, bool /*bPlaySpectacle*/)
{
	const UOfficeStageProgressManager* StageManager = Stage.Get();
	const int32 EmployeeId = ResolveWorker(WorldPosition);
	if (!StageManager || EmployeeId == INDEX_NONE)
	{
		return;
	}
	const TArray<FStepRoundData>& Steps = StageManager->GetProgressData().Steps;
	const int32 Slot = Steps.IsValidIndex(StepNumber - 1) ? Steps[StepNumber - 1].DisciplineSlot : INDEX_NONE;

	FOfficeTVNewsBuffers::FWork Work;
	Work.EmployeeId = EmployeeId;
	Work.Slot = Slot;
	Work.Score = Score;
	Buffers.RecentWork.Insert(Work, 0);
	if (Buffers.RecentWork.Num() > RecentWorkLimit)
	{
		Buffers.RecentWork.SetNum(RecentWorkLimit);
	}

	FOfficeTVBroadcastFrame Frame;
	if (bIsCritical && OfficeTVBroadcastSource::BuildCritical(World.Get(), EmployeeId, Slot, Score, Frame))
	{
		Interrupts.Add(MoveTemp(Frame));
	}
}

void UOfficeTVNewsDesk::HandleFever(bool bActive)
{
	FOfficeTVBroadcastFrame Frame;
	if (bActive && OfficeTVBroadcastSource::BuildFever(World.Get(), Frame))
	{
		Interrupts.Add(MoveTemp(Frame));
	}
}

void UOfficeTVNewsDesk::HandleProjectEvent(const FProjectEventData& EventData)
{
	UnbindChoicePanel();
	PendingEvent = EventData;
	bEventPending = true;
	TryBindChoicePanel();
}

void UOfficeTVNewsDesk::HandleEventChoice(int32 ChoiceIndex)
{
	FOfficeTVBroadcastFrame Frame;
	if (bEventPending && OfficeTVBroadcastSource::BuildIssueResult(World.Get(), PendingEvent, ChoiceIndex, Frame))
	{
		Interrupts.Add(MoveTemp(Frame));
	}
	bEventPending = false;
	UnbindChoicePanel();
}

void UOfficeTVNewsDesk::HandleReaction(int32 BuildingID, const FLaunchReaction& Reaction)
{
	if (BuildingID != CurrentBuilding() || Reaction.Comment.IsEmpty())
	{
		return;
	}
	FOfficeTVBroadcastEntry Entry;
	Entry.Title = Reaction.Comment;
	Entry.Caption = Reaction.Source;
	Entry.bHighlighted = Reaction.Kind == FName(TEXT("Critic"));
	if (Reaction.Score > 0)
	{
		Entry.Value = FText::AsNumber(Reaction.Score);
	}
	FOfficeTVBroadcastFrame Frame;
	if (OfficeTVBroadcastSource::BuildReaction(World.Get(), Entry, Frame))
	{
		Interrupts.Add(MoveTemp(Frame));
	}
}

void UOfficeTVNewsDesk::HandleTierUnlocked(int32 NewTier)
{
	FOfficeTVBroadcastFrame Frame;
	if (OfficeTVBroadcastSource::BuildTierUp(World.Get(), NewTier, Frame))
	{
		TierUps.Add(MoveTemp(Frame));
	}
}

void UOfficeTVNewsDesk::HandleLifecycle(EProjectLifecycle OldState, EProjectLifecycle NewState)
{
	if (NewState == EProjectLifecycle::Developing)
	{
		Buffers = FOfficeTVNewsBuffers();
		LaunchRecord = FOfficeTVLaunchRecord();
		RememberLevels();
	}
	else if (NewState == EProjectLifecycle::LaunchPending)
	{
		OfficeTVBroadcastSource::CaptureLaunchRecord(World.Get(), LaunchRecord);
	}
	else if (OldState == EProjectLifecycle::LaunchPending && NewState == EProjectLifecycle::Operating)
	{
		TArray<FOfficeTVBroadcastFrame> Sequence;
		OfficeTVBroadcastSource::BuildLaunchSequence(World.Get(), LaunchRecord, Sequence);
		LaunchSequence.Append(MoveTemp(Sequence));
		LaunchRecord = FOfficeTVLaunchRecord();
	}
	else if (OldState == EProjectLifecycle::LaunchPending)
	{
		LaunchRecord = FOfficeTVLaunchRecord();
	}
}

void UOfficeTVNewsDesk::HandleLevelUp(int32 EmployeeId, int32 NewLevel)
{
	const int32* Known = KnownLevels.Find(EmployeeId);
	const int32 OldLevel = Known ? *Known : NewLevel - 1;
	KnownLevels.Add(EmployeeId, NewLevel);
	UEmployeeManager* EmployeeSource = Employees.Get();
	const FEmployeeInstance* Employee = EmployeeSource ? EmployeeSource->FindEmployee(EmployeeId) : nullptr;
	if (!Employee || !Employee->bIsAssigned || Employee->AssignedBuildingIndex != CurrentBuilding())
	{
		return;
	}
	FOfficeTVBroadcastFrame Frame;
	if (OfficeTVBroadcastSource::BuildPromotion(World.Get(), EmployeeId, OldLevel, NewLevel, Frame))
	{
		Interrupts.Add(MoveTemp(Frame));
	}
}

void UOfficeTVNewsDesk::HandleHire(const FString& EmployeeId)
{
	const int32 Id = FCString::Atoi(*EmployeeId);
	FOfficeTVBroadcastFrame Frame;
	if (OfficeTVBroadcastSource::BuildNewHire(World.Get(), Id, Frame))
	{
		if (UEmployeeManager* EmployeeSource = Employees.Get())
		{
			if (const FEmployeeInstance* Employee = EmployeeSource->FindEmployee(Id))
			{
				KnownLevels.Add(Id, Employee->Level);
			}
		}
		Interrupts.Add(MoveTemp(Frame));
	}
}

int32 UOfficeTVNewsDesk::ResolveWorker(const FVector& OrbPosition) const
{
	UWorld* InWorld = World.Get();
	if (!InWorld)
	{
		return INDEX_NONE;
	}
	const FVector Origin = OrbPosition - FVector(0.f, 0.f, OrbHeightOffset);
	int32 BestId = INDEX_NONE;
	double BestDistance = FMath::Square(WorkerMatchRadius);
	for (TActorIterator<AOfficeworker> It(InWorld); It; ++It)
	{
		const double Distance = FVector::DistSquared(It->GetActorLocation(), Origin);
		if (It->GetEmployeeID() != INDEX_NONE && Distance <= BestDistance)
		{
			BestDistance = Distance;
			BestId = It->GetEmployeeID();
		}
	}
	return BestId;
}

int32 UOfficeTVNewsDesk::CurrentBuilding() const
{
	const UWorld* InWorld = World.Get();
	const UCGGameInstance* Game = InWorld ? Cast<UCGGameInstance>(InWorld->GetGameInstance()) : nullptr;
	return Game ? Game->GetCurrentManagedBuildingIndex() : INDEX_NONE;
}

bool UOfficeTVNewsDesk::TryBindChoicePanel()
{
	// The event modal is created by the office HUD from the same signal, so it may appear a tick later.
	const UWorld* InWorld = World.Get();
	for (TObjectIterator<UEventChoicePanelWidget> It; It; ++It)
	{
		UEventChoicePanelWidget* Panel = *It;
		if (Panel && !Panel->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && Panel->GetWorld() == InWorld
			&& Panel->IsActivated())
		{
			Panel->OnChoiceMade.AddUniqueDynamic(this, &ThisClass::HandleEventChoice);
			ChoicePanel = Panel;
			return true;
		}
	}
	return false;
}

void UOfficeTVNewsDesk::UnbindChoicePanel()
{
	if (UEventChoicePanelWidget* Panel = ChoicePanel.Get())
	{
		Panel->OnChoiceMade.RemoveAll(this);
	}
	ChoicePanel.Reset();
}

void UOfficeTVNewsDesk::RememberLevels()
{
	KnownLevels.Reset();
	if (const UEmployeeManager* EmployeeSource = Employees.Get())
	{
		for (const FEmployeeInstance& Employee : EmployeeSource->GetAllEmployeesRef())
		{
			KnownLevels.Add(Employee.EmployeeID, Employee.Level);
		}
	}
}
