// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/CardInfoWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Table/BuildableCardTable.h"
#include "Table/EmployeeCardTable.h"
#include "Table/WorkstationCardTable.h"
#include "Table/DecorationCardTable.h"
#include "Enum/LootBoxRarity.h"
#include "UI/Element/Common/ResourceWidget.h"

void UCardInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCardInfoWidget::SetBuildableInfo(const FBuildableCardTable& buildableInfo)
{
	// 리컴파일 재인스턴싱 중에는 required BindWidget 도 null 로 들어온다.
	if (!BackgroundImage || !BuildEntityName)
	{
		return;
	}

	// 희귀도에 따른 배경 색상 설정
	FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(buildableInfo.Rarity);
	BackgroundImage->SetColorAndOpacity(RarityColor);

	BuildEntityName->SetText(FText::FromName(buildableInfo.Name));

	// 첫 번째 건설 비용 표시
	if (UIE_Resource && buildableInfo.ConstructionCosts.Num() > 0)
	{
		if (UResourceWidget* resourceWidget = Cast<UResourceWidget>(UIE_Resource))
		{
			const FConstructionCost& cost = buildableInfo.ConstructionCosts[0];
			resourceWidget->SetResourceType(cost.ResourceType);
			resourceWidget->SetValue(cost.Cost);
		}
	}
}

void UCardInfoWidget::SetEmployeeInfo(const FEmployeeCardTable& employeeInfo)
{
	if (!BackgroundImage || !BuildEntityName)
	{
		return;
	}

	// 배경 색상 설정 (InfoBackgroundColor 사용)
	BackgroundImage->SetColorAndOpacity(FLinearColor(employeeInfo.InfoBackgroundColor));

	BuildEntityName->SetText(FText::FromName(employeeInfo.Name));

	// 첫 번째 고용 비용 표시
	if (UIE_Resource && employeeInfo.ConstructionCosts.Num() > 0)
	{
		if (UResourceWidget* resourceWidget = Cast<UResourceWidget>(UIE_Resource))
		{
			const FConstructionCost& cost = employeeInfo.ConstructionCosts[0];
			resourceWidget->SetResourceType(cost.ResourceType);
			resourceWidget->SetValue(cost.Cost);
		}
	}
}

void UCardInfoWidget::SetWorkstationInfo(const FWorkstationCardTable& workstationInfo)
{
	// 등급 개념이 없는 카드라 정보판 색은 데이터가 아니라 정적 스타일 — UIE_CardInfo1 브러시가 소유한다.
	if (!BuildEntityName)
	{
		return;
	}

	BuildEntityName->SetText(workstationInfo.DisplayName);

	if (UIE_Resource)
	{
		UIE_Resource->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCardInfoWidget::SetDecorationInfo(const FDecorationCardTable& decorationInfo)
{
	// 색은 UIE_CardInfo1 브러시 소유 (SetWorkstationInfo 와 동일)
	if (!BuildEntityName)
	{
		return;
	}

	BuildEntityName->SetText(FText::FromName(decorationInfo.Name));

	// 가격 표시
	if (UIE_Resource)
	{
		if (UResourceWidget* resourceWidget = Cast<UResourceWidget>(UIE_Resource))
		{
			resourceWidget->SetResourceType(EResourceType::Money);
			resourceWidget->SetValue(decorationInfo.Price);
		}
	}
}

void UCardInfoWidget::UpdateCostAffordability(EResourceType ResourceType, bool bCanAfford)
{
	if (UIE_Resource)
	{
		if (UResourceWidget* ResourceWidget = Cast<UResourceWidget>(UIE_Resource))
		{
			// ResourceWidget의 타입이 일치하는 경우에만 업데이트
			if (ResourceWidget->ResourceType == ResourceType)
			{
				ResourceWidget->SetCanAfford(bCanAfford);
			}
		}
	}
}
