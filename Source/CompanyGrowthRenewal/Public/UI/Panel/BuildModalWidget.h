#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/CompanyType.h"
#include "Enum/LootBoxRarity.h"
#include "Table/BuildableCardTable.h"
#include "BuildModalWidget.generated.h"

class UIndustryButtonWidget;
class UTabButtonWidget;
class UButtonWidget;
class UBuildEntityCardWidget;
class UCloseButtonWidget;
class UTileView;
class UButton;
class UTableManagerSubsystem;
class UCommonButtonGroupBase;
class UCommonButtonBase;

/**
 * 빌드 모달 (Build 1.1) — 중앙 프롬프트 모달.
 * 산업 타일 6 + 건물 카드 가로 ListView (등급순 정렬 스크롤).
 * CTA/비용 없음 — 카드 탭 = 즉시 배치(구 BuildPanel UX 복원, 산업 선택만 추가).
 * 산업은 모달 진입 시 기본 선택(Game) → 카드 탭 시 함께 넘겨 배치 확정 시 건물에 CompanyType 베이크.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildModalWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	// ── 산업 타일 6 (고정 6장, CommonButton 변형) ──
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UIndustryButtonWidget* GameTile;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UIndustryButtonWidget* ElectronicsTile;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UIndustryButtonWidget* FinanceTile;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UIndustryButtonWidget* ITTile;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UIndustryButtonWidget* SemiconductorTile;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UIndustryButtonWidget* AutomobileTile;

	// ── 건물 카드 가로 TileView (엔트리 EntryHeight 고정 → CommonButton 카드가 ListView에서 세로로 늘어나던 문제 해소) ──
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTileView* BuildItemListView;

	// ── 닫기 / 딤 ──
	// 닫기는 필수 기능이라 required. Optional 이던 시절 WBP 이름(UIE_CloseButton)과
	// 달라 조용히 null 이 되어 X 버튼이 아무 반응도 없었다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// 풀스크린 투명 딤 — 클릭 시 닫기 (ProductSellModal 패턴)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// 산업 미선택 시 카드 자리에 표시되는 빈 상태 안내("산업을 선택하세요"). 산업 선택 시 Collapsed.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UWidget* EmptyStatePrompt;

	// 카드 [일반]/[특수] 탭 — WBP 미배치 가능성이 있어 Optional. 둘 다 존재할 때만 키스톤 필터/탭 그룹 활성.
	// 타입은 BuildingManagePanel 의 탭 래퍼(UTabButtonWidget)와 동일.
	// 일반/특수 = 프로젝트 표준 필터토글(UI_Element_Button + CUI_Style2_TapButton_Blue). UButtonWidget=CommonButtonBase 파생이라 그룹 직접 등록.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButtonWidget* NormalCardTab;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButtonWidget* SpecialCardTab;

private:
	friend class FCGRBuildModalSelectionTest;

	// 건설 payload는 캐시가 아니라 버튼 그룹의 현재 선택 상태에서 결정한다.
	bool TryGetSelectedCompanyType(ECompanyType& OutCompanyType) const;

	// 산업 타일 배타선택 변경 핸들러 (UCommonButtonGroupBase)
	UFUNCTION()
	void OnIndustryTileSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	// 카드 [일반](0)/[특수](1) 탭 배타선택 변경 핸들러 — 인덱스 저장 후 카드 재필터.
	UFUNCTION()
	void OnCardTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	// ListView 엔트리 생성 시 카드 선택 델리게이트 연결 (재활용 대비)
	void HandleEntryGenerated(UUserWidget& EntryWidget);

	// 건물 카드 탭 핸들러 — 즉시 배치 (BuildEntityCard 의 OnCardSelected 구독). CTA 없음.
	UFUNCTION()
	void OnBuildCardSelected(const FBuildableCardTable& SelectedInfo);

	// 닫기 / 딤
	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnBackgroundClicked();

	// 건물 카드 리스트 채우기 (등급순 정렬 — BuildPanel 이식)
	void RefreshBuildingCards();

	void CloseModal();

	// 산업 타일 잠금 재평가 (매 오픈 — HQ 레벨은 모달이 열려 있는 동안 변하지 않음)
	void RefreshIndustryLocks();

	// 잠긴 타일 탭 안내 토스트 (OnClicked 페이로드 바인딩 — 잠금 아닐 땐 no-op)
	void OnIndustryTileClicked(UIndustryButtonWidget* Tile);

	UPROPERTY()
	UTableManagerSubsystem* TableManager = nullptr;

	// 산업 타일 배타선택 그룹 (탭 정석 — NativeConstruct 셋업)
	UPROPERTY()
	UCommonButtonGroupBase* IndustryButtonGroup = nullptr;

	// 카드 [일반]/[특수] 탭 배타선택 그룹 (두 탭 모두 존재할 때만 NativeConstruct 에서 셋업).
	UPROPERTY()
	UCommonButtonGroupBase* CardTabButtonGroup = nullptr;

	// 현재 카드 탭 인덱스. 0=일반(비키스톤), 1=특수(키스톤 모뉴먼트).
	int32 CurrentCardTabIndex = 0;

	UPROPERTY()
	TArray<UIndustryButtonWidget*> AllTiles;

	// 진입 시 기본 선택(Game). 카드 탭 시 함께 배치로 넘김.
	ECompanyType SelectedCompanyType = ECompanyType::Game;

	// 보유 건물 1채당 신축비 배율. 부지(Money)가 이미 누진이라 벽돌은 완만하게만 민다.
	// 밸런스 노브지만 DT 자리가 없어 상수로 둔다 — 바꾸려면 빌드가 필요하다(Balance Studio 의 src 라우트).
	static constexpr float BuildCostGrowthPerBuilding = 1.12f;

	// 구매용 카드 목록의 건설비에 보유 채수 누진을 곱한다.
	// 카드 표시 → 선택 시 차단 → 배치 확정 결제가 모두 같은 구조체 사본을 흘려받으므로,
	// 목록을 만드는 이 지점에서 한 번만 곱하면 셋이 자동으로 일치한다.
	void ApplyBuildCostProgression(TArray<FBuildableCardTable>& InOutCards) const;
};
