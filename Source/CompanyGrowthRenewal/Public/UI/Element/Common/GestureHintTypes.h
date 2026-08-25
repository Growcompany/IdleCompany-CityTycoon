#pragma once

#include "CoreMinimal.h"

enum class EGestureHintKind : uint8 { None, Tap, Hold, Drag };

// 제스처 힌트 모션 — 초 단위 누적 시간을 트랜스폼으로. 위젯 Tick 과 테스트가 같은 식을 쓴다.
namespace GestureHintMotion
{
	constexpr float TapCycle  = 1.1f;   // 바운스 1주기
	constexpr float TapDipPx  = 7.f;    // 손끝이 내려가는 깊이
	constexpr float HoldCycle = 1.25f;  // 링 0→1 채움 1주기
	constexpr float HoldPressScale = 0.94f;
	constexpr float DragCycle = 1.8f;
	constexpr float DragAmpPx = 44.f;

	// 탭: 0~35% 내려가고 35~55% 머문 뒤 100% 에 복귀 (목업 tapBounce 키프레임)
	inline float TapOffsetY(float Elapsed)
	{
		const float T = FMath::Fmod(FMath::Max(0.f, Elapsed), TapCycle) / TapCycle;
		if (T < 0.35f) return TapDipPx * (T / 0.35f);
		if (T < 0.55f) return TapDipPx;
		return TapDipPx * (1.f - (T - 0.55f) / 0.45f);
	}
	// 탭 점 펄스: 35% 에 시작해 100% 까지 반경 .45→1.6 배, 알파 .95→0. 시작 전이면 알파 0
	inline void TapPulse(float Elapsed, float& OutScale, float& OutAlpha)
	{
		const float T = FMath::Fmod(FMath::Max(0.f, Elapsed), TapCycle) / TapCycle;
		if (T < 0.34f) { OutScale = 0.45f; OutAlpha = 0.f; return; }
		const float U = (T - 0.34f) / 0.66f;
		OutScale = FMath::Lerp(0.45f, 1.6f, U);
		OutAlpha = 0.95f * (1.f - U);
	}
	// 홀드 링: 12%~86% 동안 0→1, 86%~94% 는 1 유지 후 소멸, 나머지 0 (목업 holdFill)
	inline float HoldPercent(float Elapsed)
	{
		const float T = FMath::Fmod(FMath::Max(0.f, Elapsed), HoldCycle) / HoldCycle;
		if (T < 0.12f) return 0.f;
		if (T < 0.86f) return (T - 0.12f) / 0.74f;
		if (T < 0.94f) return 1.f;
		return 0.f;
	}
	// 홀드 중 손 스케일: 14%~86% 눌림
	inline float HoldScale(float Elapsed)
	{
		const float T = FMath::Fmod(FMath::Max(0.f, Elapsed), HoldCycle) / HoldCycle;
		return (T >= 0.14f && T <= 0.86f) ? HoldPressScale : 1.f;
	}
	// 드래그: -Amp → +Amp → -Amp, ease in-out(코사인)
	inline float DragOffsetX(float Elapsed)
	{
		const float T = FMath::Fmod(FMath::Max(0.f, Elapsed), DragCycle) / DragCycle;
		return -DragAmpPx * FMath::Cos(T * 2.f * PI);
	}
}
