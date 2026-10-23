// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/QuestPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/WidgetSwitcher.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/QuestSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "UI/Element/Common/ListEntryUtils.h"
#include "UI/Element/LiveOps/QuestRowWidget.h"

namespace
{
	// 레일 탭 순서 = 인덱스 계약 (저작 스크립트의 rail_tabs 와 같은 순서)
	EQuestPeriod PeriodForTab(int32 TabIndex)
	{
		return TabIndex == 2 ? EQuestPeriod::Lifetime : (TabIndex == 1 ? EQuestPeriod::Weekly : EQuestPeriod::Daily);
	}
}

void UQuestPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 탭 그룹 생성·초기 선택은 NativeConstruct 에서만 (CLAUDE.md 탭 패턴)
	TGuardValue<bool> RefreshGuard(bSuppressRefresh, true);
	TabGroup = NewObject<UCommonButtonGroupBase>(this);
	TabGroup->SetSelectionRequired(true);
	for (UIconWithButtonWidget* Tab : { DailyTab.Get(), WeeklyTab.Get(), LifetimeTab.Get() })
	{
		if (!Tab)
		{
			continue;
		}
		TabGroup->AddWidget(Tab);
		Tab->SetIsSelectable(true);
	}
	TabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UQuestPanelWidget::HandleTabChanged);

	if (ContentSwitcher)
	{
		// 페이지 내용이 레일 탭의 클릭을 가로채지 않게 (CLAUDE.md 탭 패턴)
		ContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);
		ContentSwitcher->SetActiveWidgetIndex(0);
	}
	CurrentPeriod = EQuestPeriod::Daily;
	TabGroup->SelectButtonAtIndex(0);
	ApplyTabInks(0);

	if (ClaimAllButton)
	{
		ClaimAllButton->OnClicked().AddUObject(this, &UQuestPanelWidget::HandleClaimAll);
	}
	if (ClaimAllButtonLifetime)
	{
		ClaimAllButtonLifetime->OnClicked().AddUObject(this, &UQuestPanelWidget::HandleClaimAll);
	}
	// 엔트리는 풀에서 재사용된다 — 생성 방송마다 전체를 다시 건다 (공용 헬퍼)
	if (QuestList)
	{
		QuestList->OnEntryWidgetGenerated().AddUObject(this, &UQuestPanelWidget::HandleEntryGenerated);
	}
	if (LifetimeList)
	{
		LifetimeList->OnEntryWidgetGenerated().AddUObject(this, &UQuestPanelWidget::HandleEntryGenerated);
	}
}

void UQuestPanelWidget::NativeDestruct()
{
	if (TabGroup)
	{
		TabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		// 풀에서 재사용되는 창이라 등록을 안 풀면 같은 버튼이 다음 오픈의 새 그룹과 옛 그룹 양쪽에 남는다
		TabGroup->RemoveAll();
	}
	if (ClaimAllButton)
	{
		ClaimAllButton->OnClicked().RemoveAll(this);
	}
	if (ClaimAllButtonLifetime)
	{
		ClaimAllButtonLifetime->OnClicked().RemoveAll(this);
	}
	if (QuestList)
	{
		QuestList->OnEntryWidgetGenerated().RemoveAll(this);
	}
	if (LifetimeList)
	{
		LifetimeList->OnEntryWidgetGenerated().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UQuestPanelWidget::NativeOnActivated()
{
	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UQuestSubsystem* Quest = GameInst->GetSubsystem<UQuestSubsystem>())
		{
			Quest->OnQuestBoardChanged.AddUObject(this, &UQuestPanelWidget::HandleBoardChanged);
		}
		// 메뉴 타일이 남긴 탭 힌트 — 업적 타일로 들어오면 업적 탭이 열린 채 시작한다
		if (UUIManagerSubsystem* UIMgr = GameInst->GetSubsystem<UUIManagerSubsystem>())
		{
			TGuardValue<bool> RefreshGuard(bSuppressRefresh, true);
			if (UIMgr->ConsumePendingPanelTab() == TEXT("Lifetime") && TabGroup)
			{
				TabGroup->SelectButtonAtIndex(2);
			}
		}
	}

	StartResetCountdown();

	Super::NativeOnActivated();   // → RefreshAll
}

