// Fill out your copyright notice in the Description page of Project Settings.

#include "Util/CoordinateUtils.h"
#include "Player/MainMapPlayerController.h"

AMainMapPlayerController* CoordinateUtils::PlayerController = nullptr;

AMainMapPlayerController* CoordinateUtils::GetPlayerController()
{
	return PlayerController;
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

	IntersectionPos = FMath::LinePlaneIntersection(worldLocation, end, FVector::ZeroVector, FVector::UpVector);
}

bool CoordinateUtils::ProjectScreenPosToGroundPlane(FVector2D ScreenPos, FVector& IntersectionPos)
{
	auto* playerController = GetPlayerController();
	if (!playerController) return false;

	FVector worldLocation, worldDirection;
	if (!playerController->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, worldLocation, worldDirection))
		return false;

	FVector end = worldLocation + (worldDirection * 100000.0f);

	IntersectionPos = FMath::LinePlaneIntersection(worldLocation, end, FVector::ZeroVector, FVector::UpVector);

	return true;
}

bool CoordinateUtils::ProjectTouchToGroundPlane(FVector2D& ScreenPos, FVector& IntersectionPos)
{
	auto* playerController = GetPlayerController();
	if (!playerController) return false;

	FVector2D screenPos = GetViewportCenter();
	bool hasTouch = false;
	float worldLocZ = 0.f;

	// 정적 변수로 마지막 터치 위치 저장 (모바일용)
	static FVector2D LastValidTouchPosition = FVector2D::ZeroVector;
	static float LastTouchTime = 0.0f;  // 마지막 터치 시간

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
		
			float currentTime = playerController->GetWorld()->GetTimeSeconds();
		
			if (isCurrentlyTouching)
			{
				// 현재 터치 중
				hasTouch = true;
				screenPos = FVector2D(touchX, touchY);
				LastValidTouchPosition = screenPos;
				LastTouchTime = currentTime;
			}
			else
			{
				// 터치하지 않고 있음 - 터치 없음으로 처리
				hasTouch = false;
				screenPos = LastValidTouchPosition;  // 또는 마지막 위치 유지
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
		worldLocation, end, FVector::ZeroVector, FVector::UpVector);

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
	FVector retVal = position / 200.f;
	retVal.X = FMath::RoundToFloat(retVal.X) * 200.f;
	retVal.Y = FMath::RoundToFloat(retVal.Y) * 200.f;
	retVal.Z = 0.f;

	return retVal;
}

