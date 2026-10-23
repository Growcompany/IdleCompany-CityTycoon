// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#include "TimeCycle/TimeCycleSky.h"
#include "EngineUtils.h"
#include "TimeCycle/TimeCycleManager.h"
#include "TimeCycle/TimeCycleSkyLighting.h"
#include "TimeCycle/TimeCycleSkyVisualProfile.h"
#include "Components/PostProcessComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SphereReflectionCapture.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/TextureCube.h"
#include "Engine/PostProcessVolume.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "RenderingThread.h"

namespace
{
	constexpr TCHAR E6ReflectionCubemapPath[] =
		TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/TC_StudioReflect.TC_StudioReflect");

	bool IsFiniteColor(const FLinearColor& Color)
	{
		return FMath::IsFinite(Color.R)
			&& FMath::IsFinite(Color.G)
			&& FMath::IsFinite(Color.B)
			&& FMath::IsFinite(Color.A);
	}

	bool IsValidNightLook(const FTimeCycleSkyNightLook& Look)
	{
		return FMath::IsFinite(Look.SkyNightIntensity)
			&& Look.SkyNightIntensity >= 0.0f
			&& FMath::IsFinite(Look.NightDirectionalIntensity)
			&& Look.NightDirectionalIntensity >= 0.0f
			&& FMath::IsFinite(Look.NightDirectionalTemperature)
			&& Look.NightDirectionalTemperature >= 1000.0f
			&& IsFiniteColor(Look.NightAmbientColor)
			&& FMath::IsFinite(Look.NightExposureBias);
	}

	void ApplyMobileVisualProfile(
		FPostProcessSettings& Settings,
		const FTimeCycleSkyVisualProfile& Profile,
		UTexture* ColorGradingLUT)
	{
		Settings.bOverride_ColorGradingLUT = true;
		Settings.ColorGradingLUT = ColorGradingLUT;
		Settings.bOverride_ColorGradingIntensity = true;
		Settings.ColorGradingIntensity = Profile.LutIntensity;

		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(Profile.Saturation);
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(Profile.Contrast);
		Settings.bOverride_ColorGain = true;
		Settings.ColorGain = FVector4(Profile.Gain);
		Settings.bOverride_ColorSaturationMidtones = true;
		Settings.ColorSaturationMidtones = FVector4(Profile.MidtoneSaturation);
		Settings.bOverride_ColorGainMidtones = true;
		Settings.ColorGainMidtones = FVector4(Profile.MidtoneGain);

		Settings.bOverride_Sharpen = true;
		Settings.Sharpen = Profile.Sharpen;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = Profile.BloomIntensity;
		Settings.bOverride_BloomThreshold = true;
		Settings.BloomThreshold = Profile.BloomThreshold;
		Settings.bOverride_ColorCorrectionShadowsMax = true;
		Settings.ColorCorrectionShadowsMax = Profile.ShadowsMax;
		Settings.bOverride_ColorCorrectionHighlightsMin = true;
		Settings.ColorCorrectionHighlightsMin = Profile.HighlightsMin;
	}
}

