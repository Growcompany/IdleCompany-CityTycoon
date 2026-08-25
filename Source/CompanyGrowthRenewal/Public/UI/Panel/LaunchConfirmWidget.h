#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/StageProgressData.h"
#include "Enum/QualityGrade.h"
#include "LaunchConfirmWidget.generated.h"

class UCommonTextBlock;
class UStatRowWidget;
class UDisciplineBarWidget;
class UIconCardWidget;
class UConfirmCancelWidget;
class UButtonWidget;
class UWidgetSwitcher;
class UImage;
class UResourceWidget;
class UHorizontalBox;
class UVerticalBox;
class UReviewCardWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaunchConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaunchCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaunchRetryRequested);


/**
 * 출시 확인 모달 위젯
 * ConfirmCancelWidget을 프레임으로 사용하고 ContentSlot에 출시 프리뷰 UI 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULaunchConfirmWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * 프리뷰 데이터 설정
	 * @param StageData 현재 스테이지 진행 데이터
	 * @param ProjectNumber 프로젝트 번호
	 */
	UFUNCTION(BlueprintCallable, Category = "Launch Confirm")
	void SetPreviewData(const FStageProgressData& StageData, int32 ProjectNumber);

	// M10 미션 가이드 — [출시] 버튼 하이라이트 타겟 (ConfirmCancelWidget의 확인 버튼)
	UWidget* GetConfirmButtonWidget() const;

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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

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

	// 품질 등급 (S/A/B/C/D/F)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_QualityGrade;

	// 등급 스탬프 3층 — SDF 머티리얼. RoundedBox 는 단색뿐이라 그라데이션/글린트가 안 나온다.
	// StampBG=세로 그라데이션 채움 / StampLine=키라인 / StampGlint=흐르는 광택
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampBG;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampLine;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StampGlint;

	// 품질 점수
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_QualityScore;

	// 프로젝트 이미지 카드
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconCardWidget* UI_ProjectImageCard;

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

	// 기본수익 (초당)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_RevenuePerSecond;

	// 예상 총 수익
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_TotalRevenue;

	// ===== 수익 계산 리빌 (배율 → 운영시간 → 초당수익 → 총액 카운트업) =====
	// 행 "컨테이너" 를 잡는 이유: 리빌이 RenderOpacity/RenderTransform 을 거는 대상이라
	// 라벨과 값이 한 덩어리로 같이 들어와야 한다. 안의 Text_* 는 데이터 주입용으로 따로 유지.

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* RevRow_Multiplier;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* RevRow_Duration;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* RevRow_PerSecond;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* RevTotalBlock;

	// X 닫기 버튼 (에디터에서 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* CloseButton;

	// ===== 출시 실패 페이지 = 사내 테스트 리포트 (ContentSwitcher index 1 — 미달 시에만 진입) =====

	// "기획 · QA 분야가 최소 기준에 미치지 못했습니다 ― 추가 개발로 보완할 수 있습니다."
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* Text_FailSummary;

	// "경험치 +75 · 김철수 Lv2까지 1판" — XP 는 게이트 판정보다 먼저 지급되므로 실패해도 팀은 자란다
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
	// 스탬프 3층(SDF 머티리얼) + 글자 아웃라인에 등급색 주입
	void ApplyGradeStampColor(const FLinearColor& GradeColor);

	// SDF 는 Wpx/Hpx 가 실제 위젯 크기와 같아야 코너가 정합. 위젯을 고정하는 대신 크기를 주입한다
	// (UResourceWidget::UpdateChipMaterialSize 와 같은 패턴 — 크기 변화 시에만)
	void UpdateStampMaterialSize();
	FVector2D LastStampMatSize = FVector2D::ZeroVector;

	// BindWidgetOptional 이라 미바인딩이 조용히 지나간다 — 인스턴스당 1회만 알린다
	bool bFailXpWarned = false;

	// 실패 페이지 채우기 (미달 직능 목록 + 컨티뉴 가능 여부). 미달일 때만 호출
	void FillFailureSection(const FStageProgressData& StageData);

	// 실패 페이지로 열렸는가 — 미리보기 경로를 우회하고,
	// 프레임의 Confirm/Cancel 을 [추가 개발]/[폐기] 로 재해석한다
	bool bFailurePhase = false;

	// ===== 스태거 리빌 엔진 (수익 계산 행 슬라이드인 → 총액 카운트업) =====

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

	// 수익 계산을 한 줄씩 들여보내고 총액을 굴린다 (배율 → 운영시간 → 초당수익 → 총액).
	// 미리보기 페이지 전용 — 실패 페이지에서는 호출하지 않는다.
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
