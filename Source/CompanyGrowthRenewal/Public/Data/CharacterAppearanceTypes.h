// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterAppearanceTypes.generated.h"

// 헤어 파트 타입 enum
UENUM(BlueprintType)
enum class EHairPartType : uint8
{
    Base    UMETA(DisplayName = "Base"),
    Fringe  UMETA(DisplayName = "Fringe"),
    Back    UMETA(DisplayName = "Back"),
    Sides   UMETA(DisplayName = "Sides"),
    Eyebrows UMETA(DisplayName = "Eyebrows")
};

// ���� ���� ����ü��
USTRUCT(BlueprintType)
struct FHairColorSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color1;  // �⺻ ����

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color2;  // �׶��̼�/���̶���Ʈ

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor ColorLines;  // ����/��Ʈ������
};

USTRUCT(BlueprintType)
struct FEyeColorSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor EyeColor1;  // �ֿ� ����

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor EyeColor2;  // �׶��̼� ����
};

USTRUCT(BlueprintType)
struct FSkinColorSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SkinColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor MakeupColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BlushColor;
};


// ���� Ÿ�� ����ü
USTRUCT(BlueprintType)
struct FMorphTargetSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> MorphValues; // MorphTarget �̸��� ��
};

// ���� �ܸ� ����ü
USTRUCT(BlueprintType)
struct FCharacterAppearance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EyebrowsPartName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HairCombinationType = 1; // 1~7

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HairRandomSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FHairColorSet HairColors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEyeColorSet EyeColors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSkinColorSet SkinColors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMorphTargetSet FacialExpression;
};

// FHairPartTable, FRankClothingTable, FEyeColorTable, FSkinColorTable, FHairColorTable, FCharacterBaseMeshTable
// DataTable 구조체들은 Table/CharacterAppearanceTable.h로 이동됨

// FEmployeeAppearanceData는 EmployeeTypes.h로 이동됨 (순환 참조 방지)