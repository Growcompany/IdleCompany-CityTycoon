// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeStatsData.h"
#include "Enum/ItemType.h"
#include "EmployeePotentialData.generated.h"

// 확률표 UI 1행 — 래칫·상한을 반영한 실효 확률 (등급 내림차순)
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
	 * 잠재능력 등급 추첨 (하락 방지 적용)
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
	 * 확률표 UI용 실효 등급 확률 — 기본 추첨에 래칫(달성 등급 아래 → 달성 등급)과
	 * 상한 클램프(EffectiveCeiling 위 → 상한)를 적용해 실제로 나올 등급별 확률을 낸다.
	 * 기본 확률만 그대로 보여주면 래칫이 아래쪽을 한 칸에 뭉치므로 대부분의 직원에게 틀린 표가 된다.
	 * @return 확률 0 인 등급은 제외, 등급 내림차순
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Potential")
	static TArray<FPotentialRarityOdds> GetEffectiveRarityOdds(ELootBoxRarity MaxAchievedRarity, ELootBoxRarity CardCeiling);

	/** 기본 추첨 확률 (래칫·상한 적용 전) — 각주 표기용 */
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
