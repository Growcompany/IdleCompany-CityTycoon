#include "Office/OfficeDistantBackdrop.h"

#include "EngineUtils.h"
#include "Entity/Ambient/TrafficManager.h"
#include "Misc/ScopeExit.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/Float16Color.h"
#include "ProceduralMeshComponent.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "TimeCycle/TimeCycleSky.h"
#include "UObject/StrongObjectPtr.h"

namespace OfficeDistantBackdropCVars
{
	TAutoConsoleVariable<int32> CityPhotoResolution(
		TEXT("cg.Office.CityPhotoResolution"), UOfficeDistantBackdrop::MaxSourceFaceResolution,
		TEXT("도시 사진 1면당 캡처 해상도(px). 256~1024, 4의 배수로 내림."), ECVF_Default);

	// 기본 0 — GLES 에서 RGBA16f 캡처는 2D 타깃에 슬라이스를 거는 경로를 타 OpenGLRenderTarget.cpp:282 어서션으로 죽는다
	// (갤럭시 A9/Adreno 512 실기기 100% 재현, RGBA8 로 바꾸면 같은 캡처가 그대로 돈다)
	TAutoConsoleVariable<int32> CityPhotoHDR(
		TEXT("cg.Office.CityPhotoHDR"), 0,
		TEXT("0=캡처 타깃 RGBA8(기본, GLES 안전). 1=RGBA16f."), ECVF_Default);
}

int32 UOfficeDistantBackdrop::GetSourceFaceResolution()
{
	const int32 Clamped = FMath::Clamp(
		OfficeDistantBackdropCVars::CityPhotoResolution.GetValueOnGameThread(), 256, MaxSourceFaceResolution);
	return Clamped - (Clamped % 4);
}

bool UOfficeDistantBackdrop::UseHDRCapture()
{
	return OfficeDistantBackdropCVars::CityPhotoHDR.GetValueOnGameThread() != 0;
}

FRotator UOfficeDistantBackdrop::GetFaceRotation(const int32 FaceIndex)
{
	if (FaceIndex == 0) { return FRotator(0.f, 90.f, 0.f); }
	if (FaceIndex == 1) { return FRotator(0.f, 180.f, 0.f); }
	return FRotator(-90.f, 0.f, 0.f);
}

void UOfficeDistantBackdrop::BuildFaceGeometry(const int32 FaceIndex, TArray<FVector>& Vertices, TArray<FVector2D>& UVs, const float DistanceCm)
{
	const FRotator Rotation = GetFaceRotation(FaceIndex);
	const FVector Forward = Rotation.Vector() * DistanceCm;
	const FVector Right = Rotation.RotateVector(FVector::RightVector) * DistanceCm;
	const FVector Up = Rotation.RotateVector(FVector::UpVector) * DistanceCm;
	Vertices = {Forward - Right + Up, Forward + Right + Up, Forward + Right - Up, Forward - Right - Up};
	UVs = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)};
}

bool UOfficeDistantBackdrop::HasCapture() const
{
	if (RenderTargets.Num() != FaceCount) { return false; }
	for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
	{
		if (!IsValid(RenderTargets[FaceIndex])) { return false; }
	}
	return true;
}

