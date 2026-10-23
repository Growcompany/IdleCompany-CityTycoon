#pragma once

#include "CoreMinimal.h"
#include "Data/EmployeeTypes.h"
#include "Enum/ProductionDiscipline.h"
#include "Table/MissionTable.h"
#include "Table/SideDungeonLootTable.h"

/**
 * 사이드 컨텐츠 4모듈이 공유하는 순수 함수 모음 (설계 §1.1).
 * UObject 가 아니라 네임스페이스 — 상태가 없어야 테스트가 싸다.
 */
namespace FSideContentRules
{
	static constexpr int32 DisciplineCount = static_cast<int32>(EProductionDiscipline::Count);

	/** ★ 1개당 파워 가산 (설계 §6). */
	static constexpr float StarBonusPerLevel = 0.03f;

	/** DisciplinePoints 는 비어 있을 수 있다(= 전부 0). 인덱스 가드가 유일한 안전망. */
	COMPANYGROWTHRENEWAL_API int32 GetDisciplinePoints(const FEmployeeInstance& Employee, EProductionDiscipline Discipline);

	/**
	 * 이 시각에 출장 중인가 — **술어일 뿐 호출자 동작을 보장하지 않는다.**
	 * 거르는 것은 부르는 쪽이다: 기여 계열은 UEmployeeManager::GetActiveEmployeesInBuilding,
	 * 공통 축 자동 편성은 AutoPickTeam. ComputeTeamPower 는 받은 TeamIDs 를 그대로 쓴다.
	 */
	COMPANYGROWTHRENEWAL_API bool IsBusy(const FEmployeeInstance& Employee, int64 NowUnix);

	/** P[d] = Σ CalculateBaseOutput(Level) × DisciplineAffinity(Points[d]) × (1 + 0.03 × ★) */
	COMPANYGROWTHRENEWAL_API void ComputeTeamPower(const TArray<FEmployeeInstance>& Roster, const TArray<int32>& TeamIDs, TArray<float>& OutPower);

	/** 달성률 R = Σ W[d] × min(P[d]/Required[d], 1) / Σ W. 성공 = R >= 1.0 */
	COMPANYGROWTHRENEWAL_API float Evaluate(const TArray<float>& Power, const TArray<float>& Required, const TArray<float>& Weight);

	/** 공통 축 자동 편성 — 가중 직능 파워 상위 N명. */
	COMPANYGROWTHRENEWAL_API TArray<int32> AutoPickTeam(const TArray<FEmployeeInstance>& Roster, const TArray<float>& Weight, int32 Count, int64 NowUnix);

	/** 고른 직원들의 해당 직능 점수 합 — 출장 판정의 유일한 수치. */
	COMPANYGROWTHRENEWAL_API int32 SumDisciplinePoints(const TArray<FEmployeeInstance>& Roster, const TArray<int32>& EmployeeIDs, EProductionDiscipline Discipline);

	/**
	 * 출장 자동 선발 — 벤치(미배정) 우선, 그 안에서 직능 내림차순. 모자라면 그 건물 배치 팀에서 채운다.
	 * 플레이어가 직접 고를 수 있으므로 이건 [자동 선발] 버튼의 기본값일 뿐이다(사용자 확정 2026-09-24).
	 */
	COMPANYGROWTHRENEWAL_API TArray<int32> AutoPickDispatchCrew(const TArray<FEmployeeInstance>& Roster, int32 BuildingIndex, EProductionDiscipline Discipline, int32 Count, int64 NowUnix);

	/** 선발 가능한 직원 수 (벤치 + 그 건물 배치 팀, 출장 중 제외). */
	COMPANYGROWTHRENEWAL_API int32 CountAvailableCrew(const TArray<FEmployeeInstance>& Roster, int32 BuildingIndex, int64 NowUnix);

	/** 자정 리필. 상한을 넘기지 않는다. */
	COMPANYGROWTHRENEWAL_API int32 RefillEntries(int32 Current, int32 Daily, int32 Cap);

	/** 밴드에 걸리는 행을 가중 추첨해 Rolls 회. 같은 Rng 시드면 같은 결과 — 테스트 가능. */
	COMPANYGROWTHRENEWAL_API TArray<FMissionReward> RollLoot(const TArray<FSideDungeonLootRow>& Rows, int32 Stage, int32 Rolls, int32 HQLevel, FRandomStream& Rng, int32 CompanyLevel = 0);
}
