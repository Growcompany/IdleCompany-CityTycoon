// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/CardWidgetBase.h"
#include "UI/Element/Cards/EntityCardFrameWidget.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"

void UCardWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// UI 기본 크기
	FVector2d minDesiredSize = GetMinimumDesiredSize();
	minDesiredSize.Y = 250.f;
	SetMinimumDesiredSize(minDesiredSize);

	this->OnClicked().AddUObject(this, &UCardWidgetBase::OnButtonClicked);
	this->OnHovered().AddUObject(this, &UCardWidgetBase::OnButtonHovered);
	this->OnUnhovered().AddUObject(this, &UCardWidgetBase::OnButtonUnhovered);
}

void UCardWidgetBase::NativeDestruct()
{
	Super::NativeDestruct();
}

void UCardWidgetBase::OnIconLoaded(FSoftObjectPath LoadedPath)
{
	UObject* LoadedObj = LoadedPath.ResolveObject();
	if (!LoadedObj)
	{
		LoadedObj = LoadedPath.TryLoad();
	}

	if (UTexture2D* Tex = Cast<UTexture2D>(LoadedObj))
	{
		if (UImage* Image = GetEntityImage())
		{
			Image->SetBrushFromTexture(Tex);
		}
	}
}

UImage* UCardWidgetBase::GetEntityImage() const
{
	// 프레임이 있으면 프레임의 이미지 사용
	if (CardFrame)
	{
		return CardFrame->GetEntityImage();
	}
	// 없으면 직접 바인딩된 이미지 사용 (레거시)
	return EntityImage;
}

void UCardWidgetBase::OnButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("UCardWidget::OnButtonClicked (Base)"));
}

void UCardWidgetBase::OnButtonHovered()
{
}

void UCardWidgetBase::OnButtonUnhovered()
{
}

