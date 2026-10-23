// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/LiveOps/AttendanceMilestoneNodeWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"

ESlateVisibility UAttendanceMilestoneNodeWidget::Show(bool bVisible)
{
	return bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}

void UAttendanceMilestoneNodeWidget::SetNode(int32 Day, EAttendanceNodeState State)
{
	const bool bGot = State == EAttendanceNodeState::Got;
	PlateGot->SetVisibility(Show(bGot));
	PlateNext->SetVisibility(Show(State == EAttendanceNodeState::Next));
	PlatePending->SetVisibility(Show(State == EAttendanceNodeState::Pending));
	// 남은 단계만 흑백 선물 — 다음 단계는 금 테와 함께 컬러로 미리 보여 준다
	GiftColor->SetVisibility(Show(State != EAttendanceNodeState::Pending));
	GiftGray->SetVisibility(Show(State == EAttendanceNodeState::Pending));

	DayLabel->SetText(FText::Format(NSLOCTEXT("Attendance", "MilestoneNodeDay", "{0}일"), Day));
	DayLabel->SetColorAndOpacity(bGot ? LabelInkGot : LabelInkOther);
}
