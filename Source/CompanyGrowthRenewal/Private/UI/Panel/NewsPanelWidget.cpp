// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/NewsPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ListView.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Data/GameClockRules.h"
#include "Data/LiveOpsRules.h"
#include "Global/GlobalUtilFunctions.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/LiveOpsSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "UI/Element/Common/ListEntryUtils.h"
#include "UI/Element/LiveOps/EventBannerWidget.h"
#include "UI/Element/LiveOps/NoticeRowWidget.h"
#include "UI/Element/LiveOps/QuestRowWidget.h"

namespace
{
	// 레일 탭 순서 = 스위처 인덱스 계약 (저작 스크립트의 rail_tabs 와 같은 순서)
	constexpr int32 NoticeTabIndex = 1;

	FString NewsKstDate(int64 Unix)
	{
		return FGameClockRules::KstFromUnix(Unix).ToString(TEXT("%Y.%m.%d"));
	}

	// 목록 구성(아이디 순서)이 그대로인가 ― 그대로면 줄 위젯을 다시 만들지 않는다.
	// 벽돌 모으기처럼 이벤트 점수가 수시로 오를 때마다 줄을 다시 만들면 누르던 줄이 사라져 탭이 없어진다
	template <typename TItem, typename TDef>
	bool SameListIds(const TArray<TObjectPtr<TItem>>& Items, const TArray<TDef>& Defs, const UListView* List)
	{
		if (!List || Items.Num() != Defs.Num() || List->GetNumItems() != Defs.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Defs.Num(); ++Index)
		{
			if (!Items[Index] || Items[Index]->Def.Id != Defs[Index].Id)
			{
				return false;
			}
		}
		return true;
	}

	bool HasClaimableMilestone(const ULiveOpsSubsystem& LiveOps, const FLiveOpsEventDef& Def)
	{
		const FLiveOpsEventProgress Progress = LiveOps.GetEventProgress(Def.Id);
		for (int32 Index = 0; Index < Def.Milestones.Num(); ++Index)
		{
			if (FLiveOpsRules::IsMilestoneClaimable(Progress, Def, Index))
			{
				return true;
			}
		}
		return false;
	}

	// 다음으로 넘어야 할 임계 — 전부 넘겼으면 마지막 임계가 분모다
	int64 NextThreshold(const FLiveOpsEventDef& Def, int64 Points)
	{
		for (const FLiveOpsMilestone& Milestone : Def.Milestones)
		{
			if (Points < Milestone.Threshold)
			{
				return Milestone.Threshold;
			}
		}
		return Def.Milestones.Num() > 0 ? Def.Milestones.Last().Threshold : 0;
	}
}

void UNewsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 탭 그룹 생성·초기 선택은 NativeConstruct 에서만 (CLAUDE.md 탭 패턴)
	TGuardValue<bool> RefreshGuard(bSuppressRefresh, true);
	TabGroup = NewObject<UCommonButtonGroupBase>(this);
	TabGroup->SetSelectionRequired(true);
	for (UIconWithButtonWidget* Tab : { EventTab.Get(), NoticeTab.Get() })
	{
		if (!Tab)
		{
			continue;
		}
		TabGroup->AddWidget(Tab);
		Tab->SetIsSelectable(true);
	}
	TabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UNewsPanelWidget::HandleTabChanged);

	if (ContentSwitcher)
	{
		// 페이지 내용이 레일 탭의 클릭을 가로채지 않게 (CLAUDE.md 탭 패턴)
		ContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);
		ContentSwitcher->SetActiveWidgetIndex(0);
	}
	TabGroup->SelectButtonAtIndex(0);

	// 목록 두 개는 선택을 가진다 — 상세 패널이 그 선택을 따라간다
	if (BannerList)
	{
		BannerList->SetSelectionMode(ESelectionMode::Single);
		BannerList->OnItemSelectionChanged().AddUObject(this, &UNewsPanelWidget::HandleBannerSelectionChanged);
		BannerList->OnEntryWidgetGenerated().AddUObject(this, &UNewsPanelWidget::HandleEntryGenerated);
	}
	if (NoticeList)
	{
		NoticeList->SetSelectionMode(ESelectionMode::Single);
		NoticeList->OnItemSelectionChanged().AddUObject(this, &UNewsPanelWidget::HandleNoticeSelectionChanged);
		NoticeList->OnEntryWidgetGenerated().AddUObject(this, &UNewsPanelWidget::HandleEntryGenerated);
	}
	if (MilestoneList)
	{
		MilestoneList->OnEntryWidgetGenerated().AddUObject(this, &UNewsPanelWidget::HandleEntryGenerated);
	}

	// 디자인타임 샘플(HotTimeWindowsBox_Sample)의 폰트·잉크 — 첫 채움이 샘플을 지우므로 미리 떠 둔다 (보상 칩 선례)
	if (HotTimeWindowsBox)
	{
		for (int32 Index = 0; Index < HotTimeWindowsBox->GetChildrenCount(); ++Index)
		{
			if (const UTextBlock* Sample = Cast<UTextBlock>(HotTimeWindowsBox->GetChildAt(Index)))
			{
				HotTimeChipFont = Sample->GetFont();
				HotTimeChipInk = Sample->GetColorAndOpacity();
				bHotTimeChipStyleCached = true;
				break;
			}
		}
	}
}