ATimeCycleSky::ATimeCycleSky()
{
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> WindowMPC(
		TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/MPC_WindowLight.MPC_WindowLight"));
	CaptureWindowLightMPC = WindowMPC.Object;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	MobileNightLook.SkyNightIntensity = 0.17f;
	MobileNightLook.NightDirectionalIntensity = 0.07f;
	MobileNightLook.NightDirectionalTemperature = 9000.0f;
	MobileNightLook.NightAmbientColor =
		FLinearColor(0.3762621284f, 0.4564110339f, 0.5775804520f);
	MobileNightLook.NightExposureBias = 0.20f;

	// Root 컴포넌트 생성
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

#if WITH_EDITORONLY_DATA
	EditorPreviewPostProcess = CreateEditorOnlyDefaultSubobject<UPostProcessComponent>(
		TEXT("EditorPreviewPostProcess"),
		true);
	if (EditorPreviewPostProcess)
	{
		EditorPreviewPostProcess->SetupAttachment(RootComponent);
		EditorPreviewPostProcess->bEnabled = false;
		EditorPreviewPostProcess->bUnbound = true;
		EditorPreviewPostProcess->Priority = 1000000.0f;
		EditorPreviewPostProcess->BlendWeight = 1.0f;
		EditorPreviewPostProcess->Settings.bOverride_AutoExposureMethod = true;
		EditorPreviewPostProcess->Settings.AutoExposureMethod = AEM_Manual;
		EditorPreviewPostProcess->Settings.bOverride_AutoExposureBias = true;
		EditorPreviewPostProcess->Settings.AutoExposureBias = DayExposureBias;
	}
#endif

	// SunLight 생성
	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(RootComponent);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->Intensity = 3.0f;
	SunLight->bUseTemperature = true;
	SunLight->Temperature = 6500.0f;
	SunLight->bEnableLightShaftOcclusion = false;
	SunLight->DynamicShadowDistanceMovableLight = 20000.0f;
	SunLight->bAffectsWorld = true;
	SunLight->ForwardShadingPriority = 1;  // Forward Shading 우선순위 (높음)
	SunLight->SetCastShadows(false);  // [Perf] 단일 낮/밤 방향광의 도시 전체 동적 CSM 제거.

	// SkyLight 생성 — 환경광(햇빛 안 받는 면 채움). 모바일 성능 최우선: 런타임 캡처 0.
	// SkyAtmosphere 실시간 캡처는 (1) 매 프레임 GPU 비용 (2) 밤=어두운 실하늘이라 밤을 못 밝힘.
	// → 정적(Specified) 큐브맵을 소스로 굽고 강도만 낮/밤 보간 = 캡처 비용 0 + 밤 밝기 완전 제어.
	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);   // 강도 런타임 변경 위해 Movable
	SkyLight->SourceType = SLS_SpecifiedCubemap;          // 실시간 캡처 안 씀(핵심)
	SkyLight->bRealTimeCapture = false;
	static ConstructorHelpers::FObjectFinder<UTextureCube> AmbientCube(TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
	if (AmbientCube.Succeeded())
	{
		AmbientCubemapAsset = AmbientCube.Object;
		SkyLight->Cubemap = AmbientCubemapAsset;          // 정적 환경광 소스(밝은 낮 하늘)
	}
	SkyLight->Intensity = SkyDayIntensity;               // 초기값(Tick이 낮/밤으로 덮음)
	SkyLight->LightColor = FColor(213, 213, 213);        // 기존 BP 값 이전
	SkyLight->CastShadows = false;                       // 환경광 그림자 끔(저비용)
}

void ATimeCycleSky::BeginPlay()
{
	Super::BeginPlay();

	if (SunLight)
	{
		SunLight->SetCastShadows(false);
	}

	// BP 인스턴스에 옛 오버라이드(실시간 캡처/CapturedScene)가 남아 있어도 런타임에 강제 —
	// 모바일 성능: 런타임 캡처 0 보장 + 정적 큐브맵으로 환경광 항상 유효(검은 캡처 버그 불가).
	if (SkyLight)
	{
		SkyLight->SourceType = SLS_SpecifiedCubemap;
		SkyLight->bRealTimeCapture = false;
		if (!SkyLight->Cubemap && AmbientCubemapAsset)
		{
			SkyLight->Cubemap = AmbientCubemapAsset;
		}
		SkyLight->MarkRenderStateDirty();
		SkyLight->RecaptureSky(); // 정적 큐브맵 → SH 1회 빌드(씬 렌더 아님, 저비용). 이후 캡처 없음.
	}
	BindVisualTimeManager();
}

bool ATimeCycleSky::WithDaylightForCapture(TFunctionRef<bool()> CaptureOperation)
{
	if (!IsInGameThread() || !SunLight || !SkyLight || !GetWorld() || !CaptureWindowLightMPC) { return false; }
	UExponentialHeightFogComponent* CaptureFog = bApplyOfficeFogTint ? CachedOfficeFog.Get() : nullptr;
	// Office의 원래 낮색을 알기 전에는 밤색으로 영구 배경을 굽지 않는다.
	if (bApplyOfficeFogTint && !CaptureFog) { return false; }
	const FLinearColor SavedFogColor = CaptureFog ? CaptureFog->FogInscatteringLuminance : FLinearColor::White;
	UMaterialParameterCollectionInstance* MPC = GetWorld()->GetParameterCollectionInstance(CaptureWindowLightMPC);
	float SavedNightIntensity = 0.f;
	if (!MPC || !MPC->GetScalarParameterValue(TEXT("Night_Intensity"), SavedNightIntensity)) { return false; }
	const FTransform SavedSunTransform = SunLight->GetRelativeTransform();
	const float SavedSunIntensity = SunLight->Intensity;
	const float SavedTemperature = SunLight->Temperature;
	const bool bSavedUseTemperature = SunLight->bUseTemperature;
	const FColor SavedSunColor = SunLight->LightColor;
	const float SavedSkyIntensity = SkyLight->Intensity;
	const FColor SavedSkyColor = SkyLight->LightColor;
	TimeCycleSkyLighting::FParameters Parameters;
	Parameters.CurrentHour = 12.f;
	Parameters.SunMaxIntensity = SunMaxIntensity;
	Parameters.SkyDayIntensity = SkyDayIntensity;
	Parameters.SolarPitch = -90.f;
	Parameters.SunAzimuth = SunAzimuth;
	Parameters.BaseSkyColor = BaseSkyColor;
	const TimeCycleSkyLighting::FState Day = TimeCycleSkyLighting::Calculate(Parameters);
	SunLight->SetIntensity(Day.DirectionalIntensity);
	SunLight->SetUseTemperature(true);
	SunLight->SetTemperature(Day.DirectionalTemperature);
	SunLight->SetLightColor(FLinearColor::White);
	SunLight->SetRelativeRotation(FRotator(Day.DirectionalPitch, Day.DirectionalYaw, 0.f));
	SkyLight->SetIntensity(Day.SkyIntensity);
	SkyLight->SetLightColor(Day.SkyColor);
	MPC->SetScalarParameterValue(TEXT("Night_Intensity"), 0.f);
	if (CaptureFog) { CaptureFog->SetFogInscatteringColor(OfficeDayFogColor); }
	const bool bCaptured = CaptureOperation();
	// 모든 캡처가 낮 조명을 소비한 뒤 같은 프레임 안에서 원래 렌더 상태로 복원한다.
	FlushRenderingCommands();
	SunLight->SetRelativeTransform(SavedSunTransform);
	SunLight->SetIntensity(SavedSunIntensity);
	SunLight->SetUseTemperature(bSavedUseTemperature);
	SunLight->SetTemperature(SavedTemperature);
	SunLight->LightColor = SavedSunColor;
	SunLight->MarkRenderStateDirty();
	SkyLight->SetIntensity(SavedSkyIntensity);
	SkyLight->LightColor = SavedSkyColor;
	SkyLight->MarkRenderStateDirty();
	MPC->SetScalarParameterValue(TEXT("Night_Intensity"), SavedNightIntensity);
	if (CaptureFog) { CaptureFog->SetFogInscatteringColor(SavedFogColor); }
	GetWorld()->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
	return bCaptured;
}

void ATimeCycleSky::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ATimeCycleManager* BoundManager = BoundVisualTimeManager.Get())
	{
		BoundManager->OnVisualTimeChanged.RemoveDynamic(this, &ATimeCycleSky::HandleVisualTimeChanged);
	}
	BoundVisualTimeManager.Reset();
	RestoreOfficeFogTint();
	Super::EndPlay(EndPlayReason);
}

