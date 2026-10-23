#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/StageProgressData.h"
#include "Enum/QualityGrade.h"
#include "LaunchConfirmWidget.generated.h"

class UCommonTextBlock;
class UTextBlock;
class UStatRowWidget;
class UDisciplineBarWidget;
class UConfirmCancelWidget;
class UButtonWidget;
class UWidgetSwitcher;
class UImage;
class UResourceWidget;
class UHorizontalBox;
class UVerticalBox;
class UScrollBox;
class UReviewCardWidget;
class UBorder;
class UCommonButtonStyle;
class UCommonBorderStyle;
class USizeBox;
class UHorizontalBoxSlot;
class UVerticalBoxSlot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaunchConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaunchCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaunchRetryRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnManufacturingResearchResultClosed, bool, bOpenQuantity);

// 결과 등장 칼끝 빛과 등급 도장의 세기·시간. 이미지·크기는 WBP가 소유한다.
USTRUCT()
struct FLaunchStrikeFxTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Strike FX", meta = (ClampMin = 0, ClampMax = 1))
	float Intensity = .9f;

	// 도장이 떨어지기 시작할 때의 배율. 착지 = 1. 0.14초 낙하 동안만 1을 넘는다.
	UPROPERTY(EditAnywhere, Category = "Strike FX", meta = (ClampMin = 1))
	float StampStartScale = 2.6f;

	UPROPERTY(EditAnywhere, Category = "Strike FX", meta = (ClampMin = .05))
	float StampFallTime = .14f;
};