bool UOfficeDistantBackdrop::CaptureSource(AActor* SourceOwner, ATimeCycleSky* Sky,
	const TArray<UPrimitiveComponent*>& Sources, const FTransform& CaptureFrame)
{
	if (HasCapture()) { return true; }
	if (GUsingNullRHI || !IsValid(SourceOwner) || !IsValid(Sky) || !SourceOwner->GetWorld()
		|| Sky->GetWorld() != SourceOwner->GetWorld() || Sources.IsEmpty()
		|| CaptureFrame.ContainsNaN()) { return false; }
	UWorld* SourceWorld = SourceOwner->GetWorld();
	for (UPrimitiveComponent* Source : Sources)
	{
		if (!IsValid(Source) || !Source->IsRegistered() || Source->GetWorld() != SourceWorld) { return false; }
	}
	Reset();
	const double Started = FPlatformTime::Seconds();
	const int32 FaceResolution = GetSourceFaceResolution();
	const bool bHDRCapture = UseHDRCapture();
	CaptureOrigin = CaptureFrame.GetLocation();
	CaptureFrameTransform = CaptureFrame;
	// 여러 방향이 같은 차량 위치를 담고, 실패 경로에서도 원래 시뮬레이션 상태를 돌려준다.
	TArray<TPair<TWeakObjectPtr<ATrafficManager>, bool>> TrafficTickStates;
	for (TActorIterator<ATrafficManager> It(SourceWorld); It; ++It)
	{
		TrafficTickStates.Emplace(*It, It->IsActorTickEnabled());
		It->SetActorTickEnabled(false);
	}
	ON_SCOPE_EXIT
	{
		for (const auto& State : TrafficTickStates)
		{
			if (ATrafficManager* Traffic = State.Key.Get()) { Traffic->SetActorTickEnabled(State.Value); }
		}
	};
	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(SourceOwner, NAME_None, RF_Transient);
	Capture->SetupAttachment(SourceOwner->GetRootComponent());
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->FOVAngle = 90.f;
	Capture->MaxViewDistanceOverride = 60000.f;
	Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Capture->PostProcessSettings.AutoExposureBias = 0.f;
	Capture->PostProcessSettings.bOverride_BloomIntensity = true;
	Capture->PostProcessSettings.BloomIntensity = 0.f;
	Capture->SetWorldLocation(CaptureOrigin);
	ON_SCOPE_EXIT
	{
		Capture->TextureTarget = nullptr;
		Capture->ClearShowOnlyComponents();
		Capture->DestroyComponent();
	};
	for (UPrimitiveComponent* Source : Sources)
	{
		Capture->ShowOnlyComponent(Source);
	}
	Capture->RegisterComponent();
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(true);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->ShowFlags.SetEyeAdaptation(false);
	const auto MakeTarget = [bHDRCapture](const int32 Resolution)
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		Target->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		Target->AddressX = TA_Clamp;
		Target->AddressY = TA_Clamp;
		Target->Filter = TF_Bilinear;
		Target->bAutoGenerateMips = false;
		Target->RenderTargetFormat = bHDRCapture ? RTF_RGBA16f : RTF_RGBA8;
		Target->InitCustomFormat(Resolution, Resolution, bHDRCapture ? PF_FloatRGBA : PF_B8G8R8A8, true);
		Target->UpdateResourceImmediate(true);
		return Target;
	};
	const auto CaptureFaces = [&]()
	{
		for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
		{
			UTextureRenderTarget2D* Target = MakeTarget(FaceResolution);
			RenderTargets.Add(Target);
			Capture->TextureTarget = Target;
			Capture->SetWorldRotation(CaptureFrame.GetRotation() * GetFaceRotation(FaceIndex).Quaternion());
			Capture->CaptureScene();
			++CaptureCount;
		}
		FlushRenderingCommands();
		return true;
	};
	const bool bDayCaptured = Sky->WithDaylightForCapture(CaptureFaces);
	bool bHasCityPixels = false;
	if (bDayCaptured)
	{
		constexpr int32 SampleSize = 64;
		const int32 SampleStart = (FaceResolution - SampleSize) / 2;
		const FIntRect SampleRect(SampleStart, SampleStart, SampleStart + SampleSize, SampleStart + SampleSize);
		for (UTextureRenderTarget2D* Target : RenderTargets)
		{
			FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
			if (!Resource) { Reset(); return false; }
			// 알파는 불투명도가 아니라 배경 투과율이다 — 1 미만이면서 색이 있으면 도시가 찍힌 것
			if (bHDRCapture)
			{
				TArray<FFloat16Color> Pixels;
				if (!Resource->ReadFloat16Pixels(Pixels, FReadSurfaceDataFlags(), SampleRect)) { Reset(); return false; }
				for (const FFloat16Color& Pixel : Pixels)
				{
					if (Pixel.A.GetFloat() < 0.9f
						&& FMath::Max3(Pixel.R.GetFloat(), Pixel.G.GetFloat(), Pixel.B.GetFloat()) > 0.00001f)
					{
						bHasCityPixels = true;
						break;
					}
				}
			}
			else
			{
				TArray<FColor> Pixels;
				if (!Resource->ReadPixels(Pixels, FReadSurfaceDataFlags(), SampleRect)) { Reset(); return false; }
				for (const FColor& Pixel : Pixels)
				{
					if (Pixel.A < 230 && FMath::Max3(Pixel.R, Pixel.G, Pixel.B) > 0)
					{
						bHasCityPixels = true;
						break;
					}
				}
			}
		}
	}
	if (!bDayCaptured || !bHasCityPixels) { Reset(); return false; }
	Capture->TextureTarget = nullptr;
	if (!SoftenSourceCapture(SourceOwner))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeBackdrop] Source filtering unavailable; retaining original city captures"));
	}
	CaptureMilliseconds = static_cast<float>((FPlatformTime::Seconds() - Started) * 1000.0);
	return HasCapture();
}

