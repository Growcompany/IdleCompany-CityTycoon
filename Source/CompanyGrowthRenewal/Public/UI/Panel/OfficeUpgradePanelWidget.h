// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/OfficeExpansionType.h"
#include "Enum/ResourceType.h"
#include "OfficeUpgradePanelWidget.generated.h"

class UButton;
class UTextBlock;
class UUpgradeSlot;
class UCloseButtonWidget;

/**
 * 오피스 확장 하단 시트
 * - 좌/우 방향 확장, 다이아 결제
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeUpgradePanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	// ========== 공통 UI ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// 시트 바깥 풀스크린 투명 버튼 — 탭 = 닫기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// ========== 확장 UI ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* CurrentSizeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* MaxSizeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUpgradeSlot* LeftExpansionSlot;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUpgradeSlot* RightExpansionSlot;

private:
	// ========== 이벤트 핸들러 ==========

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnBackgroundClicked();

	void OnExpandLeftClicked();
	void OnExpandRightClicked();

	void OnResourceChanged(EResourceType Type, int64 NewValue);

	// ========== 갱신/결제 ==========

	void RefreshExpansionUI();

	// 방향 공통 결제 경로 — 잔액 확인 → 확장 → 차감(저장 포함)
	void TryExpand(EOfficeExpandDirection Direction);
};
