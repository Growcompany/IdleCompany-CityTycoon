#pragma once

#include "Data/EmployeeTypes.h"

namespace EmployeeTrainingRules
{
    // 비용의 int32 표현 범위를 넘지 않도록 제한한다.
    constexpr int32 MaxLevel = MAX_int32 / 150;
    constexpr int32 WorkPowerPerLevel = 3;
    constexpr int32 StatPointsPerLevel = 2;
    // 능력치 Lv 이 이만큼 오를 때마다 다음 투자 비용이 SP 1 늘어난다 (Lv.0~9 = 1, Lv.10~19 = 2 …)
    constexpr int32 StatLevelsPerCostStep = 10;

    inline int32 CostForLevel(int32 Level)
    {
        return Level >= 1 && Level < MaxLevel ? Level * 150 : 0;
    }

    inline int32 StatPointCost(int32 StatLevel)
    {
        return 1 + FMath::Max(0, StatLevel) / StatLevelsPerCostStep;
    }

    inline int32 GetStatLevel(const FEmployeeInstance& Employee, EEmployeeStatIndex Stat)
    {
        const int32 Index = static_cast<int32>(Stat);
        return Employee.StatLevels.IsValidIndex(Index) ? FMath::Max(0, Employee.StatLevels[Index]) : 0;
    }

    inline bool CanLevelUp(const FEmployeeInstance& Employee)
    {
        const int32 Cost = CostForLevel(Employee.Level);
        const int32 Primary = GetPrimaryDisciplineSlot(Employee);
        return Cost > 0 && FMath::IsFinite(Employee.Experience) && Employee.Experience >= Cost
            && (Employee.DisciplinePoints.Num() == 3
                || Employee.DisciplinePoints.Num() == static_cast<int32>(EProductionDiscipline::Count))
            && Employee.DisciplinePoints.IsValidIndex(Primary)
            && Employee.DisciplinePoints[Primary] <= MAX_int32 - WorkPowerPerLevel
            && Employee.UnspentStatPoints >= 0 && Employee.UnspentStatPoints <= MAX_int32 - StatPointsPerLevel;
    }

    inline bool TryLevelUp(FEmployeeInstance& Employee)
    {
        if (!CanLevelUp(Employee)) { return false; }
        Employee.Experience -= CostForLevel(Employee.Level);
        ++Employee.Level;
        Employee.DisciplinePoints[GetPrimaryDisciplineSlot(Employee)] += WorkPowerPerLevel;
        Employee.UnspentStatPoints += StatPointsPerLevel;
        SyncDerivedDepartment(Employee);
        return true;
    }

    // 저장 중 새로 적립된 업무 경험치는 성장 결제 실패로 되돌리지 않는다.
    inline void RollbackPreservingEarnedExperience(FEmployeeInstance& Employee,
        const FEmployeeInstance& Previous, double ExperienceAfterMutation)
    {
        const double EarnedDuringSave = Employee.Experience - ExperienceAfterMutation;
        Employee = Previous;
        if (FMath::IsFinite(EarnedDuringSave) && EarnedDuringSave > 0.0
            && FMath::IsFinite(Previous.Experience + EarnedDuringSave))
        {
            Employee.Experience = Previous.Experience + EarnedDuringSave;
        }
    }

    inline bool CanAllocateStatPoint(const FEmployeeInstance& Employee, EEmployeeStatIndex Stat)
    {
        int32 Value = 0;
        switch (Stat)
        {
        case EEmployeeStatIndex::WorkSpeed:  Value = Employee.Stats.WorkSpeed; break;
        case EEmployeeStatIndex::CritChance: Value = Employee.Stats.CritChance; break;
        case EEmployeeStatIndex::Composure:  Value = Employee.Stats.Composure; break;
        case EEmployeeStatIndex::ExpGain:    Value = Employee.Stats.ExpGain; break;
        case EEmployeeStatIndex::Stamina:    Value = Employee.Stats.Stamina; break;
        case EEmployeeStatIndex::CritDamage: Value = Employee.Stats.CritDamage; break;
        default: return false;
        }
        const int32 StatLevel = GetStatLevel(Employee, Stat);
        if (StatLevel >= MAX_int32 || Employee.UnspentStatPoints < StatPointCost(StatLevel)) { return false; }
        const int32 EnhanceBonus = UEmployeeTypeHelper::GetEnhanceStatBonus(Employee.EnhancementLevel);
        return Value < MAX_int32 - EnhanceBonus;
    }

    // 어느 능력치든 한 번은 투자할 수 있는가 (SP 가 가장 싼 비용 이상)
    inline bool CanAllocateAnyStatPoint(const FEmployeeInstance& Employee)
    {
        for (int32 Index = 0; Index < static_cast<int32>(EEmployeeStatIndex::Count); ++Index)
        {
            if (CanAllocateStatPoint(Employee, static_cast<EEmployeeStatIndex>(Index))) { return true; }
        }
        return false;
    }

    inline bool TryAllocateStatPoint(FEmployeeInstance& Employee, EEmployeeStatIndex Stat)
    {
        if (!CanAllocateStatPoint(Employee, Stat)) { return false; }
        const int32 StatLevel = GetStatLevel(Employee, Stat);
        switch (Stat)
        {
        case EEmployeeStatIndex::WorkSpeed:  ++Employee.Stats.WorkSpeed; break;
        case EEmployeeStatIndex::CritChance: ++Employee.Stats.CritChance; break;
        case EEmployeeStatIndex::Composure:  ++Employee.Stats.Composure; break;
        case EEmployeeStatIndex::ExpGain:    ++Employee.Stats.ExpGain; break;
        case EEmployeeStatIndex::Stamina:    ++Employee.Stats.Stamina; break;
        case EEmployeeStatIndex::CritDamage: ++Employee.Stats.CritDamage; break;
        default: return false;
        }
        if (Employee.StatLevels.Num() < static_cast<int32>(EEmployeeStatIndex::Count))
        {
            Employee.StatLevels.SetNumZeroed(static_cast<int32>(EEmployeeStatIndex::Count));
        }
        Employee.StatLevels[static_cast<int32>(Stat)] = StatLevel + 1;
        Employee.UnspentStatPoints -= StatPointCost(StatLevel);
        return true;
    }
}