void UNewsPanelWidget::NativeDestruct()
{
	if (TabGroup)
	{
		TabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		// 풀에서 재사용되는 창이라 등록을 안 풀면 같은 버튼이 다음 오픈의 새 그룹과 옛 그룹 양쪽에 남는다
		TabGroup->RemoveAll();
	}
	if (BannerList)
	{
		BannerList->OnItemSelectionChanged().RemoveAll(this);
		BannerList->OnEntryWidgetGenerated().RemoveAll(this);
	}
	if (NoticeList)
	{
		NoticeList->OnItemSelectionChanged().RemoveAll(this);
		NoticeList->OnEntryWidgetGenerated().RemoveAll(this);
	}
	if (MilestoneList)
	{
		MilestoneList->OnEntryWidgetGenerated().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UNewsPanelWidget::NativeOnActivated()
{
	if (ULiveOpsSubsystem* LiveOps = GetLiveOps())
	{
		LiveOps->OnLiveOpsContentChanged.AddUObject(this, &UNewsPanelWidget::HandleContentChanged);
		LiveOps->OnHotTimeChanged.AddUObject(this, &UNewsPanelWidget::HandleHotTimeChanged);
	}

	// 메뉴 타일이 남긴 탭 힌트 — 공지 타일로 들어오면 공지 탭이 열린 채 시작한다
	if (UGameInstance* GameInst = GetGameInstance())
	{
		if (UUIManagerSubsystem* UIMgr = GameInst->GetSubsystem<UUIManagerSubsystem>())
		{
			TGuardValue<bool> RefreshGuard(bSuppressRefresh, true);
			if (UIMgr->ConsumePendingPanelTab() == TEXT("Notice") && TabGroup)
			{
				TabGroup->SelectButtonAtIndex(NoticeTabIndex);
			}
		}
	}

	Super::NativeOnActivated();   // → RefreshAll
}

void UNewsPanelWidget::NativeOnDeactivated()
{
	if (ULiveOpsSubsystem* LiveOps = GetLiveOps())
	{
		LiveOps->OnLiveOpsContentChanged.RemoveAll(this);
		LiveOps->OnHotTimeChanged.RemoveAll(this);
	}

	// 입력 모드 복원은 베이스(ULiveOpsModalWidget)가 한다
	Super::NativeOnDeactivated();
}

void UNewsPanelWidget::HandleTabChanged(UCommonButtonBase* /*Button*/, int32 ButtonIndex)
{
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}

	RefreshAll();
}

void UNewsPanelWidget::HandleContentChanged()
{
	RefreshAll();
}

void UNewsPanelWidget::HandleHotTimeChanged(float /*Multiplier*/)
{
	ApplyHotTimeBadge();
}

void UNewsPanelWidget::RefreshAll()
{
	if (bSuppressRefresh || bRefreshing)
	{
		return;
	}

	{
		// 갱신 중 선택이 바뀌면 읽음 처리 방송이 목록을 갈아엎는 중에 들어온다 — 재진입을 막고 뒤로 미룬다
		TGuardValue<bool> Guard(bRefreshing, true);
		RefreshEventPage();
		RefreshNoticePage();
		RefreshTabDots();
	}

	FlushPendingNoticeRead();
}

void UNewsPanelWidget::RefreshEventPage()
{
	ULiveOpsSubsystem* LiveOps = GetLiveOps();
	if (!LiveOps)
	{
		return;
	}

	TArray<FLiveOpsEventDef> Defs;
	LiveOps->GetActiveEvents(Defs);

	UGameInstance* GameInst = GetGameInstance();
	const UGameClockSubsystem* Clock = GameInst ? GameInst->GetSubsystem<UGameClockSubsystem>() : nullptr;
	const int64 NowUnix = Clock ? Clock->GetServerNowUnix() : 0;
	const bool bServerTimeValid = IsServerTimeValidNow();

	const bool bSameList = SameListIds(EventItems, Defs, BannerList);
	if (!bSameList)
	{
		CGListEntry::SyncItems<UEventBannerItem>(EventItems, Defs.Num(), this, BannerList);
	}
	for (int32 Index = 0; Index < EventItems.Num(); ++Index)
	{
		EventItems[Index]->Def = Defs[Index];
		EventItems[Index]->NowUnix = NowUnix;
		EventItems[Index]->bServerTimeValid = bServerTimeValid;
		EventItems[Index]->bClaimable = HasClaimableMilestone(*LiveOps, Defs[Index]);
	}

	if (BannerList && bSameList)
	{
		for (UUserWidget* Entry : BannerList->GetDisplayedEntryWidgets())
		{
			UEventBannerWidget* Banner = Cast<UEventBannerWidget>(Entry);
			const UEventBannerItem* BannerItem = Banner ? Cast<UEventBannerItem>(UUserObjectListEntryLibrary::GetListItemObject(Banner)) : nullptr;
			if (BannerItem)
			{
				Banner->SetEvent(BannerItem->Def, BannerItem->bClaimable, BannerItem->NowUnix, BannerItem->bServerTimeValid);
			}
		}
	}
	else if (BannerList)
	{
		// 아이템 객체가 재사용돼 ListView 가 "같은 아이템" 으로 보고 넘어가는 경우가 있어 표시를 강제한다
		BannerList->RegenerateAllEntries();
	}
	if (EventEmptyState)
	{
		EventEmptyState->SetVisibility(Defs.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	UEventBannerItem* Target = nullptr;
	for (const TObjectPtr<UEventBannerItem>& BannerItem : EventItems)
	{
		if (BannerItem && BannerItem->Def.Id == SelectedEventId)
		{
			Target = BannerItem;
			break;
		}
	}
	if (!Target && EventItems.Num() > 0)
	{
		Target = EventItems[0];
	}

	if (BannerList && Target)
	{
		BannerList->SetSelectedItem(Target);   // → HandleBannerSelectionChanged
	}
	else
	{
		if (BannerList)
		{
			BannerList->ClearSelection();
		}
		HandleBannerSelectionChanged(Target);
	}
}

void UNewsPanelWidget::RefreshNoticePage()
{
	ULiveOpsSubsystem* LiveOps = GetLiveOps();
	if (!LiveOps)
	{
		return;
	}

	TArray<FLiveOpsNoticeDef> Defs;
	LiveOps->GetNotices(Defs);   // 고정 먼저, 그 다음 최신순 — 정렬은 매니저가 끝내 준다

	const bool bSameList = SameListIds(NoticeItems, Defs, NoticeList);
	if (!bSameList)
	{
		CGListEntry::SyncItems<UNoticeItem>(NoticeItems, Defs.Num(), this, NoticeList);
	}
	for (int32 Index = 0; Index < NoticeItems.Num(); ++Index)
	{
		NoticeItems[Index]->Def = Defs[Index];
		NoticeItems[Index]->bRead = LiveOps->IsNoticeRead(Defs[Index].Id);
	}

	if (NoticeList && bSameList)
	{
		for (UUserWidget* Entry : NoticeList->GetDisplayedEntryWidgets())
		{
			UNoticeRowWidget* Row = Cast<UNoticeRowWidget>(Entry);
			const UNoticeItem* RowItem = Row ? Cast<UNoticeItem>(UUserObjectListEntryLibrary::GetListItemObject(Row)) : nullptr;
			if (RowItem)
			{
				Row->SetNotice(RowItem->Def, RowItem->bRead);
			}
		}
	}
	else if (NoticeList)
	{
		NoticeList->RegenerateAllEntries();
	}
	if (NoticeEmptyState)
	{
		NoticeEmptyState->SetVisibility(Defs.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	UNoticeItem* Target = nullptr;
	for (const TObjectPtr<UNoticeItem>& NoticeItem : NoticeItems)
	{
		if (NoticeItem && NoticeItem->Def.Id == SelectedNoticeId)
		{
			Target = NoticeItem;
			break;
		}
	}
	if (!Target && NoticeItems.Num() > 0)
	{
		Target = NoticeItems[0];
	}

	if (NoticeList && Target)
	{
		NoticeList->SetSelectedItem(Target);   // → HandleNoticeSelectionChanged
	}
	else
	{
		if (NoticeList)
		{
			NoticeList->ClearSelection();
		}
		HandleNoticeSelectionChanged(Target);
	}
}

void UNewsPanelWidget::RefreshTabDots()
{
	const ULiveOpsSubsystem* LiveOps = GetLiveOps();

	auto ApplyDot = [](UAlertMarkWidget* Dot, bool bShow)
	{
		if (!Dot)
		{
			return;
		}
		if (bShow)
		{
			Dot->Show();
		}
		else
		{
			Dot->Hide();
		}
	};

	ApplyDot(EventTabDot, LiveOps && LiveOps->HasClaimableEvent());
	ApplyDot(NoticeTabDot, LiveOps && LiveOps->HasUnreadNotice());
}

void UNewsPanelWidget::HandleBannerSelectionChanged(UObject* SelectedItem)
{
	UEventBannerItem* BannerItem = Cast<UEventBannerItem>(SelectedItem);
	SelectedEventId = BannerItem ? BannerItem->Def.Id : FString();
	FillEventDetail(BannerItem);
}

void UNewsPanelWidget::HandleNoticeSelectionChanged(UObject* SelectedItem)
{
	UNoticeItem* NoticeItem = Cast<UNoticeItem>(SelectedItem);
	SelectedNoticeId = NoticeItem ? NoticeItem->Def.Id : FString();
	FillNoticeDetail(NoticeItem);

	PendingNoticeReadId = NoticeItem ? NoticeItem->Def.Id : FString();
	if (!bRefreshing)
	{
		// 공지 페이지가 아직 안 보이면 미뤄 둔다 — 탭을 옮겨 실제로 읽을 때 처리된다
		FlushPendingNoticeRead();
	}
}

void UNewsPanelWidget::HandleBannerClicked(UEventBannerItem* BannerItem)
{
	// 행이 CommonButton 이라 리스트 행이 클릭을 못 받는다 — 선택 상태의 주인은 여전히 리스트다
	if (BannerList && BannerItem)
	{
		BannerList->SetSelectedItem(BannerItem);
	}
}

void UNewsPanelWidget::HandleNoticeClicked(UNoticeItem* NoticeItem)
{
	if (NoticeList && NoticeItem)
	{
		NoticeList->SetSelectedItem(NoticeItem);
	}
}

void UNewsPanelWidget::HandleEntryGenerated(UUserWidget& Entry)
{
	// 엔트리는 풀에서 재사용된다 — 방송한 리스트만 통째로 다시 건다(엔트리 타입이 곧 출처다)
	if (Entry.IsA<UEventBannerWidget>())
	{
		CGListEntry::RebindGeneratedEntries<UEventBannerWidget>(BannerList, [this](UEventBannerWidget& Banner)
		{
			Banner.OnBannerClicked.RemoveAll(this);
			Banner.OnBannerClicked.AddUObject(this, &UNewsPanelWidget::HandleBannerClicked);
		});
	}
	else if (Entry.IsA<UNoticeRowWidget>())
	{
		CGListEntry::RebindGeneratedEntries<UNoticeRowWidget>(NoticeList, [this](UNoticeRowWidget& Row)
		{
			Row.OnRowClicked.RemoveAll(this);
			Row.OnRowClicked.AddUObject(this, &UNewsPanelWidget::HandleNoticeClicked);
		});
	}
	else if (Entry.IsA<UQuestRowWidget>())
	{
		CGListEntry::RebindGeneratedEntries<UQuestRowWidget>(MilestoneList, [this](UQuestRowWidget& Row)
		{
			Row.OnClaimRequested.RemoveAll(this);
			Row.OnClaimRequested.AddUObject(this, &UNewsPanelWidget::HandleRowClaim);
		});
	}
}

void UNewsPanelWidget::HandleRowClaim(UQuestRowItem* RowItem)
{
	if (!RowItem || !RowItem->bMilestone)
	{
		return;
	}

	if (ULiveOpsSubsystem* LiveOps = GetLiveOps())
	{
		// 거절 토스트·보상 토스트는 매니저가 낸다. 수령 방송이 RefreshAll 을 부른다
		LiveOps->ClaimEventMilestone(RowItem->EventId, RowItem->MilestoneIndex);
	}
}

void UNewsPanelWidget::FillEventDetail(UEventBannerItem* BannerItem)
{
	bSelectedHasHotTime = false;

	if (!BannerItem)
	{
		if (DetailTitleText)
		{
			DetailTitleText->SetText(FText::GetEmpty());
		}
		if (DetailPeriodText)
		{
			DetailPeriodText->SetText(FText::GetEmpty());
		}
		if (DetailDescText)
		{
			DetailDescText->SetText(FText::GetEmpty());
		}
		if (ProgressLabelText)
		{
			ProgressLabelText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ProgressValueText)
		{
			ProgressValueText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ProgressBarBox)
		{
			ProgressBarBox->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (HotTimeWindowsBox)
		{
			HotTimeWindowsBox->SetVisibility(ESlateVisibility::Collapsed);
		}
		ApplyHotTimeBadge();
		CGListEntry::SyncItems<UQuestRowItem>(MilestoneItems, 0, this, MilestoneList);
		return;
	}

	ULiveOpsSubsystem* LiveOps = GetLiveOps();
	if (!LiveOps)
	{
		return;
	}

	const FLiveOpsEventDef& Def = BannerItem->Def;
	if (DetailTitleText)
	{
		DetailTitleText->SetText(Def.Title);
	}
	if (DetailPeriodText)
	{
		// 한글 조판 줄표는 U+2015 — NEXON 에 U+2014 글리프가 없다 (타이포 SOT §6)
		DetailPeriodText->SetText(FText::FromString(FString::Printf(
			TEXT("%s ― %s"), *NewsKstDate(Def.StartUnix), *NewsKstDate(Def.EndUnix))));
	}
	if (DetailDescText)
	{
		DetailDescText->SetText(Def.Desc);
	}

	const FLiveOpsEventProgress Progress = LiveOps->GetEventProgress(Def.Id);
	const bool bHasMilestones = Def.Milestones.Num() > 0;
	const int64 Target = NextThreshold(Def, Progress.Points);

	if (ProgressLabelText)
	{
		ProgressLabelText->SetVisibility(bHasMilestones ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ProgressLabelText->SetText(Def.Type == ELiveOpsEventType::AttendanceEvent
			? ProgressLabelAttendance : ProgressLabelPoints);
	}
	if (ProgressValueText)
	{
		ProgressValueText->SetVisibility(bHasMilestones ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ProgressValueText->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")),
			UGlobalUtilFunctions::AbbreviateNumber(Progress.Points), UGlobalUtilFunctions::AbbreviateNumber(Target)));
	}
	if (ProgressBarBox)
	{
		ProgressBarBox->SetVisibility(bHasMilestones ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Target > 0
			? FMath::Clamp(static_cast<float>(Progress.Points) / static_cast<float>(Target), 0.f, 1.f)
			: 0.f);
	}

	bSelectedHasHotTime = Def.HotTime.Num() > 0;
	if (HotTimeWindowsBox)
	{
		HotTimeWindowsBox->SetVisibility(bSelectedHasHotTime ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		HotTimeWindowsBox->ClearChildren();
		for (int32 Index = 0; Index < Def.HotTime.Num(); ++Index)
		{
			const FLiveOpsHotTimeWindow& Window = Def.HotTime[Index];
			UCommonTextBlock* ChipText = WidgetTree ? WidgetTree->ConstructWidget<UCommonTextBlock>() : nullptr;
			if (!ChipText)
			{
				break;
			}
			if (bHotTimeChipStyleCached)
			{
				ChipText->SetFont(HotTimeChipFont);
				ChipText->SetColorAndOpacity(HotTimeChipInk);
			}
			ChipText->SetText(FText::FromString(FString::Printf(TEXT("%02d:00~%02d:00 ×%.1f"),
				Window.StartHourKst, Window.EndHourKst, Window.Multiplier)));
			if (UHorizontalBoxSlot* ChipSlot = HotTimeWindowsBox->AddChildToHorizontalBox(ChipText))
			{
				ChipSlot->SetVerticalAlignment(VAlign_Center);
				ChipSlot->SetPadding(FMargin(Index > 0 ? 20.f : 0.f, 0.f, 0.f, 0.f));
			}
		}
	}
	ApplyHotTimeBadge();

	CGListEntry::SyncItems<UQuestRowItem>(MilestoneItems, Def.Milestones.Num(), this, MilestoneList);
	const bool bServerTimeValid = IsServerTimeValidNow();
	for (int32 Index = 0; Index < MilestoneItems.Num(); ++Index)
	{
		UQuestRowItem& RowItem = *MilestoneItems[Index];
		RowItem.bMilestone = true;
		RowItem.EventId = Def.Id;
		RowItem.MilestoneIndex = Index;
		RowItem.bServerTimeValid = bServerTimeValid;

		const FLiveOpsMilestone& Milestone = Def.Milestones[Index];
		RowItem.Entry = FQuestEntry();
		RowItem.Entry.Row.Title = FText::Format(FText::FromString(TEXT("{0} 달성")),
			UGlobalUtilFunctions::AbbreviateNumber(Milestone.Threshold));
		RowItem.Entry.Row.Rewards = Milestone.Rewards;
		// 누적 출석 이벤트는 Counter 가 비어 있다 — 행의 진행 표기를 증분형("3 / 7")으로 쓰려고 출석 카운터를 넣는다
		RowItem.Entry.Row.Counter = Def.Counter != EQuestCounter::None ? Def.Counter : EQuestCounter::LoginDay;
		RowItem.Entry.Current = Progress.Points;
		RowItem.Entry.Target = FMath::Max<int64>(1, Milestone.Threshold);
		RowItem.Entry.State = Progress.ClaimedMilestones.Contains(Index)
			? EQuestState::Claimed
			: (FLiveOpsRules::IsMilestoneClaimable(Progress, Def, Index) ? EQuestState::Claimable : EQuestState::InProgress);
	}

	if (MilestoneList)
	{
		MilestoneList->RegenerateAllEntries();
	}
}

void UNewsPanelWidget::FillNoticeDetail(UNoticeItem* NoticeItem)
{
	if (NoticeTitleText)
	{
		NoticeTitleText->SetText(NoticeItem ? NoticeItem->Def.Title : FText::GetEmpty());
	}
	if (NoticeMetaText)
	{
		NoticeMetaText->SetText(NoticeItem
			? FText::FromString(FString::Printf(TEXT("%s · 운영팀"), *NewsKstDate(NoticeItem->Def.PostedUnix)))
			: FText::GetEmpty());
	}
	if (NoticeBodyText)
	{
		NoticeBodyText->SetText(NoticeItem ? NoticeItem->Def.Body : FText::GetEmpty());
	}
	if (NoticeBodyScroll)
	{
		// 다른 공지를 고르면 이전 공지의 스크롤 위치가 남는다
		NoticeBodyScroll->ScrollToStart();
	}
}

void UNewsPanelWidget::ApplyHotTimeBadge()
{
	if (!HotTimeNowBadge)
	{
		return;
	}

	const ULiveOpsSubsystem* LiveOps = GetLiveOps();
	const bool bNow = bSelectedHasHotTime && LiveOps && LiveOps->GetRevenueMultiplierNow() > 1.0f;
	HotTimeNowBadge->SetVisibility(bNow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool UNewsPanelWidget::IsNoticePageVisible() const
{
	// 애니메이션 스위처는 전이가 끝나야 Slate 인덱스를 옮긴다 — 어느 탭인지의 출처는 스위처가 아니라 탭 그룹이다.
	return TabGroup && TabGroup->GetSelectedButtonIndex() == NoticeTabIndex;
}

void UNewsPanelWidget::FlushPendingNoticeRead()
{
	// 안 보이는 페이지의 자동 선택은 "읽었다"가 아니다 — 대기 Id 를 그대로 들고 있다가 탭을 옮길 때 처리한다
	if (PendingNoticeReadId.IsEmpty() || !IsNoticePageVisible())
	{
		return;
	}

	const FString ReadId = PendingNoticeReadId;
	PendingNoticeReadId.Reset();
	if (ULiveOpsSubsystem* LiveOps = GetLiveOps())
	{
		// 이미 읽은 공지면 매니저가 조용히 돌아간다(방송 없음) — 읽음→갱신→읽음 고리가 여기서 끝난다
		LiveOps->MarkNoticeRead(ReadId);
	}
}

ULiveOpsSubsystem* UNewsPanelWidget::GetLiveOps() const
{
	UGameInstance* GameInst = GetGameInstance();
	return GameInst ? GameInst->GetSubsystem<ULiveOpsSubsystem>() : nullptr;
}
