// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/TraitGachaProbabilityWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Employee/PotentialOddsRowWidget.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/BuildingTraitSaveData.h"
#include "Enum/WidgetType.h"

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
			TEXT("에픽: %d회부터 +2%%/회 · %d회 확정\n레전드: %d회 확정"),
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

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> RowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PotentialOddsRow) : nullptr;
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TraitGachaProbability] DT_WidgetClass 에 PotentialOddsRow 행이 없습니다"));
		return;
	}

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FGachaRarityChance& Row = Rows[Index];
		UPotentialOddsRowWidget* RowWidget = Cast<UPotentialOddsRowWidget>(CreateWidget(this, RowClass));
		if (!RowWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TraitGachaProbability] PotentialOddsRow 클래스 타입이 맞지 않습니다"));
			continue;
		}
		RowWidget->SetOdds(Row.Rarity, Row.Percent);
		Container->AddChildToVerticalBox(RowWidget);
		if (Index + 1 < Rows.Num())
		{
			USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>();
			DividerSize->SetHeightOverride(2.f);
			UBorder* Divider = WidgetTree->ConstructWidget<UBorder>();
			Divider->SetBrushColor(FLinearColor(0.28f, 0.32f, 0.38f, 0.28f));
			DividerSize->AddChild(Divider);
			if (UVerticalBoxSlot* DividerSlot = Container->AddChildToVerticalBox(DividerSize))
			{
				DividerSlot->SetPadding(FMargin(24.f, 0.f, 24.f, 0.f));
			}
		}
	}
}

void UTraitGachaProbabilityWidget::OnCloseClicked()
{
	// 가챠 패널 위에 뷰포트 오버레이로 떠 있으므로(스택 X) RemoveFromParent 로 닫음
	RemoveFromParent();
}
