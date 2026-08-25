// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/ResourceType.h"
#include "MinePickerWidget.generated.h"

class UScrollBox;
class UCommonTextBlock;
class UButton;
class UCloseButtonWidget;
class UMineResourceCardWidget;

/**
 * UMinePickerWidget
 * 채광 라인 추가 모달. 현재 국가의 풀에서 미활성 자원만 카드로 노출 → 사용자 선택 → CreateMineLine.
 *
 * 사용 흐름:
 *   1. CountryDetailWidget 의 MineCreateButton 클릭 → CreateWidget + AddToViewport(ZOrder 100)
 *      (PromptStack push 안 함 — CountryDetail 위에 모달 오버레이로 띄움)
 *   2. InitializeForCountry(Country) 호출
 *   3. 카드 클릭 → MineMgr->CreateMineLine → RemoveFromParent 로 닫힘
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMinePickerWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MinePicker")
	void InitializeForCountry(ECountryType Country);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TitleText;

	// 사용자 WBP (UI_MinePicker) 의 horizontal CommonHierarchicalScrollBox 변수와 매칭
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ResourceScrollBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> CloseButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn;

private:
	ECountryType CurrentCountry = ECountryType::None;

	UPROPERTY()
	TArray<TObjectPtr<UMineResourceCardWidget>> SpawnedCards;

	void RebuildResourceList();

	UFUNCTION()
	void HandleResourceSelected(EResourceType Resource, int64 Quantity);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleBackgroundClicked();

	// 비스택(AddToViewport) 모달이라 UIBase 스택 닫힘사운드 hook 이 안 닿음 — 직접 재생
	void PlayCloseSound();
};