FTimeCycleSkyDayLook ATimeCycleSky::GetDayLook() const
{
	FTimeCycleSkyDayLook Look;
	Look.SunMaxIntensity = SunMaxIntensity;
	Look.SkyDayIntensity = SkyDayIntensity;
	Look.DayExposureBias = DayExposureBias;
	return Look;
}

FTimeCycleSkyNightLook ATimeCycleSky::GetNightLook() const
{
	UWorld* SkyWorld = GetWorld();
	if (SkyWorld
		&& SkyWorld->IsGameWorld()
		&& SkyWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1)
	{
		return MobileNightLook;
	}

	FTimeCycleSkyNightLook Look;
	Look.SkyNightIntensity = SkyNightIntensity;
	Look.NightDirectionalIntensity = NightDirectionalIntensity;
	Look.NightDirectionalTemperature = NightDirectionalTemperature;
	Look.NightAmbientColor = NightAmbientColor;
	Look.NightExposureBias = NightExposureBias;
	return Look;
}

float ATimeCycleSky::GetTwilightApexHour(bool bDawn) const
{
	// 밤 경계는 시계가 단일 소스다. 매니저가 없으면 Tick 과 같은 폴백을 쓴다.
	float SunRiseHour = 7.0f;
	float SunSetHour = 19.0f;
	if (ATimeCycleManager* Manager = GetTimeCycleManager())
	{
		const FTimeCycleCode RiseCode = Manager->GetSunRiseTime();
		const FTimeCycleCode SetCode = Manager->GetSunSetTime();
		SunRiseHour = RiseCode.Hours + RiseCode.Minutes / 60.0f;
		SunSetHour = SetCode.Hours + SetCode.Minutes / 60.0f;
	}

	const float Apex = FMath::Clamp(TwilightWarmApex, 0.01f, 0.99f);
	if (bDawn)
	{
		const float Blend = FMath::Max(0.01f, DawnBlendHours);
		const float Span = Blend + TimeCycleSkyLighting::ClampDawnLeadHours(
			PreSunriseTwilightHours,
			SunRiseHour,
			Blend);
		// 여명 구간은 SunRise 에서 끝나므로 정점은 끝에서 (1-Apex) 만큼 앞이다.
		return SunRiseHour - Span * (1.0f - Apex);
	}

	const float Blend = FMath::Max(0.01f, DuskBlendHours);
	const float Span = Blend + TimeCycleSkyLighting::ClampDuskTailHours(
		PostSunsetTwilightHours,
		SunSetHour);
	return (SunSetHour - Blend) + Span * Apex;
}

