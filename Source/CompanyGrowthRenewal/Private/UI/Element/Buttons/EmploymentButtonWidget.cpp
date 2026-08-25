// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Buttons/EmploymentButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"

#include "Core/CGGameInstance.h"

void UEmploymentButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	SetVisibility(ESlateVisibility::Visible);

	EmploymentButton->OnClicked().AddUObject(this, &UEmploymentButtonWidget::OnEmploymentButtonClicked);
}

void UEmploymentButtonWidget::NativeDestruct()
{
	Super::NativeDestruct();
	EmploymentButton->OnClicked().RemoveAll(this);
}

void UEmploymentButtonWidget::OnEmploymentButtonClicked()
{
}
