// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/ResourceType.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "CostActionButtonWidget.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class UResourceWidget;
class UCommonButtonBase;
class UCommonButtonStyle;

/** 비용 표시 + 활성/비활성 테두리 색을 갖는 업그레이드 버튼. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCostActionButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static const FLinearColor ColorEnabled;   // #FFE06CFF
	static const FLinearColor ColorDisabled;  // #4A4A4AFF

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn")
	FText ButtonText = NSLOCTEXT("UpgradeBtnWidget", "DefaultButtonText", "강화");

	// 최대 레벨 도달 시 버튼 라벨 (비용 행은 Collapsed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn")
	FText MaxLevelText = NSLOCTEXT("UpgradeBtnWidget", "MaxLevelText", "MAX");

	// 자금 부족 → 충족 전환 프레임 펀치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Feedback")
	float AffordPunchScale = 1.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Feedback")
	float AffordPunchDuration = 0.2f;

	// 부족 상태 탭 피드백(셰이크/사운드/토스트) 공통 쿨다운 — 홀드 연타 스팸 차단
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Feedback")
	float RejectFeedbackCooldown = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Cost")
	EResourceType DefaultResourceType = EResourceType::Money;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Cost")
	int64 DefaultCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Style")
	TSubclassOf<UCommonButtonStyle> ButtonStyleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Text")
	bool bOverrideTextStyle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Text", meta = (EditCondition = "bOverrideTextStyle"))
	FSlateFontInfo BtnTextFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Text", meta = (EditCondition = "bOverrideTextStyle"))
	FSlateColor BtnTextColor = FSlateColor(FLinearColor::White);

	// Overlay/HorizontalBox/VerticalBox 슬롯에서만 적용 (Canvas 슬롯은 무시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UpgradeBtn|Text", meta = (EditCondition = "bOverrideTextStyle"))
	FMargin BtnTextPadding = FMargin(0.f);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 구형 WBP 호환 (신형 WBP는 Btn 사용)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* UpgradeButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonButtonBase* Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* Border_Light;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* BtnText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UUserWidget* CostWidget;

public:
	// 외부 소유자가 "이 버튼은 쓸 수 있다"고 명시 선언하는 축.
	// true 로 부르면 비용 게이트(부족 시 입력 삼킴)를 끈다 — 비용이 아닌 값을 Cost 로 넘기는
	// 상태 슬롯(공장 재고 수령 등)이 자금 부족으로 잠기는 것을 막는다.
	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	void SetEnabled(bool bEnabled);

	// 잠금 축 전용 — 비용 게이트 오버라이드를 건드리지 않는다 (UpgradeSlot 내부용)
	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	void SetLockedState(bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	void SetButtonText(const FText& Text);

	// bCheckAfford=true면 재화 잔량을 체크해서 자동 활성/비활성
	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	void SetCost(int64 Cost, EResourceType ResourceType, bool bCheckAfford = true);

	// 구매 가능 여부 상태 세터. 잠금(SetEnabled)과 분리된 축 — 부족해도 버튼은 살아 있고,
	// 탭하면 왜 못 사는지 안내한다.
	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	void SetCanAfford(bool bCanAfford);

	// 최대 레벨 표면 — 비용 행 Collapsed + 무채 MAX 라벨. 버튼 활성/비활성은 호출부(UpgradeSlot) 소관
	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	void SetMaxLevelState(bool bInIsMaxLevel);

	// 홀드 중 억제 — 초당 N회 펀치 노이즈 차단. 홀드 시작/종료에서 쌍으로 토글할 것
	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	void SetPunchSuppressed(bool bInSuppressed);

	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	bool CanAfford() const { return bLastCanAfford; }

	UFUNCTION(BlueprintCallable, Category = "UpgradeBtn")
	UButton* GetButton() const { return UpgradeButton; }

	DECLARE_EVENT(UCostActionButtonWidget, FCostActionButtonEvent);
	FCostActionButtonEvent& OnClicked() const { return OnClickedEvent; }
	FCostActionButtonEvent& OnPressed() const { return OnPressedEvent; }
	FCostActionButtonEvent& OnReleased() const { return OnReleasedEvent; }

private:
	mutable FCostActionButtonEvent OnClickedEvent;
	mutable FCostActionButtonEvent OnPressedEvent;
	mutable FCostActionButtonEvent OnReleasedEvent;

	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonPressed();

	UFUNCTION()
	void HandleButtonReleased();

	bool bIsEnabled = true;

	EResourceType CurrentResourceType = EResourceType::Money;
	int64 CurrentCost = 0;

	bool bLastCanAfford = true;
	bool bAffordInitialized = false;
	bool bPunchSuppressed = false;
	bool bIsMaxLevel = false;
	bool bExplicitEnableOverride = false;

	float LastRejectFeedbackTime = -1000.f;

	// 마지막으로 실제 렌더(Tick)된 시각. 오래 안 그려졌다 갱신되면 = 패널 오픈 프레임 → silent init
	float LastTickTimeSeconds = -1000.f;

	FScalePunchAnimation AffordPunch;

	// CostWidget 의 Cast 결과 캐시 (Tick 이 아니라 SetCost/PreConstruct 시점에 1회)
	UPROPERTY(Transient)
	UResourceWidget* CachedCostWidget = nullptr;

	void CacheCostWidget();
	void ApplyEnabled(bool bEnabled);
	void UpdateBorderColor();

	// 부족 상태에서 눌렀을 때: 셰이크 + Disabled 사운드 + 부족액 토스트 (쿨다운 공유)
	void PlayInsufficientFeedback();

	// 잠금/MAX 가 아닌데 자금만 부족한 상태 = 입력을 삼키고 안내만 한다
	bool ShouldRejectInput() const;
};
