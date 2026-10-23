#pragma once

#include "CoreMinimal.h"

/**
 * 보이는 옥상이 전부 코인을 쏘므로, 줌아웃에서 64px 를 유지하면 화면이 코인으로 덮인다.
 * 멀어질수록 줄여 "반짝이는 점의 흐름" 으로 읽히게 한다 — 게이지와 달리 코인은 작아져도 거짓이 아니다
 * (게이지는 값을 말하므로 축소가 오독이 되지만, 코인은 "벌고 있다" 만 말한다).
 */
inline float ResolveStreamCoinDrawSize(
	float Zoom, float NearSize, float FarSize, float NearZoom, float FarZoom)
{
	// 노브가 뒤집히거나 같은 값이면 보간이 ±Inf 가 된다 — 줌인 크기로 고정해 화면을 덮는 쪽을 피한다
	if (!FMath::IsFinite(Zoom) || FarZoom - NearZoom <= KINDA_SMALL_NUMBER)
	{
		return NearSize;
	}

	const float T = FMath::Clamp((Zoom - NearZoom) / (FarZoom - NearZoom), 0.0f, 1.0f);
	return FMath::Lerp(NearSize, FarSize, T);
}

/**
 * 수익 팝업 소스 게이트 = "옥상 모듈이 카메라 안에 있는가".
 * 게이지의 ±200px 마진을 쓰지 않는 이유 = 게이지는 화면 밖에서 들어오는 동안 자리를 잡아둬야 하지만,
 * 팝업은 화면 밖에서 시작하면 가장자리에 금액만 떠오르는 유령이 된다.
 */
inline bool IsIncomePopupSourceVisible(const FVector2D& CanvasLocal, const FVector2D& CanvasSize)
{
	// 투영 실패는 NaN 으로 오고 NaN 비교는 전부 false 라 아래 범위 검사를 그냥 통과한다
	if (!FMath::IsFinite(CanvasLocal.X) || !FMath::IsFinite(CanvasLocal.Y))
	{
		return false;
	}
	if (CanvasSize.X <= 0.0 || CanvasSize.Y <= 0.0)
	{
		return false;
	}

	return CanvasLocal.X >= 0.0 && CanvasLocal.X <= CanvasSize.X
		&& CanvasLocal.Y >= 0.0 && CanvasLocal.Y <= CanvasSize.Y;
}
