// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeStatsData.h"
#include "Enum/ItemType.h"
#include "EmployeePotentialData.generated.h"

// 등급별 확률 1행 — 기본 추첨과 래칫·상한 반영 실효 확률에 공용 (등급 내림차순)
USTRUCT(BlueprintType)
struct FPotentialRarityOdds
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Employee Potential")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Employee Potential")
	float Percent = 0.f;
};

// 잠재큐브 줄 가산 집계 결과 — 소비처: Stage 기여/수익/XP/크리 (spec 2026-07-08-potential-cube-wiring)
struct FPotentialModifiers
{
	float ScoreMult = 1.0f;      // 1 + Σ(WorkEfficiency)/100
	float IncomeMult = 1.0f;     // 1 + Σ(IncomeBonus)/100
	float ExpMult = 1.0f;        // 1 + Σ(ExpGain)/100
	float CritChanceAdd = 0.0f;  // Σ(CriticalChance) × 0.0004 — 값 25 = +1%p
	float CritDamageAdd = 0.0f;  // Σ(CriticalDamage) × 0.02 — 값 25 = 크리 배수 +0.5
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeePotentialHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 슬롯 정보
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

	/**
	 * 강화 레벨에서 직급 계산
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static EEmployeeRank GetRankFromEnhancementLevel(int32 EnhancementLevel);

	/**
	 * 직급에 따른 잠재능력 슬롯 수 얻기
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static int32 GetPotentialSlotsForRank(EEmployeeRank Rank);

	/**
	 * 직급에 따른 추가옵션 슬롯 수 얻기
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static int32 GetAdditionalSlotsForRank(EEmployeeRank Rank);

	// 큐브 줄 집계 (가산 합산). 줄 최대 3개라 히트 경로 직접 호출 허용.
	static FPotentialModifiers AggregateModifiers(const FPotentialAbility& Potential);


	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 큐브 재설정
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

	/**
	 * 잠재능력 재설정 (하락 방지 적용)
	 * @param InOutPotential 재설정할 잠재능력 (수정됨)
	 * @param AvailableSlots 현재 사용 가능한 슬롯 수 (직급에 따라)
	 * @param MaxCeiling 이번 리롤에 사용한 큐브의 등급 상한 (CurrentRarity를 이 값 이하로 클램프)
	 * @return 재설정 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Employee Potential")
	static bool ResetPotentialAbility(UPARAM(ref) FPotentialAbility& InOutPotential, int32 AvailableSlots, ELootBoxRarity MaxCeiling);

	/**
	 * 잠재 큐브 아이템 타입 → 리롤 등급 상한 매핑 (일반=Rare/고급=Epic/명장=Legendary)
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static ELootBoxRarity GetCubeCeiling(EItemType CubeType);

	/**
	 * 추가옵션 재설정 (하락 가능)
	 * @param InOutAdditional 재설정할 추가옵션 (수정됨)
	 * @param AvailableSlots 현재 사용 가능한 슬롯 수
	 * @return 재설정 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Employee Potential")
	static bool ResetAdditionalOption(UPARAM(ref) FAdditionalOption& InOutAdditional, int32 AvailableSlots);

	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 확률 계산
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

	/**
	 * 잠재능력 등급 계산 (하락 방지·상승 시 다음 등급까지만 적용)
	 * @param MaxAchievedRarity 달성한 최고 등급
	 * @return 새로운 등급
	 */
	UFUNCTION(BlueprintCallable, Category = "Employee Potential")
	static ELootBoxRarity RollPotentialRarity(ELootBoxRarity MaxAchievedRarity);

	/**
	 * 추가옵션 등급 추첨 (하락 가능)
	 * @return 새로운 등급
	 */
	UFUNCTION(BlueprintCallable, Category = "Employee Potential")
	static ELootBoxRarity RollAdditionalRarity();

	/**
	 * 재설정 실효 등급 확률 — 기초 분포에 하락 방지, 한 단계 상승 제한,
	 * 명함 상한을 적용해 실제 결과의 등급별 확률을 낸다.
	 * @return 확률 0 인 등급은 제외, 등급 내림차순
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static TArray<FPotentialRarityOdds> GetEffectiveRarityOdds(ELootBoxRarity MaxAchievedRarity, ELootBoxRarity CardCeiling);

	/** 등급별 기본 확률 (래칫·상한 적용 전) — 공통 상승 확률표의 등급 열거용 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static TArray<FPotentialRarityOdds> GetBaseRarityOdds();

	/**
	 * 등급에 따른 옵션 값 범위 얻기
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static void GetValueRangeForRarity(ELootBoxRarity Rarity, float& OutMin, float& OutMax);

	/**
	 * 랜덤 잠재능력 옵션 생성
	 */
	UFUNCTION(BlueprintCallable, Category = "Employee Potential")
	static FPotentialOptionLine GenerateRandomPotentialOption(ELootBoxRarity Rarity);

	/**
	 * 랜덤 추가옵션 생성
	 */
	UFUNCTION(BlueprintCallable, Category = "Employee Potential")
	static FAdditionalOptionLine GenerateRandomAdditionalOption(ELootBoxRarity Rarity);

	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 비용 계산
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

	/**
	 * 추가옵션 재설정 비용 계산
	 * @return 재설정 비용 (Ruby)
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static int32 GetAdditionalResetCost();

	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 유틸리티
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

	// 값 칸 전용 문자열 — 홈/드로어 값 칸의 단일 진실. 크리 2종은 적용 단위(%p/배)로 환산해 표기한다
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static FText GetPotentialOptionValueText(const FPotentialOptionLine& Option);

	// 등급이 오를 확률(%) = 100 - 현재 등급 이하로 접히는 확률. 래칫·상한은 GetEffectiveRarityOdds 가 접는다
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static float GetRarityUpgradeChancePercent(ELootBoxRarity CurrentRarity, ELootBoxRarity MaxAchievedRarity, ELootBoxRarity CardCeiling);

	// 잠긴 줄 해금 문구 — 슬롯 매핑(별0~2:1줄 / 별3~5:2줄 / 별6+:3줄)의 경계 별 수
	static FString GetLockedLineUnlockText(int32 LineIndex);

	/**
	 * 잠재능력 옵션 설명 생성 (예: "게임 프로젝트 +15%")
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static FText GetPotentialOptionDescription(const FPotentialOptionLine& Option);

	/**
	 * 추가옵션 설명 생성
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static FText GetAdditionalOptionDescription(const FAdditionalOptionLine& Option);

	/**
	 * 잠재능력 옵션 이름 얻기
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static FText GetPotentialOptionName(EPotentialOptionType OptionType);

	/**
	 * 추가옵션 이름 얻기
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static FText GetAdditionalOptionName(EAdditionalOptionType OptionType);
};
