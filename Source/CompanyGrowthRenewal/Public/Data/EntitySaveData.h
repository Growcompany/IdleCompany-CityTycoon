// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/BuildingSaveData.h"
#include "EntitySaveData.generated.h"

/**
 * Building 전용 저장 데이터
 * 건물 엔티티의 위치, 회전, 스케일 및 건물 상세 정보 저장
 */
USTRUCT(BlueprintType)
struct FBuildingEntitySaveData
{
	GENERATED_BODY()

	// 건물 인덱스 (고유 식별자)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	int32 BuildingIndex = INDEX_NONE;

	// Interactable 이름 (DataTable RowName, 예: "Building_1")
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FName InteractableName;

	// 위치
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	// 회전
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FRotator Rotation = FRotator::ZeroRotator;

	// 스케일
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FVector Scale = FVector::OneVector;

	// 건물 상세 데이터 (층수, 스킨, 강화 레벨 등)
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FBuildingSaveData BuildingData;

	// 소속 도시 블록(부지) PlotId (DT_CityPlot RowName). NAME_None = 부지 미소속.
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FName PlotId = NAME_None;
};
