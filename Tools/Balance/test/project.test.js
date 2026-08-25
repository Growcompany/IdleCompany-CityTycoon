const { test } = require('node:test');
const assert = require('node:assert');
const { makeRng } = require('../sim/rng.js');
const { simulateDevelopment, gradeOf, disciplineTargets } = require('../sim/project.js');
const { fakeValues } = require('./helpers/fake_values.js');

// gate_passrate.js 의 makeEmployee 그대로 이식 — Common 신입 = 주직능 65%, 나머지 균등 분배.
function seedPts(primaryIdx) {
  const pts = new Array(6).fill(0);
  const total = 20, primary = Math.round(total * 0.65);
  pts[primaryIdx] = primary;
  let rem = total - primary;
  for (let k = 0; rem > 0; k++, rem--) pts[k % 6] += 1;
  return pts;
}

test('게이트 통과율: 4인 무탭 >= 0.90, 3인 무탭 0.60~0.85 (gate_passrate 기준선)', () => {
  const V = fakeValues();
  const roster3 = [0, 1, 2].map(i => ({ level: 1, star: 0, disciplinePts: seedPts(i) }));
  const roster4 = [0, 1, 2, 3].map(i => ({ level: 1, star: 0, disciplinePts: seedPts(i) }));
  const row = V.dt('DT_Project_Game')[0];
  // N=2000(시드 0~1999)은 4인 결과가 90.15%로 점근율(~91.3~91.6%, N=50000 확인)에서 벗어난 통계적 불운 구간이라
  // 90% 하한과 마진이 0.15pp뿐이었음(리뷰 지적) — N을 8000으로 올려 표본추정치를 점근율 근처에 앉힌다.
  const N = 8000;
  const rate = roster => {
    let pass = 0;
    for (let s = 0; s < N; s++) if (simulateDevelopment(makeRng(s), V, {}, roster, row, { tapDuty: 0 }).passGate) pass++;
    return pass / N;
  };
  const r4 = rate(roster4);
  assert.ok(r4 >= 0.90, `4인 통과율 ${r4}`);
  const r3 = rate(roster3);
  assert.ok(r3 >= 0.60 && r3 <= 0.85, `3인 통과율 ${r3}`);
});

test('gradeOf 경계: 1.625=S, 1.25=A, 0.875=B, 0.5=C', () => {
  const V = fakeValues();
  assert.strictEqual(gradeOf(1.625, V), 'S');
  assert.strictEqual(gradeOf(1.25, V), 'A');
  assert.strictEqual(gradeOf(0.875, V), 'B');
  assert.strictEqual(gradeOf(0.5, V), 'C');
});

test('disciplineTargets: weight>0 슬롯만 양수, 합이 ΣTarget 119.2 상당', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game')[0];
  const targets = disciplineTargets(V, row);
  assert.strictEqual(targets.length, 6);
  assert.strictEqual(targets[4], 0); // Weight_Server=0
  const sum = targets.reduce((a, b) => a + b, 0);
  assert.ok(Math.abs(sum - 119.2) < 1, `ΣTarget ${sum}`);
  // Weight_Dev(4)가 최댓값 → TargetBase 그 자체를 받음
  assert.ok(targets[1] > targets[0] && targets[1] > targets[2]);
});

test('simulateDevelopment: stepScores 길이=6, q/grade 정합', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game')[0];
  const roster = [0, 1, 2, 3].map(i => ({ level: 1, star: 0, disciplinePts: seedPts(i) }));
  const r = simulateDevelopment(makeRng(1), V, {}, roster, row, { tapDuty: 0 });
  assert.strictEqual(r.stepScores.length, 6);
  assert.strictEqual(r.grade, gradeOf(r.q, V));
  assert.ok(typeof r.passGate === 'boolean');
});

test('q 최종 클램프: 극단적 저DPS도 q 하한 0.5 (StageProgressData.h:226-227 경제 절연 불변)', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game')[0];
  const roster = [0, 1, 2, 3].map(i => ({ level: 1, star: 0, disciplinePts: seedPts(i) }));
  const r = simulateDevelopment(makeRng(1), V, { devScorePerEmpPerSec: 0.01 }, roster, row, { tapDuty: 0 });
  assert.strictEqual(r.q, 0.5); // 스텝별 캡(2.0)만으론 안 막힘 — 가중평균 자체의 하한 클램프가 필요했던 사례
  assert.strictEqual(r.grade, 'C');
});

