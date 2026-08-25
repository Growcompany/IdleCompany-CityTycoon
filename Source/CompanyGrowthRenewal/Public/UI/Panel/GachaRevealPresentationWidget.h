#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/BuildingSkinGachaSaveData.h"
#include "Enum/LootBoxRarity.h"
#include "GachaRevealPresentationWidget.generated.h"

class UTextBlock;
class UButtonWidget;
class UCloseButtonWidget;
class UNamedSlot;
class UHorizontalBox;
class UVerticalBox;
class UCanvasPanel;
class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UUserWidget;
class UItemCardWidget;
class UBuildingSkinCardWidget;
class UCommonButtonStyle;
class UItemTooltipWidget;
class UBuildingTraitManagerSubsystem;
class UBuildingSkinManagerSubsystem;
struct FGameplayTag;

// 수렴 스파크 1개의 고정 파라미터 (BeginStaging 랜덤 생성, 반지름은 화면 높이 비율)
struct FGachaSparkDot
{
	FVector2D Dir = FVector2D::ZeroVector;
	float RadiusFrac = 0.6f;
	float Delay = 0.f;
};

/**
 * 가챠 2D 뽑기 연출 — 특성/스킨 공용 단일 위젯 (단일 WBP).
 * 풀 스테이징: 빌드업(등급색 글로우 수렴)→플래시(쇼크웨이브)→공개(펀치+스파클+링팝+아웃라인섬광)→여운(배후광/빛기둥/블룸), 버튼 지연 등장.
 * 모든 시각 상태는 StageTime 하나에서 stateless 유도 — 탭 스킵 = 시간 점프.
 * SOT: docs/superpowers/specs/2026-07-10-gacha-reveal-staging-design.md (승인 목업 GachaReveal_Staging_MOCKUP.html)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGachaRevealPresentationWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UGachaRevealPresentationWidget(const FObjectInitializer& ObjectInitializer);

	/** 특성 1x/10x 결과 (이미 grant 됨). */
	void SetupPullTrait(const TArray<FBuildingTraitGachaResult>& Results, bool bInAdvanced);

	/** 스킨 1x/10x 결과 (이미 grant 됨). */
	void SetupPullSkin(const TArray<FBuildingSkinGachaResult>& Results, bool bInAdvanced);

	// 튜토리얼 M11/M12 — 연출 [확인] 버튼 하이라이트 타겟
	class UWidget* GetConfirmButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 카드(들)가 들어가는 공통 슬롯 — C++가 런타임에 가로 row 를 SetContent
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UNamedSlot* CardSlot;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* ResultRarityText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UButtonWidget* ConfirmButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UCloseButtonWidget* UIE_CloseButton;

	// 풀 스테이징 FX (전부 Optional — 없으면 해당 연출만 생략, 텍스처는 DT_UIVFXTexture 주입)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* DimBG;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* BuildupGlow;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* FlashOverlay;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* ShockwaveRing;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* ShockwaveRing2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* CardBackGlow;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UCanvasPanel* FXLayer;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UVerticalBox* CenterVBox;

	// 등급 → 스파클 텍스처 / 트레이트 카드 Style — 생성자 자동 매핑
	UPROPERTY() TMap<ELootBoxRarity, TObjectPtr<UTexture2D>> RaritySparkleMap;
	UPROPERTY() TMap<ELootBoxRarity, TSubclassOf<UCommonButtonStyle>> RarityStyleMap;
	UPROPERTY() TSubclassOf<UItemCardWidget> TraitCardClass;
	UPROPERTY() TSubclassOf<UBuildingSkinCardWidget> SkinCardClass;

	UFUNCTION() void OnConfirmClicked();
	UFUNCTION() void OnCloseClicked();

	// 닫기 단일 경로 — DeactivateWidget(라우터 스택 이탈) 후 RemoveFromParent.
	void CloseSelf();

private:
	// 공통 reveal: 카드들+등급을 CardSlot 의 가로 row 로 채우고 스테이징 시작.
	void BeginReveal(const TArray<UUserWidget*>& Cards, const TArray<ELootBoxRarity>& Rarities);

	UUserWidget* MakeTraitCard(const FBuildingTraitGachaResult& R);
	UUserWidget* MakeSkinCard(const FBuildingSkinGachaResult& R);

	// 결과 카드 클릭 → 인라인 툴팁(이름/설명/아이콘). 인스턴스 1개 재사용 (UResourceWidget 패턴).
	void ShowCardTooltip(const FText& Name, const FText& Desc, UTexture2D* Icon);

	// ===== 페이즈 스테이징 =====
	void BeginStaging();
	void UpdateStaging(float DeltaTime);
	void SkipToSettle();
	void ResetStagingVisuals();
	void RebuildFXPools();
	void PlayUITag(const FGameplayTag& Tag);
	FGameplayTag StingTagForTier(int32 Tier) const;
	static int32 TierOf(ELootBoxRarity R);
	static float BuildMulOf(ELootBoxRarity R);

	// ===== 기존 카드 스파클 =====
	void StartCardSparkle(int32 Index);
	void ApplySparkleBrush(UImage* Img, UMaterialInstanceDynamic*& MID, ELootBoxRarity Rarity);
	void ApplySparkleFrame(UImage* Img, float T, float SizeMul);
	static float RaritySizeMul(ELootBoxRarity R);

	// 상태
	bool bIsSkin = false;
	bool bAdvanced = false;

	// 카드/스파클
	TArray<ELootBoxRarity> RevealRarities;
	UPROPERTY() TArray<TObjectPtr<UUserWidget>> RevealCards;
	UPROPERTY() TArray<TObjectPtr<UImage>> RevealSparkles;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> RevealSparkleMIDs;
	TArray<float> RevealSparkleElapsed;
	TArray<bool> CardRevealedFlags;
	UPROPERTY() TArray<TObjectPtr<UImage>> CardGlintImages;

	// 스테이징 상태 (전부 StageTime 기준 stateless)
	bool bStagingActive = false;
	bool bReduceMotion = false;
	bool bFlashFired = false;
	bool bButtonsShown = false;
	float StageTime = 0.f;
	ELootBoxRarity BestRarity = ELootBoxRarity::Common;
	FLinearColor BestColor = FLinearColor::White;
	float BuildDuration = 0.9f;
	float RevealStartTime = 0.f;
	float SettleTime = 0.f;
	float ButtonsTime = 0.f;

	// 수렴 스파크 + 빌드업 링/텍스트 라인 (FXLayer 캔버스에 런타임 생성, 꼬리 인덱스 계약)
	TArray<FGachaSparkDot> SparkParams;
	UPROPERTY() TArray<TObjectPtr<UImage>> SparkImages;

	UPROPERTY() TObjectPtr<UTexture2D> SparkDotTexture;

	UPROPERTY() UMaterialInterface* SparkleMaterial = nullptr;
	UPROPERTY() UBuildingTraitManagerSubsystem* TraitManager = nullptr;
	UPROPERTY() UBuildingSkinManagerSubsystem* SkinManager = nullptr;

	UPROPERTY(Transient) TObjectPtr<UItemTooltipWidget> ActiveTooltip;
};
