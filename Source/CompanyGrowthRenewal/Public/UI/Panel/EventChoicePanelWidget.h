#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/ProjectEventData.h"
#include "EventChoicePanelWidget.generated.h"

class UCommonTextBlock;
class UEventChoiceCardWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventChoiceMade, int32, ChoiceIndex);

/**
 * 3선택지 이벤트 모달 (UI_EventChoicePanel WBP에 부착)
 * - WBP 구조: BackgroundImage / TitleText (이벤트 제목) / HorizontalBox 안 UIE_EventChoiceCard 3개
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEventChoicePanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Event")
	FOnEventChoiceMade OnChoiceMade;

	UFUNCTION(BlueprintCallable, Category = "Event")
	void SetEventData(const FProjectEventData& EventData);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// === WBP UI_EventChoicePanel BindWidget (이름 일치) ===

	// 이벤트 제목 (WBP의 TitleText)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TitleText;

	// 카드 3장 (WBP에 배치된 UIE_EventChoiceCard 인스턴스 이름)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEventChoiceCardWidget* UIE_EventChoiceCard;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEventChoiceCardWidget* UIE_EventChoiceCard_1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEventChoiceCardWidget* UIE_EventChoiceCard_2;

private:
	UFUNCTION()
	void HandleCardSelected(int32 ChoiceIndex);
};