bool UOfficeDistantBackdrop::SoftenSourceCapture(AActor* SourceOwner)
{
	if (GUsingNullRHI || !IsValid(SourceOwner) || !SourceOwner->GetWorld()
		|| RenderTargets.Num() != FaceCount) { return false; }
	for (UTextureRenderTarget2D* Target : RenderTargets)
	{
		if (!IsValid(Target) || Target->SizeX <= 0 || Target->SizeY <= 0) { return false; }
	}
	UMaterialInterface* FilterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Office/Exterior/Materials/M_OfficeSourceSoften.M_OfficeSourceSoften"))).LoadSynchronous();
	if (!FilterMaterial || FilterMaterial->GetBlendMode() != BLEND_AlphaComposite) { return false; }
	TArray<TStrongObjectPtr<UTextureRenderTarget2D>> FilteredTargets;
	for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
	{
		UTextureRenderTarget2D* SourceTarget = RenderTargets[FaceIndex];
		TStrongObjectPtr<UTextureRenderTarget2D> Filtered(NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
		// SceneCapture의 알파는 불투명도가 아니라 배경 투과율이다.
		Filtered->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		Filtered->AddressX = TA_Clamp;
		Filtered->AddressY = TA_Clamp;
		Filtered->Filter = TF_Bilinear;
		Filtered->bAutoGenerateMips = false;
		Filtered->RenderTargetFormat = RTF_RGBA16f;
		Filtered->InitCustomFormat(SourceDisplayResolution, SourceDisplayResolution, PF_FloatRGBA, true);
		Filtered->UpdateResourceImmediate(true);
		TStrongObjectPtr<UMaterialInstanceDynamic> FilterMID(UMaterialInstanceDynamic::Create(FilterMaterial, GetTransientPackage()));
		FilterMID->SetTextureParameterValue(TEXT("SourceCapture"), SourceTarget);
		FilterMID->SetVectorParameterValue(TEXT("TexelSize"), FLinearColor(1.f / SourceTarget->SizeX, 1.f / SourceTarget->SizeY, 0.f, 0.f));
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(SourceOwner, Filtered.Get(), FilterMID.Get());
		FlushRenderingCommands();
		FilteredTargets.Add(MoveTemp(Filtered));
	}
	for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
	{
		RenderTargets[FaceIndex]->ReleaseResource();
		RenderTargets[FaceIndex] = FilteredTargets[FaceIndex].Get();
	}
	return true;
}

bool UOfficeDistantBackdrop::ApplyCapture(AActor* DisplayOwner, UMaterialInterface* Material,
	const UOfficeDistantBackdrop* SourceCapture, const FTransform& DestinationFrame, const float NightBrightness)
{
	if (!IsValid(DisplayOwner) || !DisplayOwner->GetWorld() || !DisplayOwner->GetRootComponent() || !Material
		|| !IsValid(SourceCapture) || !SourceCapture->HasCapture() || !DestinationFrame.IsValid()
		|| !FMath::IsFinite(NightBrightness) || NightBrightness < 0.f) { return false; }
	if (SourceCapture == this && Display && Display->GetOwner() == DisplayOwner)
	{
		for (UMaterialInstanceDynamic* MID : DisplayMaterials)
		{
			if (MID) { MID->SetScalarParameterValue(TEXT("NightBrightness"), NightBrightness); }
		}
		SetActive(true);
		Display->SetWorldTransform(DestinationFrame);
		CaptureOrigin = DestinationFrame.GetLocation();
		CaptureFrameTransform = DestinationFrame;
		return true;
	}
	const TArray<UTextureRenderTarget2D*> DayTargets = SourceCapture->GetRenderTargets();
	const int32 SourceCaptureCount = SourceCapture->GetCaptureCount();
	const float SourceCaptureMilliseconds = SourceCapture->GetCaptureMilliseconds();
	Reset();
	for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
	{
		RenderTargets.Add(DayTargets[FaceIndex]);
	}
	CaptureOrigin = DestinationFrame.GetLocation();
	CaptureFrameTransform = DestinationFrame;
	CaptureCount = SourceCaptureCount;
	CaptureMilliseconds = SourceCaptureMilliseconds;
	Display = NewObject<UProceduralMeshComponent>(DisplayOwner, NAME_None, RF_Transient);
	Display->SetupAttachment(DisplayOwner->GetRootComponent());
	Display->SetMobility(EComponentMobility::Movable);
	Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Display->SetCanEverAffectNavigation(false);
	Display->SetCastShadow(false);
	Display->SetTranslucentSortPriority(-1);
	Display->PrimaryComponentTick.bCanEverTick = false;
	Display->SetWorldTransform(DestinationFrame);
	for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
	{
		TArray<FVector> Vertices;
		TArray<FVector2D> UVs;
		BuildFaceGeometry(FaceIndex, Vertices, UVs);
		TArray<FVector> Normals;
		Normals.Init(-GetFaceRotation(FaceIndex).Vector(), Vertices.Num());
		Display->CreateMeshSection_LinearColor(FaceIndex, Vertices, {0, 1, 2, 0, 2, 3}, Normals, UVs, {}, {}, false);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
		MID->SetTextureParameterValue(TEXT("DayCapture"), RenderTargets[FaceIndex]);
		MID->SetScalarParameterValue(TEXT("NightBrightness"), NightBrightness);
		DisplayMaterials.Add(MID);
		Display->SetMaterial(FaceIndex, MID);
	}
	Display->RegisterComponent();
	SetActive(true);
	return true;
}

void UOfficeDistantBackdrop::SetActive(const bool bNewActive)
{
	bActive = bNewActive && Display && HasCapture();
	if (Display) { Display->SetVisibility(bActive); }
}

void UOfficeDistantBackdrop::Reset()
{
	if (Display) { Display->DestroyComponent(); Display = nullptr; }
	RenderTargets.Reset();
	DisplayMaterials.Reset();
	CaptureFrameTransform = FTransform::Identity;
	bActive = false;
}

TArray<UTextureRenderTarget2D*> UOfficeDistantBackdrop::GetRenderTargets() const
{
	TArray<UTextureRenderTarget2D*> Result;
	for (UTextureRenderTarget2D* Target : RenderTargets) { Result.Add(Target); }
	return Result;
}
