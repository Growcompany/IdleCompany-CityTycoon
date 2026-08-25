// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BuildingTraitRequirement.generated.h"

// 건물 특성 장착 제한
// None = 모든 건물 장착 가능 (공통)
// Manufacturing = 제조업 빌딩에만 장착 (Mfg_Conveyor 등 고정값 효과)
// Project = Operation 페이즈 있는 빌딩에만 장착 (Proj_Plan 등 시간 가산)
UENUM(BlueprintType)
enum class EBuildingTraitRequirement : uint8
{
	None			UMETA(DisplayName = "제한 없음"),
	Manufacturing	UMETA(DisplayName = "제조업 전용"),
	Project			UMETA(DisplayName = "프로젝트 전용")
};
