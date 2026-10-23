// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Panel/LiveOpsModalWidget.h"
#include "NewsPanelWidget.generated.h"

class UAlertMarkWidget;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UCommonTextBlock;
class UEventBannerItem;
class UHorizontalBox;
class UIconWithButtonWidget;
class UListView;
class ULiveOpsSubsystem;
class UNoticeItem;
class UProgressBar;
class UQuestRowItem;
class UScrollBox;
class UWidgetSwitcher;

/**
 * 소식 창 — 레일 탭 2(이벤트 · 공지). 각 탭은 좌측 목록 + 우측 상세 2단이다.
 * 이벤트 상세의 마일스톤 행은 미션 창의 UIE_QuestRow 를 그대로 재사용하고, 수령은 ULiveOpsSubsystem 이 판정한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UNewsPanelWidget : public ULiveOpsModalWidget
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
	TObjectPtr<UIconWithButtonWidget> EventTab = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UIconWithButtonWidget> NoticeTab = nullptr;

	// 탭별 받을 것 / 안 읽은 것 — 가시성 토글 전용이라 Optional (수정안 G2)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> EventTabDot = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> NoticeTabDot = nullptr;

	// 0 = 이벤트, 1 = 공지
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> ContentSwitcher = nullptr;

	// ===== 이벤트 페이지 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UListView> BannerList = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DetailTitleText = nullptr;

	// "2026.09.10 ― 2027.01.15"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DetailPeriodText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DetailDescText = nullptr;

	// "포인트" / "출석"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ProgressLabelText = nullptr;

	// "12 / 100"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ProgressValueText = nullptr;

	// 진행바를 감싼 SizeBox — 마일스톤이 없는 이벤트에서는 바째로 접는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> ProgressBarBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar = nullptr;

	// 핫타임 창 칩 — 디자인타임 샘플의 폰트/색을 읽어 런타임 항목에 적용한다 (보상 칩 선례)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HotTimeWindowsBox = nullptr;

	// "지금 적용 중" — 문구는 WBP 가 소유하고 코드는 가시성만 정한다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> HotTimeNowBadge = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UListView> MilestoneList = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EventEmptyState = nullptr;

	// ===== 공지 페이지 =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UListView> NoticeList = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NoticeTitleText = nullptr;

	// "2026.09.01 · 운영팀"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NoticeMetaText = nullptr;

	// 다른 공지를 고르면 맨 위부터 읽어야 한다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> NoticeBodyScroll = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NoticeBodyText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NoticeEmptyState = nullptr;

	// 진행 밴드 라벨은 이벤트 유형에 따라 갈린다 — 표시 문구는 WBP CDO 가 소유한다(배너 TypeName* 와 같은 규약)
	UPROPERTY(EditAnywhere, Category = "LiveOps|Text")
	FText ProgressLabelAttendance;

	UPROPERTY(EditAnywhere, Category = "LiveOps|Text")
	FText ProgressLabelPoints;

private:
	UFUNCTION()
	void HandleTabChanged(UCommonButtonBase* Button, int32 ButtonIndex);

	void HandleContentChanged();
	void HandleHotTimeChanged(float Multiplier);
	void HandleBannerSelectionChanged(UObject* SelectedItem);
	void HandleNoticeSelectionChanged(UObject* SelectedItem);
	void HandleBannerClicked(UEventBannerItem* BannerItem);
	void HandleNoticeClicked(UNoticeItem* NoticeItem);
	void HandleEntryGenerated(UUserWidget& Entry);
	void HandleRowClaim(UQuestRowItem* RowItem);

	void RefreshEventPage();
	void RefreshNoticePage();
	void RefreshTabDots();
	void FillEventDetail(UEventBannerItem* BannerItem);
	void FillNoticeDetail(UNoticeItem* NoticeItem);
	// 배지는 시각이 지날 때마다 바뀐다 — 상세를 다시 채우지 않고 이 부분만 갱신한다
	void ApplyHotTimeBadge();

	// 공지는 **공지 페이지가 보이는 동안 선택된 것**만 읽음이다 — 이벤트 탭에서 도는 갱신이 미읽음을 지우면 안 된다
	bool IsNoticePageVisible() const;

	// 선택이 갱신 중에 바뀌면 그 자리에서 읽음 처리를 하지 않는다 — 목록을 갈아엎는 중에 또 방송이 온다
	void FlushPendingNoticeRead();

	ULiveOpsSubsystem* GetLiveOps() const;

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> TabGroup = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UEventBannerItem>> EventItems;

	UPROPERTY()
	TArray<TObjectPtr<UNoticeItem>> NoticeItems;

	UPROPERTY()
	TArray<TObjectPtr<UQuestRowItem>> MilestoneItems;

	// 갱신 후에도 보던 이벤트/공지를 그대로 보여주기 위한 앵커
	FString SelectedEventId;
	FString SelectedNoticeId;
	FString PendingNoticeReadId;

	// 핫타임 배지 판정에 쓰는 현재 이벤트의 창 유무
	bool bSelectedHasHotTime = false;

	// 핫타임 칩의 폰트·잉크는 WBP 의 디자인타임 샘플이 소유한다 — 첫 채움이 샘플을 지우므로 여기 떠 둔다
	FSlateFontInfo HotTimeChipFont;
	FSlateColor HotTimeChipInk = FSlateColor(FLinearColor::White);
	bool bHotTimeChipStyleCached = false;

	// 탭 그룹 초기 선택·탭 힌트가 여는 동안 RefreshAll 이 여러 번 도는 것을 막는다 (열 때 1회)
	bool bSuppressRefresh = false;

	bool bRefreshing = false;
};
