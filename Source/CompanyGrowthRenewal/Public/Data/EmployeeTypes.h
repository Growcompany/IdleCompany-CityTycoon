// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EmployeeStatsData.h"
#include "CharacterAppearanceTypes.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/ProductionDiscipline.h"
#include "EmployeeTypes.generated.h"

UENUM(BlueprintType)
enum class EEmployeeGender : uint8
{
    Male    UMETA(DisplayName = "Male"),
    Female  UMETA(DisplayName = "Female")
};

UENUM(BlueprintType)
enum class EEmployeeDepartment : uint8
{
    None = 0        UMETA(DisplayName = "미배정"),
    Development = 1 UMETA(DisplayName = "개발팀"),     // Programming - Step2 주력
    Design = 2      UMETA(DisplayName = "기획팀"),     // Planning - Step1 주력 (구 명칭 디자인팀에서 의미 정정)
    Sales = 3       UMETA(DisplayName = "영업팀"),     // Operation - 운영 페이즈 + 수주 보조
    HR = 4          UMETA(DisplayName = "인사팀"),     // 메타 - 채용 영향
    Management = 5  UMETA(DisplayName = "경영팀"),     // 메타 - 전체 효율
    Art = 6         UMETA(DisplayName = "아트팀"),     // Step1+Step2 보조 (시각/콘텐츠 제작)
    QA = 7          UMETA(DisplayName = "QA팀"),     // Step3 주력 (검증/품질)
    Sound = 8       UMETA(DisplayName = "사운드팀"),   // 생산직능 — 오디오 제작
    Server = 9      UMETA(DisplayName = "서버팀")      // 생산직능 — 백엔드/서버
};

// 스탯 인덱스 (6종). 각 값이 서로 겹치지 않는 고유 메커니즘 1개씩 담당.
// DisplayName 이 UI 표시명의 단일 진실 — 코드에 스탯명 하드코딩 금지.
UENUM(BlueprintType)
enum class EEmployeeStatIndex : uint8
{
    WorkSpeed   = 0 UMETA(DisplayName = "업무속도"),
    CritChance  = 1 UMETA(DisplayName = "크리티컬 확률"),
    Composure   = 2 UMETA(DisplayName = "침착성(이탈)"),
    ExpGain     = 3 UMETA(DisplayName = "경험치획득"),
    Stamina     = 4 UMETA(DisplayName = "체력(졸음)"),
    Focus       = 5 UMETA(DisplayName = "업무집중도"),
    Count       = 6 UMETA(Hidden)   // 스탯 수 하드코딩 방지 (8→6 사고 재발 차단). EProductionDiscipline::Count 와 같은 규약
};

// 직능 → 부서 (1:1). 운영(Sales/HR/Management)은 직능 없음.
inline EEmployeeDepartment DisciplineToDepartment(EProductionDiscipline D)
{
    switch (D)
    {
    case EProductionDiscipline::Plan:     return EEmployeeDepartment::Design;
    case EProductionDiscipline::Dev:      return EEmployeeDepartment::Development;
    case EProductionDiscipline::Graphics: return EEmployeeDepartment::Art;
    case EProductionDiscipline::Sound:    return EEmployeeDepartment::Sound;
    case EProductionDiscipline::Server:   return EEmployeeDepartment::Server;
    case EProductionDiscipline::QA:       return EEmployeeDepartment::QA;
    default:                              return EEmployeeDepartment::None;
    }
}

// 부서 → 직능 역매핑. 운영/None/미배정은 EProductionDiscipline::Count(직능 없음).
inline EProductionDiscipline DepartmentToDiscipline(EEmployeeDepartment Dept)
{
    switch (Dept)
    {
    case EEmployeeDepartment::Design:      return EProductionDiscipline::Plan;
    case EEmployeeDepartment::Development: return EProductionDiscipline::Dev;
    case EEmployeeDepartment::Art:         return EProductionDiscipline::Graphics;
    case EEmployeeDepartment::Sound:       return EProductionDiscipline::Sound;
    case EEmployeeDepartment::Server:      return EProductionDiscipline::Server;
    case EEmployeeDepartment::QA:          return EProductionDiscipline::QA;
    default:                               return EProductionDiscipline::Count;
    }
}

