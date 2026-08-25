// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/BuildingLandCameraShake.h"

void UBuildingLandShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(ShakeDuration);
}

void UBuildingLandShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
}

void UBuildingLandShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float T = FMath::Clamp(Elapsed / ShakeDuration, 0.f, 1.f);

	// 감쇠 사인 — 시작에 강하고 빠르게 잦아드는 thud 응답. 스케일 적용은 베이스(Default 플래그)가 처리
	const float Envelope = (1.f - T) * (1.f - T);
	const float Wave = FMath::Sin(Elapsed * 2.f * PI * Frequency);
	OutResult.Rotation = FRotator(PitchAmplitude * Envelope * Wave, 0.f, 0.f);
}

bool UBuildingLandShakePattern::IsFinishedImpl() const
{
	return Elapsed >= ShakeDuration;
}

UBuildingLandCameraShake::UBuildingLandCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 연속 트리거 시 중첩 대신 재시작
	bSingleInstance = true;
	SetRootShakePattern(ObjectInitializer.CreateDefaultSubobject<UBuildingLandShakePattern>(this, TEXT("LandShakePattern")));
}

// ===== 가벼운 타격 =====

void ULightImpactShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(ShakeDuration);
}

void ULightImpactShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
}

void ULightImpactShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float T = FMath::Clamp(Elapsed / ShakeDuration, 0.f, 1.f);

	const float Envelope = (1.f - T) * (1.f - T);
	const float Wave = FMath::Sin(Elapsed * 2.f * PI * Frequency);
	OutResult.Rotation = FRotator(PitchAmplitude * Envelope * Wave, 0.f, 0.f);
}

bool ULightImpactShakePattern::IsFinishedImpl() const
{
	return Elapsed >= ShakeDuration;
}

ULightImpactCameraShake::ULightImpactCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(ObjectInitializer.CreateDefaultSubobject<ULightImpactShakePattern>(this, TEXT("LightImpactPattern")));
}
