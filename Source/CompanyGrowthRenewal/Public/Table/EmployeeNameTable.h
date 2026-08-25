// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/LootBoxRarity.h"
#include "EmployeeNameTable.generated.h"

/**
 * 직원 성 데이터 테이블
 */
USTRUCT(BlueprintType)
struct FLastNameData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LastName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Language = TEXT("English");
};

/**
 * 직원 핸들(별명 명사) 데이터 테이블 — 레어도별 풀에서 추첨
 */
USTRUCT(BlueprintType)
struct FEmployeeHandleData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Handle;

    // bIsFullName 인 행에서만 의미 있다 — 별명 핸들은 등급 무관 공통 풀이라 이 값을 보지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELootBoxRarity Rarity = ELootBoxRarity::Common;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Language = TEXT("Korean");

    // true면 성 추첨을 건너뛰고 Handle 자체가 완성된 이름 (황금손, 배태랑)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFullName = false;
};

// 성씨 글자가 핸들에 이미 들어 있으면 "정정석"처럼 말이 더듬어진다 — 그 성을 후보에서 뺀다
inline TArray<FString> FilterSurnamesForHandle(const TArray<FString>& Surnames, const FString& Handle)
{
	TArray<FString> Result;
	for (const FString& Surname : Surnames)
	{
		if (!Handle.Contains(Surname))
		{
			Result.Add(Surname);
		}
	}

	return Result.Num() > 0 ? Result : Surnames;
}

// 어순 기준은 핸들 쪽 언어(FEmployeeHandleData::Language) — 성씨 행의 Language 는 성씨 풀 필터용이라 여기 넘기면 한국인 이름이 "졸림 김"으로 뒤집힌다
inline FString ComposeEmployeeName(const FString& Surname, const FString& Handle, bool bIsFullName, const FString& Language)
{
	if (bIsFullName)
	{
		return Handle;
	}

	if (Language == TEXT("Korean"))
	{
		return Surname + Handle;
	}

	return Handle + TEXT(" ") + Surname;
}
