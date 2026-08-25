// 방치(Idle) 수익/초 + 오프라인 정산 닫힌식 — EmployeeBehaviorComponent.cpp(GenerateIncome/GetIncomeMultiplier)
// + SaveLoadManager.cpp(CalculateOfflineGainsInternal) 이식.

// CalculateBaseOutput(EmployeeTypes.cpp:41) 이식: LevelFactor = 1 + max(0,Lv-1)×LevelGrowth.
function calculateBaseOutput(values, level) {
  const outputUnit = values.get('emp.outputUnit');
  const levelGrowth = values.get('emp.levelFactorCurve');
  const levelFactor = 1.0 + Math.max(0, level - 1) * levelGrowth;
  return outputUnit * levelFactor;
}

// GenerateIncome(:824-869) 이식: BaseIncomePerSecond(cdo) × WanderMult × StatMultiplier.
// StatMultiplier = max(CalculateBaseOutput×IdleIncomeScale × PotentialIncomeMult, 1.0) — 원본은 잠재능력
// AggregateModifiers(...).IncomeMult도 곱하지만, 이 인터페이스(employee={level,star})엔 잠재능력이 없어
// 베이스라인 1.0으로 둔다(값이 있으면 이 함수를 통해서만 곱해지므로 나중에 확장 가능).
// ★(star, EnhancementLevel)는 여기서 의도적으로 미사용 — GenerateIncome이 EnhancementLevel을 전혀 참조하지
// 않아(강화는 개발 스탯/전투력에만 반영) 실코드상 방치수익에 기여하지 않는다(retune §7-12).
function idlePerSecond(values, employee) {
  const baseIncomePerSecond = values.get('idle.baseIncomePerSecond');
  const wanderMult = values.get('emp.wanderIncomeMult');
  const idleIncomeScale = values.get('emp.idleIncomeScale');

  const baseOutput = calculateBaseOutput(values, employee.level);
  const statMultiplier = Math.max(baseOutput * idleIncomeScale, 1.0);
  return baseIncomePerSecond * wanderMult * statMultiplier;
}

// 오프라인 정산률 — 온라인과 동률(SaveLoadManager.cpp: RawIncome = OnlineRatePerSec × EffectiveSeconds × Trait).
// 구 GetIdleIncomeRate(강화 레벨 연동 로그 곡선)는 삭제됐다. 이제 강화 노브가 아니라 상수다.
const OFFLINE_SETTLE_RATE = 1.0;

// CalculateOfflineGainsInternal(SaveLoadManager.cpp) 이식.
// opState = {actualRatePerSec, remainingSeconds, vaultCap, stored} — 건물 1개의 운영 상태.
// 12h 캡(PlayFabManagerSubsystem.cpp:14 OfflineCapSeconds)은 원본에선 호출측에서 미리 클램프하지만,
// 이 함수 하나로 오프라인 규칙을 완결시키기 위해 여기서 직접 적용한다(브리핑 지시).
function offlineGains(values, opState, offlineSeconds, traitFactor) {
  // 60초 미만은 네트워크/재접속 오차로 간주, 무시 (SaveLoadManager.cpp:124)
  const minSeconds = values.get('offline.minSeconds');
  if (offlineSeconds < minSeconds) {
    return { gain: 0, raw: 0, opElapsedAdvance: 0, clippedByVault: false };
  }

  const capSeconds = values.get('offline.capSeconds');
  const cappedOfflineSeconds = Math.min(offlineSeconds, capSeconds);

  // 운영 남은 시간을 초과한 오프라인 수익은 발생 불가
  const effectiveSeconds = Math.min(cappedOfflineSeconds, opState.remainingSeconds);
  const raw = opState.actualRatePerSec * OFFLINE_SETTLE_RATE * effectiveSeconds * traitFactor;

  const availableSpace = Math.max(0, opState.vaultCap - opState.stored);
  const gain = Math.min(raw, availableSpace);
  const clippedByVault = raw > availableSpace;

  // 운영 시간 진행 — 수익 정산률과 분리된 별도 비율(OfflineTimeProgressRate), 잔여시간을 넘어설 수 없음
  // (원본은 ElapsedTime+Progress를 TotalOperationTime으로 클램프 = Progress를 RemainingTime으로 클램프하는 것과 동치).
  const progressRate = values.get('offline.progressRate');
  const opElapsedAdvance = Math.min(cappedOfflineSeconds * progressRate, opState.remainingSeconds);

  // raw = 금고 클리핑 **전** 발생액. 호출자가 (raw − gain) 으로 "금고에 못 담아 날린 돈"을 집계할 수 있어야
  // 한다 — gain 만 돌려주면 이미 잘린 값이라 손실이 구조적으로 0으로 보인다(Task10 리뷰 F2).
  return { gain, raw, opElapsedAdvance, clippedByVault };
}

module.exports = { idlePerSecond, offlineGains, calculateBaseOutput, OFFLINE_SETTLE_RATE };
