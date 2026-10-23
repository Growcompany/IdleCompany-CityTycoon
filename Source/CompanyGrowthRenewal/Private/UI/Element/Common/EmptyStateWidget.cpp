// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/EmptyStateWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Global/GlobalUtilFunctions.h"

void UEmptyStateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyEmptyState();
}

void UEmptyStateWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyEmptyState();
}

void UEmptyStateWidget::ApplyEmptyState()
{
	UGlobalUtilFunctions::ApplyWidgetIcon(EmptyIcon, IconTexture, IconSize, IconPadding);
	if (EmptyIcon)
	{
		EmptyIcon->SetColorAndOpacity(IconTint);
	}

	if (EmptyMessage)
	{
		EmptyMessage->SetText(Message);
		EmptyMessage->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UEmptyStateWidget::SetEmptyState(UTexture2D* Icon, const FText& Msg)
{
	// 프로퍼티에 반영해 둬야 늦은 SynchronizeProperties 가 WBP 저작값으로 되덮지 않는다
	IconTexture = Icon;
	Message = Msg;
	ApplyEmptyState();
}

void UEmptyStateWidget::SetMessage(const FText& Msg)
{
	Message = Msg;
	ApplyEmptyState();
}
