#include "Entity/OverseasDiorama/OverseasDioramaStage.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Entity/OverseasDiorama/DioramaModules.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double kDioVerticalFovDeg = 28.0;
	constexpr double kDioFactoryDistM = 110.0;
	constexpr double kDioMinDistM = 66.0;
	constexpr double kDioMaxDistM = 104.0;
	constexpr double kDioDistPerWidth = 2.3;
	constexpr double kDioStillYawDeg = 14.0;
	constexpr double kDioStillPitchDeg = 37.0;
	constexpr double kDioStillDistM = 300.0;
	constexpr double kDioStillMinY = -8.6;
	constexpr double kDioStillMaxY = 15.0;
	constexpr int32 kDioStillMaxPx = 1024;
	constexpr double kDioIntroRiseM = 7.0;
	constexpr float kDioIntroRiseDelay = 0.1f;
	constexpr float kDioIntroRiseDuration = 0.9f;
	constexpr float kDioIntroCameraDuration = 1.25f;
	// SM_Truck_Large 의 앞이 +X 라고 가정 — 렌더 탐침에서 반대면 180 으로.
	constexpr double kDioTruckYawOffsetDeg = 0.0;

	FVector MockupDirToUE(const FVector& D) { return FVector(-D.Z, D.X, D.Y); }

	double EaseOut(double T) { T = FMath::Clamp(T, 0.0, 1.0); return 1.0 - FMath::Pow(1.0 - T, 3.0); }
	double EaseBack(double T)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
		const double C1 = 1.70158;
		const double C3 = C1 + 1.0;
		return 1.0 + C3 * FMath::Pow(T - 1.0, 3.0) + C1 * FMath::Pow(T - 1.0, 2.0);
	}

	FVector CameraLocationM(const FDioramaLiveCamera& Cam)
	{
		const double Y = FMath::DegreesToRadians(Cam.YawDeg);
		const double P = FMath::DegreesToRadians(Cam.PitchDeg);
		return FVector(Cam.TargetM.X + Cam.DistM * FMath::Cos(P) * FMath::Sin(Y),
			Cam.DistM * FMath::Sin(P),
			Cam.TargetM.Z + Cam.DistM * FMath::Cos(P) * FMath::Cos(Y));
	}

	FDioramaLiveCamera LerpCamera(const FDioramaLiveCamera& A, const FDioramaLiveCamera& B, double T)
	{
		FDioramaLiveCamera Out;
		Out.TargetM = FMath::Lerp(A.TargetM, B.TargetM, T);
		Out.YawDeg = FMath::Lerp(A.YawDeg, B.YawDeg, T);
		Out.PitchDeg = FMath::Lerp(A.PitchDeg, B.PitchDeg, T);
		Out.DistM = FMath::Lerp(A.DistM, B.DistM, T);
		return Out;
	}
}

