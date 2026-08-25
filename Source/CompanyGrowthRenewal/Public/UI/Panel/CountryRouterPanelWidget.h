// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Entity/Country/CountryActor.h"
#include "CountryRouterPanelWidget.generated.h"

class UScrollBox;
class UTextBlock;
class UCloseButtonWidget;
class UButton;
class UCountryRouterCardWidget;
struct FCountryInfoTable;

/**
 * 라우터 모드
 * 월드맵 하단바 버튼(자원/공장)이 이 enum으로 패널의 동작을 지정.
 */
UENUM(BlueprintType)
enum class ECountryRouterMode : uint8
{
	Resource UMETA(DisplayName = "채광"),   // bSupportsMine 필터, CountryDetail 채광 탭
	Factory  UMETA(DisplayName = "공장")    // bSupportsFactory 필터, CountryDetail 공장 탭
};

/**
 * CountryRouterPanelWidget
 * 자원/공장 하단바 버튼 공통 라우터 패널.
 * 모드 enum 하나로 필터/제목/점프탭이 결정됨 → 클래스 1개 + WBP 1개로 2가지 variation 처리.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCountryRouterPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 호출자(WorldMapBottomWidget)가 Push 직후 모드 지정
	UFUNCTION(BlueprintCallable, Category = "CountryRouter")
	void InitializeForMode(ECountryRouterMode InMode);

	UFUNCTION(BlueprintPure, Category = "CountryRouter")
	ECountryRouterMode GetMode() const { return Mode; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> CardScrollBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> CloseButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn;

private:
	ECountryRouterMode Mode = ECountryRouterMode::Resource;

	UPROPERTY()
	TArray<TObjectPtr<UCountryRouterCardWidget>> SpawnedCards;

	// Mode별 사양을 한 군데서 해결하는 분기
	struct FModeSpec
	{
		FText Title;
		int32 TargetTabIndex;
	};
	FModeSpec ResolveModeSpec() const;

	// DT 한 행이 현재 Mode 기준으로 노출 대상인지
	bool PassesFilter(const FCountryInfoTable& Info) const;

	void RebuildCardList();
	UCountryRouterCardWidget* SpawnCard(const FCountryInfoTable& Info);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleBackgroundClicked();
};
