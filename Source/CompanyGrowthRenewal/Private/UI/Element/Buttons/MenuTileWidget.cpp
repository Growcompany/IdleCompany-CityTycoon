// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/MenuTileWidget.h"
#include "CommonTextBlock.h"

void UMenuTileWidget::SetLocked(bool bInLocked, const FText& Reason)
{
	bLocked = bInLocked;

	if (LockLayer)
	{
		LockLayer->SetVisibility(bInLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LockReasonText)
	{
		LockReasonText->SetText(Reason);
	}

	// 잠금이어도 입력은 받아 삼킨다(PitchTierTab 선례). SetIsEnabled 는 잠금 레이어까지 흐려서 쓰지 않는다.
	SetIsInteractionEnabled(true);
	if (bInLocked)
	{
		SetRejectReason(Reason.IsEmpty()
			? NSLOCTEXT("MenuPanel", "TileLockedDefault", "아직 잠겨 있습니다")
			: Reason);
	}
	else
	{
		ClearRejectReason();
	}
	if (bInLocked)
	{
		HideBadge();
	}
}
