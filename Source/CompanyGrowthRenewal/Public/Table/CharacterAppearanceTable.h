// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/EmployeeTypes.h"
#include "Data/CharacterAppearanceTypes.h"
#include "CharacterAppearanceTable.generated.h"

/**
 * 헤어 파트 DataTable 구조체
 */
USTRUCT(BlueprintType)
struct FHairPartTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PartName;  // "hair_base01", "hair_fringe02" 등

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> HairMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHairPartType PartType;  // Base, Fringe, Back, Sides
};

/**
 * 직급별 의상 DataTable 구조체
 */
USTRUCT(BlueprintType)
struct FRankClothingTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EEmployeeRank Rank;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> TopMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> AccessoryMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USkeletalMesh>> ShoeMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;
};

/**
 * 눈 색상 DataTable 구조체
 */
USTRUCT(BlueprintType)
struct FEyeColorTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ColorSetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEyeColorSet ColorSet;
};

/**
 * 피부색 DataTable 구조체
 */
USTRUCT(BlueprintType)
struct FSkinColorTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ColorSetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSkinColorSet ColorSet;
};

/**
 * 헤어 색상 DataTable 구조체
 */
USTRUCT(BlueprintType)
struct FHairColorTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ColorSetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FHairColorSet ColorSet;
};

/**
 * 기본 캐릭터 메시 DataTable (BaseFace, Mouth, Ears, Hands 등)
 */
USTRUCT(BlueprintType)
struct FCharacterBaseMeshTable : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EEmployeeGender Gender;  // Male or Female

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> BaseFaceMesh;

    // Animation Blueprint 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftClassPtr<UAnimInstance> AnimBlueprintClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> MouthMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> EarsMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> HandsMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultHairBaseMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultHairBackMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultHairSidesMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultHairFringeMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultEyebrowsMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultTopMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultTrousersMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultShoesMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultAccessoryMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> DefaultBeltMesh;
};
