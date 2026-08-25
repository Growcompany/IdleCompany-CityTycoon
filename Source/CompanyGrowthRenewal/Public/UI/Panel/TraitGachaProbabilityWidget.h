#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "TraitGachaProbabilityWidget.generated.h"

class UCloseButtonWidget;
class UVerticalBox;
class UTextBlock;
class UBuildingTraitManagerSubsystem;
class UBuildingSkinManagerSubsystem;

/**
 * 건물 특성/스킨 가챠 확률표 모달 — 2티어(일반/고급) 등급별 % + 천장 2종(60 Epic / 150 Legendary).
 * 자체 딤 + 중앙 패널 + 닫기 (GachaProbabilityWidget 클론).
 * 카테고리는 여는 쪽이 SetCategory 로 주입 — 천장/마일리지 상수는 양쪽 동일이라 확률표와 라벨만 갈린다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTraitGachaProbabilityWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCloseButtonWidget* UIE_CloseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* NormalRows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* AdvancedRows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PityRuleText;

	// 카테고리로 갈리는 라벨 3종 — WBP baked 문구는 저작 프리뷰일 뿐, 런타임 값은 코드가 소유
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* NormalHeader;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* AdvancedHeader;

public:
	/** 특성/스킨 전환 — AddToViewport 전에 호출해야 첫 빌드부터 맞는 표가 나온다 */
	void SetCategory(bool bSkin);

	/** 두 티어 표 + 라벨 재빌드 */
	void SetupTable();

private:
	void BuildTierRows(UVerticalBox* Container, bool bAdvanced);
	void UpdateCategoryLabels();

	UFUNCTION()
	void OnCloseClicked();

	UPROPERTY() UBuildingTraitManagerSubsystem* TraitManager = nullptr;
	UPROPERTY() UBuildingSkinManagerSubsystem* SkinManager = nullptr;

	bool bSkinCategory = false;
};
