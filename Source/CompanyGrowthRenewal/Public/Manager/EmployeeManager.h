// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Table/ConstructionCost.h"
#include "Data/CharacterAppearanceTypes.h"
#include "Data/EmployeeTypes.h"
#include "Enum/QualityGrade.h"
#include "Enum/GachaTier.h"
#include "Enum/ItemType.h"
#include "Enum/EmployeeState.h"

#include "EmployeeManager.generated.h"

class UTableManagerSubsystem;
class AOfficeworkerMale;
class AOfficeworkerFemale;
class AStickOfficeworker;
class UNiagaraSystem;
struct FEmployeeHandleData;

// 경험치 획득 이벤트 (UI 알림용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExperienceGained, int32, EmployeeID, float, Amount);

// 직원 상태 토스트 (졸음/딴짓/폭주 — OfficeMain 이 구독해 이벤트 레일로 라우팅).
// EmployeeID 는 레일이 같은 직원의 알림을 교체/조기해제하는 키. Duration 은 실제로 손댈 수 있는 구간 길이.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEmployeeStatusToast, int32, EmployeeID, const FText&, Message, float, Duration);

// 직원이 업무로 복귀 — 아직 떠 있는 그 직원의 알림을 조기 해제하라는 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmployeeStatusToastCleared, int32, EmployeeID);

// 캡처 요청 1건 — 촬영 리그가 1대뿐이라 인자를 스냅샷해 두고 직렬 처리한다
USTRUCT()
struct FPortraitCaptureRequest
{
    GENERATED_BODY()

    FString EmployeeID;
    FCharacterAppearance Appearance;
    EEmployeeRank Rank = EEmployeeRank::Intern;
    EEmployeeGender Gender = EEmployeeGender::Male;
    int32 HairCombinationType = 0;
    int32 RandomSeed = 0;
    EEmployeeDepartment Department = EEmployeeDepartment::None;
    EGachaTier Tier = EGachaTier::Normal;
};

