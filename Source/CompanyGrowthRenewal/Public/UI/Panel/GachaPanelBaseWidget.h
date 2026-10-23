// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/ItemType.h"
#include "Enum/WidgetType.h"
#include "GachaPanelBaseWidget.generated.h"

class UButtonWidget;
class UIconWithButtonWidget;
class UGachaPanelFrameWidget;
class UTextBlock;
class UProgressBar;
class UImage;
class UHorizontalBox;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UItemInventoryManager;
class UTableManagerSubsystem;
class UUIManagerSubsystem;

// 두 가챠 패널(직원 채용·건물 특성/스킨)이 같은 레이아웃이라 공통 배선은 여기 두고, 기능 차이는 가상 훅으로 흡수한다
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API UGachaPanelBaseWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 미션 가이드 하이라이트 타겟
	UWidget* GetPullButtonWidget() const;
	UWidget* GetCloseButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ========== BindWidget (두 WBP 공통) ==========

	// 제목·아이콘·닫기·도움말 = 헤더 전체를 소유하는 공용 틀
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UGachaPanelFrameWidget* GachaFrame;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* BannerNormal;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* BannerAdvanced;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* HeroTitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* HeroDescText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* ProbabilityPillRow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* MileageBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* MileageLabel;

	// 채움 끝 글로우 헤드 (장식 — 없어도 바는 정상 동작)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* MileageHead;

	// 런타임 NewObject 텍스트는 스타일 상속이 없어 명시 필수(Roboto 폴백 방지). WBP Class Defaults 소유.
	UPROPERTY(EditAnywhere, Category = "Style")
	FSlateFontInfo PillFont;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* PullButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* ProbabilityInfoButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* MileageExchangeButton;

	// ========== 캐시 ==========

	UPROPERTY()
	UItemInventoryManager* ItemMgr = nullptr;

	UPROPERTY()
	UTableManagerSubsystem* TableMgr = nullptr;

	UPROPERTY()
	UUIManagerSubsystem* UIMgr = nullptr;

	UPROPERTY()
	UCommonButtonGroupBase* BannerGroup = nullptr;

	// ========== 갱신 훅 ==========

	virtual void UpdateHeroAndProbability() PURE_VIRTUAL(UGachaPanelBaseWidget::UpdateHeroAndProbability, );
	virtual void UpdateGauges() PURE_VIRTUAL(UGachaPanelBaseWidget::UpdateGauges, );
	virtual void UpdateCTALabel() PURE_VIRTUAL(UGachaPanelBaseWidget::UpdateCTALabel, );
	// 베이스는 호출하지 않는다. 파생 UpdateCTALabel 이 부른다.
	virtual void UpdateTicketCounts() {}
	void RefreshAll();

	// 경제 잠금/해제 — 해제 시 서버값으로 정정된 잔액·티켓·마일리지로 게이지/CTA 재계산.
	// 베이스가 구독한다(파생 셋이 같은 배선을 반복하지 않게)
	void HandleEconomyReady(bool bReady);

	// 배너 인덱스 → 파생의 티어 상태. 하이라이트 동기화와 RefreshAll 은 베이스가 한다.
	virtual void SetBannerIndex(int32 Index) PURE_VIRTUAL(UGachaPanelBaseWidget::SetBannerIndex, );

	// ========== 클릭 훅 (OnClicked().AddUObject 바인딩이라 리플렉션 불필요) ==========

	virtual void OnPullClicked() PURE_VIRTUAL(UGachaPanelBaseWidget::OnPullClicked, );
	virtual void OnProbabilityInfoClicked() {}
	virtual void OnMileageExchangeClicked() {}

	// 동적 델리게이트(OnCloseClicked)라 UFUNCTION. override 쪽에 UFUNCTION() 을 다시 붙이면 UHT 가 거부한다.
	UFUNCTION()
	virtual void OnCloseButtonClicked();

	// ========== 헬퍼 ==========

	void MakeBannerGroup(const TArray<UCommonButtonBase*>& Banners);
	void RebuildProbabilityPills(const TArray<FGachaRarityChance>& Rows);
	void SetGauge(UProgressBar* Bar, UImage* Head, UTextBlock* Label, float Pct, const FText& Text);
	void SetBannerOwnedCount(UCommonButtonBase* Banner, EItemType Ticket);
	void ApplyPullButton(UIconWithButtonWidget* Button, const FText& Label, bool bEnabled, EItemType TicketForIcon);
	// 숫자를 교환 규칙 상수에서 받아야 라벨이 교환 게이트와 어긋나지 않는다
	void SetExchangeLabel(int32 Points);

	// 생성까지만 — 셋업/AddToViewport 순서가 패널마다 반대라 호출자가 소유한다(같은 스택 push 는 패널을 가린다)
	UUserWidget* CreateOverlayWidget(EWidgetType Type);

private:
	UFUNCTION()
	void OnBannerSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex);
};
