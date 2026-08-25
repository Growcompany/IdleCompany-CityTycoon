#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/GachaTier.h"
#include "Enum/LootBoxRarity.h"
#include "EmployeeGachaPresentationWidget.generated.h"

class UTextBlock;
class UImage;
class UCanvasPanel;
class UButtonWidget;
class UCloseButtonWidget;
class URecruitmentManagerSubsystem;
class AGachaCaptureStage;
class AWorkstationActorBase;
class UTexture2D;
class UEmployeeIdCardMiniWidget;
enum class ECompanyType : uint8;
struct FGameplayTag;

// 빛가루 분수 입자 1개의 고정 파라미터 (RebuildDustPool 랜덤 생성)
struct FEmpGachaDustParam
{
	float Angle = 0.f;   // 발사각(rad)
	float Delay = 0.f;
	float Life = 1.f;
	float Speed = 600.f; // 초속(px/s)
	float Size = 20.f;
};

/**
 * 직원 가챠 리빌 — 사원증 발급 풀 스테이징.
 * 빌드업(슬롯 등급광+카드 티징) → 플래시(사출) → 공개(사원증 착지 펀치, 증명사진=SceneCapture RT 댄스) → 여운(도장/등급 이펙트).
 * 모든 시각 상태는 StageTime 하나에서 stateless 유도 — 탭 스킵 = 시간 점프.
 * SOT: docs/superpowers/specs/2026-07-11-employee-gacha-idcard-reveal-design.md (승인 목업 EmployeeGachaReveal_IDCard_MOCKUP.html)
 * 결과는 [확인]/[닫기]/파괴 시 ConfirmGachaHire 로 풀에 적립(1회). 닫혀도 유실 방지.
 * 멀티(2~5건)는 SetupPullBatch — 센터=베스트로 같은 세리머니를 태우고, 도장 후 나머지를 좌우로 딜링,
 * 적립은 ConfirmGachaHireBatch 로 전원 일괄(부분 채용 금지).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeGachaPresentationWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UEmployeeGachaPresentationWidget(const FObjectInitializer& ObjectInitializer);

	/** 패널이 뽑기 결과로 호출 (결과는 아직 풀에 미적립). */
	void SetupPull(const FGachaResultData& Result, EGachaTier Tier, int32 BuildingIndex);

	/** 멀티 뽑기 결과 (2건+). 1건이면 내부에서 SetupPull 위임. */
	void SetupPullBatch(const TArray<FGachaResultData>& Results, EGachaTier Tier, int32 BuildingIndex);

	// 자동 착석 1순위 책상 (채용 진입 책상). SetupPull 전에 설정.
	void SetPreferredWorkstation(AWorkstationActorBase* Workstation);

	// M4 미션 가이드 — [확인] 버튼 하이라이트 타겟
	UWidget* GetConfirmButtonWidget() const;

	// 가챠 캡처 무대 BP (BP_GachaCaptureStage). 미지정 시 캐릭터 캡처 스킵(빈 증명사진 플레이트).
	UPROPERTY(EditDefaultsOnly, Category = "Gacha")
	TSubclassOf<AGachaCaptureStage> CaptureStageClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 사원증 텍스트/사진 (라이트 플레이트 위)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* ResultNameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* ResultDepartmentText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* ResultRarityText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* ResultRankText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* CharacterImage; // SceneCapture RT (증명사진 칸)

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UButtonWidget* ConfirmButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UCloseButtonWidget* UIE_CloseButton;

	// 스테이징 (전부 Optional — 없으면 해당 연출만 생략)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* DimBG;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UCanvasPanel* StageCanvas; // 셰이크 대상 (딤 제외 — 루트 흔들면 딤 가장자리 노출)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* SlotGlow;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget* SlotBar;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget* PeekClip;  // ClipToBounds — 내부엔 PeekCard 만 (FX 금지)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget* PeekCard;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget* IdCardBox; // 사원증 루트 (사출/펀치 대상)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* PhotoRing;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* CardShine; // 시트 광택 스윕 (레어+, ShineClip 안)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* CardBackGlow;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* BeamImage;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* FlashOverlay;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UImage* ShockwaveRing;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget* StampBox;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UCanvasPanel* FXLayer; // 빛가루 런타임 생성 (풀스크린)

	UFUNCTION() void OnConfirmClicked();
	UFUNCTION() void OnCloseClicked();

