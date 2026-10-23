// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/CardInfoWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "TimerManager.h"
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
	QueueNameFitCheck();

	if (UIE_Resource)
	{
		UIE_Resource->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCardInfoWidget::SetDecorationInfo(const FDecorationCardTable& decorationInfo)
{
	// 정보판과 비용 칩 스타일은 카드의 WBP가 소유한다.
	if (!BuildEntityName)
	{
		return;
	}

	BuildEntityName->SetText(decorationInfo.DisplayName);
	BuildEntityName->SetVisibility(decorationInfo.DisplayName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	QueueNameFitCheck();

	// 가격 표시
	if (UIE_Resource)
	{
		if (UResourceWidget* resourceWidget = Cast<UResourceWidget>(UIE_Resource))
		{
			resourceWidget->SetResourceType(EResourceType::Money);
			resourceWidget->SetValue(decorationInfo.Price);
		}
		// 무료 장식(바닥 등)에 「0」 가격표를 달지 않는다 ― 자리는 남겨 카드 높이를 맞추고, 재사용 카드면 되살린다
		if (decorationInfo.Price <= 0)
		{
			UIE_Resource->SetVisibility(ESlateVisibility::Hidden);
		}
		else if (UIE_Resource->GetVisibility() == ESlateVisibility::Hidden)
		{
			UIE_Resource->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

void UCardInfoWidget::QueueNameFitCheck()
{
	// 목록 항목은 값을 받을 때 아직 배치 전일 수 있어 칸 폭을 다음 틱에 읽는다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UCardInfoWidget::WarnIfNameShrinksTooSmall));
	}
}

void UCardInfoWidget::WarnIfNameShrinksTooSmall()
{
	// 화면은 늘 칸에 맞춘다 ― 하한 아래로 줄 만큼 긴 표시명은 데이터를 줄이라는 신호만 남긴다
	const UScaleBox* NameFit = BuildEntityName ? Cast<UScaleBox>(BuildEntityName->GetParent()) : nullptr;
	const float Room = NameFit ? NameFit->GetCachedGeometry().GetLocalSize().X : 0.f;
	if (Room <= 0.f || BuildEntityName->GetText().IsEmpty() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const FSlateFontInfo Font = BuildEntityName->GetFont();
	const float Need = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(BuildEntityName->GetText(), Font).X;
	const float Shown = Need > Room ? Font.Size * Room / Need : Font.Size;
	if (Shown < MinShownNameSize)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardInfo] '%s' shrinks to %.1f (< %.0f) in %.0fpx name box - shorten DisplayName"),
			*BuildEntityName->GetText().ToString(), Shown, MinShownNameSize, Room);
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
