#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/GachaTier.h"
#include "EmployeeGachaProbabilityWidget.generated.h"

class UCloseButtonWidget;
class UVerticalBox;
class UTextBlock;

/**
 * 가챠 확률표 모달 — 3티어 등급별 % + 천장 규칙.
 * 자체 딤 + 중앙 패널 + 닫기(LaunchConfirm 골격 클론).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeGachaProbabilityWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCloseButtonWidget* UIE_CloseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* NormalRows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* AdvancedRows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* PremiumRows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PityRuleText;

public:
	/** 현재 HR 파워(일반 탭 반영) 주입 후 표 빌드 */
	void SetupTable(int32 InHRPower);

private:
	int32 HRPower = 0;
	void BuildTierRows(UVerticalBox* Container, EGachaTier Tier);

	UFUNCTION()
	void OnCloseClicked();
};
