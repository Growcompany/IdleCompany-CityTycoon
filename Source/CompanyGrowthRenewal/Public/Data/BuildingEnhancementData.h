// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "Enum/CompanyType.h"
#include "Enum/CostCurveType.h"
#include "BuildingEnhancementData.generated.h"

/**
 * 건물 강화 타입 (11종)
 * 공통 7 + 프로젝트형 전용 3 + 키스톤 전용 1.
 * 제조업 전용 슬롯은 세계지도 업그레이드 시스템으로 분리 (공장/채광소/무역항별 별도 강화).
 * 빌딩(오피스)에서 하는 일은 직원 배치 + 설계 스테이지 + Operation(프로젝트형) or 생산주문서 발행(제조업) 까지.
 */
UENUM(BlueprintType)
enum class EBuildingEnhancementType : uint8
{
	// === 공통 (0~6) — 모든 빌딩 노출 ===
	BuildingFloor        = 0  UMETA(DisplayName = "빌드업"),
	MarketCapMultiplier  = 1  UMETA(DisplayName = "브랜드파워"),
	EmployeeGrowth       = 2  UMETA(DisplayName = "인재육성"),
	MarketingPower       = 3  UMETA(DisplayName = "마케팅파워"),
	VaultCapacity        = 4  UMETA(DisplayName = "금고용량"),
	ProjectYield         = 5  UMETA(DisplayName = "프로젝트 산출량"),
	ProjectGrade         = 6  UMETA(DisplayName = "프로젝트 등급"),

	// === 프로젝트형 전용 (7~9) — Operation 페이즈 있는 빌딩 ===
	ProjectLifespan      = 7  UMETA(DisplayName = "히트작수명"),
	EventResistance      = 8  UMETA(DisplayName = "이벤트내성"),
	ViralBoost           = 9  UMETA(DisplayName = "바이럴가속"),

	// === 키스톤 모뉴먼트 전용 ===
	// 영향권(돔) 반경은 층수(BuildingFloor)에서 파생, 버프 효과(%)는 이 별도 업그레이드에서 파생
	KeystoneAuraPower    = 10 UMETA(DisplayName = "영향력")
};

/**
 * 강화 슬롯 카테고리 (회사 타입별 노출 필터)
 * Manufacture 는 현재 빌딩 강화에선 사용 안 함 (세계지도 시스템 예약).
 */
UENUM(BlueprintType)
enum class EEnhancementCategory : uint8
{
	Common  UMETA(DisplayName = "공통"),       // 모든 빌딩 노출
	Project UMETA(DisplayName = "프로젝트형")  // IsProjectType() 빌딩만 노출
};

/**
 * 마일스톤 보상 데이터
 */
USTRUCT(BlueprintType)
struct FMilestoneReward : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Milestone")
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Milestone")
	EBuildingEnhancementType EnhancementType = EBuildingEnhancementType::MarketingPower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Milestone")
	float PermanentEffectPercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Milestone")
	int32 RubyReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Milestone")
	FText SpecialTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Milestone")
	FText Description;
};

/**
 * 건물 강화 슬롯 정의 (DataTable Row)
 * 한 행 = 하나의 EBuildingEnhancementType = 하나의 슬롯 의미
 * RowName은 enum 식별자와 일치시키는 것을 권장 (예: "MarketingPower")
 */
USTRUCT(BlueprintType)
struct FBuildingEnhancementDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	EBuildingEnhancementType EnhancementType = EBuildingEnhancementType::BuildingFloor;

	// 산업 필터링 카테고리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	EEnhancementCategory Category = EEnhancementCategory::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	FText SubDescription;

	// 값 단위 표시 (예: "층", "%", "원"). 비어있으면 숫자만 표시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	FString ValueUnit;

	// 정수 표시 여부 (true면 소수점 생략)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	bool bIsInteger = false;

	// 패널 내 배치 순서 (오름차순, 같으면 enum 순서)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	int32 SortOrder = 0;

	// 강화 비용에 사용되는 자원 타입 (기본 Money)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enhancement")
	EResourceType CostResourceType = EResourceType::Money;

	// Lv 0 비용 (곡선 시작값). Money 강화 10종은 10, BuildingFloor 는 Brick 1000
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	int64 BaseCost = 1000;

	// 비용 곡선 형태. Money 강화 = Power, BuildingFloor(마일스톤형) = Geometric
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	ECostCurveType CostCurveType = ECostCurveType::Power;

	// Geometric 전용 — 다음 레벨 비용 배율. BuildingFloor 1.72
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	float CostGrowthRate = 1.08f;

	// Power 전용 — 초반 기울기 폭. 작을수록 초반이 가파르다. 현행 밴드 10(가파름)/11(표준)/12(완만)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	float CostLevelScale = 11.0f;

	// Power 전용 — 곡선 세기. 현행 전 슬롯 3.0 (세제곱)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	float CostExponent = 3.0f;

	// 레벨당 효과 증가량. 배율형은 0.0008 (= +0.08%/lv), 절대값형(BuildingFloor 등)은 1.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	float EffectPerLevel = 0.0008f;

	// true 면 비용에 빌딩 footprint 칸수를 곱한다 (2x2 = x4, 3x3 = x9).
	// 증축(BuildingFloor)은 층당 인원이 칸수에 비례해 늘어나므로, 비용도 같이 비례해야
	// "벽돌당 인원" 효율이 건물 크기와 무관해진다. 켜지 않으면 큰 건물이 공짜로 4~9배 이득이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	bool bScaleCostByFootprint = false;

	// 강화 최대 레벨 (0 = 무제한)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	int32 MaxLevel = 0;
};

/**
 * 건물 강화 스탯 계산 헬퍼 클래스
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingEnhancementHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// FootprintCells = 빌딩 칸수(가로x세로). DT 의 bScaleCostByFootprint 가 켜진 슬롯에서만 비용에 곱해진다.
	// ⚠ 표시와 청구가 같은 값을 넘겨야 한다 — 액터(ABuildingBaseActor::GetFootprintCells)가 단일 출처다.
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	static int64 CalculateUpgradeCost(EBuildingEnhancementType EnhancementType, int32 CurrentLevel, int32 FootprintCells = 1);

	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	static int64 CalculateBulkUpgradeCost(EBuildingEnhancementType EnhancementType, int32 CurrentLevel, int32 UpgradeCount, int32 FootprintCells = 1);

	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	static float CalculateEffectMultiplier(EBuildingEnhancementType EnhancementType, int32 Level);

	// 금고 용량의 내부 '시간' 축(초) — 화면엔 안 나온다. Capacity(원) = 티어 기준레이트 × VaultSeconds.
	// 곡선 k 는 VaultCapacity 행 EffectPerLevel(DT).
	static float CalculateVaultSeconds(int32 Level);

	// CompanyType에 대해 이 슬롯이 노출되어야 하는가
	UFUNCTION(BlueprintCallable, Category = "Building Enhancement")
	static bool IsEnhancementVisibleForCompanyType(EEnhancementCategory Category, ECompanyType CompanyType);

	static int64 GetBaseCost(EBuildingEnhancementType EnhancementType);
	static float GetCostGrowthRate(EBuildingEnhancementType EnhancementType);
	static float GetEffectPerLevel(EBuildingEnhancementType EnhancementType);
	// 강화 비용 자원 타입 (DT 단일 진실 — 빌드업=Brick, 나머지=Money). DT 미초기화 시 Money 폴백.
	static EResourceType GetCostResourceType(EBuildingEnhancementType EnhancementType);
};
