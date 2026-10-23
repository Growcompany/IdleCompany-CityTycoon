#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GoalTrackerWidget.generated.h"

class UGoalBoardSubsystem;
class UGoalTrackerRowWidget;
class UCommonTextBlock;
class UButton;
class UVerticalBox;
class UScrollBox;
class UWidget;

/** Post-tutorial HUD showing one available growth hint. */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UGoalTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitTracker();
	void SetPresentationSuppressed(bool bSuppressed);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> ExpandedRoot = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> HeaderCountText = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> FoldButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> RowScrollBox = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> RowBox = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> MoreButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> MoreLabel = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> FoldedRoot = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> UnfoldButton = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> FoldedCountText = nullptr;

private:
	void RefreshAll();
	UFUNCTION() void HandleFoldClicked();
	UFUNCTION() void HandleUnfoldClicked();
	UFUNCTION() void HandleMoreClicked();
	void HandleBoardChanged();
	void HandleTrackedChanged(FName TrackedID);
	void HandleRowTapped(FName GoalID);
	void HandleStartRequested(FName GoalID);
	void HandleStopRequested(FName GoalID);
	void HandleDismissRequested(FName GoalID);
	void HandleClaimRequested(FName GoalID, int32 ExpectedClaimCount);
	UGoalBoardSubsystem* GetBoard() const;

	bool bPresentationSuppressed = false;
	ESlateVisibility VisibilityBeforePresentation = ESlateVisibility::SelfHitTestInvisible;
	FName ExpandedGoalID = NAME_None;
	FName CollapsedHintGoalID = NAME_None;
	bool bLoggedMissingRowClass = false;
	UPROPERTY() TArray<TObjectPtr<UGoalTrackerRowWidget>> RowPool;
};
