// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Table/DecorationCardTable.h"
#include "OfficeDecorationPanelWidget.generated.h"

class UButtonWidget;
class UTableManagerSubsystem;
class UListView;
class UCommonButtonGroupBase;
class AOfficeInterior;
class UEntityCardData;
class UOfficeDecorationCardWidget;

/**
 * 오피스 장식 배치 패널
 * - 8개 카테고리: 그림, 창문, 의자, 가구, 파티션, 벽, 화분, 바닥 타일
 * - CommonButtonGroup으로 탭 버튼 관리
 * - 각 카테고리별 WidgetSwitcher로 전환
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

	// 인덱스에 해당하는 카테고리 로드
	void LoadCategoryByIndex(int32 CategoryIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnBackButtonClicked();

	// 탭 선택 변경 이벤트 (CommonButtonGroup 델리게이트)
	UFUNCTION()
	void OnDecorationTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	// 카테고리별 카드 새로고침 (ListView 통합)
	void RefreshDecorationCards(EDecorationCategory Category);

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* BackButton;

	// 카테고리 탭 버튼들 (8개)
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

	// 장식 카드 ListView (모든 카테고리 공용)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UListView* DecorationListView;

	// 현재 선택된 카테고리
	EDecorationCategory CurrentCategory;

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
