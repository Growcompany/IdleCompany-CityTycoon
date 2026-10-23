// Fill out your copyright notice in the Description page of Project Settings.

#include "Util/CoordinateUtils.h"
#include "Player/MainMapPlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"              // TActorIterator
#include "UObject/ObjectKey.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Table/BuildingData.h"

AMainMapPlayerController* CoordinateUtils::PlayerController = nullptr;
FVector2D CoordinateUtils::LastTouchEventPosition = FVector2D::ZeroVector;

AMainMapPlayerController* CoordinateUtils::GetPlayerController()
{
	return PlayerController;
}

namespace
{
	// 부지를 아직 못 찾았을 때 다시 훑는 간격(초).
	constexpr double PlotProbeRetrySeconds = 0.5;

	// 화면→지면 투영이 쓰는 월드별 상수 묶음. 월드가 바뀌면 통째로 버린다.
	struct FGroundPlaneCache
	{
		FObjectKey WorldKey;
		bool   bCompactGridCity = false;  // 리버워크(4,300 격자)가 아닌 도시 = City_Pack 조립 맵
		double DeckZ = 0.0;
		bool   bDeckResolved = false;
		double NextProbeSeconds = 0.0;
	};

	FGroundPlaneCache& GetGroundPlaneCache(const UWorld* World)
	{
		static FGroundPlaneCache Cache;

		const FObjectKey Key(World);
		if (Cache.WorldKey != Key)
		{
			Cache = FGroundPlaneCache{};
			Cache.WorldKey = Key;
			// 맵 종류 판정은 프로젝트의 단일 디스크리미네이터(격자 셀 크기)를 공유한다. 여기서 맵 이름을
			// 따로 비교하면 도시 맵을 갈아끼울 때 이 파일만 조용히 뒤처진다 — 2026-10-03 회귀의 원인.
			Cache.bCompactGridCity = World && ResolvePlotBaseCellSize(World) != PlotBaseCellSizeRiverwalk;
		}
		return Cache;
	}

	bool IsCompactGridCity(const AMainMapPlayerController* PC)
	{
		const UWorld* World = PC ? PC->GetWorld() : nullptr;
		return World && GetGroundPlaneCache(World).bCompactGridCity;
	}

	// 배치·카메라 드래그가 광선을 꽂는 기준 평면의 Z.
	// 리버워크는 도시 지오메트리 지면이 Z=0 인 맵이라 평면도 0 이다(부지 액터 Z 는 손으로 놓은 값이라
	// 거기서 유도하면 오히려 지면과 어긋난다). 그 외 도시 맵은 지면이 높은 데크 위에 있어 레벨에 실제로
	// 있는 부지 액터에서 읽는다 — 상수로 박으면 평면만 과거 맵에 남고, 기울어진 카메라에서는 Z 뿐 아니라
	// 광선이 더 멀리 가서 XY 까지 밀린다(= 건물이 엉뚱한 곳에 생기고 CanDrop 이 0 으로 굳는 증상).
	double GroundPlaneZ(const AMainMapPlayerController* PC)
	{
		const UWorld* World = PC ? PC->GetWorld() : nullptr;
		FGroundPlaneCache& Cache = GetGroundPlaneCache(World);
		if (!World || !Cache.bCompactGridCity)
		{
			return 0.0;
		}
		if (Cache.bDeckResolved)
		{
			return Cache.DeckZ;
		}

		// 부지를 못 찾은 결과는 확정하지 않는다 — World Partition 은 월드 BeginPlay 이후에도 부지 셀을
		// 스트리밍 인 하므로 한 번의 "없음"을 캐시하면 영구히 틀린 평면을 쓴다. 반대로 매 호출 순회는
		// 드래그가 매 프레임 부르는 경로라 비싸다. 그래서 확정될 때까지만 간격을 두고 다시 훑는다.
		const double Now = World->GetTimeSeconds();
		if (Now < Cache.NextProbeSeconds)
		{
			return Cache.DeckZ;
		}
		Cache.NextProbeSeconds = Now + PlotProbeRetrySeconds;

		int32 PlotCount = 0;
		double MinZ = 0.0;
		for (TActorIterator<ACityPlotActor> It(World); It; ++It)
		{
			const double PlotZ = It->GetActorLocation().Z;
			MinZ = (PlotCount == 0) ? PlotZ : FMath::Min(MinZ, PlotZ);
			++PlotCount;
		}
		if (PlotCount == 0)
		{
			return Cache.DeckZ;
		}

		// 최소값 = 도시 본체 데크. 그보다 높은 부지는 테라스 단이다(실측 WPO_05 가 +499).
		// 단별 평면이 필요해지면 호버 부지의 Z 로 갈아타야 한다 — 권위는 UPlacementHandler::GetPlotSurfaceZ 와 같다.
		Cache.DeckZ = MinZ;
		Cache.bDeckResolved = true;
		UE_LOG(LogTemp, Log, TEXT("[CoordinateUtils] 투영 기준 평면 Z = %.0f (부지 %d개 중 최소)"), MinZ, PlotCount);
		return Cache.DeckZ;
	}

	// City_Pack 타일은 500 모듈이고 격자 원점이 실측 (18.1, 463.7) 만큼 밀려 있다.
	// 200 무오프셋으로 스냅하면 한 칸은 타일 경계, 다음 칸은 타일 안쪽으로 번갈아 어긋난다.
	// 이 오프셋은 타일 팩의 격자 위상이다 — 팩을 다른 위상으로 깐 맵에서는 다시 재야 한다.
	float SnapToCityPackGrid(float Value, float Offset)
	{
		return FMath::RoundToFloat((Value - Offset) / 500.f) * 500.f + Offset;
	}
}

