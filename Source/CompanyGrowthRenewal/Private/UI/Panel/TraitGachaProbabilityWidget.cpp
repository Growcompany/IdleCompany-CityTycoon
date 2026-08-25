// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/TraitGachaProbabilityWidget.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/BuildingTraitSaveData.h"
#include "Enum/LootBoxRarity.h"

void UTraitGachaProbabilityWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		TraitManager = GI->GetSubsystem<UBuildingTraitManagerSubsystem>();
		SkinManager = GI->GetSubsystem<UBuildingSkinManagerSubsystem>();
	}

	if (UIE_CloseButton && !UIE_CloseButton->OnCloseClicked.IsBound())
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UTraitGachaProbabilityWidget::OnCloseClicked);
	}

	if (PityRuleText)
	{
		PityRuleText->SetText(FText::FromString(FString::Printf(
			TEXT("%d회부터 Epic +2%%/회 · %d회 Epic 확정 · %d회 Legendary 확정"),
			FBuildingTraitPityData::SoftPityStart, FBuildingTraitPityData::HardPity, FBuildingTraitPityData::GrandPity)));
	}

	SetupTable();
}

void UTraitGachaProbabilityWidget::NativeDestruct()
{
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UTraitGachaProbabilityWidget::SetCategory(bool bSkin)
{
	bSkinCategory = bSkin;
}

void UTraitGachaProbabilityWidget::SetupTable()
{
	UpdateCategoryLabels();
	BuildTierRows(NormalRows, false);
	BuildTierRows(AdvancedRows, true);
}

void UTraitGachaProbabilityWidget::UpdateCategoryLabels()
{
	const TCHAR* Category = bSkinCategory ? TEXT("스킨") : TEXT("특성");

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("%s 확률 정보"), Category)));
	}
	if (NormalHeader)
	{
		NormalHeader->SetText(FText::FromString(FString::Printf(TEXT("일반 %s"), Category)));
	}
	if (AdvancedHeader)
	{
		AdvancedHeader->SetText(FText::FromString(FString::Printf(TEXT("고급 %s"), Category)));
	}
}

void UTraitGachaProbabilityWidget::BuildTierRows(UVerticalBox* Container, bool bAdvanced)
{
	if (!Container) { return; }
	Container->ClearChildren();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (!TraitManager) { TraitManager = GI->GetSubsystem<UBuildingTraitManagerSubsystem>(); }
		if (!SkinManager) { SkinManager = GI->GetSubsystem<UBuildingSkinManagerSubsystem>(); }
	}

	TArray<FGachaRarityChance> Rows;
	if (bSkinCategory)
	{
		if (!SkinManager) { return; }
		Rows = SkinManager->GetSkinProbabilityTableForUI(bAdvanced);
	}
	else
	{
		if (!TraitManager) { return; }
		Rows = TraitManager->GetTraitProbabilityTableForUI(bAdvanced);
	}

	for (const FGachaRarityChance& Row : Rows)
	{
		UTextBlock* Line = NewObject<UTextBlock>(this);
		Line->SetText(FText::FromString(FString::Printf(TEXT("%s   %.0f%%"),
			*FLootBoxRarityUtility::GetKoreanName(Row.Rarity), Row.Percent)));
		Line->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityColor(Row.Rarity)));
		Container->AddChildToVerticalBox(Line);
	}
}

void UTraitGachaProbabilityWidget::OnCloseClicked()
{
	// 가챠 패널 위에 뷰포트 오버레이로 떠 있으므로(스택 X) RemoveFromParent 로 닫음
	RemoveFromParent();
}