/**
 *
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

// 사진 촬영용
public:
    // 사진 촬영용 재사용 액터
    UPROPERTY()
    AOfficeworkerMale* MalePortraitWorker;

    UPROPERTY()
    AOfficeworkerFemale* FemalePortraitWorker;

    // 스틱맨 캡쳐 워커 (유니섹스 — 남/녀 분기 없이 1개). 레벨에 bIsPortraitMode 로 배치, 1순위로 사용.
    UPROPERTY()
    AStickOfficeworker* StickPortraitWorker;

    // Department/Tier 는 스틱 코스메틱 산출에 필요 (구 모듈러 경로는 무시). 캡쳐 호출자가 FEmployeeInstance 에서 채워 전달.
    UFUNCTION()
    void CaptureEmployeePortrait(const FString& EmployeeID, const FCharacterAppearance& Appearance,
        EEmployeeRank Rank, EEmployeeGender Gender,
        int32 HairCombinationType, int32 RandomSeed,
        EEmployeeDepartment Department, EGachaTier Tier);

    // 초상화 파이프라인 busy 여부 — 대기 큐 잔량 포함(배치 뽑기가 전체 완료를 봐야 함)
    UFUNCTION(BlueprintCallable)
    bool IsPortraitCapturing() const { return bPortraitCapturing || PendingPortraitQueue.Num() > 0; }

private:
    bool bPortraitCapturing = false;

    // 완료 브로드캐스트 중 재진입한 캡처 요청은 적재만 — 실행 중인 워커 델리게이트 재바인딩 UB 방지
    bool bInPortraitCompletion = false;

    // FIFO — 촬영 리그 1대를 여러 요청이 공유해 서로 덮는 것을 막는다
    TArray<FPortraitCaptureRequest> PendingPortraitQueue;

    void StartPortraitCapture(const FPortraitCaptureRequest& Request);
    void DequeueNextPortraitCapture();

    // 레벨 전환으로 캡처 완료 델리게이트가 영영 안 오는 경우의 해제 — 서브시스템이 월드보다 오래 살아 상태가 남는다
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    FDelegateHandle WorldCleanupHandle;

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Employee 고용 (부서 지정, 강화 레벨 기본 0 = 인턴)
    UFUNCTION(BlueprintCallable)
    bool HireEmployee(EEmployeeDepartment Department, int32 EnhancementLevel = 0);

    // 카드 데이터 기반 직원 고용 (채용 카드용 - 외모, 이름, 스탯 등 유지)
    UFUNCTION(BlueprintCallable)
    bool HireEmployeeFromCard(const FEmployeeInstance& CardData, int32 BuildingIndex, bool bShouldSave = true);

    // 고용 정원 판정 규칙. 월드 의존이 없어 단위 테스트가 가능한 형태로 분리 — 게이트의 단일 소유.
    static bool IsUnderCapacity(int32 CurrentCount, int32 Capacity) { return Capacity > 0 && CurrentCount < Capacity; }

    // 해당 건물 소속 인원(벤치 포함). 고용 상한의 분자 = EmployeeList 파생, 별도 카운터 없음.
    UFUNCTION(BlueprintPure, Category = "Employee")
    int32 GetEmployeeCountInBuilding(int32 BuildingIndex) const;

    UFUNCTION(BlueprintPure, Category = "Employee")
    bool CanHireIntoBuilding(int32 BuildingIndex) const;

    // **게임 내 유일한 인원 상한.** 고용은 OfficeMap 에서 일어나는데 빌딩 액터는 MainMap 에만 복원되므로
    // 세이브(증축 층수) + DT(FBuildingData) 에서 파생한다. 책상 개수는 상한이 아니다(무제한 배치).
    // bLogIfZero: 게이트 경로는 true(0 = 조용한 채용 차단이라 원인을 남겨야 함). 표시 경로는 false —
    // MainMap 카드가 건물마다 호출해 정상 상황의 경고로 로그를 덮는다.
    UFUNCTION(BlueprintPure, Category = "Employee")
    int32 GetBuildingEmployeeCapacity(int32 BuildingIndex, bool bLogIfZero = true) const;

    // 지정 증축 층수 기준 인원 상한(빌드업 다음 층 미리보기용). AddedFloors < 0 이면 세이브의 현재 층수를 쓴다.
    // 위 함수는 이 함수의 얇은 래퍼 — 공식·조회 경로가 하나로 유지된다.
    UFUNCTION(BlueprintPure, Category = "Employee")
    int32 GetBuildingEmployeeCapacityAtFloors(int32 BuildingIndex, int32 AddedFloors, bool bLogIfZero = true) const;

    // 인원 상한 초과 안내 문구. 고용 시점 게이트와 채용 진입 사전차단이 같은 문구를 써야 원인이 하나로 읽힌다.
    static FText GetCapacityFullMessage();

    // Employee 해고 (건물에서 해제)
    UFUNCTION(BlueprintCallable)
    bool FireEmployee(int32 EmployeeInstanceID);

    // Employee 목록 조회
    UFUNCTION(BlueprintCallable)
    TArray<FEmployeeInstance> GetAllEmployees() const { return EmployeeList; }

    // 미배치 직원 목록 조회 (채용 가능한 직원들)
    UFUNCTION(BlueprintCallable)
    TArray<FEmployeeInstance> GetUnassignedEmployees() const;

    // 특정 건물에 배치된 직원 목록 조회
    UFUNCTION(BlueprintCallable)
    TArray<FEmployeeInstance> GetEmployeesInBuilding(int32 BuildingIndex) const;

    // 특정 건물 소속 미배치(벤치) 직원 목록 — 책상 로스터 피커용
    UFUNCTION(BlueprintCallable)
    TArray<FEmployeeInstance> GetUnassignedEmployeesInBuilding(int32 BuildingIndex) const;

    // 특정 건물에 스폰된 Officeworker 액터 목록 조회 (런타임 버프/상태 접근용)
    UFUNCTION(BlueprintCallable)
    TArray<AOfficeworker*> GetSpawnedWorkersInBuilding(int32 BuildingIndex) const;

    // Employee 개수
    UFUNCTION(BlueprintCallable)
    int32 GetEmployeeCount() const { return EmployeeList.Num(); }

    // 특정 Employee 찾기
    FEmployeeInstance* FindEmployee(int32 InstanceID);

    // Employee 데이터 조회 (포인터 반환, C++에서만 사용)
    FEmployeeInstance* GetEmployeeData(int32 EmployeeID);

    // Employee 특정 빌딩에 배치
    UFUNCTION(BlueprintCallable)
    bool AssignEmployeeToBuilding(int32 EmployeeID, int32 BuildingIndex);

    // Employee 빌딩에서 해제 (해고)
    UFUNCTION(BlueprintCallable)
    bool UnassignEmployee(int32 EmployeeID);

    UFUNCTION(BlueprintCallable)
    int32 GetNextInstanceID() const { return NextInstanceID; }

    // ID 할당 및 자동 증가 (RecruitmentManager 등에서 사용)
    UFUNCTION(BlueprintCallable)
    int32 AllocateNextInstanceID() { return NextInstanceID++; }

    UFUNCTION(BlueprintCallable)
    const TMap<int32, FEmployeeAppearanceData>& GetEmployeeAppearances() const {
        return
            EmployeeAppearances;
    }

    UFUNCTION(BlueprintCallable)
    void SetEmployeeList(const TArray<FEmployeeInstance>& Data) { EmployeeList = Data; }

    UFUNCTION(BlueprintCallable)
    void SetNextInstanceID(int32 NextID) { NextInstanceID = NextID; }

    UFUNCTION(BlueprintCallable)
    void SetEmployeeAppearances(const TMap<int32, FEmployeeAppearanceData>& Appearances) {
        EmployeeAppearances = Appearances;
    }

    // ========== 건물별 직원 관리 (저장/로드용) ==========

    // 특정 건물의 직원 목록 조회 (저장용)
    UFUNCTION(BlueprintCallable)
    TArray<FEmployeeInstance> GetEmployeesByBuilding(int32 BuildingIndex) const;

    // 특정 건물의 외모 데이터 조회 (저장용)
    UFUNCTION(BlueprintCallable)
    TMap<int32, FCharacterAppearance> GetAppearancesByBuilding(int32 BuildingIndex) const;

    // 특정 건물의 직원 목록 설정 (로드용)
    UFUNCTION(BlueprintCallable)
    void SetEmployeesForBuilding(int32 BuildingIndex, const TArray<FEmployeeInstance>& Employees);

    // [dev 프리셋] 직원 배치 생성. HireEmployee 의 bPortraitCapturing 가드(false 반환으로 루프를 조용히 깸)와
    // 재화 차감을 건너뛰고, 가챠와 같은 규칙으로 스탯/직능을 만든다. RandomSeed 로 매 실행 동일 로스터.
    void SeedEmployeesForBuilding(int32 BuildingIndex, int32 Count, int32 LevelMin, int32 LevelMax,
        int32 EnhanceMin, int32 EnhanceMax, int32 RandomSeed);

    // 특정 건물의 외모 데이터 설정 (로드용)
    UFUNCTION(BlueprintCallable)
    void SetAppearancesForBuilding(int32 BuildingIndex, const TMap<int32, FCharacterAppearance>& Appearances);

    // 특정 건물의 직원 데이터 삭제 (새 건물 초기화용)
    UFUNCTION(BlueprintCallable)
    void ClearEmployeesForBuilding(int32 BuildingIndex);

    // ========== 선택 관련 함수들 ==========

    UFUNCTION(BlueprintCallable)
    void SelectEmployee(int32 EmployeeID);

    UFUNCTION(BlueprintCallable)
    void DeselectEmployee();

    UFUNCTION(BlueprintCallable)
    int32 GetSelectedEmployeeID() const { return SelectedEmployeeID; }

    FEmployeeInstance* GetSelectedEmployee();

    UFUNCTION(BlueprintCallable)
    bool IsEmployeeSelected(int32 EmployeeID) const { return SelectedEmployeeID == EmployeeID; }

    // 선택 변경 이벤트
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEmployeeSelectionChanged, int32 /*OldID*/, int32 /*NewID*/);
    FOnEmployeeSelectionChanged OnEmployeeSelectionChanged;

    // 직원 고용 완료 이벤트 (초상화 캡처 완료 시 — 실제 벤치 적립보다 이름)
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnEmployeeHireCompleted, const FString& /*EmployeeID*/);
    FOnEmployeeHireCompleted OnEmployeeHireCompleted;

    // 로스터 변경 (EmployeeList 에 벤치 적립/해고 등 실제 추가·제거 시) — 책상 패널 로스터 갱신용(타이밍 정확)
    DECLARE_MULTICAST_DELEGATE(FOnEmployeeRosterChanged);
    FOnEmployeeRosterChanged OnEmployeeRosterChanged;

    // 직급 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Employee Rank")
    FString GetRankDisplayName(EEmployeeRank Rank);

    UFUNCTION(BlueprintCallable, Category = "Employee Rank")
    FString GetEmployeeFullName(int32 EmployeeID); // "김철수 대리" 형태

    UFUNCTION(BlueprintCallable)
    int32 GetMaxExperienceForLevel(int32 Level) const;

    // ========== 경험치 시스템 ==========

    // 직원별 경험치 획득 배율 (유효 ExpGain 스탯 기반, 큐브 배율은 별도) — AddExperience 와 표기 계산이 같은 식을 쓰게 하는 단일 정의
    UFUNCTION(BlueprintPure, Category = "Employee Experience")
    float GetExpGainMultiplier(const FEmployeeInstance& Employee) const;

    // 경험치 추가 (레벨업 없이 단순 추가)
    UFUNCTION(BlueprintCallable, Category = "Employee Experience")
    bool AddExperience(int32 EmployeeID, float Amount);

    // 빌딩 내 모든 직원에게 경험치 분배. 반환 = 직원 1인당 지급 기준량(큐브 배율 전), 직원 0 이면 0 — 실패 화면 "경험치 +N" 표기용
    UFUNCTION(BlueprintCallable, Category = "Employee Experience")
    float DistributeExperienceToBuilding(int32 BuildingIndex, float BaseAmount, EQualityGrade QualityGrade);

    // 품질 등급별 경험치 배율 반환
    UFUNCTION(BlueprintPure, Category = "Employee Experience")
    float GetQualityMultiplier(EQualityGrade Grade) const;

    // 경험치 획득 이벤트 (UI 알림용)
    UPROPERTY(BlueprintAssignable, Category = "Employee Experience")
    FOnExperienceGained OnExperienceGained;

    // 직원 상태 토스트 이벤트 (OfficeMain 이 1회 구독 → 이벤트 레일). N명의 직원 상태가 이 단일 채널로 모임.
    UPROPERTY(BlueprintAssignable, Category = "Employee|Status")
    FOnEmployeeStatusToast OnEmployeeStatusToast;

    UPROPERTY(BlueprintAssignable, Category = "Employee|Status")
    FOnEmployeeStatusToastCleared OnEmployeeStatusToastCleared;

    // 직원이 업무에서 이탈할 때 behavior 컴포넌트가 호출 — 이름 조회 + 사유별 문구 조립 후 브로드캐스트.
    // DisplayDuration = 손댈 수 있는 구간 길이(호출자가 DA 에서 계산) — 해제 신호를 놓쳐도 알림이 스스로 만료되는 폴백.
    void NotifyEmployeeDown(int32 EmployeeID, EWorkerDownReason Reason, float DisplayDuration);

    // 업무 복귀 — 떠 있던 알림 조기 해제
    void NotifyEmployeeRecovered(int32 EmployeeID);

    // ========== 레벨업 / 스탯 투자 ==========

    // 레벨업 이벤트 (EmployeeID, NewLevel)
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEmployeeLevelUp, int32 /*EmployeeID*/, int32 /*NewLevel*/);
    FOnEmployeeLevelUp OnEmployeeLevelUp;

    // 스탯 변경 이벤트 (EmployeeID) — 투자/자동배분/리셋 후 UI 갱신용
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnEmployeeStatsChanged, int32 /*EmployeeID*/);
    FOnEmployeeStatsChanged OnEmployeeStatsChanged;

    // 직능 포인트 변경 이벤트 (EmployeeID) — 투자/리셋 후 카드·부서배지 갱신용
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnEmployeeDisciplineChanged, int32 /*EmployeeID*/);
    FOnEmployeeDisciplineChanged OnEmployeeDisciplineChanged;

    // 레벨업당 지급 스킬포인트(직능 투자용). 초기 롤을 낮춘 대가로 성장분을 키운 값(2026-08-13)
    static constexpr int32 SkillPointsPerLevel = 3;

    // 스킬포인트 1점을 지정 직능에 투자 → 부서 재파생 → 저장 → Broadcast. SP 없으면 false·무변경.
    UFUNCTION(BlueprintCallable, Category = "Employee Discipline")
    bool InvestDisciplinePoint(int32 EmployeeID, EProductionDiscipline Discipline);

    // 다이아 차감 후 투자분을 이너트로 되돌리고 SP 전액 반환 → 부서 재파생 → 저장 → Broadcast.
    // 다이아 부족/투자분 0/직원 없음 = false(차감 없음).
    UFUNCTION(BlueprintCallable, Category = "Employee Discipline")
    bool ResetDisciplinePoints(int32 EmployeeID);

    static constexpr int32 DisciplineResetDiamondCost = 50;

    // 잠재 큐브 리롤 — 큐브 아이템 소모(CubeType 1개) + 등급상한 클램프(GetCubeCeiling) + ResetPotentialAbility + 저장 + OnEmployeeStatsChanged.
    // CubeType이 큐브 3종이 아니거나/재고 부족/직원 없음 = false (소모 없음)
    UFUNCTION(BlueprintCallable, Category = "Employee Potential")
    bool RerollEmployeePotential(int32 EmployeeID, EItemType CubeType);