bool ATimeCycleSky::ApplyTransientNightLook(const FTimeCycleSkyNightLook& NewLook)
{
#if WITH_EDITOR
	UWorld* SkyWorld = GetWorld();
	if (!HasAuthority() || !SkyWorld || !SkyWorld->IsGameWorld() || !IsValidNightLook(NewLook))
	{
		return false;
	}

	if (SkyWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1)
	{
		MobileNightLook = NewLook;
	}
	else
	{
		SkyNightIntensity = NewLook.SkyNightIntensity;
		NightDirectionalIntensity = NewLook.NightDirectionalIntensity;
		NightDirectionalTemperature = NewLook.NightDirectionalTemperature;
		NightAmbientColor = NewLook.NightAmbientColor;
		NightExposureBias = NewLook.NightExposureBias;
	}
	return true;
#else
	(void)NewLook;
	return false;
#endif
}

bool ATimeCycleSky::ApplyTransientSunAzimuth(float NewAzimuth)
{
#if WITH_EDITOR
	UWorld* SkyWorld = GetWorld();
	if (!HasAuthority() || !SkyWorld || !SkyWorld->IsGameWorld() || !FMath::IsFinite(NewAzimuth))
	{
		return false;
	}

	SunAzimuth = TimeCycleSkyLighting::NormalizeSunAzimuth(NewAzimuth);
	return true;
#else
	(void)NewAzimuth;
	return false;
#endif
}

float ATimeCycleSky::GetCycleSunPitch(const FTimeCycleCode& Time) const
{
	if (!GetTimeCycleManager())
		return 0.f;

	int32 CurrentTimeSeconds = Time.ToSeconds();

#if WITH_EDITOR
	// 에디터에서 게임 실행 중이 아니면 EditorPreviewHour 사용
	if (!GetWorld()->IsGameWorld())
	{
		CurrentTimeSeconds = FMath::FloorToInt(EditorPreviewHour * 3600.0f);
	}
#endif
	if (CurrentTimeSeconds == SunPitchCacheTimeStamp)
	{
		return SunPitchCache;
	}

	SunPitchCacheTimeStamp = CurrentTimeSeconds;

	int32 SunRiseSeconds = GetTimeCycleManager()->GetSunRiseTime().ToSeconds();
	constexpr int32 NoonSeconds = 43200;
	int32 SunSetSeconds = GetTimeCycleManager()->GetSunSetTime().ToSeconds();
	constexpr int32 MidnightSeconds = 86400;

	constexpr float SunRiseAngle = 0;
	constexpr float SunNoonAngle = -90;
	constexpr float SunSetAngle = -180;
	constexpr float SunMidnightAngle = -270;

	if (TryGetMappedTimeToAngle(SunRiseSeconds, NoonSeconds, SunRiseAngle, SunNoonAngle, CurrentTimeSeconds, SunPitchCache))
	{
		return SunPitchCache;
	}

	if (TryGetMappedTimeToAngle(NoonSeconds, SunSetSeconds, SunNoonAngle, SunSetAngle, CurrentTimeSeconds, SunPitchCache))
	{
		return SunPitchCache;
	}

	if (TryGetMappedTimeToAngle(SunSetSeconds, MidnightSeconds, SunSetAngle, SunMidnightAngle, CurrentTimeSeconds, SunPitchCache))
	{
		return SunPitchCache;
	}

	if (TryGetMappedTimeToAngle(0, SunRiseSeconds, SunMidnightAngle, SunRiseAngle - 360.f, CurrentTimeSeconds, SunPitchCache))
	{
		return SunPitchCache;
	}

	check(false);
	return 0.f;
}

bool ATimeCycleSky::TryGetMappedTimeToAngle(int32 StartTime, int32 EndTime, float StartAngle, float EndAngle, int32 CurrentTime, float& MappedAngle) const
{
	if (CurrentTime >= StartTime && CurrentTime <= EndTime)
	{
		MappedAngle = FMath::GetMappedRangeValueClamped(FVector2D(StartTime, EndTime), FVector2D(StartAngle, EndAngle), CurrentTime);
		return true;
	}

	return false;
}

ATimeCycleManager* ATimeCycleSky::GetTimeCycleManager() const
{
	if (!IsValid(TimeCycleManager))
	{
		for (TActorIterator<ATimeCycleManager> It(GetWorld()); It; ++It)
		{
			TimeCycleManager = *It;
		}
	}

	return TimeCycleManager;
}

