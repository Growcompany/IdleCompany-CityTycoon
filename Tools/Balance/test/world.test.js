const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { writeFixture } = require('./helpers/fake_values.js');
const { runWorld } = require('../sim/world.js');
const { makePolicy, PROFILES } = require('../sim/policy.js');
const { aggregate } = require('../sim/run.js');

// 픽스처는 프로세스당 1회만 덤프(월드 런은 파일 경로를 받는다 — loadValues 인터페이스 그대로).
const FIXTURE = path.join(os.tmpdir(), `balance_world_fixture_${process.pid}.json`);
writeFixture(FIXTURE);

const REAL_VALUES = path.join(__dirname, '..', 'out', 'values.json');

// RunResult.ledger 는 직렬화 가능한 요약(toJSON) — 인스턴스 메서드 대신 요약끼리 대조한다.
function assertLedgerConsistent(l) {
  const sums = {};
  for (const [k, v] of Object.entries(l.bySrc)) {
    const cur = k.split('|')[0];
    sums[cur] = (sums[cur] ?? 0) + v;
  }
  for (const [cur, bal] of Object.entries(l.balances)) {
    assert.ok(Math.abs((sums[cur] ?? 0) - bal) < 1e-6, `원장-잔액 불일치 ${cur}: ${sums[cur]} vs ${bal}`);
    assert.ok(bal >= -1e-9, `음수 잔액 ${cur}: ${bal}`);
  }
}

test('스모크: mid 프로필 5시드가 T3+ 도달, 원장 일관, invalid 없음', () => {
  for (let seed = 1; seed <= 5; seed++) {
    const r = runWorld({ seed, profile: 'mid', valuesPath: FIXTURE, untilTier: 3, maxCalendarDays: 30 });
    assert.strictEqual(r.invalid, undefined, `seed ${seed}: ${r.invalid}`);
    assert.ok(r.tiers.length >= 3, `seed ${seed}: T${r.tiers.length}까지만 도달`);
    assertLedgerConsistent(r.ledger);
  }
});

test('결정성: 같은 시드 = 같은 결과 해시', () => {
  const h = r => JSON.stringify(r.tiers) + JSON.stringify(r.ledger.balances);
  const a = runWorld({ seed: 9, profile: 'mid', valuesPath: FIXTURE, untilTier: 2 });
  const b = runWorld({ seed: 9, profile: 'mid', valuesPath: FIXTURE, untilTier: 2 });
  assert.strictEqual(h(a), h(b));
  // 다른 시드는 달라야 함(시드가 실제로 소비되는지 = 상수 시뮬레이션이 아님을 확인)
  const c = runWorld({ seed: 10, profile: 'mid', valuesPath: FIXTURE, untilTier: 2 });
  assert.notStrictEqual(h(a), h(c));
});

test('RunResult 계약: 필수 필드 + 등급 분포 + valuesHash', () => {
  const r = runWorld({ seed: 3, profile: 'mid', valuesPath: FIXTURE, untilTier: 2 });
  assert.strictEqual(r.profile, 'mid');
  assert.strictEqual(r.seed, 3);
  assert.ok(typeof r.valuesHash === 'string' && r.valuesHash.length > 0);
  for (const t of r.tiers) {
    for (const k of ['tier', 'clears', 'repeats', 'sessions', 'inAppMin', 'calendarDay']) {
      assert.ok(Number.isFinite(t[k]), `tiers[].${k} 누락/비수치`);
    }
  }
  for (const g of ['S', 'A', 'B', 'C', 'gateFail']) assert.ok(Number.isFinite(r.gradeDist[g]));
  assert.ok(Number.isFinite(r.endState.money));
});

