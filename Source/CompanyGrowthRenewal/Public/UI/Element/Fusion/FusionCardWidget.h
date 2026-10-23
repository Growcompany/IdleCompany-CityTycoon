#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "FusionCardWidget.generated.h"

class UCommonTextBlock;
class UHorizontalBox;
class UImage;
struct FFusionProjectTable;
struct FMissionReward;

UCLASS()
class COMPANYGROWTHRENEWAL_API UFusionCardWidget : public UCommonButtonBase
{
	GENERATED_BODY()
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFusionClicked, FName);
	FOnFusionClicked OnFusionClicked;
	void Configure(FName InKey, const FFusionProjectTable& Row, int32 Level, float Requirement,
		const TArray<FMissionReward>& Rewards, const FText& Status, bool bInLocked, bool bActive);

protected:
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* NameText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* LevelText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* IndustryText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* DurationText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* RequirementText = nullptr;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* StatusText = nullptr;
	UPROPERTY(meta = (BindWidget)) UHorizontalBox* RewardBox = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* SelectedRing = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryGlyph0 = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryGlyph1 = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryGlyph2 = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryPlate0 = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryPlate1 = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* IndustryPlate2 = nullptr;
	UPROPERTY(meta = (BindWidget)) UWidget* IndustryTile2 = nullptr;
	UPROPERTY(meta = (BindWidget)) UWidget* Connector2 = nullptr;
	UPROPERTY(EditAnywhere, Category = "Style") FLinearColor LockedColor = FLinearColor(0.784f, 0.352f, 0.032f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Style") FLinearColor AvailableColor = FLinearColor(0.150f, 0.546f, 0.238f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Style") FLinearColor LockedPlateColor = FLinearColor(0.090f, 0.117f, 0.154f, 1.f);

private:
	void HandleClicked();
	FName FusionKey;
};
