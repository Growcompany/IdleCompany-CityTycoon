#pragma once

#include "Math/Color.h"

class UMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

namespace MobileFacadeMaterialTuning
{
	struct FQuietPlusTuning
	{
		float WindowsMetallic;
		float WindowsRoughness;
		float FramesMetallic;
		float FramesRoughness;
		float Specular;
		float NormalIntensity;
		float WindowLightThreshold;
		float FakeReflectionStrength;
		FLinearColor FakeReflectionTint;
		FLinearColor PrimaryWallColorScale;
		FLinearColor WindowsColorScale;
		FLinearColor FramesColorScale;
	};

	const FQuietPlusTuning& GetQuietPlusTuning();
	FLinearColor ScaleRgbPreserveAlpha(const FLinearColor& Source, const FLinearColor& Scale);
	bool ApplyToDynamicMaterial(UMaterialInstanceDynamic* FacadeMID, UMaterialInterface* PaletteSource);
	void ApplyToSlotZero(UMeshComponent* MeshComponent);
}