void UQuestPanelWidget::NativeOnDeactivated()
{
	Countdown.Stop();

	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UQuestSubsystem* Quest = GameInst->GetSubsystem<UQuestSubsystem>())
		{
			Quest->OnQuestBoardChanged.RemoveAll(this);
		}
	}

	// 입력 모드 복원은 베이스(ULiveOpsModalWidget)가 한다
	Super::NativeOnDeactivated();
}

void UQuestPanelWidget::HandleTabChanged(UCommonButtonBase* /*Button*/, int32 ButtonIndex)
{
	ApplyTabInks(ButtonIndex);
	CurrentPeriod = PeriodForTab(ButtonIndex);
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(CurrentPeriod == EQuestPeriod::Lifetime ? 1 : 0);
	}

	StartResetCountdown();
	RefreshAll();
}

void UQuestPanelWidget::HandleBoardChanged()
{
	RefreshAll();
}

void UQuestPanelWidget::RefreshAll()
{
	if (bSuppressRefresh)
	{
		return;
	}

	if (CurrentPeriod == EQuestPeriod::Lifetime)
	{
		RefreshLifetimePage();
	}
	else
	{
		RefreshPeriodPage();
	}

	RefreshTabDots();
	Countdown.Refresh();
}

void UQuestPanelWidget::FillList(UListView* TargetList, TArray<TObjectPtr<UQuestRowItem>>& TargetItems, EQuestPeriod Period)
{
	UGameInstance* GameInst = GetGameInstance();
	UQuestSubsystem* Quest = GameInst ? GameInst->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quest)
	{
		return;
	}

	TArray<FQuestEntry> Entries;
	Quest->GetEntries(Period, Entries);

	const bool bServerTimeValid = IsServerTimeValidNow();
	CGListEntry::SyncItems<UQuestRowItem>(TargetItems, Entries.Num(), this, TargetList);
	for (int32 Index = 0; Index < TargetItems.Num(); ++Index)
	{
		TargetItems[Index]->Entry = Entries[Index];
		TargetItems[Index]->bServerTimeValid = bServerTimeValid;
	}

	// 아이템 객체가 재사용돼 ListView 가 "같은 아이템" 으로 보고 넘어가는 경우가 있어 표시를 강제한다
	if (TargetList)
	{
		TargetList->RegenerateAllEntries();
	}
}

void UQuestPanelWidget::RefreshPeriodPage()
{
	FillList(QuestList, PeriodItems, CurrentPeriod);
	RefreshSummary();

	UGameInstance* GameInst = GetGameInstance();
	const UQuestSubsystem* Quest = GameInst ? GameInst->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (ClaimAllButton)
	{
		ClaimAllButton->SetIsEnabled(Quest && Quest->HasClaimable(CurrentPeriod) && IsServerTimeValidNow());
	}
}

