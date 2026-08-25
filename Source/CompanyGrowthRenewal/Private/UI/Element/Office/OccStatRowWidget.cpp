// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/OccStatRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Data/EmployeeStatsData.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

UWidget* UOccStatRowWidget::GetPlusButtonWidget() const
{
	return PlusButton;
}

void UOccStatRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PlusButton)
	{
		PlusButton->OnClicked().AddUObject(this, &UOccStatRowWidget::OnPlusClicked);
	}
}

void UOccStatRowWidget::NativeDestruct()
{
	if (PlusButton)
	{
		PlusButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UOccStatRowWidget::SetStat(EEmployeeStatIndex InStatIndex, int32 Value)
{
	StatIndex = InStatIndex;

	if (StatNameText)
	{
		StatNameText->SetText(UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(InStatIndex)));
	}

	if (StatValueText)
	{
		StatValueText->SetText(FText::AsNumber(Value));
	}

	if (StatBar && StatBarMax > 0.f)
	{
		StatBar->SetPercent(FMath::Clamp(static_cast<float>(Value) / StatBarMax, 0.f, 1.f));
	}
}

void UOccStatRowWidget::SetCanInvest(bool bCanInvest)
{
	if (PlusButton)
	{
		// 불가 시 숨기지 말고 비활성(회색) — 사라지면 빈자리로 보여 어색함
		PlusButton->SetVisibility(ESlateVisibility::Visible);
		PlusButton->SetIsEnabled(bCanInvest);
	}
}

void UOccStatRowWidget::OnPlusClicked()
{
	OnRowInvestRequested.Broadcast(StatIndex);
}
