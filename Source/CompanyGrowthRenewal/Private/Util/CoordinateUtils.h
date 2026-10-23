// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AMainMapPlayerController;

class CoordinateUtils
{

private:
	static AMainMapPlayerController* PlayerController;
	static AMainMapPlayerController* GetPlayerController();
	static FVector2D LastTouchEventPosition;

public:
	static void SetPlayerController(AMainMapPlayerController* controller)
	{
		PlayerController = controller;
	}

	// 짧은 탭은 누름·뗌이 한 프레임에 들어와 처리 시점의 터치 상태가 이미 '뗌'이다 — 이벤트가 준 위치를 기억해 둔다.
	static void RecordTouchEventPosition(const FVector2D& ScreenPos)
	{
		LastTouchEventPosition = ScreenPos;
	}
	static FVector2D GetLastTouchEventPosition() { return LastTouchEventPosition; }

	static FVector2D GetViewportCenter();
	static void ProjectViewportCenterToGroundPlane(FVector& IntersectionPos);
	static bool ProjectScreenPosToGroundPlane(FVector2D ScreenPos, FVector& IntersectionPos);
	static bool ProjectTouchToGroundPlane(FVector2D& ScreenPos, FVector& IntersectionPos);
	static bool SingleTouchCheck();
	static FVector SteppedPosition(const FVector& position);
};
