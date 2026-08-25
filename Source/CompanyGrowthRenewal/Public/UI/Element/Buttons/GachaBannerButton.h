// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "GachaBannerButton.generated.h"

class UImage;
class UTextBlock;

/**
 * 가챠 배너 셀 버튼 — 티켓 아이콘 + 이름(ButtonText 상속) + 서브 한 줄.
 * 셀 = 버튼 자신(UCommonButtonBase 파생 유지 → 패널 BindWidget 계약 무변경).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGachaBannerButton : public UButtonWidget
{
	GENERATED_BODY()

public:
	// 숫자만 넣는다 — 티켓 종류는 좌측 TierIcon 이 이미 보여줘서 아이콘을 또 붙이면 중복이다
	void SetOwnedCount(int32 Count);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* TierIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* SubText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* CountText;

	// 배지 전체(판+숫자+단위). 수량을 주입하지 않는 패널(특성/스킨)에서 "0장"이 붙지 않게 기본은 숨김
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* CountPlate;

	// 배너별 정적 값 — 디자이너 인스턴스 지정 (아이콘 텍스처/표시 크기/서브 문구)
	UPROPERTY(EditAnywhere, Category = "Banner")
	UTexture2D* TierIconTexture = nullptr;

	UPROPERTY(EditAnywhere, Category = "Banner")
	FVector2D TierIconSize = FVector2D(56.f, 56.f);

	UPROPERTY(EditAnywhere, Category = "Banner")
	FText SubTextValue;

	virtual void NativePreConstruct() override;
};
