// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DirectionalLightComponent.h"
#include "TimeCycleSky.generated.h"

struct FTimeCycleCode;
class APostProcessVolume;
class ATimeCycleManager;
class UPostProcessComponent;
class UExponentialHeightFogComponent;
class UReflectionCaptureComponent;
class USkyLightComponent;
class UTexture;
class UTextureCube;

USTRUCT(BlueprintType)
struct COMPANYGROWTHRENEWAL_API FTimeCycleSkyDayLook
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category=TimeCycleSky)
	float SunMaxIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category=TimeCycleSky)
	float SkyDayIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category=TimeCycleSky)
	float DayExposureBias = 0.0f;
};

USTRUCT(BlueprintType)
struct COMPANYGROWTHRENEWAL_API FTimeCycleSkyNightLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleSky)
	float SkyNightIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleSky)
	float NightDirectionalIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleSky)
	float NightDirectionalTemperature = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleSky)
	FLinearColor NightAmbientColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimeCycleSky)
	float NightExposureBias = 0.0f;
};

/*
 * 하늘과 낮/밤 방향광 표현을 위한 Actor
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ATimeCycleSky : public AActor
{
	GENERATED_BODY()

public:
	ATimeCycleSky();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaTime) override;

	// 에디터에서도 Tick 활성화
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UFUNCTION(BlueprintPure, Category=TimeCycleSky)
	FTimeCycleSkyDayLook GetDayLook() const;

	UFUNCTION(BlueprintPure, Category=TimeCycleSky)
	FTimeCycleSkyNightLook GetNightLook() const;

	// 노을색이 가장 짙어지는 시각(0~24). 황혼 폭·박명 꼬리·apex 노브에서 파생하므로 맵마다 다르다
	// (MainMap 18:00 / WP_Office 19:00) — 치트나 캡처가 상수로 박으면 한쪽은 밤을 찍는다.
	UFUNCTION(BlueprintPure, Category=TimeCycleSky)
	float GetTwilightApexHour(bool bDawn = false) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycleSky, meta=(DevelopmentOnly))
	bool ApplyTransientNightLook(const FTimeCycleSkyNightLook& NewLook);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycleSky, meta=(DevelopmentOnly))
	bool ApplyTransientSunAzimuth(float NewAzimuth);

	// 게임 시계·이벤트·노출·안개 위치/밀도·갱신 캐시를 유지하는 동기식 낮 조명/안개색 범위.
	bool WithDaylightForCapture(TFunctionRef<bool()> CaptureOperation);

protected:
	// 낮 태양과 야간 fill을 연속 보간하는 단일 DirectionalLight
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	UDirectionalLightComponent* SunLight;

	// SkyLight — Sun과 동일하게 C++ 소유. 정적(Specified) 큐브맵 + 강도만 낮/밤 보간(런타임 캡처 0 = 모바일 최적). ※ BP의 기존 SkyLight 컴포넌트는 삭제할 것(이름 충돌).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	USkyLightComponent* SkyLight;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// 시간에 따른 태양 피치 각도 반환
	UFUNCTION(BlueprintPure, Category=Sky)
	float GetCycleSunPitch(const FTimeCycleCode& Time) const;

	// 에디터에서 태양 각도 직접 미리보기
	UFUNCTION(BlueprintCallable, Category=Sky)
	float GetEditorPreviewSunPitch() const;

#if WITH_EDITORONLY_DATA
	// 에디터 프리뷰 시간 (0~24시)
	UPROPERTY(EditAnywhere, Category="Sky|Editor Preview", meta=(ClampMin="0", ClampMax="24", UIMin="0", UIMax="24"))
	float EditorPreviewHour = 12.0f;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category="Sky|Editor Preview")
	TObjectPtr<UPostProcessComponent> EditorPreviewPostProcess;
#endif

	// === 밤/낮 룩 튜닝 (에디터에서 조절, 리빌드 불필요) ===
	// 태양 최대 강도(낮)
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0"))
	float SunMaxIntensity = 2.0f;

	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="-180.0", ClampMax="180.0", UIMin="-180.0", UIMax="180.0"))
	float SunAzimuth = 35.0f;

	// 깊은 밤의 건물 실루엣을 채우는 단일 방향광 최소 강도. 태양과 별도 라이트를 스폰하지 않는다.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0"))
	float NightDirectionalIntensity = 0.08f;

	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="1000.0"))
	float NightDirectionalTemperature = 9000.0f;

	// 여명 블렌드 시간(시) — 일출 전 이만큼 동안 밝아짐
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.1"))
	float DawnBlendHours = 2.0f;

	// 황혼 블렌드 시간(시) — 일몰 전 이만큼 동안 어두워짐
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.1"))
	float DuskBlendHours = 2.0f;

	// 황혼의 태양 직접광만 노을 가중치에 맞춰 증감한다.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0"))
	float DuskDirectionalIntensityScale = 1.0f;

	// 황혼의 지표면 직접광이 수평선에 누워 사라지지 않도록 최소 고도를 정한다.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0", ClampMax="90.0"))
	float DuskMinimumDirectionalElevation = 0.0f;

	// 일몰 후 박명(시) — 해가 지평선을 넘은 뒤에도 이만큼 노을빛이 남는다. 0이면 일몰 순간 밤.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0"))
	float PostSunsetTwilightHours = 0.0f;

	// 일출 전 선행 박명(시) — 여명이 시작되기 전 하늘이 먼저 트는 구간.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0"))
	float PreSunriseTwilightHours = 0.0f;

	// 전환 구간에서 노을색이 가장 짙어지는 위치 0~1. 일몰 시각 위치 = DuskBlend/(DuskBlend+PostSunset).
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.01", ClampMax="0.99"))
	float TwilightWarmApex = 0.5f;

	// 밝기 램프 양 끝 기울기를 0으로 — 전환이 시작·끝나는 순간이 눈에 안 띈다.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting")
	bool bSmoothTwilightRamp = false;

	// 낮 환경광(SkyLight) 강도 — 햇빛 안 받는 건물 면을 채움. 1.5로 부족해 상향. 밝은 낮 캡처라 값이 선형으로 먹음. 라이브 조절 가능.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0"))
	float SkyDayIntensity = 3.0f;

	// 밤 환경광 강도 — 깊은 밤엔 낮/황혼의 밝은 캡처를 얼려두고 이 값으로만 줄임(검은 밤하늘 캡처 안 씀) → 중립색 약광 가독성. 라이브 조절 가능.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0"))
	float SkyNightIntensity = 0.20f;

	// PC와 비모바일 프리뷰는 B2 적용 전의 따뜻한 직렬화 룩을 유지한다.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(HideAlphaChannel))
	FLinearColor NightAmbientColor = FLinearColor(0.5149176654f, 0.4452011945f, 0.3762621230f);

	// 황혼/여명 환경광 색 — 태양이 낮게 깔린 골든아워에 SkyLight(환경광)를 이 색으로 물들여 태양을 안 받는 면까지 노을색으로. 낮/밤엔 자동 중립 복귀. 라이브 조절 가능.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(HideAlphaChannel))
	FLinearColor DuskAmbientColor = FLinearColor(1.0f, 0.42f, 0.16f);

	// 황혼 환경광 물듦 세기 0~1 (0=효과 없음). 라이브 조절 가능.
	UPROPERTY(EditAnywhere, Category="Sky|Lighting", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DuskAmbientStrength = 0.7f;

	// 노출 bias 낮/밤 — 오토 노출은 모바일서 자기진동(가로등 halo 펄스 진범)이라 Manual 고정(디바이스 프로파일이 강제),
	// 밝기는 여기서 시간 구동. 여명/황혼은 태양 강도 비율로 자동 보간. 라이브 조절 가능.
	UPROPERTY(EditAnywhere, Category="Sky|Exposure")
	float DayExposureBias = -0.15f;

	UPROPERTY(EditAnywhere, Category="Sky|Exposure")
	float NightExposureBias = 0.20f;

	// Office는 자체 PP를 유지하면서 같은 낮밤 조명 계산을 사용한다.
	UPROPERTY(EditAnywhere, Category="Sky|Mobile Visual Profile")
	bool bApplyMainMapVisualProfile = true;

	// OfficeMap에 배치된 안개 높이·밀도를 유지하고 시간에 따른 색만 적용한다.
	UPROPERTY(EditAnywhere, Category="Sky|Office Fog")
	bool bApplyOfficeFogTint = false;

	UPROPERTY(EditAnywhere, Category="Sky|Office Fog", meta=(EditCondition="bApplyOfficeFogTint", HideAlphaChannel))
	FLinearColor OfficeNightFogColor = FLinearColor(0.012f, 0.020f, 0.038f);

	// ES3_1 게임월드 전용 B2 야간 룩. 공용 PC 직렬화 값과 분리해 플랫폼 간 색 누출을 막는다.
	UPROPERTY(EditAnywhere, Category="Sky|Mobile Visual Profile")
	FTimeCycleSkyNightLook MobileNightLook;

	// Production tool이 배치 액터에 정확한 LUT를 할당하며, 직렬화 hard reference가 쿠킹 의존성을 만든다.
	UPROPERTY(EditAnywhere, Category="Sky|Mobile Visual Profile")
	TObjectPtr<UTexture> L2ColorGradingLUT;

	UPROPERTY(EditAnywhere, Category="Sky|Mobile Visual Profile")
	TObjectPtr<UTexture> DCL2ColorGradingLUT;

	// 황혼/여명에 전역 PPV까지 씬 틴트로 노을색 주입(풀스크린). 끄면 SkyLight 환경광 색만으로. 라이브 조절 가능.
	UPROPERTY(EditAnywhere, Category="Sky|Exposure")
	bool bDuskSceneTint = true;

	// PPV 씬 틴트 세기 0~1 (환경광보다 은은하게). 라이브 조절 가능.
	UPROPERTY(EditAnywhere, Category="Sky|Exposure", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bDuskSceneTint"))
	float DuskSceneTintStrength = 0.35f;

private:
	// 시간을 각도로 매핑
	bool TryGetMappedTimeToAngle(int32 StartTime, int32 EndTime, float StartAngle, float EndAngle, int32 CurrentTime, float& MappedAngle) const;

	// TimeCycleManager 캐시
	ATimeCycleManager* GetTimeCycleManager() const;
	void BindVisualTimeManager();
	void UpdateVisualLighting();
	UFUNCTION()
	void HandleVisualTimeChanged();
	APostProcessVolume* GetUnboundPostProcessVolume();
	void UpdateE6ReflectionCaptureVisibility(bool bShouldBeVisible);
	void UpdateOfficeFogTint(float DayAlpha);
	void RestoreOfficeFogTint();

#if WITH_EDITOR
	void UpdateEditorPreviewExposure(float ExposureBias);
#endif

	UPROPERTY()
	mutable TObjectPtr<ATimeCycleManager> TimeCycleManager;
	TWeakObjectPtr<ATimeCycleManager> BoundVisualTimeManager;

	// 프레임당 중복 계산 방지용 캐시
	mutable float SunPitchCache;
	mutable int32 SunPitchCacheTimeStamp;

	// 갱신 스로틀 + 변경 감지 (RHI dirty 회피)
	float UpdateAccumulator = 0.0f;
	static constexpr float UpdateInterval = 0.1f;
	float LastSunIntensity = -1.0f;
	float LastSunTemperature = -1.0f;
	float LastSunPitch = FLT_MAX;
	float LastSunAzimuth = FLT_MAX;
	float LastSkyIntensity = -1.0f;
	float LastExposureBias = FLT_MAX;
	// 노을 틴트는 노출과 따로 캐시한다 — 완만한 램프에선 노출이 멈춘 구간에도 노을색이 움직인다.
	float LastWarmWeight = FLT_MAX;
	FLinearColor LastSkyColor = FLinearColor(-1.0f, -1.0f, -1.0f, -1.0f);

#if WITH_EDITORONLY_DATA
	float LastEditorPreviewExposureBias = FLT_MAX;
#endif

	// 낮 중립 환경광 색. 황혼 웜틴트 lerp의 시작점.
	FLinearColor BaseSkyColor = FLinearColor(FColor(213, 213, 213));

	// MainMap 전역 PPV(unbound) 캐시. 유효한 동안은 월드를 다시 탐색하지 않는다.
	TWeakObjectPtr<APostProcessVolume> CachedPPV;
	TWeakObjectPtr<UReflectionCaptureComponent> CachedE6ReflectionCapture;
	TWeakObjectPtr<UExponentialHeightFogComponent> CachedOfficeFog;
	FLinearColor OfficeDayFogColor = FLinearColor::White;
	TWeakObjectPtr<UTexture> LastAppliedColorGradingLUT;
	int32 LastAppliedVisualPhase = INDEX_NONE;
	float LastAppliedVisualProfileHour = FLT_MAX;
	bool bVisualProfileNeedsRecovery = true;

	// 정적 환경광 큐브맵(엔진 기본). 멤버 하드참조라 모바일 쿠킹 보장 + BeginPlay 재지정용.
	UPROPERTY()
	TObjectPtr<UTextureCube> AmbientCubemapAsset;

	UPROPERTY()
	TObjectPtr<class UMaterialParameterCollection> CaptureWindowLightMPC;
};
