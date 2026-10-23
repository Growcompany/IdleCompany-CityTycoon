// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/LiveOps/QuestRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Engine/Texture2D.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Common/AlertMarkWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"

namespace
{
	// 칸 나눔은 셀 수 있는 작은 목표에서만 읽힌다 — 그 이상은 눈금이 막대를 덮는다
	constexpr int64 QuestRowMinSegmentTarget = 2;
	constexpr int64 QuestRowMaxSegmentTarget = 10;
}

void UQuestRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ClaimButton)
	{
		ClaimButton->OnClicked().AddUObject(this, &UQuestRowWidget::HandleClaimClicked);
	}
}

void UQuestRowWidget::NativeDestruct()
{
	if (ClaimButton)
	{
		ClaimButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UQuestRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Item = Cast<UQuestRowItem>(ListItemObject);
	if (!Item)
	{
		if (ClaimButton)
		{
			ClaimButton->SetIsEnabled(false);
			ClaimButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ClaimDot) ClaimDot->Hide();
		if (DoneStamp) DoneStamp->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FQuestEntry& Entry = Item->Entry;

	if (TitleText)
	{
		TitleText->SetText(Entry.Row.Title);
	}
	if (TierText)
	{
		TierText->SetText(FText::FromString(FString::Printf(TEXT("%d단계"), Entry.Row.Tier)));
		TierText->SetVisibility(Entry.Row.Tier > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyIcon(Entry.Row);
	ApplyProgress(Entry);
	ApplyRewards(Entry.Row.Rewards);
	ApplyClaim(Entry.State, Item->bServerTimeValid);

	SetRenderOpacity(Entry.State == EQuestState::Claimed ? ClaimedOpacity : 1.f);
}

void UQuestRowWidget::ApplyIcon(const FQuestTable& Row)
{
	if (IconWell)
	{
		IconWell->SetVisibility(Row.Icon.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Row.Icon.IsNull())
	{
		return;
	}
	if (RowIcon)
	{
		RowIcon->SetBrushResourceObject(Row.Icon.LoadSynchronous());
	}
}

void UQuestRowWidget::ApplyProgress(const FQuestEntry& Entry)
{
	// Target 은 UQuestSubsystem::GetEntries 가 1 이상으로 보정한다
	const int64 Shown = FMath::Clamp<int64>(Entry.Current, 0, Entry.Target);
	const bool bFull = Entry.Current >= Entry.Target;
	const bool bIncrement = FQuestRules::IsIncrementCounter(Entry.Row.Counter);

	// 절대값형(업적 누적 매출 등)은 자릿수가 커서 축약 표기한다
	auto FormatValue = [bIncrement](int64 Value)
	{
		return bIncrement ? FText::FromString(FString::Printf(TEXT("%lld"), Value)) : UGlobalUtilFunctions::AbbreviateNumber(Value);
	};

	if (ProgressCurrentText)
	{
		ProgressCurrentText->SetText(FormatValue(Shown));
		ProgressCurrentText->SetColorAndOpacity(bFull ? ProgressInkFull : ProgressInkProgress);
	}
	if (ProgressTargetText)
	{
		ProgressTargetText->SetText(FText::Format(FText::FromString(TEXT(" / {0}")), FormatValue(Entry.Target)));
	}
	if (ProgressBar)
	{
		ProgressBar->SetPercent(static_cast<float>(static_cast<double>(Shown) / static_cast<double>(Entry.Target)));
		ProgressBar->SetFillColorAndOpacity(bFull ? BarFillFull : BarFillProgress);
	}

	const bool bSegmented = bIncrement && Entry.Target >= QuestRowMinSegmentTarget && Entry.Target <= QuestRowMaxSegmentTarget;
	ApplySegments(bSegmented ? static_cast<int32>(Entry.Target) : 0);
}

void UQuestRowWidget::ApplySegments(int32 Count)
{
	if (!SegmentBox)
	{
		return;
	}
	if (Count <= 0)
	{
		SegmentBox->ClearChildren();
		SegmentBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 풀링된 줄은 같은 목표로 자주 다시 채워진다 — 칸 수가 같으면 그대로 둔다
	const int32 WantedChildren = Count * 2 - 1;
	if (SegmentBox->GetChildrenCount() != WantedChildren && WidgetTree)
	{
		SegmentBox->ClearChildren();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (UHorizontalBoxSlot* CellSlot = SegmentBox->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>()))
			{
				CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			if (Index == Count - 1)
			{
				break;
			}
			UImage* TickImage = WidgetTree->ConstructWidget<UImage>();
			TickImage->SetBrush(SegmentTickBrush);
			if (UHorizontalBoxSlot* TickSlot = SegmentBox->AddChildToHorizontalBox(TickImage))
			{
				TickSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				TickSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}
	SegmentBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UQuestRowWidget::ApplyRewards(const TArray<FMissionReward>& Rewards)
{
	const TArray<CGRewardChip::FDisplayEntry> Entries = CGRewardChip::BuildDisplayEntries(Rewards);
	UItemCardSlotWidget* const Cards[] = { RewardCard0, RewardCard1 };
	int32 Index = 0;
	for (UItemCardSlotWidget* Card : Cards)
	{
		const int32 EntryIndex = Index++;
		if (!Card)
		{
			continue;
		}
		if (!Entries.IsValidIndex(EntryIndex))
		{
			Card->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Card->SetIconResource(CGRewardChip::ResolveIcon(GetGameInstance(), Entries[EntryIndex]));
		Card->SetLabel(CGRewardChip::FormatQuantity(Entries[EntryIndex]));
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UQuestRowWidget::ApplyClaim(EQuestState State, bool bServerTimeValid)
{
	const bool bClaimable = State == EQuestState::Claimable;
	const bool bClaimed = State == EQuestState::Claimed;

	if (ClaimButton)
	{
		if (bClaimed)
		{
			ClaimButton->SetIsEnabled(false);
			ClaimButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			const TSubclassOf<UCommonButtonStyle> ButtonStyle = bClaimable ? ClaimStyle_Claim : ClaimStyle_Wait;
			if (ButtonStyle) ClaimButton->SetStyle(ButtonStyle);
			ClaimButton->SetButtonText(FText::FromString(bClaimable ? TEXT("받기") : TEXT("진행 중")));
			ClaimButton->SetTextColor(bClaimable ? ClaimLabelInk : WaitLabelInk);
			ClaimButton->SetIsEnabled(bClaimable && bServerTimeValid);
			ClaimButton->SetVisibility(ESlateVisibility::Visible);
		}
	}
	if (ClaimDot)
	{
		if (bClaimable && bServerTimeValid) ClaimDot->Show();
		else ClaimDot->Hide();
	}
	if (DoneStamp)
	{
		DoneStamp->SetVisibility(bClaimed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UQuestRowWidget::HandleClaimClicked()
{
	if (Item)
	{
		OnClaimRequested.Broadcast(Item);
	}
}
