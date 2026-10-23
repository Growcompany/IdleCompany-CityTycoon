#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "EmployeeRotateRingWidget.generated.h"

class UTexture2D;

/**
 * 사무실 직원 패널 장비 탭 — 고른 직원 발밑의 「끌어서 돌리기」 바닥 고리.
 * 눕힌 타원(뒤쪽 호는 옅게) + 바깥 번짐 + 양 끝 굽은 화살표 + 접지 그림자. 타원은 이 위젯 상자에 내접한다.
 * 위치·크기는 호스트가 발 투영점에 슬롯으로 정하고, 그리기는 자기 로컬 공간에서만 한다(플레이북 §3 「모양과 위치 분리」).
 * 스스로 돌지 않는다 — 끄는 동안만 화살표·고리가 밝아진다.
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEmployeeRotateRingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDragging(bool bInDragging);
	bool IsDragging() const { return bDragging; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// 제스처 안내 팩 Rotate_Left/Right — 흰 채움 + 검정 테라 틴트하면 채움만 물든다
	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	TObjectPtr<UTexture2D> ArrowLeftTexture;

	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	TObjectPtr<UTexture2D> ArrowRightTexture;

	// 흰 부드러운 타원(Glow_Oval) — 검정 틴트로 발밑을 눌러 고리를 바닥에 붙인다
	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	TObjectPtr<UTexture2D> ShadowTexture;

	// #8CC4FF — 고른 부위 지시선·장착 빛 고리와 같은 파랑
	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	FLinearColor RingColor = FLinearColor(0.262251f, 0.552011f, 1.0f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	FLinearColor ArrowColor = FLinearColor(0.262251f, 0.552011f, 1.0f, 0.92f);

	// 끄는 동안 — #DDEEFF
	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	FLinearColor ArrowDragColor = FLinearColor(0.723055f, 0.854993f, 1.0f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	FLinearColor ShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	float RingThickness = 4.f;

	// 카메라에서 먼 위쪽 호 — 앞쪽보다 옅게 그려 바닥에 누운 원으로 읽히게
	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	float BackArcOpacity = 0.5f;

	// 화살표 높이 (폭은 텍스처 비율)
	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	float ArrowHeight = 40.f;

	// 그림자 크기 = 타원 상자 × 이 배율
	UPROPERTY(EditAnywhere, Category = "Rotate Ring")
	FVector2D ShadowScale = FVector2D(1.1f, 1.5f);

private:
	FSlateBrush ArrowLeftBrush;
	FSlateBrush ArrowRightBrush;
	FSlateBrush ShadowBrush;
	bool bDragging = false;
};
