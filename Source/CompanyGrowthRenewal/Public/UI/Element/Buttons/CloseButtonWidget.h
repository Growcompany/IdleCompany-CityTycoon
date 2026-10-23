// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CloseButtonWidget.generated.h"

class UButton;
class USizeBox;

/**
 * 재사용 가능한 닫기 버튼 위젯
 * - 각 패널에서 OnCloseClicked 델리게이트에 바인딩하여 사용
 * - 보이는 크기(GlyphExtent)와 터치 영역(TouchExtent)이 독립 축이다
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UCloseButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;

	// 닫기 버튼 클릭 델리게이트
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCloseClicked);

	UPROPERTY(BlueprintAssignable, Category = "CloseButton")
	FOnCloseClicked OnCloseClicked;

protected:
	UPROPERTY(meta = (BindWidget))
	USizeBox* RootSizeBox;

	UPROPERTY(meta = (BindWidget))
	UButton* CloseBtn;

	// 글리프 전용 박스. 구조 이관 중인 WBP 가 있어 Optional — 없으면 GlyphExtent 만 무시된다
	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* GlyphSize;

	// 보이는 X 한 변 = 레이아웃 점유 크기. cross_thick 캔버스가 104 라 초과하면 확대되어 흐려진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloseButton", meta = (ClampMin = "32", ClampMax = "104"))
	float GlyphExtent = 104.0f;

	// 히트 영역 한 변. 차액은 음수 패딩이 흡수하므로 부모가 보는 크기는 GlyphExtent 그대로다 (Material 3 하한 154)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CloseButton", meta = (ClampMin = "104"))
	float TouchExtent = 154.0f;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleCloseButtonClicked();
};
