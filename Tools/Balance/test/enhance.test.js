const { test } = require('node:test');
const assert = require('node:assert');
const { starExpectedCost, starAttempt, buildingUpgradeCost, buildingEffect, hqLevelCost } = require('../sim/enhance.js');
const { makeRng } = require('../sim/rng.js');
const { fakeValues } = require('./helpers/fake_values.js');

// ⚠ 브리핑 Step1 원문의 "★0→10 기대비용 ≈ 812,404"는 재현되지 않는다 — 끝까지 추적한 결과:
// specs/2026-07-29-balance-grind-retune.md §표8행 "2,500 (★0→10 기대 812,404/인)"의 근거 계산은
// 밸런스 재도출 세션의 임시 스크립트(scratchpad/final2.py `enh_expect()`, 세션 종료 후 소실 가능)에서
// 나왔는데, 그 함수가 `if E<5: w=c/p`로 하락 임계값을 5로 오프바이원 했다 — 실코드
// (EmployeeManager.cpp:597 `EnhancementLevel >= 6`)도, 이식 원본 enhance_curve.js(무수정, `if (E<6)`)도,
// 이 브리핑 자신의 규칙 서술("★0~5 유지, ★6+ 하락")도 전부 임계값 6이라 812,404 쪽이 브리핑 자기 자신과도
// 모순된다. 3중 교차검증: (1) enhance_curve.js를 base만 1000→2500으로 바꿔 무수정 실행 → 789,945
// (2) 상태 0~9 흡수 마르코프 체인을 점화식과 별개로 value-iteration 직접 풀이 → 789,945.18
// (3) 게임 규칙 그대로(임계값 6) 5000런 몬테카를로 → 아래 테스트에서 이 값과 3% 이내 합치.
// "812,404가 안 나오면 마르코프 점화식을 의심하라"는 브리핑 지시를 따른 결과이지, 값을 임의로 낮춘 게 아니다
// — 자세한 추적 과정은 task-8-report.md 참고.
test('★0→10 기대비용 ≈ 789,945 (base 2500, 임계값=6 그대로 — EmployeeManager.cpp:597/enhance_curve.js 대조 완료)', () => {
  const c = starExpectedCost(fakeValues(), 0, 10);
  assert.ok(Math.abs(c - 789945) / 789945 < 0.001, `got ${c}`);
});

test('몬테카를로 ≈ 마르코프 (±3%, 5000런)', () => {
  const V = fakeValues();
  let sum = 0; const N = 5000;
  for (let s = 0; s < N; s++) {
    const rng = makeRng(s); let star = 0, spent = 0;
    while (star < 10) { const r = starAttempt(rng, V, star); star = r.newStar; spent += r.cost; }
    sum += spent;
  }
  const mc = sum / N, mk = starExpectedCost(V, 0, 10);
  assert.ok(Math.abs(mc - mk) / mk < 0.03, `MC ${mc} vs Markov ${mk}`);
});

// 추가 커버리지 — 브리핑이 명시한 "★0~5 유지 / ★6+ 하락" 분기, MaxEnhancementLevel 가드, 구간 부분합.
test('starExpectedCost: ★0→1 = cost(0)/p(0) (단일 전이, 재귀항 없음)', () => {
  const V = fakeValues();
  // cost(0)=2500, p(0)=0.95 → 2500/0.95
  assert.ok(Math.abs(starExpectedCost(V, 0, 1) - 2500 / 0.95) < 0.01);
});

test('starExpectedCost: ★5→6 은 아직 유지 구간(E<6) — 재귀항 없이 cost/p', () => {
  const V = fakeValues();
  const c5 = 2500 * Math.pow(1.45, 5);
  const p5 = 0.95 - 0.055 * 5;
  assert.ok(Math.abs(starExpectedCost(V, 5, 6) - c5 / p5) / (c5 / p5) < 1e-9);
});

