// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/LootBoxRarity.h"
#include "EmployeeStatsData.generated.h"

/**
 * 잠재능력 옵션 타입 (메이플스토리 큐브 시스템)
 */
UENUM(BlueprintType)
enum class EPotentialOptionType : uint8
{
	None = 0 UMETA(DisplayName = "없음"),

	// 기본 능력치 강화 (항상 유용)
	WorkEfficiency        UMETA(DisplayName = "업무 효율"),
	IncomeBonus           UMETA(DisplayName = "수익 보너스"),
	ExpGain               UMETA(DisplayName = "경험치 획득"),

	// 프로젝트 타입 특화
	GameProject           UMETA(DisplayName = "게임 프로젝트"),
	AppProject            UMETA(DisplayName = "앱 개발"),
	WebProject            UMETA(DisplayName = "웹 서비스"),
	AIProject             UMETA(DisplayName = "AI 프로젝트"),

	// 부서 특화 (7개 부서 — 직원이 해당 부서일 때 효율 보너스)
	DevelopmentExpert     UMETA(DisplayName = "개발 전문"),
	DesignExpert          UMETA(DisplayName = "기획 전문"),       // 구 디자인. Design enum 의미 정정 (기획팀)
	SalesExpert           UMETA(DisplayName = "영업 전문"),       // 운영 페이즈 보너스
	HRExpert              UMETA(DisplayName = "인사 전문"),       // 채용 메타
	ManagementExpert      UMETA(DisplayName = "경영 전문"),       // 전체 메타
	ArtExpert             UMETA(DisplayName = "아트 전문"),       // 신규 — Step1+Step2 보조
	QAExpert              UMETA(DisplayName = "품질 전문"),       // 신규 — Step3 주력

	// 제품 카테고리 특화
	ElectronicsProduct    UMETA(DisplayName = "전자제품"),
	FurnitureProduct      UMETA(DisplayName = "가구"),
	FoodProduct           UMETA(DisplayName = "식품"),
	FashionProduct        UMETA(DisplayName = "패션"),

	// 피버타임 관련 (수치형)
	FeverTimeChance       UMETA(DisplayName = "피버타임 확률"),
	FeverTimeDuration     UMETA(DisplayName = "피버타임 지속시간"),
	FeverTimeEffect       UMETA(DisplayName = "피버타임 효과"),

	// 크리티컬 관련
	CriticalChance        UMETA(DisplayName = "크리티컬 확률"),
	CriticalDamage        UMETA(DisplayName = "크리티컬 배율"),

	// 생산 관련
	DoubleProduction      UMETA(DisplayName = "2배 생산 확률"),
	QualityBonus          UMETA(DisplayName = "제품 품질"),
	SpeedBonus            UMETA(DisplayName = "작업 속도 보너스"),

	// 수익 관련
	BonusIncome           UMETA(DisplayName = "추가 보너스 수익"),
	PassiveIncome         UMETA(DisplayName = "자동 수익"),

	// 실패 방지
	FailureResistance     UMETA(DisplayName = "실패 확률 감소"),
	GuaranteeSuccess      UMETA(DisplayName = "성공 보장"),
};

/**
 * 추가옵션 타입 (전역 버프)
 */
UENUM(BlueprintType)
enum class EAdditionalOptionType : uint8
{
	None = 0 UMETA(DisplayName = "없음"),

	// 전역 효율 버프
	AllEmployeeBonus      UMETA(DisplayName = "전체 직원 효율"),
	BuildingIncome        UMETA(DisplayName = "소속 건물 수익"),
	CompanyMarketCap      UMETA(DisplayName = "회사 시가총액"),
	AllProjectBonus       UMETA(DisplayName = "모든 프로젝트 수익"),
	FactorySpeed          UMETA(DisplayName = "모든 공장 속도"),

	// 수치형 전역 버프
	EmployeeExpBonus      UMETA(DisplayName = "전체 직원 경험치"),
	ResourceGeneration    UMETA(DisplayName = "자원 획득량"),
};

