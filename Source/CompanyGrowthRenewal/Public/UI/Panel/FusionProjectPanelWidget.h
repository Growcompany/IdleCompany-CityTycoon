#pragma once
#include "CoreMinimal.h"
#include "UI/Panel/LiveOpsModalWidget.h"
#include "UI/Element/Common/ChipPlateSize.h"
#include "Data/SideFusionTypes.h"
#include "FusionProjectPanelWidget.generated.h"

class UButtonWidget;
class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UScrollBox;
class UUniformGridPanel;
class UVerticalBox;
class UFusionCardWidget;
class UFusionSlotWidget;
class UFusionBuildingRowWidget;
class USideContentSubsystem;
class UTableManagerSubsystem;
class UEmptyStateWidget;

UCLASS()
class COMPANYGROWTHRENEWAL_API UFusionProjectPanelWidget : public ULiveOpsModalWidget
{
	GENERATED_BODY()
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void RefreshAll() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(meta=(BindWidget)) UImage* BoardGrid = nullptr;

	// 좌: 콜라보 레일 (행은 런타임 생성)
	UPROPERTY(meta=(BindWidget))         UVerticalBox* CollabRail = nullptr;

	// 중앙: 편성
	UPROPERTY(meta=(BindWidget))         UHorizontalBox* SlotBox = nullptr;
	UPROPERTY(meta=(BindWidget))         UUniformGridPanel* CandidateGrid = nullptr;
	UPROPERTY(meta=(BindWidget))         UScrollBox* CandidateGridScroll = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* CandidateHintText = nullptr;
	UPROPERTY(meta=(BindWidget))         UEmptyStateWidget* EmptyBuildings = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* RewardTitleText = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* RewardHintText = nullptr;
	UPROPERTY(meta=(BindWidget))         UHorizontalBox* RewardBox = nullptr;

	// 우: 상세 카드
	UPROPERTY(meta=(BindWidget))         UHorizontalBox* DetailCoverBox = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* DetailNameText = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* DetailIndustryText = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* PowerLabelText = nullptr;
	// 파워 열은 최대 3칸(스마트카)이라 WBP 에 3벌을 저작해 두고 C++ 는 채우거나 Collapse 만 한다.
	// 런타임 ConstructWidget 으로 만들면 폰트·잉크를 C++ 가 들고 있게 되어 "정적 스타일은 WBP" 규칙을 깬다.
	UPROPERTY(meta=(BindWidget))         UHorizontalBox* PowerColumnBox = nullptr;
	UPROPERTY(meta=(BindWidget))         UWidget* PowerCol0 = nullptr;
	UPROPERTY(meta=(BindWidget))         UWidget* PowerCol1 = nullptr;
	UPROPERTY(meta=(BindWidget))         UWidget* PowerCol2 = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* PowerPct0 = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* PowerPct1 = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* PowerPct2 = nullptr;
	UPROPERTY(meta=(BindWidget))         UProgressBar* PowerMeter0 = nullptr;
	UPROPERTY(meta=(BindWidget))         UProgressBar* PowerMeter1 = nullptr;
	UPROPERTY(meta=(BindWidget))         UProgressBar* PowerMeter2 = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* PowerName0 = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* PowerName1 = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* PowerName2 = nullptr;
	UPROPERTY(meta=(BindWidget))         UWidget* InfoWell = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* DurationText = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* MarketCapText = nullptr;
	UPROPERTY(meta=(BindWidget))         UWidget* RunWell = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* RemainingText = nullptr;
	UPROPERTY(meta=(BindWidget))         UProgressBar* ProgressBar = nullptr;
	UPROPERTY(meta=(BindWidget))         UCommonTextBlock* BlockMessageText = nullptr;
	UPROPERTY(meta=(BindWidget))         UButtonWidget* StartButton = nullptr;
	UPROPERTY(meta=(BindWidget))         UButtonWidget* ClaimButton = nullptr;

	// 렌더 픽스처(Tests/FusionProjectRenderValidation)가 같은 값으로 캡처를 그린다 — 함께 움직일 것
	UPROPERTY(EditAnywhere, Category="Layout") float RailSpacing = 6.f;
	UPROPERTY(EditAnywhere, Category="Layout") float SlotSpacing = 8.f;
private:
	mutable FChipPlateSizer GridSizer;
	USideContentSubsystem* Side() const;
	UTableManagerSubsystem* Tables() const;
	void RefreshBoard();          // 레일·슬롯·후보·보상·상세를 한 번에 (열 때 / 선택 바뀔 때만)
	void RefreshRail();
	void RefreshSlots();
	void RefreshCandidates();
	void RefreshDetail();
	void RefreshCover(const TArray<ECompanyType>& Industries);
	void RefreshPowerColumns(float Requirement);
	void RefreshClock();
	void StopClock();
	int32 SlotForBuilding(int32 BuildingIndex) const;
	bool IsRunning(FName Key) const;
	// 표시를 건드리지 않는 선택 상태 갱신 — 활성화 시점엔 베이스가 RefreshAll 을 이미 부른다
	bool SelectCollab(FName Key);
	void HandleCard(FName Key);
	void HandleBuilding(int32 BuildingIndex);
	void HandleStart();
	void HandleClaim();
	void HandleStatsChanged(int32 EmployeeID);
	FText BlockReason(EFusionBlockReason Reason) const;
	FText MissingIndustryText(ECompanyType Industry) const;
	FName SelectedKey;
	TArray<int32> PickedBuildings;
	TArray<int32> ListedBuildings;
	TArray<FName> ListedKeys;
	TMap<int32, int32> BuildingToSlot;   // RefreshCandidates 가 채움 — 타일 클릭의 슬롯 결정
	TArray<ECompanyType> SlotIndustries; // 선택된 콜라보의 RequiredIndustries 사본
	FFusionClaimResult ClaimResult;
	bool bLastClaimable = false;
	FTimerHandle ClockHandle;
	UPROPERTY(Transient) TArray<TObjectPtr<UFusionCardWidget>> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<UFusionSlotWidget>> Slots;
	UPROPERTY(Transient) TArray<TObjectPtr<UFusionBuildingRowWidget>> BuildingRows;
};
