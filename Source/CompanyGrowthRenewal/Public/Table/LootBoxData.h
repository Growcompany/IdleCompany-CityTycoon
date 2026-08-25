// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/LootBoxCategory.h"
#include "LootBoxData.generated.h"

class UNiagaraSystem;
class USoundBase;

// 루트 박스 DataTable Row 구조체
// Row Name 예시: "Square_Epic", "Sphere_Legendary"
USTRUCT(BlueprintType)
struct FLootBoxTable : public FTableRowBase
{
	GENERATED_BODY()

	// 루트 박스 카테고리 (BuildingSkin, BuildingItem 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ELootBoxCategory Category = ELootBoxCategory::BuildingSkin;

	// 루트 박스 등급
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 루트 박스 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ELootBoxType Type = ELootBoxType::Square;

	// UI 표시용 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	// 닫혀있는 상자 메시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> ClosedMesh;

	// 열린 상자 메시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> OpenedMesh;

	// 상자 재질
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<UMaterialInterface> ChestMaterial;

	// 닫힌 상태 VFX (대기 중 효과)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> ClosedVFX;

	// 열리는 순간 VFX (NS_LtBox_Open - 모든 상자 공통)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> OpenVFX;

	// 열린 후 지속 VFX (NS_LtBoxSquare_Opened_Epic 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> OpenedVFX;

	// 기둥 효과 VFX (Epic, Rare, Unique, Legendary만 - 선택적)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> PillarVFX;

	// 열림 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> OpenSound;

	// 흔들림 최대 회전 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float ShakeRotationDegrees = 8.0f;

	// 흔들림 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float ShakeDuration = 0.25f;

	// 열린 후 떠오르는 높이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float RiseHeight = 10.0f;

	// 떠오름 지속 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float RiseDuration = 0.35f;
};
