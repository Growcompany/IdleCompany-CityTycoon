// 시뮬 예측 ↔ PIE 실측 대조 게이트 (기본 ±10%) + sim 이 --calib 로 먹을 calibration.json 생성.
//
// 게이트 규칙:
//  - errPct = |measured − predicted| / predicted, tolerance 이하면 pass (경계값 포함).
//  - 측정이 없는 예측 키는 pass=false 로 **표면화**한다 (라인 부재 = 조용한 0 금지).
//  - 예측이 없는 측정 키는 참고용 행으로만 남기고 게이트에 불참(gated=false).

const TOLERANCE = 0.10;

function errorPct(predicted, measured) {
  if (predicted === 0) return measured === 0 ? 0 : Infinity;
  return Math.abs(measured - predicted) / Math.abs(predicted);
}

function calibrate(predictions, measurements, tolerance = TOLERANCE) {
  const rows = [];
  for (const key of Object.keys(predictions)) {
    const predicted = predictions[key];
    const measured = measurements ? measurements[key] : undefined;
    if (measured === undefined || measured === null || !Number.isFinite(Number(measured))) {
      rows.push({ key, predicted, measured: null, errPct: null, pass: false, gated: true, note: '측정값 없음 ([BALV] 라인 부재 — 0 으로 간주 금지)' });
      continue;
    }
    const errPct = errorPct(predicted, Number(measured));
    rows.push({ key, predicted, measured: Number(measured), errPct, pass: errPct <= tolerance, gated: true });
  }
  for (const key of Object.keys(measurements || {})) {
    if (key in predictions) continue;
    rows.push({ key, predicted: null, measured: measurements[key], errPct: null, pass: null, gated: false, note: '예측 없음 — 참고용' });
  }
  const gatedRows = rows.filter(r => r.gated);
  return { ok: gatedRows.length > 0 && gatedRows.every(r => r.pass), tolerance, rows };
}

// 실측 → sim 의 calib 인터페이스(project.js / world.js 가 읽는 키)로 환산.
// 무탭 관측치 = dps × duty 이므로 풀탭(duty=1) 측정이 있어야만 둘을 분리할 수 있다 — 없으면 지어내지 않는다.
function buildCalibParams(m) {
  const c = { _notes: [] };
  const a1 = m && m.a1;
  if (a1 && Number.isFinite(a1.devScorePerEmpPerSecFullTap) && a1.devScorePerEmpPerSecFullTap > 0) {
    c.devScorePerEmpPerSec = a1.devScorePerEmpPerSecFullTap;
    if (Number.isFinite(a1.devScorePerEmpPerSecNoTap)) {
      c.noTapDuty = Number((a1.devScorePerEmpPerSecNoTap / a1.devScorePerEmpPerSecFullTap).toFixed(4));
    }
  } else {
    c._notes.push('a1 풀탭(devScorePerEmpPerSecFullTap) 측정 부재 — dps/duty 분리 불가라 devScorePerEmpPerSec·noTapDuty 를 비워 둔다(시뮬은 values 유도식 기본값 사용).');
  }
  if (m && m.a3 && Number.isFinite(m.a3.idlePerSecPerEmp)) c.idlePerSecPerEmp = m.a3.idlePerSecPerEmp;
  if (m && m.a4) {
    if (Number.isFinite(m.a4.offlineGainRatio)) c.offlineGainRatio = m.a4.offlineGainRatio;
    if (Number.isFinite(m.a4.vaultFallbackCap)) c.vaultFallbackCap = m.a4.vaultFallbackCap;
  }
  // ★→개발점수 커플링. 소비자(world.js:STAR_DEV_BONUS)만 있고 **생산자가 어디에도 없어** 항상 0이었다
  // (2026-08-03 리뷰 F7). 계측 경로가 없는 값이므로 측정 절차(§11 "차분/N")로 손 계산한 뒤
  // measurements.json 의 manual 블록에 적으면 여기서 calibration.json 으로 넘어간다.
  if (m && m.manual && Number.isFinite(m.manual.starDevBonusPerStar)) {
    c.starDevBonusPerStar = m.manual.starDevBonusPerStar;
  } else {
    c._notes.push('starDevBonusPerStar 미기입 — world.js 가 ★를 사지 않는다(K4 실행 0회). 측정 절차 = 리포트 §11 "★0 로스터 ↔ ★N 로스터 A1 2회 측정 → 차분/N".');
  }
  return c;
}

// 예측이 "측정 없이 가정한 것" 목록 — 침묵 기본값 금지. 게이트 표에 숫자로 안 잡히는 가정을 여기로 뽑아
// 리포트/CLI 가 그대로 싣는다(2026-08-03 리뷰 F9②).
function predictionAssumptions(ctx = {}) {
  const a = [];
  if (!Number.isFinite(ctx.projectIndex)) {
    a.push('a2_passRate*: `projectIndex` 미관측 → `DT_Project_*` 1행 가정. `Balance_GateRun` 은 `[BALV] GateRunDone ... proj=<N> dir=<N>` 으로 실제 행을 찍으므로, 그 로그를 파싱했다면 자동으로 잡힌다.');
  }
  if (!Array.isArray(ctx.rosterSizes) || ctx.rosterSizes.length === 0) {
    a.push('a2_passRate*: 측정된 로스터 크기가 0개 → **예측 행 자체를 만들지 않는다**(과거엔 [3,4] 를 지어내 게이트 분모만 부풀렸다). ⚠ A2 는 **게이트 분모에서 빠진 것이지 통과한 게 아니다** — 여전히 미실측이다(§11 방법 A).');
  }
  return a;
}

