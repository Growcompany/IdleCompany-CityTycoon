#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "DrawerPageToggleWidget.generated.h"

class UButtonWidget;
class UCommonButtonBase;
class UCommonButtonGroupBase;

/**
 * 국가 서랍의 생산/강화 토글 (UIE_DrawerPageToggle) — 어두운 홈 + 흰 손잡이 두 칸.
 * 모양만 토글이고 동작은 탭 배타선택 그대로다. 전환음은 소유 패널이 낸다.
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UDrawerPageToggleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_EVENT_OneParam(UDrawerPageToggleWidget, FPageChanged, CountryDrawer::EDrawerPage);
	FPageChanged& OnPageChanged() { return PageChanged; }

	void SetPage(CountryDrawer::EDrawerPage Page, bool bBroadcast);
	CountryDrawer::EDrawerPage GetPage() const { return CurrentPage; }

	void SetProductionDot(bool bShow);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ProductionSegment;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> UpgradeSegment;

	// 강화를 보는 동안 생산 쪽에 받을 것이 있으면 켜진다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> ProductionDot;

	// 칸 글자 잉크 ― 선택 = 흰 손잡이 위, 비선택 = 어두운 홈 위. 버튼은 선택에 따라 글자색을 바꾸지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerPageToggle|Ink")
	FSlateColor SelectedInk = FSlateColor(FLinearColor(FColor(0x1B, 0x23, 0x30)));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerPageToggle|Ink")
	FSlateColor UnselectedInk = FSlateColor(FLinearColor(FColor(0x8F, 0x9A, 0xAB)));

private:
	void ApplySegmentInk();

	UFUNCTION()
	void HandleSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UButtonWidget* SegmentFor(CountryDrawer::EDrawerPage Page) const;
	// 코드가 고른 선택은 방송하지 않는다
	void SelectSegment(CountryDrawer::EDrawerPage Page);

	UPROPERTY()
	TObjectPtr<UCommonButtonGroupBase> PageGroup;

	FPageChanged PageChanged;
	CountryDrawer::EDrawerPage CurrentPage = CountryDrawer::EDrawerPage::Production;
	bool bApplyingPage = false;
};