FVector2D CoordinateUtils::GetViewportCenter()
{
	auto playerController = GetPlayerController();

	int32 viewportSizeX, viewportSizeY;
	playerController->GetViewportSize(viewportSizeX, viewportSizeY);

	FVector2D screenPos;
	screenPos.X = viewportSizeX / 2.f;
	screenPos.Y = viewportSizeY / 2.f;

	return screenPos;
}

void CoordinateUtils::ProjectViewportCenterToGroundPlane(FVector& IntersectionPos)
{
	auto* playerController = GetPlayerController();
	if (!playerController) return;

	FVector2D screenPos = GetViewportCenter();

	FVector worldLocation, worldDirection;
	playerController->DeprojectScreenPositionToWorld(screenPos.X, screenPos.Y, worldLocation, worldDirection);

	FVector end = worldLocation + (worldDirection * 100000.0f);

	IntersectionPos = FMath::LinePlaneIntersection(worldLocation, end, FVector(0.f, 0.f, GroundPlaneZ(GetPlayerController())), FVector::UpVector);
}

bool CoordinateUtils::ProjectScreenPosToGroundPlane(FVector2D ScreenPos, FVector& IntersectionPos)
{
	auto* playerController = GetPlayerController();
	if (!playerController) return false;

	FVector worldLocation, worldDirection;
	if (!playerController->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, worldLocation, worldDirection))
		return false;

	FVector end = worldLocation + (worldDirection * 100000.0f);

	IntersectionPos = FMath::LinePlaneIntersection(worldLocation, end, FVector(0.f, 0.f, GroundPlaneZ(GetPlayerController())), FVector::UpVector);

	return true;
}

bool CoordinateUtils::ProjectTouchToGroundPlane(FVector2D& ScreenPos, FVector& IntersectionPos)
{
	auto* playerController = GetPlayerController();
	if (!playerController) return false;

	FVector2D screenPos = GetViewportCenter();
	bool hasTouch = false;
	float worldLocZ = 0.f;

	switch (static_cast<int>(playerController->GetCurrentInputType()))
	{
		case static_cast<int>(EInputType::KeyMouse):
		{
			double mouseX, mouseY;
			if (playerController->GetMousePosition(mouseX, mouseY))
			{
				hasTouch = true;
				screenPos = FVector2D(mouseX, mouseY);
			}
			break;
		}
		case static_cast<int>(EInputType::GamePad):
		{
			hasTouch = true;
			// 일부 GamePad 입력은 직접 위치 제공 안 하므로 center 사용
			break;
		}
		case static_cast<int>(EInputType::Touch):
		{
			double touchX, touchY;
			bool isCurrentlyTouching = false;
			playerController->GetInputTouchState(ETouchIndex::Touch1, touchX, touchY, isCurrentlyTouching);

			if (isCurrentlyTouching)
			{
				hasTouch = true;
				screenPos = FVector2D(touchX, touchY);
				LastTouchEventPosition = screenPos;
			}
			else
			{
				// 손을 이미 뗐어도 이번 제스처의 이벤트 위치는 유효하다 — 직전 제스처 위치로 쏘면 엉뚱한 대상이 맞는다.
				hasTouch = false;
				screenPos = LastTouchEventPosition;
			}
			break;
		}
	}

	if (!hasTouch) return false;


	// worldLocation: Camera(Player위치), wolrdDirection : Camera에서 클릭한 곳까지 방향
	FVector worldLocation, worldDirection;
	if (!playerController->DeprojectScreenPositionToWorld(screenPos.X, screenPos.Y, worldLocation, worldDirection))
		return false;

	// 카메라에서 시작해서 매우 먼 거리까지 뻗은 직선 생성
	FVector end = worldLocation + (worldDirection * 100000.0f);

	// 평면(0,0,0)위의 점과 법선벡터를 등록해서 교차점 계산
	FVector intersectionLoc = FMath::LinePlaneIntersection(
		worldLocation, end, FVector(0.f, 0.f, GroundPlaneZ(GetPlayerController())), FVector::UpVector);

	// 터치 안할 때는 아래쪽으로 위치하게
	intersectionLoc.Z += worldLocZ;

	ScreenPos = screenPos;
	IntersectionPos = intersectionLoc;

	return true;
}

bool CoordinateUtils::SingleTouchCheck()
{
	float locX, locY;
	bool isCurrentlyTouching = false;
	GetPlayerController()->GetInputTouchState(ETouchIndex::Touch2, locX, locY, isCurrentlyTouching);

	return !isCurrentlyTouching;
}

FVector CoordinateUtils::SteppedPosition(const FVector& position) // 200으로 칸을 나눠서 값 반환
{
	const AMainMapPlayerController* PC = GetPlayerController();
	if (IsCompactGridCity(PC))
	{
		return FVector(
			SnapToCityPackGrid(position.X, 18.1f),
			SnapToCityPackGrid(position.Y, 463.7f),
			GroundPlaneZ(PC));
	}

	FVector retVal = position / 200.f;
	retVal.X = FMath::RoundToFloat(retVal.X) * 200.f;
	retVal.Y = FMath::RoundToFloat(retVal.Y) * 200.f;
	retVal.Z = GroundPlaneZ(PC);

	return retVal;
}

