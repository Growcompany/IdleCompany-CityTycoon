#include "Utils/MobileFacadeMaterialTuning.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UMaterialInterface* GetPaletteSourceMaterial(UMaterialInterface* CurrentMaterial)
	{
		const UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
		return ExistingMID && ExistingMID->Parent ? ExistingMID->Parent.Get() : CurrentMaterial;
	}

	void ApplyScaledColor(
		UMaterialInstanceDynamic* FacadeMID,
		UMaterialInterface* PaletteSource,
		const FName ParameterName,
		const FLinearColor& Scale)
	{
		FLinearColor SourceColor;
		if (PaletteSource && PaletteSource->GetVectorParameterValue(
				FHashedMaterialParameterInfo(ParameterName), SourceColor))
		{
			FacadeMID->SetVectorParameterValue(
				ParameterName,
				MobileFacadeMaterialTuning::ScaleRgbPreserveAlpha(SourceColor, Scale));
		}
	}
}

const MobileFacadeMaterialTuning::FQuietPlusTuning& MobileFacadeMaterialTuning::GetQuietPlusTuning()
{
	static const FQuietPlusTuning Tuning = {
		0.10f,
		0.40f,
		0.12f,
		0.56f,
		0.34f,
		0.08f,
		0.85f,
		0.45f,
		FLinearColor(0.72f, 0.78f, 0.86f, 1.0f),
		FLinearColor(0.78f, 0.78f, 0.78f, 1.0f),
		FLinearColor(0.70f, 0.70f, 0.70f, 1.0f),
		FLinearColor(0.60f, 0.60f, 0.62f, 1.0f),
	};
	return Tuning;
}

FLinearColor MobileFacadeMaterialTuning::ScaleRgbPreserveAlpha(
	const FLinearColor& Source,
	const FLinearColor& Scale)
{
	return FLinearColor(
		Source.R * Scale.R,
		Source.G * Scale.G,
		Source.B * Scale.B,
		Source.A);
}

bool MobileFacadeMaterialTuning::ApplyToDynamicMaterial(
	UMaterialInstanceDynamic* FacadeMID,
	UMaterialInterface* PaletteSource)
{
	if (!FacadeMID || !PaletteSource)
	{
		return false;
	}

	float UnusedValue = 0.0f;
	if (!PaletteSource->GetScalarParameterValue(
			FHashedMaterialParameterInfo(TEXT("Windows_Roughness")), UnusedValue)
		|| !PaletteSource->GetScalarParameterValue(
			FHashedMaterialParameterInfo(TEXT("Mobile_FakeReflection_Strength")), UnusedValue))
	{
		return false;
	}

	const FQuietPlusTuning& Tuning = GetQuietPlusTuning();
	FacadeMID->SetScalarParameterValue(TEXT("Windows_Metallic"), Tuning.WindowsMetallic);
	FacadeMID->SetScalarParameterValue(TEXT("Windows_Roughness"), Tuning.WindowsRoughness);
	FacadeMID->SetScalarParameterValue(TEXT("Frames_Metallic"), Tuning.FramesMetallic);
	FacadeMID->SetScalarParameterValue(TEXT("Frames_Roughness"), Tuning.FramesRoughness);
	FacadeMID->SetScalarParameterValue(TEXT("Specular"), Tuning.Specular);
	FacadeMID->SetScalarParameterValue(TEXT("Normal_Intensity"), Tuning.NormalIntensity);
	FacadeMID->SetScalarParameterValue(TEXT("Window_Light_Threshold"), Tuning.WindowLightThreshold);
	FacadeMID->SetScalarParameterValue(TEXT("Mobile_FakeReflection_Strength"), Tuning.FakeReflectionStrength);
	FacadeMID->SetVectorParameterValue(TEXT("Mobile_FakeReflection_Tint"), Tuning.FakeReflectionTint);
	ApplyScaledColor(FacadeMID, PaletteSource, TEXT("Walls_1_Color"), Tuning.PrimaryWallColorScale);
	ApplyScaledColor(FacadeMID, PaletteSource, TEXT("Walls_Color"), Tuning.PrimaryWallColorScale);
	ApplyScaledColor(FacadeMID, PaletteSource, TEXT("Windows_Color"), Tuning.WindowsColorScale);
	ApplyScaledColor(FacadeMID, PaletteSource, TEXT("Frames_Color"), Tuning.FramesColorScale);
	return true;
}

void MobileFacadeMaterialTuning::ApplyToSlotZero(UMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	const UWorld* ComponentWorld = MeshComponent->GetWorld();
	if (!ComponentWorld || ComponentWorld->GetFeatureLevel() != ERHIFeatureLevel::ES3_1)
	{
		return;
	}

	UMaterialInterface* CurrentMaterial = MeshComponent->GetMaterial(0);
	if (!CurrentMaterial)
	{
		return;
	}

	float UnusedValue = 0.0f;
	if (!CurrentMaterial->GetScalarParameterValue(
			FHashedMaterialParameterInfo(TEXT("Windows_Roughness")), UnusedValue)
		|| !CurrentMaterial->GetScalarParameterValue(
			FHashedMaterialParameterInfo(TEXT("Mobile_FakeReflection_Strength")), UnusedValue))
	{
		return;
	}

	UMaterialInterface* PaletteSource = GetPaletteSourceMaterial(CurrentMaterial);
	UMaterialInstanceDynamic* FacadeMID = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
	if (!FacadeMID)
	{
		FacadeMID = MeshComponent->CreateDynamicMaterialInstance(0, CurrentMaterial);
	}
	if (!FacadeMID)
	{
		return;
	}

	ApplyToDynamicMaterial(FacadeMID, PaletteSource);
}
