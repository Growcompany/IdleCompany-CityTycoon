// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/QuestTypes.h"
#include "UI/Element/Common/CountdownText.h"
#include "UI/Panel/LiveOpsModalWidget.h"
#include "QuestPanelWidget.generated.h"

class UAlertMarkWidget;
class UButtonWidget;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UCommonTextBlock;
class UHorizontalBox;
class UIconWithButtonWidget;
class UListView;
class UQuestRowItem;
class UQuestRowWidget;
class UWidgetSwitcher;

/**
 * 미션 창 — 레일 탭 3(일일 · 주간 · 업적) + 주기판(요약 줄 + 행 리스트) / 업적판(요약 스트립 + 행 리스트).
 * 수령은 행 끝 [받기]와 푸터 [모두 받기] 두 경로뿐이고 판정·보상은 전부 UQuestSubsystem 이 한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UQuestPanelWidget : public ULiveOpsModalWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void RefreshAll() override;

	// ===== 레일 탭 (UIE_RailTab — CommonButtonBase 파생이라 그룹에 직접 등록) =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UIconWithButtonWidget> DailyTab = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UIconWithButtonWidget> WeeklyTab = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UIconWithButtonWidget> LifetimeTab = nullptr;

	// 탭별 받을 것 — 가시성 토글 전용이라 Optional (수정안 G2)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> DailyTabDot = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> WeeklyTabDot = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> LifetimeTabDot = nullptr;

	// 0 = 주기판(일일·주간), 1 = 업적판
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> ContentSwitcher = nullptr;

	// ===== 페이지 0 — 요약 줄 + 일일/주간 리스트 =====

	// "오늘의 미션" / "이번 주 미션"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> SummaryTitleText = nullptr;

	// 완료 수 (받을 것 + 받은 것)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> SummaryDoneText = nullptr;

	// " / 6 완료"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> SummaryTotalText = nullptr;

	// 줄 수만큼 점 (런타임 생성)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PipBox = nullptr;

	// "초기화까지 03:12:44"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ResetText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UListView> QuestList = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimAllButton = nullptr;

	// ===== 페이지 1 — 업적 =====

	// "달성 12 / 50"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LifetimeSummaryText = nullptr;

	// "받을 수 있는 보상 2"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LifetimeClaimableText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UListView> LifetimeList = nullptr;

	// 페이지 1 푸터 — 같은 버튼을 두 페이지에 재부모화할 수 없어 별도 인스턴스다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimAllButtonLifetime = nullptr;

	UPROPERTY(EditAnywhere, Category = "Style|Summary")
	FSlateBrush PipOnBrush;

	UPROPERTY(EditAnywhere, Category = "Style|Summary")
	FSlateBrush PipOffBrush;

	UPROPERTY(EditAnywhere, Category = "Style|Summary", meta = (ClampMin = "0.0"))
	float PipGap = 10.f;

	// 레일 탭 글자 ― 버튼의 TextColor 가 스타일 글자색을 덮으므로 선택 상태에 맞춰 직접 싣는다
	UPROPERTY(EditAnywhere, Category = "Style|Rail")
	FSlateColor RailInkSelected = FSlateColor(FLinearColor::White);

	UPROPERTY(EditAnywhere, Category = "Style|Rail")
	FSlateColor RailInkNormal = FSlateColor(FLinearColor(FColor(0x8E, 0x9A, 0xAD)));

private:
	UFUNCTION()
	void HandleTabChanged(UCommonButtonBase* Button, int32 ButtonIndex);

	void HandleBoardChanged();
	void HandleClaimAll();
	void HandleRowClaim(UQuestRowItem* RowItem);
	void HandleEntryGenerated(UUserWidget& Entry);

	void RefreshPeriodPage();
	void RefreshLifetimePage();
	// 주기판 요약 줄 — FillList 가 채운 PeriodItems 를 센다
	void RefreshSummary();
	void RefreshTabDots();
	void ApplyTabInks(int32 SelectedIndex);
	void StartResetCountdown();

	// 주기의 행을 읽어 아이템 배열·리스트를 통째로 다시 채운다
	void FillList(UListView* TargetList, TArray<TObjectPtr<UQuestRowItem>>& TargetItems, EQuestPeriod Period);

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> TabGroup = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UQuestRowItem>> PeriodItems;

	UPROPERTY()
	TArray<TObjectPtr<UQuestRowItem>> LifetimeItems;

	EQuestPeriod CurrentPeriod = EQuestPeriod::Daily;

	// 탭 그룹 초기 선택·탭 힌트가 여는 동안 RefreshAll 이 세 번 도는 것을 막는다 (열 때 1회)
	bool bSuppressRefresh = false;

	FCountdownTicker Countdown;
};
