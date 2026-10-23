#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TrainingRewardChipWidget.generated.h"

class UCommonTextBlock;
class UImage;
struct FSideDungeonLootRow;
namespace CGRewardChip { struct FDisplayEntry; }

/** 연구 성과 미리보기 칩. 폰트·배치·면은 WBP가 소유한다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTrainingRewardChipWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 1회 추첨 수량 범위와 DT에 저작된 확률 밴드를 표시한다. */
	void ConfigurePreview(const FSideDungeonLootRow& Loot);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> Icon = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> QuantityText = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> OddsText = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Training|Odds")
	FLinearColor OddsHighColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Training|Odds")
	FLinearColor OddsNormalColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Training|Odds")
	FLinearColor OddsLowColor = FLinearColor::White;

private:
	void ApplyIdentity(const CGRewardChip::FDisplayEntry& Entry);
};
