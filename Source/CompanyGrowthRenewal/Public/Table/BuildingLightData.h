// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/LootBoxRarity.h"
#include "BuildingLightData.generated.h"

/**
 * 건물 조명 데이터
 * - 건물 머티리얼의 Window_Emissive_Color 파라미터 값만 관리 (머티리얼 자체 교체 X)
 * - DataTable로 관리
 */
USTRUCT(BlueprintType)
struct FBuildingLightData : public FTableRowBase
{
    GENERATED_BODY()

    // 조명 ID (희귀도 범위에 맞춰 할당: 100-199=Common, 200-299=Unusual, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
    int32 LightID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
    ELootBoxRarity Rarity = ELootBoxRarity::Common;

    // 조명 이름 (UI 표시용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
    FString LocalizationKey;

    // 카드 미리보기/저장된 컬러 (0~1 sRGB tint 그대로 EntityImage에 적용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
    FLinearColor EmissiveColor = FLinearColor::White;

    // 머티리얼 전달 시 EmissiveColor에 곱하는 강도 (>1 가능, HDR 발광)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
    float EmissiveIntensity = 1.0f;

    FBuildingLightData()
        : LightID(0)
        , Rarity(ELootBoxRarity::Common)
        , EmissiveColor(FLinearColor::White)
        , EmissiveIntensity(1.0f)
    {
    }
};

/**
 * 보유 중인 건물 조명 인스턴스
 * - 사용자가 획득한 조명 ID만 저장
 */
USTRUCT(BlueprintType)
struct FBuildingLightInstance
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Light")
    int32 LightID = 0;

    FBuildingLightInstance()
        : LightID(0)
    {
    }

    FBuildingLightInstance(int32 InLightID)
        : LightID(InLightID)
    {
    }
};
