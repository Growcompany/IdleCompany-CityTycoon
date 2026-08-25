// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/BuildingTraitRequirement.h"
#include "TraitCardSlotWidget.generated.h"

class UTextBlock;
class UCommonButtonStyle;
struct FBuildingTraitTableRow;

UCLASS()
class COMPANYGROWTHRENEWAL_API UTraitCardSlotWidget : public UItemCardSlotWidget
{
	GENERATED_BODY()

public:
	// TraitID 기반으로 DT 조회 → 아이콘/등급/이름 일괄 세팅
	UFUNCTION(BlueprintCallable, Category = "TraitCard")
	void SetTraitData(FName InTraitID, int32 InQuantity, int32 InEquippedCount = 0);

	// 등급만 변경 (MID 색상 + CUI Style 교체)
	UFUNCTION(BlueprintCallable, Category = "TraitCard")
	void SetRarity(ELootBoxRarity InRarity);

	// 장착 중 뱃지 표시/숨김
	UFUNCTION(BlueprintCallable, Category = "TraitCard")
	void SetEquippedCount(int32 InCount);

	UFUNCTION(BlueprintPure, Category = "TraitCard")
	ELootBoxRarity GetRarity() const { return CurrentRarity; }

	UFUNCTION(BlueprintPure, Category = "TraitCard")
	FName GetTraitID() const { return TraitID; }

	// 에디터 디자이너에서 등급별 프리뷰용 — 인스턴스마다 다르게 설정 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraitCard|Preview")
	ELootBoxRarity PreviewRarity = ELootBoxRarity::Common;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void SynchronizeProperties() override;

	// 특성 이름 라벨 (카드 아래 또는 안쪽, WBP 배치에 따라)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TraitNameText;

	// 장착 뱃지 영역 (건물 아이콘 + 숫자, WBP에서 배치)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EquippedBadgeText;

	// 장착 뱃지 배경 래퍼 (plain TextBlock 은 배경이 없어 Border 로 감쌈 — 텍스트와 함께 토글)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EquippedBadgeBorder;

	// 전용 특성 뱃지 (우상단 — 좌상단 장착 뱃지와 자리를 나눠 쓴다)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RequirementBadgeText;

	// 전용 뱃지 배경 래퍼 (장착 뱃지와 동일 구조 — 있으면 래퍼가 가시성을 소유)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RequirementBadgeBorder;

	// 등급별 CUI Style 매핑 — WBP Class Defaults에서 6개 채우면 모든 인스턴스가 공유
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraitCard|Style")
	TMap<ELootBoxRarity, TSubclassOf<UCommonButtonStyle>> RarityStyleMap;

private:
	FName TraitID = NAME_None;
	ELootBoxRarity CurrentRarity = ELootBoxRarity::Common;

	// SetTraitData 가 NativeConstruct 보다 먼저 오므로, 구성 후 재적용할 값들을 들고 있어야 한다
	EBuildingTraitRequirement CurrentRequirement = EBuildingTraitRequirement::None;
	int32 CurrentEquippedCount = 0;

	void ApplyRarityVisual(ELootBoxRarity Rarity);
	void ApplyRequirementBadge(EBuildingTraitRequirement InRequirement);
};
