#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DrawerGoodsTileWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UTexture2D;

/**
 * 국가 서랍 무역항 「보관 중인 완제품」 4열 격자의 한 칸 (UIE_DrawerGoodsTile) — 그림 · 이름 · 개수.
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UDrawerGoodsTileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetGoods(UTexture2D* Icon, const FText& Name, int64 Quantity);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CountText;
};
