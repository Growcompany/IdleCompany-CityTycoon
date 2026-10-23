#include "UI/HUD/GoalTrackerWidget.h"
#include "UI/Element/GoalTrackerRowWidget.h"
#include "Manager/GoalBoardSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"

namespace CGRGoalTrackerDisplay
{
FName PreserveExpandedRow(const TArray<FGoalBoardEntry>& InAllEntries, FName InExpandedGoalID)
{
	if (InExpandedGoalID.IsNone()) { return NAME_None; }
	const FGoalBoardEntry* Selected = InAllEntries.FindByPredicate([InExpandedGoalID](const FGoalBoardEntry& Entry)
	{
		return Entry.GoalID == InExpandedGoalID;
	});
	return Selected && (Selected->State == EGoalState::ReadyToClaim ||
		(Selected->State == EGoalState::InProgress && Selected->bRecommendable))
		? InExpandedGoalID : NAME_None;
}

void BuildDisplaySet(
	const TArray<FGoalBoardEntry>& InAllEntries,
	FName InTrackedGoalID,
	FName InRecommendedGoalID,
	bool bInRowsExpanded,
	TArray<FGoalBoardEntry>& OutVisible,
	int32& OutHiddenCount)
{
	OutVisible.Reset();
	OutHiddenCount = 0;
	TArray<FGoalBoardEntry> Available;
	for (const FGoalBoardEntry& Entry : InAllEntries)
	{
		if (Entry.State == EGoalState::ReadyToClaim ||
			(Entry.State == EGoalState::InProgress && Entry.bRecommendable))
		{
			Available.Add(Entry);
		}
	}
	if (bInRowsExpanded)
	{
		OutVisible = MoveTemp(Available);
		return;
	}
	const auto FindAvailable = [&Available](FName GoalID) -> const FGoalBoardEntry*
	{
		return Available.FindByPredicate([GoalID](const FGoalBoardEntry& Entry) { return Entry.GoalID == GoalID; });
	};
	const FGoalBoardEntry* Selected = InRecommendedGoalID.IsNone() ? nullptr : FindAvailable(InRecommendedGoalID);
	if (!Selected && !InTrackedGoalID.IsNone()) { Selected = FindAvailable(InTrackedGoalID); }
	if (!Selected && !Available.IsEmpty()) { Selected = &Available[0]; }
	if (Selected)
	{
		OutVisible.Add(*Selected);
		OutHiddenCount = Available.Num() - 1;
	}
}
}

UGoalBoardSubsystem* UGoalTrackerWidget::GetBoard() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UGoalBoardSubsystem>() : nullptr;
}

void UGoalTrackerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (FoldButton) { FoldButton->OnClicked.AddDynamic(this, &UGoalTrackerWidget::HandleFoldClicked); }
	if (UnfoldButton) { UnfoldButton->OnClicked.AddDynamic(this, &UGoalTrackerWidget::HandleUnfoldClicked); }
	if (MoreButton) { MoreButton->OnClicked.AddDynamic(this, &UGoalTrackerWidget::HandleMoreClicked); }
	InitTracker();
	if (bPresentationSuppressed) { SetVisibility(ESlateVisibility::Hidden); }
}

