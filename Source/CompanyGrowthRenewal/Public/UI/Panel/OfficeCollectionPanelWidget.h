// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "OfficeCollectionPanelWidget.generated.h"

class UButton;
class UCommonButtonGroupBase;
class UCommonHierarchicalScrollBox;
class UHorizontalBox;
class UButtonWidget;
class UTableManagerSubsystem;

/**
 * 프로젝트 도감 패널 (구 OfficeStagePanelWidget)
 * - 자체개발로 성공한 프로젝트만 티어별로 표시
 * - 각 티어 범위 필터 (1-10, ..., 91-100)
 * - Back 버튼으로 닫기
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeCollectionPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* BackButton;

	// 티어 범위 필터 버튼들 (10개 단위, Project Index 기준)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage1to10Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage11to20Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage21to30Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage31to40Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage41to50Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage51to60Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage61to70Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage71to80Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage81to90Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* Stage91to100Btn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonHierarchicalScrollBox* ProjectScrollBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* ProjectItemBox;

private:
	UPROPERTY()
	UTableManagerSubsystem* TableManager;

	UPROPERTY()
	UCommonButtonGroupBase* StageFilterButtonGroup;

	int32 CurrentStageFilter = 0;

	UFUNCTION()
	void OnBackgroundClicked();

	UFUNCTION()
	void OnBackButtonClicked();

	UFUNCTION()
	void OnStageFilterSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	void RefreshProjectCards();

	UFUNCTION()
	void OnProjectCardStartClicked(int32 ProjectIndex);
};
