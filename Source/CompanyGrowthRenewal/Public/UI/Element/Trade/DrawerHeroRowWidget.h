#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/ResourceType.h"
#include "DrawerHeroRowWidget.generated.h"

class UBorder;
class UImage;
class UCommonTextBlock;
class UCostActionButtonWidget;
class UTexture2D;
class UCommonButtonStyle;

/**
 * 국가 서랍 시설 페이지 맨 위 줄 (UIE_DrawerHeroRow) — 요약 두 줄 + 큰 버튼 하나(모두 수령/모두 받기/모두 판매).
 * 흰 판 위라 글자는 WBP 가 Font + 잉크를 직접 갖는다(스케일 스타일 슬롯 금지).
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UDrawerHeroRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_EVENT(UDrawerHeroRowWidget, FDrawerHeroActionEvent);
	FDrawerHeroActionEvent& OnActionClicked() { return ActionClicked; }

	void SetSummary(const FText& Title, const FText& Detail);

	// Amount 가 0 이하면 받을/팔 것이 없다 ― 버튼을 끄고 수량 칩을 접는다.
	void SetAction(const FText& Label, EResourceType AmountIcon, int64 Amount);

	// 왼쪽 시설 칸 — 공장 = 산업 글리프·대표색, 광산 = 자원 아이콘·갈색, 무역항 = 상자·청록.
	void SetFacilityTile(UTexture2D* Icon, FLinearColor TileColor);

	// 시설별 버튼 색 ― 무역항(WBP 기본) = Green, 공장·광산 인스턴스 = Gold. 비우면 저작값 그대로
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	TSubclassOf<UCommonButtonStyle> ActionButtonStyle;

	// 버튼 색에 맞는 라벨 잉크(골드 위 = 진한 잉크 #3B2700)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FSlateColor ActionLabelInk = FSlateColor(FLinearColor(FColor(0xFF, 0xF8, 0xF0)));

	// 받을 것이 없을 때 회색 판 위 라벨(카탈로그 §6 라이트 면 규칙)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FSlateColor DisabledLabelInk = FSlateColor(FLinearColor(FColor(0x1B, 0x23, 0x30)));

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DetailText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCostActionButtonWidget> ActionButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> FacilityTile;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> FacilityIcon;

private:
	void HandleActionClicked();
	// 자식 버튼의 PreConstruct 가 이미 지나간 뒤라 속성만이 아니라 안쪽 버튼·라벨에 직접 입힌다
	void ApplyActionStyle();
	void ApplyActionLabelInk(const FSlateColor& Ink);

	FDrawerHeroActionEvent ActionClicked;
	bool bWarnedMissingBindings = false;
	// 바깥 버튼은 SetEnabled 가 안쪽 Btn 만 끄므로 재구성 때 잉크를 고를 근거를 따로 쥔다
	bool bActionEnabled = true;
};