/**
 * 잠재능력/추가옵션 옵션 라인
 */
USTRUCT(BlueprintType)
struct FPotentialOptionLine
{
	GENERATED_BODY()

	/** 옵션 타입 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Potential")
	EPotentialOptionType OptionType = EPotentialOptionType::None;

	/** 옵션 값 (퍼센트 또는 수치) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Potential")
	float Value = 0.0f;

	/** 등급 (Common ~ Mythic) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Potential")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	FPotentialOptionLine()
		: OptionType(EPotentialOptionType::None)
		, Value(0.0f)
		, Rarity(ELootBoxRarity::Common)
	{
	}
};

/**
 * 추가옵션 라인
 */
USTRUCT(BlueprintType)
struct FAdditionalOptionLine
{
	GENERATED_BODY()

	/** 옵션 타입 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additional")
	EAdditionalOptionType OptionType = EAdditionalOptionType::None;

	/** 옵션 값 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additional")
	float Value = 0.0f;

	/** 등급 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additional")
	ELootBoxRarity Rarity = ELootBoxRarity::Common;

	FAdditionalOptionLine()
		: OptionType(EAdditionalOptionType::None)
		, Value(0.0f)
		, Rarity(ELootBoxRarity::Common)
	{
	}
};

/**
 * 잠재능력 데이터 (하락 방지 시스템)
 */
USTRUCT(BlueprintType)
struct FPotentialAbility
{
	GENERATED_BODY()

	/** 현재 등급 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Potential")
	ELootBoxRarity CurrentRarity = ELootBoxRarity::Common;

	/** 최고 달성 등급 (하락 방지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Potential")
	ELootBoxRarity MaxAchievedRarity = ELootBoxRarity::Common;

	/** 옵션 라인 (최대 3줄) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Potential")
	TArray<FPotentialOptionLine> Options;

	FPotentialAbility()
		: CurrentRarity(ELootBoxRarity::Common)
		, MaxAchievedRarity(ELootBoxRarity::Common)
	{
	}
};

/**
 * 추가옵션 데이터 (하락 가능)
 */
USTRUCT(BlueprintType)
struct FAdditionalOption
{
	GENERATED_BODY()

	/** 현재 등급 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additional")
	ELootBoxRarity CurrentRarity = ELootBoxRarity::Common;

	/** 옵션 라인 (최대 2줄) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additional")
	TArray<FAdditionalOptionLine> Options;

	FAdditionalOption()
		: CurrentRarity(ELootBoxRarity::Common)
	{
	}
};

/**
 * 직원 스탯 (하이브리드 시스템)
 * - 자동 성장: BaseEfficiency (레벨당 +2%)
 * - 자유 배분: 기본 스탯 6종 (스폰 분배 + ★ 성장)
 * - 잠재능력: 큐브 리롤 % 축 (메이플식, 하락 방지)
 * - 추가옵션: 전역 버프 (하락 가능)
 */
USTRUCT(BlueprintType)
struct FEmployeeStats
{
	GENERATED_BODY()

	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 1. 자동 성장 스탯
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

	/** 기본 업무 효율 (레벨당 자동 +2%) */
	UPROPERTY(BlueprintReadOnly, Category = "Stats|Auto")
	float BaseEfficiency = 1.0f;

	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 2. 기본 스탯 (6종) — 스폰 시 총량 30 분배, 성장은 ★ 강화 단일 경로
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

