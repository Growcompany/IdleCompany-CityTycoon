#pragma once

#include "CoreMinimal.h"
#include "Enum/OfficeExpansionType.h"

struct COMPANYGROWTHRENEWAL_API FOfficeFootprintGeometry
{
	static constexpr int32 MinTileCountX = 1;
	static constexpr int32 MinTileCountY = 2;
	static constexpr float FirstTileCenterX = 204.f;
	static constexpr float FirstTileCenterY = -209.f;
	static constexpr float FloorTileCenterZ = -6.f;
	static constexpr float StructuralFloorZ = 0.f;

	static FIntPoint NormalizeTileCount(FIntPoint Requested, FIntPoint Maximum);
	static bool IsTileCountValid(FIntPoint Count, FIntPoint Maximum);
	static FBox2D MakeFloorBounds(FIntPoint Count, float TileSizeCm);
	static FVector MakeTileCenter(int32 TileX, int32 TileY, float TileSizeCm);

	// 다음 확장으로 생길 칸. 성장은 +X(Left)·-Y(Right) 로 고정 모서리 반대쪽이다.
	// 그 축이 이미 최대면 무효 FBox2D 를 돌려준다 ― 호출 측이 막기 전에 여기서 막는다.
	static FBox2D MakeExpansionCellBounds(FIntPoint Count, FIntPoint Maximum, EOfficeExpandDirection Dir, float TileSizeCm);
	static FVector MakeExpansionCellCenter(FIntPoint Count, FIntPoint Maximum, EOfficeExpandDirection Dir, float TileSizeCm);

	/**
	 * 기존 구조물(바닥/벽)을 "한 칸 더" 복제할 때의 트랜스폼.
	 *
	 * 회전을 새로 계산하지 않는다 ― 원본 회전을 그대로 쓰고 성장 축으로 밀기만 한다.
	 * (회전 수식을 직접 쓰다 벽 토막이 90도 틀어진 적이 있다. 원본 복사는 그 실수가 불가능하다.)
	 * 미는 거리도 살아 있는 컴포넌트의 스케일에서 읽는다 ― WallRight 처럼 저작 보정값(+0.05)이
	 * 섞인 경우까지 자동으로 따라간다.
	 *
	 * @param Source           원본 컴포넌트의 상대 트랜스폼
	 * @param LocalAxis        성장이 일어나는 원본 로컬 축 (X 또는 Z)
	 * @param SignedLocalSpan  그 축으로 메시가 차지하는 길이, 성장 방향 부호 포함 (예: 벽 +400, 바닥 -400)
	 * @param AxisScale        그 축의 길이 배율. 1 이면 한 칸 전체, 0.2 면 기존 경계에 붙은 얇은 띠다.
	 *                         시작점은 배율과 무관하게 기존 구조물의 끝이다 ― 띠는 안쪽에서 바깥으로 자란다.
	 */
	static FTransform MakeExpansionSegmentTransform(
		const FTransform& Source, EAxis::Type LocalAxis, float SignedLocalSpan, float AxisScale = 1.f);
};
