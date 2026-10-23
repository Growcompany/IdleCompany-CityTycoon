#include "UI/Ranking/RankingBuildingPreviewRenderer.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Utils/MobileFacadeMaterialTuning.h"

namespace RankingBuildingPreview
{
struct FResolvedBuilding
{
	FCitySnapshotBuilding Building;
	FBuildingData Data;
	UStaticMesh* Base = nullptr;
	UStaticMesh* Body = nullptr;
	UStaticMesh* Top = nullptr;
	FBox Bounds = FBox(ForceInit);
	float RoofZ = 0.f;
};

static bool Resolve(const FCitySnapshotBuilding& Source, UTableManagerSubsystem* Tables, FResolvedBuilding& Out)
{
	if (!Tables || Source.InteractableName.IsNone() || Source.Scale.ContainsNaN()) return false;
	bool bFound = false;
	Out.Data = Tables->GetBuildingData(Source.InteractableName, bFound);
	if (!bFound || Out.Data.BaseMesh.IsNull()) return false;
	Out.Base = Out.Data.BaseMesh.LoadSynchronous();
	Out.Body = Out.Data.BodyMesh.IsNull() ? nullptr : Out.Data.BodyMesh.LoadSynchronous();
	Out.Top = Out.Data.TopMesh.IsNull() ? nullptr : Out.Data.TopMesh.LoadSynchronous();
	if (!Out.Base || (!Out.Data.BodyMesh.IsNull() && !Out.Body) || (!Out.Data.TopMesh.IsNull() && !Out.Top)) return false;
	Out.Building = Source;
	// 외부 스냅샷의 비정상 크기가 렌더 비용이나 카메라 범위를 무한히 키우지 않도록 제한한다.
	Out.Building.Body_Module_Copies = FMath::Clamp(Source.Body_Module_Copies, 0, 128);
	Out.Building.Scale = FVector(FMath::Clamp(FMath::Abs(Source.Scale.X), .01, 10.),
		FMath::Clamp(FMath::Abs(Source.Scale.Y), .01, 10.), FMath::Clamp(FMath::Abs(Source.Scale.Z), .01, 10.));
	const FBox BaseBox = Out.Base->GetBoundingBox();
	Out.Bounds = BaseBox;
	float Height = BaseBox.Max.Z;
	if (Out.Body)
	{
		const FBox BodyBox = Out.Body->GetBoundingBox();
		for (int32 Index = 0; Index <= Out.Building.Body_Module_Copies; ++Index)
		{
			Out.Bounds += BodyBox.ShiftBy(FVector(0., 0., Height));
			// RebuildBodyModules는 모듈의 Max.Z를 피치로 사용한다. 스냅샷에 없는 층스케일은 방문 기본값1.
			Height += BodyBox.Max.Z;
		}
	}
	Out.RoofZ = Height;
	if (Out.Top) Out.Bounds += Out.Top->GetBoundingBox().ShiftBy(FVector(0., 0., Height));
	Out.Bounds = Out.Bounds.TransformBy(FTransform(FQuat::Identity, FVector::ZeroVector, Out.Building.Scale));
	return Out.Bounds.IsValid && !Out.Bounds.GetExtent().ContainsNaN() && Out.Bounds.GetSize().Z > 0.;
}
}

bool URankingBuildingPreviewRenderer::SelectRepresentative(const FCitySnapshot& Snapshot, UTableManagerSubsystem* Tables, FCitySnapshotBuilding& OutBuilding)
{
	double BestRevenue = 0.;
	double BestHeight = -1.;
	FString BestName;
	bool bSelected = false;
	for (const FCitySnapshotBuilding& Candidate : Snapshot.Buildings)
	{
		RankingBuildingPreview::FResolvedBuilding Resolved;
		if (!RankingBuildingPreview::Resolve(Candidate, Tables, Resolved)) continue;
		const double Revenue = FMath::IsFinite(Candidate.LifetimeRevenueEarned) && Candidate.LifetimeRevenueEarned > 0.
			? Candidate.LifetimeRevenueEarned : 0.;
		const double Height = Resolved.Bounds.GetSize().Z;
		const FString Name = Candidate.InteractableName.ToString();
		if (!bSelected || Revenue > BestRevenue || (Revenue == BestRevenue
			&& (Height > BestHeight || (Height == BestHeight && Name < BestName))))
		{
			bSelected = true;
			BestRevenue = Revenue;
			BestHeight = Height;
			BestName = Name;
			OutBuilding = Resolved.Building;
		}
	}
	return bSelected;
}

