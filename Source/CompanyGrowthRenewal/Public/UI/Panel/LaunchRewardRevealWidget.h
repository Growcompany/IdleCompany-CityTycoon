#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Table/MissionTable.h"
#include "GameplayTagContainer.h"
#include "LaunchRewardRevealWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UBorder;
class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UProgressBar;
class UButtonWidget;
class UItemCardSlotWidget;
class USizeBox;

// OfficeMainWidget 이 StartLaunch 직후 조립해 Setup 으로 넘긴다. Loot 은 이미 지급됨(표시 전용).
USTRUCT(BlueprintType)
struct FLaunchRewardRevealData
{
	GENERATED_BODY()

	UPROPERTY() FText ProjectName;
	UPROPERTY() int32 ReviewScore = 0;
	// Low / Mid / High = ULaunchLootManagerSubsystem::ReviewScoreToTableKey
	UPROPERTY() FName BandKey;
	UPROPERTY() TArray<FMissionReward> Loot;
	UPROPERTY() bool bFirstDiscovery = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaunchRewardRevealClosed);

/**
 * 출시 보상 리빌 — C안 "스탬프 런" (specs/2026-08-22 §3).
 * 평점 카운트업 → 판정 도장 → 빈 슬롯 N 선공개 → 카드 가속 연타 → 첫 발견 도장 → [확인].
 * StageTime 하나에서 전 시각 상태를 유도(stateless) — 탭 = 종료 시각으로 점프.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULaunchRewardRevealWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void Setup(const FLaunchRewardRevealData& InData);

	UPROPERTY(BlueprintAssignable, Category = "Launch Reveal|Events")
	FOnLaunchRewardRevealClosed OnRevealClosed;

	// 미션 가이드 하이라이트 타겟
	UWidget* GetConfirmButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDeactivated() override;

	// ===== BindWidget (required — 데이터 주입 대상은 loud fail) =====
	UPROPERTY(meta = (BindWidget)) UImage* DimBG;
	UPROPERTY(meta = (BindWidget)) UVerticalBox* HeaderBox;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* EyebrowText;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* ProjectNameText;
	UPROPERTY(meta = (BindWidget)) UBorder* StubPlate;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* ScoreText;
	UPROPERTY(meta = (BindWidget)) UProgressBar* ScoreRail;
	UPROPERTY(meta = (BindWidget)) UOverlay* StampBox;
	UPROPERTY(meta = (BindWidget)) UBorder* StampFrame;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* StampText;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* CountText;
	UPROPERTY(meta = (BindWidget)) UHorizontalBox* SlotRow;
	UPROPERTY(meta = (BindWidget)) UBorder* DiscoveryBadge;
	UPROPERTY(meta = (BindWidget)) UButtonWidget* ConfirmButton;

	// ===== Optional FX (없으면 해당 FX 만 생략, 로그 1줄) =====
	UPROPERTY(meta = (BindWidgetOptional)) UImage* FlashOverlay;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* InkRing;

	// ===== 튜닝 노브 (WBP Class Defaults 가 튜닝 표면 — 재빌드 금지) =====
	UPROPERTY(EditAnywhere, Category = "Reveal") float Stagger = 0.12f;
	UPROPERTY(EditAnywhere, Category = "Reveal") float StaggerDecay = 0.9f;
	UPROPERTY(EditAnywhere, Category = "Reveal") float StaggerMin = 0.10f;
	UPROPERTY(EditAnywhere, Category = "Reveal") float StampShakeAmp = 16.f;
	UPROPERTY(EditAnywhere, Category = "Reveal") float CardShakeAmp = 7.f;
	UPROPERTY(EditAnywhere, Category = "Reveal") float DimAlpha = 0.74f;
	UPROPERTY(EditAnywhere, Category = "Reveal") float CardSize = 240.f;
	UPROPERTY(EditAnywhere, Category = "Reveal") float ConfirmDelay = 0.5f;

private:
	// 카드 1장의 런타임 FX 묶음 — 전부 C++ 생성 (Wrap = Overlay[(Glow), SizeBox, Slot, Card, Flash, Ring, Sparkle×3])
	struct FRevealCard
	{
		TWeakObjectPtr<UOverlay> Wrap;
		TWeakObjectPtr<UBorder> Silhouette;
		TWeakObjectPtr<UItemCardSlotWidget> Card;
		TWeakObjectPtr<UImage> Flash;
		TWeakObjectPtr<UImage> Ring;
		TWeakObjectPtr<UImage> Glow;   // 최고 카드 배후광(GachaCardGlow) — 최고 카드 1장에만 생성
		TArray<TWeakObjectPtr<UImage>> Sparkles;
		float StartAt = 0.f;
		bool bSoundFired = false;
		bool bBest = false;
	};

	void BuildCards(const TArray<FMissionReward>& Ordered);
	void ClearCards();
	void ApplyBandStyle();
	void RenderAt(float T);
	void PlayKey(FName Key, float Volume = 1.f, float Pitch = 1.f);
	void PlayUITag(const FGameplayTag& Tag);
	UFUNCTION() void HandleConfirmClicked();

	FLaunchRewardRevealData Data;
	TArray<FRevealCard> Cards;
	TArray<float> CardTimes;
	FLinearColor BandColor = FLinearColor::White;

	float StageTime = 0.f;
	float EndTime = 0.f;
	float BadgeAt = 0.f;
	float ConfirmAt = 0.f;
	bool bSetupDone = false;
	// 다른 프롬프트가 위에 덮이면 CommonUI 가 deactivate 를 부른다 — 닫힘(확인)과 구분
	bool bClosing = false;
	bool bConfirmEnabled = false;
	bool bScoreSoundFired = false;
	bool bStampSoundFired = false;
	bool bSlotsSoundFired = false;
	bool bBestSoundFired = false;
	bool bBadgeSoundFired = false;
};
