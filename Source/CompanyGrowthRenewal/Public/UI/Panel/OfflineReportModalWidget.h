// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Manager/SaveLoadManager.h"
#include "OfflineReportModalWidget.generated.h"

class UButton;
class UButtonWidget;
class UCloseButtonWidget;
class UCommonTextBlock;
class UImage;
class UVerticalBox;

/**
 * 오프라인 정산 모달 — 복귀 시 1회. 금고 적립분을 보여주고, 금고 초과 손실을 강화 유도로 번역한다.
 *
 * 데이터는 USaveLoadManager::OnOfflineGainsDetailed (ConsumePendingOfflineReport 에서 1회 발화) 를
 * 받아 SetReportData 로 주입. 푸터 버튼은 닫기가 아니라 [모두 수령] — 자동수거는 EndOperation
 * 시점에만 걸려서 복귀 시점엔 금고에 그대로 남기 때문(수동 수거 강제 = 순수 마찰).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfflineReportModalWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void SetReportData(float InTotalGained, float InOfflineSeconds,
		const TArray<FOfflineGainEntry>& InEntries, bool bInCapReached);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* TimeAwayText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* TotalGainText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* HeroCaptionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* RowBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* CollectAllButton;

	// 손실 0 이면 밴드째 접는다 (버튼을 품고 있어 Visible/Collapsed 로만 토글)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LossBand = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LossBandAmountText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* CapNote = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* MoneyIcon = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCloseButtonWidget* CloseButton = nullptr;

	// 스택이 딤을 안 깔아주므로 WBP 자체 딤 — 클릭 시 닫기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn = nullptr;

private:
	void HandleCollectAllClicked();
	void HandleCloseClicked();

	UFUNCTION()
	void HandleBackgroundClicked();

	UFUNCTION()
	void HandleCloseButtonClicked();

	void ApplyTotalText(float Value);

	float TotalGained = 0.0f;
	float OfflineSeconds = 0.0f;
	bool bCapReached = false;

	// 히어로 카운트업 — 적립 0 이면 비활성(0원을 축하색으로 칠하면 거짓)
	float CountUpElapsed = 0.0f;
	bool bCountUpActive = false;
};