UTextureRenderTarget2D* URankingBuildingPreviewRenderer::CaptureRepresentative(UWorld* CaptureWorld, UTableManagerSubsystem* Tables, const FCitySnapshot& Snapshot)
{
	FCitySnapshotBuilding Representative;
	if (!CaptureWorld || CaptureWorld->bIsTearingDown || !CaptureWorld->Scene || !SelectRepresentative(Snapshot, Tables, Representative)) return nullptr;
	RankingBuildingPreview::FResolvedBuilding Resolved;
	if (!RankingBuildingPreview::Resolve(Representative, Tables, Resolved)) return nullptr;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector StageOrigin(0., 0., -1000000.);
	AActor* Stage = CaptureWorld->SpawnActor<AActor>(AActor::StaticClass(), StageOrigin, FRotator::ZeroRotator, SpawnParameters);
	if (!Stage) return nullptr;
	Stage->Tags.Add(TEXT("RankingPreviewCapture"));
	ON_SCOPE_EXIT { Stage->Destroy(); };
	USceneComponent* StageRoot = NewObject<USceneComponent>(Stage);
	Stage->AddInstanceComponent(StageRoot);
	Stage->SetRootComponent(StageRoot);
	StageRoot->RegisterComponent();
	StageRoot->SetWorldLocation(StageOrigin);

	UMaterialInterface* MainMaterial = Resolved.Data.MainMaterial.LoadSynchronous();
	if (Representative.AppliedSkinID != 0)
	{
		bool bSkinFound = false;
		const FBuildingSkinData Skin = Tables->GetBuildingSkinData(Representative.AppliedSkinID, bSkinFound);
		if (bSkinFound && !Skin.SkinMaterial.IsNull())
		{
			if (UMaterialInterface* SkinMaterial = Skin.SkinMaterial.LoadSynchronous()) MainMaterial = SkinMaterial;
		}
	}
	UMaterialInterface* RoofMaterial = Resolved.Data.RoofElementsMaterial.LoadSynchronous();
	// 촬영 무대로 등비 축소하되 실제 폭·깊이·높이 비율은 스냅샷대로 유지한다.
	const double Normalize = 600. / FMath::Max(Resolved.Bounds.GetSize().GetMax(), 1.);
	const FVector ModelScale = Representative.Scale * Normalize;
	const FVector CenterOffset = -Resolved.Bounds.GetCenter() * Normalize;
	const FBox StageBounds = Resolved.Bounds.TransformBy(FTransform(FQuat::Identity, StageOrigin + CenterOffset, FVector(Normalize)));
	auto AddMesh = [&](UStaticMesh* Mesh, double LocalZ, bool bRoof, int32 InstanceCount = 0)
	{
		if (!Mesh) return;
		UInstancedStaticMeshComponent* Instances = InstanceCount > 0 ? NewObject<UInstancedStaticMeshComponent>(Stage) : nullptr;
		UStaticMeshComponent* Component = Instances ? Instances : NewObject<UStaticMeshComponent>(Stage);
		Stage->AddInstanceComponent(Component);
		Component->SetupAttachment(StageRoot);
		Component->SetStaticMesh(Mesh);
		Component->SetRelativeScale3D(ModelScale);
		Component->SetRelativeLocation(CenterOffset + FVector(0., 0., LocalZ * ModelScale.Z));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		Component->SetVisibleInSceneCaptureOnly(true);
		Component->SetLightingChannels(false, false, true);
		Component->SetCustomPrimitiveDataFloat(0, 1.f);
		Component->SetCustomPrimitiveDataFloat(1, 0.f);
		if (MainMaterial) Component->SetMaterial(0, MainMaterial);
		if (bRoof && RoofMaterial) Component->SetMaterial(1, RoofMaterial);
		if (Instances)
		{
			for (int32 Index = 0; Index < InstanceCount; ++Index)
			{
				Instances->AddInstance(FTransform(FVector(0., 0., Index * Mesh->GetBoundingBox().Max.Z)));
			}
		}
		Component->RegisterComponent();
		MobileFacadeMaterialTuning::ApplyToSlotZero(Component, StageBounds);
	};
	AddMesh(Resolved.Base, 0., false);
	if (Resolved.Body)
	{
		AddMesh(Resolved.Body, Resolved.Base->GetBoundingBox().Max.Z, false, Representative.Body_Module_Copies + 1);
	}
	AddMesh(Resolved.Top, Resolved.RoofZ, true);

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Stage);
		Stage->AddInstanceComponent(Light);
		Light->SetupAttachment(StageRoot);
		Light->SetRelativeLocation(Index == 0 ? FVector(-1400., -1800., 1800.) : FVector(1200., 600., 800.));
		Light->SetIntensityUnits(ELightUnits::Unitless);
		Light->SetIntensity(Index == 0 ? 2200000.f : 700000.f);
		Light->SetAttenuationRadius(6500.f);
		Light->SetCastShadows(false);
		Light->SetLightingChannels(false, false, true);
		Light->RegisterComponent();
	}
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
	Target->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
	Target->RenderTargetFormat = RTF_RGBA16f;
	Target->InitAutoFormat(512, 768);
	Target->UpdateResourceImmediate(true);
	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Stage);
	Stage->AddInstanceComponent(Capture);
	Capture->SetupAttachment(StageRoot);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Add(Stage);
	Capture->TextureTarget = Target;
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	const FVector CameraOffset(-1600., -2200., 1500.);
	const FRotator CameraRotation = (-CameraOffset).Rotation();
	Capture->SetRelativeLocation(CameraOffset);
	Capture->SetRelativeRotation(CameraRotation);
	const FBox NormalizedBounds = Resolved.Bounds.TransformBy(FTransform(FQuat::Identity, CenterOffset, FVector(Normalize)));
	const FBox ViewBounds = NormalizedBounds.TransformBy(FTransform(CameraRotation).Inverse());
	Capture->OrthoWidth = FMath::Max(ViewBounds.GetSize().Y, ViewBounds.GetSize().Z * (512.f / 768.f)) * 1.18f;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetSkyLighting(false);
	Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Capture->PostProcessSettings.bOverride_BloomIntensity = true;
	Capture->PostProcessSettings.BloomIntensity = 0.f;
	Capture->RegisterComponent();
	Capture->CaptureScene();
	FlushRenderingCommands();
	if (CapturedTargets.Num() >= 6) CapturedTargets.RemoveAt(0);
	CapturedTargets.Add(Target);
	return Target;
}

void URankingBuildingPreviewRenderer::Release()
{
	CapturedTargets.Reset();
}
