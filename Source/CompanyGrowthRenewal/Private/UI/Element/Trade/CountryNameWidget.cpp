// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Trade/CountryNameWidget.h"
#include "UI/Element/Trade/CountrySellRowWidget.h"
#include "Manager/CountryMarketManager.h"
#include "Data/WorldMapTypes.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"

void UCountryNameWidget::SetCountryType(ECountryType InType)
{
	if (CountryType == InType) return;
	CountryType = InType;

	// CountryType 변경 시 닷 즉시 갱신 — SetCountryType 이 NativeConstruct 후 호출되거나
	// 동적 변경 시 stale 표시 방지. MarketMgr nullptr 시 RefreshDot이 자체적으로 Collapsed 처리.
	RefreshAllDots();
}

void UCountryNameWidget::SetFlagTexture(UTexture2D* FlagTexture)
{
	if (FlagIcon && FlagTexture)
	{
		FlagIcon->SetBrushFromTexture(FlagTexture);
		FlagIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UCountryNameWidget::SetLockState(bool bInLocked, const FText& InLockReason)
{
	bLocked = bInLocked;

	const FLinearColor TargetColor = bInLocked ? LockedTint : FLinearColor::White;
	if (BGImage)  BGImage->SetColorAndOpacity(TargetColor);
	if (FlagIcon) FlagIcon->SetColorAndOpacity(TargetColor);

	const ESlateVisibility LockVis = bInLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (LockImage) LockImage->SetVisibility(LockVis);
	if (LockText)
	{
		LockText->SetVisibility(LockVis);
		if (bInLocked && !InLockReason.IsEmpty())
		{
			LockText->SetText(InLockReason);
		}
	}

	SetIsInteractionEnabled(!bInLocked);
}

void UCountryNameWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (bLocked) return;
	OnFlagClicked.ExecuteIfBound(CountryType);
}

void UCountryNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		MarketMgr = GI->GetSubsystem<UCountryMarketManager>();
		if (MarketMgr)
		{
			MarketMgr->OnDemandChanged.AddDynamic(this, &UCountryNameWidget::HandleDemandChanged);
		}
	}

	RefreshAllDots();
}

void UCountryNameWidget::NativeDestruct()
{
	if (MarketMgr)
	{
		MarketMgr->OnDemandChanged.RemoveDynamic(this, &UCountryNameWidget::HandleDemandChanged);
		MarketMgr = nullptr;
	}

	Super::NativeDestruct();
}

void UCountryNameWidget::RefreshAllDots()
{
	RefreshDot(DotSemiconductor, ECompanyType::Semiconductor);
	RefreshDot(DotElectronics, ECompanyType::Electronics);
	RefreshDot(DotAutomobile, ECompanyType::Automobile);
}

void UCountryNameWidget::RefreshDot(UImage* Dot, ECompanyType Industry)
{
	if (!Dot) return;

	if (!MarketMgr || CountryType == ECountryType::None)
	{
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bool bFound = false;
	const FCountryMarketState State = MarketMgr->GetMarketState(CountryType, Industry, bFound);
	if (!bFound)
	{
		// DT_CountryDemand 미정의 셀 — 닷 자체 숨김 (이 나라엔 이 산업 수요 정의 없음)
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
	Dot->SetColorAndOpacity(UCountrySellRowWidget::GetGaugeColor(State.GetRatio()));
}

void UCountryNameWidget::HandleDemandChanged(ECountryType InCountry, ECompanyType InIndustry, float NewRatio)
{
	if (InCountry != CountryType) return;

	switch (InIndustry)
	{
	case ECompanyType::Semiconductor: RefreshDot(DotSemiconductor, InIndustry); break;
	case ECompanyType::Electronics:   RefreshDot(DotElectronics, InIndustry);   break;
	case ECompanyType::Automobile:    RefreshDot(DotAutomobile, InIndustry);    break;
	default: break;
	}
}