void UQuestPanelWidget::RefreshLifetimePage()
{
	FillList(LifetimeList, LifetimeItems, EQuestPeriod::Lifetime);

	UGameInstance* GameInst = GetGameInstance();
	UQuestSubsystem* Quest = GameInst ? GameInst->GetSubsystem<UQuestSubsystem>() : nullptr;
	UTableManagerSubsystem* TableMgr = GameInst ? GameInst->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Quest || !TableMgr)
	{
		return;
	}

	int32 TotalTiers = 0;
	for (const TPair<FName, FQuestTable>& Pair : TableMgr->GetAllQuestData())
	{
		if (Pair.Value.Period == EQuestPeriod::Lifetime)
		{
			++TotalTiers;
		}
	}
	const int32 Achieved = Quest->GetQuestData().ClaimedLifetime.Num();

	int32 ClaimableCount = 0;
	for (const TObjectPtr<UQuestRowItem>& RowItem : LifetimeItems)
	{
		if (RowItem && RowItem->Entry.State == EQuestState::Claimable)
		{
			++ClaimableCount;
		}
	}

	if (LifetimeSummaryText)
	{
		LifetimeSummaryText->SetText(FText::FromString(FString::Printf(TEXT("달성 %d / %d"), Achieved, TotalTiers)));
	}
	if (LifetimeClaimableText)
	{
		LifetimeClaimableText->SetText(
			FText::FromString(FString::Printf(TEXT("받을 수 있는 보상 %d"), ClaimableCount)));
	}
	if (ClaimAllButtonLifetime)
	{
		ClaimAllButtonLifetime->SetIsEnabled(ClaimableCount > 0 && IsServerTimeValidNow());
	}
}

