#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "HQLevelUpCelebrationWidget.generated.h"

class UButton;
class UImage;
class UOverlay;
class UCommonTextBlock;

/**
 * 본사 레벨업 축하 전체화면 오버레이
 * 골드 메달 히어로 + Ray 회전 + 순차 등장 → 터치 시 닫힘
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQLevelUpCelebrationWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	void SetLevelUpInfo(int32 InOldLevel, int32 InNewLevel, const FText& InRewardText);

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* RayEffectImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* FlashOverlay;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UOverlay* TitleContainer = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelUpTitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OldLevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* NewLevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RewardDescText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* TouchToCloseText;

	// ===== 메달 히어로 (2026-07-20 리디자인 B안) =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* MedalLevelText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* MedalBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LevelRow = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* RewardPlate = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* SparkleA = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* SparkleB = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* SparkleC = nullptr;

	// ===== VFX 레이어 (WBP에 넣은 것만 동작) =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* GlowImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* GlowSmallImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* RaySubImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* SparkelsImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* ShineImage = nullptr;

private:
	UFUNCTION()
	void OnBackgroundClicked();

	// 전달받은 레벨 정보
	int32 CachedOldLevel = 1;
	int32 CachedNewLevel = 2;
	FText CachedRewardText;

	// 회전 상태
	float RayRotation = 0.f;
	float RaySubRotation = 0.f;
	float SparkelsRotation = 0.f;

	// Shine 목적지 X (메달/Ray 중심 기준, ShineImage 로컬 좌표)
	float ShineTargetX = 0.f;
	bool bShineTargetCalculated = false;

	// 등장 연출 타이머
	float IntroTimer = 0.f;
	bool bIntroComplete = false;

	// 메달+새 레벨 강조 펀치 (0.6s 1회)
	FScalePunchAnimation MedalPunchAnim;
	bool bMedalPunchFired = false;

	// 스파클 팝 (StartTime부터 0.5s, scale 0→1.15→0)
	void DriveSparklePop(UImage* Sparkle, float StartTime) const;

	// 위젯 Opacity 보간 헬퍼
	static void LerpOpacity(UWidget* Widget, float Alpha);
};