// 목록을 PROFILES 에서 뽑는다 — 진단 arm 이 추가되면 자동으로 계약 검사에 포함된다.
test('전 프로필(진단용 midNoTap/lowTap 포함)이 정책 훅을 갖고 완주한다', () => {
  for (const profile of Object.keys(PROFILES)) {
    const p = makePolicy(profile, null);
    assert.strictEqual(p.profile, profile);
    assert.ok(p.sessionMinutes > 0 && p.sessionsPerDay > 0);
    for (const hook of ['pickProject', 'investPriority', 'useBoostGamble', 'useSubEconomy', 'targetGradeMix']) {
      assert.strictEqual(typeof p[hook], 'function', `${profile}.${hook} 누락`);
    }
    const mix = p.targetGradeMix();
    const sum = mix.C + mix.B + mix.A + mix.S;
    assert.ok(Math.abs(sum - 1) < 1e-9, `${profile} 등급 mix 합 ${sum}`);
    const r = runWorld({ seed: 7, profile, valuesPath: FIXTURE, untilTier: 2, maxCalendarDays: 40 });
    assert.strictEqual(r.invalid, undefined, `${profile}: ${r.invalid}`);
  }
});

test('티어 진입 규칙: 지나온 티어는 7첫클리어 + 빌딩Lv 게이트 충족', () => {
  const r = runWorld({ seed: 4, profile: 'mid', valuesPath: FIXTURE, untilTier: 3, maxCalendarDays: 40 });
  const clearToUnlock = 7;
  // 마지막(진행 중) 티어를 제외한 나머지는 전부 게이트를 넘겨서 빠져나온 것
  for (const t of r.tiers.slice(0, -1)) {
    assert.ok(t.clears >= clearToUnlock, `T${t.tier} clears=${t.clears} < ${clearToUnlock}`);
    assert.ok(t.clears <= 10, `T${t.tier} 첫클리어가 티어 프로젝트 수(10)를 초과: ${t.clears}`);
  }
  // 재개발은 첫클리어 카운트에 기여 0 (별도 집계)
  assert.ok(r.tiers.every(t => t.repeats >= 0));
  const reached = r.tiers.length;
  assert.ok(r.endState.primaryBuildingLevel >= (reached - 1) * 2,
    `빌딩 게이트 위반: Lv${r.endState.primaryBuildingLevel} < ${(reached - 1) * 2}`);
});

test('내부 어서션 실패는 throw 가 아니라 invalid 마킹 + 시드 보존', () => {
  const broken = JSON.parse(fs.readFileSync(FIXTURE, 'utf8'));
  // 개발비를 비수치로 오염 → 원장 post 시 NaN 이 되어 내부 어서션이 걸려야 한다
  broken.dt.DT_Project_Game = broken.dt.DT_Project_Game.map(r => ({ ...r, DevelopmentCost: 'corrupted' }));
  const p = path.join(os.tmpdir(), `balance_world_broken_${process.pid}.json`);
  fs.writeFileSync(p, JSON.stringify(broken));
  let r;
  assert.doesNotThrow(() => { r = runWorld({ seed: 11, profile: 'mid', valuesPath: p, untilTier: 3 }); });
  assert.ok(typeof r.invalid === 'string' && r.invalid.length > 0, 'invalid 미마킹');
  assert.strictEqual(r.seed, 11);
  fs.unlinkSync(p);
});

test('maxCalendarDays 를 넘으면 invalid 없이 도달 티어만 낮게 끝난다', () => {
  const r = runWorld({ seed: 2, profile: 'low', valuesPath: FIXTURE, untilTier: 10, maxCalendarDays: 3 });
  assert.strictEqual(r.invalid, undefined);
  assert.ok(r.endState.calendarDay <= 4, `캘린더 초과: ${r.endState.calendarDay}`);
  assert.ok(r.tiers.length < 10);
});

test('aggregate: 중앙값/사분위 요약', () => {
  const fake = n => ({ tiers: [{ tier: 1 }], endState: { inAppMin: n * 60, calendarDay: n, reachedTier: 3 }, invalid: undefined });
  const agg = aggregate([1, 2, 3, 4].map(fake), 'mid');
  assert.strictEqual(agg.profile, 'mid');
  assert.strictEqual(agg.runs, 4);
  assert.strictEqual(agg.invalidRuns, 0);
  // R-7(선형보간) 기준: [1,2,3,4] → p25 1.75 / median 2.5 / p75 3.25
  assert.strictEqual(agg.inAppHours.median, 2.5);
  assert.strictEqual(agg.inAppHours.p25, 1.75);
  assert.strictEqual(agg.inAppHours.p75, 3.25);
});