test('calib.devScorePerEmpPerSec / calib.noTapDuty 오버라이드가 결과에 반영된다', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game')[0];
  const roster = [0, 1, 2, 3].map(i => ({ level: 1, star: 0, disciplinePts: seedPts(i) }));
  const lowDps = simulateDevelopment(makeRng(1), V, { devScorePerEmpPerSec: 0.01 }, roster, row, { tapDuty: 0 });
  assert.strictEqual(lowDps.passGate, false); // DPS 를 극단적으로 낮추면 게이트를 못 넘겨야 함
  const highDuty = simulateDevelopment(makeRng(1), V, { noTapDuty: 5 }, roster, row, { tapDuty: 0 });
  assert.strictEqual(highDuty.passGate, true); // duty 를 극단적으로 높이면 게이트를 넘겨야 함
});

// ── F1 (2026-08-03 리뷰): 부스트 도박은 dps 곱셈이 아니라 **목표합 기준 절대 가감** ──
// 실코드 ResolveBoostGamble(OfficeStageProgressManager.cpp:1684-1687, 1717-1740).

test('F1 — boostScoreFrac 은 각 활성 스텝에 (목표합×Frac)/활성수 를 절대 가감한다', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game').find(r => r.ProjectIndex === 1);
  const roster = [{ level: 1, disciplinePts: seedPts(1) }];
  const targets = disciplineTargets(V, row);
  const active = targets.map((t, i) => (t > 0 ? i : -1)).filter(i => i >= 0);
  const targetSum = active.reduce((a, i) => a + targets[i], 0);
  const frac = 0.22;
  const perStep = (targetSum * frac) / active.length;

  const base = simulateDevelopment(makeRng(7), V, {}, roster, row);
  const boosted = simulateDevelopment(makeRng(7), V, {}, roster, row, { boostScoreFrac: frac });
  for (const i of active) {
    // 같은 시드 = 같은 히트열이므로 차이는 정확히 perStep 이어야 한다(0 클램프에 안 걸리는 +방향).
    assert.ok(Math.abs((boosted.stepScores[i] - base.stepScores[i]) - perStep) < 1e-9,
      `스텝 ${i}: Δ${boosted.stepScores[i] - base.stepScores[i]} ≠ perStep ${perStep}`);
  }
});

test('F1 — 음의 Frac 은 0 에서 클램프된다(FMath::Max(0, ...))', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game').find(r => r.ProjectIndex === 1);
  const roster = [{ level: 1, disciplinePts: seedPts(1) }];
  // −10.0 이면 어떤 스텝도 살아남을 수 없다 — 전부 정확히 0, 음수 금지.
  const wrecked = simulateDevelopment(makeRng(7), V, {}, roster, row, { boostScoreFrac: -10.0 });
  const targets = disciplineTargets(V, row);
  for (let i = 0; i < targets.length; i++) {
    if (targets[i] > 0) assert.strictEqual(wrecked.stepScores[i], 0, `스텝 ${i} 가 음수로 내려갔다`);
  }
  assert.strictEqual(wrecked.passGate, false);
});

test('F1 — durationBonusSec 이 작업 시간을 늘려 총점이 올라간다(TimeExtend 절대 연장)', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game').find(r => r.ProjectIndex === 1);
  const roster = [{ level: 1, disciplinePts: seedPts(1) }, { level: 1, disciplinePts: seedPts(3) }];
  const sum = d => d.stepScores.reduce((a, v) => a + v, 0);
  const base = sum(simulateDevelopment(makeRng(11), V, {}, roster, row));
  const longer = sum(simulateDevelopment(makeRng(11), V, {}, roster, row, { durationBonusSec: row.Duration }));
  // 작업 시간이 2배면 총 기여도 대략 2배(히트 수·개당 점수 모두 seconds 비례).
  assert.ok(longer > base * 1.8, `연장 효과 부족: ${base} → ${longer}`);
});

test('F1 — durationBonusSec 음수(TimeCut)는 0 에서 클램프되고 총점이 0 이 된다', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game').find(r => r.ProjectIndex === 1);
  const roster = [{ level: 1, disciplinePts: seedPts(1) }];
  const d = simulateDevelopment(makeRng(3), V, {}, roster, row, { durationBonusSec: -9999 });
  assert.strictEqual(d.stepScores.reduce((a, v) => a + v, 0), 0);
});

test('F1 — 옵션 미지정이면 기존 동작과 완전히 동일(회귀 가드)', () => {
  const V = fakeValues();
  const row = V.dt('DT_Project_Game').find(r => r.ProjectIndex === 1);
  const roster = [{ level: 1, disciplinePts: seedPts(1) }];
  const a = simulateDevelopment(makeRng(42), V, {}, roster, row);
  const b = simulateDevelopment(makeRng(42), V, {}, roster, row, { durationBonusSec: 0, boostScoreFrac: 0 });
  assert.deepStrictEqual(a, b);
});