void UGoalTrackerWidget::NativeDestruct()
{
	if (FoldButton) { FoldButton->OnClicked.RemoveDynamic(this, &UGoalTrackerWidget::HandleFoldClicked); }
	if (UnfoldButton) { UnfoldButton->OnClicked.RemoveDynamic(this, &UGoalTrackerWidget::HandleUnfoldClicked); }
	if (MoreButton) { MoreButton->OnClicked.RemoveDynamic(this, &UGoalTrackerWidget::HandleMoreClicked); }
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->OnGoalBoardChanged.RemoveAll(this);
		Board->OnTrackedGoalChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UGoalTrackerWidget::SetPresentationSuppressed(bool bSuppressed)
{
	if (bPresentationSuppressed == bSuppressed) { return; }
	bPresentationSuppressed = bSuppressed;
	if (bSuppressed)
	{
		VisibilityBeforePresentation = GetVisibility();
		SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetVisibility(VisibilityBeforePresentation);
		RefreshAll();
	}
}

void UGoalTrackerWidget::InitTracker()
{
	UGoalBoardSubsystem* Board = GetBoard();
	if (!Board)
	{
		UE_LOG(LogTemp, Error, TEXT("[GoalTracker] GoalBoardSubsystem missing"));
		return;
	}
	Board->OnGoalBoardChanged.RemoveAll(this);
	Board->OnTrackedGoalChanged.RemoveAll(this);
	Board->OnGoalBoardChanged.AddUObject(this, &UGoalTrackerWidget::HandleBoardChanged);
	Board->OnTrackedGoalChanged.AddUObject(this, &UGoalTrackerWidget::HandleTrackedChanged);
	UGameInstance* GI = GetGameInstance();
	UMissionManagerSubsystem* MissionMgr = GI ? GI->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
	const bool bShouldSuppress = MissionMgr && MissionMgr->ShouldHideGoalTracker();
	const bool bResuming = bPresentationSuppressed && !bShouldSuppress;
	if (bPresentationSuppressed != bShouldSuppress) { SetPresentationSuppressed(bShouldSuppress); }
	if (!bResuming) { RefreshAll(); }
}

void UGoalTrackerWidget::RefreshAll()
{
	UGoalBoardSubsystem* Board = GetBoard();
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Board || !RowBox) { return; }
	TArray<FGoalBoardEntry> AllEntries;
	Board->GetBoardEntries(AllEntries);
	const FName TrackedID = Board->GetTrackedGoalID();
	const bool bRowsExpanded = Board->AreRowsExpanded();
	ExpandedGoalID = CGRGoalTrackerDisplay::PreserveExpandedRow(AllEntries, ExpandedGoalID);
	TArray<FGoalBoardEntry> Entries;
	int32 HiddenCount = 0;
	CGRGoalTrackerDisplay::BuildDisplaySet(AllEntries, TrackedID, Board->GetRecommendedGoalID(),
		bRowsExpanded, Entries, HiddenCount);
	const bool bFolded = Board->IsTrackerFolded();
	const bool bHasGuide = !Entries.IsEmpty() || HiddenCount > 0;
	if (ExpandedRoot) { ExpandedRoot->SetVisibility(bFolded || !bHasGuide ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible); }
	if (FoldedRoot) { FoldedRoot->SetVisibility(bFolded && bHasGuide ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
	if (HeaderCountText) { HeaderCountText->SetText(NSLOCTEXT("GoalTracker", "GrowthQuest", "성장 퀘스트")); }
	if (FoldedCountText) { FoldedCountText->SetText(NSLOCTEXT("GoalTracker", "FoldedGrowthQuest", "성장 퀘스트")); }
	if (MoreButton)
	{
		MoreButton->SetVisibility(HiddenCount > 0 || (bRowsExpanded && Entries.Num() > 1)
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (MoreLabel)
	{
		MoreLabel->SetText(bRowsExpanded
			? NSLOCTEXT("GoalTracker", "OneQuest", "현재 목표만 보기")
			: NSLOCTEXT("GoalTracker", "OtherQuests", "다른 목표"));
	}
	if (bFolded) { return; }

	FText PlaceHint;
	if (!TrackedID.IsNone())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
			{
				PlaceHint = MissionMgr->GetTrackedGoalPlaceHint();
			}
		}
	}
	const TSubclassOf<UUserWidget> RowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::GoalTrackerRow) : nullptr;
	if (!RowClass)
	{
		if (!bLoggedMissingRowClass)
		{
			bLoggedMissingRowClass = true;
			UE_LOG(LogTemp, Error, TEXT("[GoalTracker] GoalTrackerRow class missing from DT_WidgetClass"));
		}
		return;
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (!RowPool.IsValidIndex(Index))
		{
			UGoalTrackerRowWidget* NewRow = CreateWidget<UGoalTrackerRowWidget>(GetOwningPlayer(), RowClass);
			if (!NewRow)
			{
				UE_LOG(LogTemp, Error, TEXT("[GoalTracker] GoalTrackerRow construction failed"));
				break;
			}
			NewRow->OnRowTapped.AddUObject(this, &UGoalTrackerWidget::HandleRowTapped);
			NewRow->OnStartRequested.AddUObject(this, &UGoalTrackerWidget::HandleStartRequested);
			NewRow->OnStopRequested.AddUObject(this, &UGoalTrackerWidget::HandleStopRequested);
			NewRow->OnDismissRequested.AddUObject(this, &UGoalTrackerWidget::HandleDismissRequested);
			NewRow->OnClaimRequested.AddUObject(this, &UGoalTrackerWidget::HandleClaimRequested);
			RowPool.Add(NewRow);
			if (UVerticalBoxSlot* BoxSlot = RowBox->AddChildToVerticalBox(NewRow))
			{
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			}
		}
		UGoalTrackerRowWidget* Row = RowPool[Index];
		if (!Row) { continue; }
		Row->SetVisibility(ESlateVisibility::Visible);
		Row->Configure(Entries[Index], Entries[Index].GoalID == TrackedID,
			bRowsExpanded ? Entries[Index].GoalID == ExpandedGoalID
				: Entries[Index].State == EGoalState::ReadyToClaim || Entries[Index].GoalID != CollapsedHintGoalID,
			PlaceHint);
	}
	for (int32 Index = Entries.Num(); Index < RowPool.Num(); ++Index)
	{
		if (RowPool[Index]) { RowPool[Index]->SetVisibility(ESlateVisibility::Collapsed); }
	}
}

void UGoalTrackerWidget::HandleFoldClicked()
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		ExpandedGoalID = NAME_None;
		CollapsedHintGoalID = NAME_None;
		Board->SetTrackerFolded(true);
	}
}

