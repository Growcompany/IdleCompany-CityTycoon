// 개별 버블 비주얼 위젯
// Container가 SetRenderScale/Opacity/Translation으로 애니메이션 제어

#include "UI/Element/Chat/BubbleElementWidget.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

void UBubbleElementWidget::SetBGMaterial(UMaterialInterface* InMaterial)
{
	if (!BGImage || !InMaterial) return;

	BGImage->SetBrushFromMaterial(InMaterial);
}

void UBubbleElementWidget::SetBGTexture(UTexture2D* InTexture)
{
	if (!BGImage) return;

	if (InTexture)
	{
		BGImage->SetBrushFromTexture(InTexture);
	}
	else
	{
		// 텍스처 미로드 시 폴백: 흰색 둥근 사각형
		FSlateBrush FallbackBrush;
		FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FallbackBrush.TintColor = FSlateColor(FLinearColor::White);
		FallbackBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		BGImage->SetBrush(FallbackBrush);
	}
}

void UBubbleElementWidget::SetIconTexture(UTexture2D* InTexture)
{
	if (!IconImage) return;

	if (InTexture)
	{
		IconImage->SetBrushFromTexture(InTexture);
	}
	else
	{
		// 아이콘 미로드 시 폴백: 회색 원
		FSlateBrush FallbackBrush;
		FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FallbackBrush.TintColor = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
		FallbackBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		IconImage->SetBrush(FallbackBrush);
	}
}

void UBubbleElementWidget::SetBubbleVisual(UMaterialInterface* InShell, UTexture2D* InBGTexture, UTexture2D* InIconTexture)
{
	if (InShell)
	{
		SetBGMaterial(InShell);
	}
	else
	{
		SetBGTexture(InBGTexture);
	}

	SetIconTexture(InIconTexture);
}

void UBubbleElementWidget::ApplyIdleBounce(float YOffset)
{
	if (!BounceRoot) return;

	BounceRoot->SetRenderTranslation(FVector2D(0.0f, YOffset));
}

void UBubbleElementWidget::SetBubbleInfo(int32 InBuildingIndex, EBubbleType InType)
{
	BuildingIndex = InBuildingIndex;
	BubbleType = InType;
}

void UBubbleElementWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnBubbleClicked.ExecuteIfBound(BuildingIndex, BubbleType);
}
