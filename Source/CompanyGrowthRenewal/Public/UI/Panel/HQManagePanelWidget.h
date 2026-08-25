// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Table/HQLevelData.h"
#include "HQManagePanelWidget.generated.h"

class UButtonWidget;
class UIconWithButtonWidget;
class UWidgetSwitcher;
class UBorder;
class UScrollBox;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UButton;
class UCloseButtonWidget;
class UStatRowWidget;
class UCommonTextBlock;
class UProgressBar;
class UImage;
enum class EResourceType : uint8;

// 빌딩 목록 정렬 기준 (패널 내부 전용)
enum class EHQBuildingSortMode : uint8
{
	Level,
	Income,
	Employees
};

/**
 * 본사 관리 패널 — 본사/빌딩목록/재무제표 3탭 구성
 * 레이아웃/UX 스펙: docs/05_UI/HQ_PANEL_REDESIGN.md (claim 레벨업 + 가로 3컬럼)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQManagePanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== 공용 (헤더 영역) =====

	// 배경 클릭 시 닫기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn;

	// 닫기 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// ===== 레일 탭 (UIE_RailTab — 설정창 레일 문법 공용, CommonButtonBase 파생이라 그룹 직접 등록) =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* HQTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* BuildingListTab;

	// 재무제표 — 데이터 레이어 전까지 트리에서 Collapsed (숨김 탭)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* FinancialTab;

	// ===== 콘텐츠 전환 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidgetSwitcher* ContentSwitcher;

	// ===== HQ 탭 (인덱스 0) — 좌 컬럼: 레벨 히어로 =====
	// 신규 멤버는 W1(WBP 재배치) 전까지 Optional — 구 WBP에서도 컴파일/동작 유지

	// 메달 안 레벨 숫자 ("7" — "LEVEL" 캡션은 트리 정적)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LevelText;

	// Lv 전환 라인 "Lv.7 ▶ Lv.8" — 최대 레벨 시 Cur/Arrow Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CurLevelText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TransArrowText = nullptr;

	// 전환 라인 골드 "Lv.8" / 최대 레벨 시 "최고 등급 달성"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* NextLevelText;

	// 종합 진행바 — 활성 조건들의 정규화 진행률 평균 (이산 충족 카운트 아님)
	// ※ 선행 글로우 헤드는 의도적으로 없음 — 퍼센트 텍스트가 바 위에 겹친 labeled meter 라 역할이 중복
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* OverallProgressBar;

	// "76%" / 최대 레벨 시 "MAX"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OverallProgressText;

	// 현황 요약 행 — 라벨/색은 WBP 디자이너 값 그대로, cpp는 값만 갱신
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_Buildings;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_Employees;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_TopStage;

	// ===== HQ 탭 — 레벨업 조건 체크리스트 =====
	// DT 값 0 = 조건 없음 → 행 Collapsed. 다음 레벨 없으면 전체 Collapsed.

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_Cond_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_Cond_Buildings;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_Cond_Tier;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UStatRowWidget* Row_Cond_Employees;

	// 시총 조건행 — 2막 레벨 전용이라 대부분 Collapsed. WBP 배치 전에도 패널이 살아야 하므로 Optional
	// (다른 Row_Cond_* 는 required — WBP 반영 확인 후 required로 승격할 것)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* Row_Cond_MarketCap = nullptr;

	// ===== HQ 탭 — 보상 스트립(조건 웰 하단) + 하단 밴드 캔디 CTA =====

	// 보상 스트립 라벨 "Lv.8에 열리는 것"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* RewardTitleText;

	// 해금 칩 — UnlockDescription 없으면 칩째 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* UnlockChipBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* UnlockText = nullptr;

	// Lv N+2 미래 칩 — 해금 내용은 가리고 존재만 예고
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* FutureChipBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* FutureText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* FutureCaptionText = nullptr;

	// 하단 밴드 — 미충족 시 "남은 조건 N개", 레디 시 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* RemainText = nullptr;

	// 캔디 CTA 안 비용 금액 (부족 시 빨강 — v4 비용 인라인 예외)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CostAmountText = nullptr;

	// 레벨업 버튼 — claim: 조건 충족 시 활성+샤인 (펄스 금지 — v4)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* LevelUpButton;

	// ===== 장식 VFX (WBP에 넣은 것만 동작) =====

	// 레벨업 가능 동안 CTA 위를 주기 횡단하는 샤인 (버튼 Overlay 안, ClipToBounds 전제)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* CTAShineImage = nullptr;

	// 메달 골드 링 글로우 — 전 조건 충족(레디) 동안만 은은한 브리딩 (스케일 펄스 아님)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* MedalGlowImage = nullptr;

	// ===== 빌딩 목록 탭 (인덱스 1) =====

	// CommonHierarchicalScrollBox는 UScrollBox 상속 — 부모 타입으로 바인딩
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* BuildingListScrollBox;

	// 정렬 바 — WBP 반영 전까지 Optional (수십 개 빌딩 대비)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* SortLevelBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* SortIncomeBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* SortEmployeeBtn = nullptr;

private:
	UPROPERTY()
	UCommonButtonGroupBase* TabButtonGroup = nullptr;

	UPROPERTY()
	UCommonButtonGroupBase* SortButtonGroup = nullptr;

	// 기본 = 레벨 내림차순. 같은 모드 재클릭 시 방향 토글 (ProductSellModal 컨벤션)
	EHQBuildingSortMode BuildingSortMode = EHQBuildingSortMode::Level;
	bool bBuildingSortAscending = false;

	int32 CachedHQLevel = 1;

	// 빌딩 관리 패널로 인계 중. 닫힘 애니메이션 탓에 이 패널의 비활성화는 후임 패널이 열린 뒤에 오므로,
	// 그때의 입력 모드/가림 고스트는 이미 후임 것이다 (되돌리면 후임 고스트가 죽는다)
	bool bHandingOffToManagePanel = false;

	// RefreshAffordability(경량 경로)용 스냅샷 — 돈 외 조건은 UpdateHQInfo 시점 값 재사용
	bool bNextLevelAvailable = false;
	FHQLevelData CachedNextData;
	int32 CachedBuildingCount = 0;
	int32 CachedTierBuildingCount = 0;
	int32 CachedTotalEmployees = 0;
	int64 CachedMarketCap = 0;

	// 장식 VFX 상태 — 이벤트 구동 패널이라 틱은 VFX 전용 (이미지 없으면 즉시 탈출)
	bool bCTAShineActive = false;
	bool bMedalGlowActive = false;
	float VFXTimer = 0.f;
	static constexpr float ShinePeriod = 2.6f;     // 스윕 주기 (이동 + 휴지)
	static constexpr float ShineTravel = 0.6f;     // 횡단 시간
	static constexpr float ShineSweepHalf = 180.f; // 횡단 반폭 (버튼 폭 기준, 시각루프로 튜닝)

	UFUNCTION()
	void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void HandleHQLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleStageComplete();

	void HandleResourceChanged(EResourceType Type, int64 NewValue);
	void HandleEmployeeHireCompleted(const FString& EmployeeID);

	void OnLevelUpButtonClicked();

	// bAnimateConditions: 레벨업 직후 2차 연출 — 조건행 카운트업+펀치
	void UpdateHQInfo(bool bAnimateConditions = false);

	// Money 변동 경량 경로 — OnResourceChanged는 고빈도라 전체 갱신 금지
	void RefreshAffordability(int64 CurrentMoney);

	// 해당 레벨의 보상 문구 — "건설 가능 건물 +N" 을 항상 앞세우고 DT 해금 문구가 있으면 뒤에 잇는다.
	// 보상 스트립과 축하 화면이 같은 문구를 써야 하므로 생성처를 하나로 둔다.
	// +N 을 상수로 박지 않는 이유 = CSV 에서 구간별로 +2 로 튜닝해도 문구가 따라와야 한다.
	FText BuildLevelRewardText(int32 Level) const;

	// Money 조건행 — int64는 float 정밀도 한계로 SetCurrentProgress 대신 축약 문자열+바 직접 갱신
	void UpdateMoneyConditionRow(int64 CurrentMoney);
	void UpdateOverallProgress(int64 CurrentMoney);
	float ComputeOverallProgress(int64 CurrentMoney) const;
	int32 ComputeUnmetCount(int64 CurrentMoney) const;
	void UpdateLevelUpButtonState();

	void PopulateBuildingList();
	void OnBuildingMoveClicked(int32 BuildingIndex);

	// 정렬 바 — 그룹은 배타 선택 비주얼만, 모드/방향은 개별 OnPressed 핸들러가 관리
	void SetupSortButtonGroup();
	void UpdateSortButtonTexts();
	void HandleSortLevelClicked();
	void HandleSortIncomeClicked();
	void HandleSortEmployeeClicked();
};