void UQuestPanelWidget::RefreshSummary()
{
	int32 DoneCount = 0;
	for (const TObjectPtr<UQuestRowItem>& RowItem : PeriodItems)
	{
		if (RowItem && RowItem->Entry.State != EQuestState::InProgress)
		{
			++DoneCount;
		}
	}
	const int32 TotalCount = PeriodItems.Num();

	if (SummaryTitleText)
	{
		SummaryTitleText->SetText(FText::FromString(CurrentPeriod == EQuestPeriod::Weekly ? TEXT("이번 주 미션") : TEXT("오늘의 미션")));
	}
	if (SummaryDoneText)
	{
		SummaryDoneText->SetText(FText::AsNumber(DoneCount));
	}
	if (SummaryTotalText)
	{
		SummaryTotalText->SetText(FText::FromString(FString::Printf(TEXT(" / %d 완료"), TotalCount)));
	}

	if (!PipBox || !WidgetTree)
	{
		return;
	}
	// 줄 수가 같으면 점을 재사용하고 브러시만 바꾼다 — 수령마다 RefreshAll 이 돈다
	if (PipBox->GetChildrenCount() != TotalCount)
	{
		PipBox->ClearChildren();
		for (int32 Index = 0; Index < TotalCount; ++Index)
		{
			UImage* Pip = WidgetTree->ConstructWidget<UImage>();
			if (UHorizontalBoxSlot* PipSlot = PipBox->AddChildToHorizontalBox(Pip))
			{
				PipSlot->SetPadding(FMargin(Index > 0 ? PipGap : 0.f, 0.f, 0.f, 0.f));
				PipSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
	for (int32 Index = 0; Index < TotalCount; ++Index)
	{
		const UQuestRowItem* RowItem = PeriodItems[Index];
		if (UImage* Pip = Cast<UImage>(PipBox->GetChildAt(Index)))
		{
			const bool bDone = RowItem && RowItem->Entry.State != EQuestState::InProgress;
			Pip->SetBrush(bDone ? PipOnBrush : PipOffBrush);
		}
	}
}

void UQuestPanelWidget::RefreshTabDots()
{
	UGameInstance* GameInst = GetGameInstance();
	const UQuestSubsystem* Quest = GameInst ? GameInst->GetSubsystem<UQuestSubsystem>() : nullptr;

	auto ApplyDot = [Quest](UAlertMarkWidget* Dot, EQuestPeriod Period)
	{
		if (!Dot)
		{
			return;
		}
		if (Quest && Quest->HasClaimable(Period))
		{
			Dot->Show();
		}
		else
		{
			Dot->Hide();
		}
	};

	ApplyDot(DailyTabDot, EQuestPeriod::Daily);
	ApplyDot(WeeklyTabDot, EQuestPeriod::Weekly);
	ApplyDot(LifetimeTabDot, EQuestPeriod::Lifetime);
}

void UQuestPanelWidget::ApplyTabInks(int32 SelectedIndex)
{
	UIconWithButtonWidget* const Tabs[] = { DailyTab, WeeklyTab, LifetimeTab };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tabs); ++Index)
	{
		if (Tabs[Index])
		{
			Tabs[Index]->SetTextColor(Index == SelectedIndex ? RailInkSelected : RailInkNormal);
		}
	}
}

void UQuestPanelWidget::StartResetCountdown()
{
	Countdown.Stop();
	if (CurrentPeriod == EQuestPeriod::Lifetime)
	{
		// 업적은 리셋이 없다 — 카운트다운 자리는 주기판에만 있다
		return;
	}

	const EQuestPeriod Period = CurrentPeriod;
	Countdown.Start(GetWorld(), ResetText,
		[this, Period]() -> int64
		{
			UGameInstance* TickGameInst = GetGameInstance();
			const UGameClockSubsystem* Clock = TickGameInst ? TickGameInst->GetSubsystem<UGameClockSubsystem>() : nullptr;
			if (!Clock)
			{
				return 0;
			}
			return Period == EQuestPeriod::Weekly ? Clock->GetSecondsUntilWeekRoll() : Clock->GetSecondsUntilDayRoll();
		},
		TEXT("초기화까지 "));
}

void UQuestPanelWidget::HandleEntryGenerated(UUserWidget& /*Entry*/)
{
	// 어느 리스트가 방송했든 둘 다 다시 건다 — 탭 전환 직후 방송이 오면 현재 주기와 리스트가 어긋난다
	auto Rebind = [this](UQuestRowWidget& Row)
	{
		Row.OnClaimRequested.RemoveAll(this);
		Row.OnClaimRequested.AddUObject(this, &UQuestPanelWidget::HandleRowClaim);
	};
	CGListEntry::RebindGeneratedEntries<UQuestRowWidget>(QuestList, Rebind);
	CGListEntry::RebindGeneratedEntries<UQuestRowWidget>(LifetimeList, Rebind);
}

void UQuestPanelWidget::HandleRowClaim(UQuestRowItem* RowItem)
{
	if (!RowItem)
	{
		return;
	}

	UGameInstance* GameInst = GetGameInstance();
	if (UQuestSubsystem* Quest = GameInst ? GameInst->GetSubsystem<UQuestSubsystem>() : nullptr)
	{
		// 거부 토스트는 매니저가 낸다
		Quest->ClaimQuest(RowItem->Entry.QuestID);
	}
}

void UQuestPanelWidget::HandleClaimAll()
{
	UGameInstance* GameInst = GetGameInstance();
	UQuestSubsystem* Quest = GameInst ? GameInst->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quest)
	{
		return;
	}

	// 수령마다 원장이 방송 → RefreshAll 이 아이템을 갈아엎는다. 먼저 ID 를 떠 놓고 돈다
	TArray<FQuestEntry> ClaimableEntries;
	TArray<FQuestEntry> Entries;
	Quest->GetEntries(CurrentPeriod, Entries);
	for (const FQuestEntry& Entry : Entries)
	{
		if (Entry.State == EQuestState::Claimable)
		{
			ClaimableEntries.Add(Entry);
		}
	}

	TArray<FMissionReward> ClaimedRewards;
	for (const FQuestEntry& Entry : ClaimableEntries)
	{
		if (Quest->ClaimQuestWithPresentation(Entry.QuestID, false))
		{
			ClaimedRewards.Append(Entry.Row.Rewards);
		}
	}
	if (!ClaimedRewards.IsEmpty())
	{
		if (UUIManagerSubsystem* UIMgr = GameInst->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowRewardToast(ClaimedRewards,
				CurrentPeriod == EQuestPeriod::Lifetime
					? NSLOCTEXT("Quest", "LifetimeClaimAllReward", "업적 보상")
					: NSLOCTEXT("Quest", "PeriodClaimAllReward", "미션 보상"), true);
		}
	}
}
