const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { recoveryRatio, vaultCapacity, spreadMult, scoreMult, operationTotals } = require('../sim/operation.js');
const { fakeValues, writeFixture } = require('./helpers/fake_values.js');
const { loadValues } = require('../sim/values.js');

// fakeValues() 스냅샷을 떠서 DT_IndustryProfile.Game.Volatility 만 갈아끼운 변형 fixture — 조용한 근사 금지 검증용.
function fakeValuesWithVolatility(v) {
  const p = path.join(os.tmpdir(), `balance_fake_values_vol_${process.pid}_${Date.now()}_${Math.random().toString(36).slice(2)}.json`);
  writeFixture(p);
  const raw = JSON.parse(fs.readFileSync(p, 'utf8'));
  raw.dt.DT_IndustryProfile.find(r => r.Industry === 'Game').Volatility = v;
  fs.writeFileSync(p, JSON.stringify(raw));
  const V = loadValues(p);
  fs.unlinkSync(p);
  return V;
}

// retune §4: B궁합 기준 C 1.568 / B 3.570 / A 6.487 / S 10.751 (Game, Trend/Building/Stat 1.0)
const CASES = [['C', 0.6875, 1.568], ['B', 1.0625, 3.570], ['A', 1.4375, 6.487], ['S', 1.8125, 10.751]];
test('회수율 표 재현 (오차 1% 이내, idx 무관)', () => {
  const V = fakeValues();
  for (const [grade, q, expected] of CASES) {
    for (const idx of [10, 55]) {
      const r = recoveryRatio(V, idx, { grade, q, affinityGrade: 'B', trendMatched: false, buildingIncomeMult: 1, statBonus: 1, industry: 'Game' });
      assert.ok(Math.abs(r - expected) / expected < 0.01, `${grade}@idx${idx}: ${r} vs ${expected}`);
    }
  }
});

test('금고 용량: Lv0 = stableRate × 2h', () => {
  const V = fakeValues();
  assert.ok(Math.abs(vaultCapacity(V, 10, 0) - 10 * 7200) < 1);
});

test('금고 용량: 하한 = vault.baseCapacity (운영 없음/stableRate=0)', () => {
  const V = fakeValues();
  assert.strictEqual(vaultCapacity(V, 0, 0), V.get('vault.baseCapacity'));
});

test('금고 용량: 레벨 오를수록 VaultSeconds 증가 (점근 12h 방향)', () => {
  const V = fakeValues();
  const c0 = vaultCapacity(V, 10, 0);
  const c50 = vaultCapacity(V, 10, 50);
  const c500 = vaultCapacity(V, 10, 500);
  assert.ok(c50 > c0);
  assert.ok(c500 > c50);
  assert.ok(c500 < 10 * 3600 * 12 + 1); // 점근 상한 12h 못 넘음
});

test('spreadMult: 궁합 없음(affinityGrade falsy)은 1.0 (Grade.IsNone() 분기, leverage 값과 무관)', () => {
  const V = fakeValues();
  // Grade.IsNone() 분기는 leverage를 쓰지 않지만, 원본 함수 계약상 leverage 인자 자체는 항상 전달돼야 한다
  // (leverage 값이 뭐든 이 분기 결과에는 영향 없음을 서로 다른 값으로 재확인).
  assert.strictEqual(spreadMult(V, null, 1.25, 1.5), 1.0);
  assert.strictEqual(spreadMult(V, undefined, 1.9, 0.6), 1.0);
});

test('spreadMult: B궁합 q=0.6875, leverage=1.5(Game) -> 0.65125 (검산 힌트 그대로)', () => {
  const V = fakeValues();
  const s = spreadMult(V, 'B', 0.6875, 1.5);
  assert.ok(Math.abs(s - 0.65125) < 1e-6, `${s}`);
});

test('spreadMult: leverage 누락(undefined/null) 시 명시적 에러 — 조용한 산업 기본값 금지', () => {
  const V = fakeValues();
  assert.throws(() => spreadMult(V, 'B', 0.6875), /leverage/); // 인자 자체를 안 넘긴 경우
  assert.throws(() => spreadMult(V, 'B', 0.6875, undefined), /leverage/);
  assert.throws(() => spreadMult(V, 'B', 0.6875, null), /leverage/);
});

