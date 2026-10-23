// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/MailboxPanelWidget.h"

#include "CommonTextBlock.h"
#include "Components/ListView.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/MailboxSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/ListEntryUtils.h"
#include "UI/Element/LiveOps/MailRowWidget.h"

void UMailboxPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ClaimAllButton)
	{
		ClaimAllButton->OnClicked().AddUObject(this, &UMailboxPanelWidget::HandleClaimAll);
	}
	// 엔트리는 풀에서 재사용된다 — 생성 방송마다 전체를 다시 건다 (공용 헬퍼)
	if (MailList)
	{
		MailList->OnEntryWidgetGenerated().AddUObject(this, &UMailboxPanelWidget::HandleEntryGenerated);
	}
}

void UMailboxPanelWidget::NativeDestruct()
{
	if (ClaimAllButton)
	{
		ClaimAllButton->OnClicked().RemoveAll(this);
	}
	if (MailList)
	{
		MailList->OnEntryWidgetGenerated().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMailboxPanelWidget::NativeOnActivated()
{
	if (UMailboxSubsystem* Mailbox = GetMailbox())
	{
		// 만료 정리는 우편함을 열 때 한다 — 구독보다 먼저라 정리 방송이 헛돌지 않는다
		Mailbox->PurgeExpiredNow();
		Mailbox->OnMailboxChanged.AddUObject(this, &UMailboxPanelWidget::HandleMailboxChanged);
	}

	Super::NativeOnActivated();   // → RefreshAll
}

void UMailboxPanelWidget::NativeOnDeactivated()
{
	if (UMailboxSubsystem* Mailbox = GetMailbox())
	{
		Mailbox->OnMailboxChanged.RemoveAll(this);
	}

	// 패널은 풀에서 재사용된다 — 펼침을 남기면 다시 연 창이 지난 세션의 아코디언을 펼친 채 뜬다 (재오픈 = 접힘)
	ExpandedIds.Reset();
	// 아이템 풀도 재사용되고 RefreshAll 이 그 플래그로 ExpandedIds 를 되살리므로 여기서 같이 끈다
	for (const TObjectPtr<UMailRowItem>& Item : Items)
	{
		if (Item)
		{
			Item->bExpanded = false;
		}
	}

	// 입력 모드 복원은 베이스(ULiveOpsModalWidget)가 한다
	Super::NativeOnDeactivated();
}

void UMailboxPanelWidget::RefreshAll()
{
	UMailboxSubsystem* Mailbox = GetMailbox();
	if (!Mailbox)
	{
		return;
	}

	TArray<FMailEntry> Entries = Mailbox->GetMails();
	// 최근 받은 우편이 위 (원장은 도착 순으로 쌓는다)
	Entries.Sort([](const FMailEntry& A, const FMailEntry& B) { return A.ReceivedUnix > B.ReceivedUnix; });

	UGameInstance* GameInst = GetGameInstance();
	const UGameClockSubsystem* Clock = GameInst ? GameInst->GetSubsystem<UGameClockSubsystem>() : nullptr;
	const int64 NowUnix = Clock ? Clock->GetServerNowUnix() : 0;
	const bool bServerTimeValid = IsServerTimeValidNow();

	// 행이 스스로 토글한 아코디언 상태를 먼저 Id 로 걷어 둔다 — 수령으로 목록이 밀려도 남은 우편은 펼침을 유지한다
	for (const TObjectPtr<UMailRowItem>& Previous : Items)
	{
		if (!Previous)
		{
			continue;
		}
		if (Previous->bExpanded)
		{
			ExpandedIds.Add(Previous->Entry.Id);
		}
		else
		{
			ExpandedIds.Remove(Previous->Entry.Id);
		}
	}

	TSet<FGuid> LiveIds;
	LiveIds.Reserve(Entries.Num());
	CGListEntry::SyncItems<UMailRowItem>(Items, Entries.Num(), this, MailList);
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		Items[Index]->Entry = Entries[Index];
		Items[Index]->NowUnix = NowUnix;
		Items[Index]->bServerTimeValid = bServerTimeValid;
		Items[Index]->bExpanded = ExpandedIds.Contains(Entries[Index].Id);
		LiveIds.Add(Entries[Index].Id);
	}
	// 수령·만료로 사라진 우편의 상태는 들고 있지 않는다
	ExpandedIds = ExpandedIds.Intersect(LiveIds);

	// 아이템 객체가 재사용돼 ListView 가 "같은 아이템" 으로 보고 넘어가는 경우가 있어 표시를 강제한다
	if (MailList)
	{
		MailList->RegenerateAllEntries();
	}

	const int32 Unclaimed = Mailbox->GetUnclaimedCount();
	if (MailCountNumText)
	{
		MailCountNumText->SetText(FText::AsNumber(Unclaimed));
	}
	if (EmptyState)
	{
		EmptyState->SetVisibility(Entries.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ClaimAllButton)
	{
		ClaimAllButton->SetIsEnabled(Unclaimed > 0 && bServerTimeValid);
	}
}

void UMailboxPanelWidget::HandleMailboxChanged()
{
	RefreshAll();
}

void UMailboxPanelWidget::HandleEntryGenerated(UUserWidget& /*Entry*/)
{
	CGListEntry::RebindGeneratedEntries<UMailRowWidget>(MailList, [this](UMailRowWidget& Row)
	{
		Row.OnClaimRequested.RemoveAll(this);
		Row.OnClaimRequested.AddUObject(this, &UMailboxPanelWidget::HandleRowClaim);
	});
}

void UMailboxPanelWidget::HandleRowClaim(UMailRowItem* RowItem)
{
	if (!RowItem)
	{
		return;
	}

	if (UMailboxSubsystem* Mailbox = GetMailbox())
	{
		// 거절 토스트·보상 토스트는 매니저가 낸다. 수령 방송이 RefreshAll 을 부른다
		Mailbox->ClaimMail(RowItem->Entry.Id);
	}
}

void UMailboxPanelWidget::HandleClaimAll()
{
	if (UMailboxSubsystem* Mailbox = GetMailbox())
	{
		Mailbox->ClaimAll();
	}
}

UMailboxSubsystem* UMailboxPanelWidget::GetMailbox() const
{
	UGameInstance* GameInst = GetGameInstance();
	return GameInst ? GameInst->GetSubsystem<UMailboxSubsystem>() : nullptr;
}