private:
    // 레벨업 시각/UI 효과 (워커 위치 Niagara + 알림 문구)
    void PlayLevelUpEffects(int32 EmployeeID, int32 NewLevel);

    // 레벨업 이펙트 Niagara (워커 위치 스폰)
    TSoftObjectPtr<UNiagaraSystem> LevelUpVFX = TSoftObjectPtr<UNiagaraSystem>(
        FSoftObjectPath(TEXT("/Game/Level_UP_VFX/Niagara/NS_Level_Up_2.NS_Level_Up_2")));

public:
    // ===== 강화 (Money 스타포스: 성공/유지/하락 — 파괴 없음) =====

    enum class EEnhanceResult : uint8 { Success, Maintain, Downgrade };

    // 강화 결과 이벤트 (EmployeeID, 결과) — 스타포스 모달 연출용
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEmployeeEnhanced, int32 /*EmployeeID*/, EEnhanceResult);
    FOnEmployeeEnhanced OnEmployeeEnhanced;

    // Money 차감 → 확률 롤 → 성공+1 / ★0~5 유지 / ★6+ 하락. 파괴 없음.
    // false = 직원 없음 / ★12 도달 / Money 부족 (차감 없음) / 실패 롤.
    UFUNCTION(BlueprintCallable)
    bool EnhanceEmployee(int32 TargetEmployeeID);

    // ★E → E+1 시도 Money 비용 (마일스톤형 지수 곡선, 튜닝 노브)
    UFUNCTION(BlueprintPure)
    static int64 GetEnhanceCost(int32 EnhancementLevel);

    // 성공/유지/하락 3구간 확률 (UI 공개용). 합 = 1.
    void GetEnhanceOdds(int32 EnhancementLevel, float& OutSuccess, float& OutMaintain, float& OutDowngrade) const;

    UFUNCTION(BlueprintCallable)
    int32 GetEmployeeEnhancementLevel(int32 EmployeeID);

    static constexpr int32 MaxEnhancementLevel = 15;