void UGoalTrackerWidget::HandleUnfoldClicked()
{
	if (UGoalBoardSubsystem* Board = GetBoard()) { Board->SetTrackerFolded(false); }
}

void UGoalTrackerWidget::HandleMoreClicked()
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		const bool bWasExpanded = Board->AreRowsExpanded();
		if (bWasExpanded)
		{
			ExpandedGoalID = NAME_None;
			CollapsedHintGoalID = NAME_None;
			if (RowScrollBox) { RowScrollBox->ScrollToStart(); }
		}
		Board->SetRowsExpanded(!bWasExpanded);
	}
}

void UGoalTrackerWidget::HandleBoardChanged() { RefreshAll(); }
void UGoalTrackerWidget::HandleTrackedChanged(FName /*TrackedID*/) { RefreshAll(); }

void UGoalTrackerWidget::HandleRowTapped(FName GoalID)
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		if (!Board->AreRowsExpanded())
		{
			CollapsedHintGoalID = CollapsedHintGoalID == GoalID ? NAME_None : GoalID;
			RefreshAll();
			return;
		}
	}
	ExpandedGoalID = GoalID;
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->SetActiveGoal(GoalID);
	}
	RefreshAll();
	if (RowScrollBox && !ExpandedGoalID.IsNone())
	{
		for (UGoalTrackerRowWidget* Row : RowPool)
		{
			if (Row && Row->GetGoalID() == ExpandedGoalID)
			{
				RowScrollBox->ScrollWidgetIntoView(Row, true);
				break;
			}
		}
	}
}

void UGoalTrackerWidget::HandleStartRequested(FName GoalID)
{
	if (UGoalBoardSubsystem* Board = GetBoard()) { Board->SetTrackedGoal(GoalID); }
}

void UGoalTrackerWidget::HandleStopRequested(FName GoalID)
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		Board->SetTrackedGoal(NAME_None);
		Board->DismissGoal(GoalID);
	}
}

void UGoalTrackerWidget::HandleDismissRequested(FName GoalID)
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		if (Board->GetTrackedGoalID() == GoalID) { Board->SetTrackedGoal(NAME_None); }
		Board->DismissGoal(GoalID);
	}
}

void UGoalTrackerWidget::HandleClaimRequested(FName GoalID, int32 ExpectedClaimCount)
{
	if (UGoalBoardSubsystem* Board = GetBoard())
	{
		TArray<FGoalBoardEntry> Entries;
		Board->GetBoardEntries(Entries);
		const FGoalBoardEntry* ClaimEntry = Entries.FindByPredicate([GoalID, ExpectedClaimCount](const FGoalBoardEntry& Entry)
		{
			return Entry.GoalID == GoalID && Entry.ClaimCount == ExpectedClaimCount && Entry.State == EGoalState::ReadyToClaim;
		});
		if (!ClaimEntry) { return; }
		const int64 RewardMoney = ClaimEntry->Row.RewardMoney;
		if (!Board->ClaimGoal(GoalID, ExpectedClaimCount)) { return; }
		ExpandedGoalID = NAME_None;
		CollapsedHintGoalID = NAME_None;
		RefreshAll();
		if (RewardMoney > 0)
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
				{
					UIMgr->ShowFundsToast(RewardMoney);
				}
			}
		}
	}
}
