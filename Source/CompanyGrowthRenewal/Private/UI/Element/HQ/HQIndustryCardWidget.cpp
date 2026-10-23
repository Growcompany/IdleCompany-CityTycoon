// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/HQ/HQIndustryCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/CompanyInfoTable.h"

void UHQIndustryCardWidget::SetIndustry(const FCompanyInfoTable& Info, EHQIndustryState State, const FHQIndustryMetrics& Metrics)
{
	const bool bLocked = State == EHQIndustryState::Locked;
	const bool bOperating = State == EHQIndustryState::Operating;

	if (NameText) NameText->SetText(Info.DisplayName);
	if (CategoryText) CategoryText->SetText(Info.CategoryLabel);

	const FLinearColor Top = bLocked ? LockedRibbonTop : HQIndustryPalette::RibbonTop(Info.AccentColor);
	const FLinearColor Bottom = bLocked ? LockedRibbonBottom : HQIndustryPalette::RibbonBottom(Info.AccentColor);
	if (RibbonBorder) RibbonBorder->SetBrushColor(Bottom);
	if (RibbonSheen) RibbonSheen->SetColorAndOpacity(Top);
	if (NotchRing)
	{
		FSlateBrush RingBrush = NotchRing->GetBrush();
		RingBrush.OutlineSettings.Color = FSlateColor(Top);
		NotchRing->SetBrush(RingBrush);
	}
	if (LockBadge) LockBadge->SetVisibility(bLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	UTexture2D* IconTexture = Info.PanelIcon.IsNull() ? nullptr : Info.PanelIcon.LoadSynchronous();
	if (!IconTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HQIndustryCard] DT_CompanyInfo.PanelIcon missing for company type %d"), static_cast<int32>(Info.CompanyType));
	}
	ApplyIcon(IconTexture, bLocked);

	if (MetricsBox) MetricsBox->SetVisibility(bOperating ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (FeatureBox) FeatureBox->SetVisibility(bOperating ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	if (bOperating)
	{
		if (BuildingsValue) BuildingsValue->SetText(FText::AsNumber(Metrics.Buildings));
		if (EmployeesValue) EmployeesValue->SetText(FText::AsNumber(Metrics.Employees));
		if (TierValue) TierValue->SetText(FText::AsNumber(Metrics.HighestTier));
		if (IncomeValue) IncomeValue->SetText(UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(Metrics.NetPerSec)));
	}
	else
	{
		if (FeatureTitleText)
		{
			FeatureTitleText->SetText(bLocked
				? FText::Format(NSLOCTEXT("HQ", "IndustryUnlockAt", "Lv.{0}에 해금"), FText::AsNumber(Info.RequiredHQLevel))
				: NSLOCTEXT("HQ", "IndustryNotEntered", "미진출"));
		}
		if (FeatureDescText) FeatureDescText->SetText(Info.Feature1);
		if (FeatureSubText)
		{
			FeatureSubText->SetText(bLocked
				? Info.Feature2
				: NSLOCTEXT("HQ", "IndustryEnterHint", "건설 메뉴에서 이 업종 빌딩을 지으면 진출합니다"));
		}
	}

	if (StatusStrip) StatusStrip->SetBrushColor(bOperating ? HQIndustryPalette::StripFill(Info.AccentColor) : NeutralStripColor);
	if (StatusText)
	{
		StatusText->SetColorAndOpacity(FSlateColor(bOperating ? HQIndustryPalette::StripInk(Info.AccentColor) : NeutralStripInk));
		StatusText->SetText(bOperating ? NSLOCTEXT("HQ", "IndustryOperating", "운영 중")
			: bLocked ? NSLOCTEXT("HQ", "IndustryLocked", "잠김")
			: NSLOCTEXT("HQ", "IndustryNoBuilding", "빌딩 0개"));
	}
}

void UHQIndustryCardWidget::ApplyIcon(UTexture2D* Texture, bool bLocked)
{
	if (!IconImage) return;
	if (!Texture)
	{
		IconImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (bLocked && LockedIconMaterial)
	{
		UMaterialInstanceDynamic* Desaturated = UMaterialInstanceDynamic::Create(LockedIconMaterial, this);
		Desaturated->SetTextureParameterValue(TEXT("Icon"), Texture);
		IconImage->SetBrushFromMaterial(Desaturated);
	}
	else
	{
		IconImage->SetBrushFromTexture(Texture, false);
	}
	IconImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bLocked ? LockedIconOpacity : 1.f));
}
