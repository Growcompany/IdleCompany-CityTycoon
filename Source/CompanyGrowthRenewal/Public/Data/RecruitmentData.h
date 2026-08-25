// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EmployeeTypes.h"
#include "Data/CharacterAppearanceTypes.h"
#include "Enum/LootBoxRarity.h"
#include "RecruitmentData.generated.h"

/**
 * [DEPRECATED] 지원자 이력서 데이터
 * - 구 채용 시스템용, 가챠 전환 후 사용하지 않음
 * - 기존 세이브 역호환을 위해 구조체 유지
 */
USTRUCT(BlueprintType)
struct FApplicantResume
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Identity")
	int32 ApplicantID = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Identity")
	FString Name;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Identity")
	EEmployeeGender Gender = EEmployeeGender::Male;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Identity")
	EEmployeeDepartment Department = EEmployeeDepartment::None;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Appearance")
	FCharacterAppearance Appearance;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Rarity")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Rarity")
	bool bRarityHidden = false;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Enhancement")
	int32 EnhancementLevel = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Enhancement")
	bool bEnhancementHidden = false;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Potential")
	ELootBoxRarity PotentialRarity = ELootBoxRarity::Common;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Salary")
	int64 ExpectedSalary = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Cooldown")
	FDateTime AvailableTime;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Applicant|Cooldown")
	bool bOnCooldown = false;

	UPROPERTY(BlueprintReadWrite, Category = "Applicant|Runtime", Transient)
	TObjectPtr<UTexture2D> Portrait = nullptr;

	FApplicantResume()
		: ApplicantID(0)
		, Name(TEXT(""))
		, Gender(EEmployeeGender::Male)
		, Department(EEmployeeDepartment::None)
		, Rarity(ELootBoxRarity::Common)
		, bRarityHidden(false)
		, EnhancementLevel(0)
		, bEnhancementHidden(false)
		, PotentialRarity(ELootBoxRarity::Common)
		, ExpectedSalary(0)
		, AvailableTime(FDateTime::MinValue())
		, bOnCooldown(false)
		, Portrait(nullptr)
	{
	}
};

/**
 * [DEPRECATED] 건물별 채용 데이터
 * - 구 지원자 기반 채용 시스템용
 * - 기존 세이브 역호환을 위해 구조체 유지
 */
USTRUCT(BlueprintType)
struct FBuildingRecruitmentData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Recruitment|Level")
	int32 Level = 1;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Recruitment|Applicants")
	TArray<FApplicantResume> Applicants;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Recruitment|Refresh")
	FDateTime LastRefreshTime;

	FBuildingRecruitmentData()
		: Level(1)
		, LastRefreshTime(FDateTime::MinValue())
	{
	}
};

/**
 * 오피스별 채용 데이터
 */
USTRUCT(BlueprintType)
struct FOfficeRecruitmentData
{
	GENERATED_BODY()

	// 건물 인덱스
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "OfficeRecruitment|Identity")
	int32 BuildingIndex = INDEX_NONE;

	// 이 건물 누적 채용 수 — 사원증 사번(뽑힌 순서) 표기용. 해고돼도 줄지 않는 단조 증가 카운터.
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "OfficeRecruitment|Identity")
	int32 TotalHiredCount = 0;

	FOfficeRecruitmentData()
		: BuildingIndex(INDEX_NONE)
	{
	}

	explicit FOfficeRecruitmentData(int32 InBuildingIndex)
		: BuildingIndex(InBuildingIndex)
	{
	}
};