/**
 * 출시 확인 모달 위젯
 * ConfirmCancelWidget을 프레임으로 사용하고 ContentSlot에 출시 프리뷰 UI 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULaunchConfirmWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	ULaunchConfirmWidget();

	/**
	 * 프리뷰 데이터 설정
	 * @param StageData 현재 스테이지 진행 데이터
	 * @param ProjectNumber 프로젝트 번호
	 */
	UFUNCTION(BlueprintCallable, Category = "Launch Confirm")
	void SetPreviewData(const FStageProgressData& StageData, int32 ProjectNumber);
	void SetManufacturingResearchResult(ECompanyType CompanyType, int32 ProjectIndex);

	UPROPERTY(BlueprintAssignable, Category = "Launch Confirm|Events")
	FOnManufacturingResearchResultClosed OnManufacturingResearchResultClosed;

	// M10 미션 가이드 — [출시] 버튼 하이라이트 타겟 (ConfirmCancelWidget의 확인 버튼)
	UWidget* GetConfirmButtonWidget() const;
	UWidget* GetGuideRewardWidget() const { return LaunchLootPanel; }
	// 내부 테스트 미달 창의 [폐기] ― 튜토리얼이 다시 착수하도록 짚는다(미달 창이 아니면 nullptr)
	UWidget* GetDiscardButtonWidget() const;

	UPROPERTY(BlueprintAssignable, Category = "Launch Confirm|Events")
	FOnLaunchConfirmed OnLaunchConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Launch Confirm|Events")
	FOnLaunchCancelled OnLaunchCancelled;

	// 실패 화면 [추가 개발] — 호스트가 TryRetryDevelopment 호출 후 카드 UI 갱신
	UPROPERTY(BlueprintAssignable, Category = "Launch Confirm|Events")
	FOnLaunchRetryRequested OnLaunchRetryRequested;

	/** 진행 중인 수익 계산 리빌을 최종 상태로 즉시 점프 (탭 스킵). 미재생 사운드는 버린다. */
	void SkipRevealQueue();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// 공통 다이얼로그 프레임 (ConfirmCancelWidget 인스턴스)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UConfirmCancelWidget* ConfirmCancelWidget;

	// 미리보기(0) ↔ 실패 리포트(1)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidgetSwitcher* ContentSwitcher;

	// 실패 페이지 = ContentSwitcher 두 번째 자식 (리뷰 페이지는 2026-08-22 폐기 — specs/2026-08-22-launch-reward-reveal-design.md)
	static constexpr int32 FailurePageIndex = 1;

	// 프로젝트 이름
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ProjectName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_ManufacturingResearchHint;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LaunchCompletionHeader;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_LaunchCompletionTitle;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LaunchLootPanel;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* LaunchLootRow;

	// 성공 결과의 이번 프로젝트 직원 경험치 총 적립량.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_Experience;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LaunchCoverSurface;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LaunchGradeStamp;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UScrollBox* LaunchResultScroll;

	UPROPERTY(EditDefaultsOnly, Category = "Launch Confirm|Rewards")
	TSubclassOf<UCommonButtonStyle> LaunchLootCardStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Launch Confirm|Rewards")
	TSubclassOf<UCommonBorderStyle> LaunchLootQuantityStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Launch Confirm|Rewards", meta = (ClampMin = 0))
	int32 LaunchLootLabelFontSize = 0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LaunchFlare;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LaunchSpark1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LaunchSpark2;

	// 칼끝 빛 캔버스 두 장. 이미지는 캔버스 원점에 저작하고 C++ 가 변형으로 옮긴다.
	// LaunchBackdropFx = 스크롤 뒤(제목 찌르기), LaunchStampFx = 표지와 등급 사이(도장 충격)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LaunchBackdropFx;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LaunchStampFx;

	UPROPERTY(EditDefaultsOnly, Category = "Launch Confirm|Strike FX")
	FLaunchStrikeFxTuning StrikeFx;

	// 품질 등급 (S/A/B/C/D/F)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_QualityGrade;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* LetterPlate;

	// 등급 스탬프 3층 — SDF 머티리얼. RoundedBox 는 단색뿐이라 그라데이션/글린트가 안 나온다.
	// StampBG=세로 그라데이션 채움 / StampLine=키라인 / StampGlint=흐르는 광택
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampBG;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampLine;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampGlint;

	// 실제 총수익에 곱해지는 품질 수익 배율(×2.5) — 품질 점수가 아니다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_QualityMultiplier;

	// "품질 배율 (S)" — 등급 글자가 들어가므로 C++ 가 채운다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_QualityMultiplierLabel;

	// 원본 텍스처 크기를 ScaleBox에 전달하는 작품 커버
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* Image_ProjectCover;

	// 분야별 성과 — 고정 축 6칸(EProductionDiscipline 슬롯 = 칸 인덱스). 비활성 분야는 빈 트랙.
	// 이 패널은 페이지 단위 paste 전환기를 위해 프레임 제외 전부 BindWidgetOptional 관행 유지.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineBarWidget* DiscBar6;

	// 예상 운영 시간
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_OperationTime;

	// 기본 수익 = 품질 1 기준 운영 기간 총수익. × 품질 배율 = 예상 총수익이 정확히 성립한다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_BaseRevenue;

	// 예상 총 수익
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_TotalRevenue;

	// ===== 수익 상세 원본 행과 요약 총액 리빌 =====
	// 총액 컨테이너에 리빌을 적용하므로 라벨과 값이 함께 등장한다.

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* RevRow_Multiplier;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* RevRow_Duration;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* RevRow_BaseRevenue;

	// 곱셈 단계마다 그 줄을 비추는 면 — 브러시 색 알파만 C++ 가 움직인다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* RevRowGlow_Base;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* RevRowGlow_Multiplier;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* RevTotalBlock;

	// 내용 위 캔버스 — 도장에서 총액으로 쏘는 칼끝 한 줄과 "×2.5" 칩
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LaunchFrontFx;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LaunchMulStreak;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* LaunchMulChip;

	// X 닫기 버튼 (에디터에서 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* CloseButton;

	// ===== 출시 실패 페이지 = 사내 테스트 리포트 (ContentSwitcher index 1 — 미달 시에만 진입) =====

	// "기획 · QA 분야가 최소 기준에 미치지 못했습니다 ― 추가 개발로 보완할 수 있습니다."
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_FailSummary;

	// 이번 판에 적립한 직원 경험치 — 실패해도 적립된다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_FailXp;

	// 미달 분야 게이지 행 (UIE_FailStatRow — 채움=획득/목표, 최소선 마커는 WBP 고정 50%). 미달 수만큼 표시.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* FailStatRow1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* FailStatRow2;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* FailStatRow3;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* FailStatRow4;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* FailStatRow5;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* FailStatRow6;

	// 테스터 의견 2장 (UIE_ReviewSnsCard 재사용 — 사내 직함이라 @ 접두사 없음)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UReviewCardWidget* TesterCard1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UReviewCardWidget* TesterCard2;

	// 컨티뉴 비용 칩 (Diamond). 버튼 안에 가격을 넣지 않는 게 가격/비용 CTA 표준 —
	// 행동 라벨은 프레임의 Confirm 버튼("추가 개발")이 맡고, 가격은 이 칩이 맡는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* RetryCostChip;

