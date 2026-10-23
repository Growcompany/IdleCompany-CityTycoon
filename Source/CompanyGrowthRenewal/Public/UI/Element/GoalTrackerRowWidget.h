#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Manager/GoalBoardSubsystem.h"
#include "GoalTrackerRowWidget.generated.h"

class UCommonButtonStyle;
class UCommonTextBlock;
class UBorder;
class UButton;
class UButtonWidget;
class UImage;
class UProgressBar;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoalRowIntent, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGoalRowClaimIntent, FName, int32);

/** One available growth hint with an optional guided action. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGoalTrackerRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(const FGoalBoardEntry& InEntry, bool bInTracked, bool bInExpanded, const FText& InPlaceHint);
	FName GetGoalID() const { return Entry.GoalID; }

	FOnGoalRowIntent OnRowTapped;
	FOnGoalRowIntent OnStartRequested;
	FOnGoalRowIntent OnStopRequested;
	FOnGoalRowIntent OnDismissRequested;
	FOnGoalRowClaimIntent OnClaimRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RowButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> TitleText = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> StateTagText = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> StateTagPlate = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> ExpandRoot = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> DescText = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> ActionButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> GuideDismissButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> StateDot = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ProgressShortText = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ProgressLongText = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ProgressBar = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> BarBox = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RewardRow = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RewardSampleValue = nullptr;

	UPROPERTY(EditAnywhere, Category = "GoalRow|Style")
	TSoftClassPtr<UCommonButtonStyle> ActionStyle_Start = TSoftClassPtr<UCommonButtonStyle>(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Blue_Sq.CUI_Style2_Btn_Blue_Sq_C")));
	UPROPERTY(EditAnywhere, Category = "GoalRow|Style")
	TSoftClassPtr<UCommonButtonStyle> ActionStyle_Stop = TSoftClassPtr<UCommonButtonStyle>(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Ghost_Sq.CUI_Style2_Btn_Ghost_Sq_C")));
	UPROPERTY(EditAnywhere, Category = "GoalRow|Style")
	TSoftClassPtr<UCommonButtonStyle> ActionStyle_Claim = TSoftClassPtr<UCommonButtonStyle>(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_Green_Sq.CUI_Style2_Btn_Green_Sq_C")));

private:
	UFUNCTION() void HandleRowClicked();
	void HandleActionClicked();
	void HandleDismissClicked();

	FGoalBoardEntry Entry;
	bool bTracked = false;
};
