#pragma once

#include "Math/Box.h"
#include "Math/Color.h"

class UMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

namespace MobileFacadeMaterialTuning
{
	struct FMobileBakedAOParameters
	{
		float GroundZ = 0.0f;
		float InvHeight = 0.0f;
	};

	struct FQuietPlusTuning
	{
		float NormalIntensityScale;
		float WindowLightThreshold;
		FLinearColor PrimaryWallColorScale;
		FLinearColor WindowsColorScale;
		FLinearColor FramesColorScale;
		float WindowsColorSaturation;
	};

	const FQuietPlusTuning& GetQuietPlusTuning();
	FLinearColor ScaleRgbPreserveAlpha(const FLinearColor& Source, const FLinearColor& Scale);
	// 원본 색조와 알파를 유지한 채 채도와 명도만 조정한다.
	FLinearColor DeepenGlassColor(
		const FLinearColor& Source,
		const FLinearColor& Scale,
		float Saturation);
	bool TryCalculateBakedAOParameters(const FBox& ActorBounds, FMobileBakedAOParameters& OutParameters);
	FLinearColor ResolveWindowEmissiveColor(const FLinearColor& Source, bool bMobileES31);
	void SetWindowEmissiveColorOnMaterials(UMeshComponent* MeshComponent, const FLinearColor& Color);
	// 점등 비율(켜지는 창문 분율). 머티리얼이 액터 위치로 ±0.05 를 더해 건물마다 흔든다.
	void SetWindowLightDensityOnMaterials(UMeshComponent* MeshComponent, float Density);
	bool IsFacadeMaterialQualified(UMaterialInterface* Material);
	bool ApplyToDynamicMaterial(
		UMaterialInstanceDynamic* FacadeMID,
		UMaterialInterface* PaletteSource,
		const FBox* ActorBounds = nullptr);
	void ApplyToSlotZero(UMeshComponent* MeshComponent, const FBox& ActorBounds);
}
