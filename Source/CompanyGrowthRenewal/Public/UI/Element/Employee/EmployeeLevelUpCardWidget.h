#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "EmployeeLevelUpCardWidget.generated.h"

class UButtonWidget;
class UCommonButtonStyle;
class UCommonTextBlock;
class UTextBlock;
class UEmployeeManager;
class UImage;
class UProgressBar;
class UTexture2D;

/** Shows the selected employee's next level, rewards and personal experience cost. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeLevelUpCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Configure(int32 InEmployeeID);
	UWidget* GetLevelUpButtonWidget() const;
	UTexture2D* GetDisciplineIconTexture(int32 DisciplineIndex) const;
	/** The owning window drives this with its shared CTA sweep clock. */
	UImage* GetLevelUpShineImage() const { return LevelUpShine; }
	bool IsLevelUpReady() const { return bLevelUpReady; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> CurrentLevelText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> NextLevelText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> NextLevelGroup;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> PrimaryDisciplineText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> CurrentWorkPowerText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> NextWorkPowerText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> CurrentStatPointText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> NextStatPointText;
	// 사무실 패널 띠의 「SP +N」 칩 숫자 — 레벨업 1번에 받는 SP. 직원창 카드에는 없다
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SPChipGain;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> ExperienceText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> LevelUpCostText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> ExperienceBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PrimaryDisciplineIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> LevelUpButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> LevelUpShine;

	// Runtime state colors; static fonts and layout stay in the WBP.
	UPROPERTY(EditDefaultsOnly, Category = "State Colors") FLinearColor ShortBalanceColor = FLinearColor(0.552f, 0.033f, 0.028f);
	UPROPERTY(EditDefaultsOnly, Category = "State Colors") FLinearColor ReadyBalanceColor = FLinearColor(0.028f, 0.34f, 0.18f);
	UPROPERTY(EditDefaultsOnly, Category = "State Colors") FLinearColor ShortBarColor = FLinearColor(0.239f, 0.608f, 0.878f);
	UPROPERTY(EditDefaultsOnly, Category = "State Colors") FLinearColor ReadyBarColor = FLinearColor(0.263f, 0.788f, 0.369f);
	UPROPERTY(EditDefaultsOnly, Category = "State Colors") FLinearColor ReadyLabelColor = FLinearColor(1.0f, 0.939f, 0.871f);
	UPROPERTY(EditDefaultsOnly, Category = "State Colors") FLinearColor BlockedLabelColor = FLinearColor(0.011f, 0.017f, 0.030f);

	// Shown while the button is blocked; a disabled button would grey out its reason label.
	UPROPERTY(EditDefaultsOnly, Category = "Level Up Button") TSubclassOf<UCommonButtonStyle> BlockedButtonStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Discipline Icons") TSoftObjectPtr<UTexture2D> DisciplineIcon0;
	UPROPERTY(EditDefaultsOnly, Category = "Discipline Icons") TSoftObjectPtr<UTexture2D> DisciplineIcon1;
	UPROPERTY(EditDefaultsOnly, Category = "Discipline Icons") TSoftObjectPtr<UTexture2D> DisciplineIcon2;
	UPROPERTY(EditDefaultsOnly, Category = "Discipline Icons") TSoftObjectPtr<UTexture2D> DisciplineIcon3;
	UPROPERTY(EditDefaultsOnly, Category = "Discipline Icons") TSoftObjectPtr<UTexture2D> DisciplineIcon4;
	UPROPERTY(EditDefaultsOnly, Category = "Discipline Icons") TSoftObjectPtr<UTexture2D> DisciplineIcon5;

private:
	UPROPERTY() TObjectPtr<UEmployeeManager> EmployeeManager;
	int32 CurrentEmployeeID = INDEX_NONE;
	bool bLevelUpReady = false;
	UPROPERTY(Transient) TSubclassOf<UCommonButtonStyle> ReadyButtonStyle;

	void RefreshFromData();
	void HandleLevelUpClicked();
	void HandleEmployeeExperienceChanged(int32 EmployeeID);
	void HandleEmployeeLevelUp(int32 EmployeeID, int32 NewLevel);
	void HandleDisciplineChanged(int32 EmployeeID);
};
