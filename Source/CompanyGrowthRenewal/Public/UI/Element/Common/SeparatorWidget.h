// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SeparatorWidget.generated.h"

class USizeBox;
class UImage;
class UHorizontalBox;

/**
 * 재사용 가능한 구분선 위젯
 * - SizeBox로 크기 제어, Image(RoundedBox)로 렌더링
 * - 에디터에서 두께/길이/색상을 바로 설정 가능
 * - GradientLength > 0 이면 양 끝이 투명으로 페이드 (HBox 3분할 WBP 구조 필요)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USeparatorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(meta = (BindWidget))
	USizeBox* SizeBox;

	UPROPERTY(meta = (BindWidget))
	UImage* Separator;

	// 새 WBP 구조 (HBox 3분할). 기존 WBP 하위호환 위해 OptionalWidget 처리
	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* HBox_Root;

	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* SizeBox_Left;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* Image_LeftGradient;

	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* SizeBox_Right;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* Image_RightGradient;

	// 가로 크기 (0이면 override 없이 부모 채움)
	UPROPERTY(EditAnywhere, Category = "Separator", meta = (ClampMin = "0.0"))
	float WidthOverride = 0.0f;

	// 세로 크기 (0이면 override 없이 부모 채움)
	UPROPERTY(EditAnywhere, Category = "Separator", meta = (ClampMin = "0.0"))
	float HeightOverride = 2.0f;

	// 구분선 색상
	UPROPERTY(EditAnywhere, Category = "Separator")
	FLinearColor Color = FLinearColor(0.244792f, 0.244792f, 0.244792f, 1.0f);

	// 모서리 둥글기 (0이면 직선)
	UPROPERTY(EditAnywhere, Category = "Separator", meta = (ClampMin = "0.0"))
	float CornerRadius = 1.0f;

	// 왼쪽 그라데이션 페이드 길이 (px). 0 이면 왼쪽 그라데이션 비활성
	UPROPERTY(EditAnywhere, Category = "Separator|Gradient", meta = (ClampMin = "0.0"))
	float LeftGradientLength = 0.0f;

	// 오른쪽 그라데이션 페이드 길이 (px). 0 이면 오른쪽 그라데이션 비활성
	UPROPERTY(EditAnywhere, Category = "Separator|Gradient", meta = (ClampMin = "0.0"))
	float RightGradientLength = 0.0f;

	// 그라데이션 알파 배율 (0~1). Color 의 알파와 곱해져 끝단 밝기 제어
	UPROPERTY(EditAnywhere, Category = "Separator|Gradient", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GradientAlpha = 1.0f;

	virtual void NativePreConstruct() override;

private:
	// NativePreConstruct/SetSeparatorColor 가 공유하는 그라데이션 Brush 동기화
	void ApplyGradientBrushes();

public:
	// 런타임에 크기 변경
	UFUNCTION(BlueprintCallable, Category = "Separator")
	void SetSeparatorSize(float NewWidth, float NewHeight);

	// 런타임에 색상 변경
	UFUNCTION(BlueprintCallable, Category = "Separator")
	void SetSeparatorColor(FLinearColor NewColor);
};