	/** orb 배출 간격 단축 → 바닥 후 배출 개수 증가 (=DPS) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 WorkSpeed = 0;

	/** 크리티컬 확률 가산 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 CritChance = 0;

	/** 폭주(뛰쳐나감) 확률 감소 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Composure = 0;

	/** 경험치 획득 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 ExpGain = 0;

	/** 피로 저항 + 근무/휴식 지속 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Stamina = 0;

	/** 배출 오브가 자기 주 직능으로 나올 확률 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Focus = 0;

	FEmployeeStats()
		: BaseEfficiency(1.0f)
		, WorkSpeed(0)
		, CritChance(0)
		, Composure(0)
		, ExpGain(0)
		, Stamina(0)
		, Focus(0)
	{
	}
};

/**
 * 6스탯 효과 계수 — 단일 진실. 예전엔 각 계수가 EmployeeBehaviorComponent(private constexpr) /
 * EmployeeManager(함수 안 리터럴) / ProjectOperationManager(익명 namespace) 에 흩어져 있어
 * UI가 실제 효과를 읽을 방법이 없었다(2026-07-26 스탯 재설계 후속). 여기로 이동만 — 값 변경 없음.
 * (침착성/체력 일부 노브는 기획자 런타임 튜닝 표면인 UFatigueConfig DataAsset에 남는다 — DA_FatigueConfig 참조)
 */
namespace EmployeeStatTuning
{
	// ★ 밴드(2026-07-27) 로 유효스탯 상한이 45 → 140 으로 커졌다. 아래 계수는 그 기준으로 재조정된 값.
	//   · 업무속도만 끝점을 의도적으로 올렸다(SF 2.02 → 3.0) — 배출 간격 곡선이 이번 재설계의 목적.
	//   · 나머지는 × 0.25(=35/140) 로 끝점 보존 — 밴드는 "후반 별을 값지게"지 전체 파워 인플레가 아니다.

	// 업무속도 — 유효값 1당 SpeedFactor 가산 (EmployeeBehaviorComponent::GetSpeedFactor)
	// ★15(유효 140) 에서 SF 4.0 → 배출 간격 2.0s / 4.0 = 0.50초. 그 아래 0.5→0.2초 구간은 잠재 SpeedMult 몫.
	constexpr float WorkSpeedFactorPerPoint = 3.0f / 140.0f;

	// 크리티컬 확률 — 유효값 1당 크리 확률 가산 (EmployeeBehaviorComponent 크리 판정)
	// ★15(유효 140) 에서 20%p — 만강 보상이 체감되도록 2026-08-13 상향(구 7.65%p)
	constexpr float CritChancePerPoint = 0.20f / 140.0f;

	// 스테이지 기본 크리 확률 — 스탯과 무관하게 전 직원에 깔리는 바닥.
	// 스탯은 순수 비례라(1포인트 = 일정 기여) ★0 에서 0.7%p 밖에 안 되는데, 그것만으론 크리가 아예 안 터져
	// "죽은 스탯"으로 보인다. 절편은 스탯 곡선을 휘게 하는 대신 여기가 담당한다 — ★0 합계 2.0%.
	// 2026-08-13: 0.003 → 0.013. SOT 는 여기이며 UOfficeStageProgressManager::CriticalChance 가 이 값으로 초기화된다.
	constexpr float BaseCritChance = 0.013f;

	// 크리 확률 최종 합산 상한. 크리에만 sweep 사운드+orb 버스트가 붙어 punctuation 역할을 하므로,
	// 이 선을 넘기면 연출이 배경 소음이 된다 (스탯만으로는 최대 7.65%p — 상한은 버프/큐브 폭주 대비).
	constexpr float MaxCritChance = 0.30f;

	// 경험치획득 — 유효경험치획득 1당 경험치 배율 가산 (EmployeeManager::AddExperience)
	// 끝점 보존: 140 × 0.005 = ×1.70 (구 35 × 0.02 와 동일)
	constexpr float ExpGainMultiplierPerPoint = 0.005f;

	// 체력 — 유효체력 1당 근무/휴식 지속시간 배율 (EmployeeBehaviorComponent::GetWorkDuration/GetRestDuration)
	// 끝점 보존: 근무 ×3.0 / 휴식 ÷2.0 (구 35 기준과 동일)
	constexpr float WorkDurationStaminaScale = 0.0143f;
	constexpr float RestDurationStaminaScale = 0.0073f;

