// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/TraitCardSlotWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CommonButtonBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Enum/LootBoxRarity.h"

void UTraitCardSlotWidget::SetTraitData(FName InTraitID, int32 InQuantity, int32 InEquippedCount)
{
	TraitID = InTraitID;
	SetItemID(InTraitID);

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	FBuildingTraitTableRow Row;
	if (!TableMgr->GetBuildingTraitData(InTraitID, Row)) return;

	// 아이콘
	if (!Row.Icon.IsNull())
	{
		UTexture2D* IconTex = Row.Icon.LoadSynchronous();
		if (IconTex) SetIcon(IconTex);
	}

	// 수량 (B안: 총량 표시)
	SetQuantity(InQuantity + InEquippedCount);

	// 등급 비주얼
	SetRarity(Row.Rarity);

	// 이름
	if (TraitNameText)
	{
		TraitNameText->SetText(Row.DisplayName);
	}

	// 장착 뱃지
	SetEquippedCount(InEquippedCount);
}

void UTraitCardSlotWidget::SetRarity(ELootBoxRarity InRarity)
{
	CurrentRarity = InRarity;
	ApplyRarityVisual(InRarity);
}

void UTraitCardSlotWidget::SetEquippedCount(int32 InCount)
{
	CurrentEquippedCount = InCount;

	if (!EquippedBadgeText) return;

	const ESlateVisibility Vis = (InCount > 0) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (InCount > 0)
	{
		EquippedBadgeText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InCount)));
	}

	// 래퍼가 있으면 래퍼가 가시성을 소유(텍스트는 항상 표시), 없으면(구 트리) 텍스트가 직접 토글
	if (EquippedBadgeBorder)
	{
		EquippedBadgeBorder->SetVisibility(Vis);
		EquippedBadgeText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		EquippedBadgeText->SetVisibility(Vis);
	}
}

void UTraitCardSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터 디자이너에서만 PreviewRarity 적용. 런타임에는 SetRarity() 호출 결과 보존.
	if (IsDesignTime())
	{
		ApplyRarityVisual(PreviewRarity);
	}
}

void UTraitCardSlotWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// 디자이너 프리뷰만. 런타임은 SetTraitData/SetRarity 가 처리.
	if (IsDesignTime())
	{
		ApplyRarityVisual(PreviewRarity);
	}
}

void UTraitCardSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// SetTraitData 는 AddChild(=NativeConstruct 유발) 보다 먼저 실행되므로 무조건 Collapsed 는 뱃지를 지운다 — 캐시값 재적용
	SetEquippedCount(CurrentEquippedCount);
	SetDetailSelected(bDetailSelected);
}

void UTraitCardSlotWidget::SetDetailSelected(bool bInSelected)
{
	bDetailSelected = bInSelected;
	PulseTime = 0.f;
	if (!SelectRing) return;
	SelectRing->SetVisibility(bDetailSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ApplyRingPulse();
}

void UTraitCardSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bDetailSelected)
	{
		PulseTime += InDeltaTime;
		ApplyRingPulse();
	}
}

void UTraitCardSlotWidget::ApplyRingPulse()
{
	UMaterialInstanceDynamic* MID = SelectRing ? SelectRing->GetDynamicMaterial() : nullptr;
	if (!MID) return;
	if (RingPeakAlpha < 0.f)
	{
		RingPeakAlpha = MID->K2_GetScalarParameterValue(TEXT("BaseA"));
	}
	// UTraitSlotWidget::ApplySelectedRingPulse 와 같은 곡선 ― 최고점의 0.45~1.0 배, 고른 순간이 가장 밝다
	MID->SetScalarParameterValue(TEXT("BaseA"), RingPeakAlpha * (0.725f + 0.275f * FMath::Cos(PulseTime * 4.5f)));
}

void UTraitCardSlotWidget::ApplyRarityVisual(ELootBoxRarity Rarity)
{
	if (!UIE_ItemCard) return;

	if (const TSubclassOf<UCommonButtonStyle>* Found = RarityStyleMap.Find(Rarity))
	{
		if (*Found)
		{
			UIE_ItemCard->SetStyle(*Found);
		}
	}
}