private:
	void DisplayCard(const FGachaResultData& Result);
	void BankCurrentResult();
	// 뱅크 성공 직후 배치 결과 안내 (착석/벤치) — 오버레이는 곧 닫히므로 토스트로
	void ShowPlacementToast() const;
	// 공유 무대 획득(AcquireShared) → 뽑힌 직원 댄스 캡처 → RT 를 CharacterImage 에 표시
	void ShowCharacterCapture(const FGachaResultData& Result);
	void EndCharacterCapture();
	// 배치 빌딩의 회사 타입 (부서 표시명 DT 조회 키) — 센터 카드와 사이드 카드가 같은 값을 봐야 한다
	ECompanyType GetBuildingCompanyType() const;

	// ===== 멀티 뽑기 (2~5건) =====
	void SpawnSideCards();  // 미니 카드 생성 + StageCanvas 배치 (초기 숨김)
	void UpdateDealing();   // StageTime 기반 stateless 딜링 (UpdateStaging 에서 호출)
	void BankAllResults();  // 캡처 큐 완료 게이트 → ConfirmGachaHireBatch → 집계 토스트
	bool IsMulti() const { return BatchResults.Num() >= 2; }

	// ===== 페이즈 스테이징 =====
	void BeginStaging();
	void UpdateStaging(float DeltaTime);
	void SkipToSettle();
	void ResetStagingVisuals();
	void RebuildDustPool();
	void PlayUITag(const FGameplayTag& Tag);
	FGameplayTag StingTagForTier(int32 Tier) const;
	static int32 TierOf(ELootBoxRarity R);
	static float BuildMulOf(ELootBoxRarity R);

	// 결과/뱅킹
	FGachaResultData CurrentResult;
	EGachaTier CurrentTier = EGachaTier::Normal;
	int32 CurrentBuildingIndex = INDEX_NONE;
	bool bCurrentBanked = false;

	// 멀티 결과 — BatchResults 는 뽑기 순서(사번/뱅킹 순서), DisplayOrder 는 슬롯 C,R1,L1,R2,L2 순 인덱스
	TArray<FGachaResultData> BatchResults;
	TArray<int32> DisplayOrder;
	UPROPERTY() TArray<TObjectPtr<UEmployeeIdCardMiniWidget>> SideCards; // DisplayOrder[1..] 순

	// 스테이징 상태 (전부 StageTime 기준 stateless)
	bool bStagingActive = false;
	bool bReduceMotion = false;
	bool bFlashFired = false;
	bool bStampFired = false;
	bool bButtonsShown = false;
	float StageTime = 0.f;
	ELootBoxRarity RevealRarity = ELootBoxRarity::Common;
	FLinearColor RarityColor = FLinearColor::White;
	float BuildDuration = 1.15f;
	float RevealStartTime = 0.f;
	float SettleTime = 0.f;
	float StampTime = 0.f;
	float ButtonsTime = 0.f;
	// 멀티 딜링 시간표 (BeginStaging 산정) — 연출감소면 DealStaggerEff = 0 (전원 동시 정착)
	float DealStartTime = 0.f;
	float DealEndTime = 0.f;
	float DealStaggerEff = 0.f;

	// 빛가루 분수 (FXLayer 런타임 생성, 레전드리+)
	TArray<FEmpGachaDustParam> DustParams;
	UPROPERTY() TArray<TObjectPtr<UImage>> DustImages;
	UPROPERTY() TObjectPtr<UTexture2D> DustTexture;

	UPROPERTY() URecruitmentManagerSubsystem* RecruitmentManager = nullptr;
	UPROPERTY() AGachaCaptureStage* CaptureStage = nullptr;
	TWeakObjectPtr<AWorkstationActorBase> PreferredWorkstationWeak;
};
