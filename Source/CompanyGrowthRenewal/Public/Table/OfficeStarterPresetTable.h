// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "OfficeStarterPresetTable.generated.h"

/**
 * 스타터 데코 슬롯 — 산업 무드 팔레트로 치환되는 항목 구분
 * None = 고정 항목 (DecorationRowName 그대로 사용)
 */
UENUM(BlueprintType)
enum class EStarterDecoSlot : uint8
{
	None      UMETA(DisplayName = "고정"),
	Picture   UMETA(DisplayName = "그림"),
	Plant     UMETA(DisplayName = "화분"),
	Accent    UMETA(DisplayName = "액센트")
};

/**
 * 스타터 프리셋의 데코 1개 배치 정보
 * RelLocation/YawDeg는 OfficeInterior 액터 기준 상대 좌표 (벽 장식 포함 — Transform이 배치를 완전 결정)
 */
USTRUCT(BlueprintType)
struct FStarterDecoEntry
{
	GENERATED_BODY()

	// Slot==None일 때 사용할 데코 RowName (DT_DecorationCard 키)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName DecorationRowName;

	// 산업 팔레트 치환 슬롯 (None이면 고정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EStarterDecoSlot Slot = EStarterDecoSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector RelLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawDeg = 0.f;
};

/**
 * 스타터 프리셋의 업무공간 1개 배치 정보 — 실제 기능하는 워크스테이션으로 시드됨
 */
USTRUCT(BlueprintType)
struct FStarterWorkstationEntry
{
	GENERATED_BODY()

	// DT_WorkstationCard 키
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName WorkstationTypeID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector RelLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawDeg = 0.f;
};

/**
 * DT_OfficeStarterPreset 행 — footprint 칸수별 베이스 레이아웃
 * RowName은 임의 키(Common_0 등 유지), 선택은 FootprintCells 버킷
 */
USTRUCT(BlueprintType)
struct FOfficeStarterPresetRow : public FTableRowBase
{
	GENERATED_BODY()

	// 런타임 자기 식별 (InitializeOfficeStarterPresetTable에서 할당, CSV 미포함 — BuildableCardTable 패턴)
	UPROPERTY(BlueprintReadOnly)
	FName RowName;

	// 시작 타일 크기를 결정하는 footprint 칸수 버킷 (1/2/4/9). CSV 임포트.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FootprintCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Variant = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TileCountX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TileCountY = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStarterDecoEntry> Decorations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStarterWorkstationEntry> Workstations;
};

/**
 * DT_IndustryMoodPalette 행 — 산업별 무드 치환 풀
 * RowName = 산업명 ("Game", "Electronics", "Finance", "IT", "Semiconductor", "Automobile")
 */
USTRUCT(BlueprintType)
struct FIndustryMoodPaletteRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECompanyType Industry = ECompanyType::None;

	// 시작 바닥 타일 (FOfficeSaveData.CurrentFloorTileRowName 시드)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName FloorTileRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> PicturePool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> PlantPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> AccentPool;

	// 산업별 워크스테이션 치환 풀 (DT_WorkstationCard 키) — 비어 있으면 프리셋 기본값 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> WorkstationPool;
};
