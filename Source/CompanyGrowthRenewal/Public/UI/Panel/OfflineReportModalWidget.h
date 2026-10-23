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

// 리포트가 어떤 경로로 닫히든 금고를 비우되, 같은 리포트에서 두 번 수거하지 않는다.
inline bool ShouldCollectOfflineVault(bool bAlreadyCollected)
{
	return !bAlreadyCollected;
}

/**
 * 오프라인 정산 모달 — 복귀 시 1회. 금고 적립분을 보여주고, 금고 초과 손실을 강화 유도로 번역한다.
 *
 * 데이터는 USaveLoadManager::OnOfflineGainsDetailed (ConsumePendingOfflineReport 에서 1회 발화) 를
 * 받아 SetReportData 로 주입. 푸터 버튼은 닫기가 아니라 [모두 수령] — 자동수거는 EndOperation
 * 시점에만 걸려서 복귀 시점엔 금고에 그대로 남기 때문. 단 어느 경로로 닫아도 금고는 비워진다
 * (MainMap 에 수거 UI 가 없어 안 비우면 잔액이 갇힌다) — [모두 수령] 만 연출을 동반한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfflineReportModalWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void SetReportData(float InTotalGained, float InOfflineSeconds,
		const TArray<FOfflineGainEntry>& InEntries, bool bInCapReached, int64 InBrickGained);

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

	// 벽돌 지급 줄 — 건물별 금고(RowBox)와 다른 축(자원)이라 목록에 섞지 않고 별도 줄로 둔다.
	// 벽돌은 공장이 정산 때 이미 인벤토리로 넣었다 — 이 줄은 보고이지 수령 버튼이 아니다.
	// 0 이면 접는다. WBP 미배선 시 안내만 사라지고 지급은 정상이라 Optional.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* BrickRow = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* BrickAmountText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCloseButtonWidget* CloseButton = nullptr;

	// 스택이 딤을 안 깔아주므로 WBP 자체 딤 — 클릭 시 닫기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn = nullptr;

private:
	void HandleCollectAllClicked();
	void HandleCloseClicked();

	// 수거 UI 가 MainMap 에서 사라져, 어떤 경로로 닫혀도 금고를 비운다 (NativeDestruct 에서, 코인·토스트 없이)
	void CollectVaultSilentlyIfNeeded();

	UFUNCTION()
	void HandleBackgroundClicked();

	UFUNCTION()
	void HandleCloseButtonClicked();

	void ApplyTotalText(float Value);

	// 같은 리포트에서 [전체 수거] 와 닫기(deactivate) 가 연달아 불린다. 풀 재사용 인스턴스라 리포트마다 리셋이 필요하다.
	bool bVaultCollectedThisReport = false;

	float TotalGained = 0.0f;
	float OfflineSeconds = 0.0f;
	bool bCapReached = false;

	// 히어로 카운트업 — 적립 0 이면 비활성(0원을 축하색으로 칠하면 거짓)
	float CountUpElapsed = 0.0f;
	bool bCountUpActive = false;
};
