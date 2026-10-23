#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/CompanyType.h"
#include "Enum/LootBoxRarity.h"
#include "Table/BuildableCardTable.h"
#include "BuildModalWidget.generated.h"

class UIndustryButtonWidget;
class UTabButtonWidget;
class UButtonWidget;
class UBuildEntityCardWidget;
class UModalFrameWidget;
class UTileView;
class UButton;
class UTableManagerSubsystem;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UCommonTextBlock;

/**
 * 빌드 모달 (Build 1.1) — 중앙 프롬프트 모달.
 * 산업 타일 6 + 건물 카드 가로 ListView (등급순 정렬 스크롤).
 * CTA/비용 없음 — 카드 탭 = 즉시 배치(구 BuildPanel UX 복원, 산업 선택만 추가).
 * 산업은 모달 진입 시 기본 선택(Game) → 카드 탭 시 함께 넘겨 배치 확정 시 건물에 CompanyType 베이크.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildModalWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	// 카드 표시·결제와 성장 안내가 공유하는 보유 채수별 건설비 계산.
	static void ApplyBuildCostProgression(TArray<FBuildableCardTable>& InOutCards, int32 OwnedBuildings);

	// 현재 표시된 해금·활성 건물 카드가 있을 때 자유 선택 목록을 안내 앵커로 제공한다.
	UWidget* GetBuildGuideCatalogWidget() const;
	// 가상화된 엔트리의 반환 순서 대신 목록의 실제 첫 항목에 연결된 카드를 찾는다.
	UWidget* GetBuildGuideFirstCardWidget() const;
	// 해금·구매 가능한 카드 중 수익 보너스가 가장 큰 카드 ― 본사 성장으로 막 열린 건물을 짚는다. 못 고르면 첫 카드
	UWidget* GetBuildGuideBestCardWidget(FText* OutName = nullptr) const;
	UWidget* GetBuildGuideIndustryWidget() const;
	// 튜토리얼 산업 지정 건설(M22 게임사·M30 IT) — 잠겼거나 안 보이면 nullptr
	UWidget* GetIndustryTileWidget(ECompanyType Industry) const;
	// 버튼 그룹에서 지금 골라진 산업(잠긴 타일이면 None)
	ECompanyType GetSelectedIndustry() const;
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
	// 공통 셸과 제목 영역은 필수 바인딩이다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UModalFrameWidget> ModalFrame;

	// 풀스크린 투명 딤 — 클릭 시 닫기 (ProductSellModal 패턴)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// 산업 미선택 시 카드 자리에 표시되는 빈 상태 안내("산업을 선택하세요"). 산업 선택 시 Collapsed.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UWidget* EmptyStatePrompt;

	// 건물 개수 칩 글자 ― Office 카탈로그 개수 칩과 같은 부품
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CatalogCount;

private:
	friend class FCGRBuildModalSelectionTest;

	// 건설 payload는 캐시가 아니라 버튼 그룹의 현재 선택 상태에서 결정한다.
	bool TryGetSelectedCompanyType(ECompanyType& OutCompanyType) const;

	// 산업 타일 배타선택 변경 핸들러 (UCommonButtonGroupBase)
	UFUNCTION()
	void OnIndustryTileSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

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

	// 목록을 다시 채운 뒤 개수 칩과 오른쪽 끝 흐림을 맞춘다
	void RefreshListChrome();

	void CloseModal();

	// 산업 타일 잠금 재평가 (매 오픈 — HQ 레벨은 모달이 열려 있는 동안 변하지 않음)
	void RefreshIndustryLocks();

	// 잠긴 타일은 토스트, 열린 타일의 실제 클릭은 미션에 알린다(산업 지정 튜토리얼이 목표 산업 클릭을 기다린다).
	void OnIndustryTileClicked(UIndustryButtonWidget* Tile);

	UPROPERTY()
	UTableManagerSubsystem* TableManager = nullptr;

	// 산업 타일 배타선택 그룹 (탭 정석 — NativeConstruct 셋업)
	UPROPERTY()
	UCommonButtonGroupBase* IndustryButtonGroup = nullptr;

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