// CalculateBaseOutput(EmployeeTypes.cpp) 의 LevelFactor = 1 + max(0,Lv-1)×LevelGrowth 를 로스터 평균으로.
// 레벨이 안 관측되면 1.0 (= values 유도식의 레벨1 기준선 그대로).
function meanLevelFactor(values, levels) {
  if (!Array.isArray(levels) || levels.length === 0) return 1;
  const growth = values.get('emp.levelFactorCurve');
  return levels.reduce((a, L) => a + (1 + Math.max(0, Number(L) - 1) * growth), 0) / levels.length;
}

// 실측과 같은 조건(같은 DT 행 / 같은 로스터 크기)으로 예측을 만든다 — 사과 대 사과.
// ctx 는 measurements 에서 관측된 컨텍스트(직원 레벨, 보고된 실수익률 등)를 넘긴다.
function buildPredictions(values, ctx = {}) {
  const { simulateDevelopment, deriveDevScorePerEmpPerSec, DEFAULT_NO_TAP_DUTY } = require('../sim/project.js');
  const { idlePerSecond, OFFLINE_SETTLE_RATE } = require('../sim/idle.js');
  const { makeRng } = require('../sim/rng.js');

  const p = {};
  const dps = deriveDevScorePerEmpPerSec(values);
  // 개발 기여도도 레벨 비례다(EmployeeBehaviorComponent 가 CalculateBaseOutput(Level) 로 히트 점수를 만든다).
  // values 유도식은 레벨1 기준이라, 실측 로스터가 고레벨이면 그대로 비교하면 사과 대 오렌지가 된다.
  const lfA1 = meanLevelFactor(values, ctx.a1RosterLevels);
  p.a1_devScorePerEmpPerSecFullTap = dps * lfA1;
  p.a1_devScorePerEmpPerSecNoTap = dps * lfA1 * DEFAULT_NO_TAP_DUTY;

  const industry = ctx.industry ?? 'Game';
  // GateRunDone 의 proj= 를 그대로 쓴다(parse_log.js → m.a2.projectIndex). 미관측이면 1행 가정 —
  // 그 가정 자체는 predictionAssumptions() 가 리포트/CLI 에 문장으로 싣는다(조용한 기본값 금지).
  const projectIndex = Number.isFinite(ctx.projectIndex) ? ctx.projectIndex : 1;
  const row = values.dt(`DT_Project_${industry}`).find(r => r.ProjectIndex === projectIndex);
  if (row) {
    const lfA2 = meanLevelFactor(values, ctx.a2RosterLevels);
    // 안 잰 로스터 크기를 지어내지 않는다 — [3,4] 기본 생성은 게이트 분모만 부풀리고 영원히 FAIL 로 남았다
    // (2026-08-03 리뷰 F9③). 측정이 0개면 a2 예측 행 자체를 만들지 않는다.
    const sizes = Array.isArray(ctx.rosterSizes) ? ctx.rosterSizes : [];
    for (const n of sizes) {
      const rng = makeRng(0xBA1A0000 + n);
      const roster = Array.from({ length: n }, () => ({ level: 1, disciplinePts: [0, 0, 0, 0, 0, 0] }));
      const trials = ctx.trials ?? 2000;
      let pass = 0;
      // project.js 는 레벨을 안 보고 calib.devScorePerEmpPerSec 만 쓴다 → 레벨 보정은 여기서 주입한다.
      const calib = { devScorePerEmpPerSec: dps * lfA2 };
      for (let i = 0; i < trials; i++) {
        if (simulateDevelopment(rng, values, calib, roster, row).passGate) pass++;
      }
      p[`a2_passRate${n}NoTap`] = pass / trials;
    }
  }

  // 로스터 레벨이 관측됐으면 그 로스터의 평균으로 예측한다 — 방치수익은 레벨 비례라
  // 레벨 1 가정으로 비교하면 사과 대 오렌지가 된다.
  const levels = Array.isArray(ctx.rosterLevels) && ctx.rosterLevels.length > 0 ? ctx.rosterLevels : [ctx.empLevel ?? 1];
  p.a3_idlePerSecPerEmp = levels.reduce((a, L) => a + idlePerSecond(values, { level: L }), 0) / levels.length;

  // A4 금고 적립률은 시뮬 예측이 아니라 **런타임 자기정합성** 대조 —
  // "치트가 보고한 실수익률(b<N>_rate)" 대로 금고가 쌓이는지 본다.
  if (Number.isFinite(ctx.reportedRatePerSec)) p.a4_vaultAccrualRate = ctx.reportedRatePerSec;
  // 오프라인 정산률은 상수 1.0 이라 강화 레벨 컨텍스트가 필요 없다 — 남는 변수는 트레이트 배율뿐.
  p.a4_offlineGainRatio = OFFLINE_SETTLE_RATE * (ctx.traitFactor ?? 1.0);
  p.a4_vaultFallbackCap = values.get('vault.baseCapacity');
  return p;
}

