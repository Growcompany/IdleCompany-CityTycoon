// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/BuildCardInfoWidget.h"

#include "Components/Image.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Manager/TableManagerSubsystem.h"
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

	if (BuildCostBackgroundColor.A > 0.f)
	{
		auto ApplyCostColor = [this](UUserWidget* CostWidget)
		{
			if (!CostWidget || !CostWidget->WidgetTree) return;
			CostWidget->WidgetTree->ForEachWidget([this](UWidget* CostChild)
			{
				if (UBorder* CostBorder = Cast<UBorder>(CostChild))
				{
					UMaterialInstanceDynamic* CostMaterial = CostBorder->GetDynamicMaterial();
					FSlateBrush CostBrush = CostBorder->Background;
					if (CostMaterial)
					{
						CostMaterial->SetVectorParameterValue(TEXT("Button Col"), BuildCostBackgroundColor);
						CostBrush.TintColor = FSlateColor(FLinearColor::White);
					}
					else
					{
						CostBrush.TintColor = FSlateColor(BuildCostBackgroundColor);
					}
					CostBorder->SetBrush(CostBrush);
				}
			});
		};
		ApplyCostColor(UIE_Resource);
		ApplyCostColor(SecondaryCostResource);
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

void UBuildCardInfoWidget::SetBuildingStats(int32 WidthCells, int32 DepthCells, int32 NewBuildEmployees, float IncomeBonusPercent)
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

	if (IncomeStatRow)
	{
		// 0% 도 접지 않고 그대로 보여준다 — 칸수 안에서 등급이 올라가는 사다리가 보이려면 바닥 칸이 필요하다.
		IncomeStatRow->SetStatValue(FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(FMath::Max(0.f, IncomeBonusPercent))));

		UTexture2D* MoneyIcon = nullptr;
		if (UGameInstance* CardGameInstance = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = CardGameInstance->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bFound = false;
				const FResourceInfo MoneyInfo = TableMgr->GetResourceInfo(EResourceType::Money, bFound);
				if (bFound && !MoneyInfo.Icon.IsNull())
				{
					MoneyIcon = MoneyInfo.Icon.LoadSynchronous();
					if (!MoneyIcon)
					{
						UE_LOG(LogTemp, Warning, TEXT("UBuildCardInfoWidget: Money icon could not be loaded from DT_Resource."));
					}
				}
			}
		}
		IncomeStatRow->SetStatIcon(MoneyIcon);
	}
}

