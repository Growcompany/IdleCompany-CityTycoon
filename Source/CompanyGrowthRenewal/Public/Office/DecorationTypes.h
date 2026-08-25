// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DecorationTypes.generated.h"

/**
 * 장식물이 배치 가능한 표면 타입
 */
UENUM(BlueprintType)
enum class EDecorationSurface : uint8
{
	None        UMETA(DisplayName = "없음"),     // 배치 대상 아님 (바닥 타일 등 전체 교체 방식)
	Wall        UMETA(DisplayName = "벽"),       // 벽 표면에 부착 (그림, 창문)
	Grid        UMETA(DisplayName = "그리드")    // 바닥 그리드에 배치 (가구, 의자, 파티션, 화분 등)
};

/**
 * 장식물 주요 카테고리
 */
UENUM(BlueprintType)
enum class EDecorationCategory : uint8
{
	None            UMETA(DisplayName = "없음"),

	// 벽 표면 (Wall Surface)
	Picture         UMETA(DisplayName = "그림"),
	Window          UMETA(DisplayName = "창문"),

	// 바닥 그리드 (Grid Surface)
	Chair           UMETA(DisplayName = "의자"),
	Furniture       UMETA(DisplayName = "가구"),       // 책상, 테이블, 수납장, 소파, TV장 등
	Partition       UMETA(DisplayName = "파티션"),
	Wall            UMETA(DisplayName = "벽"),         // 새로운 벽 생성 오브젝트
	Plant           UMETA(DisplayName = "화분"),

	// 전체 교체 (Surface = None)
	FloorTile       UMETA(DisplayName = "바닥 타일")
};

/**
 * 벽 방향 (왼쪽/오른쪽만 사용)
 */
UENUM(BlueprintType)
enum class EWallSide : uint8
{
	Left        UMETA(DisplayName = "왼쪽 벽"),
	Right       UMETA(DisplayName = "오른쪽 벽")
};
