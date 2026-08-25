// ★강화(직원) 마르코프 기대비용 + 빌딩 강화 축 + HQ 레벨 비용 — 반복 소모형 싱크 2대장 + HQ.
// ★강화 이식 원본: Tools/Balance/enhance_curve.js (git 추적, 검증된 구현) — 하락 재등반 포함 점화식을 값 파라미터화해 그대로 옮긴다.
// 대조 실코드: EmployeeManager.cpp GetBaseEnhanceChance(:569)/GetEnhanceCost(:586)/GetEnhanceOdds(:592)/EnhanceEmployee(:601).

// ★ 구조 상수 — MaxEnhancementLevel(EmployeeManager.h:309)/하락 임계값(EmployeeManager.cpp:597)은
// 밸런스 노브가 아니라 시스템 규칙(값 자체가 DT/values에 없음 — extract_src.js가 명시적으로 뽑은 이름값 없음).
// enhance_curve.js도 동일하게 MAX=15, 임계값=6을 리터럴로 둔다.
const MAX_STAR = 15;           // EmployeeManager.h:309 MaxEnhancementLevel
const DOWNGRADE_THRESHOLD = 6; // EmployeeManager.cpp:597 — ★0~5 실패=유지 / ★6+ 실패=하락(파괴 없음)

function starChance(values, star) {
  // GetBaseEnhanceChance(:569-578): CurrentLevel>=Max → 0(강화 자체 불가), 그 외 선형 감쇠.
  if (star >= MAX_STAR) return 0;
  const base = values.get('enhance.chanceBase');
  const step = values.get('enhance.chanceStep');
  const min = values.get('enhance.chanceMin');
  return Math.max(min, base - step * star);
}

function starCost(values, star) {
  // GetEnhanceCost(:586-590): base × growth^E. int64 캐스트(정수 절삭)는 이 밸런스 모델에서는
  // 재현하지 않는다 — 다른 sim/*.js(operation.js DevelopmentCost 등)와 동일하게 연속값으로 취급.
  const base = values.get('emp.enhanceCostBase');
  const growth = values.get('emp.enhanceCostGrowth');
  return base * Math.pow(growth, star);
}

// EnhanceEmployee(:601-663) 이식: Money 선지불(성공/실패 무관, 비용은 항상 cost(star)) → 롤.
// 성공: star+1. 실패: star>=6 이면 star-1(하락), 아니면 유지.
// ★MaxEnhancementLevel(15) 도달 시 :609-616 에서 지불 전 조기 return(시도 자체 불가) — cost 0, 변화 없음으로 재현.
function starAttempt(rng, values, star) {
  if (star >= MAX_STAR) return { newStar: star, cost: 0 };
  const cost = starCost(values, star);
  const p = starChance(values, star);
  const roll = rng.next();
  let newStar = star;
  if (roll <= p) {
    newStar = star + 1;
  } else if (star >= DOWNGRADE_THRESHOLD) {
    newStar = star - 1;
  }
  return { newStar, cost };
}

// ★E→E+1 기대비용(하락 재등반 포함) 점화식 — enhance_curve.js expectedTotal() 그대로:
//   E<6  : h[E] = cost(E)/p(E)                                    (실패=유지, 기하분포)
//   E>=6 : h[E] = cost(E) + (1-p(E))×(h[E-1]+h[E]) 를 h[E]에 대해 풀면
//          h[E] = cost(E)/p(E) + (1-p(E))/p(E) × h[E-1]           (실패→E-1로 하락→재등반(h[E-1])→재시도(h[E]))
// from/to 구간 합만 필요해도 0부터 전부 계산 — E>=6 항이 h[E-1]을 참조하므로 부분 구간만 뜯어낼 수 없다.
function starExpectedCost(values, from, to) {
  if (to > MAX_STAR) throw new Error(`starExpectedCost: to(${to})가 MaxEnhancementLevel(${MAX_STAR}) 초과`);
  const h = new Array(MAX_STAR).fill(0);
  for (let e = 0; e < MAX_STAR; e++) {
    const pe = starChance(values, e);
    const c = starCost(values, e);
    h[e] = e < DOWNGRADE_THRESHOLD ? c / pe : c / pe + ((1 - pe) / pe) * h[e - 1];
  }
  const cum = new Array(MAX_STAR + 1).fill(0);
  for (let e = 0; e < MAX_STAR; e++) cum[e + 1] = cum[e] + h[e];
  return cum[to] - cum[from];
}

// DT_BuildingEnhancementDefinition 행 조회 — EnhancementType(=Name)으로 강화 축 하나를 찾는다.
function findBuildingRow(values, axisName) {
  const row = values.dt('DT_BuildingEnhancementDefinition').find(
    r => r.EnhancementType === axisName || r.Name === axisName
  );
  if (!row) throw new Error(`DT_BuildingEnhancementDefinition 누락 행: ${axisName}`);
  return row;
}

// CalculateUpgradeCost(BuildingEnhancementData.cpp:114-121) 이식: cost = BaseCost × CostGrowthRate^lv.
// lv = 현재 레벨(이 레벨→lv+1 시도 비용). currency = CostResourceType(Money/Brick 등, 축마다 다름).
function buildingUpgradeCost(values, axisName, lv) {
  const row = findBuildingRow(values, axisName);
  const amount = row.BaseCost * Math.pow(row.CostGrowthRate, lv);
  return { amount, currency: row.CostResourceType };
}

// CalculateEffectMultiplier(BuildingEnhancementData.cpp) 이식 — 전 축 가산형, 예외 분기 없음.
function buildingEffect(values, axisName, lv) {
  if (lv <= 0) return 1.0;
  const row = findBuildingRow(values, axisName);
  return 1.0 + lv * row.EffectPerLevel;
}

// HQManagePanelWidget.cpp:249-251 이식: NextLevel(=lv) 행의 MoneyCost가 CurrentLevel→lv 비용.
function hqLevelCost(values, lv) {
  const rows = values.dt('DT_HQLevel');
  const row = rows.find(r => String(r.Name) === String(lv));
  if (!row) throw new Error(`DT_HQLevel 누락 행: ${lv}`);
  return row.MoneyCost;
}

module.exports = { starAttempt, starExpectedCost, buildingUpgradeCost, buildingEffect, hqLevelCost };
