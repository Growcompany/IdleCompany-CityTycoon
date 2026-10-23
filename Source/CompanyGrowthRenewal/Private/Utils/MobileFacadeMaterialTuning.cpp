#include "Utils/MobileFacadeMaterialTuning.h"

#include "Components/MeshComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// 마스터 기본값 TC_StudioReflect 는 저대비 스튜디오 그라데이션이라 반사에 비칠 내용이 없다.
	const TCHAR* const ReflectionCubemapPath =
		TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/TC_CitySkyReflect.TC_CitySkyReflect");

	UTextureCube* ResolveReflectionCubemap()
	{
		static TWeakObjectPtr<UTextureCube> CachedCubemap;
		if (UTextureCube* Existing = CachedCubemap.Get())
		{
			return Existing;
		}

		// 에셋 부재는 영구 상태다 — 건물 40개가 각자 실패 로드를 찍지 않게 여기서 한 번만 막는다.
		// ⚠ "로드 시도함" 과 묶지 말 것: 위 캐시는 약참조라 수명을 붙들지 못하고, 맵을 넘기면
		// 큐브맵을 들고 있던 파사드 MID 가 전부 사라져 GC 가 수거한다. 그때 재시도를 막으면
		// 남은 세션 전체가 조용히 마스터 기본 큐브맵으로 떨어진다(로그도 안 남는다).
		static bool bAssetMissing = false;
		if (bAssetMissing)
		{
			return nullptr;
		}

		UTextureCube* Loaded = LoadObject<UTextureCube>(
			nullptr, ReflectionCubemapPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Loaded)
		{
			bAssetMissing = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[MobileFacade] 반사 큐브맵 로드 실패: %s"), ReflectionCubemapPath);
			return nullptr;
		}

		CachedCubemap = Loaded;
		return Loaded;
	}

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

	void ApplyDeepenedWindowColor(
		UMaterialInstanceDynamic* FacadeMID,
		UMaterialInterface* PaletteSource,
		const MobileFacadeMaterialTuning::FQuietPlusTuning& Tuning)
	{
		static const FName WindowsColorName(TEXT("Windows_Color"));
		FLinearColor SourceColor;
		if (PaletteSource && PaletteSource->GetVectorParameterValue(
				FHashedMaterialParameterInfo(WindowsColorName), SourceColor))
		{
			FacadeMID->SetVectorParameterValue(
				WindowsColorName,
				MobileFacadeMaterialTuning::DeepenGlassColor(
					SourceColor,
					Tuning.WindowsColorScale,
					Tuning.WindowsColorSaturation));
		}
	}

	void ApplyPaletteScalar(
		UMaterialInstanceDynamic* FacadeMID,
		UMaterialInterface* PaletteSource,
		const FName ParameterName,
		const float Scale = 1.0f)
	{
		float SourceValue = 0.0f;
		if (PaletteSource->GetScalarParameterValue(FHashedMaterialParameterInfo(ParameterName), SourceValue))
		{
			FacadeMID->SetScalarParameterValue(ParameterName, SourceValue * Scale);
		}
	}

	void ApplyPaletteVector(
		UMaterialInstanceDynamic* FacadeMID,
		UMaterialInterface* PaletteSource,
		const FName ParameterName)
	{
		FLinearColor SourceValue;
		if (PaletteSource->GetVectorParameterValue(FHashedMaterialParameterInfo(ParameterName), SourceValue))
		{
			FacadeMID->SetVectorParameterValue(ParameterName, SourceValue);
		}
	}
}