// measurements.json → buildPredictions ctx. parse_log 산출물과 수동 기입(manual)을 한 곳에서 합친다
// (calibrate CLI / report.js buildGate 가 각자 다른 ctx 를 만들어 결과가 갈리던 것을 통일).
function contextFromMeasurements(m) {
  const flat = (m && m.flat) || {};
  const manual = (m && m.manual) || {};
  return {
    reportedRatePerSec: m && m.a4 ? m.a4.reportedRatePerSec : undefined,
    rosterLevels: m && m.a3 ? m.a3.rosterLevels : undefined,
    a1RosterLevels: m && m.a1 ? m.a1.rosterLevels : undefined,
    a2RosterLevels: m && m.a2 ? m.a2.rosterLevels : undefined,
    // GateRunDone proj= 를 소비 (parse_log.js F9①). 수동 기입이 있으면 그쪽을 우선.
    projectIndex: Number.isFinite(manual.projectIndex) ? manual.projectIndex
      : (m && m.a2 && Number.isFinite(m.a2.projectIndex) ? m.a2.projectIndex : undefined),
    direction: m && m.a2 && Number.isFinite(m.a2.direction) ? m.a2.direction : undefined,
    traitFactor: Number.isFinite(manual.traitFactor) ? manual.traitFactor : undefined,
    // 실측이 낸 로스터 크기로만 예측한다 — 안 잰 크기를 예측하면 게이트가 영원히 FAIL 로 남는다.
    rosterSizes: Object.keys(flat)
      .map(k => (k.match(/^a2_passRate(\d+)NoTap$/) || [])[1])
      .filter(Boolean)
      .map(Number),
  };
}

module.exports = {
  calibrate, buildCalibParams, buildPredictions, predictionAssumptions,
  contextFromMeasurements, meanLevelFactor, errorPct, TOLERANCE,
};

if (require.main === module) {
  const fs = require('fs');
  const path = require('path');
  const { loadValues } = require('../sim/values.js');
  const base = path.join(__dirname, '..');
  const mPath = process.argv[2] || path.join(base, 'out', 'measurements.json');
  const vPath = process.argv[3] || path.join(base, 'out', 'values.json');
  const outPath = path.join(base, 'out', 'calibration.json');

  if (!fs.existsSync(mPath)) {
    console.error(`measurements.json 없음: ${mPath} — 먼저 node Tools/Balance/harness/parse_log.js <로그> 를 돌릴 것`);
    process.exit(2);
  }
  const m = JSON.parse(fs.readFileSync(mPath, 'utf8'));
  const values = loadValues(vPath);
  const ctx = contextFromMeasurements(m);
  const predictions = buildPredictions(values, ctx);
  const assumptions = predictionAssumptions(ctx);
  const flat = m.flat || {};
  const gate = calibrate(predictions, flat);
  const calib = buildCalibParams(m);
  calib._generatedAt = new Date().toISOString();
  calib._valuesHash = values.hash;
  calib._gateOk = gate.ok;
  calib._assumptions = assumptions;
  if (Number.isFinite(ctx.projectIndex)) calib._measuredProjectIndex = ctx.projectIndex;
  if (Number.isFinite(ctx.direction)) calib._measuredDirection = ctx.direction;
  fs.writeFileSync(outPath, JSON.stringify(calib, null, 2));

  const pad = (s, n) => String(s).padEnd(n);
  console.log(`캘리브레이션 게이트 (tolerance ±${(gate.tolerance * 100).toFixed(0)}%)  values=${values.hash}`);
  for (const r of gate.rows) {
    const e = r.errPct === null ? '   —  ' : `${(r.errPct * 100).toFixed(1)}%`;
    const st = r.gated ? (r.pass ? 'PASS' : 'FAIL') : ' -- ';
    console.log(`  ${st} ${pad(r.key, 34)} pred=${pad(r.predicted === null ? '—' : Number(r.predicted).toFixed(4), 12)} meas=${pad(r.measured === null ? '—' : Number(r.measured).toFixed(4), 12)} err=${e}${r.note ? '  ' + r.note : ''}`);
  }
  if (assumptions.length) {
    console.log('\n미측정 가정 (숫자로 안 잡히는 것 — 조용히 넘기지 않는다):');
    for (const a of assumptions) console.log(`  ! ${a.replace(/`/g, '')}`);
  }
  for (const n of calib._notes || []) console.log(`  ! ${n}`);
  console.log(`\n게이트: ${gate.ok ? 'OK' : 'FAIL'}  → ${outPath}`);
  if (!gate.ok) process.exitCode = 1;
}
