// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/RawMaterialType.h"
#include "Entity/Country/CountryActor.h"
#include "WorldResourcePanelWidget.generated.h"

class UTextBlock;
class UButton;
class UCloseButtonWidget;
class UVerticalBox;
class UWorldMapManager;
class UTableManagerSubsystem;

/**
 * 자원 패널 (우측 슬라이드인)
 *
 * 10종 원자재 + 에너지 + 정제유 표시.
 * 항목 클릭 시 주력 산출국으로 카메라 포커스 + CountryDetail 패널 열림.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldResourcePanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> ResourceContainer = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnergyText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RefinedOilText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> UIE_CloseButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

public:
	// 자원 행 위젯에서 호출. 주력 산출국으로 카메라 포커스 + CountryDetail 호출
	UFUNCTION(BlueprintCallable, Category = "WorldResourcePanel")
	void JumpToCountry(ERawMaterialType Mat);

private:
	UPROPERTY()
	TObjectPtr<UWorldMapManager> WorldMapMgr = nullptr;

	UPROPERTY()
	TObjectPtr<UTableManagerSubsystem> TableMgr = nullptr;

	UPROPERTY()
	TMap<ERawMaterialType, TObjectPtr<UTextBlock>> MaterialRowMap;

	void BuildRows();
	void RefreshAllValues();
	void SetMaterialRowText(ERawMaterialType Mat, int64 Amount);

	UFUNCTION() void HandleMaterialChanged(ERawMaterialType Mat, int64 NewAmount);
	UFUNCTION() void HandleEnergyChanged(int64 NewEnergy);
	UFUNCTION() void HandleRefinedOilChanged(int64 NewOil);

	UFUNCTION() void OnCloseButtonClicked();
	UFUNCTION() void OnBackgroundClicked();
};
