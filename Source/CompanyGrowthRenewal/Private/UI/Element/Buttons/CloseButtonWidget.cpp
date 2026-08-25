// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"

void UCloseButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseBtn)
	{
		CloseBtn->OnClicked.AddDynamic(this, &UCloseButtonWidget::HandleCloseButtonClicked);
	}
}

void UCloseButtonWidget::NativeDestruct()
{
	if (CloseBtn)
	{
		CloseBtn->OnClicked.RemoveDynamic(this, &UCloseButtonWidget::HandleCloseButtonClicked);
	}

	Super::NativeDestruct();
}

void UCloseButtonWidget::HandleCloseButtonClicked()
{
	// 닫힘 사운드는 UIBase 의 스택 표시 변경 hook 이 재생 — 여기서 재생하면 X버튼 닫기만 더블플레이.
	// (이 버튼은 모달 전용이 아니라 BottomStack 패널에서도 쓰이므로 스택 hook 이 올바른 사운드를 고름)
	OnCloseClicked.Broadcast();
}
