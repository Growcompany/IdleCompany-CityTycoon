#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Enum/CompanyType.h"
#include "UI/Element/Common/ChipPlateSize.h"
#include "FusionBuildingRowWidget.generated.h"

class UCommonTextBlock;
class UImage;

UCLASS()
class COMPANYGROWTHRENEWAL_API UFusionBuildingRowWidget : public UCommonButtonBase
{
	GENERATED_BODY()
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFusionBuildingClicked, int32);
	FOnFusionBuildingClicked OnFusionBuildingClicked;
	void Configure(int32 InBuildingIndex, ECompanyType Industry, float Power, int32 CrewCount,
		float Requirement, bool bInSelected, bool bUsedElsewhere);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget)) UImage* BuildingRowFrameBG = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* BuildingRowFrameLine = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryGlyph = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* SelectedRing = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* NameText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* PowerText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* StateText = nullptr;

private:
	void HandleClicked();
	int32 CurrentBuildingIndex = INDEX_NONE;
	FChipPlateSizer PlateSizer;
};
