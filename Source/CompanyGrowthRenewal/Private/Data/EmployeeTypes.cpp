#include "Data/EmployeeTypes.h"

EEmployeeRank UEmployeeTypeHelper::GetRankFromEnhancementLevel(int32 EnhancementLevel)
{
    // 1:1 매칭: EnhancementLevel = 직급
    switch (EnhancementLevel)
    {
    case 0:  return EEmployeeRank::Intern;            // +0  = 인턴
    case 1:  return EEmployeeRank::Assistant;         // +1  = 사원
    case 2:  return EEmployeeRank::Associate;         // +2  = 주임
    case 3:  return EEmployeeRank::SeniorAssociate;   // +3  = 대리
    case 4:  return EEmployeeRank::Manager;           // +4  = 과장
    case 5:  return EEmployeeRank::SeniorManager;     // +5  = 차장
    case 6:  return EEmployeeRank::Director;          // +6  = 부장
    case 7:  return EEmployeeRank::ManagingDirector;  // +7  = 이사
    case 8:  return EEmployeeRank::ExecutiveDirector; // +8  = 상무
    case 9:  return EEmployeeRank::VP;                // +9  = 전무
    case 10: return EEmployeeRank::VicePresident;     // +10 = 부사장
    case 11: return EEmployeeRank::President;         // +11 = 사장
    default: return EEmployeeRank::Chairman;          // +12 = 회장 (최고)
    }
}

FString UEmployeeTypeHelper::MakeStarString(int32 EnhancementLevel)
{
    // 상한 = UEmployeeManager::MaxEnhancementLevel(15) — 순환 include 회피로 값 미러 (변경 시 동기)
    // 15강 확장 후 글리프 나열은 정보 표시에서 노이즈 — "★ N" 카운트 표기 (풀 별판은 강화 모달 전용)
    const int32 Count = FMath::Clamp(EnhancementLevel, 0, 15);
    return Count > 0 ? FString::Printf(TEXT("★ %d"), Count) : FString();
}

namespace
{
    // 밸런스 튜닝 상수 (Live Coding 대상) — 앵커: 무강화 Lv1 신입 2명(튜토 M5 ×2 채용) = 티어1 달성률 ~1.0(C등급)
    // 2026-07-29: 0.65 → 5.2(×8). 구 값은 요구치(ΣTarget 128)의 1/8이라 T1 달성률이 0.12였다.
    // ⚠ 화폐 스케일 노브가 아니다 — 2026-08-13 방치 수익 폐지 이후 이 값이 흐르는 곳은 개발 점수뿐이다
    //    (EmployeeBehaviorComponent 의 BaseScore / StageProgressData 의 추정 PerSec).
    //    여기를 곱하면 요구치(DT_Project_*.RequiredScore_Step*)는 그대로라 달성률까지 같이 뛴다.
    //    화폐 스케일은 운영 수익 축(ProjectOperationManager)에서 조정할 것.
    constexpr float OutputUnit = 5.2f;    // Lv1 무강화 기준 출력 단위
}

// 레벨은 산출에 직접 곱하지 않는다 `[확정 2026-08-13]` — 레벨업이 이미 SkillPointsPerLevel 로
// 직능 포인트를 주고, 그 포인트가 DisciplineAffinity 로 산출을 올린다. 여기에 LevelFactor 를 두면
// 같은 레벨업이 두 번 계상된다. 성장 축은 직능 SP / 스탯 / ★ 셋이 전담.
float UEmployeeTypeHelper::CalculateBaseOutput(int32 /*Level*/)
{
    return OutputUnit;
}
