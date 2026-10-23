#pragma once

#include "CoreMinimal.h"
#include "UI/Panel/HQLevelUpCelebrationWidget.h"
#include "WorkstationPromotionWidget.generated.h"

class UImage;
class UTexture2D;

/** Full-screen workstation grade reveal using the existing HQ celebration choreography. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorkstationPromotionWidget : public UHQLevelUpCelebrationWidget
{
	GENERATED_BODY()

public:
	void SetPromotionInfo(int32 OldGradeNumber, int32 NewGradeNumber, const FText& EquipmentChange,
		float GradeSpeedDeltaPercentPoints, TSoftObjectPtr<UTexture2D> NewGradeImage);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* PromotionImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* EffectDeltaText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RetainedNoteText = nullptr;

private:
	FText CachedEquipmentChange;
	float CachedGradeSpeedDeltaPercentPoints = 0.f;
	TSoftObjectPtr<UTexture2D> CachedGradeImage;
	float RevealElapsed = 0.f;
};
