#!/usr/bin/env node
// 닫힌형 **JS 이식본**(closedForm — C++ FStageProgressData::EstimateExpectedQuality 을 손으로 옮긴 것)
// vs sim/project.js(몬테카를로) 구조 대조. **C++ 을 한 줄도 실행하지 않는다** — 여기 통과는
// "두 JS 모델의 배분식이 서로 맞다"까지고, C++ 자체의 검증이 아니다(그건 Private/Tests/PitchEstimateTests.cpp).
// C++ 만 바뀌고 이식본이 안 따라오면 이 스크립트는 조용히 통과한다.
// 절대값 일치는 기대하지 않는다 — 두 모델은 의도적으로 다르다:
//   (1) 시뮬은 히트 점수에 affinity 를 곱하지 않는다(선택 확률만 affinity 비례) → 배분이 aff/Σaff, 닫힌형은 aff²/Σaff.
//   (2) 시뮬만 duty(무탭 가동률)와 DPS_AUDIT_CALIB_FACTOR 를 곱한다.
// 판정축 2개:
//   [크기] 비(cfQ/simQ)가 프로젝트마다 일정한가 + 위 두 항으로 예측한 해석해와 실측 비의 편차가 0 인가.
//   [형상] 슬롯별 got 비가 예측 형상과 맞는가. 크기축은 target_i ∝ w_i 라 Σgot 의 순수 함수로 떨어져
//          **슬롯 분포를 못 본다**(총량 보존형 배분 오류 = 슬롯 인덱싱 어긋남이 안 잡힘) — 그 사각을 메우는 축.
// 둘 다 통과해도 검증되는 건 "닫힌형 이식본 vs 이 시뮬" 까지다. 실제 런타임 일치는 PIE 실측 소관이다.

const path = require('node:path');
const { simulateDevelopment, disciplineTargets, deriveDevScorePerEmpPerSec, DEFAULT_NO_TAP_DUTY } = require('../sim/project.js');
const { loadValues } = require('../sim/values.js');
const { makeRng } = require('../sim/rng.js');

const values = loadValues(path.join(__dirname, '..', 'out', 'values.json'));

const SLOTS = ['Weight_Plan', 'Weight_Dev', 'Weight_Graphics', 'Weight_Sound', 'Weight_Server', 'Weight_QA'];
const SLOT_LABELS = ['기획', '개발', '그래픽', '사운드', '서버', 'QA'];

// UOfficeStageProgressManager::DisciplineAffinity(Points) — 계수는 C++ 헤더에서 추출된 값을 읽는다.
// 여기에 하드코딩하면 sim/project.js 의 사본과 함께 stale 해져 C++ 리튠 시 **편차 0 으로 통과**한다.
// values 경유면 이 쪽만 따라 움직여 sim 사본과 벌어지므로 그 상황이 실패로 드러난다.
const affinityBase = values.get('work.affinityBase');
const affinityPerPoint = values.get('work.affinityPerPoint');
const affinityOf = pt => affinityBase + pt * affinityPerPoint;
// UEmployeeTypeHelper::CalculateBaseOutput(Level)
const baseOutput = lvl => values.get('emp.outputUnit') * (1 + Math.max(0, lvl - 1) * values.get('emp.levelFactorCurve'));
const perSecOf = (emp, scale) => baseOutput(emp.level ?? 1) * values.get('work.scoreNormInterval') * scale;

const weightsOf = row => SLOTS.map(k => row[k] ?? 0);
const activeOf = W => W.map((w, i) => (w > 0 ? i : -1)).filter(i => i >= 0);

// FStageProgressData::CalculateQualityScore — 캡 2.0, weight 가중평균, clamp[0.5,2.0].
function qualityScore(got, targets, W) {
  let weightedSum = 0;
  let weightTotal = 0;
  for (let i = 0; i < W.length; i++) {
    if (W[i] <= 0) continue;
    const rate = targets[i] > 0 ? got[i] / targets[i] : 0;
    weightedSum += Math.min(rate, 2.0) * W[i];
    weightTotal += W[i];
  }
  if (weightTotal <= 0) return 0.5;
  return Math.min(2.0, Math.max(0.5, weightedSum / weightTotal));
}

// 캡·클램프 이전 가중평균 달성률. 산출 배율에 선형이라 포화와 무관하게 두 모델의 비를 볼 수 있다.
function rawAchievement(got, targets, W) {
  let weightedSum = 0;
  let weightTotal = 0;
  for (let i = 0; i < W.length; i++) {
    if (W[i] <= 0) continue;
    weightedSum += (targets[i] > 0 ? got[i] / targets[i] : 0) * W[i];
    weightTotal += W[i];
  }
  return weightTotal > 0 ? weightedSum / weightTotal : 0;
}