private:
    float GetBaseEnhanceChance(int32 CurrentLevel);

    // 2026-07-30: 1000 → 2500. ★가 후반의 유일한 실질 성장 싱크인데 구 값은 ★0→10 을 12인 기준
    // 3.9M(전 게임 순익의 1% 미만)에 사버려서 T6 에 SF 2.00 을 다 채우고 T7~T10 4개 티어 동안 축이 죽었다.
    // 2500 이면 첫 강화가 첫 프로젝트 직후 잔액으로 약 1.9분에 도달해 온보딩은 유지된다.
    // 2026-08-08: 2500 → 50000(×20) 화폐 리디노미. 위 절대값(3.9M 등)은 리디노미 이전 기준이라 그대로 비교 금지.
    static constexpr int64 EnhanceCostBase = 50000;     // ★0→1 비용
    // 1.7 → 1.45 (2026-07-29). 단 **공비는 실질 노브가 아니다** — base 를 역보정해 ★10 비용을 고정하면
    // g=1.35~1.60 에서 곡선이 사실상 같다. ★6+ 하락 랜덤워크가 후반을 지배하기 때문
    // (★14→15 는 시도비 76만인데 기대비 1.56억 = ×204 증폭). 실질 노브는 위 base 다.
    static constexpr float EnhanceCostGrowth = 1.45f;