void ATimeCycleSky::BindVisualTimeManager()
{
	ATimeCycleManager* Manager = GetTimeCycleManager();
	if (BoundVisualTimeManager.Get() == Manager) { return; }
	if (ATimeCycleManager* PreviousManager = BoundVisualTimeManager.Get())
	{
		PreviousManager->OnVisualTimeChanged.RemoveDynamic(this, &ATimeCycleSky::HandleVisualTimeChanged);
	}
	BoundVisualTimeManager = Manager;
	if (Manager && GetWorld() && GetWorld()->IsGameWorld())
	{
		Manager->OnVisualTimeChanged.AddUniqueDynamic(this, &ATimeCycleSky::HandleVisualTimeChanged);
	}
}

void ATimeCycleSky::HandleVisualTimeChanged()
{
	UpdateVisualLighting();
}

APostProcessVolume* ATimeCycleSky::GetUnboundPostProcessVolume()
{
	if (APostProcessVolume* CachedVolume = CachedPPV.Get())
	{
		if (CachedVolume->bUnbound)
		{
			return CachedVolume;
		}
	}

	CachedPPV.Reset();
	UWorld* SkyWorld = GetWorld();
	if (!SkyWorld)
	{
		return nullptr;
	}

	for (TActorIterator<APostProcessVolume> It(SkyWorld); It; ++It)
	{
		if (It->bUnbound)
		{
			CachedPPV = *It;
			return *It;
		}
	}

	return nullptr;
}

void ATimeCycleSky::UpdateE6ReflectionCaptureVisibility(bool bShouldBeVisible)
{
	UReflectionCaptureComponent* CaptureComponent = CachedE6ReflectionCapture.Get();
	if (!CaptureComponent)
	{
		UWorld* SkyWorld = GetWorld();
		if (!SkyWorld)
		{
			return;
		}

		for (TActorIterator<ASphereReflectionCapture> It(SkyWorld); It; ++It)
		{
			UReflectionCaptureComponent* Candidate = It->GetCaptureComponent();
			if (Candidate
				&& Candidate->ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap
				&& Candidate->Cubemap
				&& Candidate->Cubemap->GetPathName() == E6ReflectionCubemapPath)
			{
				CachedE6ReflectionCapture = Candidate;
				CaptureComponent = Candidate;
				break;
			}
		}
	}

	if (CaptureComponent && CaptureComponent->IsVisible() != bShouldBeVisible)
	{
		CaptureComponent->SetVisibility(bShouldBeVisible, true);
	}
}

void ATimeCycleSky::UpdateOfficeFogTint(float DayAlpha)
{
	if (!bApplyOfficeFogTint)
	{
		RestoreOfficeFogTint();
		return;
	}
	UExponentialHeightFogComponent* FogComponent = CachedOfficeFog.Get();
	if (!FogComponent)
	{
		for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
		{
			FogComponent = It->GetComponent();
			if (FogComponent)
			{
				CachedOfficeFog = FogComponent;
				OfficeDayFogColor = FogComponent->FogInscatteringLuminance;
				break;
			}
		}
	}
	if (FogComponent)
	{
		const FLinearColor FogColor = FMath::Lerp(OfficeNightFogColor, OfficeDayFogColor, FMath::Clamp(DayAlpha, 0.f, 1.f));
		if (!FogComponent->FogInscatteringLuminance.Equals(FogColor, 0.001f))
		{
			FogComponent->SetFogInscatteringColor(FogColor);
		}
	}
}

void ATimeCycleSky::RestoreOfficeFogTint()
{
	if (UExponentialHeightFogComponent* FogComponent = CachedOfficeFog.Get())
	{
		FogComponent->SetFogInscatteringColor(OfficeDayFogColor);
	}
	CachedOfficeFog.Reset();
}

float ATimeCycleSky::GetEditorPreviewSunPitch() const
{
#if WITH_EDITOR
	// 에디터 프리뷰 시간으로 태양 각도 계산
	FTimeCycleCode PreviewTime;
	PreviewTime.Hours = FMath::FloorToInt(EditorPreviewHour);
	PreviewTime.Minutes = FMath::FloorToInt(FMath::Frac(EditorPreviewHour) * 60.0f);
	PreviewTime.Seconds = 0;
	return GetCycleSunPitch(PreviewTime);
#else
	return 0.f;
#endif
}

void ATimeCycleSky::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 시간은 분 단위로 변하므로 10Hz 갱신으로 충분 (매 프레임 → 약 1/6 비용)
	UpdateAccumulator += DeltaTime;
	if (UpdateAccumulator < UpdateInterval) return;
	UpdateAccumulator = 0.0f;
	BindVisualTimeManager();
	UpdateVisualLighting();
}

