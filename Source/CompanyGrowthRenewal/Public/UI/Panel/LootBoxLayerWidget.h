// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/LootBoxCategory.h"
#include "Enum/LootBoxRarity.h"
#include "LootBoxLayerWidget.generated.h"

class UButtonWidget;
class UHorizontalBox;
class UCommonTextBlock;
class UIconButtonWidget;
class UTableManagerSubsystem;

// 룩박스 선택 시 호출되는 델리게이트
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLootBoxSelected, FName /*LootBoxID*/);

// 룩박스 오픈 시 호출되는 델리게이트 (LootBoxID, RewardSkinID)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLootBoxOpening, FName /*LootBoxID*/, int32 /*RewardSkinID*/);

/**
 * LootBoxMap 전용 레이어 위젯
 * - 카테고리별 룩박스 목록 표시
 * - 보유 개수 표시 및 오픈 처리
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ULootBoxLayerWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 룩박스가 선택되었을 때 브로드캐스트 (3D 모델 변경용)
	FOnLootBoxSelected OnLootBoxSelected;

	// 룩박스 오픈이 시작될 때 브로드캐스트 (애니메이션 재생용)
	FOnLootBoxOpening OnLootBoxOpening;

protected:
	// ===== Blueprint Widgets =====

	// 뒤로가기 버튼 (메인맵으로 복귀)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* BackButton;

	// 룩박스 리스트 컨테이너 (6개를 가로로 배치)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* LootBoxContainer;

	// 카테고리 타이틀 텍스트 (선택사항)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CategoryTitleText;

	// 상자 열기 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* OpenButton;

	// 상자 구매 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* PurchaseButton;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

private:
	// 현재 표시 중인 카테고리
	ELootBoxCategory CurrentCategory = ELootBoxCategory::BuildingSkin;

	// 현재 선택된 룩박스 ID
	FName SelectedLootBoxID;

	// TableManager 참조
	UPROPERTY()
	UTableManagerSubsystem* TableMgr = nullptr;

	// IconButtonWidget Blueprint 클래스 (TableManager에서 자동 로드)
	UPROPERTY()
	TSubclassOf<UIconButtonWidget> IconButtonClass;

	// 생성된 IconButtonWidget 맵 (LootBoxID -> Widget)
	UPROPERTY()
	TMap<FName, UIconButtonWidget*> LootBoxButtons;

	// 현재 선택된 버튼 (선택 표시용)
	UPROPERTY()
	UIconButtonWidget* CurrentSelectedButton = nullptr;

	// 버튼 이벤트 바인딩
	void BindButtonEvents();
	void UnbindButtonEvents();

	// 특정 룩박스의 개수만 업데이트 (효율적)
	void UpdateLootBoxCount(FName LootBoxID, int32 NewCount);

	// 뒤로가기 버튼 클릭 핸들러
	UFUNCTION()
	void OnBackButtonClicked();

	// 룩박스 버튼 클릭 핸들러 (선택만 함)
	UFUNCTION()
	void OnLootBoxButtonClicked(FName LootBoxID);

	// 상자 열기 버튼 클릭 핸들러
	UFUNCTION()
	void OnOpenButtonClicked();

	// 상자 구매 버튼 클릭 핸들러 (가챠)
	UFUNCTION()
	void OnPurchaseButtonClicked();

	// 보상 추첨 (상자를 열었을 때) - 숫자 ID 반환
	int32 RollReward(FName LootBoxID);

public:
	// 카테고리 설정 및 UI 갱신
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void SetCategory(ELootBoxCategory Category);

	// 룩박스 목록 갱신
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void RefreshLootBoxList();

	// 특정 룩박스 오픈 처리
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void OnLootBoxOpened(FName LootBoxID);

	// UI 숨기기 (애니메이션 중)
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void HideUIForAnimation();

	// UI 보이기 (애니메이션 완료)
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void ShowUIAfterAnimation();
};
