#pragma once

#include "CoreMinimal.h"

// 포커스 가림 판정의 순수 계산부. 월드/액터에 의존하지 않아 자동화 테스트로 검증한다.
namespace FocusOcclusionMath
{
	// 타깃 바운드에서 가림 판정용 샘플 포인트 5개를 순서대로 뽑는다: 중심 / 상 / 하 / 우 / 좌.
	// Inset: 하프익스텐트 대비 비율(0~1). 가로는 짧은 축(Min(X,Y))을 쓰므로 1.0 이어도 긴 축 쪽 면에는 못 닿는다.
	// 바운드가 무효면 OutPoints 는 비워진 채 반환된다.
	COMPANYGROWTHRENEWAL_API void ComputeSamplePoints(
		const FBox& TargetBounds,
		const FVector& CameraRight,
		float Inset,
		TArray<FVector>& OutPoints);

	// 고스트 알파를 목표(0 또는 1)로 한 프레임 진행시킨다. 오버슈트하지 않는다.
	// TransitionTime <= 0 이면 즉시 목표값 (0 나눗셈 방지).
	COMPANYGROWTHRENEWAL_API float StepGhostAlpha(
		float Current,
		float Goal,
		float DeltaTime,
		float TransitionTime);
}
