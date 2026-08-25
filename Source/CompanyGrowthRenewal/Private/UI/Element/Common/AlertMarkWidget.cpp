#include "UI/Element/Common/AlertMarkWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UAlertMarkWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyBrush();
}

void UAlertMarkWidget::SetMark(EAlertMarkSize InSize, EAlertMarkColor InColor)
{
	Size = InSize;
	Color = InColor;
	ApplyBrush();
}

void UAlertMarkWidget::SetSize(EAlertMarkSize InSize)
{
	Size = InSize;
	ApplyBrush();
}

void UAlertMarkWidget::SetColor(EAlertMarkColor InColor)
{
	Color = InColor;
	ApplyBrush();
}

void UAlertMarkWidget::Show()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UAlertMarkWidget::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UAlertMarkWidget::IsShown() const
{
	return GetVisibility() != ESlateVisibility::Collapsed
		&& GetVisibility() != ESlateVisibility::Hidden;
}

void UAlertMarkWidget::ApplyBrush()
{
	if (!IsValid(DotImage))
	{
		return;
	}

	UTexture2D* Tex = PickTexture();
	if (!IsValid(Tex))
	{
		DotImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	DotImage->SetBrushFromTexture(Tex);
	// Size enum별 설정된 픽셀 크기 강제 — 텍스처 원본이 너무 크더라도 일정 규격
	const FVector2D Target = (Size == EAlertMarkSize::Small) ? SmallSize : LargeSize;
	DotImage->SetDesiredSizeOverride(Target);
	DotImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

UTexture2D* UAlertMarkWidget::PickTexture() const
{
	switch (Size)
	{
	case EAlertMarkSize::Small:
		return (Color == EAlertMarkColor::Red) ? Tex_SmallRed : Tex_SmallGreen;
	case EAlertMarkSize::Large:
		return (Color == EAlertMarkColor::Red) ? Tex_LargeRed : Tex_LargeGreen;
	}
	return nullptr;
}