void ATimeCycleSky::UpdateVisualLighting()
{
	ATimeCycleManager* Manager = GetTimeCycleManager();

	float CurrentHour = 0.0f;
	FTimeCycleCode CurrentTime;

#if WITH_EDITORONLY_DATA
	// 에디터에서 게임 실행 중이 아니면 EditorPreviewHour 사용
	if (!GetWorld()->IsGameWorld())
	{
		CurrentHour = EditorPreviewHour;
		CurrentTime.Hours = FMath::FloorToInt(EditorPreviewHour);
		CurrentTime.Minutes = FMath::FloorToInt(FMath::Frac(EditorPreviewHour) * 60.0f);
		CurrentTime.Seconds = 0;
	}
	else
#endif
	{
		if (!Manager)
		{
			return;
		}
		CurrentTime = Manager->GetVisualTime();
		CurrentHour = CurrentTime.Hours + (CurrentTime.Minutes / 60.0f);
	}

	// 밤 경계는 시계(단일 소스)의 일출/일몰에서 파생. 매니저 없으면(에디터 프리뷰 등) 안전 폴백.
	float SunRiseHour = 7.0f;
	float SunSetHour = 19.0f;
	if (Manager)
	{
		const FTimeCycleCode RiseCode = Manager->GetSunRiseTime();
		const FTimeCycleCode SetCode = Manager->GetSunSetTime();
		SunRiseHour = RiseCode.Hours + RiseCode.Minutes / 60.0f;
		SunSetHour = SetCode.Hours + SetCode.Minutes / 60.0f;
	}

	USkyAtmosphereComponent* SkyAtmosphere = nullptr;
	if (GetWorld()->IsGameWorld())
	{
		for (TActorIterator<ASkyAtmosphere> It(GetWorld()); It; ++It)
		{
			USkyAtmosphereComponent* AtmosphereComponent = It->GetComponent();
			if (AtmosphereComponent && AtmosphereComponent->IsRegistered() && AtmosphereComponent->ShouldRender())
			{
				SkyAtmosphere = AtmosphereComponent;
				break;
			}
		}
	}

	TimeCycleSkyLighting::FParameters LightingParameters;
	LightingParameters.CurrentHour = CurrentHour;
	LightingParameters.SunRiseHour = SunRiseHour;
	LightingParameters.SunSetHour = SunSetHour;
	LightingParameters.DawnBlendHours = DawnBlendHours;
	LightingParameters.DuskBlendHours = DuskBlendHours;
	LightingParameters.DuskDirectionalIntensityScale = DuskDirectionalIntensityScale;
	LightingParameters.DuskMinimumDirectionalElevation = DuskMinimumDirectionalElevation;
	LightingParameters.PostSunsetTwilightHours = PostSunsetTwilightHours;
	LightingParameters.PreSunriseTwilightHours = PreSunriseTwilightHours;
	LightingParameters.TwilightWarmApex = TwilightWarmApex;
	LightingParameters.bSmoothTwilightRamp = bSmoothTwilightRamp;
	LightingParameters.bContinuousAtmosphereTwilight = SkyAtmosphere != nullptr;
	LightingParameters.SunMaxIntensity = SunMaxIntensity;
	const FTimeCycleSkyNightLook EffectiveNightLook = GetNightLook();
	LightingParameters.NightDirectionalIntensity = EffectiveNightLook.NightDirectionalIntensity;
	LightingParameters.DayDirectionalTemperature = 6500.0f;
	LightingParameters.TransitionDirectionalTemperature = 1800.0f;
	LightingParameters.NightDirectionalTemperature = EffectiveNightLook.NightDirectionalTemperature;
	LightingParameters.SolarPitch = GetCycleSunPitch(CurrentTime);
	LightingParameters.SunAzimuth = SunAzimuth;
	LightingParameters.SkyNightIntensity = EffectiveNightLook.SkyNightIntensity;
	LightingParameters.SkyDayIntensity = SkyDayIntensity;
	LightingParameters.NightAmbientColor = EffectiveNightLook.NightAmbientColor;
	LightingParameters.BaseSkyColor = BaseSkyColor;
	LightingParameters.DuskAmbientColor = DuskAmbientColor;
	LightingParameters.DuskAmbientStrength = DuskAmbientStrength;
	LightingParameters.NightExposureBias = EffectiveNightLook.NightExposureBias;
	LightingParameters.DayExposureBias = DayExposureBias;

	const TimeCycleSkyLighting::FState LightingState =
		TimeCycleSkyLighting::Calculate(LightingParameters);
	const bool bPitchChanged = !FMath::IsNearlyEqual(
		LightingState.DirectionalPitch,
		LastSunPitch,
		0.01f);
	const bool bAzimuthChanged = !FMath::IsNearlyEqual(
		LightingState.DirectionalYaw,
		LastSunAzimuth,
		0.01f);

	if (SunLight)
	{
		if (!FMath::IsNearlyEqual(LightingState.DirectionalIntensity, LastSunIntensity, 0.001f)
			|| (LightingState.DirectionalIntensity == 0.0f && LastSunIntensity != 0.0f))
		{
			SunLight->SetIntensity(LightingState.DirectionalIntensity);
			LastSunIntensity = LightingState.DirectionalIntensity;
		}
		if (!FMath::IsNearlyEqual(LightingState.DirectionalTemperature, LastSunTemperature, 0.5f))
		{
			SunLight->Temperature = LightingState.DirectionalTemperature;
			LastSunTemperature = LightingState.DirectionalTemperature;
		}
		if (bPitchChanged || bAzimuthChanged)
		{
			SunLight->SetRelativeRotation(FRotator(
				LightingState.DirectionalPitch,
				LightingState.DirectionalYaw,
				0.0f));
		}

		UWorld* SkyWorld = GetWorld();
		if (SkyWorld && SkyWorld->IsGameWorld())
		{
			const bool bWasAtmosphereSunLight = SunLight->IsUsedAsAtmosphereSunLight();
			const bool bShouldBeAtmosphereSunLight = SkyAtmosphere
				? LightingState.DayAlpha > 0.0f
				: TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(
					LightingParameters.SolarPitch,
					bWasAtmosphereSunLight);
			if (SkyAtmosphere && bShouldBeAtmosphereSunLight)
			{
				// 대기는 실제 공전을 따르되 황혼 최소 고도가 있으면 직접광의 지평선 감쇠도 보정한다.
				FVector AtmosphereDirection = -FRotator(
					LightingState.AtmospherePitch, LightingState.DirectionalYaw, 0.0f).Vector();
				if (const USceneComponent* SunParentComponent = SunLight->GetAttachParent())
				{
					AtmosphereDirection = SunParentComponent->GetComponentTransform().TransformVectorNoScale(AtmosphereDirection);
				}
				const int32 AtmosphereIndex = SunLight->AtmosphereSunLightIndex;
				if (!SkyAtmosphere->IsAtmosphereLightDirectionOverriden(AtmosphereIndex)
					|| !SkyAtmosphere->GetOverridenAtmosphereLightDirection(AtmosphereIndex).Equals(AtmosphereDirection, 0.0001))
				{
					SkyAtmosphere->OverrideAtmosphereLightDirection(AtmosphereIndex, AtmosphereDirection);
				}
			}
			if (bShouldBeAtmosphereSunLight != bWasAtmosphereSunLight)
			{
				if (SkyAtmosphere)
				{
					SunLight->SetIntensity(LightingState.DirectionalIntensity);
					LastSunIntensity = LightingState.DirectionalIntensity;
				}
				SunLight->SetAtmosphereSunLight(bShouldBeAtmosphereSunLight);
			}
		}
	}

	// SkyLight: 정적 큐브맵이라 런타임 캡처 0 — 강도만 낮/밤 보간(변경 시에만 SetIntensity → RHI dirty 회피).
	if (SkyLight && !FMath::IsNearlyEqual(LightingState.SkyIntensity, LastSkyIntensity, 0.001f))
	{
		SkyLight->SetIntensity(LightingState.SkyIntensity);
		LastSkyIntensity = LightingState.SkyIntensity;
	}

	// SkyLight 색: 야간 쿨 환경광→낮 중립 환경광, 황혼/여명은 웜앰버 범프를 추가한 최종 색을 캐시.
	if (SkyLight && !LastSkyColor.Equals(LightingState.SkyColor, 0.002f))
	{
		SkyLight->SetLightColor(LightingState.SkyColor);
		LastSkyColor = LightingState.SkyColor;
	}

#if WITH_EDITOR
	UpdateEditorPreviewExposure(LightingState.ExposureBias);
#endif

	// 노출 bias: SkyLight와 같은 낮밤 알파로 보간해 전역 PPV에 주입 — 게임 월드 전용(에디터에서 레벨 dirty 방지).
	// PPV의 저장값(bias override OFF)은 건드리지 않고 런타임에만 override ON.
	UWorld* SkyWorld = GetWorld();
	if (SkyWorld && SkyWorld->IsGameWorld())
	{
		APostProcessVolume* PreviousPPV = CachedPPV.Get();
		UpdateOfficeFogTint(LightingState.DayAlpha);
		APostProcessVolume* PPV = GetUnboundPostProcessVolume();
		if (PPV != PreviousPPV)
		{
			LastExposureBias = FLT_MAX;
			LastWarmWeight = FLT_MAX;
			bVisualProfileNeedsRecovery = true;
		}

		if (PPV && !FMath::IsNearlyEqual(LightingState.ExposureBias, LastExposureBias, 0.001f))
		{
			PPV->Settings.bOverride_AutoExposureBias = true;
			PPV->Settings.AutoExposureBias = LightingState.ExposureBias;
			LastExposureBias = LightingState.ExposureBias;
		}

		// 풀스크린 노을 틴트(옵션) — WarmWeight=0이면 White라 낮/밤엔 무효(자동 복귀).
		if (PPV && bDuskSceneTint
			&& !FMath::IsNearlyEqual(LightingState.WarmWeight, LastWarmWeight, 0.001f))
		{
			PPV->Settings.bOverride_SceneColorTint = true;
			PPV->Settings.SceneColorTint = FMath::Lerp(
				FLinearColor::White,
				DuskAmbientColor,
				LightingState.WarmWeight * DuskSceneTintStrength);
			LastWarmWeight = LightingState.WarmWeight;
		}

		if (bApplyMainMapVisualProfile && SkyWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1)
		{
			const FTimeCycleSkyVisualProfile VisualProfile =
				TimeCycleSkyVisualProfile::Calculate(
					CurrentHour,
					SunRiseHour,
					SunSetHour,
					DawnBlendHours,
					DuskBlendHours);
			UpdateE6ReflectionCaptureVisibility(VisualProfile.Phase != EMainMapVisualPhase::Night);

			UTexture* SelectedLUT = VisualProfile.LutRole == EMainMapLutRole::DCL2
				? DCL2ColorGradingLUT.Get()
				: L2ColorGradingLUT.Get();

			if (!PPV || !SelectedLUT)
			{
				bVisualProfileNeedsRecovery = true;
			}
			else
			{
				const int32 VisualPhase = static_cast<int32>(VisualProfile.Phase);
				const bool bPhaseChanged = LastAppliedVisualPhase != VisualPhase;
				const bool bLutChanged = LastAppliedColorGradingLUT.Get() != SelectedLUT;
				const bool bDuskHourChanged = VisualProfile.Phase == EMainMapVisualPhase::Dusk
					&& !FMath::IsNearlyEqual(CurrentHour, LastAppliedVisualProfileHour, KINDA_SMALL_NUMBER);
				if (bPhaseChanged || bLutChanged || bDuskHourChanged || bVisualProfileNeedsRecovery)
				{
					ApplyMobileVisualProfile(PPV->Settings, VisualProfile, SelectedLUT);
					LastAppliedVisualPhase = VisualPhase;
					LastAppliedColorGradingLUT = SelectedLUT;
					LastAppliedVisualProfileHour = CurrentHour;
					bVisualProfileNeedsRecovery = false;
				}
			}
		}
	}

	if (bPitchChanged || bAzimuthChanged)
	{
		LastSunPitch = LightingState.DirectionalPitch;
		LastSunAzimuth = LightingState.DirectionalYaw;
	}
}