protected:
    UPROPERTY()
    TArray<FEmployeeInstance> EmployeeList;

    UPROPERTY()
    int32 NextInstanceID = 1;

private:
    FEmployeeInstance CreateEmployeeInstance(EEmployeeDepartment Department);

    // 선택한 EmpolyeeID
    UPROPERTY()
    int32 SelectedEmployeeID = -1;  // -1은 선택 없음

public:
    // 등급별 이름 생성 — 통짜 행이 있으면 그대로(황금손), 없으면 "성 + 핸들"(김졸림)
    FString GenerateRandomName(ELootBoxRarity Rarity);

private:
    void EnsureDataTablesLoaded();
    EEmployeeGender GenerateRandomGender();
    // 등급 전용 통짜 행이 있으면 그 풀, 없으면 별명 핸들 공통 풀에서 1행 추첨
    const FEmployeeHandleData* PickNameRow(const FString& Language, ELootBoxRarity Rarity);
    // 핸들과 글자가 겹치지 않는 성을 뽑는다
    FString PickSurname(const FString& Language, const FString& Handle);

    UDataTable* LastNameDataTable = nullptr;
    UDataTable* FirstNameDataTable = nullptr;

public:
    // 외모 생성 함수들
    UFUNCTION(BlueprintCallable)
    FCharacterAppearance GenerateRandomAppearance(EEmployeeRank Rank, EEmployeeGender Gender);

    FCharacterAppearance GetEmployeeAppearance(int32 EmployeeID);

