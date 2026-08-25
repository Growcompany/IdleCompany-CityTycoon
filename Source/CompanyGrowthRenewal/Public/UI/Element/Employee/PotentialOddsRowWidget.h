#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/LootBoxRarity.h"
#include "PotentialOddsRowWidget.generated.h"

class UCommonTextBlock;
class UProgressBar;

/**
 * 확률표 1행 (등급명 + 막대 + %).
 *
 * 런타임 NewObject<UTextBlock> 대신 WBP 인스턴스를 쓰는 이유 = 스타일 없는 TextBlock 은
 * 엔진 Roboto 로 폴백해 한글이 깨진다 (UI_TYPOGRAPHY §1). 폰트/색은 WBP 가 소유.
 */
UCLASS()
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

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* OddsBar;

	// 1% 미만도 막대가 보이게 하는 하한 (0.5% 레전드리가 완전히 사라지지 않도록)
	UPROPERTY(EditAnywhere, Category = "Odds")
	float MinBarFraction = 0.02f;
};
