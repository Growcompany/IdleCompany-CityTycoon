// Fill out your copyright notice in the Description page of Project Settings.

#include "Utils/FWidgetAnimationUtils.h"

//-----------------------------------------------------------------------------
// FWidgetAnimationUtils - Easing 함수들
//-----------------------------------------------------------------------------

float FWidgetAnimationUtils::EaseOutQuad(float T)
{
	// T * (2 - T) : 점점 느려지는 감속 곡선
	return T * (2.0f - T);
}

float FWidgetAnimationUtils::EaseOutBack(float T)
{
	// 오버슈트 상수 (1.70158은 약 10% 오버슈트를 만듦)
	const float C1 = 1.70158f;
	const float C3 = C1 + 1.0f;  // 2.70158

	// 공식: 1 + (T-1)^3 * C3 + (T-1)^2 * C1
	const float TMinusOne = T - 1.0f;
	return 1.0f + TMinusOne * TMinusOne * TMinusOne * C3 + TMinusOne * TMinusOne * C1;
}

float FWidgetAnimationUtils::EaseOutElastic(float T)
{
	// 특수 케이스 처리
	if (T <= 0.0f)
	{
		return 0.0f;
	}
	if (T >= 1.0f)
	{
		return 1.0f;
	}

	// 탄성 공식: 2^(-10*T) * sin((T*10 - 0.75) * (2*PI/3)) + 1
	const float C4 = (2.0f * PI) / 3.0f;
	return FMath::Pow(2.0f, -10.0f * T) * FMath::Sin((T * 10.0f - 0.75f) * C4) + 1.0f;
}

//-----------------------------------------------------------------------------
// FNumberCountUpAnimation - 숫자 카운트업 애니메이션
//-----------------------------------------------------------------------------

void FNumberCountUpAnimation::Start(float From, float To, float InDuration)
{
	FromValue = From;
	ToValue = To;
	Duration = FMath::Max(InDuration, 0.01f);  // 최소 지속 시간 보장
	ElapsedTime = 0.f;
	bIsPlaying = true;
}

float FNumberCountUpAnimation::Tick(float DeltaTime)
{
	if (!bIsPlaying)
	{
		return ToValue;
	}

	ElapsedTime += DeltaTime;

	// 완료 체크
	if (ElapsedTime >= Duration)
	{
		bIsPlaying = false;
		ElapsedTime = Duration;
		return ToValue;
	}

	// 진행률 계산 (0 ~ 1)
	const float Alpha = ElapsedTime / Duration;

	// EaseOutQuad 적용으로 부드러운 감속 효과
	const float EasedAlpha = FWidgetAnimationUtils::EaseOutQuad(Alpha);

	// 선형 보간으로 현재 값 계산
	return FMath::Lerp(FromValue, ToValue, EasedAlpha);
}

void FNumberCountUpAnimation::Finish()
{
	bIsPlaying = false;
	ElapsedTime = Duration;
}

float FNumberCountUpAnimation::GetCurrentValue() const
{
	if (!bIsPlaying || Duration <= 0.f)
	{
		return ToValue;
	}

	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.f, 1.f);
	const float EasedAlpha = FWidgetAnimationUtils::EaseOutQuad(Alpha);
	return FMath::Lerp(FromValue, ToValue, EasedAlpha);
}

//-----------------------------------------------------------------------------
// FScalePunchAnimation - 스케일 펀치 애니메이션
//-----------------------------------------------------------------------------

void FScalePunchAnimation::Start(float InPunchScale, float InDuration)
{
	PunchScale = InPunchScale;
	Duration = FMath::Max(InDuration, 0.01f);  // 최소 지속 시간 보장
	ElapsedTime = 0.f;
	bIsPlaying = true;
}

float FScalePunchAnimation::Tick(float DeltaTime)
{
	if (!bIsPlaying)
	{
		return 1.0f;
	}

	ElapsedTime += DeltaTime;

	// 완료 체크
	if (ElapsedTime >= Duration)
	{
		bIsPlaying = false;
		ElapsedTime = Duration;
		return 1.0f;
	}

	// 진행률 계산 (0 ~ 1)
	const float Alpha = ElapsedTime / Duration;

	// 절반 지점까지: 1.0 -> PunchScale
	// 나머지 절반: PunchScale -> 1.0
	float CurrentScale;
	if (Alpha < 0.5f)
	{
		// 전반부: 빠르게 확대 (0~0.5 -> 0~1로 정규화)
		const float HalfAlpha = Alpha * 2.0f;
		const float EasedAlpha = FWidgetAnimationUtils::EaseOutQuad(HalfAlpha);
		CurrentScale = FMath::Lerp(1.0f, PunchScale, EasedAlpha);
	}
	else
	{
		// 후반부: EaseOutBack으로 오버슈트 효과와 함께 축소 (0.5~1 -> 0~1로 정규화)
		const float HalfAlpha = (Alpha - 0.5f) * 2.0f;
		const float EasedAlpha = FWidgetAnimationUtils::EaseOutBack(HalfAlpha);
		CurrentScale = FMath::Lerp(PunchScale, 1.0f, EasedAlpha);
	}

	return CurrentScale;
}

float FScalePunchAnimation::GetCurrentScale() const
{
	if (!bIsPlaying || Duration <= 0.f)
	{
		return 1.0f;
	}

	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.f, 1.f);

	if (Alpha < 0.5f)
	{
		const float HalfAlpha = Alpha * 2.0f;
		const float EasedAlpha = FWidgetAnimationUtils::EaseOutQuad(HalfAlpha);
		return FMath::Lerp(1.0f, PunchScale, EasedAlpha);
	}
	else
	{
		const float HalfAlpha = (Alpha - 0.5f) * 2.0f;
		const float EasedAlpha = FWidgetAnimationUtils::EaseOutBack(HalfAlpha);
		return FMath::Lerp(PunchScale, 1.0f, EasedAlpha);
	}
}

//-----------------------------------------------------------------------------
// FWidgetAnimationUtils - 고무도장 연출 (직원 가챠 / 출시 판정 공유)
//-----------------------------------------------------------------------------

float FWidgetAnimationUtils::StampInScale(float T01)
{
	const float T = FMath::Clamp(T01, 0.f, 1.f);
	if (T < 0.7f)
	{
		const float K = T / 0.7f;
		return 2.8f - (2.8f - 0.94f) * K * K;
	}
	return FMath::Lerp(0.94f, 1.f, (T - 0.7f) / 0.3f);
}

float FWidgetAnimationUtils::StampInOpacity(float T01, float MaxOpacity)
{
	return FMath::Clamp(T01 * 3.f, 0.f, MaxOpacity);
}

FVector2D FWidgetAnimationUtils::ShakeOffset(float Elapsed, float Duration, float Amp)
{
	if (Duration <= 0.f || Elapsed < 0.f || Elapsed >= Duration)
	{
		return FVector2D::ZeroVector;
	}
	const float K = 1.f - Elapsed / Duration;
	const float M = Amp * K;
	return FVector2D(FMath::Sin(Elapsed * 93.f) * M, FMath::Cos(Elapsed * 87.f) * M);
}