AOverseasDioramaStage::AOverseasDioramaStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	StageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
	StageRoot->Mobility = EComponentMobility::Movable;
	SetRootComponent(StageRoot);
	ContentRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ContentRoot"));
	ContentRoot->SetupAttachment(StageRoot);
	TileMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetupAttachment(ContentRoot);
	TileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TileMesh->SetCastShadow(false);
	TileMesh->SetVisibleInSceneCaptureOnly(true);

	LiveCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LiveCapture"));
	LiveCapture->SetupAttachment(StageRoot);
	StillCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("StillCapture"));
	StillCapture->SetupAttachment(StageRoot);
	ConfigureCapture(LiveCapture);
	ConfigureCapture(StillCapture);
	StillCapture->ProjectionType = ECameraProjectionMode::Orthographic;

	OilKeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("OilKeyLight"));
	OilKeyLight->SetupAttachment(StageRoot);
	OilKeyLight->SetRelativeLocation(FVector(-1200.0, -1500.0, 1400.0));
	OilKeyLight->SetIntensityUnits(ELightUnits::Unitless);
	OilKeyLight->SetIntensity(1500000.f);
	OilKeyLight->SetAttenuationRadius(5500.f);
	OilKeyLight->SetCastShadows(false);
	OilKeyLight->SetLightingChannels(false, true, false);
	OilFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("OilFillLight"));
	OilFillLight->SetupAttachment(StageRoot);
	OilFillLight->SetRelativeLocation(FVector(1400.0, 1500.0, 900.0));
	OilFillLight->SetIntensityUnits(ELightUnits::Unitless);
	OilFillLight->SetIntensity(600000.f);
	OilFillLight->SetAttenuationRadius(5500.f);
	OilFillLight->SetCastShadows(false);
	OilFillLight->SetLightingChannels(false, true, false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ToonFinder(
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_FunnyToon.M_FunnyToon"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TruckFinder(
		TEXT("/Game/City_Pack/Meshes/Vehicles/SM_Truck_Large.SM_Truck_Large"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> OilPumpFinder(
		TEXT("/Game/CompanyGrowth/Factory/OilDrilling/SM_OilDrilling.SM_OilDrilling"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> OilAnimationFinder(
		TEXT("/Game/CompanyGrowth/Factory/OilDrilling/uploads_files_4730463_Oil_Drilling_Animated_for_Blender_Anim.uploads_files_4730463_Oil_Drilling_Animated_for_Blender_Anim"));
	ToonMaterial = ToonFinder.Object;
	TruckMesh = TruckFinder.Object;
	CubeMesh = CubeFinder.Object;
	OilPumpMesh = OilPumpFinder.Object;
	OilPumpAnimation = OilAnimationFinder.Object;
}

void AOverseasDioramaStage::ConfigureCapture(USceneCaptureComponent2D* Capture) const
{
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
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
}

void AOverseasDioramaStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLive();
	Super::EndPlay(EndPlayReason);
}

UMaterialInstanceDynamic* AOverseasDioramaStage::MaterialForColor(uint32 PackedColor)
{
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = ColorMaterials.Find(PackedColor))
	{
		return *Found;
	}
	if (!ToonMaterial)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ToonMaterial, this);
	MID->SetVectorParameterValue(TEXT("TintColor"), FLinearColor::FromSRGBColor(FColor(PackedColor)));
	MID->SetScalarParameterValue(TEXT("TexBlend"), 0.f);
	MID->SetScalarParameterValue(TEXT("Brightness"), 1.f);
	MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
	MID->SetScalarParameterValue(TEXT("Emissive"), 0.f);
	ColorMaterials.Add(PackedColor, MID);
	return MID;
}

UProceduralMeshComponent* AOverseasDioramaStage::NewMeshComponent(USceneComponent* Parent, const FVector& RelativeLocation)
{
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
	AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(Parent);
	Mesh->SetRelativeLocation(RelativeLocation);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Mesh->SetVisibleInSceneCaptureOnly(true);
	Mesh->bUseAsyncCooking = false;
	Mesh->RegisterComponent();
	GeneratedComponents.Add(Mesh);
	return Mesh;
}

void AOverseasDioramaStage::CommitGeometry(UProceduralMeshComponent* Target, const FDioramaGeometry& Geometry)
{
	if (!Target)
	{
		return;
	}
	Target->ClearAllMeshSections();
	int32 Section = 0;
	for (const TPair<uint32, FDioramaMeshBucket>& Pair : Geometry.GetBuckets())
	{
		const FDioramaMeshBucket& Bucket = Pair.Value;
		if (Bucket.Triangles.Num() == 0)
		{
			continue;
		}
		Target->CreateMeshSection_LinearColor(Section, Bucket.Vertices, Bucket.Triangles, Bucket.Normals,
			TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		Target->SetMaterial(Section, MaterialForColor(Pair.Key));
		++Section;
	}
}

void AOverseasDioramaStage::ClearGenerated()
{
	for (UPrimitiveComponent* Component : GeneratedComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	GeneratedComponents.Reset();
	ModuleMeshes.Reset();
	Rigs.Reset();
	ModuleOffsetsM.Reset();
	ModuleRectsM.Reset();
	ModuleTypes.Reset();
	if (TileMesh)
	{
		TileMesh->ClearAllMeshSections();
	}
}

void AOverseasDioramaStage::BuildFor(const FOverseasDioramaSpec& Spec, bool bMiniature)
{
	ClearGenerated();
	BuiltSpec = Spec;
	bBuiltMiniature = bMiniature;
	bHasBuild = Spec.HasAny();
	if (!bHasBuild)
	{
		return;
	}

	const TArray<FDioramaLayoutItem> Items = DioramaModules::Layout(Spec, bMiniature);
	FDioramaGeometry Tile;
	TileRectM = DioramaModules::BuildTile(Items, Tile);
	CommitGeometry(TileMesh, Tile);

	for (const FDioramaLayoutItem& Item : Items)
	{
		FDioramaModuleBuild Build;
		Build.Module = Item.Module;
		switch (Item.Module)
		{
		case EDioramaModule::Factory: DioramaModules::BuildFactory(Spec.Factory, bMiniature, Build); break;
		case EDioramaModule::Mine: DioramaModules::BuildMine(Spec.Mine, bMiniature, Build); break;
		case EDioramaModule::Port: DioramaModules::BuildPort(Spec.Port, bMiniature, Build); break;
		}

		UProceduralMeshComponent* ModuleMesh = NewMeshComponent(ContentRoot, FDioramaGeometry::ToUE(Item.OffsetXZ.X, 0.0, Item.OffsetXZ.Y));
		CommitGeometry(ModuleMesh, Build.Static);
		ModuleMeshes.Add(ModuleMesh);
		ModuleOffsetsM.Add(Item.OffsetXZ);
		ModuleRectsM.Add(Item.RectXZ);
		ModuleTypes.Add(Item.Module);

		for (const FDioramaRigDesc& Desc : Build.Rigs)
		{
			FRigRuntime Rig;
			Rig.Kind = static_cast<uint8>(Desc.Kind);
			Rig.BaseLocation = Desc.PivotUE;
			Rig.PathStart = Desc.PathStartUE;
			Rig.PathEnd = Desc.PathEndUE;
			Rig.YawDeg = Desc.YawDeg;
			Rig.Speed = Desc.Speed;
			Rig.Amplitude = Desc.Amplitude;
			Rig.Period = FMath::Max(0.05f, Desc.Period);
			Rig.Phase = static_cast<float>(Rigs.Num()) * 0.37f;
			Rig.bActive = Desc.bActive;

			if (Desc.Kind == EDioramaRigKind::Truck)
			{
				if (bMiniature || !TruckMesh)
				{
					continue;
				}
				UStaticMeshComponent* Truck = NewObject<UStaticMeshComponent>(this);
				AddInstanceComponent(Truck);
				Truck->SetupAttachment(ModuleMesh);
				Truck->SetStaticMesh(TruckMesh);
				// 도시 트럭 재질은 이 격리 무대에서 빛을 못 받아 검게 찍힌다 ― 시설과 같은 툰 색으로 칠한다(차체=밝은 회백, 나머지 슬롯=유리색).
				for (int32 MatIndex = 0; MatIndex < Truck->GetNumMaterials(); ++MatIndex)
				{
					Truck->SetMaterial(MatIndex, MaterialForColor(DioramaModules::Hex(MatIndex == 0 ? DioramaPalette::Machine : DioramaPalette::Dark).ToPackedARGB()));
				}
				Truck->SetRelativeLocation(Desc.PathStartUE.IsNearlyZero() ? Desc.PivotUE : Desc.PathStartUE);
				Truck->SetRelativeRotation(FRotator(0.0, Desc.YawDeg + kDioTruckYawOffsetDeg, 0.0));
				Truck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Truck->SetCastShadow(false);
				Truck->SetVisibleInSceneCaptureOnly(true);
				Truck->RegisterComponent();
				GeneratedComponents.Add(Truck);
				Rig.Component = Truck;
			}
			else if (Desc.Kind == EDioramaRigKind::BeltPackets)
			{
				if (!CubeMesh)
				{
					continue;
				}
				for (int32 i = 0; i < FMath::Clamp(Desc.MaxPackets, 1, 12); ++i)
				{
					UStaticMeshComponent* Packet = NewObject<UStaticMeshComponent>(this);
					AddInstanceComponent(Packet);
					Packet->SetupAttachment(ModuleMesh);
					Packet->SetStaticMesh(CubeMesh);
					Packet->SetMaterial(0, MaterialForColor(Desc.PacketColor.ToPackedARGB()));
					Packet->SetRelativeScale3D(Desc.PacketSizeUE / 100.0);
					Packet->SetRelativeLocation(Desc.PathStartUE);
					Packet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Packet->SetCastShadow(false);
					Packet->SetVisibleInSceneCaptureOnly(true);
					Packet->SetHiddenInGame(!Desc.bActive);
					Packet->RegisterComponent();
					GeneratedComponents.Add(Packet);
					Rig.Packets.Add(Packet);
				}
			}
			else if (Desc.Kind == EDioramaRigKind::OilPump)
			{
				if (!OilPumpMesh || !OilPumpAnimation)
				{
					UE_LOG(LogTemp, Warning, TEXT("Overseas diorama oil pump mesh or animation is missing"));
					continue;
				}
				USkeletalMeshComponent* Pump = NewObject<USkeletalMeshComponent>(this);
				AddInstanceComponent(Pump);
				Pump->SetupAttachment(ModuleMesh);
				Pump->SetSkeletalMesh(OilPumpMesh);
				const FBoxSphereBounds Bounds = OilPumpMesh->GetBounds();
				const double Length = Bounds.BoxExtent.Y * 2.0;
				const double Scale = Length > UE_KINDA_SMALL_NUMBER ? 460.0 * Desc.MeshScale / Length : 1.0;
				Pump->SetRelativeScale3D(FVector(Scale));
				Pump->SetRelativeLocation(Desc.PivotUE + FVector(0.0, 0.0,
					-(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
				Pump->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Pump->SetCanEverAffectNavigation(false);
				Pump->SetCastShadow(false);
				Pump->SetVisibleInSceneCaptureOnly(true);
				Pump->SetLightingChannels(false, true, false);
				Pump->RegisterComponent();
				Pump->SetComponentTickEnabled(false);
				Pump->PlayAnimation(OilPumpAnimation, true);
				Pump->SetPosition(0.f, false);
				Pump->TickAnimation(0.f, false);
				Pump->RefreshBoneTransforms();
				GeneratedComponents.Add(Pump);
				Rig.Component = Pump;
			}
			else
			{
				UProceduralMeshComponent* RigMesh = NewMeshComponent(ModuleMesh, Desc.PivotUE);
				RigMesh->SetRelativeRotation(FRotator(0.0, Desc.YawDeg, 0.0));
				CommitGeometry(RigMesh, Desc.Geometry);
				Rig.Component = RigMesh;
			}
			Rigs.Add(MoveTemp(Rig));
		}
	}

	for (USceneCaptureComponent2D* Capture : { LiveCapture.Get(), StillCapture.Get() })
	{
		Capture->ShowOnlyActors.Reset();
		Capture->ShowOnlyActors.Add(this);
	}

	if (!bLive)
	{
		Goal = ComputeGoal(Spec.PrimaryModule());
		Current = Goal;
		ApplyLiveCamera(Current);
	}
}

FDioramaLiveCamera AOverseasDioramaStage::ComputeGoal(EDioramaModule Module) const
{
	FDioramaLiveCamera Cam;
	int32 Index = ModuleTypes.IndexOfByKey(Module);
	if (Index == INDEX_NONE)
	{
		Index = ModuleTypes.IndexOfByKey(BuiltSpec.PrimaryModule());
	}
	if (Index == INDEX_NONE)
	{
		return Cam;
	}
	const FBox2D& Rect = ModuleRectsM[Index];
	const FVector2D Offset = ModuleOffsetsM[Index];
	const FVector2D Center = Rect.GetCenter() + Offset;
	Cam.TargetM = FVector(Center.X, 0.0, Center.Y);
	const double Width = Rect.Max.X - Rect.Min.X;
	Cam.DistM = ModuleTypes[Index] == EDioramaModule::Factory
		? kDioFactoryDistM
		: FMath::Clamp(Width * kDioDistPerWidth, kDioMinDistM, kDioMaxDistM);
	return Cam;
}

void AOverseasDioramaStage::FocusModule(EDioramaModule Module, bool bInstant)
{
	Goal = ComputeGoal(Module);
	if (bInstant)
	{
		Current = Goal;
		IntroTime = -1.f;
		ApplyLiveCamera(Current);
	}
}

void AOverseasDioramaStage::PlayIntro()
{
	IntroFrom = Goal;
	IntroFrom.DistM = 250.0;
	IntroFrom.PitchDeg = 80.0;
	IntroFrom.YawDeg = -8.0;
	Current = IntroFrom;
	IntroTime = 0.f;
	ContentRoot->SetRelativeLocation(FVector(0.0, 0.0, -kDioIntroRiseM * 100.0));
	ApplyLiveCamera(Current);
}

void AOverseasDioramaStage::StartLive(UTextureRenderTarget2D* Target)
{
	if (!Target)
	{
		return;
	}
	LiveCapture->TextureTarget = Target;
	LiveCapture->bCaptureEveryFrame = true;
	bLive = true;
	SetActorTickEnabled(true);
	ApplyLiveCamera(Current);
}

void AOverseasDioramaStage::CaptureLiveFrameNow()
{
	if (bLive && LiveCapture && LiveCapture->TextureTarget && FApp::CanEverRender())
	{
		ApplyLiveCamera(Current);
		LiveCapture->CaptureScene();
	}
}

void AOverseasDioramaStage::StopLive()
{
	if (LiveCapture)
	{
		LiveCapture->bCaptureEveryFrame = false;
		LiveCapture->TextureTarget = nullptr;
	}
	bLive = false;
	IntroTime = -1.f;
	if (ContentRoot)
	{
		ContentRoot->SetRelativeLocation(FVector::ZeroVector);
	}
	SetActorTickEnabled(false);
}

void AOverseasDioramaStage::ApplyLiveCamera(const FDioramaLiveCamera& Camera)
{
	if (!LiveCapture)
	{
		return;
	}
	const FVector EyeM = CameraLocationM(Camera);
	const FVector LookM(Camera.TargetM.X, -1.5, Camera.TargetM.Z + 0.5);
	const FVector EyeUE = FDioramaGeometry::ToUE(EyeM.X, EyeM.Y, EyeM.Z);
	const FVector LookUE = FDioramaGeometry::ToUE(LookM.X, LookM.Y, LookM.Z);
	LiveCapture->SetRelativeLocation(EyeUE);
	LiveCapture->SetRelativeRotation((LookUE - EyeUE).Rotation());

	double Aspect = 1.0;
	if (const UTextureRenderTarget2D* Target = LiveCapture->TextureTarget)
	{
		Aspect = Target->SizeY > 0 ? static_cast<double>(Target->SizeX) / Target->SizeY : 1.0;
	}
	const double HalfV = FMath::DegreesToRadians(kDioVerticalFovDeg * 0.5);
	LiveCapture->FOVAngle = static_cast<float>(FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(HalfV) * Aspect)));
}

void AOverseasDioramaStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLive)
	{
		return;
	}
	AnimTime += DeltaSeconds;

	if (IntroTime >= 0.f)
	{
		IntroTime += DeltaSeconds;
		const double K = EaseOut(IntroTime / kDioIntroCameraDuration);
		Current = LerpCamera(IntroFrom, Goal, K);
		const double Rise = EaseBack((IntroTime - kDioIntroRiseDelay) / kDioIntroRiseDuration);
		ContentRoot->SetRelativeLocation(FVector(0.0, 0.0, -kDioIntroRiseM * 100.0 * (1.0 - Rise)));
		if (IntroTime >= FMath::Max(kDioIntroCameraDuration, kDioIntroRiseDelay + kDioIntroRiseDuration))
		{
			IntroTime = -1.f;
			ContentRoot->SetRelativeLocation(FVector::ZeroVector);
		}
	}
	else
	{
		const double Dt = DeltaSeconds;
		Current.YawDeg += (Goal.YawDeg - Current.YawDeg) * FMath::Min(1.0, Dt * 8.0);
		Current.PitchDeg += (Goal.PitchDeg - Current.PitchDeg) * FMath::Min(1.0, Dt * 8.0);
		Current.DistM += (Goal.DistM - Current.DistM) * FMath::Min(1.0, Dt * 5.0);
		Current.TargetM = FMath::Lerp(Current.TargetM, Goal.TargetM, FMath::Min(1.0, Dt * 4.0));
	}
	ApplyLiveCamera(Current);
	TickRigs(DeltaSeconds);
}

void AOverseasDioramaStage::TickRigs(float DeltaSeconds)
{
	const float T = AnimTime;
	for (FRigRuntime& Rig : Rigs)
	{
		const EDioramaRigKind Kind = static_cast<EDioramaRigKind>(Rig.Kind);
		const float Wave = FMath::Sin(UE_TWO_PI * (T / Rig.Period) + Rig.Phase);
		if (Kind == EDioramaRigKind::BeltPackets)
		{
			const double Length = FVector::Dist(Rig.PathStart, Rig.PathEnd);
			const int32 Count = Rig.Packets.Num();
			for (int32 i = 0; i < Count; ++i)
			{
				UPrimitiveComponent* Packet = Rig.Packets[i].Get();
				if (!Packet)
				{
					continue;
				}
				Packet->SetHiddenInGame(!Rig.bActive);
				if (!Rig.bActive || Length <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const double Travel = (T * Rig.Speed * 100.0) / Length + static_cast<double>(i) / Count;
				const double Alpha = Travel - FMath::FloorToDouble(Travel);
				Packet->SetRelativeLocation(FMath::Lerp(Rig.PathStart, Rig.PathEnd, Alpha));
			}
			continue;
		}

		USceneComponent* Component = Rig.Component.Get();
		if (!Component)
		{
			continue;
		}
		switch (Kind)
		{
		case EDioramaRigKind::RobotArm:
		case EDioramaRigKind::Excavator:
			Component->SetRelativeRotation(FRotator(0.0, Rig.YawDeg + (Rig.bActive ? Rig.Amplitude * Wave : 0.0), 0.0));
			break;
		case EDioramaRigKind::OilPump:
			if (Rig.bActive)
			{
				USkeletalMeshComponent* Pump = Cast<USkeletalMeshComponent>(Component);
				Pump->TickAnimation(DeltaSeconds, false);
				Pump->RefreshBoneTransforms();
			}
			break;
		case EDioramaRigKind::CraneTrolley:
		{
			const double Alpha = 0.5 - 0.5 * FMath::Cos(UE_TWO_PI * (T / Rig.Period) + Rig.Phase);
			Component->SetRelativeLocation(FMath::Lerp(Rig.PathStart, Rig.PathEnd, Alpha));
			break;
		}
		case EDioramaRigKind::Truck:
		{
			if (Rig.PathStart.Equals(Rig.PathEnd))
			{
				break;
			}
			const double Phase01 = FMath::Fmod(T / Rig.Period + Rig.Phase, 1.0);
			const bool bForward = Phase01 < 0.5;
			const double Alpha = bForward ? Phase01 * 2.0 : 2.0 - Phase01 * 2.0;
			const FVector Dir = (Rig.PathEnd - Rig.PathStart) * (bForward ? 1.0 : -1.0);
			Component->SetRelativeLocation(FMath::Lerp(Rig.PathStart, Rig.PathEnd, Alpha));
			Component->SetRelativeRotation(FRotator(0.0, Dir.Rotation().Yaw + kDioTruckYawOffsetDeg, 0.0));
			break;
		}
		case EDioramaRigKind::Ship:
			Component->SetRelativeLocation(Rig.BaseLocation + FVector(0.0, 0.0, Rig.Amplitude * 100.0 * Wave));
			break;
		case EDioramaRigKind::Flare:
			Component->SetRelativeScale3D(FVector(1.0 + 0.15 * FMath::Sin(T * 9.0 + Rig.Phase)));
			break;
		default:
			break;
		}
	}
}

void AOverseasDioramaStage::SetPacketsHidden(bool bHide)
{
	for (FRigRuntime& Rig : Rigs)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Packet : Rig.Packets)
		{
			if (UPrimitiveComponent* P = Packet.Get())
			{
				P->SetHiddenInGame(bHide || !Rig.bActive);
			}
		}
	}
}

bool AOverseasDioramaStage::CaptureStill(UTextureRenderTarget2D*& InOutTarget, FDioramaStillInfo& OutInfo)
{
	if (!bHasBuild || !bBuiltMiniature)
	{
		return false;
	}

	const double X0 = TileRectM.Min.X, X1 = TileRectM.Max.X, Z0 = TileRectM.Min.Y, Z1 = TileRectM.Max.Y;
	const double Cx = (X0 + X1) * 0.5, Cz = (Z0 + Z1) * 0.5;
	const double Yw = FMath::DegreesToRadians(kDioStillYawDeg);
	const double Pt = FMath::DegreesToRadians(kDioStillPitchDeg);
	const FVector Eye(Cx + kDioStillDistM * FMath::Cos(Pt) * FMath::Sin(Yw), kDioStillDistM * FMath::Sin(Pt),
		Cz + kDioStillDistM * FMath::Cos(Pt) * FMath::Cos(Yw));
	const FVector Target(Cx, 0.0, Cz);
	// three.js lookAt 과 같은 카메라 기저(오른손, 위 = +Y)
	const FVector AxisZ = (Eye - Target).GetSafeNormal();
	const FVector AxisX = FVector::CrossProduct(FVector(0.0, 1.0, 0.0), AxisZ).GetSafeNormal();
	const FVector AxisY = FVector::CrossProduct(AxisZ, AxisX);

	double A = TNumericLimits<double>::Max(), B = -A, C = A, D = -A;
	for (double X : { X0, X1 })
	{
		for (double Z : { Z0, Z1 })
		{
			for (double Y : { kDioStillMinY, kDioStillMaxY })
			{
				const FVector Rel = FVector(X, Y, Z) - Eye;
				const double U = FVector::DotProduct(Rel, AxisX);
				const double V = FVector::DotProduct(Rel, AxisY);
				A = FMath::Min(A, U); B = FMath::Max(B, U);
				C = FMath::Min(C, V); D = FMath::Max(D, V);
			}
		}
	}
	A -= 1.0; B += 1.0; C -= 1.0; D += 1.0;

	const int32 W = FMath::Clamp(FMath::CeilToInt((B - A) * StillPixelsPerMeter), 1, kDioStillMaxPx);
	const int32 H = FMath::Clamp(FMath::CeilToInt((D - C) * StillPixelsPerMeter), 1, kDioStillMaxPx);
	const FVector PivotRel = Target - Eye;
	OutInfo.SizePx = FIntPoint(W, H);
	OutInfo.PivotPx = FVector2D((FVector::DotProduct(PivotRel, AxisX) - A) * StillPixelsPerMeter,
		(D - FVector::DotProduct(PivotRel, AxisY)) * StillPixelsPerMeter);

	if (!FApp::CanEverRender())
	{
		return true;
	}

	if (!InOutTarget)
	{
		InOutTarget = NewObject<UTextureRenderTarget2D>(this);
		InOutTarget->RenderTargetFormat = RTF_RGBA16f;
		InOutTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		InOutTarget->InitAutoFormat(W, H);
		InOutTarget->UpdateResourceImmediate(true);
	}
	else if (InOutTarget->SizeX != W || InOutTarget->SizeY != H)
	{
		InOutTarget->ResizeTarget(W, H);
	}

	const FVector CenterM = Eye + AxisX * ((A + B) * 0.5) + AxisY * ((C + D) * 0.5);
	const FVector ForwardUE = MockupDirToUE(-AxisZ);
	const FVector UpUE = MockupDirToUE(AxisY);
	StillCapture->SetRelativeLocation(FDioramaGeometry::ToUE(CenterM.X, CenterM.Y, CenterM.Z));
	StillCapture->SetRelativeRotation(FRotationMatrix::MakeFromXZ(ForwardUE, UpUE).Rotator());
	StillCapture->OrthoWidth = static_cast<float>((B - A) * 100.0);
	StillCapture->TextureTarget = InOutTarget;

	const FVector SavedLocation = ContentRoot->GetRelativeLocation();
	ContentRoot->SetRelativeLocation(FVector::ZeroVector);
	ContentRoot->SetRelativeScale3D(FVector(1.0, 1.0, StillHeightScale));
	SetPacketsHidden(true);
	StillCapture->CaptureScene();
	ContentRoot->SetRelativeScale3D(FVector::OneVector);
	ContentRoot->SetRelativeLocation(SavedLocation);
	SetPacketsHidden(false);
	return true;
}