// Department enum을 한글 문자열로 변환
inline FString DepartmentToString(EEmployeeDepartment Department)
{
    switch (Department)
    {
    case EEmployeeDepartment::Development:
        return TEXT("개발팀");
    case EEmployeeDepartment::Design:
        return TEXT("기획팀");
    case EEmployeeDepartment::Sales:
        return TEXT("영업팀");
    case EEmployeeDepartment::HR:
        return TEXT("인사팀");
    case EEmployeeDepartment::Management:
        return TEXT("경영팀");
    case EEmployeeDepartment::Art:
        return TEXT("아트팀");
    case EEmployeeDepartment::QA:
        return TEXT("QA팀");
    case EEmployeeDepartment::Sound:
        return TEXT("사운드팀");
    case EEmployeeDepartment::Server:
        return TEXT("서버팀");
    default:
        return TEXT("미배정");
    }
}

UENUM(BlueprintType)
enum class EEmployeeRank : uint8
{
    Intern = 0            UMETA(DisplayName = "인턴"),      // +0  | 잠재 1줄
    Assistant = 1         UMETA(DisplayName = "사원"),      // +1  | 잠재 1줄
    Associate = 2         UMETA(DisplayName = "주임"),      // +2  | 잠재 1줄
    SeniorAssociate = 3   UMETA(DisplayName = "대리"),      // +3  | 잠재 2줄
    Manager = 4           UMETA(DisplayName = "과장"),      // +4  | 잠재 2줄
    SeniorManager = 5     UMETA(DisplayName = "차장"),      // +5  | 잠재 2줄, 추가 1줄
    Director = 6          UMETA(DisplayName = "부장"),      // +6  | 잠재 3줄, 추가 1줄
    ManagingDirector = 7  UMETA(DisplayName = "이사"),      // +7  | 잠재 3줄, 추가 1줄
    ExecutiveDirector = 8 UMETA(DisplayName = "상무"),      // +8  | 잠재 3줄, 추가 2줄
    VP = 9                UMETA(DisplayName = "전무"),      // +9  | 잠재 3줄, 추가 2줄
    VicePresident = 10    UMETA(DisplayName = "부사장"),    // +10 | 잠재 3줄, 추가 2줄
    President = 11        UMETA(DisplayName = "사장"),      // +11 | 잠재 3줄, 추가 2줄
    Chairman = 12         UMETA(DisplayName = "회장")       // +12 | 잠재 3줄, 추가 2줄 (최고 직급)
};

// FLastNameData, FEmployeeHandleData는 Table/EmployeeNameTable.h로 이동됨

USTRUCT(BlueprintType)
struct FEmployeeInstance
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 EmployeeID = 0;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FString EmployeeName;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    EEmployeeGender Gender = EEmployeeGender::Male;

    // 입사(가챠 추첨) 시점 레어도 — 이름 핸들 tier 고정용. 잠재능력 큐브로 변하는 PotentialAbility 와 달리 영구 불변
    UPROPERTY(SaveGame, BlueprintReadWrite)
    ELootBoxRarity SpawnRarity = ELootBoxRarity::Common;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 Level = 1;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    float Experience = 0.0f;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 EnhancementLevel = 0;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FDateTime HiredDate;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    EEmployeeDepartment Department = EEmployeeDepartment::None;

    // 6직능 능력 포인트 (인덱스=EProductionDiscipline). 8범용스탯과 별개 특화 축. 비면 0×6.
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Discipline")
    TArray<int32> DisciplinePoints;

    // 플레이어가 투자한 직능 포인트(6). DisciplinePoints 에 이미 합산됨 — 이건 리셋 시 되돌릴 몫. 이너트 = DisciplinePoints - Invested.
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Discipline")
    TArray<int32> InvestedDisciplinePoints;

    // 미사용 스킬포인트 풀. 레벨업으로 적립, 직능 투자로 소모, 리셋으로 반환.
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Discipline")
    int32 AvailableSkillPoints = 0;

    // 배정된 건물 인덱스 (INDEX_NONE = 미배정)
    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 AssignedBuildingIndex = INDEX_NONE;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    bool bIsAssigned = false;

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // RPG 스탯 시스템 (레벨업 시 성장)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Stats")
    FEmployeeStats Stats;

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 잠재능력 & 추가옵션 (큐브 시스템)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Potential")
    FPotentialAbility PotentialAbility;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Potential")
    FAdditionalOption AdditionalOption;

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 외모 & 초상화
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Appearance")
    FCharacterAppearance Appearance;

    /** 초상화 텍스처 (런타임 전용, 저장 안됨) */
    UPROPERTY(BlueprintReadWrite, Category = "Appearance", Transient)
    TObjectPtr<UTexture2D> Portrait = nullptr;
};

