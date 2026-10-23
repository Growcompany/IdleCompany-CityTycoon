// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Table/DecorationCardTable.h"
#include "Table/DecorationCatalogTable.h"
#include "OfficeDecorationPanelWidget.generated.h"

class UButtonWidget;
class UCloseButtonWidget;
class UTabButtonWidget;
class UTableManagerSubsystem;
class UListView;
class UCommonButtonGroupBase;
class AOfficeInterior;
class UEntityCardData;
class UOfficeDecorationCardWidget;
class UCommonTextBlock;
class UCommonButtonBase;

/**
 * 오피스 장식 배치 패널
 * - 큰 분류와 세부 필터를 별도 CommonButtonGroup으로 관리
 * - 카드 클릭은 기존 배치 경로로 바로 연결
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeDecorationPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

	UPROPERTY()
	UTableManagerSubsystem* TableManager;

	// 카테고리 탭 버튼 그룹 (벽/바닥/가구/타일)
	UPROPERTY()
	UCommonButtonGroupBase* DecorationTabButtonGroup;

	UPROPERTY() UCommonButtonGroupBase* CatalogGroupButtons;
	UPROPERTY() TArray<FDecorationCatalogRow> CatalogRows;
	TArray<FName> CatalogGroups;
	int32 CurrentGroupIndex = 0;
	int32 CurrentFilterIndex = 0;
	void RefreshCatalog();
	void UpdateCategoryFilters();
	TArray<UButtonWidget*> GetCategoryButtons() const;
	const FDecorationCatalogRow* FindCategory(EDecorationCategory Category) const;

public:
	UWidget* GetCloseButtonWidget() const;
	// 튜토리얼 M20 — 카테고리가 지금 목록에 나오는가(RefreshCatalog 의 그룹·세부 필터 거르기와 같은 규칙)
	bool IsCategoryListed(EDecorationCategory Category) const;
	// 그 카테고리로 가는 다음 탭 — 다른 그룹이면 그룹 탭, 같은 그룹의 다른 세부 필터면 그 카테고리 버튼, 이미 보이면 nullptr
	UWidget* GetCategoryNavigationWidget(EDecorationCategory Category) const;
	// 생성된 카드 중 지금 자금으로 살 수 있는 가장 싼 카드. 가상화로 아직 안 만들어진 카드는 짚지 못한다
	UWidget* GetCheapestAffordableCardWidget(EDecorationCategory Category) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnBackButtonClicked();
	UFUNCTION() void OnRelocateClicked();
	bool bPendingPlacementEdit = false;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* RelocateButton;

	// 탭 선택 변경 이벤트 (CommonButtonGroup 델리게이트)
	UFUNCTION()
	void OnDecorationTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);
	UFUNCTION()
	void OnCatalogGroupChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) UTabButtonWidget* GroupTab0;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) UTabButtonWidget* GroupTab1;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) UTabButtonWidget* GroupTab2;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) UTabButtonWidget* GroupTab3;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) UButtonWidget* AllTab;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) UCommonTextBlock* CatalogCount;


	// 공용 닫기 X (UIE_CloseButton)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> BackButton;

	// 카테고리 탭 버튼들
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* PictureTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* WindowTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* ChairTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* FurnitureTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* PartitionTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* WallTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* PlantTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* FloorTileTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* CarpetTab;

	// 장식 카드 ListView (모든 카테고리 공용)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UListView* DecorationListView;

	// 인덱스 → 카테고리 변환
	EDecorationCategory IndexToCategory(int32 Index) const;

private:
	// 현재 선택된 FloorTile RowName (OfficeInterior에서 적용된 타일)
	FName SelectedFloorTileRowName;

	// OfficeInterior 캐시
	UPROPERTY()
	AOfficeInterior* CachedOfficeInterior;

	// ListView Entry 생성 이벤트 핸들러 (non-UFUNCTION for reference parameter)
	void OnFloorTileEntryGenerated(UUserWidget& EntryWidget);

	// FloorTile 카드 선택 이벤트 수신
	UFUNCTION()
	void OnFloorTileCardSelected(const FDecorationCardTable& CardData);

	// 모든 FloorTile 카드 선택 해제
	void DeselectAllFloorTileCards();
};
