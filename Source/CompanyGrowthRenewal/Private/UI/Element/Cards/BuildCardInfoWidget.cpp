// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/BuildCardInfoWidget.h"

#include "Components/Image.h"
#include "Table/BuildableCardTable.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "UI/Element/Common/ResourceWidget.h"

void UBuildCardInfoWidget::SetBuildableInfo(const FBuildableCardTable& buildableInfo)
{
	Super::SetBuildableInfo(buildableInfo);

	// D3: 등급 배경 틴트 무력화 — 베이스가 BackgroundImage 를 RarityColor 로 칠한 직후 흰 곱셈(중립)으로 되돌림.
	if (BackgroundImage)
	{
		BackgroundImage->SetColorAndOpacity(FLinearColor::White);
	}

	// 멀티코스트: 1차는 베이스가 UIE_Resource 에 세팅. 2차 칩은 비용 2종 이상일 때만.
	const int32 CostNum = buildableInfo.ConstructionCosts.Num();
	if (SecondaryCostResource)
	{
		if (CostNum >= 2)
		{
			const FConstructionCost& Cost = buildableInfo.ConstructionCosts[1];
			SecondaryCostResource->SetResourceType(Cost.ResourceType);
			SecondaryCostResource->SetValue(Cost.Cost);
			SecondaryCostResource->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			SecondaryCostResource->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 칩은 최대 2개 — 3종 이상이면 1·2차만 표시되고 나머지는 누락되므로 경고로 드러냄.
	if (CostNum > 2)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UBuildCardInfoWidget: '%s' has %d construction costs; cost chips truncated to 2."),
			*buildableInfo.Name.ToString(), CostNum);
	}
}

void UBuildCardInfoWidget::UpdateCostAffordability(EResourceType ResourceType, bool bCanAfford)
{
	// 1차(UIE_Resource)는 베이스가 처리.
	Super::UpdateCostAffordability(ResourceType, bCanAfford);

	if (SecondaryCostResource && SecondaryCostResource->ResourceType == ResourceType)
	{
		SecondaryCostResource->SetCanAfford(bCanAfford);
	}
}

void UBuildCardInfoWidget::SetBuildingStats(int32 WidthCells, int32 DepthCells, int32 NewBuildEmployees)
{
	// 라벨("크기"/"인원")은 WBP DefaultStatName 으로 베이크 — 코드는 값만 주입(데이터주도).
	if (SizeStatRow)
	{
		FString SizeStr = FString::Printf(TEXT("%d × %d"), WidthCells, DepthCells);
		if (WidthCells != DepthCells)
		{
			// 비정사각만 총 칸수 접미(예: 2 x 3 (6)) — 정사각은 자명하므로 생략.
			SizeStr += FString::Printf(TEXT(" (%d)"), WidthCells * DepthCells);
		}
		SizeStatRow->SetStatValue(SizeStr);
	}

	if (CapacityStatRow)
	{
		// "최대"가 아니라 "기본" — 이 값은 증축 0층 기준이고 층을 올리면 늘어난다.
		// 증축 상한(MaxModuleCount)이 전 행 동일이라 건물 간 변별점은 이 기본값 쪽이다.
		CapacityStatRow->SetStatValue(FString::Printf(TEXT("기본 %d명"), NewBuildEmployees));
	}
}