// 최종 Q 가 안 붙어도 개별 스텝이 캡(2.0)에 걸리면 Q비는 미포화비와 갈린다 — 전문화 로스터에서 특히.
function anyStepCapped(got, targets, W) {
  for (let i = 0; i < W.length; i++) {
    if (W[i] > 0 && targets[i] > 0 && got[i] / targets[i] > 2.0) return true;
  }
  return false;
}

// AccumulateExpectedScores(StageProgressData.cpp:17-51) + CalculateQualityScore 이식.
function closedForm(roster, row, perWorkerOutputScale = 1) {
  const W = weightsOf(row);
  const targets = disciplineTargets(values, row);
  const active = activeOf(W);
  const got = new Array(W.length).fill(0);
  if (roster.length === 0 || row.Duration <= 0 || active.length === 0) {
    return { q: 0.5, raw: 0, stepCapped: false, got };
  }
  for (const emp of roster) {
    const aff = W.map((w, i) => (w > 0 ? affinityOf(emp.disciplinePts[i] ?? 0) : 0));
    const sumAff = aff.reduce((a, b) => a + b, 0);
    if (sumAff <= 0) continue;
    const perSec = perSecOf(emp, perWorkerOutputScale);
    for (const i of active) {
      got[i] += perSec * row.Duration * ((aff[i] * aff[i]) / sumAff);
    }
  }
  return { q: qualityScore(got, targets, W), raw: rawAchievement(got, targets, W), stepCapped: anyStepCapped(got, targets, W), got };
}

// 해석해 — target_i 가 weight 비례라 w_i/target_i 가 상수로 떨어지고, 가중평균 달성률의 비는
//   Σ_e(perSec_e x Σaff²/Σaff) / (dps x duty x N) 으로 프로젝트 의존성이 "활성 직능 집합" 하나만 남는다.
// (시뮬 쪽은 Σ_i aff_i/Σaff = 1 이라 직원당 총점이 배분과 무관하게 dps x duty x sec 로 고정된다.)
function predictedRatio(roster, row, perWorkerOutputScale = 1) {
  const active = activeOf(weightsOf(row));
  if (active.length === 0 || roster.length === 0) return NaN;
  const dps = deriveDevScorePerEmpPerSec(values);
  let cf = 0;
  for (const emp of roster) {
    const aff = active.map(i => affinityOf(emp.disciplinePts[i] ?? 0));
    const sumAff = aff.reduce((a, b) => a + b, 0);
    if (sumAff <= 0) continue;
    cf += perSecOf(emp, perWorkerOutputScale) * (aff.reduce((a, x) => a + x * x, 0) / sumAff);
  }
  return cf / (dps * perWorkerOutputScale * DEFAULT_NO_TAP_DUTY * roster.length);
}

function simAverage(roster, row, runs, scale) {
  const W = weightsOf(row);
  const targets = disciplineTargets(values, row);
  const calib = { devScorePerEmpPerSec: deriveDevScorePerEmpPerSec(values) * scale };
  const avgGot = new Array(W.length).fill(0);
  let qSum = 0;
  for (let s = 0; s < runs; s++) {
    const r = simulateDevelopment(makeRng(s + 1), values, calib, roster, row, {});
    qSum += r.q;
    for (let i = 0; i < W.length; i++) avgGot[i] += r.stepScores[i] / runs;
  }
  return { q: qSum / runs, raw: rawAchievement(avgGot, targets, W), stepCapped: anyStepCapped(avgGot, targets, W), got: avgGot };
}

// 슬롯별 형상 신호 — 미포화비는 target_i ∝ w_i 때문에 Σgot 의 순수 함수라 **슬롯 분포를 적분해 없앤다**
// (총량 보존형 배분 오류 = 슬롯 인덱싱 어긋남이 안 보임). 그래서 슬롯별 got 비를 따로 본다.
// 예측치는 로스터와 활성 집합만으로 계산 — 닫힌형의 배분 결과를 참조하지 않으므로 자기충족이 아니고,
// 분모는 MC 실측(시뮬 got)이라 어느 쪽이 슬롯을 뒤바꿔도 어긋난다.
function predictedSlotShape(roster, row, scale) {
  const active = activeOf(weightsOf(row));
  const dpsDuty = deriveDevScorePerEmpPerSec(values) * scale * DEFAULT_NO_TAP_DUTY;
  const cf = new Array(active.length).fill(0);
  const sim = new Array(active.length).fill(0);
  for (const emp of roster) {
    const aff = active.map(i => affinityOf(emp.disciplinePts[i] ?? 0));
    const sumAff = aff.reduce((a, b) => a + b, 0);
    if (sumAff <= 0) continue;
    const perSec = perSecOf(emp, scale);
    for (let k = 0; k < active.length; k++) {
      cf[k] += (perSec * aff[k] * aff[k]) / sumAff;
      sim[k] += (dpsDuty * aff[k]) / sumAff;
    }
  }
  return active.map((slot, k) => ({ slot, ratio: sim[k] > 0 ? cf[k] / sim[k] : NaN }));
}

