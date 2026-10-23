// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/LiveOps/NoticeRowWidget.h"

#include "CommonTextBlock.h"
#include "Data/GameClockRules.h"
#include "UI/Element/Common/AlertMarkWidget.h"

void UNoticeRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PlateSizer.Bind(RowPlateBG, RowPlateLine);
}

void UNoticeRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	PlateSizer.Update();
}

void UNoticeRowWidget::SetNotice(const FLiveOpsNoticeDef& InDef, bool bInRead)
{
	if (TitleText)
	{
		TitleText->SetText(InDef.Title);
	}
	if (DateText)
	{
		DateText->SetText(FText::FromString(
			FGameClockRules::KstFromUnix(InDef.PostedUnix).ToString(TEXT("%Y.%m.%d"))));
	}
	if (PinText)
	{
		PinText->SetVisibility(InDef.bPinned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (AlertMark)
	{
		if (bInRead)
		{
			AlertMark->Hide();
		}
		else
		{
			AlertMark->Show();
		}
	}
}

void UNoticeRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Item = Cast<UNoticeItem>(ListItemObject);
	if (!Item)
	{
		return;
	}

	SetNotice(Item->Def, Item->bRead);
	// 엔트리 위젯은 풀에서 재사용된다 — 링은 지금 이 아이템의 선택 상태로 다시 맞춘다
	ApplySelectRing(IsListItemSelected());
}

void UNoticeRowWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserListEntry::NativeOnItemSelectionChanged(bIsSelected);

	ApplySelectRing(bIsSelected);
}

void UNoticeRowWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	// 행이 버튼이라 리스트 행(STableRow)이 클릭을 못 받는다 — 선택은 창이 리스트에 되돌려 세운다
	if (Item)
	{
		OnRowClicked.Broadcast(Item);
	}
}

void UNoticeRowWidget::ApplySelectRing(bool bRingVisible)
{
	if (SelectRing)
	{
		SelectRing->SetVisibility(bRingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
