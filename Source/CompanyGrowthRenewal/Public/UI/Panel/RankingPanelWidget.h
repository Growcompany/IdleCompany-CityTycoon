#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/RankingData.h"
#include "RankingPanelWidget.generated.h"

class UScrollBox;
class UWidgetSwitcher;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UButtonWidget;
class UTabButtonWidget;
class UButton;
class URankingManagerSubsystem;
class UTableManagerSubsystem;
class UCloseButtonWidget;
class URankingEntryCardWidget;

/**
 * 랭킹 메인 패널
 * - 매출/레벨 탭 전환 (WidgetSwitcher)
 * - 각 탭에 리더보드 스크롤 + 본인 카드 상단 고정
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URankingPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// 매출 탭 — 내 카드 (상단 고정)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<URankingEntryCardWidget> MyRankingEntryCard = nullptr;

	// 매출 리더보드 스크롤
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> RevenueLeaderboardScrollBox = nullptr;

	// 레벨 탭 — 내 카드 (상단 고정)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<URankingEntryCardWidget> MyLevelRankingEntryCard = nullptr;

	// 레벨 리더보드 스크롤
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> LevelLeaderboardScrollBox = nullptr;

	// 탭 전환 스위처
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> ContentSwitcher = nullptr;

	// 탭 버튼 (UIE_PillTabButton = UTabButtonWidget 합성 래퍼) — 시총 / 주간 매출 2탭
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTabButtonWidget> MarketCapTab = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTabButtonWidget> WeeklyRevenueTab = nullptr;

	// 닫기 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> UIE_CloseButton = nullptr;

	// 배경 클릭 닫기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

private:
	UPROPERTY()
	URankingManagerSubsystem* RankingMgr = nullptr;

	UPROPERTY()
	UTableManagerSubsystem* TableMgr = nullptr;

	// 현재 선택된 탭 인덱스 (0=시총, 1=주간 매출)
	int32 CurrentTabIndex = 0;

	// 탭 버튼 그룹 (배타적 선택 관리)
	UPROPERTY()
	UCommonButtonGroupBase* TabButtonGroup = nullptr;

	// 탭 선택 변경 콜백 (ButtonGroup에서 호출)
	UFUNCTION()
	void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	// 탭 직접 클릭 콜백 (OnClicked 바인딩)
	UFUNCTION()
	void OnMarketCapTabClicked();

	UFUNCTION()
	void OnWeeklyRevenueTabClicked();

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnBackgroundClicked();

	// 리더보드 로드 콜백
	void HandleLeaderboardLoaded(int32 TabIndex, const TArray<FRankingEntry>& Entries);

	// 스크롤박스에 카드 채우기 (본인 카드 상단 고정)
	void PopulateScrollBox(UScrollBox* ScrollBox, const TArray<FRankingEntry>& Entries);

	// 탭 전환
	void SwitchToTab(int32 TabIndex);
};
