// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Enum/LootBoxRarity.h"
#include "BuildingSkinData.generated.h"

/**
 * 건물 스킨 데이터
 * - 건물에 적용할 머티리얼만 관리
 * - DataTable로 관리
 */
USTRUCT(BlueprintType)
struct FBuildingSkinData : public FTableRowBase
{
	GENERATED_BODY()

	// 스킨 ID (희귀도 범위에 맞춰 할당: 100-199=Common, 200-299=Unusual, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	int32 SkinID = 0;

	// 스킨 희귀도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	// 스킨 이름 (UI 표시용 - String Table Key)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText DisplayName;

	// 다국어 지원을 위한 이름 키 (선택사항)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FString LocalizationKey;

	// 스킨 아이콘 (UI 표시용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	// 스킨 머티리얼 (건물에 적용할 재질)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	TSoftObjectPtr<UMaterialInterface> SkinMaterial;

	FBuildingSkinData()
		: SkinID(0)
		, Rarity(ELootBoxRarity::Common)
	{
	}
};

/**
 * 보유 중인 건물 스킨 인스턴스
 * - 사용자가 획득한 스킨 ID만 저장
 */
USTRUCT(BlueprintType)
struct FBuildingSkinInstance
{
	GENERATED_BODY()

	// 스킨 ID
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Skin")
	int32 SkinID = 0;

	FBuildingSkinInstance()
		: SkinID(0)
	{
	}

	FBuildingSkinInstance(int32 InSkinID)
		: SkinID(InSkinID)
	{
	}
};