// 등급 → 초기 직능 총 포인트 (튜닝 노브). 고등급일수록 뾰족한 전문가.
// 2026-08-13: 성장분(SkillPointsPerLevel)을 키운 대신 태생 롤을 낮춰 주 직능 상한을 10 으로 묶었다.
inline int32 GetDisciplineTotalForRarity(ELootBoxRarity Rarity)
{
    switch (Rarity)
    {
    case ELootBoxRarity::Mythic:    return 15;
    case ELootBoxRarity::Legendary: return 13;
    case ELootBoxRarity::Epic:      return 12;
    case ELootBoxRarity::Rare:      return 10;
    case ELootBoxRarity::Unusual:   return 9;
    default:                        return 8;    // Common
    }
}

// 주 직능 집중 + 소량 스프레드 프로필(6). PrimaryShare=0.65(튜닝 노브).
inline TArray<int32> MakeDisciplineProfile(EProductionDiscipline Primary, ELootBoxRarity Rarity)
{
    const int32 N = static_cast<int32>(EProductionDiscipline::Count); // 6
    TArray<int32> Points;
    Points.Init(0, N);
    const int32 Total = GetDisciplineTotalForRarity(Rarity);
    const int32 PrimaryIdx = FMath::Clamp(static_cast<int32>(Primary), 0, N - 1);
    const int32 PrimaryPts = FMath::RoundToInt(Total * 0.65f);
    Points[PrimaryIdx] = PrimaryPts;
    int32 Remaining = Total - PrimaryPts;
    while (Remaining > 0)   // 나머지는 부직능에만 — 주 직능에 얹히면 등급별 상한이 무너진다
    {
        int32 Idx = FMath::RandRange(0, N - 2);
        if (Idx >= PrimaryIdx) { ++Idx; }
        Points[Idx] += 1;
        --Remaining;
    }
    return Points;
}

// 6스탯에 Total 을 랜덤 분배 + 각 스탯 MinPerStat 보장. 0으로 태어나 죽은 스탯을 없애는 하한.
inline TArray<int32> MakeStatProfile(int32 Total = 30, int32 MinPerStat = 3)
{
    const int32 N = 6;
    TArray<int32> Out;
    Out.Init(MinPerStat, N);
    int32 Remaining = Total - MinPerStat * N;
    while (Remaining > 0)
    {
        Out[FMath::RandRange(0, N - 1)] += 1;
        --Remaining;
    }
    return Out;
}

// 프로필 배열(길이 6)을 스탯 필드에 적용. 스폰 경로가 2곳이라 매핑을 여기 한 곳에만 둔다.
inline void ApplyStatProfile(FEmployeeStats& S, const TArray<int32>& P)
{
    if (P.Num() < 6) { return; }
    S.WorkSpeed   = P[0];
    S.CritChance  = P[1];
    S.Composure   = P[2];
    S.ExpGain     = P[3];
    S.Stamina     = P[4];
    S.Focus       = P[5];
}

