// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Common/SeparatorWidget.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"

void USeparatorWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!IsValid(SizeBox) || !IsValid(Separator))
	{
		return;
	}

	// SizeBox 크기 설정 (0이면 override 해제 → 부모 채움)
	if (WidthOverride > 0.0f)
	{
		SizeBox->SetWidthOverride(WidthOverride);
	}
	else
	{
		SizeBox->ClearWidthOverride();
	}

	if (HeightOverride > 0.0f)
	{
		SizeBox->SetHeightOverride(HeightOverride);
	}
	else
	{
		SizeBox->ClearHeightOverride();
	}

	// RoundedBox 브러시 설정 (중앙 솔리드 라인)
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Color);
	Brush.ImageSize = FVector2D(2.0f, 2.0f);
	Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Separator->SetBrush(Brush);

	// 양 끝 그라데이션 (새 WBP 구조에만 존재, 기존 WBP 는 바인딩 없어서 스킵)
	ApplyGradientBrushes();
}

void USeparatorWidget::SetSeparatorSize(float NewWidth, float NewHeight)
{
	WidthOverride = NewWidth;
	HeightOverride = NewHeight;

	if (!IsValid(SizeBox))
	{
		return;
	}

	if (NewWidth > 0.0f)
	{
		SizeBox->SetWidthOverride(NewWidth);
	}
	else
	{
		SizeBox->ClearWidthOverride();
	}

	if (NewHeight > 0.0f)
	{
		SizeBox->SetHeightOverride(NewHeight);
	}
	else
	{
		SizeBox->ClearHeightOverride();
	}
}

void USeparatorWidget::SetSeparatorColor(FLinearColor NewColor)
{
	Color = NewColor;

	if (IsValid(Separator))
	{
		FSlateBrush Brush = Separator->GetBrush();
		Brush.TintColor = FSlateColor(NewColor);
		Separator->SetBrush(Brush);
	}

	// 양 끝 그라데이션 TintColor 도 동기화
	ApplyGradientBrushes();
}

void USeparatorWidget::ApplyGradientBrushes()
{
	// 새 WBP 구조 (HBox 3분할) 일 때만 동작 — 기존 단일 Image 구조는 Optional 바인딩이 null 이라 자동 스킵
	if (!IsValid(HBox_Root))
	{
		return;
	}

	// 끝단 색상 = Color 에 GradientAlpha 배율
	FLinearColor EndColor = Color;
	EndColor.A *= GradientAlpha;

	// 한쪽 끝 (좌/우) 설정. Length <= 0 이면 해당 쪽 Collapsed (HBox 의 Fill 영역이 자동으로 흡수)
	auto ConfigureEnd = [&](USizeBox* EndBox, UImage* EndImage, float Length)
	{
		const bool bEnabled = Length > 0.0f;

		if (IsValid(EndBox))
		{
			EndBox->SetVisibility(bEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			if (bEnabled)
			{
				EndBox->SetWidthOverride(Length);
			}
			else
			{
				EndBox->ClearWidthOverride();
			}
		}

		// WBP 에서 지정한 텍스처/DrawAs 는 보존하고 Tint·ImageSize 만 갱신
		if (IsValid(EndImage) && bEnabled)
		{
			FSlateBrush GradientBrush = EndImage->GetBrush();
			GradientBrush.TintColor = FSlateColor(EndColor);
			GradientBrush.ImageSize = FVector2D(Length, FMath::Max(HeightOverride, 2.0f));
			EndImage->SetBrush(GradientBrush);
		}
	};

	ConfigureEnd(SizeBox_Left,  Image_LeftGradient,  LeftGradientLength);
	ConfigureEnd(SizeBox_Right, Image_RightGradient, RightGradientLength);
}