const MobileFacadeMaterialTuning::FQuietPlusTuning& MobileFacadeMaterialTuning::GetQuietPlusTuning()
{
	static const FQuietPlusTuning Tuning = {
		0.28f,
		0.85f,
		FLinearColor(0.92f, 0.92f, 0.92f, 1.0f),
		FLinearColor(0.62f, 0.62f, 0.62f, 1.0f),
		FLinearColor(0.60f, 0.60f, 0.60f, 1.0f),
		1.35f,
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

FLinearColor MobileFacadeMaterialTuning::DeepenGlassColor(
	const FLinearColor& Source,
	const FLinearColor& Scale,
	const float Saturation)
{
	const float Luma = Source.R * 0.3f + Source.G * 0.59f + Source.B * 0.11f;

	auto DeepenChannel = [&](const float SourceChannel, const float ScaleChannel)
	{
		const float Saturated = Luma + (SourceChannel - Luma) * Saturation;
		return FMath::Max(Saturated, 0.0f) * ScaleChannel;
	};

	return FLinearColor(
		DeepenChannel(Source.R, Scale.R),
		DeepenChannel(Source.G, Scale.G),
		DeepenChannel(Source.B, Scale.B),
		Source.A);
}

bool MobileFacadeMaterialTuning::TryCalculateBakedAOParameters(
	const FBox& ActorBounds,
	FMobileBakedAOParameters& OutParameters)
{
	OutParameters = FMobileBakedAOParameters();
	if (!ActorBounds.IsValid
		|| ActorBounds.Min.ContainsNaN()
		|| ActorBounds.Max.ContainsNaN())
	{
		return false;
	}

	const double Height = ActorBounds.Max.Z - ActorBounds.Min.Z;
	if (!FMath::IsFinite(Height) || Height <= 0.0f)
	{
		return false;
	}

	const float GroundZ = static_cast<float>(ActorBounds.Min.Z);
	const float InvHeight = static_cast<float>(
		1.0 / FMath::Max(Height, static_cast<double>(UE_SMALL_NUMBER)));
	if (!FMath::IsFinite(GroundZ) || !FMath::IsFinite(InvHeight))
	{
		return false;
	}

	OutParameters.GroundZ = GroundZ;
	OutParameters.InvHeight = InvHeight;
	return true;
}

FLinearColor MobileFacadeMaterialTuning::ResolveWindowEmissiveColor(
	const FLinearColor& Source,
	bool bMobileES31)
{
	if (!bMobileES31)
	{
		return Source;
	}

	const FLinearColor Emissive(Source.R * 4.0f, Source.G * 4.0f, Source.B * 4.0f, Source.A);
	constexpr float MaxMobileWindowLuminance = 2.4f;
	const float Luminance = Emissive.GetLuminance();
	// 작은 창문의 강한 발광을 제한하되 RGB 비율과 점등 알파는 유지한다.
	const float Scale = Luminance > MaxMobileWindowLuminance ? MaxMobileWindowLuminance / Luminance : 1.0f;
	return ScaleRgbPreserveAlpha(Emissive, FLinearColor(Scale, Scale, Scale, 1.0f));
}

namespace
{
	template <typename TSetter>
	void ForEachFacadeDynamicMaterial(UMeshComponent* MeshComponent, TSetter&& Setter)
	{
		if (!MeshComponent)
		{
			return;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			UMaterialInterface* CurrentMaterial = MeshComponent->GetMaterial(MaterialIndex);
			if (!CurrentMaterial)
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
			if (!DynamicMaterial)
			{
				DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(
					MaterialIndex,
					CurrentMaterial);
			}
			if (DynamicMaterial)
			{
				Setter(*DynamicMaterial);
			}
		}
	}
}

void MobileFacadeMaterialTuning::SetWindowEmissiveColorOnMaterials(
	UMeshComponent* MeshComponent,
	const FLinearColor& Color)
{
	static const FName EmissiveParameterName(TEXT("Window_Emissive_Color"));
	ForEachFacadeDynamicMaterial(MeshComponent, [&Color](UMaterialInstanceDynamic& DynamicMaterial)
	{
		DynamicMaterial.SetVectorParameterValue(EmissiveParameterName, Color);
	});
}

void MobileFacadeMaterialTuning::SetWindowLightDensityOnMaterials(
	UMeshComponent* MeshComponent,
	float Density)
{
	static const FName DensityParameterName(TEXT("Window_Light_Density"));
	const float ClampedDensity = FMath::Clamp(Density, 0.0f, 1.0f);
	ForEachFacadeDynamicMaterial(MeshComponent, [ClampedDensity](UMaterialInstanceDynamic& DynamicMaterial)
	{
		DynamicMaterial.SetScalarParameterValue(DensityParameterName, ClampedDensity);
	});
}

bool MobileFacadeMaterialTuning::IsFacadeMaterialQualified(UMaterialInterface* Material)
{
	if (!Material)
	{
		return false;
	}

	float UnusedValue = 0.0f;
	return Material->GetScalarParameterValue(
			FHashedMaterialParameterInfo(TEXT("Windows_Roughness")), UnusedValue)
		&& Material->GetScalarParameterValue(
			FHashedMaterialParameterInfo(TEXT("Mobile_FakeReflection_Strength")), UnusedValue);
}

bool MobileFacadeMaterialTuning::ApplyToDynamicMaterial(
	UMaterialInstanceDynamic* FacadeMID,
	UMaterialInterface* PaletteSource,
	const FBox* ActorBounds)
{
	if (!FacadeMID || !PaletteSource)
	{
		return false;
	}

	if (!IsFacadeMaterialQualified(PaletteSource))
	{
		return false;
	}

	const FQuietPlusTuning& Tuning = GetQuietPlusTuning();
	ApplyPaletteScalar(FacadeMID, PaletteSource, TEXT("Windows_Metallic"));
	ApplyPaletteScalar(FacadeMID, PaletteSource, TEXT("Windows_Roughness"));
	ApplyPaletteScalar(FacadeMID, PaletteSource, TEXT("Frames_Metallic"));
	ApplyPaletteScalar(FacadeMID, PaletteSource, TEXT("Frames_Roughness"));
	ApplyPaletteScalar(FacadeMID, PaletteSource, TEXT("Specular"));
	ApplyPaletteScalar(FacadeMID, PaletteSource, TEXT("Normal_Intensity"), Tuning.NormalIntensityScale);
	FacadeMID->SetScalarParameterValue(TEXT("Window_Light_Threshold"), Tuning.WindowLightThreshold);
	ApplyPaletteScalar(FacadeMID, PaletteSource, TEXT("Mobile_FakeReflection_Strength"));
	ApplyPaletteVector(FacadeMID, PaletteSource, TEXT("Mobile_FakeReflection_Tint"));
	if (UTextureCube* ReflectionCubemap = ResolveReflectionCubemap())
	{
		FacadeMID->SetTextureParameterValue(TEXT("Mobile_FakeReflection_Cubemap"), ReflectionCubemap);
	}
	FacadeMID->SetScalarParameterValue(TEXT("Mobile_WindowBakedHalo_Mix"), 1.0f);

	FMobileBakedAOParameters BakedAOParameters;
	const bool bHasValidBakedAO = ActorBounds
		&& TryCalculateBakedAOParameters(*ActorBounds, BakedAOParameters);
	FacadeMID->SetScalarParameterValue(
		TEXT("Mobile_FacadeLookTransfer_Strength"),
		bHasValidBakedAO ? 1.0f : 0.0f);
	FacadeMID->SetScalarParameterValue(TEXT("Mobile_BAO_GroundZ"), BakedAOParameters.GroundZ);
	FacadeMID->SetScalarParameterValue(TEXT("Mobile_BAO_InvHeight"), BakedAOParameters.InvHeight);

	ApplyScaledColor(FacadeMID, PaletteSource, TEXT("Walls_1_Color"), Tuning.PrimaryWallColorScale);
	ApplyScaledColor(FacadeMID, PaletteSource, TEXT("Walls_Color"), Tuning.PrimaryWallColorScale);
	ApplyDeepenedWindowColor(FacadeMID, PaletteSource, Tuning);
	ApplyScaledColor(FacadeMID, PaletteSource, TEXT("Frames_Color"), Tuning.FramesColorScale);
	return true;
}

void MobileFacadeMaterialTuning::ApplyToSlotZero(
	UMeshComponent* MeshComponent,
	const FBox& ActorBounds)
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

	if (!IsFacadeMaterialQualified(CurrentMaterial))
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

	ApplyToDynamicMaterial(FacadeMID, PaletteSource, &ActorBounds);
}
