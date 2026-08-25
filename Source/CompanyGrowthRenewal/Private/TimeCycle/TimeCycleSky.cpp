// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#include "TimeCycle/TimeCycleSky.h"
#include "EngineUtils.h"
#include "TimeCycle/TimeCycleManager.h"
#include "TimeCycle/TimeCycleSkyLighting.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/PostProcessVolume.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
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
}

ATimeCycleSky::ATimeCycleSky()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

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
	FTimeCycleSkyNightLook Look;
	Look.SkyNightIntensity = SkyNightIntensity;
	Look.NightDirectionalIntensity = NightDirectionalIntensity;
	Look.NightDirectionalTemperature = NightDirectionalTemperature;
	Look.NightAmbientColor = NightAmbientColor;
	Look.NightExposureBias = NightExposureBias;
	return Look;
}

bool ATimeCycleSky::ApplyTransientNightLook(const FTimeCycleSkyNightLook& NewLook)
{
#if WITH_EDITOR
	UWorld* SkyWorld = GetWorld();
	if (!HasAuthority() || !SkyWorld || !SkyWorld->IsGameWorld() || !IsValidNightLook(NewLook))
	{
		return false;
	}

	SkyNightIntensity = NewLook.SkyNightIntensity;
	NightDirectionalIntensity = NewLook.NightDirectionalIntensity;
	NightDirectionalTemperature = NewLook.NightDirectionalTemperature;
	NightAmbientColor = NewLook.NightAmbientColor;
	NightExposureBias = NewLook.NightExposureBias;
	return true;
#else
	(void)NewLook;
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
		CurrentTime = Manager->GetCurrentTime();
		CurrentHour = CurrentTime.Hours + (CurrentTime.Minutes / 60.0f);
	}

	// 밤 경계는 시계(단일 소스)의 일출/일몰에서 파생. 매니저 없으면(에디터 프리뷰 등) 안전 폴백.
	float SunRiseHour = 7.0f;
	float SunSetHour = 20.0f;
	if (Manager)
	{
		const FTimeCycleCode RiseCode = Manager->GetSunRiseTime();
		const FTimeCycleCode SetCode = Manager->GetSunSetTime();
		SunRiseHour = RiseCode.Hours + RiseCode.Minutes / 60.0f;
		SunSetHour = SetCode.Hours + SetCode.Minutes / 60.0f;
	}

	TimeCycleSkyLighting::FParameters LightingParameters;
	LightingParameters.CurrentHour = CurrentHour;
	LightingParameters.SunRiseHour = SunRiseHour;
	LightingParameters.SunSetHour = SunSetHour;
	LightingParameters.DawnBlendHours = DawnBlendHours;
	LightingParameters.DuskBlendHours = DuskBlendHours;
	LightingParameters.SunMaxIntensity = SunMaxIntensity;
	LightingParameters.NightDirectionalIntensity = NightDirectionalIntensity;
	LightingParameters.DayDirectionalTemperature = 6500.0f;
	LightingParameters.TransitionDirectionalTemperature = 1800.0f;
	LightingParameters.NightDirectionalTemperature = NightDirectionalTemperature;
	LightingParameters.SolarPitch = GetCycleSunPitch(CurrentTime);
	LightingParameters.SkyNightIntensity = SkyNightIntensity;
	LightingParameters.SkyDayIntensity = SkyDayIntensity;
	LightingParameters.NightAmbientColor = NightAmbientColor;
	LightingParameters.BaseSkyColor = BaseSkyColor;
	LightingParameters.DuskAmbientColor = DuskAmbientColor;
	LightingParameters.DuskAmbientStrength = DuskAmbientStrength;
	LightingParameters.NightExposureBias = NightExposureBias;
	LightingParameters.DayExposureBias = DayExposureBias;

	const TimeCycleSkyLighting::FState LightingState =
		TimeCycleSkyLighting::Calculate(LightingParameters);
	const bool bPitchChanged = !FMath::IsNearlyEqual(
		LightingState.DirectionalPitch,
		LastSunPitch,
		0.01f);

	if (SunLight)
	{
		if (!FMath::IsNearlyEqual(LightingState.DirectionalIntensity, LastSunIntensity, 0.001f))
		{
			SunLight->SetIntensity(LightingState.DirectionalIntensity);
			LastSunIntensity = LightingState.DirectionalIntensity;
		}
		if (!FMath::IsNearlyEqual(LightingState.DirectionalTemperature, LastSunTemperature, 0.5f))
		{
			SunLight->Temperature = LightingState.DirectionalTemperature;
			LastSunTemperature = LightingState.DirectionalTemperature;
		}
		if (bPitchChanged)
		{
			SunLight->SetRelativeRotation(FRotator(LightingState.DirectionalPitch, 0.0f, 0.0f));
		}

		UWorld* SkyWorld = GetWorld();
		if (SkyWorld && SkyWorld->IsGameWorld())
		{
			const bool bWasAtmosphereSunLight = SunLight->IsUsedAsAtmosphereSunLight();
			const bool bShouldBeAtmosphereSunLight =
				TimeCycleSkyLighting::ShouldEnableAtmosphereSunLight(
					LightingParameters.SolarPitch,
					bWasAtmosphereSunLight);
			if (bShouldBeAtmosphereSunLight != bWasAtmosphereSunLight)
			{
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
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (!FMath::IsNearlyEqual(LightingState.ExposureBias, LastExposureBias, 0.001f))
		{
			if (!CachedPPV.IsValid() && !bTriedFindPPV)
			{
				bTriedFindPPV = true;
				// 전역(unbound) PPV에만 주입 — bounded 로컬 PPV에 쓰면 그 볼륨 안에서만 먹혀 낮밤 노출이 무효화됨
				for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
				{
					if (It->bUnbound)
					{
						CachedPPV = *It;
						break;
					}
				}
			}
			if (APostProcessVolume* PPV = CachedPPV.Get())
			{
				PPV->Settings.bOverride_AutoExposureBias = true;
				PPV->Settings.AutoExposureBias = LightingState.ExposureBias;
				LastExposureBias = LightingState.ExposureBias;

				// 풀스크린 노을 틴트(옵션) — WarmWeight=0이면 White라 낮/밤엔 무효(자동 복귀).
				if (bDuskSceneTint)
				{
					PPV->Settings.bOverride_SceneColorTint = true;
					PPV->Settings.SceneColorTint = FMath::Lerp(
						FLinearColor::White,
						DuskAmbientColor,
						LightingState.WarmWeight * DuskSceneTintStrength);
				}
			}
		}
	}

	if (bPitchChanged) LastSunPitch = LightingState.DirectionalPitch;
}

#if WITH_EDITOR
void ATimeCycleSky::UpdateEditorPreviewExposure(float ExposureBias)
{
#if WITH_EDITORONLY_DATA
	UWorld* SkyWorld = GetWorld();
	const bool bShouldEnable = SkyWorld
		&& SkyWorld->WorldType == EWorldType::Editor
		&& !SkyWorld->IsGameWorld()
		&& SkyWorld->GetFeatureLevel() == ERHIFeatureLevel::ES3_1
		&& SkyWorld->GetPackage()->GetName()
			== TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity");
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