private:
	bool bManufacturingResearchResult = false;
	bool bManufacturingResultAnswered = false;
	FText OriginalLaunchCompletionTitle;
	bool bHasOriginalLaunchCompletionTitle = false;
	TArray<TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>> ManufacturingHiddenWidgets;
	void SetManufacturingHidden(UWidget* Widget);
	void RestoreManufacturingResultVisuals();
	struct FManufacturingBoxSize
	{
		TWeakObjectPtr<USizeBox> Box;
		bool bWidthOverride = false;
		float Width = 0.f;
		bool bHeightOverride = false;
		float Height = 0.f;
	};
	TArray<FManufacturingBoxSize> ManufacturingBoxSizes;
	TWeakObjectPtr<UHorizontalBoxSlot> ManufacturingProjectSlot;
	FMargin ManufacturingProjectPadding;
	float ManufacturingProjectSlotValue = 1.f;
	bool bManufacturingProjectSlotFill = false;
	uint8 ManufacturingProjectSlotAlignment = 0;
	TArray<TPair<TWeakObjectPtr<UVerticalBoxSlot>, uint8>> ManufacturingTextSlotAlignments;
	void CaptureManufacturingBox(USizeBox* Box);
	void ApplyManufacturingResultLayout();
	void RestoreManufacturingResultLayout();
	TWeakObjectPtr<UWidget> EntrySurface;
	FWidgetTransform EntryBaseTransform;
	float EntryBaseOpacity = 1.f;
	float EntryElapsed = .2f;
	void BeginResultEntry();
	void ResetResultEntry();
	// 남아 있는 스탬프 SDF 층에 등급색 주입
	void ApplyGradeStampColor(const FLinearColor& GradeColor);
	void ApplyGradeMedal(ECompanyType CompanyType, EQualityGrade Grade);

	// SDF 는 Wpx/Hpx 가 실제 위젯 크기와 같아야 코너가 정합. 위젯을 고정하는 대신 크기를 주입한다
	// (UResourceWidget::UpdateChipMaterialSize 와 같은 패턴 — 크기 변화 시에만)
	void UpdateStampMaterialSize();
	FVector2D LastStampMatSize = FVector2D::ZeroVector;

	// BindWidgetOptional 이라 미바인딩이 조용히 지나간다 — 인스턴스당 1회만 알린다
	bool bFailXpWarned = false;

	// 실패 페이지 채우기 (미달 직능 목록 + 컨티뉴 가능 여부). 미달일 때만 호출
	void FillFailureSection(const FStageProgressData& StageData);
	void FillLaunchLootSection(const FStageProgressData& StageData);
	void UpdateLaunchCelebration(float Time);
	void BeginGradeStamp();
	void TickGradeStamp(float DeltaTime);
	void ResetGradeStamp();
	FWidgetTransform GradeStampBaseTransform;
	FWidgetTransform GradeCoverBaseTransform;
	float GradeStampBaseOpacity = 1.f;
	float GradeStampElapsed = 0.f;
	bool bGradeStampActive = false;

	// 칼끝 빛 층 — NativeConstruct 에서 이름 계약(LaunchThrust00.., LaunchImpactShard0..)으로 모은다
	TArray<TWeakObjectPtr<UImage>> ThrustBlades;
	TArray<TWeakObjectPtr<UImage>> BurstBlades;
	TArray<TWeakObjectPtr<UImage>> TitleFlashBlades;
	TArray<TWeakObjectPtr<UImage>> ImpactShards;
	TArray<TWeakObjectPtr<UImage>> ImpactFlashBlades;
	TWeakObjectPtr<UImage> TitleGlow;
	TWeakObjectPtr<UImage> ImpactRing;
	TWeakObjectPtr<UImage> ImpactGlow;
	// < 0 = 비활성. 찌르기는 리빌 시작, 충격은 도장 착지에서 0이 된다.
	float StrikeTime = -1.f;
	float ImpactTime = -1.f;
	FVector2D ImpactCenter = FVector2D::ZeroVector;
	void CollectStrikeLayers();
	void ResetStrikeFx();
	void TickStrikeFx(float DeltaTime);
	void TickImpactFx(float DeltaTime);

	// 실패 페이지로 열렸는가 — 미리보기 경로를 우회하고,
	// 프레임의 Confirm/Cancel 을 [추가 개발]/[폐기] 로 재해석한다
	bool bFailurePhase = false;

	// ===== 결과 리빌 엔진 (총액 등장·카운트업, 보상 팝) =====

	struct FRevealItem
	{
		TWeakObjectPtr<UWidget> Widget;
		float StartAt = 0.0f;      // 시퀀스 로컬 시작 시각
		float Duration = 0.28f;
		bool bPop = false;         // true = 스케일 펀치(등급 리빌), false = 좌→우 슬라이드인
		bool bGrand = false;       // S등급 강조 — 펀치 진폭 상향
		bool bStarted = false;
		FName SoundKey;            // 등장 순간 1회 재생 (None = 무음)
		float SoundLead = 0.0f;    // 시각 등장보다 이만큼 앞서 재생 (어택이 착지에 겹치도록)
		float SoundVolume = 1.0f;  // 연속 등장 구간 볼륨 감쇠
		bool bSkipSilent = false;  // 탭 스킵의 보상 재생에서 제외 — 다중 카드 사운드가 한 프레임에 뭉치는 것 방지
	};
	TArray<FRevealItem> RevealQueue;
	float RevealClock = 0.0f;
	bool bRevealActive = false;

	// 스킵 시 점프할 시퀀스 종료 시각 (아이템/카운트업 중 최댓값)
	float RevealEndTime = 0.0f;

	// 리빌 사운드 스팸 게이트 — 마지막 재생 시각 (RevealClock 기준)
	float LastRevealSoundClock = -10.0f;

	// 수익 총액 카운트업 (0 → 목표). RevenueCountStartAt < 0 = 비활성
	int64 RevenueCountTarget = 0;
	float RevenueCountStartAt = -1.0f;

	// SetPreviewData 가 계산해 둔 총액 — 실패 여부 판정 뒤에 리빌 목표로 넘긴다
	int64 PendingTotalRevenue = 0;

	// 곱셈 연출: 0 → 기본 수익(품질 1 기준)까지 센 뒤, 도장 착지 순간 배율을 곱해 총액까지 다시 센다
	int64 PendingBaseRevenue = 0;
	int64 RevenueBaseTarget = 0;
	float RevenueMultiplyAt = 0.0f;
	FText PendingMultiplierText;
	FSlateColor TotalRevenueBaseColor;
	bool bTotalColorCaptured = false;
	void DriveMultiplyFx(float SinceMultiply);
	void FinishRevenueFx();

	// 분야 바 차오름 — 활성 칸만 0.08초 시차로 채운다
	TArray<TWeakObjectPtr<UDisciplineBarWidget>> RevealBars;
	TArray<float> RevealBarStarts;
	bool TickRevealBars(float Clock);

	// 요약 총액과 실제 보상을 공개한다. 미리보기 페이지 전용.
	void QueueRevenueReveal(int64 TotalRevenue);

	// 위젯을 숨김 상태(투명)로 예약 — NativeTick 이 시퀀스 시각에 맞춰 드러냄
	void QueueReveal(UWidget* Widget, float StartAt, bool bPop = false, FName SoundKey = NAME_None,
		bool bGrand = false, float SoundLead = 0.0f, float SoundVolume = 1.0f, bool bSkipSilent = false);

	// 스팸 게이트 통과 시에만 재생. bForce = 클라이맥스(등급)라 게이트 무시
	void PlayRevealSound(FName SoundKey, float VolumeScale, bool bForce = false);

	// 리빌 진행 중 확인 버튼 잠금 / 종료 시 복원
	void SetConfirmInteractionEnabled(bool bEnable);

	void OnDialogConfirmed();
	void OnDialogCancelled();

	UFUNCTION()
	void OnCloseBtnClicked();
};