// 공통 산출 배율을 양쪽에 똑같이 걸어(C++ PerWorkerOutputScale = 시뮬 calib.devScorePerEmpPerSec) 달성률을
// 이 값에 맞춘다. 두 모델 모두 이 배율에 선형이라 비는 불변 — Q 가 캡(2.0)/바닥(0.5)에 붙지 않는 동작점을
// 고르는 것뿐이다. 1.5 인 이유: 시뮬은 닫힌형의 ~1/2.3 이라 달성률 1.0 에서는 Q 바닥에 눌어붙는다.
const TARGET_ACHIEVEMENT = 1.5;
function chooseScale(roster, row) {
  const unit = closedForm(roster, row, 1).raw;
  return unit > 0 ? TARGET_ACHIEVEMENT / unit : 1;
}

function evaluate(roster, row, runs) {
  const weights = weightsOf(row);
  if (activeOf(weights).length === 0) return null;
  const scale = chooseScale(roster, row);
  const cf = closedForm(roster, row, scale);
  const sim = simAverage(roster, row, runs, scale);

  const shape = predictedSlotShape(roster, row, scale);
  let shapeDev = 0;
  for (const { slot, ratio } of shape) {
    if (!(sim.got[slot] > 0) || !Number.isFinite(ratio) || ratio <= 0) continue;
    shapeDev = Math.max(shapeDev, Math.abs((cf.got[slot] / sim.got[slot]) / ratio - 1));
  }
  // 활성 슬롯의 예측 형상이 평평하면 슬롯을 뒤바꿔도 got 이 같아 원리적으로 판별 불가 — 커버리지로 세어 밝힌다.
  const finite = shape.map(s => s.ratio).filter(r => Number.isFinite(r) && r > 0);
  const shapeBlind = finite.length < 2 || Math.max(...finite) / Math.min(...finite) < 1.01;

  return {
    idx: row.ProjectIndex,
    scale,
    simQ: sim.q,
    cfQ: cf.q,
    qRatio: sim.q > 0 ? cf.q / sim.q : NaN,
    rawRatio: sim.raw > 0 ? cf.raw / sim.raw : NaN,
    predicted: predictedRatio(roster, row, scale),
    stepCapped: cf.stepCapped || sim.stepCapped,
    shapeDev,
    shapeBlind,
    weights,
  };
}

function stats(xs) {
  const v = xs.filter(Number.isFinite).slice().sort((a, b) => a - b);
  if (v.length === 0) return null;
  const mean = v.reduce((a, b) => a + b, 0) / v.length;
  const sd = Math.sqrt(v.reduce((a, b) => a + (b - mean) ** 2, 0) / v.length);
  return { min: v[0], med: v[Math.floor(v.length / 2)], max: v[v.length - 1], mean, cv: mean !== 0 ? sd / mean : NaN, spread: v[0] !== 0 ? v[v.length - 1] / v[0] : NaN };
}

const ROSTERS = [
  { name: 'A 전문화 1인 (개발 13, 나머지 3, Lv1)', template: [{ disciplinePts: [3, 13, 3, 3, 0, 3], level: 1 }] },
  { name: 'B 균등 1인 (3/4/3/3/4/3, Lv1)', template: [{ disciplinePts: [3, 4, 3, 3, 4, 3], level: 1 }] },
  { name: 'C 혼합 3인 (기획/개발/그래픽 특화, Lv 1/5/10)', template: [
    { disciplinePts: [10, 2, 2, 2, 2, 2], level: 1 },
    { disciplinePts: [2, 10, 2, 2, 2, 2], level: 5 },
    { disciplinePts: [2, 2, 10, 2, 2, 2], level: 10 },
  ] },
];

