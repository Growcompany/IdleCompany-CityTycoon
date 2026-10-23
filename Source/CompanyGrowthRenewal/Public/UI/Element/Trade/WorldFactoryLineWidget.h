// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/CountryType.h"
#include "Enum/ResourceType.h"
#include "WorldFactoryLineWidget.generated.h"

class UImage;
class UBorder;
class UCommonTextBlock;
class UProgressBar;
class UWidgetSwitcher;
class UCostActionButtonWidget;
class UTextBlock;
class UTexture2D;

UENUM(BlueprintType)
enum class EWorldFactoryLineState : uint8
{
	Producing UMETA(DisplayName = "Producing"),
	Completed UMETA(DisplayName = "Completed"),
};

/**
 * UWorldFactoryLineWidget
 * 월드맵 공장 패널 안에 배치되는 배치 생산 라인 위젯 (UIE_FactoryLine).
 *
 * 상태 (BtnSwitcher 기반):
 *  - Producing (index 0): 진행바 + 남은 시간 + 다이아 즉시완료 버튼
 *  - Completed (index 1): 완료 표시 + 박스 수령 버튼
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldFactoryLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstantFinishRequested, UWorldFactoryLineWidget*, Line);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClaimRequested, UWorldFactoryLineWidget*, Line);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLineCompleted, UWorldFactoryLineWidget*, Line);

	UPROPERTY(BlueprintAssignable, Category = "WorldFactoryLine|Events")
	FOnInstantFinishRequested OnInstantFinishRequested;

	UPROPERTY(BlueprintAssignable, Category = "WorldFactoryLine|Events")
	FOnClaimRequested OnClaimRequested;

	UPROPERTY(BlueprintAssignable, Category = "WorldFactoryLine|Events")
	FOnLineCompleted OnLineCompleted;

	UFUNCTION(BlueprintCallable, Category = "WorldFactoryLine")
	void SetProductionData(UTexture2D* IconTexture, const FText& ProductName,
		float RatePerSecond, int64 CurrentQty, int64 TargetQty, const FTimespan& Remaining,
		double InLineElapsedSec);

	// 매니저 OnLineProgressed broadcast 마다 호출 — 위젯 자체 시계를 매니저 시계로 동기화.
	// PlayFab catchup 같은 큰 시간 점프 흡수용 (자체 NativeTick 누적은 평상시 부드러운 표시).
	UFUNCTION(BlueprintCallable, Category = "WorldFactoryLine")
	void UpdateProgress(int64 CurrentQty, const FTimespan& Remaining, double InLineElapsedSec);

	UFUNCTION(BlueprintCallable, Category = "WorldFactoryLine")
	void SetCompleted(int64 FinalQty);

	UFUNCTION(BlueprintPure, Category = "WorldFactoryLine")
	EWorldFactoryLineState GetState() const { return CurrentState; }

	// 매니저 연동용 라인 식별자 (매니저가 StartProduction 후 할당)
	UFUNCTION(BlueprintCallable, Category = "WorldFactoryLine")
	void SetLineId(int32 InLineId) { LineId = InLineId; }

	UFUNCTION(BlueprintPure, Category = "WorldFactoryLine")
	int32 GetLineId() const { return LineId; }

	// NativeTick polling 에서 매니저 GetLineState 호출용 — CountryDetailWidget 가 spawn 시점에 set.
	UFUNCTION(BlueprintCallable, Category = "WorldFactoryLine")
	void SetCountry(ECountryType InCountry) { CachedCountry = InCountry; }

	// 서랍 한 줄 행(UIE_FactoryLineRow)의 번호 칸. 번호 칸이 없는 카드형 WBP 에서는 무동작.
	void SetLineNumber(int32 InNumber);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TierBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LineNumberText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductionRateText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RemainingTimeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CurrentStatValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> BtnSwitcher;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCostActionButtonWidget> InstantFinishButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCostActionButtonWidget> RecieveButton;

	// 막대 안 가운데 `현재 / 목표개`
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> BarText;

	// 1행 오른쪽 상태 — `남은 4분 59초` / `완료`
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldFactoryLine|Visual")
	FLinearColor GaugeColorProducing = FLinearColor(0.263f, 0.788f, 0.369f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldFactoryLine|Visual")
	FLinearColor GaugeColorCompleted = FLinearColor(0.94f, 0.75f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldFactoryLine|Visual")
	FSlateColor StatusInk = FSlateColor(FLinearColor(FColor(0xA8, 0xB4, 0xC2)));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldFactoryLine|Visual")
	FSlateColor StatusDoneInk = FSlateColor(FLinearColor(FColor(0xFF, 0xD4, 0x5C)));

	// 잠긴 버튼(회색 판) 위 라벨. 열린 버튼은 버튼 인스턴스의 저작 잉크(BtnTextColor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldFactoryLine|Visual")
	FSlateColor DisabledLabelInk = FSlateColor(FLinearColor(FColor(0x1B, 0x23, 0x30)));

	virtual void NativePreConstruct() override;

private:
	bool bInstantFinishLocked = false;
	bool bReceiveLocked = false;
	void ApplyButtonLabelInks();
	// 1초마다 부르므로 안쪽 라벨은 한 번만 찾아 둔다
	void PaintButtonLabel(UCostActionButtonWidget* Button, TObjectPtr<UTextBlock>& LabelCache, bool bLocked);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InstantFinishLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReceiveLabel;

	EWorldFactoryLineState CurrentState = EWorldFactoryLineState::Producing;
	int64 CachedTargetQty = 0;
	int32 LineId = 0;
	ECountryType CachedCountry = ECountryType::None;

	float CachedRatePerSecond = 0.0f;
	// 자체 누적 — 매 NativeTick 마다 += InDeltaTime. 매니저 LineElapsedSec 와 같은 모델
	double CachedLineElapsedSec = 0.0;

	// NativeTick SetText 매 프레임 호출 회피용 — 정수 단위 변화만 갱신
	int64 LastDisplayedWholeCount = -1;
	int32 LastDisplayedRemainingSec = -1;
	bool bLastShownAsCompleted = false;

	// 매니저 polling 캐시 — broadcast 누락 / hidden->visible 안전망.
	int64 LastSeenManagerQty = -1;
	bool bLastSeenManagerCompleted = false;
	float InstantFinishRefreshElapsed = 0.f;
	void RefreshInstantFinishCost();

	void ApplyState();
	void SetQuantityTexts(int64 Current, int64 Target);
	void SetProgressBarPercent(float Percent01);
	// 막대 = 라인 전체 진행(완료 개수 + 지금 만드는 1개의 진행분)
	void SetLineProgress(int64 WholeCount, double UnitFraction01);
	void ApplyStatus(const FText& Status, bool bDone);
	// RecieveButton 박스에 수령할 제품 수 표시 (UpgradeBtnWidget 의 비용 슬롯 재활용).
	// Factory product 는 EResourceType::Box 로 표시 (제품 박스 아이콘).
	void RefreshRecieveButtonCount(int64 CurrentQty);

	void HandleInstantFinishClicked();
	void HandleRecieveClicked();

	static FString FormatDuration(const FTimespan& Duration);
};