	// 업무집중도 — 유효값 1당 "주 직능 강제 지정" 확률 가산 (EmployeeBehaviorComponent 오브 직능 선택)
	// ★15(유효 140) 에서 70%. 2026-08-13 구 수익보너스 슬롯을 대체 — 사무실 평균으로만 작동하던 축이라
	// 개인 카드에 개인값을 적으면 거짓말이 됐다(직원 → 운영 수익 기여는 이때 폐지).
	constexpr float FocusPerPoint = 0.005f;

	// 주 직능 강제 지정 확률의 상한. 1.0 이면 직원 1명 사무실에서 다른 스텝 점수가 영구 0 이 되어
	// 출시 게이트(MeetsMinimumClearScore)를 영영 못 넘는다 — 15% 를 남겨 전 스텝에 최소 유입을 보장한다.
	constexpr float MaxFocusChance = 0.85f;
}

/**
 * 직원 스탯 헬퍼 클래스
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeStatsHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 레벨에 따른 BaseEfficiency 계산 (레벨당 +2%)
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static float CalculateBaseEfficiencyForLevel(int32 Level);

	/**
	 * 전체 스탯의 총합 계산 (Overall 점수)
	 * @param Stats 직원 스탯
	 * @return 총 스탯 포인트
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static int32 CalculateOverall(const FEmployeeStats& Stats);

	/**
	 * 유효 스탯 총합 (저장값 + ★ 파생 × 스탯 수). 표시용 종합은 이걸 쓸 것 —
	 * CalculateOverall 은 저장값만이라 ★가 유일 성장 경로인 현 설계에서 "성장 안 함"으로 보인다.
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static int32 CalculateEffectiveOverall(const FEmployeeStats& Stats, int32 EnhancementLevel);

	/**
	 * 스탯 인덱스로 스탯 값 조회
	 * @param StatIndex 스탯 인덱스 (0~5)
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static int32 GetStatValueByIndex(const FEmployeeStats& Stats, uint8 StatIndex);

	/**
	 * 스탯 인덱스로 표시명 조회 (EEmployeeStatIndex UMETA DisplayName 이 단일 진실 — 하드코딩 금지)
	 * @param StatIndex 스탯 인덱스 (0~5)
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static FText GetStatDisplayName(uint8 StatIndex);

	/**
	 * 스탯 인덱스 + 유효값(저장값+강화보너스) → 실효과 설명 1줄 (예: "크리티컬 확률 +6.0%").
	 * 계수는 EmployeeStatTuning(코드 상수) / DA_FatigueConfig(침착성·체력 DataAsset 노브) 단일 진실 — 하드코딩 금지.
	 * @param StatIndex 스탯 인덱스 (0~5)
	 * @param EffectiveValue 유효값 (저장값 + 강화보너스)
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static FText GetStatEffectText(uint8 StatIndex, int32 EffectiveValue);

	/**
	 * 스탯 인덱스 + 유효값 → 값 자리에 그대로 찍는 체감 단위 1개 ("0.50초" / "20.0%" / "215초").
	 * 원시 포인트는 플레이어에게 의미가 없어 카드 표면에서 감춘다 — 포인트가 필요한 표면은 GetStatValueByIndex 를 쓸 것.
	 * @param StatIndex 스탯 인덱스 (0~5)
	 * @param EffectiveValue 유효값 (저장값 + 강화보너스)
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static FText GetStatFeltText(uint8 StatIndex, int32 EffectiveValue);

	/**
	 * 강화분이 체감 단위로 얼마나 바뀌었는지 ("-1.31초" / "+151초"). 변화 없으면 빈 FText.
	 * 부호는 실제 방향 그대로 — 업무속도만 음수가 이득이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Employee Stats")
	static FText GetStatFeltDeltaText(uint8 StatIndex, int32 BaseValue, int32 EffectiveValue);
};
