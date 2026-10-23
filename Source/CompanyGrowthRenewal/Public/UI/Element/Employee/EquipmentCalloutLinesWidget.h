#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentCalloutLinesWidget.generated.h"

// 고른 장비 칸 → 몸 부위 곡선 하나. 점은 전부 이 위젯의 로컬 공간
struct FEquipmentCallout
{
	FVector2D Start = FVector2D::ZeroVector;    // 칸 안쪽 가장자리
	FVector2D Control = FVector2D::ZeroVector;  // 2차 베지어 조절점
	FVector2D End = FVector2D::ZeroVector;      // 곡선 끝 (앵커 바로 앞)
	FVector2D Anchor = FVector2D::ZeroVector;   // 부위 (후광·끝 점 중심)
};

/**
 * 사무실 직원 패널 장비 탭의 부위 곡선·후광·장착 빛 고리 (설계 §5, 승인 목업 안 B).
 * 월드 레이어를 덮도록 슬롯 배치되고, 호스트가 같은 레이어 좌표계로 계산한 점을 받아 자기 로컬 공간에 그린다
 * — 루트 NativePaint 의 절대좌표 역변환 함정(플레이북 §3 「월드 앵커 UI 배치」)을 피한다.
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEquipmentCalloutLinesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCallout(const FEquipmentCallout& InCallout);
	void ClearCallout();

	// 장착 순간 빛 고리 — Scale 은 1.0 이하(작았다가 원 크기로), Opacity 0..1
	void SetFlash(const FVector2D& Center, float Scale, float Opacity);
	void ClearFlash();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// 목업 .wsoftB .ln — 지름 7 둥근 구슬을 17.1 간격(dasharray 0.1 17 + round cap)으로, drop-shadow 0 0 4px rgba(80,160,255,.9)
	UPROPERTY(EditAnywhere, Category = "Callout")
	FLinearColor BeadColor = FLinearColor::FromSRGBColor(FColor(0xCF, 0xE8, 0xFF));

	UPROPERTY(EditAnywhere, Category = "Callout")
	FLinearColor BeadGlowColor = FLinearColor::FromSRGBColor(FColor(80, 160, 255));

	UPROPERTY(EditAnywhere, Category = "Callout")
	float BeadDiameter = 7.f;

	UPROPERTY(EditAnywhere, Category = "Callout")
	float BeadSpacing = 17.1f;

	// 번짐 두 겹이 구슬 가장자리 밖으로 나가는 폭과 알파
	UPROPERTY(EditAnywhere, Category = "Callout")
	FVector2D BeadGlowReach = FVector2D(2.f, 4.f);

	UPROPERTY(EditAnywhere, Category = "Callout")
	FVector2D BeadGlowAlpha = FVector2D(0.34f, 0.18f);

	// 목업 .wsoftB .end — 앵커 흰 점 r6
	UPROPERTY(EditAnywhere, Category = "Callout")
	float EndDotRadius = 6.f;

	// 목업 radialGradient r64 — #7CC0FF α.95 → (50%) #3D9BE0 α.45 → #5AA8FF α0
	UPROPERTY(EditAnywhere, Category = "Callout")
	float HaloRadius = 64.f;

	UPROPERTY(EditAnywhere, Category = "Callout")
	FLinearColor HaloInnerColor = FLinearColor::FromSRGBColor(FColor(0x7C, 0xC0, 0xFF)).CopyWithNewOpacity(0.95f);

	UPROPERTY(EditAnywhere, Category = "Callout")
	FLinearColor HaloMidColor = FLinearColor::FromSRGBColor(FColor(0x3D, 0x9B, 0xE0)).CopyWithNewOpacity(0.45f);

	UPROPERTY(EditAnywhere, Category = "Callout")
	FLinearColor HaloOuterColor = FLinearColor::FromSRGBColor(FColor(0x5A, 0xA8, 0xFF)).CopyWithNewOpacity(0.f);

	// 원판을 겹쳐 방사 그라데이션을 만드는 겹 수 (SDF 가장자리가 겹 경계를 부드럽게 한다)
	UPROPERTY(EditAnywhere, Category = "Callout")
	int32 HaloSteps = 20;

	// 장착 순간 고리 색 (목업 .wflash)
	UPROPERTY(EditAnywhere, Category = "Callout")
	FLinearColor FlashColor = FLinearColor(0.262251f, 0.552011f, 1.0f, 1.f);

	// 목업 .wflash — 지름 150 고리(테 5 + 바깥 번짐) · 가운데 작은 흰 점
	UPROPERTY(EditAnywhere, Category = "Callout")
	float FlashRadius = 72.f;

	UPROPERTY(EditAnywhere, Category = "Callout")
	float FlashThickness = 5.f;

	UPROPERTY(EditAnywhere, Category = "Callout")
	float FlashDotRadius = 6.f;

private:
	bool bHasCallout = false;
	FEquipmentCallout Callout;
	bool bFlashVisible = false;
	FVector2D FlashCenter = FVector2D::ZeroVector;
	float FlashScale = 1.f;
	float FlashOpacity = 0.f;
};