private:
    // 얼굴 표정 랜덤 생성
    FMorphTargetSet GenerateRandomFacialExpression();

    // 외모 데이터 저장
    UPROPERTY()
    TMap<int32, FEmployeeAppearanceData> EmployeeAppearances;

    // TableManager mutable로 const성을 유지하면서 물리적으로는 변경 가능
    UPROPERTY()
    mutable UTableManagerSubsystem* TableManager = nullptr;

    // 캐시된 TableManager 가져오기
    UTableManagerSubsystem* GetTableManager() const;

    // Upgrade맵에서 외형 바꿔주는 부분
public:
    UFUNCTION(BlueprintCallable)
    void UpdateOfficeworkerAppearance(int32 EmployeeID);

//디버깅용
public:

    // 콘솔 명령어 함수들
    UFUNCTION(Exec)
    void TestClothingRank(int32 RankValue);

    UFUNCTION(Exec)
    void ListEmployees();

    // 경험치 테스트 명령어: TestAddExp [EmployeeID] [Amount]
    // EmployeeID 생략 시 선택된 직원에게 적용
    UFUNCTION(Exec)
    void TestAddExp(int32 Amount, int32 EmployeeID = -1);

    // 현재 빌딩 직원들에게 경험치 분배: TestDistributeExp [Amount] [QualityGrade(0-5)]
    UFUNCTION(Exec)
    void TestDistributeExp(int32 Amount, int32 QualityGradeValue = 3);

    // 직원들의 경험치/레벨 상태 표시
    UFUNCTION(Exec)
    void ShowExpStatus();
};