test('실 values.json 스모크 — 로드/런 완주(존재할 때만)', { skip: !fs.existsSync(REAL_VALUES) }, () => {
  const r = runWorld({ seed: 1, profile: 'mid', valuesPath: REAL_VALUES, untilTier: 2, maxCalendarDays: 30 });
  assert.strictEqual(r.invalid, undefined, String(r.invalid));
  assertLedgerConsistent(r.ledger);
  assert.ok(r.tiers.length >= 1);
});

// F2 회귀 — settleGap 이 이미 클리핑된 gain 을 storeRevenue 에 다시 넣으면 손실이 항상 0이 된다(죽은 지표).
// 프로필 하나를 못박으면 수명/등급 리튠에 취약하므로 "적어도 한 프로필에서 손실이 잡힌다"로 검사한다.
test('금고 손실 지표가 구조적 0이 아니다', { skip: !fs.existsSync(REAL_VALUES) }, () => {
  const losses = ['low', 'mid', 'midNoTap', 'high', 'skilled'].map(profile => {
    const r = runWorld({ seed: 1, profile, valuesPath: REAL_VALUES });
    assert.ok(Number.isFinite(r.endState.vaultLostMoney) && r.endState.vaultLostMoney >= 0);
    return r.endState.vaultLostMoney;
  });
  assert.ok(losses.some(v => v > 0), `전 프로필 금고 손실 0 — 지표가 다시 죽었을 가능성: ${losses.join(',')}`);
});

// ── F1 (2026-08-03 리뷰): 부스트 도박 배선 격리 ──

test('F1 — boostGamble:true 프로필만 K2 를 태우고, false 프로필은 원장에 K2 자체가 없다', () => {
  const high = runWorld({ seed: 5, profile: 'high', valuesPath: FIXTURE, untilTier: 4 });
  const mid = runWorld({ seed: 5, profile: 'mid', valuesPath: FIXTURE, untilTier: 4 });
  assert.ok(PROFILES.high.boostGamble === true && PROFILES.mid.boostGamble === false,
    '프로필 정의가 바뀌었다 — 이 테스트의 전제 갱신 필요');
  assert.ok((high.ledger.bySrc['Money|K2'] ?? 0) < 0, 'high 가 GoCost 를 안 태웠다');
  assert.strictEqual(mid.ledger.bySrc['Money|K2'], undefined, 'mid 원장에 K2 가 생겼다 — 도박이 새고 있다');
});

// TimeExtend 의 연장 시간은 세션 예산에서 차감된다(runLaunches 가 launch() 반환값을 정산).
// ⚠ 실 DT 값(TimeSeconds=4)에서는 이 항이 **결과를 바꾸지 않는다** — 세션 예산 450s 에 대해
//    병렬 슬롯 P≤5 × 판당 35s = 175s 라 시간이 애초에 병목이 아니기 때문(병목은 "빈 빌딩 수").
//    그래서 배선 자체는 예산이 실제로 묶이는 큰 값으로 검증한다.
function fixtureWithGambleTime(t) {
  const raw = JSON.parse(fs.readFileSync(FIXTURE, 'utf8'));
  raw.dt.DT_BoostGamble[0].TimeSeconds = t;
  const p = path.join(os.tmpdir(), `balance_world_gtime_${t}_${process.pid}.json`);
  fs.writeFileSync(p, JSON.stringify(raw));
  return p;
}

test('F1 — TimeExtend 연장분이 세션 예산에서 실제로 차감된다(공짜 부스트 금지)', () => {
  const raw = JSON.parse(fs.readFileSync(FIXTURE, 'utf8'));
  assert.strictEqual(raw.dt.DT_BoostGamble[0].EffectType, 'TimeExtend');
  const zero = fixtureWithGambleTime(0);
  const huge = fixtureWithGambleTime(600); // 판당 35s + 600s > 세션 450s → 세션당 1판으로 묶인다
  const a = runWorld({ seed: 9, profile: 'high', valuesPath: zero, untilTier: 6 });
  const b = runWorld({ seed: 9, profile: 'high', valuesPath: huge, untilTier: 6 });
  fs.unlinkSync(zero); fs.unlinkSync(huge);
  assert.ok(a.endState.launches > b.endState.launches,
    `야근 시간이 세션 예산에 안 잡힌다: T=0 ${a.endState.launches}판 vs T=600 ${b.endState.launches}판`);
});
