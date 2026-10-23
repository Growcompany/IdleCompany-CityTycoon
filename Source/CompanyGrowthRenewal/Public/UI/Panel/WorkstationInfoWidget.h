// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Office/WorkstationEquipmentText.h"
#include "WorkstationInfoWidget.generated.h"

class UImage;
class UButton;
class UButtonWidget;
class UCostActionButtonWidget;
class UPanelHeaderWidget;
class UCommonTextBlock;
class UListView;
class UUpgradeSlot;
class UBulkModeSelectorWidget;
class UBorder;
class UEmployeeRosterCardWidget;
class AWorkstationActorBase;
class UEmployeeManager;
enum class EEnhanceBulkMode : uint8;
enum class EResourceType : uint8;
enum class EWorkstationUpgradeType : uint8;

/**
 * 책상 도크 (우측 고정, 무광 셸 + 밝은 트레이) — 책상 강화(등급)·세 항목 강화 + 이 책상의 착석 관리.
 *
 * 착석/자리 비우기는 도크가 소유하고, 육성(스탯/강화)만 UI_EmployeeWindow(직원창)에 남는다.
 * 구성: 헤더(책상 이름이 제목) / 현재·다음 등급 장비 비교 + 세 항목 강화 행 + 독립 등급 강화 카드
 *      / 근무 직원 카드(점유 시만 — 로스터 카드 + 특화 + [직원창 열기] [자리 비우기])
 *      / 대기 직원 목록(빈 좌석일 때만, 탭 = 즉시 착석). 재배치·배수 선택은 도크 밖 좌하단.
 * 목업 SOT = docs/05_UI/mockups/WorkstationDockRedesign_MOCKUP.html
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorkstationInfoWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Workstation")
	void SetWorkstationData(AWorkstationActorBase* Workstation);

	UFUNCTION(BlueprintCallable, Category = "Workstation")
	void SetEmployeeData(int32 EmployeeID);
	AWorkstationActorBase* GetWorkstation() const { return CurrentWorkstation; }
	// 실제 구매 가능한 첫 항목 또는 준비된 무료 책상 강화 버튼.
	UWidget* GetFirstAffordableGrowthButton(bool bPromotionOnly) const;
	UWidget* GetCloseButtonWidget() const;

	// 미션 가이드 스텁 — 구 도크의 [스탯 관리]/첫 [+] 타겟은 직원창 이관으로 소멸(M9 재배선은 직능 SDD 작업).
	// MissionManagerSubsystem 호출 계약 유지용(nullptr = 가이드 링 생략).
	UWidget* GetOccupantManageStatsButton() const { return nullptr; }
	UWidget* GetFirstStatPlusButton() const { return nullptr; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// 최초 진입 코치마크 (DT_PanelIntro PanelKey="WorkstationInfo")
	void TryPlayPanelIntro();
	void HandlePanelIntroFinished();
	FTimerHandle PanelIntroTimer;
	TWeakObjectPtr<class UPanelIntroOverlayWidget> PanelIntroOverlay;

	// ========== 헤더 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

	// 배경 클릭 캐처 (도크 밖 클릭 = 닫기) — 풀스크린 투명 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// ========== 책상 카드 ==========

	// 등급 장비 구성 이미지는 DT_WorkstationSetupLevel의 대표 구성이며 선택한 책상 외형은 아니다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* DeskCompareCard;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* CurrentGradeText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* CurrentEquipImage;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* CurrentEquipText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* NextCompareColumn;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* NextGradeText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* NextEquipImage;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* NextEquipText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RetainedEffectText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* DeskPromotionCard;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PromotionTitleText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PromotionRewardText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PromotionConditionText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCostActionButtonWidget* PromoteButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUpgradeSlot* ProcessingUpgradeSlot;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUpgradeSlot* ToolsUpgradeSlot;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUpgradeSlot* EnvironmentUpgradeSlot;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBulkModeSelectorWidget* BulkModeSelector;

	// ========== 근무자 칩 카드 (점유 시만, 빈 책상 = Collapsed) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* WorkerCard;

	// 점유 직원 신원 = 직원창 로스터 카드 재사용. 초상·이름·Lv·★·종합·등급 젬을 한 부품이 담당하므로
	// 개별 위젯(WorkerFaceImage/WorkerNameText/…)은 폐기했다 — 같은 직원이 세 화면에서 갈라질 수 없게.
	// ⚠ 트리 인스턴스 이름도 OccupantCard 여야 한다(에셋명으로 두면 이름 불일치로 silent null).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEmployeeRosterCardWidget* OccupantCard;

	// ===== 근무자 서류 — 특화/연혁만 남는다 (신원은 OccupantCard 소관) =====

	// "그래픽 특화" (argmax 직능, enum DisplayName)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantSpecText;

	// 주력/보조 직능 값 (라벨 "주력"/"보조"는 트리 정적)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantLeadValue0;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantLeadValue1;

	// 연혁: 사번/근속 (입사일은 직원 창에서 본다)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantNoText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OccupantTenureText;

	// [직원창] → EWidgetType::EmployeeWindow PushPrompt (도크는 아래 유지 → 닫으면 복귀)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EmployeeWindowButton;

	// [자리 비우기] — UnseatEmployee 호출. 확인 모달 없음(되돌리기가 바로 아래 목록 1탭)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* UnseatButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* RelocateButton;

	// ========== 벤치 섹션 (빈 좌석일 때만) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* BenchSection;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* BenchHeadText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UListView* BenchListView;

	// 대기 0명 안내 + [채용하기]
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* BenchEmptyBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* RecruitButton;

	// 대기 목록 제목 옆 캡션(문구는 WBP) — 대기 직원이 없으면 접는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* NoteText;

private:
	UPROPERTY()
	AWorkstationActorBase* CurrentWorkstation;

	// 착석 직원 ID (-1이면 빈 책상)
	int32 CurrentEmployeeID = -1;

	UPROPERTY()
	UEmployeeManager* EmployeeManager;

	void UpdateUI();

	// 책상 실점유 ↔ CurrentEmployeeID 양방향 동기화 (직원창 자리비우기/해고/자동착석이 가려진 채로 바꿔도 칩 일치)
	void SyncOccupancyFromWorkstation();

	// 헤더 제목 + 현재/다음 비교와 등급 강화 갱신 (직원 유무와 무관)
	void UpdateDeskSection();
	void UpdateComponentSection();
	void UpdateComponentSlot(UUpgradeSlot* UpgradeSlot, EWorkstationUpgradeType Type);
	void HandleBulkModeChanged(EEnhanceBulkMode NewMode);
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	// 근무자 칩 채우기 (빈 책상이면 카드째 Collapsed)
	void UpdateWorkerCard();

	// 벤치 섹션 표시/갱신. 빈 좌석 없으면 섹션째 Collapsed
	void UpdateBenchSection();

	// 리스트 엔트리 생성 시 클릭 델리게이트 연결 (재활용 엔트리 대비 매번 재바인딩)
	void OnBenchEntryGenerated(UUserWidget& EntryWidget);

	UFUNCTION()
	void OnBenchCardClicked(int32 EmployeeID);

	void OnUnseatClicked();
	UFUNCTION() void OnRelocateClicked();
	bool bPendingPlacementEdit = false;
	void OnRecruitClicked();

	void FocusCameraOnDesk();

	// 점유 직원 워커에 흰색 선택 오버레이 on/off (CurrentEmployeeID 기준, 워커는 OfficeGameMode 에서 조회)
	void SetOccupantHighlight(bool bHighlighted);

	// ===== 핸들러 =====

	// 독립 등급 강화 버튼 클릭 → 책상 세팅 레벨 업그레이드
	void OnDeskUpgradeClicked();
	void OnProcessingUpgradeClicked();
	void OnToolsUpgradeClicked();
	void OnEnvironmentUpgradeClicked();
	void TryComponentUpgrade(EWorkstationUpgradeType Type, UUpgradeSlot* UpgradeSlot);

	// [직원창] → EmployeeWindow 푸시 (OfficeMainWidget::OnEmployeeButtonClicked 와 동일 경로)
	void OnEmployeeWindowClicked();

	// EmployeeManager OnEmployeeStatsChanged 구독 핸들러 (키 필터 — 점유 직원이면 칩 부분 갱신)
	void HandleEmployeeStatsChanged(int32 EmployeeID);

	// EmployeeManager OnEmployeeRosterChanged 구독 핸들러 (착석/해제 변동 → 점유 재동기화, 패널 가려져도 동작)
	void HandleRosterChanged();

	UFUNCTION()
	void OnCloseDelegate();

	// 배경 버튼 클릭 = 패널 닫기 (CloseButton 과 동일)
	UFUNCTION()
	void OnBackgroundClicked();
};
