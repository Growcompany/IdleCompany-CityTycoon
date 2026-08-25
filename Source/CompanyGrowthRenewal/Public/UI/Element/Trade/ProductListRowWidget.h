// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProductListRowWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UTexture2D;

/**
 * UIE_ProductListRow
 * CountryDetail 정보 탭의 해금 제품 리스트에서 한 줄을 차지하는 행 위젯.
 *
 * Name + Quantity 표시, Icon은 옵셔널 (있으면 set, 없으면 무시).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UProductListRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ProductRow")
	void SetProductData(const FText& InProductName, int32 InQuantity);

	// 아이콘 옵셔널 — 나중에 ProductRecipe에서 가져와서 set할 때 호출
	UFUNCTION(BlueprintCallable, Category = "ProductRow")
	void SetProductIcon(UTexture2D* InIcon);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ProductIconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ProductNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> QuantityText;

	// 수량 텍스트 포맷 (예: "{0}개")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProductRow")
	FText QuantityFormat = NSLOCTEXT("ProductRow", "QtyFmt", "{0}개");
};