const SAMPLE_IDX = [1, 11, 31, 51, 71, 91];
const SAMPLE_RUNS = 2000;
const SWEEP_RUNS = 2000; // 슬롯형상 축의 MC 노이즈를 판정선(SHAPE_LIMIT)의 1/3 이하로 낮추려면 이 정도가 필요
const SWEEP_TABLES = ['DT_Project_Game', 'DT_Project_IT', 'DT_Project_Finance', 'DT_Project_Electronics', 'DT_Project_Automobile', 'DT_Project_Semiconductor'];

const pad = (s, n) => String(s).padStart(n);
const f = (x, d = 3) => (Number.isFinite(x) ? x.toFixed(d) : '  -  ');

console.log('=== 추정기 대조 검산 (닫힌형 JS 이식본 vs 몬테카를로 sim — C++ 미실행) ===');
console.log(`values.json hash = ${values.hash}`);
console.log(`상수: outputUnit=${values.get('emp.outputUnit')} levelGrowth=${values.get('emp.levelFactorCurve')} scoreNormInterval=${values.get('work.scoreNormInterval')} baseInterval=${values.get('work.baseInterval')}`);
console.log(`affinity = ${affinityBase} + pt x ${affinityPerPoint} (C++ 헤더 추출값; sim/project.js 는 아직 사본 하드코딩)`);
console.log(`시뮬 전용 항: dps=${deriveDevScorePerEmpPerSec(values).toFixed(4)} (= outputUnit x scoreNorm x CALIB), duty=${DEFAULT_NO_TAP_DUTY}`);
console.log(`시뮬 배분=aff/Σaff, 닫힌형 배분=aff²/Σaff → 비는 활성 직능 집합에 따라 달라지는 게 정상.`);

const usableTables = SWEEP_TABLES.filter(t => {
  const rows = values.dt(t);
  const usable = rows.filter(r => activeOf(weightsOf(r)).length > 0).length;
  if (usable === 0) {
    // 스냅샷 결손이 아니라 콘텐츠 미저작이다 — DataImport/DT_Project_<산업>_Import.csv 헤더에
    // Weight_* 6컬럼 자체가 없어 DT 가 기본값 0 으로 임포트된다.
    // ⚠ 이 상태면 EstimateGateRisk 의 활성 가중치 가드가 걸려 그 산업 카드가 **전부 빨강(위험)** 이 된다.
    console.log(`[건너뜀] ${t}: ${rows.length}행 전부 직능 가중치 0 — CSV 에 Weight_* 컬럼 미저작. 대조 불가 + 카드 전량 게이트위험.`);
    return false;
  }
  if (usable < rows.length) console.log(`[주의] ${t}: ${rows.length}행 중 ${usable}행만 활성 직능 있음`);
  return true;
});

const gameRows = values.dt('DT_Project_Game');
const activeSets = new Set(gameRows.map(r => weightsOf(r).map(w => (w > 0 ? 1 : 0)).join('')));
console.log(`DT_Project_Game 활성 직능 집합 종류 = ${activeSets.size} (비를 좌우하는 축), 고유 weight 프로파일 = ${new Set(gameRows.map(r => weightsOf(r).join(','))).size}`);

