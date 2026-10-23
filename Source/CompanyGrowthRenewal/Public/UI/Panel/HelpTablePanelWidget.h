#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/Element/Common/HelpTableProvider.h"
#include "HelpTablePanelWidget.generated.h"

class UButton;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UCloseButtonWidget;
class UCommonTextBlock;
class UHorizontalBox;
class UVerticalBox;

/**
 * 도움말 표 창 ― 설계 R2 §6.2.
 *
 * 말풍선이 아니라 창이다. 말풍선에 표를 넣으면 앵커 옆 좁은 폭에 갇힌다.
 * 축을 창 안에서 바꾼다 ― 등급은 페이저, 보는 항목은 탭. 창을 닫았다 다시 열 일이 없다.
 *
 * 내용은 이 위젯이 만들지 않는다. 소유 패널이 IHelpTableProvider 로 넘겨준다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UHelpTablePanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Push 직후 호출. Caption 은 DT_Help.Body ― 규칙과 숫자를 한 창에서 같이 보여준다
	void Configure(UObject* InProvider, FName InHelpKey, const FText& InCaption);

	// 창이 떴는지가 아니라 숫자가 그려졌는지를 묻는 검증용. 0 이면 표가 비어 있다
	int32 GetRenderedRowCount() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText = nullptr;

	// 표가 들어갈 자리. 행 위젯은 C++ 이 만들어 채운다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> RowBox = nullptr;

	// 등급 페이저 ― 축이 1개 이하면 통째로 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PagerBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PrevButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> NextButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PageText = nullptr;

	// 항목 탭 ― 축이 1개 이하면 Collapsed. 버튼은 C++ 이 만들어 채운다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> AxisBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CaptionText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> FootnoteText = nullptr;

	// UI_PotentialOdds 와 같은 부품을 쓴다 ― 확률 표 화면 둘이 서로 다른 닫기를 가지면 안 된다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> UIE_CloseButton = nullptr;

private:
	UFUNCTION()
	void HandlePrevClicked();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleAxisChanged(UCommonButtonBase* Button, int32 Index);

	void RebuildAxisTabs();
	void RefreshPager();
	void RefreshRows();

	UPROPERTY(Transient)
	TObjectPtr<UObject> ProviderObject = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonGroupBase> AxisGroup = nullptr;

	FHelpTableInfo Info;
	FName HelpKey;
	FText Caption;
	int32 PageIndex = 0;
	int32 AxisIndex = 0;
};