test('starAttempt: ★0~5 구간은 실패해도 유지(하락 없음)', () => {
  const V = fakeValues();
  const alwaysFail = { next: () => 0.999 };
  const r = starAttempt(alwaysFail, V, 3);
  assert.strictEqual(r.newStar, 3);
  assert.ok(r.cost > 0);
});

test('starAttempt: ★6+ 구간은 실패 시 하락(k-1), 파괴 없음', () => {
  const V = fakeValues();
  const alwaysFail = { next: () => 0.999 };
  const r = starAttempt(alwaysFail, V, 7);
  assert.strictEqual(r.newStar, 6);
});

test('starAttempt: 성공 시 항상 +1 (유지/하락 구간 무관)', () => {
  const V = fakeValues();
  const alwaysSucceed = { next: () => 0.0 };
  assert.strictEqual(starAttempt(alwaysSucceed, V, 3).newStar, 4);
  assert.strictEqual(starAttempt(alwaysSucceed, V, 9).newStar, 10);
});

test('starAttempt: ★15(MaxEnhancementLevel) 도달 시 시도 자체 불가 — cost 0, 변화 없음 (EnhanceEmployee:609-616)', () => {
  const V = fakeValues();
  const r = starAttempt({ next: () => 0.0 }, V, 15);
  assert.strictEqual(r.newStar, 15);
  assert.strictEqual(r.cost, 0);
});

test('buildingUpgradeCost: cost = BaseCost × CostGrowthRate^lv, currency는 DT CostResourceType', () => {
  const V = fakeValues();
  const r0 = buildingUpgradeCost(V, 'VaultCapacity', 0);
  assert.strictEqual(r0.amount, 800);
  assert.strictEqual(r0.currency, 'Money');
  const r5 = buildingUpgradeCost(V, 'BuildingFloor', 5);
  assert.ok(Math.abs(r5.amount - 1000 * Math.pow(1.15, 5)) < 0.01);
  assert.strictEqual(r5.currency, 'Brick'); // BuildingFloor는 Money가 아니라 Brick 소모 — 축마다 화폐가 다름
});

test('buildingUpgradeCost: 존재하지 않는 축은 loud fail(throw)', () => {
  const V = fakeValues();
  assert.throws(() => buildingUpgradeCost(V, 'NoSuchAxis', 1));
});

test('buildingEffect: 기본(가산형) = 1 + Lv×EffectPerLevel, Lv<=0은 1.0', () => {
  const V = fakeValues();
  assert.strictEqual(buildingEffect(V, 'VaultCapacity', 0), 1.0);
  assert.ok(Math.abs(buildingEffect(V, 'VaultCapacity', 100) - 1.0625) < 1e-9);
  assert.strictEqual(buildingEffect(V, 'BuildingFloor', 5), 6); // 1 + 5×1
});

test('buildingEffect: 예외 축 없음 — 모든 축이 같은 가산형', () => {
  const V = fakeValues();
  const lv = 1000;
  for (const axis of ['VaultCapacity', 'MarketCapMultiplier', 'BuildingFloor']) {
    const k = V.dt('DT_BuildingEnhancementDefinition').find(r => r.EnhancementType === axis).EffectPerLevel;
    assert.ok(Math.abs(buildingEffect(V, axis, lv) - (1 + lv * k)) < 1e-9, `${axis} 가 가산형이 아님`);
  }
});

test('hqLevelCost: DT_HQLevel[Name=lv].MoneyCost — Lv1은 0(시작), Lv2는 500', () => {
  const V = fakeValues();
  assert.strictEqual(hqLevelCost(V, 1), 0);
  assert.strictEqual(hqLevelCost(V, 2), 500);
  assert.strictEqual(hqLevelCost(V, 5), 6000);
});

test('hqLevelCost: 존재하지 않는 레벨은 loud fail(throw)', () => {
  const V = fakeValues();
  assert.throws(() => hqLevelCost(V, 999));
});
