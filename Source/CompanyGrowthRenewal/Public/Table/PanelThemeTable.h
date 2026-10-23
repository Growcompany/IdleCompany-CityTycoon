#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PanelThemeTable.generated.h"

class UMaterialInterface;

// 팔레트만 보관한다. 문양 선택과 치수·림·패딩은 각각 인스턴스와 공용 WBP가 소유한다.
USTRUCT(BlueprintType)
struct FPanelThemeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
	FLinearColor ShellFillTop = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
	FLinearColor ShellFillBottom = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
	FLinearColor ShellBorderColor = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
	FLinearColor ShellLipColor = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FLinearColor HeaderFillTop = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FLinearColor HeaderFillBottom = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FLinearColor HeaderLipColor = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FLinearColor TitleColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	TSoftObjectPtr<UMaterialInterface> HeaderDepthMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FVector2D TitleShadowOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FLinearColor TitleShadowColor = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FLinearColor PatternTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header")
	FLinearColor GradientTint = FLinearColor::White;

	// 제목면만 조절한다. 전체화면 헤더의 제목·버튼·선택 장식은 불투명도를 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Header", meta = (ClampMin = "0", ClampMax = "1"))
	float HeaderOpacity = 1.0f;
};
