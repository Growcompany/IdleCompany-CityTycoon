#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Enum/CompanyType.h"
#include "UI/Element/Common/ChipPlateSize.h"
#include "FusionSlotWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UProgressBar;

UCLASS()
class COMPANYGROWTHRENEWAL_API UFusionSlotWidget : public UCommonButtonBase
{
	GENERATED_BODY()
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFusionSlotClicked, int32);
	FOnFusionSlotClicked OnFusionSlotClicked;
	void Configure(int32 InSlotIndex, ECompanyType Industry, int32 BuildingIndex, float Power,
		float Requirement, bool bEditing, bool bMissingIndustry = false);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget)) UImage* SlotFrameBG = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* SlotFrameLine = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryGlyph = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryPlate = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* SelectedRing = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* IndustryText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* BuildingNameText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PowerText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* StateText = nullptr;
	UPROPERTY(meta = (BindWidget)) UProgressBar* RequirementBar = nullptr;
	UPROPERTY(EditAnywhere, Category = "Style") FLinearColor ReadyColor = FLinearColor(0.150f, 0.546f, 0.238f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Style") FLinearColor ShortColor = FLinearColor(0.784f, 0.352f, 0.032f, 1.f);

private:
	void HandleClicked();
	int32 CurrentSlotIndex = INDEX_NONE;
	FChipPlateSizer PlateSizer;
};
