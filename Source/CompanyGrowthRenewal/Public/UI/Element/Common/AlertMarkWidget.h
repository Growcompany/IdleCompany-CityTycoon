#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/AlertMark.h"
#include "AlertMarkWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 범용 알림 도트 위젯 (WBP: UIE_AlertMark)
 * - 부모 WBP의 Overlay/Canvas 슬롯에 드롭인으로 얹어 사용
 * - Size(Small/Large) + Color(Red/Green) 조합으로 4종 텍스처 자동 선택
 * - Show/Hide/SetMark 만으로 표시 제어 — BindWidgetOptional 패턴 지원
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UAlertMarkWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> DotImage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AlertMark")
	EAlertMarkSize Size = EAlertMarkSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AlertMark")
	EAlertMarkColor Color = EAlertMarkColor::Red;

	// Size enum 별 렌더 픽셀 크기 — 텍스처 원본 대신 이 값으로 DesiredSize 강제
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AlertMark|Size", meta = (ClampMin = "1.0"))
	FVector2D SmallSize = FVector2D(16.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AlertMark|Size", meta = (ClampMin = "1.0"))
	FVector2D LargeSize = FVector2D(32.0f, 32.0f);

	// 4종 하드 참조 — UPROPERTY 덕에 자동 쿠킹 (Additional Asset Directories 등록 불필요)
	UPROPERTY(EditAnywhere, Category = "AlertMark|Textures")
	TObjectPtr<UTexture2D> Tex_SmallRed = nullptr;

	UPROPERTY(EditAnywhere, Category = "AlertMark|Textures")
	TObjectPtr<UTexture2D> Tex_SmallGreen = nullptr;

	UPROPERTY(EditAnywhere, Category = "AlertMark|Textures")
	TObjectPtr<UTexture2D> Tex_LargeRed = nullptr;

	UPROPERTY(EditAnywhere, Category = "AlertMark|Textures")
	TObjectPtr<UTexture2D> Tex_LargeGreen = nullptr;

	virtual void NativePreConstruct() override;

public:
	UFUNCTION(BlueprintCallable, Category = "AlertMark")
	void SetMark(EAlertMarkSize InSize, EAlertMarkColor InColor);

	UFUNCTION(BlueprintCallable, Category = "AlertMark")
	void SetSize(EAlertMarkSize InSize);

	UFUNCTION(BlueprintCallable, Category = "AlertMark")
	void SetColor(EAlertMarkColor InColor);

	UFUNCTION(BlueprintCallable, Category = "AlertMark")
	void Show();

	UFUNCTION(BlueprintCallable, Category = "AlertMark")
	void Hide();

	UFUNCTION(BlueprintPure, Category = "AlertMark")
	bool IsShown() const;

private:
	void ApplyBrush();
	UTexture2D* PickTexture() const;
};
