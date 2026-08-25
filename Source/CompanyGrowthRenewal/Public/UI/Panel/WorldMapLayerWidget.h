// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "Entity/Country/CountryActor.h"
#include "WorldMapLayerWidget.generated.h"

class UResourceWidget;
class UCanvasPanel;
class UCountryNameWidget;
class UCanvasPanelSlot;
class UTextBlock;
class UButtonWidget;
class UTableManagerSubsystem;
class UTradeOrderBoardWidget;

/**
 * WorldMap 메인 레이어 위젯 (MainStack)
 *
 * 역할: 자원 표시, Canvas 기반 나라 이름표 배치, 좌측 무역 게시판
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldMapLayerWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// ========== 자원 표시 ==========
	void HandleResourceChanged(EResourceType Type, int64 NewValue);
	TMap<EResourceType, UResourceWidget*> ResourceWidgets;
	FDelegateHandle UIResourceChangedHandle;

	// ========== 나라 이름표 (Canvas 기반) ==========

	struct FCountryNameData
	{
		ACountryActor* CountryActor = nullptr;
		UCountryNameWidget* NameWidget = nullptr;
	};
	TArray<FCountryNameData> CountryNameDatas;

	// 나라 이름표 생성
	void CreateCountryNameWidgets();

	// 매 프레임 위치 업데이트
	void UpdateCountryNamePositions();

	// 국기 클릭 핸들러
	void HandleCountryFlagClicked(ECountryType CountryType);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Brick;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Diamond;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCanvasPanel* InGameCanvas;

	// 좌측 상시 무역 게시판
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTradeOrderBoardWidget> UIE_TradeBoard = nullptr;

private:
	UPROPERTY()
	TObjectPtr<UTableManagerSubsystem> TableMgr = nullptr;
};