#if WITH_EDITOR
void ATimeCycleSky::UpdateEditorPreviewExposure(float ExposureBias)
{
#if WITH_EDITORONLY_DATA
	UWorld* SkyWorld = GetWorld();
	const FString MapPackage = SkyWorld ? SkyWorld->GetPackage()->GetName() : FString();
	const bool bShouldEnable = SkyWorld
		&& SkyWorld->WorldType == EWorldType::Editor
		&& !SkyWorld->IsGameWorld()
		&& SkyWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1
		&& (MapPackage == TEXT("/Game/CompanyGrowth/Level/MainMap_City")
			|| MapPackage == TEXT("/Game/CompanyGrowth/Level/MainMap_Manhattan")
			|| MapPackage == TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"));
	if (!EditorPreviewPostProcess)
	{
		return;
	}

	const bool bEnabledChanged = EditorPreviewPostProcess->bEnabled != bShouldEnable;
	EditorPreviewPostProcess->bEnabled = bShouldEnable;
	if (!bShouldEnable)
	{
		if (bEnabledChanged)
		{
			EditorPreviewPostProcess->MarkRenderStateDirty();
		}
		return;
	}

	const bool bBiasChanged = !FMath::IsNearlyEqual(
		ExposureBias,
		LastEditorPreviewExposureBias,
		0.001f);
	EditorPreviewPostProcess->bUnbound = true;
	EditorPreviewPostProcess->Settings.bOverride_AutoExposureMethod = true;
	EditorPreviewPostProcess->Settings.AutoExposureMethod = AEM_Manual;
	EditorPreviewPostProcess->Settings.bOverride_AutoExposureBias = true;
	EditorPreviewPostProcess->Settings.AutoExposureBias = ExposureBias;
	LastEditorPreviewExposureBias = ExposureBias;
	if (bEnabledChanged || bBiasChanged)
	{
		EditorPreviewPostProcess->MarkRenderStateDirty();
	}
#endif
}

void ATimeCycleSky::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property == nullptr)
		return;

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// EditorPreviewHour 변경 시 블루프린트 이벤트 호출을 위해 마킹
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ATimeCycleSky, EditorPreviewHour))
	{
		// BP_TimeCycleSky의 Construction Script 재실행
		RerunConstructionScripts();
	}
}
#endif
