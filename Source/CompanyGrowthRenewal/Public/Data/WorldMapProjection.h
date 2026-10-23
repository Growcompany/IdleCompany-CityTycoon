#pragma once

#include "CoreMinimal.h"

/**
 * 판(전략 지도 테이블)의 위경도 ↔ 정규화 좌표 변환.
 *
 * 판 범위는 경도 -140~160 / 위도 74~-48 로 잘려 있다. 게임의 11개국이
 * 캐나다~일본 사이에만 있어, 경도 360도를 전부 담으면 빈 태평양이 화면의
 * 최대 면적을 먹고 나라들이 좁은 영역에 몰려 메달리온이 겹친다.
 */
struct COMPANYGROWTHRENEWAL_API FWorldMapProjection
{
	static constexpr double Lon0 = -140.0;
	static constexpr double Lon1 =  160.0;
	static constexpr double Lat0 =   74.0;
	static constexpr double Lat1 =  -48.0;

	static constexpr double AspectRatio = (Lon1 - Lon0) / (Lat0 - Lat1);

	/**
	 * 메달리온 지름의 판 너비 대비 상한. 이웃 간 거리가 이보다 좁으면 원이 겹친다.
	 * 근거 = 판 너비의 3.9% 디스크 + 알림 도트 오버행. **메달리온이 판 너비에 비례해 그려질 때만 유효하다** ―
	 * 슬레이트 절대 px 로 그려지면 해상도마다 비가 달라져 이 상수가 거짓말을 한다.
	 */
	static constexpr double MinSeparation = 0.051;

	static FVector2D ToAnchor(double Lon, double Lat);

	/** 판 너비를 1 로 둔 거리. 판이 가로로 길어 Y 차이는 AspectRatio 로 나눠야 같은 축이 된다. */
	static double AnchorDistance(const FVector2D& A, const FVector2D& B);

	/** (0,0) 은 "DT 에 안 채웠다"의 신호로 예약돼 있다 — 배치 대상에서 뺀다. */
	static bool IsAnchorSet(const FVector2D& Anchor);
};