test('spreadMult: leverage 값에 따라 결과가 달라진다 (조용한 Game 기본값이 아님을 재현) — 리뷰 지적 시나리오', () => {
  const V = fakeValues();
  const gameLeverage = spreadMult(V, 'B', 0.6875, 1.5); // Game ReviewLeverage
  const itLeverage = spreadMult(V, 'B', 0.6875, 0.6);   // 다른 산업(예: IT) ReviewLeverage
  assert.ok(Math.abs(gameLeverage - 0.65125) < 1e-9, `${gameLeverage}`);
  assert.ok(Math.abs(itLeverage - 0.8605) < 1e-9, `${itLeverage}`); // band=0.7675 → 1+0.6×(0.7675-1)=0.8605
  assert.notStrictEqual(gameLeverage, itLeverage); // 30%+ 괴리 — 예전엔 둘 다 조용히 0.65125를 반환했음
});

test('scoreMult: q=0.6875 -> 0.8875, clamp 범위 [0.85,1.15] 존중', () => {
  const V = fakeValues();
  assert.ok(Math.abs(scoreMult(V, 0.6875) - 0.8875) < 1e-6);
  assert.strictEqual(scoreMult(V, 0.5), 0.85); // 하한 클램프
  assert.strictEqual(scoreMult(V, 2.0), 1.15); // 상한 클램프
});

test('operationTotals: Volatility<1(실 Game row 0.15)은 정상 계산', () => {
  const V = fakeValues();
  assert.doesNotThrow(() => operationTotals(V, 10, { grade: 'B', q: 1.0625, affinityGrade: 'B', trendMatched: false, buildingIncomeMult: 1, statBonus: 1, industry: 'Game' }));
});

test('operationTotals: Volatility>=1 은 조용한 근사 대신 명시적 에러', () => {
  const ctx = { grade: 'B', q: 1.0625, affinityGrade: 'B', trendMatched: false, buildingIncomeMult: 1, statBonus: 1, industry: 'Game' };
  assert.throws(() => operationTotals(fakeValuesWithVolatility(1.0), 10, ctx), /[Vv]olatility/);
  assert.throws(() => operationTotals(fakeValuesWithVolatility(1.5), 10, ctx), /[Vv]olatility/);
});

test('recoveryRatio: idx 무관 (개발비 N² 계열이면 idx가 소거됨을 재확인)', () => {
  const V = fakeValues();
  const ctx = { grade: 'A', q: 1.4375, affinityGrade: 'B', trendMatched: false, buildingIncomeMult: 1, statBonus: 1, industry: 'Game' };
  const r10 = recoveryRatio(V, 10, ctx);
  const r55 = recoveryRatio(V, 55, ctx);
  assert.ok(Math.abs(r10 - r55) / r10 < 0.01, `idx10=${r10} idx55=${r55}`);
});

// ── F1 (2026-08-03 리뷰): PayoffGamble 의 EventRewardMultiplier 는 BaseRevenuePerSecond 단에 곱해진다 ──
test('F1 — eventRewardMult 가 baseRatePerSec 에 선형으로 곱해진다(미지정이면 1.0)', () => {
  const V = fakeValues();
  const ctx = {
    grade: 'B', q: 1.0, affinityGrade: null, trendMatched: false,
    buildingIncomeMult: 1, statBonus: 1, industry: 'Game',
  };
  const plain = operationTotals(V, 10, ctx);
  const boosted = operationTotals(V, 10, { ...ctx, eventRewardMult: 1.3 });
  assert.ok(Math.abs(boosted.baseRatePerSec / plain.baseRatePerSec - 1.3) < 1e-9);
  assert.ok(Math.abs(boosted.totalRevenue / plain.totalRevenue - 1.3) < 1e-9);
  // 미지정 = 1.0 (기존 호출부 회귀 없음)
  assert.deepStrictEqual(operationTotals(V, 10, { ...ctx, eventRewardMult: 1 }), plain);
});
