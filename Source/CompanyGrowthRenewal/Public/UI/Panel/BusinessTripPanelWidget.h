#pragma once

#include "CoreMinimal.h"
#include "UI/Panel/LiveOpsModalWidget.h"
#include "UI/Element/Common/CountdownText.h"
#include "Enum/CompanyType.h"
#include "Enum/ProductionDiscipline.h"
#include "BusinessTripPanelWidget.generated.h"

class UImage;
class UProgressBar;
class UWidgetSwitcher;
class UHorizontalBox;
class UCommonTextBlock;
class UButtonWidget;
class UBusinessTripCardWidget;
class UBusinessTripCrewCardWidget;
class UBusinessTripProgressWidget;
class USideContentSubsystem;
enum class EDispatchBlockReason : uint8;

/**
 * 출장 모달 (건물 1개). 카드 3장을 고르고 이 건물 직원을 직접 선발해 보낸다.
 * 상태는 USideContentSubsystem 이 소유하고 이 위젯은 표시·입력만 한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBusinessTripPanelWidget : public ULiveOpsModalWidget
{
	GENERATED_BODY()

public:
	/** 건물 산업은 여는 쪽(오피스 도크 · 허브 행)이 이미 알고 있다 — 여기서 다시 조회하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "BusinessTrip")
	void Configure(int32 InBuildingIndex, ECompanyType InCompanyType);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshAll() override;

	// ── 헤더 · 메타 줄 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* IndustryGlyph = nullptr;

	/** 메타 밴드 아우로라 — 산업색이 「이 건물의 출장」을 색으로 세운다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeaderAurora = nullptr;

	/** 글리프 뒤 색판 — 산업 색은 DT_CompanyInfo.AccentColor 가 단일 진실이라 코드가 칠한다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* IndustryPlate = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* IndustryText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* TeamPowerText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* SlotStateText = nullptr;

	/** 카드 갱신까지 남은 시간. 출장은 입장권이 없어 칩 자리를 이게 쓴다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RefreshCountdownText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidgetSwitcher* PaneSwitcher = nullptr;

	// ── A. 카드 선택 + 편성 ──
	/** 카드는 정확히 3장이라 런타임 생성 대신 자리를 고정한다(스펙 §모듈4). */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBusinessTripCardWidget* Card0 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBusinessTripCardWidget* Card1 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBusinessTripCardWidget* Card2 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* CrewRail = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* PickSlotBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* PickCountText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RosterNoteText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RequireText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* RequireBar = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* HaveText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* HintText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* AutoPickButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* DepartButton = nullptr;

	/** 선발 슬롯 점 — 디자인타임 샘플에서 브러시·크기를 복사해 인원 수만큼 찍는다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PickSlotSample = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor PickSlotFilledColor = FLinearColor(0.047f, 0.328f, 0.745f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor PickSlotEmptyColor = FLinearColor(1.f, 1.f, 1.f, 0.26f);

	// 판정 바는 충족/미달이 런타임에 갈리므로 색만 코드가 고른다 — 값은 WBP 가 소유한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor BarOkColor = FLinearColor(0.263f, 0.788f, 0.369f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor BarShortColor = FLinearColor(0.434f, 0.068f, 0.048f, 1.f);

	/** 미달 수치 강조 — 다크 위 빨강은 안 읽혀서 앰버로 뺀다(목업 검수 결과). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor ShortValueColor = FLinearColor(0.571f, 0.209f, 0.002f, 1.f);

	/** 충족일 때의 판정 문구 색. UseForeground 로 되돌리면 WBP 가 지정한 색을 부모 색이 덮는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Color")
	FLinearColor HaveOkColor = FLinearColor(0.839f, 0.855f, 0.871f, 1.f);

	// ── B. 진행 · 수령 ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBusinessTripProgressWidget* ProgressWidget = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidget* ResultBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ResultSubText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* ResultRewardBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ResultLineText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* ClaimButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* HintTextB = nullptr;

	/** 초상 로드 크기 — 레일 칩과 진행 카드가 같은 값을 써야 캐시가 두 벌이 안 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusinessTrip")
	FVector2D PortraitSize = FVector2D(132.f, 132.f);

	/** 출발 가능할 때의 라벨. 막혔을 때는 사유가 라벨을 대신하므로 원래 문구를 여기서 되찾는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusinessTrip")
	FText DepartLabel = NSLOCTEXT("BusinessTrip", "DepartReady", "출장 출발");

private:
	void RebuildCards();
	void RebuildCrewRail();
	void RebuildPickSlots(int32 InNeed, int32 InPicked);
	void RefreshJudgement();
	void RefreshProgressPane();
	void ShowResult(const TArray<struct FMissionReward>& InRewards);

	/** 카드 3장을 순회용으로 묶는다 — 비어 있는 자리는 걸러 준다. */
	TArray<UBusinessTripCardWidget*> GetCardWidgets() const;

	USideContentSubsystem* GetSideContent() const;
	FText GetDisciplineName(EProductionDiscipline InDiscipline) const;

	/** 비활성 출발 버튼의 라벨 = 막힌 사유. 회색 판만 두면 눌러 보고 나서야 이유를 찾게 된다. */
	FText MakeDepartBlockedLabel(bool bInServerTimeOk, EDispatchBlockReason InReason,
		const FText& InDisciplineName, int32 InCrewShort, int32 InScoreShort) const;

	void HandleCardClicked(FName InCardKey);

	// 로스터 카드의 OnRosterCardClicked 는 dynamic 멀티캐스트라 핸들러가 UFUNCTION 이어야 한다
	UFUNCTION()
	void HandleCrewToggled(int32 InEmployeeID);

	void HandleAutoPickClicked();
	void HandleDepartClicked();
	void HandleClaimClicked();
	void HandleSideContentChanged();

	int32 BuildingIndex = INDEX_NONE;
	ECompanyType CompanyType = ECompanyType::None;

	FName SelectedCardKey;
	TArray<int32> PickedCrew;

	/** 결과 화면을 띄운 동안에는 상태 변경 방송으로 화면을 되돌리지 않는다. */
	bool bShowingResult = false;

	UPROPERTY()
	TArray<TObjectPtr<UBusinessTripCrewCardWidget>> CrewChips;

	FCountdownTicker RefreshTicker;
	FDelegateHandle SideContentChangedHandle;
};
