// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Building/BuildingLightCardWidget.h"
#include "UI/Element/Building/SkinInfoWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"

namespace
{
	// HSV 공간에서 채도/명도 보정 — 카드 tint를 모던 톤으로 (머티리얼 발광색은 원본 유지)
	// Saturation 0.70: 형광 순색은 죽이되 컬러 정체성은 유지
	// Value 0.85: 살짝만 다운 — 흰 배경 위에서 형태 살아남는 정도
	FLinearColor ToCardDisplayTint(const FLinearColor& Source)
	{
		FLinearColor HSV = Source.LinearRGBToHSV();
		HSV.G *= 0.70f; // Saturation
		HSV.B *= 0.85f; // Value
		FLinearColor Result = HSV.HSVToLinearRGB();
		Result.A = Source.A;
		return Result;
	}
}

void UBuildingLightCardWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (IsDesignTime())
    {
        const FLinearColor DisplayTint = ToCardDisplayTint(PreviewColor);
        if (EntityImage)
        {
            EntityImage->SetColorAndOpacity(DisplayTint);
        }

        if (UIE_SkinInfo)
        {
            UIE_SkinInfo->SetSkinName(PreviewLightName);
            UIE_SkinInfo->SetBackgroundColor(DisplayTint);
        }
    }
}

void UBuildingLightCardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!bLightDataSet)
    {
        const FLinearColor DisplayTint = ToCardDisplayTint(PreviewColor);
        if (EntityImage)
        {
            EntityImage->SetColorAndOpacity(DisplayTint);
        }

        if (UIE_SkinInfo)
        {
            UIE_SkinInfo->SetSkinName(PreviewLightName);
            UIE_SkinInfo->SetBackgroundColor(DisplayTint);
        }
    }

    SetIsSelectable(true);
    SetIsToggleable(false);
}

void UBuildingLightCardWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void UBuildingLightCardWidget::SetLightData(const FBuildingLightData& InLightData, bool bIsUnlocked)
{
    LightData = InLightData;
    bUnlocked = bIsUnlocked;
    bLightDataSet = true;

    // 카드 표시용으로 채도/명도 다운 — 머티리얼 발광색(ApplyLight 경로)은 원본 그대로
    const FLinearColor DisplayTint = ToCardDisplayTint(LightData.EmissiveColor);

    if (EntityImage)
    {
        EntityImage->SetColorAndOpacity(DisplayTint);
    }

    if (UIE_SkinInfo)
    {
        UIE_SkinInfo->SetSkinInfo(LightData.DisplayName, LightData.Rarity);
    }

    if (LockBorder)
    {
        LockBorder->SetVisibility(bUnlocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}

void UBuildingLightCardWidget::SetLocked(bool bIsLocked)
{
    bUnlocked = !bIsLocked;

    if (LockBorder)
    {
        LockBorder->SetVisibility(bIsLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}