const verdicts = [];
for (const { name, template } of ROSTERS) {
  console.log(`\n--- 로스터 ${name} ---`);
  console.log(`DT_Project_Game 표본 (${SAMPLE_RUNS}런, 공통 산출배율은 양 모델 동일 적용)`);
  console.log(' idx | 배율 | 활성직능                    | 시뮬평균Q | 닫힌형Q |  Q비 | 미포화비 | 해석해 | 편차%');
  for (const idx of SAMPLE_IDX) {
    const row = gameRows.find(r => r.ProjectIndex === idx);
    if (!row) continue;
    const e = evaluate(template, row, SAMPLE_RUNS);
    if (!e) continue;
    const act = activeOf(e.weights).map(i => SLOT_LABELS[i]).join('/');
    const dev = (e.rawRatio / e.predicted - 1) * 100;
    console.log(`${pad(idx, 4)} |${pad(f(e.scale, 2), 5)} | ${act.padEnd(26)} |${pad(f(e.simQ), 10)} |${pad(f(e.cfQ), 8)} |${pad(f(e.qRatio, 2), 5)} |${pad(f(e.rawRatio, 3), 9)} |${pad(f(e.predicted, 3), 7)} |${pad(dev.toFixed(2), 7)}`);
  }

  for (const table of usableTables) {
    const res = values.dt(table).map(r => evaluate(template, r, SWEEP_RUNS)).filter(Boolean);
    if (res.length === 0) continue;
    const raw = stats(res.map(e => e.rawRatio));
    const qr = stats(res.map(e => e.qRatio));
    const dev = stats(res.map(e => Math.abs(e.rawRatio / e.predicted - 1) * 100));
    const sat = res.filter(e => e.cfQ >= 1.999 || e.cfQ <= 0.501 || e.simQ >= 1.999 || e.simQ <= 0.501).length;
    console.log(`${table} 전수(${res.length}행, ${SWEEP_RUNS}런):`);
    console.log(`  미포화비   min=${f(raw.min)} med=${f(raw.med)} max=${f(raw.max)} 변동폭=${f(raw.spread, 2)}배 CV=${f(raw.cv * 100, 1)}%`);
    console.log(`  Q비        min=${f(qr.min)} med=${f(qr.med)} max=${f(qr.max)} (Q 포화행 ${sat}개, 스텝캡 발동행 ${res.filter(e => e.stepCapped).length}개 — 캡은 비선형이라 Q비를 미포화비에서 벌린다)`);
    console.log(`  해석해 편차 max=${f(dev.max, 3)}% (MC 노이즈 수준이면 배분의 크기 항엔 알려진 2개 항 외 차이 없음 — 슬롯 분포는 아래 축 소관)`);
    const shp = stats(res.map(e => e.shapeDev * 100));
    const blind = res.filter(e => e.shapeBlind).length;
    console.log(`  슬롯형상 편차 max=${f(shp.max, 2)}% (슬롯별 got 비 vs 예측, 판별불가행 ${blind}개 — 활성 슬롯 affinity 가 같아 순열이 무의미한 경우)`);
    verdicts.push({ label: `${name} / ${table}`, spread: raw.spread, dev: dev.max, shape: shp.max, blind, rows: res.length });
  }
}

const SPREAD_LIMIT = 2.0;
const DEV_LIMIT = 1.0;
// 슬롯별 got 은 저affinity 슬롯의 픽 확률이 낮아 MC 노이즈가 총량축보다 크다(2000런에서 실측 최대 ~4%).
// 이 축의 표적인 슬롯 인덱싱 어긋남은 편차가 수십~수백%라 판별선을 노이즈의 3배로 둬도 여유가 크다.
// 대신 이 축은 **거친** 형상 오류 탐지용이고, 10% 미만의 미세한 배분 왜곡은 못 잡는다.
const SHAPE_LIMIT = 12.0;
console.log(`\n판정 기준: 미포화비 변동폭 < ${SPREAD_LIMIT}배 + 해석해 편차 < ${DEV_LIMIT}% + 슬롯형상 편차 < ${SHAPE_LIMIT}%`);
console.log('  => 배분의 크기 항과 슬롯 형상이 모두 일치(나머지 차이는 스케일 상수).');

const skipped = SWEEP_TABLES.length - usableTables.length;
if (verdicts.length === 0) {
  console.log(`판정: 검증 불가 — 대조 가능한 DT 가 0개다(스킵 ${skipped}/${SWEEP_TABLES.length}). 통과가 아니다.`);
  process.exitCode = 2;
} else {
  const failed = verdicts.filter(v => !(v.spread < SPREAD_LIMIT) || !(v.dev < DEV_LIMIT) || !(v.shape < SHAPE_LIMIT));
  const cover = `${verdicts.length}조합 / DT ${SWEEP_TABLES.length}개 중 ${usableTables.length}개 검사(스킵 ${skipped})`;
  if (failed.length === 0) {
    const worst = verdicts.reduce((a, b) => (b.spread > a.spread ? b : a));
    const blindTotal = verdicts.reduce((a, v) => a + v.blind, 0);
    console.log(`판정: 이식본 통과 — C++ 검증 아님 (${cover}, 최악 변동폭 ${f(worst.spread, 2)}배 @ ${worst.label}, 최악 해석해 편차 ${f(Math.max(...verdicts.map(v => v.dev)), 3)}%, 최악 슬롯형상 편차 ${f(Math.max(...verdicts.map(v => v.shape)), 2)}%, 형상 판별불가 ${blindTotal}행)`);
  } else {
    console.log(`판정: 실패 — 배분의 크기 항 또는 슬롯 형상이 어긋났을 수 있다 (${cover}). 아래 조합 확인:`);
    for (const v of failed) console.log(`  ${v.label}: 변동폭 ${f(v.spread, 2)}배, 해석해 편차 ${f(v.dev, 3)}%, 슬롯형상 편차 ${f(v.shape, 2)}%`);
    process.exitCode = 1;
  }
}
