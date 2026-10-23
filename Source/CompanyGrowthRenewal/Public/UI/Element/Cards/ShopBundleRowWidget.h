#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/ItemType.h"
#include "ShopBundleRowWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UHorizontalBox;
class UBorder;
class UOverlay;
class UImage;

/**
 * 패키지 카드의 구성품 한 줄 (아이콘 · 이름 · 수량).
 * ShopItemCard 가 FShopItemTable::BundleItems 개수만큼 만들어 BundleList 에 채운다.
 *
 * 트리는 C++ 자가 구성이다 — stock Python 이 WidgetTree.RootWidget 를 안 열어 줘서
 * WBP 를 무인 생성할 수 없다(2026-09-24 실측: 읽기도 쓰기도 protected).
 * 디자이너가 나중에 같은 이름으로 트리를 넣으면 BindWidgetOptional 이 그걸 잡아 그쪽이 이긴다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UShopBundleRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 아이콘/이름은 EItemType 으로 역참조 — 패키지 행에 별도 데이터를 두지 않는다 */
	void SetEntry(EItemType Item, int32 Quantity);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* ItemIcon;

	/** 아이콘 웰. 아이콘을 못 찾으면 이것째 접어 빈 막대가 안 남게 한다 */
	UPROPERTY(Transient)
	class USizeBox* IconWellBox = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonTextBlock* ItemNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonTextBlock* ItemQtyText;

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float IconSize = 36.0f;

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float NameFontSize = 21.0f;

	/** 수량 칸 최소 폭. 이름이 길어도 이만큼은 수량이 먼저 가져간다 */
	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float QtyMinWidth = 56.0f;

	/** #1B2330 — 쿨 화이트 카드 위 잉크 (linear) */
	UPROPERTY(EditAnywhere, Category = "Layout")
	FLinearColor InkColor = FLinearColor(0.0103f, 0.0168f, 0.0296f, 1.0f);

	/** #2F7FCC — 수량은 블루로 이름과 갈라 읽힌다 (linear) */
	UPROPERTY(EditAnywhere, Category = "Layout")
	FLinearColor QtyColor = FLinearColor(0.0284f, 0.2122f, 0.6038f, 1.0f);

	/** #EAF0F6 — 흰 카드 면보다 한 단 어두워야 행이 판으로 읽힌다 */
	UPROPERTY(EditAnywhere, Category = "Layout")
	FLinearColor PlateColor = FLinearColor(0.8228f, 0.8714f, 0.9216f, 1.0f);

	/** 타일 아래 두께. 이게 없으면 그림자를 줘도 얹힌 느낌이 안 난다 (#8D99AB A0.30) */
	UPROPERTY(EditAnywhere, Category = "Layout")
	FLinearColor RidgeColor = FLinearColor(0.2664f, 0.3185f, 0.4072f, 0.30f);

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float RidgeDepth = 3.0f;

	/** #C4CDD9 A0.60 — 판 안쪽 링, 카드 테두리와 같은 회청색 (linear) */
	UPROPERTY(EditAnywhere, Category = "Layout")
	FLinearColor PlateRingColor = FLinearColor(0.5520f, 0.6105f, 0.6939f, 0.60f);

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float PlateRadius = 12.0f;

private:
	/** 디자이너 트리가 없을 때만 코드가 만든다. 이미 있으면 그대로 둔다 */
	void BuildSelfTree();

	/**
	 * ⚠ 자가 구성 트리는 CreateWidget 시점에 아직 없다 — 트리는 RebuildWidget 에서 만들어진다.
	 * 그래서 SetEntry 는 값만 받아 두고, 트리가 생긴 뒤 여기서 다시 밀어 넣는다.
	 * (디자이너 트리 위젯은 Initialize 가 트리를 복제해 줘서 이 문제가 없다)
	 */
	void ApplyEntry();

	static void ApplyInk(class UCommonTextBlock* Text, const FLinearColor& Ink, float Size, bool bBold);

	EItemType PendingItem = EItemType::None;
	int32 PendingQuantity = 0;
};
