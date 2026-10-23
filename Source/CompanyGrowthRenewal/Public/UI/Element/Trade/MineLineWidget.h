// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/CountryType.h"
#include "Enum/ResourceType.h"
#include "Manager/MineManager.h"
#include "MineLineWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UProgressBar;
class UWidgetSwitcher;
class UCostActionButtonWidget;
class UTextBlock;
class UTexture2D;

/**
 * 라인의 실제 채광 상태. 한도 도달 시 수령, 진행 중에는 즉시 완료를 노출한다.
 *  - Producing: 한도 미도달.
 *  - Saturated: 한도 도달.
 */
UENUM(BlueprintType)
enum class EMineLineState : uint8
{
	Producing UMETA(DisplayName = "Producing"),
	Saturated UMETA(DisplayName = "Saturated"),
};

/**
 * UMineLineWidget — UIE_MineLine.
 * WorldFactoryLine 패턴 차용: NativeTick 에서 매니저 채광량으로 ProgressBar 와 수량을 갱신.
 * 매니저 broadcast 는 `UpdateStorage`/`UpdateRate` 로 elapsed 재동기화 용 (catchup, claim, instantfinish).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMineLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMineClaimRequested, UMineLineWidget*, Line);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMineInstantFinishRequested, UMineLineWidget*, Line);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMineQuantityChanged, UMineLineWidget*, Line);

	UPROPERTY(BlueprintAssignable, Category = "MineLine|Events")
	FOnMineClaimRequested OnClaimRequested;

	UPROPERTY(BlueprintAssignable, Category = "MineLine|Events")
	FOnMineInstantFinishRequested OnInstantFinishRequested;

	UPROPERTY(BlueprintAssignable, Category = "MineLine|Events")
	FOnMineQuantityChanged OnQuantityChanged;

	// 초기 1회 세팅. RatePerMinute 분당 채광 속도 — 내부에서 per-sec 변환.
	UFUNCTION(BlueprintCallable, Category = "MineLine")
	void SetLineState(ECountryType Country, const FMineLineState& State, int64 TargetQty,
		int32 RateLevel, float EffectiveRatePerMinute);

	// 매니저 OnLineStorageChanged/OnLineSaturated 응답 — elapsed 를 CurrentQty 기준으로 재싱크.
	UFUNCTION(BlueprintCallable, Category = "MineLine")
	void UpdateStorage(int64 CurrentQty, int64 TargetQty, bool bSaturated);

	// 매니저 OnUpgradeChanged 응답 (rate 변경 시 elapsed 재계산)
	UFUNCTION(BlueprintCallable, Category = "MineLine")
	void UpdateRate(int32 RateLevel, float EffectiveRatePerMinute);

	UFUNCTION(BlueprintPure, Category = "MineLine")
	EResourceType GetResource() const { return CachedResource; }

	void SetLineNumber(int32 InNumber);

	UFUNCTION(BlueprintPure, Category = "MineLine")
	EMineLineState GetState() const
	{
		return bCachedSaturated ? EMineLineState::Saturated : EMineLineState::Producing;
	}

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ── BindWidget — UIE_MineLine 변수와 매칭 ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductText;

	// 그림 칸 왼쪽 위 라인 번호 원(공장 줄과 같은 문법)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LineNumberText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CurrentStatValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RemainingTimeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductionRateText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> BtnSwitcher;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCostActionButtonWidget> InstantFinishButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCostActionButtonWidget> RecieveButton;

	// 막대 안 가운데 `현재 / 한도개`
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> BarText;

	// 1행 오른쪽 상태 — `가득까지 1분 12초` / `가득 참`
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineLine|Visual")
	FLinearColor GaugeColorNormal = FLinearColor(0.263f, 0.788f, 0.369f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineLine|Visual")
	FLinearColor GaugeColorSaturated = FLinearColor(0.94f, 0.75f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineLine|Visual")
	FSlateColor StatusInk = FSlateColor(FLinearColor(FColor(0xA8, 0xB4, 0xC2)));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineLine|Visual")
	FSlateColor StatusDoneInk = FSlateColor(FLinearColor(FColor(0xFF, 0xD4, 0x5C)));

	// 잠긴 버튼(회색 판) 위 라벨. 열린 버튼은 버튼 인스턴스의 저작 잉크(BtnTextColor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineLine|Visual")
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

	ECountryType CachedCountry = ECountryType::None;
	EResourceType CachedResource = EResourceType::None;

	// ── self-clocking 상태 ──
	int64 CachedTargetQty = 0;          // 라인 한도 (effective max)
	float CachedRatePerSecond = 0.0f;   // 강화 multiplier 적용된 per-sec rate
	double CachedLineElapsedSec = 0.0;  // 현재 사이클의 누적 시간 — frame 마다 += DeltaTime
	int32 CachedRateLevel = 0;
	bool bCachedSaturated = false;

	// 매 frame SetText 호출 회피 — 정수 변경 시에만 갱신
	int64 LastDisplayedWholeCount = -1;
	int32 LastDisplayedRemainingSec = -1;
	bool bLastShownAsCompleted = false;

	// 매니저 polling 결과 캐시. NativeTick 진입 시 매니저 CurrentQty 가 이 값과 다르면
	// 위젯이 hidden 상태에서 매니저가 진행한 것 → elapsed 재싱크.
	int64 LastSeenManagerQty = -1;
	float InstantFinishRefreshElapsed = 0.f;
	void RefreshInstantFinishCost();

	void ApplyState();              // 한도 도달 여부로 BtnSwitcher 인덱스 결정
	// 상태 글자 + 막대 채움색 — 가득 참 = 금색, 채광 중 = 초록
	void ApplyStatus(const FText& Status, bool bFull);
	void ApplyProducingStatus(double ExpectedTotal);
	// RecieveButton 박스에 수령할 자원 수 표시 (UpgradeBtnWidget 의 비용 슬롯 재활용).
	void RefreshRecieveButtonCount(int64 CurrentQty);
	void ApplyResourceMeta(EResourceType Resource);
	void ApplyLevelText();
	void ApplyRateText();           // ProductionRateText (분당) — 정적 표시

	void SetQuantityTexts(int64 Current, int64 Target);
	void SetProgressBarPercent(float Percent01);
	void SetLineProgress(int64 WholeCount, double UnitFraction01);
	void SetRemainingTimeText(double RemainingSec);

	// elapsed 재계산 헬퍼 — CurrentQty 기준으로 cycle 시작점에 맞춤.
	void SyncElapsedToQuantity(int64 CurrentQty);

	UFUNCTION()
	void HandleClaimClicked();

	UFUNCTION()
	void HandleInstantFinishClicked();
};
