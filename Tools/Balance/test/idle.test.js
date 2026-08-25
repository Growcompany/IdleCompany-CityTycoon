const { test } = require('node:test');
const assert = require('node:assert');
const { idlePerSecond, offlineGains, OFFLINE_SETTLE_RATE } = require('../sim/idle.js');
const { fakeValues } = require('./helpers/fake_values.js');

// 브리핑 Step1 지정 테스트 그대로.
test('Idle 실효 4.875/초/인 (Lv1) — retune 확정치', () => {
  assert.ok(Math.abs(idlePerSecond(fakeValues(), { level: 1, star: 0 }) - 4.875) < 0.01);
});
test('2인 6시간 = 210,600 (retune §5-9 보정값)', () => {
  const V = fakeValues();
  const total = 2 * idlePerSecond(V, { level: 1, star: 0 }) * 21600;
  assert.ok(Math.abs(total - 210600) < 100);
});
test('오프라인: 정산률 1.0(온라인 동률), 금고 캡 클리핑, 진행 0.2배', () => {
  const V = fakeValues();
  const r0 = offlineGains(V, { actualRatePerSec: 100, remainingSeconds: 7200, vaultCap: 5000, stored: 0 }, 3600, 1.0);
  // raw = 클리핑 전 발생액. gain 만 있으면 호출자가 "금고 때문에 날린 돈"을 계산할 수 없다(Task10 F2).
  assert.ok(r0.raw > r0.gain, `클리핑됐는데 raw(${r0.raw}) <= gain(${r0.gain})`);
  assert.strictEqual(r0.gain, 5000);
  // raw = 100×1.0×3600 = 360,000 → 캡 5000 클리핑
  assert.strictEqual(r0.raw, 360000);
  assert.strictEqual(r0.clippedByVault, true);
  assert.ok(Math.abs(r0.opElapsedAdvance - 720) < 1); // 3600×0.2
});

// 추가 커버리지 — 브리핑이 "테스트로 커버" 요구한 60초 미만 무시 / 12h 캡, 레벨 곡선, ★ 미기여.
test('idlePerSecond: Lv2는 LevelFactor(1+0.10×(Lv-1)) 반영 — 5.3625/초', () => {
  // CalculateBaseOutput(2)=5.2×1.10=5.72, ×IdleIncomeScale 1.875=10.725(StatMultiplier), ×Wander 0.5 = 5.3625
  assert.ok(Math.abs(idlePerSecond(fakeValues(), { level: 2, star: 0 }) - 5.3625) < 0.001);
});
test('idlePerSecond: ★(star)는 방치수익에 미기여 — GenerateIncome이 EnhancementLevel 미참조(retune §7-12)', () => {
  const V = fakeValues();
  assert.strictEqual(idlePerSecond(V, { level: 1, star: 0 }), idlePerSecond(V, { level: 1, star: 99 }));
});
test('offlineGains: 60초 미만은 네트워크 오차로 간주 — 전액 무시', () => {
  const V = fakeValues();
  const r = offlineGains(V, { actualRatePerSec: 100, remainingSeconds: 7200, vaultCap: 5000, stored: 0 }, 59, 1.0);
  assert.strictEqual(r.gain, 0);
  assert.strictEqual(r.opElapsedAdvance, 0);
  assert.strictEqual(r.clippedByVault, false);
});
test('offlineGains: 정확히 60초는 무시되지 않고 정상 계산됨', () => {
  const V = fakeValues();
  const r = offlineGains(V, { actualRatePerSec: 100, remainingSeconds: 7200, vaultCap: 1e9, stored: 0 }, 60, 1.0);
  // raw = 100×1.0×60 = 6000, 캡 없음
  assert.ok(Math.abs(r.gain - 6000) < 0.01);
  assert.strictEqual(r.clippedByVault, false);
});
test('offlineGains: 12시간(43200초) 캡 — 초과분은 수익/진행 모두 반영 안 됨', () => {
  const V = fakeValues();
  const r = offlineGains(V, { actualRatePerSec: 1, remainingSeconds: 1e9, vaultCap: 1e9, stored: 0 }, 100000, 1.0);
  // cappedOffline = min(100000, 43200) = 43200 → raw = 1×1.0×43200 = 43200
  assert.ok(Math.abs(r.gain - 43200) < 0.01);
  assert.ok(Math.abs(r.opElapsedAdvance - 8640) < 0.01); // 43200×0.2
});
test('offlineGains: 정산률은 강화 노브가 아니라 상수 — 오프라인 raw = 온라인 rate × 초', () => {
  const V = fakeValues();
  const r = offlineGains(V, { actualRatePerSec: 100, remainingSeconds: 7200, vaultCap: 1e9, stored: 0 }, 3600, 1.0);
  assert.strictEqual(r.raw, 100 * 3600); // 감쇠율 외 어떤 계수도 끼지 않는다
  assert.strictEqual(OFFLINE_SETTLE_RATE, 1.0);
});
test('offlineGains: 운영 잔여시간이 오프라인보다 짧으면 EffectiveSeconds가 잔여시간으로 클램프', () => {
  const V = fakeValues();
  const r = offlineGains(V, { actualRatePerSec: 100, remainingSeconds: 1800, vaultCap: 1e9, stored: 0 }, 3600, 1.0);
  // EffectiveSeconds = min(3600, 1800) = 1800 → raw = 100×1.0×1800 = 180000
  assert.ok(Math.abs(r.gain - 180000) < 0.01);
});
test('offlineGains: traitFactor가 Raw 수익에 곱연산으로 반영', () => {
  const V = fakeValues();
  const r = offlineGains(V, { actualRatePerSec: 100, remainingSeconds: 7200, vaultCap: 1e9, stored: 0 }, 3600, 1.5);
  // raw = 100×1.0×3600×1.5 = 540000
  assert.ok(Math.abs(r.gain - 540000) < 0.01);
});
test('offlineGains: 금고에 이미 재고가 있으면 남은 공간만큼만 획득', () => {
  const V = fakeValues();
  const r = offlineGains(V, { actualRatePerSec: 100, remainingSeconds: 7200, vaultCap: 5000, stored: 4500 }, 3600, 1.0);
  // availableSpace = 5000-4500 = 500 < raw(360000) → 클리핑
  assert.strictEqual(r.gain, 500);
  assert.strictEqual(r.clippedByVault, true);
});
