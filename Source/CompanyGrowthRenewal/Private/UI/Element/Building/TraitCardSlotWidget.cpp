// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/TraitCardSlotWidget.h"
#include "Components/TextBlock.h"
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

	// 전용 뱃지 — Row 를 이미 들고 있으므로 재조회 없음
	ApplyRequirementBadge(Row.RequiredType);
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

void UTraitCardSlotWidget::ApplyRequirementBadge(EBuildingTraitRequirement InRequirement)
{
	CurrentRequirement = InRequirement;

	// 카드는 좁아서 축약 문안. None 은 60/95 행이라 표시하지 않는다(노이즈)
	FText BadgeText;
	switch (InRequirement)
	{
	case EBuildingTraitRequirement::Manufacturing: BadgeText = FText::FromString(TEXT("제조"));     break;
	case EBuildingTraitRequirement::Project:       BadgeText = FText::FromString(TEXT("프로젝트")); break;
	default:                                       break;
	}

	const bool bShow = !BadgeText.IsEmpty();
	if (RequirementBadgeText && bShow) { RequirementBadgeText->SetText(BadgeText); }

	// 카드 본체가 UCommonButtonBase 라 뱃지가 클릭을 먹으면 카드가 안 눌린다
	const ESlateVisibility BadgeVis = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (RequirementBadgeBorder)    { RequirementBadgeBorder->SetVisibility(BadgeVis); }
	else if (RequirementBadgeText) { RequirementBadgeText->SetVisibility(BadgeVis); }
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
	ApplyRequirementBadge(CurrentRequirement);
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
