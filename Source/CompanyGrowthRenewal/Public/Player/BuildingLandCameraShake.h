// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "BuildingLandCameraShake.generated.h"

// 건물 배치 착지/층 추가 thud 용 감쇠 사인 패턴.
// 엔진 기본 패턴(WaveOscillator 등)은 GameplayCameras 플러그인 소속이라 모듈 의존성 없이 자체 구현.
// 회전(Pitch) 위주 — 위치 셰이크와 달리 줌아웃 카메라에서도 체감 일정
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingLandShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float Elapsed = 0.f;

	static constexpr float ShakeDuration = 0.3f;
	static constexpr float PitchAmplitude = 0.35f;   // 도(deg)
	static constexpr float Frequency = 11.f;         // Hz
};

// PC->ClientStartCameraShake(UBuildingLandCameraShake::StaticClass(), Scale) 로 사용.
// 배치 착지 = 1.0, 층 추가 = 0.7 권장
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingLandCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UBuildingLandCameraShake(const FObjectInitializer& ObjectInitializer);
};

// 가벼운 타격용 — 무거운 착지(위)와 파형 분리. UI 계열(개발 크리티컬/막판 스퍼트)에 사용.
// 낮은 진폭·높은 주파수·짧은 지속 = "톡" 치는 응답. 월드 착지/철거는 현행 유지(사운드 동기 회귀 방지).
UCLASS()
class COMPANYGROWTHRENEWAL_API ULightImpactShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float Elapsed = 0.f;

	static constexpr float ShakeDuration = 0.12f;
	static constexpr float PitchAmplitude = 0.16f;   // 도(deg) — 착지(0.35)보다 가볍게
	static constexpr float Frequency = 26.f;         // Hz — 더 빠른 진동
};

UCLASS()
class COMPANYGROWTHRENEWAL_API ULightImpactCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	ULightImpactCameraShake(const FObjectInitializer& ObjectInitializer);
};