// argmax(직능) 슬롯. 동점=낮은 인덱스 우선. 전부 0/빈 배열 = INDEX_NONE(직능 없음).
// 부서칩 표시와 업무집중도 판정이 같은 "주 직능"을 봐야 하므로 판정은 여기 한 곳에만 둔다.
inline int32 GetPrimaryDisciplineSlot(const FEmployeeInstance& Emp)
{
    int32 BestIdx = INDEX_NONE, BestVal = 0;
    for (int32 i = 0; i < Emp.DisciplinePoints.Num(); ++i)
    {
        if (Emp.DisciplinePoints[i] > BestVal)
        {
            BestVal = Emp.DisciplinePoints[i];
            BestIdx = i;
        }
    }
    return BestIdx;
}

// argmax(직능) → 부서. 전부 0/빈 배열 = None(직능 없음).
inline EEmployeeDepartment GetDerivedDepartment(const FEmployeeInstance& Emp)
{
    const int32 BestIdx = GetPrimaryDisciplineSlot(Emp);
    if (BestIdx == INDEX_NONE) { return EEmployeeDepartment::None; }
    return DisciplineToDepartment(static_cast<EProductionDiscipline>(BestIdx));
}

// 파생부서를 저장필드에 동기화 — 직능 프로필이 있는 생산직원만. 운영/None은 기존 부서 유지.
inline void SyncDerivedDepartment(FEmployeeInstance& Emp)
{
    const EEmployeeDepartment Derived = GetDerivedDepartment(Emp);
    if (Derived != EEmployeeDepartment::None)
    {
        Emp.Department = Derived;
    }
}

// 직원 외모 데이터 (CharacterAppearanceTypes.h에서 이동)
USTRUCT(BlueprintType)
struct FEmployeeAppearanceData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FCharacterAppearance Appearance;

    UPROPERTY(BlueprintReadWrite)
    EEmployeeRank CurrentRank;
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeTypeHelper : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Employee")
    static EEmployeeRank GetRankFromEnhancementLevel(int32 EnhancementLevel);

    // 강화 별 표기 단일 진실 — ★(U+2605)×EnhancementLevel. 0성 = 빈 문자열(UI는 배지째 숨김). 직급 명칭 UI 표기 금지.
    static FString MakeStarString(int32 EnhancementLevel);

    // 강화 ★당 전 8스탯 파생 보너스 (유효스탯 = 저장값 + E×StatPerStar. 저장 안 함 — 하락 시 재계산만)
    // 밴드별 ★당 보너스 — 구 선형(전 구간 +2)은 위험 곡선과 어긋나 있었다.
    // 성공률이 95%→18%로 떨어지고 6성부터 하락까지 붙는데 보상이 평평하면 후반 강화를 할 이유가 없다.
    // GDD_CORE §강화 곡선 이 명시한 "마일스톤형 예외"(적은 횟수 + 의도적 큰 점프)에 해당.
    static constexpr int32 StatPerStarBand1 = 5;   // ★1~5   누적  25
    static constexpr int32 StatPerStarBand2 = 9;   // ★6~10  누적  70  (하락 위험 시작)
    static constexpr int32 StatPerStarBand3 = 13;  // ★11~15 누적 135

    UFUNCTION(BlueprintPure, Category = "Employee")
    static int32 GetEnhanceStatBonus(int32 EnhancementLevel)
    {
        const int32 E = FMath::Clamp(EnhancementLevel, 0, 15);
        const int32 B1 = FMath::Min(E, 5);
        const int32 B2 = FMath::Clamp(E - 5, 0, 5);
        const int32 B3 = FMath::Clamp(E - 10, 0, 5);
        return B1 * StatPerStarBand1 + B2 * StatPerStarBand2 + B3 * StatPerStarBand3;
    }

    // 직원 기본 출력(점수/수익 공용 스케일) 단일 정의 — 레벨=미세 드립. 강화 이득은 스탯 파생(GetEnhanceStatBonus) 경로.
    // 스펙: docs/superpowers/specs/2026-07-14-enhancement-starforce-design.md
    static float CalculateBaseOutput(int32 Level);
};