// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "OfficeWorkstationPanelWidget.generated.h"

class UButtonWidget;
class UTableManagerSubsystem;
class UListView;

/**
 * 오피스 업무공간 배치/업그레이드 패널
 *
 * 기능:
 * - 업무공간(책상) 구매 및 배치
 * - 컴퓨터 세팅 레벨 업그레이드
 * - 장비 스킨 선택 (의자, 노트북, 모니터, 키보드마우스)
 * - Lv3 타입 선택 (A: 노트북+Curved, B: 듀얼Curved, C: Curved+Vertical)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeWorkstationPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

	UPROPERTY()
	UTableManagerSubsystem* TableManager;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnBackButtonClicked();

	// ========== UI 바인딩 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* BackButton;

	// 업무공간 카드 ListView
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UListView* WorkstationListView;

private:
	// 업무공간 카드 새로고침
	void RefreshWorkstationCards();
};
