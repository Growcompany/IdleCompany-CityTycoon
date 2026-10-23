#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/LootBoxRarity.h"
#include "PotentialOddsRowWidget.generated.h"

class UCommonTextBlock;

/** 등급 확률표의 등급명과 퍼센트 한 행. */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UPotentialOddsRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @param Percent 0~100 */
	void SetOdds(ELootBoxRarity Rarity, float Percent);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* GradeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PercentText;
};
