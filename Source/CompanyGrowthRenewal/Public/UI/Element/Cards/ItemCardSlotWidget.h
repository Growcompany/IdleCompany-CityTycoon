// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Element/Cards/ItemCardWidget.h"
#include "ItemCardSlotWidget.generated.h"

class UCommonTextBlock;
class UTexture2D;
class UItemTooltipWidget;
class UCommonBorderStyle;

/**
 * UIE_ItemCard_QtyInside / UIE_ItemCard_QtyBelow 등 변형의 공통 베이스.
 * 카드 본체(UIE_ItemCard) + 수량 라벨 layout 묶음. 클릭은 안에 있는 카드가 받고 Slot 으로 forward.
 *
 * BindWidget 명명 규칙:
 *   - UIE_ItemCard : 임베드된 자식 WBP. 자동 명명 그대로 받음 → 디자이너 rename 불필요.
 *   - QuantityText : 라벨 텍스트. 디자이너가 외부 명패든 안쪽 오버레이든 자유 배치.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UItemCardSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetItem(UTexture2D* InIcon, int32 InQuantity);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIcon(UTexture2D* InIcon);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIconResource(UObject* InResource);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetQuantity(int32 InQuantity);

	// 자유 텍스트 라벨 (돈 축약/×수량 등 호출자가 포맷). QuantityText 에 직접 세팅.
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetLabel(const FText& InLabel);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetItemID(FName InItemID);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetLocked(bool bInLocked);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetInsufficient(bool bInInsufficient);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetSelected(bool bInSelected);

	// Forward: 내부 UIE_ItemCard 의 IconSize 변경. Product 카드(4:3) 동적 적용용.
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIconSize(FVector2D NewSize);

	// Forward: 카드 몸체/아이콘 크기 명시 리사이즈 (미리보기 스트립 등 축소 배치)
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetCardSize(FVector2D InCardSize, FVector2D InIconSize);

	// Forward: 아이콘 그림 곱색 (opt-in — 부르지 않으면 저작값 그대로)
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIconTint(FLinearColor InTint);

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	FLinearColor GetIconTint() const;

	// 라벨 폰트 크기 컨텍스트 조정 — 기본 WBP(18)는 밀집 그리드 기준, 배치처가 컨텍스트에 맞게 주입
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetLabelFontSize(int32 InSize);

	// 라벨 잉크 상태 전환용 — 되돌릴 저작값은 GetLabelColor 로 미리 읽어 둔다
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetLabelColor(FSlateColor InColor);

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	FSlateColor GetLabelColor() const;

	/** 수량 라벨의 공용 배경 스타일을 이 카드 인스턴스에만 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetQuantityBackgroundStyle(TSubclassOf<UCommonBorderStyle> InStyle);

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	UItemCardWidget* GetCardWidget() const { return UIE_ItemCard; }

	// 클릭 시 인라인 툴팁(이름/설명/아이콘/단가) 표시 활성화. 호출 전에는 클릭해도 툴팁 없음(opt-in).
	// 재화/아이템 등 표시 데이터는 호출자가 DT 에서 조회해 넘긴다 (슬롯은 재화/아이템 구분 모름).
	// InZOrder = 툴팁 viewport ZOrder. 기본 101. 보상 리빌 등 높은 오버레이 안에서는 host+1 을 넘긴다.
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetTooltipInfo(const FText& InName, const FText& InDesc, UObject* InIcon, int32 InPrice = 0, int32 InZOrder = 101);

	FOnItemCardClicked OnItemCardClicked;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 배치처가 컨텍스트 규격을 굽는 자리 — 0 이면 손대지 않는다(기존 소비자는 C++ 세터 경로 그대로).
	// 정적 스타일은 WBP 소유라는 규칙 때문에 카드 크기도 배치 인스턴스에 굽는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemCard|Layout", meta = (ClampMin = 0.0))
	FVector2D CardSize = FVector2D::ZeroVector;

	// 정사각 아이콘 한 변. 0 이면 카드 WBP 의 IconSize 를 그대로 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemCard|Layout", meta = (ClampMin = 0.0))
	float IconSize = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemCard|Layout", meta = (ClampMin = 0))
	int32 LabelFontSize = 0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UItemCardWidget> UIE_ItemCard;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> QuantityText;

private:
	// 저작값(CardSize/IconSize/LabelFontSize)을 카드에 흘려보낸다 — 0 인 항목은 건너뛴다
	void ApplyAuthoredLayout();

	// 클릭 시 1회 생성 후 재사용하는 인라인 툴팁 (UResourceWidget 패턴). bTooltipEnabled 일 때만 표시.
	void ShowTooltip();

	UPROPERTY(Transient)
	TObjectPtr<UItemTooltipWidget> ActiveTooltip;

	UPROPERTY(Transient)
	TObjectPtr<UObject> TooltipIcon;

	FText TooltipName;
	FText TooltipDesc;
	int32 TooltipPrice = 0;
	int32 TooltipZOrder = 101;
	bool bTooltipEnabled = false;
};
