// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/WidgetType.h"
#include "UI/Panel/CountryRouterPanelWidget.h"
#include "WorldMapBottomWidget.generated.h"

class UButton;
class UButtonWidget;
class UIconWithButtonWidget;

/**
 * WorldMap 하단 네비게이션 위젯 (BottomStack)
 *
 * 역할:
 *  - 메인맵 복귀 버튼
 *  - 자원/공장/완성품 패널 호출 액션바 (각각 우측 슬라이드인)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldMapBottomWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION()
	void OnHomeBtnClicked();

	UFUNCTION()
	void OnResourcePanelClicked();

	UFUNCTION()
	void OnFactoryPanelClicked();

	UFUNCTION()
	void OnProductsPanelClicked();

	void OnCollectAllClicked();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> HomeBtn;

	// 액션바 3개 (UButtonWidget — UCommonButtonBase native 이벤트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> ResourcePanelBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> FactoryPanelBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> ProductsPanelBtn = nullptr;

	// 수집 버튼 — 채광 saturated + 공장 completed 라인 모두 수령 + 토스트로 요약 표시.
	// 항상 활성 (disabled 회색 처리 안 함). 수령 가능 라인 없을 때는 "수집할 게 없습니다" 토스트.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UIconWithButtonWidget> CollectAllButton = nullptr;

private:
	void PushPanel(EWidgetType PanelType);
	void PushRouterPanel(ECountryRouterMode Mode);
};
