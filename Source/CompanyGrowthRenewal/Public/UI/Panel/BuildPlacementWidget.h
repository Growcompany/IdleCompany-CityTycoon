// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Table/BuildableCardTable.h"
#include "Table/WorkstationCardTable.h"
#include "Table/DecorationCardTable.h"
#include "BuildPlacementWidget.generated.h"

class UButtonWidget;
class APlayerCamera;
class UCommonBorder;

/**
 *
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildPlacementWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

	APlayerCamera* Player = nullptr;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnPlaceButtonClicked();
	UFUNCTION()
	void OnRotateButtonClicked();
	UFUNCTION()
	void OnCancelButtonClicked();

	void CheckHasConstructionCost() const;
	void SpendConstructionCost() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonBorder* RootBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* PlaceButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* RotateButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* CancelButton;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FBuildableCardTable BuildableInfo;

	void SetBuildableInfo(const FBuildableCardTable& buildableInfo, bool bIsMoving = false);

	// 업무공간 정보 설정 (Office Map용)
	UFUNCTION(BlueprintCallable, Category = "Workstation")
	void SetWorkstationInfo(const FWorkstationCardTable& InWorkstationInfo);

	// 장식품 정보 설정 (Office Map용)
	UFUNCTION(BlueprintCallable, Category = "Decoration")
	void SetDecorationInfo(const FDecorationCardTable& InDecorationInfo);

private:
	// 기존 건물 재배치 모드인지 여부
	bool bIsMovingExistingBuilding = false;

	// 업무공간 모드인지 여부
	bool bIsWorkstationMode = false;

	// 장식품 모드인지 여부
	bool bIsDecorationMode = false;

	// 업무공간 정보
	FWorkstationCardTable WorkstationInfo;

	// 장식품 정보
	FDecorationCardTable DecorationInfo;

	// 장식품 구매 비용 지불
	void SpendDecorationCost() const;
};
