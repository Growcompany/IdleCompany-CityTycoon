#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/LootBoxRarity.h"
#include "PotentialTierUpOverlayWidget.generated.h"

class UImage;
class UCommonTextBlock;

/**
 * 잠재능력 등급이 오른 재설정 직후 화면 전체에 한 번 뜨는 연출.
 * 금빛 칼끝이 가운데로 꽂히고 등급색 띠 위에 「이전 등급 → 새 등급」을 보여 준 뒤 스스로 닫힌다. 화면을 누르면 바로 걷힌다.
 * 이미지·크기·글꼴은 WBP(UI_PotentialTierUpOverlay)가 소유하고, 이 클래스는 등급색 주입과 시간에 따른 변형만 맡는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPotentialTierUpOverlayWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Play(ELootBoxRarity From, ELootBoxRarity To, bool bPlaySound);
	void Skip();

	// 걷히기 시작(자연 종료·스킵 공통) — 잠재 판의 줄 다시 뽑기가 이 순간 시작한다
	FSimpleDelegate OnRelease;

#if !UE_BUILD_SHIPPING
	// 캡처 하네스 전용 — Time 초 시점의 모습으로 세운다
	void DebugSeek(float Time);
	bool DebugIsLive(FString& OutDetail) const;
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget)) UImage* DimImage;
	UPROPERTY(meta = (BindWidget)) UWidget* BandRoot;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* TitleText;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* FromText;
	UPROPERTY(meta = (BindWidget)) UCommonTextBlock* ToText;
	UPROPERTY(meta = (BindWidget)) UWidget* FxFront;
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* FromChip;
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* ArrowText;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* BandSheen;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* CoreGlow;

private:
	float Elapsed = -1.f;
	float FadeAt = 0.f;
	float Intensity = 1.f;
	// 띠 면의 등급색 — 착지 순간 흰빛에서 이 색으로 가라앉는다
	FLinearColor BandDeep = FLinearColor::White;
	ELootBoxRarity ToRarity = ELootBoxRarity::Common;
	bool bReleased = false;
	bool bHitPlayed = false;
	bool bPlaySoundOnHit = false;
	bool bFrozen = false;

	// 이름 계약: TierUpThrust00..(가운데로 꽂힘), TierUpBurst00..(튀어 나감), TierUpStar00..
	// 띠 면(BandFillL·C·R = 양끝 흰 그라데이션 + 가운데 흰 단색)과 띠 선(BandLine*)을 등급색으로 물들인다
	TArray<TWeakObjectPtr<UImage>> ThrustBlades;
	TArray<TWeakObjectPtr<UImage>> BurstBlades;
	TArray<TWeakObjectPtr<UImage>> Stars;
	TArray<TWeakObjectPtr<UImage>> BandFills;
	TArray<TWeakObjectPtr<UImage>> BandLines;

	void CollectLayers();
	// 시각 Time 의 모습을 그린다 — 실시간 틱과 하네스 시크가 같은 함수를 탄다. true = 끝
	bool Apply(float Time);
	void Release();
	void Finish();
};
