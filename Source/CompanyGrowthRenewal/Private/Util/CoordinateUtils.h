// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AMainMapPlayerController;

class CoordinateUtils
{

private:
	static AMainMapPlayerController* PlayerController;
	static AMainMapPlayerController* GetPlayerController();

public:
	static void SetPlayerController(AMainMapPlayerController* controller)
	{
		PlayerController = controller;
	}

	static FVector2D GetViewportCenter();
	static void ProjectViewportCenterToGroundPlane(FVector& IntersectionPos);
	static bool ProjectScreenPosToGroundPlane(FVector2D ScreenPos, FVector& IntersectionPos);
	static bool ProjectTouchToGroundPlane(FVector2D& ScreenPos, FVector& IntersectionPos);
	static bool SingleTouchCheck();
	static FVector SteppedPosition(const FVector& position);
};
