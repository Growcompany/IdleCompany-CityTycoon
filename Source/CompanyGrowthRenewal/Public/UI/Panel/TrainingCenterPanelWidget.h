#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "UI/Panel/LiveOpsModalWidget.h"
#include "TrainingCenterPanelWidget.generated.h"

class UButtonWidget;
class UCommonTextBlock;
class UProgressBar;
class UUniformGridPanel;
class UVerticalBox;
class UImage;
class UWidget;

/**
 * 기술 연구소 모달 (WBP: UI_TrainingCenterPanel) — 건물 1개.
 * 해당 사무실 오른쪽 컨텐츠 레일에서 연다.
 *
 * 판정이 즉시·결정적이라 화면은 누르기 전에 결과를 다 보여준다 — 그래서 CTA 구성이 상태로 갈린다:
 *   충족   [시작] 하나
 *   미달   [성과 수령] + [시작]
 *   최종   [성과 수령] 하나 — 유일한 행동이라 주 CTA로 승격
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTrainingCenterPanelWidget : public ULiveOpsModalWidget
{
	GENERATED_BODY()

public:
	UTrainingCenterPanelWidget();

	/** 출장 패널과 같은 시그니처 — 두 창이 같은 방식으로 열려야 호출부가 안 갈린다. */
	void Configure(int32 InBuildingIndex, ECompanyType InCompanyType);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void RefreshAll() override;

	// ── 컨텍스트 밴드: 건물 칩 + 입장권 칩 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> IndustryWell = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> IndustryGlyph = nullptr;

	/** `배치 전원 N` — 책상을 받은 인원이다(벤치 제외). 출장자는 뺀다고 치지 않는다 — 판정 모집단과 같다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TeamCountText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EntryChipText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> RefillNote = nullptr;

	// ── 좌측 기술 레일: 현재 Lv 의 기술 전부를 완료/현재/잠김으로 보여준다 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> CourseRailBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RailLevelText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RailProgressText = nullptr;

	// ── 현재 목표와 다음 보상 증가 시점 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CurrentResearchText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CurrentResearchLabelText = nullptr;

	/** 시작 버튼 **옆**에 서는 비용 표시 — 아이콘 + 수량. 버튼 안에 넣지 않는다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CostRow = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CostIcon = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CostText = nullptr;

	/** 막힘은 채움색이 아니라 겉 아웃라인으로만 알린다 — 회색 비활성 금지. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ChallengeOutline = nullptr;

	UPROPERTY(EditAnywhere, Category = "Training|Color")
	FSlateColor CostInk = FSlateColor(FLinearColor(0.00334f, 0.01680f, 0.06124f, 1.0f));

	UPROPERTY(EditAnywhere, Category = "Training|Color")
	FSlateColor CostInkShort = FSlateColor(FLinearColor(0.27057f, 0.02845f, 0.01599f, 1.0f));

	/** 요구와 팀 파워는 따로 선다 — 한 문자열로 붙이면 어느 쪽이 모자란지 눈이 못 가른다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RequirementValueText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TeamPowerValueText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UProgressBar> PowerBar = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ResearchStatusText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NextGrowthText = nullptr;

	// ── 보상 플레이트 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RewardTitleText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RewardHintText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> RewardBox = nullptr;

	// ── 푸터 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> SweepButton = nullptr;

	/** 최종 연구 성과 수령 — 유일한 행동이라 주 CTA로 승격한다.
	 *  스타일을 런타임에 갈아끼우지 않고 버튼을 두 벌 둔다: 정적 스타일은 WBP 소유, C++ 은 자리만 나눠 쓴다.
	 *  라벨 색·아웃라인이 스타일을 따라가야 하는데 UButtonWidget 에는 그 세터가 없다는 것도 같은 결론을 가리킨다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> SweepButtonPromoted = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ChallengeButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> FooterNoteText = nullptr;

	/** 직원 0명 안내 — 본문을 덮는다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> EmptyState = nullptr;

private:
	void HandleChallenge();
	void HandleSweep();

	void RefreshRewardPreview(int32 Course);

	/** 현재 Lv 의 기술 전부를 레일에 다시 깐다. Lv 경계는 DT 의 Sequence 로 찾는다. */
	void RefreshCourseRail(int32 Course, int32 Best, int32 MaxCourse);

	int32 BuildingIndex = INDEX_NONE;
	ECompanyType CompanyType = ECompanyType::None;
	bool bActionInFlight = false;
};
