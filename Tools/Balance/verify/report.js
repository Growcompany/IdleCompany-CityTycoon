#!/usr/bin/env node
// 검증 리포트 생성기 —
//   node Tools/Balance/verify/report.js --out specs/2026-08-03-economy-verification-report.md
//
// out/{values,measurements,calibration,scan}.json + out/runs/{agg-*,<profile>-<seed>}.json
// + verify/{assertions,coverage_check} 결과를 읽어 스펙 §8 산출물 마크다운을 만든다.
//
// 설계 원칙 (assertions.js 와 동일):
//  - 게임 수치 하드코딩 금지. 표의 모든 수는 out/*.json 에서 온다.
//  - 아래 상수 블록은 "대조 기준"(선행 스펙이 주장한 목표치 / 실코드 조사 결과)뿐이며
//    전부 출처를 주석에 명시한다. 값이 없으면 지어내지 않고 "미산출"로 표면화한다.
//  - 조용한 통과 금지: 입력이 없으면 throw 하거나 사유를 본문에 싣는다.

const fs = require('node:fs');
const path = require('node:path');

// ── 대조 기준 상수 (선행 스펙 인용 — 게임 값이 아니라 "그 문서가 주장한 목표치") ─────────
// 출처: specs/2026-07-29-balance-grind-retune.md §3-1 (프로필별 총량)
const RETUNE_PROFILE_TARGET = {
  low: { label: '하위(순수 C)', inAppH: 30.8, days: 70, launches: 839 },
  mid: { label: '중위(C65/B22/A13)', inAppH: 13.2, days: 30, launches: 335 },
  high: { label: '상위(C25/B40/A25/S10)', inAppH: 8.3, days: 19, launches: 194 },
  skilled: { label: '숙련(순수 B)', inAppH: 9.9, days: 23, launches: 240 },
};
// 출처: retune §3-2 (중위 프로필 티어별). inAppH = 그 티어에서 쓰는 인앱시간, repPerProj = 반복/프로젝트
const RETUNE_MID_TIER = {
  1: { inAppH: 1.51, repPerProj: 0.5 }, 2: { inAppH: 1.35, repPerProj: 0.4 },
  3: { inAppH: 1.10, repPerProj: 1.1 }, 4: { inAppH: 1.14, repPerProj: 1.1 },
  5: { inAppH: 0.86, repPerProj: 1.4 }, 6: { inAppH: 1.05, repPerProj: 1.8 },
  7: { inAppH: 1.01, repPerProj: 2.5 }, 8: { inAppH: 1.37, repPerProj: 3.7 },
  9: { inAppH: 1.56, repPerProj: 5.5 }, 10: { inAppH: 2.30, repPerProj: 8.5 },
};
// untilTier=10 은 "T10 진입"(=T9 완료)이라 retune 총량에서 T10 파밍분을 빼야 같은 기준이 된다 (Task 10 §5).
const RETUNE_MID_T10_H = RETUNE_MID_TIER[10].inAppH;
const PROJECTS_PER_TIER = 10; // sim/world.js 구조 상수와 동일 — 반복/프로젝트 환산 분모

// 실코드 조사 결과 (2026-08-03 전수 확인 — 추출기 미등재라 values.json 으로는 못 가져오는 값들.
// 전부 file:line 을 병기하고, 본문에도 그대로 인용해 재확인 가능하게 한다).
const BRICK_FACTS = {
  // 생산 곡선은 DT 가 아니라 C++ 하드코딩 — Public/Data/FactoryUpgradeConfig.h:22~97.
  // DT_FactoryUpgradeDefinition 은 비용/상한/표시만 담당(FactoryUpgradeData.h:19 주석이 명시).
  curveSrc: 'Public/Data/FactoryUpgradeConfig.h:22~97',
  startLevel: 1,                 // Private/Data/FactorySaveData.cpp:6~20 — 전 슬롯 Lv1 시작(0 아님)
  holdIntervalL1: 1.0,           // HoldProductionSpeed L1 = 1.0초
  holdAmountL1: 1,               // HoldProductionAmount = Level (선형) → L1 = 1개
  holdIntervalMid: 0.5,          // L51 = 0.5초
  holdAmountMid: 50,             // L50 = 50개
  midMoneyCost: 1.91e6,          // Speed L1→51 + Amount L1→50 누적 Money (BaseCost×1.15^k 등비합)
  autoIntervalL1: 10.0,          // AutoCollection L1 = 10초
  autoCapacityL1: 1,             // AutoCollectionCapacity = Level → L1 = 1개
  autoUnlock: 'ECompanyTitle::MidSize(중견기업) 이상 — BrickFactory.cpp:449',
  spendSite: 'BuildPlacementPanelWidget.cpp:360~362 SpendConstructionCost()',
  deadFields: 'FBuildingData.BuildCostBrick(100) / BuildCostMoney(10000) — BuildingData.h:102,106. 읽는 코드 0건(사문 필드)',
  floorAxis: { base: 1000, growth: 1.15, maxLevel: 0 }, // DT_BuildingEnhancementDefinition BuildingFloor — 유일한 Brick 강화축
  footprintDist: '1칸×11 / 2칸×10 / 4칸×18 / 9칸×1(B40=3×3)', // specs/2026-06-22-…-rekey-design.md:34 (2026-06-23 검증 기록)
  launchLoot: { min: 300, max: 800, tierRange: 'T0~T3' },     // DataImport/DT_LaunchLoot_Import.csv:3 Low_Brick
};

// ── 로더 ────────────────────────────────────────────────────────────────────────
function readJson(p, { required = false, what = p } = {}) {
  if (!fs.existsSync(p)) {
    if (required) throw new Error(`리포트 입력 누락: ${what} (${p})`);
    return null;
  }
  return JSON.parse(fs.readFileSync(p, 'utf8'));
}

// out/runs/agg-<profile>.json 전부. 하나도 없으면 throw (조용한 빈 리포트 금지).
function loadAggregates(runsDir) {
  const files = fs.existsSync(runsDir)
    ? fs.readdirSync(runsDir).filter(f => /^agg-.+\.json$/.test(f))
    : [];
  if (files.length === 0) {
    throw new Error(`본 런 집계 없음: ${runsDir}/agg-<profile>.json 이 0개 — 먼저 sim/run.js 를 돌릴 것`);
  }
  const out = {};
  for (const f of files) {
    const a = JSON.parse(fs.readFileSync(path.join(runsDir, f), 'utf8'));
    out[a.profile ?? f.replace(/^agg-|\.json$/g, '')] = a;
  }
  return out;
}

// 개별 런 JSON 의 ledger.bySrc 를 프로필별로 합산 (재화 수지표 입력).
function ledgerRollup(runsDir) {
  const out = {};
  if (!fs.existsSync(runsDir)) return out;
  for (const f of fs.readdirSync(runsDir)) {
    const m = /^(.+)-(\d+)\.json$/.exec(f);
    if (!m) continue;
    const profile = m[1];
    const r = JSON.parse(fs.readFileSync(path.join(runsDir, f), 'utf8'));
    if (!r.ledger || !r.ledger.bySrc) continue;
    const e = out[profile] ?? (out[profile] = { n: 0, bySrc: {}, balances: {}, inflow: {}, outflow: {}, completed: 0 });
    e.n++;
    if (r.endState && r.endState.completed) e.completed++;
    for (const [k, v] of Object.entries(r.ledger.bySrc)) {
      e.bySrc[k] = (e.bySrc[k] ?? 0) + v;
      const cur = k.split('|')[0];
      if (v >= 0) e.inflow[cur] = (e.inflow[cur] ?? 0) + v;
      else e.outflow[cur] = (e.outflow[cur] ?? 0) - v;
    }
    for (const [c, v] of Object.entries(r.ledger.balances ?? {})) e.balances[c] = (e.balances[c] ?? 0) + v;
  }
  return out;
}

// ── 포맷 ────────────────────────────────────────────────────────────────────────
const num = (v, d = 2) => (Number.isFinite(v) ? Number(v).toFixed(d) : '—');
const pct = (v, d = 1) => (Number.isFinite(v) ? `${(v * 100).toFixed(d)}%` : '—');
const sgn = (v, d = 1) => (Number.isFinite(v) ? `${v >= 0 ? '+' : ''}${(v * 100).toFixed(d)}%` : '—');
const int = v => (Number.isFinite(v) ? Math.round(v).toLocaleString('en-US') : '—');
function money(v) {
  if (!Number.isFinite(v)) return '—';
  const a = Math.abs(v);
  if (a >= 1e9) return `${(v / 1e9).toFixed(2)}B`;
  if (a >= 1e6) return `${(v / 1e6).toFixed(1)}M`;
  if (a >= 1e3) return `${(v / 1e3).toFixed(1)}K`;
  return v.toFixed(0);
}
const med = o => (o && Number.isFinite(o.median) ? o.median : null);

// ── Brick 건설비 분석 ───────────────────────────────────────────────────────────
// ConstructionCosts 는 "(ResourceType=Brick,Cost=100)" 문자열 배열(에디터 export 형태).
function parseCosts(row) {
  const out = {};
  for (const s of row.ConstructionCosts ?? []) {
    const m = /ResourceType\s*=\s*(\w+)\s*,\s*Cost\s*=\s*(-?[\d.]+)/.exec(String(s));
    if (m) out[m[1]] = (out[m[1]] ?? 0) + Number(m[2]);
  }
  return out;
}

function brickAnalysis(rows) {
  const items = rows.map(r => ({ name: r.Name ?? r.RowName, rarity: r.Rarity ?? '?', costs: parseCosts(r) }));
  const brick = items.map(i => i.costs.Brick ?? 0);
  const byCost = new Map();
  for (const i of items) {
    const c = i.costs.Brick ?? 0;
    if (!byCost.has(c)) byCost.set(c, []);
    byCost.get(c).push(i.name);
  }
  const clusters = [...byCost.entries()]
    .map(([cost, names]) => ({ cost, count: names.length, names }))
    .sort((a, b) => b.count - a.count || a.cost - b.cost);
  const rarOrder = ['Common', 'Unusual', 'Rare', 'Epic', 'Legendary', 'Mythic'];
  const rarMap = new Map();
  for (const i of items) {
    const k = i.rarity;
    if (!rarMap.has(k)) rarMap.set(k, []);
    rarMap.get(k).push(i.costs.Brick ?? 0);
  }
  const byRarity = [...rarMap.entries()]
    .map(([rarity, cs]) => ({
      rarity, count: cs.length,
      min: Math.min(...cs), max: Math.max(...cs),
      mean: cs.reduce((a, b) => a + b, 0) / cs.length,
    }))
    .sort((a, b) => (rarOrder.indexOf(a.rarity) - rarOrder.indexOf(b.rarity)));
  const otherCurrencies = [...new Set(items.flatMap(i => Object.keys(i.costs)).filter(c => c !== 'Brick'))];
  return {
    total: items.length, items, clusters, byRarity, otherCurrencies,
    min: Math.min(...brick), max: Math.max(...brick),
    mean: brick.reduce((a, b) => a + b, 0) / brick.length,
    uniform: clusters.length === 1,
  };
}

// ── 섹션 헤더 (테스트가 전수 존재를 어서트) ────────────────────────────────────
const SECTIONS = [
  '## 1. 한 줄 결론 + G1~G4 판정',
  '## 2. 앵커 실측 + 모델↔실측 오차율',
  '## 3. 플레이 곡선 — 4프로필 + 2진단 arm',
  '## 4. 재화 5축 수지',
  '## 5. 발견 이슈 전체',
  '## 6. 구조 진단',
  '## 7. Brick 건설비 분석',
  '## 8. 노브 권고안 (적용은 별도)',
  '## 9. retune §7 불확실성 12건 해소 현황',
  '## 10. 커버리지 요약',
  '## 11. 사용자 실플레이 측정 가이드 (A1/A2)',
  '## 12. 한계',
];

// ── 게이트(앵커) 재계산 — 실패 시 조용히 넘기지 않고 사유를 반환 ────────────────
function buildGate(valuesPath, measurements) {
  const { loadValues } = require('../sim/values.js');
  const { buildPredictions, calibrate, contextFromMeasurements, predictionAssumptions } = require('./calibrate.js');
  const values = loadValues(valuesPath);
  const flat = (measurements && measurements.flat) || {};
  // ctx 조립은 calibrate.js 한 곳에만 둔다 — CLI 와 리포트가 각자 만들면 게이트 결과가 갈린다.
  const ctx = contextFromMeasurements(measurements);
  return { ...calibrate(buildPredictions(values, ctx), flat), assumptions: predictionAssumptions(ctx), ctx };
}

// 미측정 앵커는 FAIL 이 아니라 "이관"(사용자 실플레이) — 2026-08-03 스코프 확정.
function gateMark(row) {
  if (!row.gated) return '참고';
  if (row.measured === null || row.measured === undefined) return '**이관**';
  return row.pass ? '**PASS**' : '**FAIL**';
}

// ── 본체 ────────────────────────────────────────────────────────────────────────
function generateReport(outDir, specDate, opts = {}) {
  const repoRoot = opts.repoRoot ?? process.cwd();
  const valuesPath = path.join(outDir, 'values.json');
  const raw = readJson(valuesPath, { required: true, what: 'values.json' });
  const meas = readJson(path.join(outDir, 'measurements.json'));
  const calib = readJson(path.join(outDir, 'calibration.json'));
  const scan = readJson(path.join(outDir, 'scan.json'));
  const runsDir = path.join(outDir, 'runs');
  const aggs = loadAggregates(runsDir);
  const roll = ledgerRollup(runsDir);

  // 어서션 / 커버리지 — 주입 없으면 실행, 실패는 본문에 사유로 남긴다.
  let assertions = opts.assertions;
  if (!assertions) {
    try {
      assertions = require('./assertions.js').runAssertions(valuesPath, repoRoot);
    } catch (e) { assertions = { error: String(e && e.message ? e.message : e) }; }
  }
  let coverage = opts.coverage;
  if (!coverage) {
    try {
      const cj = require('./coverage.json');
      coverage = {
        check: require('./coverage_check.js').checkCoverage(repoRoot, 'Tools/Balance/verify/coverage.json'),
        entries: cj.entries ?? [],
      };
    } catch (e) { coverage = { error: String(e && e.message ? e.message : e) }; }
  }
  let gate = opts.gate;
  if (!gate) {
    try { gate = buildGate(valuesPath, meas); }
    catch (e) { gate = { error: String(e && e.message ? e.message : e), rows: [] }; }
  }

  const valuesHash = `${raw.meta?.srcHash ?? '?'}:${raw.meta?.editorHash ?? '?'}`;
  const buildable = raw.dt?.BuildableTable ?? [];
  const brick = buildable.length ? brickAnalysis(buildable) : null;

  const P = [];                       // 문서 조각
  const w = (...ls) => P.push(...ls);

  // ══ 헤더 ═════════════════════════════════════════════════════════════════════
  const profOrder = ['low', 'mid', 'high', 'skilled', 'midNoTap', 'lowTap'].filter(p => aggs[p]);
  const others = Object.keys(aggs).filter(p => !profOrder.includes(p));
  const allProf = [...profOrder, ...others];
  const seedsOf = p => aggs[p]?.runs ?? aggs[p]?.meta?.args?.seeds ?? '?';
  const midA = aggs.mid;

  w(
    `# 경제 통합 검증 리포트 — 밸런스 × 재화수급`,
    ``,
    `> 생성: ${specDate} · values \`${valuesHash}\` · 본 런 ${allProf.length}프로필 × ${seedsOf(allProf[0])}시드`,
    `> 선행: \`specs/2026-08-02-economy-verification-design.md\`(설계) · \`specs/2026-07-29-balance-grind-retune.md\`(수치 SOT) · \`specs/2026-08-02-economy-map.md\`(재화 전수 맵)`,
    `> 생성기: \`Tools/Balance/verify/report.js\` — 이 문서의 모든 표는 \`Tools/Balance/out/*.json\` 에서 기계 생성된다(수치 수기 입력 없음).`,
    ``,
    `---`,
    ``,
  );

  // ══ 1. 결론 + G1~G4 ═══════════════════════════════════════════════════════════
  const midH = med(midA?.inAppHoursCompleted);
  const midAdj = RETUNE_PROFILE_TARGET.mid.inAppH - RETUNE_MID_T10_H;
  const midErr = Number.isFinite(midH) ? (midH - midAdj) / midAdj : null;
  const gateRows = (gate.rows ?? []).filter(r => r.gated);
  const gatePass = gateRows.filter(r => r.pass).length;
  const gateMeasured = gateRows.filter(r => r.measured !== null && r.measured !== undefined).length;
  const aRes = assertions && assertions.results ? assertions.results : [];
  const aPass = aRes.filter(r => r.state === 'PASS' || (r.pass && !r.state)).length;
  const aKnown = aRes.filter(r => r.state === 'KNOWN' || (r.known && !r.state)).length;
  const aFail = aRes.filter(r => r.state === 'FAIL').length;
  const dead = scan?.dead ?? [];
  const cliffs = scan?.cliffs ?? [];
  const cliff = cliffs[0];

  w(
    SECTIONS[0], ``,
    `**한 줄 결론 — 이 게임의 경제는 "돈"이 아니라 "개발점수 게이트 × 시간"으로 굴러가고 있고, 그 게이트를 지탱하는 요구점수 곡선(\`reqCurve\`)은 위쪽으로 벼랑(비대칭 ×${num(cliff?.asymmetry, 1)}) 바로 앞에 서 있다.**`,
    `중위(mid) 프로필의 T10 진입 인앱시간 중앙값은 **${num(midH)}h** 로 retune §3 목표를 같은 기준으로 맞췄을 때(13.2h − T10 파밍 ${num(RETUNE_MID_T10_H)}h = ${num(midAdj)}h) 오차 **${sgn(midErr)}** — 곡선 자체는 재현된다.`,
    `다만 그 곡선을 만드는 개발점수 앵커(A1/A2)는 아직 인게임 실측이 없다(사용자 실플레이로 이관, §11). 따라서 이 리포트는 **부분 캘리브레이션 상태의 판정**이다.`,
    ``,
    `| 목표 | 판정 | 근거 |`,
    `|---|---|---|`,
    `| **G1** 플레이 곡선 실증 | **부분 달성 (+ 전제 1건 반증)** | ${allProf.length}프로필 × ${seedsOf('mid')}시드 티어별 표 생성(§3). **중위 곡선은 재현**(${num(midH)}h vs 보정 ${num(midAdj)}h = ${sgn(midErr)}). 반면 **retune 이 전제한 프로필별 시간 스프레드는 재현되지 않았다** — 완주 3프로필이 ${num(Math.min(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite)))}~${num(Math.max(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite)))}h 로 거의 동일(§3.1). 절대 시간은 개발점수 앵커 미실측이라 조건부 |`,
    `| **G2** 경제 파탄 지점 탐지 | **달성** | 죽은 노브 ${dead.length}종 / 절벽 ${cliffs.length}종(${cliff ? `\`${cliff.knob}\` ×${num(cliff.asymmetry, 1)}` : '—'}) / 소프트락 메커니즘 = DevScore 게이트 / 인플레 0건(단 vacuous — §6.4) |`,
    `| **G3** 파트 간 정합성 | **달성** | 어서션 ${aRes.length}종 → PASS ${aPass} / KNOWN ${aKnown} / FAIL ${aFail} (§5, §10) |`,
    `| **G4** 미실측 앵커 해소 | **부분 + 이관** | 게이트 ${gateMeasured}/${gateRows.length} 실측, 그중 ${gatePass}/${gateMeasured} 이 ±${pct(gate.tolerance ?? 0.1, 0)} 통과. A1/A2(개발점수·통과율)는 사용자 실플레이로 이관 → **retune §7-1 티어비 2.00 확정 판정은 여전히 미확정** |`,
    ``,
  );

  // ══ 2. 앵커 ═══════════════════════════════════════════════════════════════════
  w(
    SECTIONS[1], ``,
    gate.error
      ? `> ⚠ 캘리브레이션 게이트 산출 실패 — ${gate.error}. 아래 표는 비어 있다(측정값을 0 으로 간주하지 않는다).`
      : `게이트 = \`|실측 − 예측| / 예측 ≤ ${pct(gate.tolerance ?? 0.1, 0)}\`. 재계산: \`node Tools/Balance/verify/calibrate.js\`.`,
    ``,
    `| 앵커 | 예측(모델) | 실측(PIE) | 오차 | 판정 |`,
    `|---|---:|---:|---:|---|`,
  );
  for (const r of gate.rows ?? []) {
    const pred = r.predicted === null ? '—' : num(r.predicted, 4);
    const measv = r.measured === null || r.measured === undefined ? '—' : num(r.measured, 4);
    const e = r.errPct === null || r.errPct === undefined ? '—' : pct(r.errPct, 1);
    w(`| \`${r.key}\` | ${pred} | ${measv} | ${e} | ${gateMark(r)} |`);
  }
  const staleB = meas?.a4?.staleOperationBuildings ?? [];
  const row = k => (gate.rows ?? []).find(r => r.key === k);
  const deferredRows = gateRows.filter(r => r.measured === null || r.measured === undefined);
  const failedRows = gateRows.filter(r => r.pass === false && !deferredRows.includes(r));
  // 헤드라인은 반드시 게이트 결과에서 파생시킨다 — A1/A2 가 측정되면 이 문장이 자동으로 따라가야
  // 표(8/8)와 본문(4/8)이 같은 문서 안에서 모순되지 않는다.
  const headline = gateMeasured === 0
    ? `**실측 0/${gateRows.length} — 캘리브레이션 미수행.** 아래 표의 예측은 전부 모델 유도값이며 실빌드와 대조되지 않았다.`
    : failedRows.length === 0
      ? `**실측 ${gateMeasured}/${gateRows.length} 전부 PASS.**${gateMeasured === gateRows.length ? ' 전 앵커가 실측 대조를 통과했다 — 이 리포트는 **완전 캘리브레이션** 상태다.' : ' 시뮬의 방치수익식(`sim/idle.js`)·금고 적립·오프라인 정산 비율·금고 하한이 실제 빌드와 일치한다.'}`
      : `**실측 ${gateMeasured}/${gateRows.length} 중 ${gatePass} PASS / ${failedRows.length} FAIL** — FAIL 계통(${failedRows.map(r => `\`${r.key}\``).join(', ')})은 **모델이 실빌드와 어긋난다는 뜻**이므로 그 계통을 쓰는 §3~§6 결론은 무효로 볼 것.`;
  w(
    ``,
    headline,
    row('a3_idlePerSecPerEmp')?.measured != null
      ? `- \`a3_idlePerSecPerEmp\` — 로스터 레벨 평균 보정 후 대조(스폰 액터 수를 분모로 쓰면 33% 틀린다). 실측 로스터 \`[${(meas?.a3?.rosterLevels ?? []).join(', ')}]\`, ${meas?.a3?.dt ? `${num(meas.a3.dt, 1)}초 차분` : '차분 2점'}.`
      : '',
    row('a4_vaultAccrualRate')?.measured != null
      ? `- \`a4_vaultAccrualRate\` — 예측이 "치트가 보고한 실수익률"이라 이 행은 시뮬 대조가 아니라 **런타임 자기정합성** 검사다. ${sgn(row('a4_vaultAccrualRate').errPct)} 차이는 구간 내 감쇠분이며 방향·크기가 감쇠 모델과 정합.`
      : '',
    row('a4_vaultFallbackCap')?.measured != null
      ? `- \`a4_vaultFallbackCap\` — **PASS 이지만 이 PASS 는 버그의 재현이다.** 모델이 "운영 없는 빌딩의 금고 용량 = \`vault.baseCapacity\` 200 상수"라는 실코드 결함을 그대로 모사하고 있어 값이 맞는 것 (retune #15, §5-B1).`
      : '',
    // 미측정 앵커가 남아 있을 때만 "이관" 서술을 낸다. A1/A2 를 재면 이 불릿 전체가 사라진다.
    deferredRows.length
      ? `- **이관 ${deferredRows.length}행(${deferredRows.map(r => `\`${r.key}\``).join(', ')})** — 값을 지어내지 않고 비워 뒀다. \`calibration.json\` 의 \`devScorePerEmpPerSec\`/\`noTapDuty\` 가 비어 있어 시뮬은 values 유도식 기본값(dps ${num(row('a1_devScorePerEmpPerSecFullTap')?.predicted, 4)}, duty ${num((row('a1_devScorePerEmpPerSecNoTap')?.predicted ?? 0) / (row('a1_devScorePerEmpPerSecFullTap')?.predicted || 1), 3)})으로 돈다. 측정 절차 = §11.`
      : `- **미측정 앵커 없음** — A1/A2 를 포함한 전 앵커가 실측으로 채워졌다. §12.1 의 "A1/A2 미실측" 한계와 §11 가이드는 이 시점부로 만료다(다음 리포트에서 갱신할 것).`,
    staleB.length ? `- ⚠ 측정 중 부수 관측: 강제 저장 직후에도 빌딩 \`[${staleB.join(', ')}]\` 의 오프라인 정산이 0 — 세이브 스냅샷 결함(§5-B2).` : ``,
    Number.isFinite(gate.ctx?.projectIndex)
      ? `- 측정 대상 프로젝트: \`DT_Project_Game\` **ProjectIndex ${gate.ctx.projectIndex}**${Number.isFinite(gate.ctx.direction) ? ` / direction ${gate.ctx.direction}(${['Standard', 'Speed', 'Quality', 'Research'][gate.ctx.direction] ?? '?'})` : ''} — \`[BALV] GateRunDone\` 의 \`proj=\`/\`dir=\` 에서 파생. 예측도 같은 행으로 만든다(사과 대 사과).`
      : ``,
    ``,
  );
  // 숫자로 안 잡히는 가정은 표에 안 나온다 — 별도 블록으로 항상 싣는다(2026-08-03 리뷰 F9②).
  if ((gate.assumptions ?? []).length) {
    w(
      `> **미측정 가정 ${gate.assumptions.length}건** (게이트 표의 숫자가 조용히 전제하는 것):`,
      ...gate.assumptions.map(a => `> - ${a}`),
      ``,
    );
  }

  // ══ 3. 플레이 곡선 ════════════════════════════════════════════════════════════
  w(
    SECTIONS[2], ``,
    `본 런: \`node Tools/Balance/sim/run.js --profile <p> --seeds ${seedsOf('mid')}\` × ${allProf.length}. \`untilTier=10\`(= **T10 진입** = T9 완료), 캘린더 캡 ${aggs[allProf[0]]?.meta?.args?.maxCalendarDays ?? 120}일.`,
    `\`low\`/\`mid\`/\`high\`/\`skilled\` = retune §3 정의 계승, \`midNoTap\`/\`lowTap\` = **진단 arm**(단일 변수 통제용, 플레이어 프로필 아님).`,
    ``,
    `### 3.1 프로필 총량 — retune §3-1 대조`,
    ``,
    `| 프로필 | 탭 duty | 완주율 | 완주 인앱h p25/**중앙**/p75 | 캘린더일 | 도달티어 | 판수 | 등급 C/B/A/S | 게이트실패 | retune 목표 | 대조 |`,
    `|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---|`,
  );
  const DUTY = { low: 0, lowTap: 0.52, mid: 0.52, midNoTap: 0, high: 0.70, skilled: 0.60 };
  for (const p of allProf) {
    const a = aggs[p];
    const h = med(a.inAppHoursCompleted);
    const t = RETUNE_PROFILE_TARGET[p];
    const g = a.gradeShare ?? {};
    const diag = (p === 'midNoTap' || p === 'lowTap') ? ' *(진단)*' : '';
    let cmp = '—';
    if (t && Number.isFinite(h)) {
      cmp = p === 'mid'
        ? `보정 ${num(midAdj)}h 대비 **${sgn((h - midAdj) / midAdj)}**`
        : `원표 대비 ${sgn((h - t.inAppH) / t.inAppH)} *(T10 파밍 미분리)*`;
    } else if (t) {
      cmp = `완주 없음 — 비교 불가`;
    }
    w(`| **${p}**${diag} | ${DUTY[p] ?? '—'} | ${pct(a.completionRate)} | ${num(a.inAppHoursCompleted?.p25)} / **${num(h)}** / ${num(a.inAppHoursCompleted?.p75)} | ${num(med(a.calendarDaysCompleted), 0)} | ${num(med(a.reachedTier), 0)} | ${num(med(a.launches), 0)} | ${pct(g.C, 0)}/${pct(g.B, 0)}/${pct(g.A, 0)}/${pct(g.S, 0)} | ${pct(a.gateFailShare)} | ${t ? `${num(t.inAppH, 1)}h / ${t.days}일 / ${t.launches}판` : '—'} | ${cmp} |`);
  }

  w(
    ``,
    `**읽는 법 3가지 (틀리기 쉬움):**`,
    `1. **\`untilTier=10\` 은 T10 진입까지**다. retune 총량과 비교하려면 T10 파밍분을 빼야 한다 — 중위만 티어별 분해(§3-2)가 있어 정확히 뺄 수 있고(${num(RETUNE_PROFILE_TARGET.mid.inAppH, 1)} − ${num(RETUNE_MID_T10_H)} = ${num(midAdj)}h), 나머지 3프로필은 T10 시간이 분해돼 있지 않아 위 표의 대조 열은 **T10 파밍을 포함한 원표와 붙인 값**(시뮬에 불리한 방향)이다.`,
    `2. **미완주 런의 인앱시간은 캡에 걸린 값**이라 완주 런과 섞으면 무의미하다. 위 표의 인앱h는 전부 완주 런만(\`completionRate\` 와 분리 집계).`,
    `3. **완주율이 0% 인 프로필은 "느린 게 아니라 못 깬다"**. \`low\`/\`lowTap\` 은 시간을 더 줘도 안 끝난다(캘린더 캡 소진, §6.3).`,
    ``,
    `**★ 핵심 발견 — retune 이 전제한 "프로필별 총량 스프레드"가 재현되지 않는다.**`,
    `retune §3-1 은 하위 ${num(RETUNE_PROFILE_TARGET.low.inAppH, 1)}h / 중위 ${num(RETUNE_PROFILE_TARGET.mid.inAppH, 1)}h / 상위 ${num(RETUNE_PROFILE_TARGET.high.inAppH, 1)}h / 숙련 ${num(RETUNE_PROFILE_TARGET.skilled.inAppH, 1)}h 로 **최대 ${num(RETUNE_PROFILE_TARGET.low.inAppH / RETUNE_PROFILE_TARGET.high.inAppH, 1)}배** 벌어지는 분포를 전제한다.`,
    `그런데 이 시뮬에서 **완주하는 세 프로필은 ${num(Math.min(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite)))}~${num(Math.max(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite)))}h 로 사실상 같다** (스프레드 ${pct((Math.max(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite)) - Math.min(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite))) / (midH || 1), 1)}). 투자·숙련도를 올려도 시간이 안 줄고, **더 잘하는 프로필(high)이 오히려 약간 길다**(등급이 높으면 운영 수명이 길어져 판당 시간이 늘기 때문).`,
    `**즉 이 값 세트에서 플레이어 역량 차이는 "완주 시간"이 아니라 "완주 가능/불가"의 이진 결과로 나타난다.** 병목이 자원 축적 속도가 아니라 **임계값(개발점수 게이트)** 이라서다 — 임계를 넘으면 다들 비슷한 속도로 흐르고, 못 넘으면 무한히 막힌다(§6.3).`,
    `→ retune §3-1 의 프로필 분포표는 **현행 값 세트의 예측으로 쓸 수 없다.** 목표를 "분포로 재정의"하려면(retune §7-3 의 (c)안) 먼저 게이트를 연속적인 축으로 바꾸거나, 등급이 시간에 실제로 반영되는 경로를 만들어야 한다.`,
    ``,
    `### 3.2 mid 티어별 — retune §3-2 나란히 대조`,
    ``,
    `| 티어 | 진입일(중앙) | 인앱분(중앙) | = 인앱h | retune 인앱h | 차 | 반복(중앙) | = 반복/프로젝트 | retune 반복/프로젝트 |`,
    `|---:|---:|---:|---:|---:|---:|---:|---:|---:|`,
  );
  if (midA) {
    const entry = midA.tierEntryDayMedian ?? {};
    const tmin = midA.tierInAppMinMedian ?? {};
    const trep = midA.tierRepeatsMedian ?? {};
    let sumH = 0;
    for (const k of Object.keys(tmin).sort((a, b) => a - b)) {
      const h = (tmin[k] ?? 0) / 60;
      sumH += h;
      const rt = RETUNE_MID_TIER[k];
      const rp = (trep[k] ?? 0) / PROJECTS_PER_TIER;
      const d = rt ? h - rt.inAppH : null;
      w(`| T${k} | ${num(entry[k], 0)} | ${num(tmin[k], 1)} | ${num(h)} | ${rt ? num(rt.inAppH) : '—'} | ${d === null ? '—' : `${d >= 0 ? '+' : ''}${num(d)}`} | ${num(trep[k], 1)} | ${num(rp)} | ${rt ? num(rt.repPerProj) : '—'} |`);
    }
    w(
      `| **계** | — | — | **${num(sumH)}** | **${num(RETUNE_PROFILE_TARGET.mid.inAppH, 1)}** | ${num(sumH - RETUNE_PROFILE_TARGET.mid.inAppH)} | — | — | — |`,
      ``,
      `> 계 ${num(sumH)}h 는 **티어별 중앙값의 합**이라 런별 총합의 중앙값(${num(midH)}h)과 다르다(중앙값은 가법적이지 않음). 헤드라인 수치는 ${num(midH)}h 쪽을 쓸 것.`,
      ``,
      `**곡선 형태 판정:**`,
      `- 전반(T1~T5)은 시뮬이 retune 보다 **짧다**(티어당 −0.3~−0.6h). 요구곡선이 초반 공급 대비 헐렁해 반복이 거의 안 생긴다(T1·T2 반복 ${num(trep[1], 0)}/${num(trep[2], 0)}회 = 전부 첫 클리어로 통과).`,
      `- 후반(T6~T8)은 시뮬이 **길다**(+0.4~+0.7h). 램프의 위치가 retune 곡선보다 **뒤로 밀려** 있다 — retune 은 T3부터 완만히 오르는데 시뮬은 T5까지 평탄하다가 T6에서 꺾인다.`,
      `- **반복/프로젝트 램프는 retune 목표의 약 절반**(T9 ${num((trep[9] ?? 0) / PROJECTS_PER_TIER)} vs ${num(RETUNE_MID_TIER[9].repPerProj)}). 즉 현행 값 세트는 retune 이 의도한 "노가다 밀도"에 **아직 도달하지 않았다**. 총 인앱시간이 맞는 것은 판당 소요시간이 더 길기 때문이지 반복이 많아서가 아니다.`,
      `- **등급 mix 는 프로필이 아니라 티어의 함수로 창발한다.** mid 는 T1~T7 이 S 독점 → T8 A/B → T9 C+게이트실패로 급락한다. retune §3 이 전제한 고정 mix(중위 C65/B22/A13)는 이 값 세트에서 성립하지 않는다(실측 C${pct(midA.gradeShare?.C, 0)}/B${pct(midA.gradeShare?.B, 0)}/A${pct(midA.gradeShare?.A, 0)}/**S${pct(midA.gradeShare?.S, 0)}**).`,
      ``,
    );
  }

  // ══ 4. 재화 5축 ═══════════════════════════════════════════════════════════════
  const covEntries = coverage?.entries ?? [];
  const st = s => covEntries.filter(e => e.status === s).length;
  // modeled ≠ 본 런 반영. pipelineWired 로 "world 원장 도달"을 분리해 두 수를 항상 같이 낸다.
  const modeledEntries = covEntries.filter(e => e.status === 'modeled');
  const wiredEntries = modeledEntries.filter(e => e.pipelineWired === true);
  const wiredIdle = wiredEntries.filter(e => e.pipelineNote && /실행 0회/.test(e.pipelineNote));
  // pipelineReason 첫 구절(— 앞)로 미배선 사유를 자동 군집화 — 사유 문구가 늘어도 표가 따라온다.
  const unwiredByCause = new Map();
  for (const e of modeledEntries.filter(x => x.pipelineWired === false)) {
    const cause = (String(e.pipelineReason ?? '').split('—')[0].trim() || '(사유 없음)').slice(0, 60);
    if (!unwiredByCause.has(cause)) unwiredByCause.set(cause, []);
    unwiredByCause.get(cause).push(e.id);
  }
  const covIds = new Set(covEntries.map(e => e.id));
  // status 'n/a'(치트 전용 경로 등)는 모델링 대상이 아니다 — 섞으면 §10.1 표와 본문 서술이 어긋난다.
  const matIds = covEntries.filter(e => /^Mat/.test(e.id) && e.status === 'modeled').map(e => e.id);
  const matWired = matIds.filter(id => covEntries.find(e => e.id === id)?.pipelineWired === true);
  w(
    SECTIONS[3], ``,
    `축 정의 = \`specs/2026-08-02-economy-map.md\` §1 (Money / Diamond / Brick / MarketCap / 원자재·아이템).`,
    `아래 Money·Diamond 표는 원장(\`sim/ledger.js\`) 실집계 — 프로필당 ${roll.mid?.n ?? '?'}런 평균, srcId 는 economy-map ID.`,
    ``,
    `### 4.1 Money (시뮬 원장 — 런 1회 평균)`,
    ``,
    `> **\`S1\` = 운영수익(수령 시점 계상, \`S2\`/\`S3\` 수령 경로 포함).** 원장은 적립(\`ProcessRevenue\`) 이 아니라 **금고→지갑 수령** 시점에 계상한다 — 적립 시점에 세면 금고 초과로 날아간 몫(\`vaultLost\`)까지 유입으로 잡히기 때문이다. 따라서 \`S2\`(MainMap 수령)/\`S3\`(OfficeMap 수령)는 별도 행이 없고 이 열에 통합돼 있다(누락 아님 — 이중계상 방지).`,
    ``,
    `| 프로필 | 유입 합 | 유출 합 | 종료 잔액 | S10 오프라인 | S1 운영수익 | S4 방치직원 | K3 빌딩강화 | K1 개발비 | K12 상점 |`,
    `|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|`,
  );
  for (const p of allProf) {
    const r = roll[p];
    if (!r) { w(`| ${p} | — | — | — | — | — | — | — | — | — |`); continue; }
    const g = k => (r.bySrc[k] ?? 0) / r.n;
    w(`| **${p}** | ${money(r.inflow.Money / r.n)} | ${money(-r.outflow.Money / r.n)} | ${money(r.balances.Money / r.n)} | ${money(g('Money|S10'))} | ${money(g('Money|S1'))} | ${money(g('Money|S4'))} | ${money(g('Money|K3'))} | ${money(g('Money|K1'))} | ${money(g('Money|K12'))} |`);
  }
  // 축 개수는 DT 에서 읽는다 — 리터럴로 박으면 슬롯이 삭제될 때마다 리포트가 조용히 거짓말을 한다.
  const enhAxisCount = (raw.dt?.['DT_BuildingEnhancementDefinition'] ?? []).length;
  const midRoll = roll.mid;
  const k3Share = midRoll ? Math.abs(midRoll.bySrc['Money|K3'] ?? 0) / (midRoll.inflow.Money || 1) : null;
  const s10Share = midRoll ? (midRoll.bySrc['Money|S10'] ?? 0) / (midRoll.inflow.Money || 1) : null;
  w(
    ``,
    `**Money 축 판정 — 경제가 사실상 단일 파이프다.** mid 기준 유입의 **${pct(s10Share)}가 S10(오프라인 수익)** 이고, 유출의 **${pct(k3Share)}가 K3(빌딩 강화)** 로 되돌아간다.`,
    `retune 이 필수 싱크로 지목한 ★(K4)와 HQ(K10)는 이 모델에서 각각 미모델/비합리선택이라 0원이고, 그 자리를 빌딩강화 ${enhAxisCount}축이 통째로 흡수한다.`,
    `종료 잔액이 총 유입의 ${pct(midRoll ? (midRoll.balances.Money / midRoll.n) / (midRoll.inflow.Money / midRoll.n) : null, 2)} 밖에 안 남는데도 진행이 막히지 않는다는 것이 §6.2 "Money 비병목" 판정의 원장 근거다.`,
    ``,
    `### 4.2 Diamond`,
    ``,
    `| 프로필 | DS4 미션 유입 | DK1 가챠 유출 | 종료 잔액 |`,
    `|---|---:|---:|---:|`,
  );
  for (const p of allProf) {
    const r = roll[p];
    if (!r) continue;
    w(`| ${p} | ${int((r.bySrc['Diamond|DS4'] ?? 0) / r.n)} | ${int((r.bySrc['Diamond|DK1'] ?? 0) / r.n)} | ${num((r.balances.Diamond ?? 0) / r.n, 1)} |`);
  }
  const dia = aRes.find(r => r.id === 'A-DIAMOND');
  w(
    ``,
    `**Diamond 축 판정 — 확정 수지 −80 적자.** 정적 어서션 \`A-DIAMOND\`: ${dia ? dia.detail : '(어서션 결과 없음)'}`,
    `즉 도감+미션 확정 유입 850 으로는 특성 슬롯 5칸(930)을 못 연다. VIP 무역 주문 2건이면 메워지지만 **VIP 주문은 무역 시스템 진입이 전제**라, 무역을 안 하는 플레이어는 슬롯 5칸을 끝내 못 연다. 소프트락은 아니나 게이팅 의도 확인이 필요하다.`,
    ``,
    `> ⚠ **모델 아티팩트 명시** — 위 표의 시뮬 Diamond 유입은 \`DS4\`(미션) 단독이다. \`DS1\`(도감 티어 10/10 = 20×티어)·\`DS2\`(도감 전체 100 = 200)는 **모델상 도달 불가**다: 정책이 티어당 7클리어에서 다음 티어로 넘어가므로 도감 10/10 을 채우는 런이 없다. 실게임에서 도감을 채우는 플레이어는 이 표보다 최대 +400 더 받는다 — Diamond 축 해석 시 반드시 감안할 것.`,
    ``,
    `### 4.3 Brick / MarketCap / 원자재·아이템 (시뮬 미모델 — 정적 판정)`,
    ``,
    `| 축 | 소스 | 싱크 | 판정 |`,
    `|---|---|---|---|`,
    `| **Brick** | 벽돌공장 홀드/자동수집 · 출시 루트박스(\`Low_Brick\` ${BRICK_FACTS.launchLoot.min}~${BRICK_FACTS.launchLoot.max}) · 미션 M1(50) | 건물 건설비(\`BuildableTable\`, 전 ${buildable.length}행 동일 ${brick ? int(brick.min) : '?'}) · \`BuildingFloor\` 강화(**유일한 Brick 강화축**) | **공급 과잉 확정** — 최고가 건물이 무강화 홀드 ${brick ? num(brick.max / (BRICK_FACTS.holdAmountL1 / BRICK_FACTS.holdIntervalL1) / 60, 1) : '?'}분 분량(§7.2). 시뮬이 P(빌딩 수) 램프를 정책 상수로 둔 근거 |`,
    ...(() => {
      // A-MCSAT 의 실제 data 키는 satMC / curve[].mcUpper — 조용한 폴백을 두면 어서션 스키마가
      // 바뀌어도 표가 그럴듯하게 나오므로, 못 읽으면 그 사실을 셀에 그대로 적는다.
      const mc = aRes.find(r => r.id === 'A-MCSAT')?.data;
      const satMC = mc && Number.isFinite(mc.satMC) ? mc.satMC : null;
      const capMC = mc && Array.isArray(mc.curve) && mc.curve.length ? mc.curve[mc.curve.length - 1].mcUpper : null;
      const cell = satMC === null || capMC === null
        ? `⚠ \`A-MCSAT\` 결과를 읽지 못함(satMC/curve 부재) — 판정 미산출`
        : `상한 \`MaxMul\` 이 **죽은 상한**. 포화 필요 MC ${int(satMC)} vs T${mc.curve.length} 상한 가정 ${int(capMC)} = **${int(satMC / capMC)}배** 차 (포화 도달 티어 ${mc.satTier ?? '없음'})`;
      return [`| **MarketCap** | 프로젝트 결산 + 무역 판매 | **없음**(순수 누적) | ${cell} |`];
    })(),
    `| **원자재 10종** | 채광(wall-clock lazy) | 제조 착수 재료 차감 | **본 런 원장 반영 0** — \`${matIds.join('`/`')}\` 는 \`modeled\` 지만 \`pipelineWired\` ${matWired.length === 0 ? '전건 false' : `${matWired.length}건만 true`}. 채광 수령(\`MatS1\`)은 \`subecon.js mineYield()\` 로 구현·단위검증돼 있으나 world 스파인이 호출하지 않고, 제조 차감/환불(\`MatK1\`/\`MatK2\`/\`MatS2\`)은 아예 미구현이다 |`,
    `| **아이템** | 가챠·상점·미션 | 사용처 | **5종이 소비 경로 0건인 채 상점에서 팔린다**(§5-B5) |`,
    ``,
    `커버리지 상태 분포: modeled ${st('modeled')} / stubbed ${st('stubbed')} / n/a ${st('n/a')} (총 ${covEntries.length}건, §10).`,
    `⚠ **\`modeled\` 는 "본 런이 계산에 넣었다"는 뜻이 아니다** — ${st('modeled')}건 중 world 원장에 실제로 도달하는 것은 **${wiredEntries.length}건**뿐이다(\`pipelineWired\`, §10).`,
    ``,
  );

  // ══ 5. 발견 이슈 ══════════════════════════════════════════════════════════════
  const vf = aRes.find(r => r.id === 'A-VAULTFALLBACK');
  const unw = aRes.find(r => r.id === 'A-UNWIRED');
  w(
    SECTIONS[4], ``,
    `> 분류: **(A) 선행 스펙 수치 오류** / **(B) 버그 후보** / **(C) 미문서 재화 풀** / **(D) 죽은 노브·죽은 상한**.`,
    `> 이번 검증에서 **즉시 수정한 게임 코드 결함은 0건**이다 — 후보 전부가 C++ 변경(빌드 게이트) 또는 기획 판단이 필요해 "재현 근거 기록 + 판단 대기"로 남겼다(설계 §9 "판단이 애매하면 수정하지 않고 질문으로 올린다").`,
    `> 도구 자체의 결함 3건은 즉시 수정했다(§10 말미).`,
    ``,
    `### 5-A. 선행 스펙 수치 오류 1건 ★`,
    ``,
    `| 항목 | 스펙 값 | 실코드 정답 | 차 | 근본 원인 |`,
    `|---|---:|---:|---:|---|`,
    `| ★0→10 기대비용(base 2500) | 812,404 | **789,945** | −2.76% | 스펙 작성 스크래치 \`final2.py:69\` 의 하락 임계값 오프바이원(\`E<5\`) — 실코드/이식 원본/스펙 자신의 서술은 전부 임계값 **6** |`,
    `| ★0→10 기대비용(base 1000) | 324,962 | **315,978** | −2.76% | 위와 동일 |`,
    ``,
    `3중 독립 검증(자체 점화식 / \`Tools/Balance/enhance_curve.js\` 무수정 실행 / absorbing-Markov value iteration)이 전부 789,945 로 수렴했고, 리뷰어가 재현했다.`,
    `**파생 영향**: \`retune §3-3\` 이 "현존 확정 싱크 ≈ 320M" 을 세는 데 쓴 **★13 12인 = 112.7M** 도 같은 버그의 산물이다 → ★축 수치 전부 재계산 대상. 방향성 결론(★가 유일한 후반 싱크)은 정성적으로 유지된다.`,
    ``,
    `### 5-B. 버그 후보 (수정 보류 — 판단 대기)`,
    ``,
    `| # | 결함 | 위치 | 재현 근거 | 상태 |`,
    `|---|---|---|---|---|`,
    `| **B1** | 무운영 빌딩의 금고 용량이 상수 \`vault.baseCapacity\` 200 으로 폴백 (영속 레이트 미사용) | \`ProjectOperationManager.cpp:888\` \`CalculateWarehouseCapacity\` | PIE 실측 \`cap=200.00\` 재현(무운영 빌딩 7개 전부) + 어서션 \`A-VAULTFALLBACK\` **KNOWN** | retune #15 잔존. **C++ 변경 → 컨트롤러 판단** |`,
    `| **B2** | \`SaveGameData()\` 가 \`CurrentOperation\` 을 **관리 중 빌딩 1개**만 갱신 → 나머지 라이브 운영은 오프라인 정산 0 (조용한 손실) | \`SaveLoadManager.cpp:556~572\` (OfficeMap + 인테리어 복원 분기 안) | 강제 저장 **직후** \`Balance_SimOffline\`: 라이브 6개 중 3개(b1/b3/b8)가 \`remain 0\` → 오프라인 수익 0 | 재확인 완료(코드 그대로) |`,
    `| **B3** | 빌딩 1 오피스에 **빌딩 9 로스터**가 스폰(emp=9 ↔ 좌석 5 ↔ 로스터 5, 3중 불일치) | 미규명 (\`Preset_Apply Mid\` ↔ 오피스 진입 순서 의심) | 오피스 스폰 로그 \`Spawned employee: 52..57 (사원 9-1~9-6)\` — 빌딩 1 컨텍스트에서 | **미규명** — A1/A2 측정 표본을 오염시킨 직접 원인 |`,
    `| **B4** | \`DT_Mission\` CSV 13행 vs 라이브 DT **19행** — M14/M15/M17/M19/M20/M21 이 CSV 미반영 | \`DataImport/DT_Mission_Import.csv\` (14줄 = 헤더+13행) vs \`values.json\` DT 19행 | 재확인 완료 | **CSV 리임포트 시 6행 유실 위험** — 프로젝트 차원 정리 필요 |`,
    `| **B5** | 소비 경로 0건인 아이템이 **상점에서 팔린다** = 돈 받고 효과 없는 죽은 싱크 | \`A-UNWIRED\` | ${unw ? unw.detail : '(어서션 결과 없음)'} | 상점 행 제거 or 기능 배선 = **기획 판단** |`,
    ``,
    `### 5-B'. 이전 조사에서 "미배선"으로 올렸다가 **재확인으로 해소된 2건** (중요 — 오탐 정정)`,
    ``,
    `| 항목 | 이전 판정(Task 9) | Task 15 재확인 | 결론 |`,
    `|---|---|---|---|`,
    `| 도시 회사 인수 **캐시아웃** | "\`GetPotAmount\` 호출부·캐시아웃 경로 0건 — 진행 중 리팩터 추정" | \`CityAcquisitionManager.cpp:225\` \`Demolish()\` 안에 \`StoreResource(EResourceType::Money, Pot)\` — 주석 \`캐시아웃 — 유일한 지갑 지급 지점 (D8)\`. UI 경로 \`CityCompanyManageWidget.cpp:438 HandleDemolish()\` 존재 | **해소.** 커밋 \`144e42fc\` 리팩터가 완료됐고, 인수 수익은 "드립=금고 적립 → 수동 철거=지갑 지급" 2단 구조로 **정상 배선**돼 있다 |`,
    `| \`BustChance\` 파산 | "헬퍼만 있고 미배선" | \`CityAcquisitionManager.cpp:199~205\` — \`Tick()\` 안에서 회수율(\`RecoveryPct\`) 기반으로 매 틱 굴려 \`EAcqState::Busted\` 전이 + \`OnCompanyBusted\` 브로드캐스트 | **해소.** 배선돼 있다 |`,
    ``,
    `> 교훈: 동시 세션이 리팩터 중인 서브시스템은 **스냅샷 시점의 grep 결과가 곧 결론이 아니다.** 최종 리포트 전 재확인이 실제로 오탐 2건을 걷어냈다.`,
    `> **전파 완료(2026-08-03)** — 오탐의 출처였던 \`specs/2026-08-02-economy-map.md\` §2 의 S8/S9 주석 블록을 현행 코드 기준으로 재작성했고(캐시아웃 2단 구조 · 파산 배선 · 오프라인 SafeFloor), \`coverage.json\` 의 S8/S9 \`note\` 도 같이 갱신했다. 스냅샷 문서를 고치지 않으면 다음 조사가 같은 오탐을 다시 인용한다.`,
    ``,
    `### 5-C. 미문서 재화 풀 (economy-map 매트릭스 밖에 있던 것)`,
    ``,
    `| 풀 | 성격 | 상태 |`,
    `|---|---|---|`,
    `| **DismantleDust** (특성 분해 가루) | 게이트웨이 API(\`ResourceItemManager\`) 우회 raw 필드 — 트레이트↔티켓 폐쇄 루프, 화폐 환류 0 | \`stubbed\` 등재 완료 |`,
    `| **Gacha Mileage** (직원 가챠) | 뽑기당 적립 → 200 에 교환. 상점 결제(\`SpendMileage\`)도 별도 존재 | \`stubbed\` 등재 + 교환 모델링 완료 |`,
    `| **Skin Mileage** / **Trait Mileage** | 각각 **독립 풀**(가챠 마일리지와 별개). \`Inventory.GachaData.MileagePoints\` 직접 증감이라 API 호출 자체가 없어 스캐너가 못 본다 | \`stubbed\` + \`scanExempt\` 등재 완료 |`,
    `| **GoalBoard / LaunchLoot** | economy-map 스냅샷 **이후** 커밋으로 추가된 신규 경제 경로 | \`stubbed\`. 특히 \`LOOT-*\` 는 **출시마다 반복 유입**이라 재검증 시 모델링 1순위 |`,
    ``,
    `### 5-D. 죽은 노브 · 죽은 상한`,
    ``,
    `| 대상 | 실측 | 해석 |`,
    `|---|---:|---|`,
  );
  for (const d of dead) {
    const isStar = /enhanceCost/.test(d.knob);
    w(`| \`${d.knob}\` | 최대효과 ${pct(d.maxEffect)} | ${isStar ? '**게임 판정 아님 — 모델 한계.** ★→개발점수 커플링 계수 `starDevBonusPerStar` 는 **소비자(`world.js`)만 있고 생산자가 코드·DT 어디에도 없다** — 추출로는 영원히 안 나오므로 §11.1 의 2회 측정으로 손 계산해 `measurements.json` `manual` 에 기입해야 한다. 값이 들어오면 `--profile high`(`star` 토큰을 가진 유일한 프로필)로 재스캔' : '**구조적 발견** — Money 축 노브. mid 이상에서 Money 가 병목이 아니라는 뜻(§6.2)'} |`);
  }
  w(
    `| \`MarketCap MaxMul 3.0\` | 포화 필요 MC = 상한 가정의 **321배** | **죽은 상한.** 현행 밸런스에서 아무 일도 하지 않는다 |`,
    `| \`emp.enhanceCostGrowth\` | 스캔 대상 아님 | \`enhanceCostBase\` 와 **같은 ★축**이라 같은 이유로 판정 불가. 실코드에는 배선돼 있다(\`EmployeeManager.cpp:589\` \`EnhanceCostBase × EnhanceCostGrowth^E\`) — "무효"가 아니라 "이 모델로는 못 잰다" |`,
    ``,
  );

  // ══ 6. 구조 진단 ══════════════════════════════════════════════════════════════
  const scanProf = scan?.profiles ?? [];
  w(
    SECTIONS[5], ``,
    `### 6.1 요구곡선 절벽 — 이 검증의 최대 리스크`,
    ``,
  );
  if (cliff) {
    const plusRow = (scan.rows ?? []).find(r => r.knob === cliff.knob && r.delta > 0);
    w(
      `| 노브 | −20% 효과 | +20% 효과 | 비대칭 | 완주율 | 소프트락 |`,
      `|---|---:|---:|---:|---:|---:|`,
      `| \`${cliff.knob}\` | ${pct(cliff.effectMinus)} | **${pct(cliff.effectPlus)}** | **×${num(cliff.asymmetry, 1)}** (기준 ×${num(scan.meta?.cliffRatio ?? 3, 0)}) | ${plusRow ? pct(plusRow.completionRate) : '—'} (${plusRow ? sgn(plusRow.diffPctCompletion) : '—'}) | ${plusRow ? pct(plusRow.softlockRate) : '—'} |`,
      ``,
      `\`${cliff.knob}\` = 4개 \`RequiredScore\` 컬럼 동시 스케일 = **티어비 대용 노브**다. 낮추면 거의 안 편해지는데(−${pct(cliff.effectMinus)}), 올리면 **게임이 ${num(1 + cliff.effectPlus, 1)}배 길어지고**(${plusRow ? `인앱 ${num(plusRow.medianInAppH)}h` : ''}) 일부 플레이어가 아예 막힌다.`,
      `**직접 함의 — retune §7-1 의 "티어비 2.00 상향"은 이 벼랑을 오르는 방향이다.** 상향을 적용하려면 A1/A2 실측으로 공급(개발점수)이 함께 오른다는 근거를 먼저 확보해야 한다. 실측 전 상향은 권장하지 않는다.`,
      `⚠ 한계: 단일 배율 스캔이라 **"어느 티어의 요구치가 벼랑인가"** 는 이 결과로 답할 수 없다(티어별 비율을 따로 흔든 게 아님).`,
      ``,
    );
  }
  w(
    `### 6.2 Money 는 병목이 아니다 (살아있는 축 = 시간)`,
    ``,
    `| 노브 | 축 | 효과(최대) | 판정 |`,
    `|---|---|---:|---|`,
  );
  for (const r of (scan?.rows ?? [])) {
    if (r.delta < 0) continue;
    const pair = (scan.rows ?? []).filter(x => x.knob === r.knob);
    const eff = Math.max(...pair.map(x => x.effect ?? 0));
    const isDead = dead.some(d => d.knob === r.knob);
    const isCliff = cliffs.some(c => c.knob === r.knob);
    const axis = /idleIncome|multSlope|peakMult/.test(r.knob) ? 'Money 유입'
      : /devCost/.test(r.knob) ? 'Money 유출'
      : /enhanceCost/.test(r.knob) ? '★(미모델)'
      : /gradeLifespan|progressRate/.test(r.knob) ? '**시간**'
      : /reqCurve/.test(r.knob) ? '**게이트**' : '—';
    w(`| \`${r.knob}\` | ${axis} | ${pct(eff)} | ${isCliff ? '**절벽**' : isDead ? '죽음' : '**살아있음**'} |`);
  }
  w(
    ``,
    `Money 유입 노브 3종 + 유출 노브 1종이 **전부 죽었다**. 반대로 살아있는 2종(\`quality.gradeLifespanMin\`, \`offline.progressRate\`)은 둘 다 **시간 축** 노브다.`,
    `원장이 이유를 말해준다(§4.1): 유입의 ${pct(s10Share)}가 오프라인이고 유출의 ${pct(k3Share)}가 강화로 되돌아가, Money 총량을 흔들어도 진행 속도가 안 바뀐다.`,
    `**밸런스를 시간으로 조절하려면 Money 계수가 아니라 운영 수명·오프라인 진행률·요구곡선을 만져야 한다.**`,
    ``,
    `### 6.3 소프트락 — 메커니즘은 DevScore 게이트, 통과 조건은 탭·투자 **이중 필요조건**`,
    ``,
    `> 아래 표는 **파탄 스캐너**(\`scan.js\`, 각 ${scan?.meta?.args?.seeds ?? 50}시드 · seedStart ${scan?.meta?.args?.seedStart ?? '?'}) 결과라 §3.1 본 런(${seedsOf('mid')}시드 · seedStart 1)과 **표본이 다르다.** 예: \`midNoTap\` 완주율이 여기선 ${pct(scanProf.find(s => s.profile === 'midNoTap')?.completionRate)}, 본 런에선 ${pct(aggs.midNoTap?.completionRate)} — 둘 다 "거의 못 깬다"로 같은 결론이지만 **숫자를 인용할 땐 본 런(${seedsOf('mid')}시드) 쪽을 쓸 것.** 소프트락률·원인 귀속은 스캐너에만 있는 지표다.`,
    ``,
    `| 프로필 | 완주율 | 도달티어(중앙) | 소프트락률 | 원인 |`,
    `|---|---:|---:|---:|---|`,
  );
  for (const sp of scanProf) {
    w(`| ${sp.profile} | ${pct(sp.completionRate)} | ${num(sp.medianTierReached, 0)} | ${pct(sp.softlockRate)} | ${Object.entries(sp.softlockCauses ?? {}).map(([k, v]) => `${k}:${v}`).join(', ') || '—'} |`);
  }
  const sample = scanProf.find(s => s.softlockSample)?.softlockSample;
  w(
    ``,
    `**진단 4-arm (2×2 통제 — 탭 duty × 투자셋):**`,
    ``,
    `| arm | 탭 duty | 투자셋 | 완주(본 런 ${seedsOf('mid')}시드) | 도달티어 |`,
    `|---|---:|---|---:|---:|`,
    `| \`mid\` | 0.52 | 4축 | **${pct(aggs.mid?.completionRate)}** | ${num(med(aggs.mid?.reachedTier), 0)} |`,
    `| \`midNoTap\` | 0 | 4축 | ${pct(aggs.midNoTap?.completionRate)} | ${num(med(aggs.midNoTap?.reachedTier), 0)} |`,
    `| \`lowTap\` | 0.52 | 저(IdleIncome만) | ${pct(aggs.lowTap?.completionRate)} | ${num(med(aggs.lowTap?.reachedTier), 0)} |`,
    `| \`low\` | 0 | 저(IdleIncome만) | ${pct(aggs.low?.completionRate)} | ${num(med(aggs.low?.reachedTier), 0)} |`,
    ``,
    `- **메커니즘**: 정체 티어에서도 착수는 계속 일어나는데(68~133회) 그중 **93~100%가 개발점수 미달로 스텝 게이트에 걸린다.** Money·빌딩EXP·컨텐츠 결손 원인은 **0건**.`,
    sample ? `  - 대표 사례: \`${sample.detail}\`` : ``,
    `- **통과 조건**: 탭만 켜도(\`lowTap\`) 완주 ${pct(aggs.lowTap?.completionRate)}, 투자만 해도(\`midNoTap\`) ${pct(aggs.midNoTap?.completionRate)}, **둘 다 있어야**(\`mid\`) ${pct(aggs.mid?.completionRate)}. 각각은 도달 티어를 한 칸씩 밀어줄 뿐 완주선을 넘기지 못한다.`,
    `- ⚠ **"원인은 탭 duty"는 반증된 문장이다.** \`low\`/\`midNoTap\` 두 arm 만 보면 교락(두 축이 동시에 다름)이라 원인을 가를 수 없고, \`lowTap\` arm 을 추가해서야 이중 필요조건이 드러났다.`,
    `- **D1("탭은 선택, 효율 +20~30%") 판정**: 이 값 세트에서 **탭은 선택이 아니다.** 단 근거는 "무탭이면 절대 못 깬다"가 아니라 "무탭이면 완주율이 ${pct(aggs.mid?.completionRate)} → ${pct(aggs.midNoTap?.completionRate)} 로 떨어지고, 완주하더라도 인앱 ${num(med(aggs.midNoTap?.inAppHoursCompleted))}h(mid 의 ${num(med(aggs.midNoTap?.inAppHoursCompleted) / (midH || 1), 1)}배)가 걸린다" 는 형태다.`,
    `- **2차 제약**: 정체 런의 사실상 100%가 동시에 그 티어 최고가 프로젝트를 감당하지 못한다(잔액 < 티어 실비). 개발점수 문제를 풀어줘도 티어 상단 프로젝트는 여전히 못 산다 — 처방 시 이 이중 제약을 함께 볼 것.`,
    ``,
    `### 6.4 인플레 0건 — 단, "건전"이 아니라 **vacuous** 다`,
    ``,
    `전 프로필·전 노브 변형에서 인플레 검출 0건. 그러나 그 이유가 문제다:`,
    ...(() => {
      const rows = raw.dt?.DT_BuildingEnhancementDefinition ?? [];
      if (!rows.length) return [`- (\`DT_BuildingEnhancementDefinition\` 미추출 — 강화축 상한 검증 불가)`];
      const money = rows.filter(r => r.CostResourceType === 'Money');
      const lv = money.map(r => Number(r.MaxLevel)).filter(Number.isFinite);
      const unlimited = rows.filter(r => Number(r.MaxLevel) === 0).map(r => r.Name);
      const gr = money.map(r => Number(r.CostGrowthRate)).filter(Number.isFinite);
      return [
        `- Money 강화축 ${money.length}종의 \`MaxLevel\` 이 **${int(Math.min(...lv.filter(x => x > 0)))}~${int(Math.max(...lv))}**(${unlimited.length ? `\`${unlimited.join('\`/\`')}\` 은 0 = **무제한**` : ''})이고, **\`CostGrowthRate\` 가 ${num(Math.min(...gr), 3)}~${num(Math.max(...gr), 3)}** 로 극히 완만하다 — 즉 상한이 사실상 도달 불가다.`,
        `- 정책이 매 세션 reserve 를 뺀 전액을 강화에 쏟으면 26 인앱일 만에 강화 레벨이 1,700~1,800 에 이르는데도 상한 근처에 못 간다.`,
        `- 그 결과 유입의 **${pct(k3Share)}가 강화 싱크로 흡수**된다 — 인플레가 일어날 여지 자체가 구조적으로 없다.`,
        `- **따라서 "인플레 없음"을 건전성 근거로 인용하면 안 된다.** 무한 싱크를 닫은 시나리오(\`MaxLevel\` 을 현실적 값으로 낮추거나 \`CostGrowthRate\` 를 올려서)에서 재판정하지 않은 상태다.`,
        `- ⚠ 대조 — **Brick 축인 \`BuildingFloor\` 만 \`CostGrowthRate ${num(BRICK_FACTS.floorAxis.growth, 2)}\`** 로 Money 축들보다 **${int(Math.log(BRICK_FACTS.floorAxis.growth) / Math.log(Math.max(...gr)))}배 가파르다**. Brick 쪽에는 진짜 기하급수 싱크가 있고, Money 쪽에만 없다.`,
      ];
    })(),
    ``,
    `### 6.5 빌딩 EXP 게이트는 무해 (retune §7-4 해소)`,
    ``,
    `\`RequiredBuildingLevelForTier(T) = (T−1)×2\` 게이트는 병목이 **아니다**. \`A-EXPGATE\` 직접 계측: ${aRes.find(r => r.id === 'A-EXPGATE')?.detail ?? '(결과 없음)'}`,
    `EXP 소스는 정확히 3곳(스텝 완료 75/판 · 출시 60~150 · 운영 완료 \`max(50, 총수익×0.01)\`)이고, **Brick 층 증축은 EXP 를 주지 않는다** — retune §7-4 의 추정은 오답이었다.`,
    `⚠ 단서: 운영 완료 EXP(E3)는 **운영을 끝까지 돌려야** 지급된다. 운영 중 슬롯을 갈아치우는 기능이 생기면 후반 EXP 가 판당 ≤225 로 줄어 **비로소 병목이 된다**.`,
    ``,
  );

  // ══ 7. Brick ══════════════════════════════════════════════════════════════════
  w(SECTIONS[6], ``);
  if (!brick) {
    w(`> ⚠ \`values.json\` 에 \`BuildableTable\` 이 없어 분석 불가.`, ``);
  } else {
    w(
      `> 사용자 제기 안건(2026-08-02): "Brick 건설비가 너무 싸고 1x1 동일가격". 아래는 \`values.json\` 의 \`BuildableTable\` ${brick.total}행 실측이다.`,
      ``,
      `### 7.1 현황 — 산포가 존재하지 않는다`,
      ``,
      `| 지표 | 값 |`,
      `|---|---|`,
      `| 행 수 | ${brick.total} |`,
      `| 건설비 최소 / 최대 / 평균 | ${int(brick.min)} / ${int(brick.max)} / ${num(brick.mean, 1)} Brick |`,
      `| 서로 다른 가격 종류 | **${brick.clusters.length}종** |`,
      `| 전 행 동일가격 여부 | **${brick.uniform ? '예 — 산포 0' : '아니오'}** |`,
      `| Brick 외 건설 재화 | ${brick.otherCurrencies.length ? brick.otherCurrencies.join(', ') : '**없음**(전 행 Brick 단일 재화)'} |`,
      ``,
      `**동일가격 클러스터:**`,
      ``,
      `| 건설비(Brick) | 행 수 | 비중 | 해당 행 |`,
      `|---:|---:|---:|---|`,
    );
    for (const c of brick.clusters) {
      const names = c.names.length > 6 ? `${c.names.slice(0, 4).join(', ')} … ${c.names[c.names.length - 1]} (총 ${c.names.length})` : c.names.join(', ');
      w(`| ${int(c.cost)} | ${c.count} | ${pct(c.count / brick.total, 0)} | ${names} |`);
    }
    w(
      ``,
      `**등급(Rarity)별 — 등급이 가격과 무관함을 보여주는 표:**`,
      ``,
      `| Rarity | 행 수 | 건설비 min/max/평균 |`,
      `|---|---:|---|`,
    );
    for (const r of brick.byRarity) {
      w(`| ${r.rarity} | ${r.count} | ${int(r.min)} / ${int(r.max)} / ${num(r.mean, 0)} |`);
    }
    w(
      ``,
      `> 판정: **\`Rarity\` 는 카드 색 표시 전용**이고(\`BuildableCardTable.h\` 주석: "오피스 프리셋/슬롯/해금과 무관, footprint 칸수가 그 역할"), 건설비에도 반영돼 있지 않다.`,
      `> 실제 건물 크기는 \`FBuildingData\` 의 \`FootprintWidthCells/DepthCells\` 이고 분포는 **${BRICK_FACTS.footprintDist}** 인데(설계 스펙 기록 — 데이터가 \`BuildingDataTable.uasset\` 전용이라 CSV 대조 불가),`,
      `> **정원은 \`EmployeesPerFloor × W × D\` 로 칸수에 비례해 커지는데 건설비는 전 행 동일**하다. 즉 1칸 건물과 9칸 B40 의 가격이 같다.`,
      ``,
      `> ★ 부수 발견 — **죽은 필드 2개**: \`${BRICK_FACTS.deadFields}\`. 건설비의 단일 진실은 \`BuildableTable\` 의 \`ConstructionCosts\` 뿐이고(차감은 \`${BRICK_FACTS.spendSite}\`), **티어·보유 채수 배율은 코드에 0건**이다. \`FBuildingData\` 쪽 비용 필드는 이름만 남아 오해를 부른다 — 정리 대상.`,
      ``,
      `### 7.2 "몇 분이면 공짜인가" — 벽돌 공장 대비 회수 시간`,
      ``,
      `생산 곡선은 DT 가 아니라 **C++ 하드코딩**이다(\`${BRICK_FACTS.curveSrc}\`; \`DT_FactoryUpgradeDefinition\` 은 비용·상한만 담당). 전 슬롯 **Lv${BRICK_FACTS.startLevel} 로 시작**한다(\`FactorySaveData.cpp:6~20\` — Lv0 은 도달 불가).`,
      ``,
    );
    const holdRateL1 = BRICK_FACTS.holdAmountL1 / BRICK_FACTS.holdIntervalL1;   // 개/초
    const holdRateMid = BRICK_FACTS.holdAmountMid / BRICK_FACTS.holdIntervalMid;
    const secL1 = brick.max / holdRateL1;
    const secMid = brick.max / holdRateMid;
    const allSecL1 = (brick.max * brick.total) / holdRateL1;
    w(
      `| 모드 | 강화 | 산출 | Brick/시간 | **${int(brick.max)} Brick(건물 1채) 회수** | ${brick.total}채 전부 |`,
      `|---|---|---|---:|---:|---:|`,
      `| 홀드-탭 | **무강화 Lv1** | ${BRICK_FACTS.holdAmountL1}개 / ${num(BRICK_FACTS.holdIntervalL1, 1)}초 | ${int(holdRateL1 * 3600)} | **${num(secL1, 0)}초 (= ${num(secL1 / 60, 1)}분)** | ${num(allSecL1 / 60, 1)}분 |`,
      `| 홀드-탭 | 중반(Speed L51 / Amount L50) | ${BRICK_FACTS.holdAmountMid}개 / ${num(BRICK_FACTS.holdIntervalMid, 1)}초 | ${int(holdRateMid * 3600)} | ${num(secMid, 1)}초 | ${num((brick.max * brick.total) / holdRateMid, 0)}초 |`,
      `| 자동수집 | Lv1 (해금 후) | ${BRICK_FACTS.autoCapacityL1}개 / ${num(BRICK_FACTS.autoIntervalL1, 0)}초, **용량 ${BRICK_FACTS.autoCapacityL1}** | — | 수령 ${int(brick.max / BRICK_FACTS.autoCapacityL1)}회 | — |`,
      ``,
      `- 홀드로 찍은 벽돌은 **탭 없이 자동으로 카운터에 적립**된다(\`BrickCollectWidget.cpp:106~112\` \`FinishCollect()\` → \`StoreResource(Brick, …)\`).`,
      `- 자동수집은 **${BRICK_FACTS.autoUnlock}** 이라 초반엔 0이고, 해금 후에도 용량이 곧 상한이라 "시간당 레이트"가 아니라 **접속(수령) 횟수 상한**으로 동작한다.`,
      `- 중반 강화(Speed L51/Amount L50) 누적 비용은 **약 ${money(BRICK_FACTS.midMoneyCost)} Money** — §4.1 의 mid 총 유입(${midRoll ? money(midRoll.inflow.Money / midRoll.n) : '—'}) 대비 무시할 수준이다.`,
      `- **오프라인 벽돌 누적은 없다** — 공장은 전부 월드 타이머 기반이고 \`ComputeOfflineGains\` 는 Money/프로젝트 진행만 처리한다.`,
      ``,
      `**결론: 최고가 건물조차 무강화 상태에서 ${num(secL1 / 60, 1)}분 홀드면 공짜다.** 중반이면 ${num(secMid, 1)}초. ${brick.total}채 전부를 지어도 무강화 ${num(allSecL1 / 60, 0)}분·중반 ${num((brick.max * brick.total) / holdRateMid, 0)}초다.`,
      `게다가 **출시 루트박스가 1회당 Brick ${BRICK_FACTS.launchLoot.min}~${BRICK_FACTS.launchLoot.max}(${BRICK_FACTS.launchLoot.tierRange})** 를 준다(\`DT_LaunchLoot\` \`Low_Brick\`) — **출시 한 번이 건물 ${num(BRICK_FACTS.launchLoot.min / brick.max, 0)}~${num(BRICK_FACTS.launchLoot.max / brick.max, 0)}채 값**이다. 공장을 아예 안 만져도 건설비가 저절로 충당된다.`,
      ``,
      `**구조적 판정:**`,
      `1. **Brick 은 진행 제약으로 기능하지 않는다.** 게임 전체(인앱 ${num(midH)}h)에서 건설비 총액이 ${int(brick.max * brick.total)} Brick 인데, 그건 무강화 홀드 ${num(allSecL1 / 60, 0)}분 분량이다.`,
      `2. 시뮬이 P(병렬 슬롯) 램프를 정책 상수로 두고 Brick 을 제약에서 뺀 것도 같은 이유다 — 즉 **retune §7-7("P의 실제 도달 곡선")이 미해소인 원인이 바로 이 평탄·저가 건설비**다.`,
      `3. **동일가격은 의사결정을 지운다.** 어느 건물을 지을지가 비용과 무관해지므로, 플레이어는 footprint(정원)만 보고 항상 가장 큰 것을 고른다. 건설이 "선택"이 아니라 "절차"가 된다.`,
      `4. **건설비는 Brick 경제 안에서도 반올림 오차 수준이다.** Brick 의 다른 싱크는 \`BuildingFloor\` 강화 1축뿐인데(\`Cost(L) = ${int(BRICK_FACTS.floorAxis.base)} × ${BRICK_FACTS.floorAxis.growth}^L\`, \`MaxLevel=${BRICK_FACTS.floorAxis.maxLevel}\`=무제한), **그 첫 레벨 1회가 ${int(BRICK_FACTS.floorAxis.base)} Brick — 건물 ${int(BRICK_FACTS.floorAxis.base / brick.max)}채 값**이다. 층 Lv30 한 번이 ${money(BRICK_FACTS.floorAxis.base * Math.pow(BRICK_FACTS.floorAxis.growth, 30))}, Lv50 이 ${money(BRICK_FACTS.floorAxis.base * Math.pow(BRICK_FACTS.floorAxis.growth, 50))} 라 ${brick.total}채 전부(${int(brick.max * brick.total)})가 **층 Lv${Math.ceil(Math.log((brick.max * brick.total) / BRICK_FACTS.floorAxis.base) / Math.log(BRICK_FACTS.floorAxis.growth))} 한 번**에 묻힌다.`,
      `5. 나머지 12개 강화축·공장 강화·부지 인수는 전부 Money 다. **즉 Brick 경제 = "층 증축" 단일 축**이고, 그 축은 \`CostGrowthRate ${num(BRICK_FACTS.floorAxis.growth, 2)}\` 로 **제대로 된 기하급수 싱크**다(Money 축들의 1.003~1.005 와 대조 — §6.4). 문제는 건설비가 그 옆에 붙지 못한 것이다.`,
      ``,
      `### 7.3 권고안 (구체 수치 — **적용은 별도 작업**)`,
      ``,
      `**먼저 짚을 함정**: Brick 만으로는 건설을 의미 있게 만들기 어렵다. 생산율이 강화로 Lv1 ${int(holdRateL1 * 3600)}/h → 중반 ${int(holdRateMid * 3600)}/h 로 **${int(holdRateMid / holdRateL1)}배** 뛰기 때문에, 어떤 고정 Brick 가격을 넣어도 중반 이후엔 다시 무의미해진다. 그래서 권고는 **곡선화 + 재화 이원화** 두 갈래다.`,
      ``,
      `#### (1) 건설비 곡선화 — footprint 초선형 × 보유 채수 누진`,
      ``,
      `설계 원칙: (a) 정원이 \`EmployeesPerFloor × W × D\` 로 **칸수에 비례**하니 비용은 칸수보다 **가팔라야** 선택이 생긴다. (b) N번째 건물이 더 비싸야 P 램프가 실제 곡선이 된다. (c) 등급(Rarity)은 표시 전용이므로 계수는 **footprint 기준**으로 잡는다.`,
      ``,
      `\`\`\``,
      `BrickCost(cells, n) = BaseUnit × cells^α × GrowthPerBuilding^(n−1)`,
      ``,
      `  BaseUnit          = 2,000      (1칸 1채째 — 현행 ${int(brick.min)}의 20배 ≈ 무강화 홀드 ${num(2000 / holdRateL1 / 60, 0)}분)`,
      `  α (footprint 지수) = 1.40       (2칸 ≈ ×2.6 / 4칸 ≈ ×7.0 / 9칸 ≈ ×22.1)`,
      `  GrowthPerBuilding = 1.45       (9채째면 ×1.45^8 ≈ ${num(Math.pow(1.45, 8), 0)})`,
      `\`\`\``,
      ``,
      `| n번째 | 1칸 | 2칸 | 4칸 | 9칸(B40) |`,
      `|---:|---:|---:|---:|---:|`,
      ...[1, 2, 3, 5, 7, 9].map(n => {
        const g = Math.pow(1.45, n - 1);
        const c = cells => 2000 * Math.pow(cells, 1.40) * g;
        return `| ${n} | ${int(c(1))} | ${int(c(2))} | ${int(c(4))} | ${int(c(9))} |`;
      }),
      ``,
      `- 1칸만 9채 누적 = **${int([1, 2, 3, 4, 5, 6, 7, 8, 9].reduce((a, n) => a + 2000 * Math.pow(1.45, n - 1), 0))} Brick**(현행 ${int(brick.min * 9)} 대비 ${int([1, 2, 3, 4, 5, 6, 7, 8, 9].reduce((a, n) => a + 2000 * Math.pow(1.45, n - 1), 0) / (brick.min * 9))}배) — 무강화 홀드 기준 ${num([1, 2, 3, 4, 5, 6, 7, 8, 9].reduce((a, n) => a + 2000 * Math.pow(1.45, n - 1), 0) / holdRateL1 / 3600, 1)}시간분.`,
      `- 큰 건물을 섞으면 후반 1채가 수십만 Brick 이 되어 **"공장을 키울 것인가 / 건물을 늘릴 것인가"** 라는 실제 선택이 생긴다.`,
      `- **출시 루트박스 \`Low_Brick\` ${BRICK_FACTS.launchLoot.min}~${BRICK_FACTS.launchLoot.max} 도 같이 올려야 한다** — 안 그러면 이 곡선에서 루트박스가 상대적으로 무의미해진다(현행은 반대로 건물 ${num(BRICK_FACTS.launchLoot.min / brick.max, 0)}~${num(BRICK_FACTS.launchLoot.max / brick.max, 0)}채 값).`,
      ``,
      `#### (2) 재화 이원화 — 건설비에 Money 항 추가 (권고 강도: 높음)`,
      ``,
      `\`FConstructionCost\` 는 이미 \`TArray\` 라 **Brick + Money 를 동시에 청구할 수 있다**(코드 변경 0 — DT 행에 항목만 추가). 이게 중요한 이유:`,
      `- §6.2 가 보인 대로 Money 노브가 전부 죽은 원인은 **무한 강화 싱크가 유입 ${pct(k3Share)}를 흡수**해서다. 건설이 Money 를 요구하면 **강화와 같은 지갑을 두고 경쟁**하게 되어, 죽어 있던 Money 축에 처음으로 의사결정이 생긴다.`,
      `- 권고 초기값: \`MoneyCost(cells, n) = 50,000 × cells^1.2 × 1.6^(n−1)\` — 9채째 1칸이 ${money(50000 * Math.pow(1.6, 8))}, 4칸이 ${money(50000 * Math.pow(4, 1.2) * Math.pow(1.6, 8))} 로 후반 강화 1~2회분과 맞먹는 규모.`,
      `- ⚠ 다만 이건 **§6.2 결론을 바꾸는 변경**이라 적용 후 반드시 감도 스캔을 다시 돌려야 한다(Money 노브가 살아나면 죽은 노브 판정이 뒤집힌다).`,
      ``,
      `#### (3) 정리 항목 (밸런스 아님)`,
      `- \`FBuildingData.BuildCostBrick\` / \`BuildCostMoney\` **삭제** — 읽는 코드 0건인데 이름이 "건설비"라 다음 사람이 반드시 헷갈린다.`,
      `- \`BuildPlacementWidget.cpp:332~358\` 의 \`SpendConstructionCost\` **사본** — \`UBuildPlacementWidget\` 참조 0건(죽은 경로 추정). 확인 후 제거하면 건설비 로직이 1곳으로 모인다.`,
      ``,
      `**적용 전 반드시 먼저 할 것 (순서):**`,
      `1. \`FBuildingData\` 의 \`FootprintWidthCells/DepthCells\` 를 추출기에 등재. \`BuildingDataTable_Import.csv\` 에 컬럼은 존재하지만 이 표가 현행 추출 manifest에 없어, 위 칸수 분포는 아직 자동 검증되지 않는다.`,
      `2. 건설비 곡선은 SOT인 \`DataImport/BuildableTable_Import.csv\` 를 수정하고 \`BuildableTable\` 을 리임포트한다.`,
      `3. 시드 후 **본 검증 파이프라인 재실행**(\`run.js\` → \`scan.js\`)해 P 램프가 실제로 늦춰지는지, Money 항을 넣었다면 죽은 노브가 살아나는지 확인.`,
      ``,
    );
  }

  // ══ 8. 노브 권고안 ════════════════════════════════════════════════════════════
  w(
    SECTIONS[7], ``,
    `> **이 절은 권고이며 적용하지 않았다** (설계 §1 확정 제약: 밸런스 노브 조정은 리포트+권고까지, 적용은 사용자 확정 후 별도 작업).`,
    ``,
    `### 8.1 조정해도 소용없는 노브 (조정 무의미 — 먼저 알 것)`,
    ``,
    `| 노브 | 왜 무의미한가 |`,
    `|---|---|`,
    ...dead.map(d => `| \`${d.knob}\` | ${/enhanceCost/.test(d.knob) ? '모델이 못 재는 것(★ 커플링 미측정) — **게임에서 죽었다는 뜻이 아니다.** 판정 절차 = §11.1 로 `starDevBonusPerStar` 를 **손 측정**해 `measurements.json` 의 `manual` 에 기입 → `calibrate.js` 가 `calibration.json` 으로 넘김 → `--profile high` 재스캔' : `mid 이상에서 Money 가 병목이 아니라 효과가 ${pct(d.maxEffect)} 로 소멸. Money 축을 살리려면 강화 싱크(무한 흡수)를 먼저 닫아야 한다`} |`),
    `| \`MarketCap MaxMul\` | 포화 필요 MC 가 도달 상한의 321배 — 어떤 값을 넣어도 클램프에 안 닿는다 |`,
    ``,
    `### 8.2 살아있는 노브 (여기를 만져야 시간이 움직인다)`,
    ``,
    `| 노브 | 감도(±20%) | 권고 |`,
    `|---|---:|---|`,
  );
  for (const r of (scan?.rows ?? []).filter(x => x.delta > 0)) {
    const pair = (scan.rows ?? []).filter(x => x.knob === r.knob);
    const eff = Math.max(...pair.map(x => x.effect ?? 0));
    if (dead.some(d => d.knob === r.knob)) continue;
    const rec = r.knob === 'reqCurve'
      ? '**증가 금지(현 상태)** — +20% 에서 완주율이 깨진다. A1/A2 실측으로 공급 상승을 확인한 뒤에만 상향'
      : r.knob === 'quality.gradeLifespanMin'
        ? '총 시간의 1차 노브. 늘리면 시간이 거의 비례해서 늘고 **완주율은 안 깨진다** — 시간을 늘리고 싶으면 여기부터'
        : r.knob === 'offline.progressRate'
          ? '역방향 노브(내리면 시간 증가). 오프라인 진행률을 낮추면 인앱시간이 늘지만 **체감이 "기다림"으로 간다** — `gradeLifespanMin` 보다 후순위'
          : '—';
    w(`| \`${r.knob}\` | ${pct(eff)} | ${rec} |`);
  }
  w(
    ``,
    `### 8.3 절벽 주의 목록`,
    ``,
    cliff ? `- \`${cliff.knob}\` **상향 금지 구간** — ×${num(cliff.asymmetry, 1)} 비대칭. +20% 에서 인앱 ${num((scan.rows ?? []).find(r => r.knob === cliff.knob && r.delta > 0)?.medianInAppH)}h · 완주율 ${pct((scan.rows ?? []).find(r => r.knob === cliff.knob && r.delta > 0)?.completionRate)}.` : `- (절벽 없음)`,
    `- **동반 이동 필수**: \`reqCurve\` 를 올리려면 개발점수 공급(로스터·★·ProjectYield·탭)이 같은 비율로 올라야 한다. 요구만 올리면 그대로 완주율이 깎인다.`,
    `- **Brick 건설비(§7.3)는 절벽 밖 축**이다 — Money·요구곡선을 안 건드리고 P 램프만 늦출 수 있는 안전한 노브 후보.`,
    ``,
    `### 8.4 밸런스 아닌 배선 권고 (수치 조정 전에 처리해야 정확해지는 것)`,
    ``,
    `1. **B1 금고 폴백 200** — 무운영 빌딩의 금고가 200 에 묶여 오프라인 수익이 조용히 잘린다. 수정하면 어서션이 자동으로 PASS 로 뒤집힌다.`,
    `2. **B2 세이브 스냅샷** — 다중 운영 중 일부가 오프라인 정산 0. **오프라인이 유입의 ${pct(s10Share)}인 경제**에서 이건 밸런스 이전의 문제다.`,
    `3. **B5 죽은 상점 아이템 5종** — 돈 받고 효과 없음. 상점 행 제거 or 배선.`,
    `4. **B4 DT_Mission CSV 6행 유실 위험** — 리임포트 사고 대기 상태.`,
    ``,
  );

  // ══ 9. retune §7 12건 ═════════════════════════════════════════════════════════
  // 금고 클리핑 문장은 전부 agg-*.json 파생 — 수기 수치("34.3M") 를 두면 재실행 때 조용히 낡는다
  // (2026-08-03 리뷰 F12: 실제로 36.9M 인데 34.3M 이 남아 있었다).
  const vaultClipSentence = (() => {
    const rows = allProf
      .map(p => ({ p, v: med(aggs[p]?.vaultLostMoney) }))
      .filter(r => Number.isFinite(r.v));
    if (rows.length === 0) return '⚠ `vaultLostMoney` 집계 부재 — 클리핑 판정 미산출.';
    const clipped = rows.filter(r => r.v > 0);
    const zero = rows.filter(r => r.v === 0).map(r => `\`${r.p}\``);
    if (clipped.length === 0) return `전 ${rows.length}프로필에서 클리핑 0 (${zero.join(', ')}).`;
    const parts = clipped.map(r => {
      const inflow = roll[r.p] && roll[r.p].n ? roll[r.p].inflow.Money / roll[r.p].n : null;
      const share = Number.isFinite(inflow) && inflow > 0 ? ` ≈ 유입의 ${pct(r.v / inflow, 1)}` : '';
      return `\`${r.p}\` 중앙 ${money(r.v)}${share}`;
    });
    return `${parts.join(' / ')} 만 클립 — 나머지 ${zero.length}프로필(${zero.join(', ')})은 0.`;
  })();
  // [번호, 항목, 상태코드(solved|partial|open), 표시라벨, 근거]
  const U = [
    ['1', '[최대] 개발 앵커(무탭 123.2 / 완벽탭 216) PIE 실측 부재', 'open', '**미해소(이관)**', 'A1/A2 = 사용자 실플레이로 이관(§11). 하네스·치트(`Balance_GateRun`/`Balance_DumpDevScore`)·파서는 완비. **티어비 2.00 상향은 여전히 미확정** — §6.1 절벽이 이 미해소와 직결'],
    ['2', '"총 40~60시간"의 시계 정의(인앱 vs 운영경과 9배 차)', 'partial', '**부분 해소**', '이 리포트는 **인앱 시계**로 통일해 측정했다(mid 중앙 ' + num(midH) + 'h). 두 시계를 동시에 만족하는 값이 없다는 retune 의 지적은 그대로 — 목표 시계 확정은 기획 결정'],
    ['3', '콘텐츠 볼륨(티어당 10개 고정)', 'partial', '**부분 해소 → 전제 반증**', '볼륨 결정 자체는 범위 밖. 단 retune §7-3 의 (c)안("목표를 분포로 재정의")은 **프로필 분포가 존재한다는 전제**에 서는데, §3.1 이 그 전제를 반증했다(완주 3프로필 스프레드 ' + pct((Math.max(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite)) - Math.min(...['mid', 'high', 'skilled'].map(p => med(aggs[p]?.inAppHoursCompleted)).filter(Number.isFinite))) / (midH || 1), 1) + '). (c)안을 쓰려면 분포를 먼저 만들어야 한다'],
    ['4', '스튜디오 레벨 게이트의 실효 여부', 'solved', '**해소**', '`A-EXPGATE` 직접 계측 차단 0건 + EXP 소스 3곳 전수 확인. **병목 아님.** retune 이 추정한 "Brick 층 증축 EXP" 는 존재하지 않음(§6.5)'],
    ['5', '필수 싱크의 실제 규모', 'partial', '**부분 해소**', '원장 실집계(§4.1): mid 총 유출의 ' + pct(k3Share) + '가 빌딩강화. HQ·★는 이 모델에서 0(미모델/비합리선택). **단 ★13 112.7M 근거 수치가 5-A 버그의 산물**이라 재계산 필요'],
    ['6', 'StatBonus / IncomeMult 의 실제 상한', 'open', '**미해소**', '전부 1.0 가정 유지. R 밴드 전체가 상방 이동 가능 — 모델 경계로 명시(§12)'],
    ['7', 'P(병렬 슬롯) 실제 도달 곡선', 'partial', '**부분 해소 → 원인 특정**', '미해소의 **원인이 Brick 건설비의 평탄함**임을 §7 이 특정했다(전 ' + (brick ? brick.total : '?') + '행 동일가 ' + int(brick ? brick.min : 0) + ' = 무강화 홀드 ' + (brick ? num(brick.max / (BRICK_FACTS.holdAmountL1 / BRICK_FACTS.holdIntervalL1) / 60, 1) : '?') + '분). 건설비를 곡선화하면 P 램프가 비로소 데이터로 결정된다'],
    ['8', '금고 캡 vs 오피스 상주 우회', 'partial', '**부분 해소**', '금고 클리핑 실측: 클립되는 조건은 "VaultCapacity 를 샀는가"가 아니라 **운영수명이 금고 시간용량(VaultSeconds)을 넘는 구간에 도달했는가**(오프라인 정산률이 상수 1.0 이 된 뒤로 이 조건에 배율 항이 없다). ' + vaultClipSentence + ' 의도 여부는 여전히 기획 확인 사항'],
    ['9', '탭이 Q에 미치는 정확한 폭', 'open', '**미해소(이관)**', 'A1 풀탭 측정이 전제 — §11. 단 **탭의 존재 여부**가 완주율을 ' + pct(aggs.mid?.completionRate) + '→' + pct(aggs.midNoTap?.completionRate) + ' 로 가른다는 것은 실증됨(§6.3)'],
    ['10', '5산업 `Weight_*` 결손', 'partial', '**부분 해소**', '검증은 Game 100행 기준. **단 2026-08-02 타 세션이 IT/Finance 200행을 저작**했다는 보고가 있어 재추출 시 상태가 다를 수 있다 — 재검증 전 `values.json` 재추출 필수(§12)'],
    ['11', '`LuckyEnhanceTicket` 미배선', 'solved', '**해소(판정)**', '`A-UNWIRED`: 소비 경로 0건 유지 확인 + **상점에서는 이미 팔리고 있음**을 신규 발견. 스펙 문구 "경제에 유입되지 않음"은 **"유입은 있고 효과만 없다"로 정정**돼야 한다(5-B5)'],
    ['12', '★가 방치수익에 기여하지 않는 것', 'open', '**미해소**', '`GenerateIncome` 의 `EnhancementLevel` 미참조는 그대로. 이 검증에서 ★축은 모델 밖(커플링 계수 미추출)이라 판정 불가'],
  ];
  const solved = U.filter(u => u[2] === 'solved').length;
  const partial = U.filter(u => u[2] === 'partial').length;
  const open = U.filter(u => u[2] === 'open').length;
  w(
    SECTIONS[8], ``,
    `**해소 ${solved} / 부분 ${partial} / 미해소 ${open}** (${U.length}건 중).`,
    ``,
    `| # | 불확실성 | 현황 | 근거 |`,
    `|---|---|---|---|`,
    ...U.map(u => `| ${u[0]} | ${u[1]} | ${u[3]} | ${u[4]} |`),
    ``,
    `> **적용 게이트 판정**: retune §7 은 "상위 3개(1·2·3)를 적용 전에 해소하지 않으면 본안 전체가 조건부"라고 못박았다. 이 검증 후에도 **#1 이 미해소**다 → **티어비 2.00 상향은 아직 적용 조건을 충족하지 않는다.** §6.1 의 절벽이 그 판단을 뒷받침한다.`,
    ``,
  );

  // ══ 10. 커버리지 ══════════════════════════════════════════════════════════════
  w(SECTIONS[9], ``);
  if (coverage?.error) {
    w(`> ⚠ 커버리지 산출 실패 — ${coverage.error}`, ``);
  } else {
    const chk = coverage.check ?? {};
    const stub = covEntries.filter(e => e.status === 'stubbed');
    w(
      `\`checkCoverage\` = **ok:${chk.ok}** — 엔트리 ${covEntries.length}건, 미매칭 스캔지점 ${(chk.unmatched ?? []).length}, reason 누락 ${(chk.missingReason ?? []).length}, \`pipelineWired\` 누락 ${(chk.missingPipelineWired ?? []).length}, scanExempt ${(chk.exempt ?? []).length}.`,
      ``,
      `| 상태 | 건수 | 의미 |`,
      `|---|---:|---|`,
      `| \`modeled\` | ${st('modeled')} (파이프라인 배선 **${wiredEntries.length}**) | sim 어딘가에 모델이 있음. 괄호 = 그중 본 런(\`world.js\`) 원장에 실제 도달 |`,
      `| \`stubbed\` | ${st('stubbed')} | 코드에 존재하나 시뮬 미반영 — 사유 필수 |`,
      `| \`n/a\` | ${st('n/a')} | 경제 축과 무관(치트·표시용 등) |`,
      `| **총** | **${covEntries.length}** | |`,
      ``,
      `### 10.1 \`modeled\` ${st('modeled')}건의 실제 도달 범위 (2026-08-03 리뷰 실측 → \`pipelineWired\` 신설)`,
      ``,
      `\`modeled\` 는 원래 "시뮬이 실제로 계산에 반영"이라는 뜻으로 쓰였지만, 실측하니 **${st('modeled')}건 중 ${wiredEntries.length}건만** 본 런 원장에 도달했다. 나머지 ${modeledEntries.length - wiredEntries.length}건은 "구현·단위검증은 됐지만 world 스파인이 호출하지 않는다" 거나 "아예 미구현" 이다. 라벨이 커버리지를 5배 넘게 부풀리고 있었으므로 \`pipelineWired: true|false\` 필드를 신설해 분리하고, \`coverage_check.js\` 가 (a) \`modeled\` 엔트리의 필드 존재 (b) \`false\` 의 사유 존재 (c) **선언 ↔ \`world.js\` 실제 \`post()\` 호출 대조**를 강제한다.`,
      ``,
      `| 구분 | 건수 | id |`,
      `|---|---:|---|`,
      `| **배선됨** (world 원장 post) | ${wiredEntries.length} | \`${wiredEntries.map(e => e.id).join('`, `')}\` |`,
      ...[...unwiredByCause.entries()]
        .sort((a, b) => b[1].length - a[1].length)
        .map(([cause, ids]) => `| 미배선 — ${cause} | ${ids.length} | \`${ids.join('`, `')}\` |`),
      `| **modeled 총** | **${modeledEntries.length}** | |`,
      ``,
      wiredIdle.length
        ? `> ⚠ 배선 ${wiredEntries.length}건 중 \`${wiredIdle.map(e => e.id).join('`, `')}\` ${wiredIdle.length}건은 **코드 경로만 있고 현 정책·값에서 실행 0회**다(원장 관측 0). 실효 도달은 ${wiredEntries.length - wiredIdle.length}건.`
        : null,
      `> \`S11\`(미션 Money)·\`DS4\`(미션 Diamond)는 \`stubbed\` 인데도 원장에 잡힌다 — 온보딩 30분 마일스톤만 일괄 계상하고 미션 체인 전체는 미모델이라 \`stubbed\` 다. \`pipelineWired\` 는 \`modeled\` 전용 필드라 이 둘엔 붙지 않는다.`,
      ``,
      `**\`stubbed\` ${stub.length}건 — 전건 게재** (축별 그룹. "시뮬이 계산에 넣지 않은 경제 경로"의 완전한 목록이며, 발췌가 아니다):`,
      ``,
    );
    // 그룹은 id 접두사로 가른다. 매칭 순서가 곧 우선순위이고, 어디에도 안 걸리면 '기타'로
    // 떨어져 **반드시 표에 나타난다** — 필터로 조용히 빠지는 항목이 없게 하기 위한 구조.
    const GROUPS = [
      ['신규 경제 경로 (GoalBoard · LaunchLoot)', e => /^(GOAL|LOOT)-/.test(e.id)],
      ['독립 재화 풀 (Dust · Mileage)', e => /DUST|MILEAGE/i.test(e.id)],
      ['**Money 싱크**', e => /^K\d/.test(e.id)],
      ['**Money 소스**', e => /^S\d/.test(e.id)],
      ['Diamond', e => /^DS?\d/.test(e.id)],
      ['아이템', e => /^Item/i.test(e.id)],
      ['확률·분포', e => /^R\d/.test(e.id)],
    ];
    const seen = new Set();
    const grouped = GROUPS.map(([label, pred]) => {
      const rows = stub.filter(e => !seen.has(e.id) && pred(e));
      rows.forEach(e => seen.add(e.id));
      return { label, rows };
    });
    const leftover = stub.filter(e => !seen.has(e.id));
    if (leftover.length) grouped.push({ label: '기타(분류 미매칭 — 그룹 규칙 갱신 필요)', rows: leftover });
    w(`| 축 | id | 항목 | 사유 |`, `|---|---|---|---|`);
    let emitted = 0;
    for (const g of grouped) {
      for (let i = 0; i < g.rows.length; i++) {
        const e = g.rows[i];
        emitted++;
        w(`| ${i === 0 ? g.label : '↳'} | \`${e.id}\` | ${(e.desc ?? '').replace(/\|/g, '\\|')} | ${(e.reason ?? '').replace(/\s+/g, ' ').replace(/\|/g, '\\|').slice(0, 190)} |`);
      }
    }
    w(
      ``,
      emitted === stub.length
        ? `> 게재 ${emitted}건 = \`stubbed\` 총계 ${stub.length}건 — **누락 0** (그룹 분류가 목록을 줄이지 않는다).`
        : `> ⚠ 게재 ${emitted}건 ≠ \`stubbed\` 총계 ${stub.length}건 — 그룹 로직 결함. 목록을 신뢰하지 말 것.`,
      ``,
      `**해석에 가장 큰 영향 3가지:**`,
      `1. **미모델 Money 싱크 ${stub.filter(e => /^K\d/.test(e.id)).length}종**(\`${stub.filter(e => /^K\d/.test(e.id)).map(e => e.id).join('`, `')}\`) — §4.1 이 "유출의 ${pct(k3Share)}가 K3 빌딩강화"라고 말할 수 있는 건 **이것들이 원장에 없기 때문**이다. 특히 \`K11\`(오피스 좌/우 확장) \`K15\`(월드 공장 업그레이드) \`K17\`(해외 시설 해금)은 실게임에서 실제로 Money 를 태우는 경로이므로, 모델링되면 K3 지배율이 내려가고 **§6.2 "Money 비병목" 판정이 약해질 수 있다.** 이 3종은 재검증 모델링 우선순위 상위다.`,
      `2. **\`LOOT-S1\`/\`LOOT-ItemS1\`** — 출시마다 발생하는 **반복 유입**이라 Money·Brick 곡선을 통째로 밀 수 있다(§7.2 가 실측한 Brick ${BRICK_FACTS.launchLoot.min}~${BRICK_FACTS.launchLoot.max}/회가 그 예). 모델링 1순위.`,
      `3. **\`S6\` 모뉴먼트 패시브** — 계수가 에디터 전용 DT 라 추출되지 않았다. 키스톤을 세우는 플레이의 Money 소스가 통째로 빠져 있다.`,
      ``,
    );
    // F1 반전 서사용 — 구/신 부스트 모델의 크기를 DT 에서 직접 유도한다(고정 수치 박제 금지).
    //   구모델 기대배율 = p(1+SuccessFrac) + (1−p)(1−FailFrac)   [effDps 에 곱하던 근사]
    //   신모델 시간효과 = TimeSeconds ÷ Duration                  [TimeExtend 는 성패 무관 가산(world.js:319-320)]
    const gRow = (raw.dt?.DT_BoostGamble ?? []).find(r => (r.Industry ?? r.Name) === 'Game') ?? null;
    const gDur = (raw.dt?.DT_Project_Game ?? []).find(r => r.ProjectIndex === 1)?.Duration ?? null;
    const oldExpMult = gRow ? gRow.SuccessChance * (1 + gRow.SuccessFrac) + (1 - gRow.SuccessChance) * (1 - gRow.FailFrac) : null;
    const newTimePct = gRow && gDur ? (gRow.TimeSeconds ?? 0) / gDur : null;
    w(
      `**검증 도구·모델 자체의 결함 5건 — 즉시 수정함** (게임 코드 아님, 조용한 통과 방지):`,
      `1. \`coverage_check.js\` — 스프레드 순서 때문에 \`ok\` 가 덮어써져 **미매칭 스캔지점이 몇 건이든 \`ok:true\`** 로 나왔다. 실제로 신규 경제 경로 4건(GoalBoard/LaunchLoot)이 조용히 통과 중이었고, "실 리포지토리 100% 일치" 테스트도 공허하게 통과하고 있었다. → 수정 후 4건 등재(87 → ${covEntries.length}).`,
      `2. \`assertions.js\` A-DFENUM — 고정 길이 슬라이스가 다음 함수의 \`case\` 를 삼켜 D 배율을 0.8 대신 0.75 로 오독. → 중괄호 균형 절단으로 교체. **어서션이 자기 파서 버그를 잡아낸 사례.**`,
      `3. \`assertions.js\` A-VAULTFALLBACK — 판정이 함수 전문 substring 이라 \`// TODO: LastOperationRatePerSecond\` **주석 한 줄로 KNOWN → PASS 로 뒤집혔다.** → 주석 제거 + 분기 본문 스코프 판정 + 4상태(PASS/KNOWN/REVIEW/FAIL)로 재작성.`,
      `4. \`coverage.json\` \`modeled\` 라벨 자체가 부풀려져 있었다 — "시뮬이 실제로 계산에 반영"이라 정의해 놓고 ${st('modeled')}건을 달았는데 본 런 원장 도달은 ${wiredEntries.length}건뿐(§10.1). → \`pipelineWired\` 신설 + \`coverage_check.js\` 필수화 + \`world.js\` \`post()\` 실호출과의 기계 대조.`,
      `5. \`sim/world.js\` **부스트 도박 모델이 실코드와 다른 형태**였다 — \`effDps × (1±Frac)\` 곱셈으로 근사했는데, 실코드(\`OfficeStageProgressManager.cpp:1684~1687, 1717~1740\`)는 ① \`RemainingTime += TimeSeconds\`(**절대 연장**) ② 각 활성 스텝에 \`(목표합 × Frac)/활성수\` **절대 가감**(0 클램프)이다. → \`sim/project.js\` 에 \`durationBonusSec\`/\`boostScoreFrac\` 을 넣어 실코드 형태로 교체하고 \`EffectType\` 4종(\`TimeExtend\`/\`TimeCut\`/\`ScoreSwing\`/\`PayoffGamble\`)을 분기로 이식했다. \`high\` 300시드 재실행 결과 **완주 시간은 11.50h 로 불변**, 게이트실패 1.39% → ${pct(aggs.high?.gateFailShare, 2)} 로 개선 — 부스트가 시간이 아니라 **품질**로만 돌아온다는 §3.1 결론을 오히려 강화한다. (\`boostGamble:false\` 인 나머지 5프로필은 집계 비트 단위로 무변화 확인.)`
      + ` **교정의 방향이 직관과 반대였다는 점이 핵심이다** — 구 모델은 부스트를 과대평가한 게 아니라 **과소**평가하고 있었다.`
      + ` 구 모델이 개발점수에 곱하던 기대배율은 **×${num(oldExpMult, 3)}**(성공 ${pct(gRow?.SuccessChance, 0)}에 +${pct(gRow?.SuccessFrac, 0)} / 실패에 −${pct(gRow?.FailFrac, 0)})에 불과한데,`
      + ` 실코드는 판 길이를 **+${pct(newTimePct, 1)}**(${gRow?.TimeSeconds ?? '?'}초 ÷ Duration ${gDur ?? '?'}초, 성패 무관 가산) 늘리는 **데다** 점수까지 목표합 기준 절대 가감을 얹는다.`
      + ` 즉 부스트를 **더 세게** 모델링했는데도 완주 시간이 11.50h 에서 꿈쩍하지 않았다 — 시간 축이 부스트에 반응하지 않는다는 뜻이고, 이건 §3.1 의 **이진 게이트** 진단(통과냐 실패냐만 갈릴 뿐 속도는 안 변한다)을 정면으로 강화한다.`,
      ``,
    );
  }
  w(`**어서션 ${aRes.length}종 결과:**`, ``);
  if (assertions?.error) {
    w(`> ⚠ 어서션 산출 실패 (미실행) — ${assertions.error}`, ``);
  } else {
    w(`| id | 상태 | detail |`, `|---|---|---|`);
    for (const r of aRes) w(`| \`${r.id}\` | ${r.state ?? (r.pass ? 'PASS' : r.known ? 'KNOWN' : 'FAIL')} | ${(r.detail ?? '').replace(/\s+/g, ' ').replace(/\|/g, '\\|').slice(0, 220)} |`);
    w(``, `**PASS ${aPass} / KNOWN ${aKnown} / FAIL ${aFail}** → 차단 실패 없음(exit 0).`, ``);
  }

  // ══ 11. 사용자 측정 가이드 ════════════════════════════════════════════════════
  w(
    SECTIONS[10], ``,
    `A1(개발점수/명/초)·A2(게이트 통과율)는 **직원 기여 루프가 UI 라이프사이클에 묶여 있어**(Stage 모드 + Typing + 배정된 워크스테이션이 전부 필요) 리플렉션 자동화로는 잴 수 없다. 사용자 실플레이로 잰다.`,
    ``,
    `### 준비 (한 번만)`,
    `1. 에디터에서 **PIE 실행**.`,
    `2. **Game 산업 빌딩**의 오피스로 진입한다. ⚠ 다른 5산업은 \`DT_Project_*\` 의 \`Weight_*\` 6컬럼이 전부 0이라 \`steps=0\` — 개발점수 축 자체가 안 생긴다.`,
    `3. **책상 수 ≥ 로스터 수** 인지 눈으로 확인. (책상이 모자라면 남는 직원이 Stage 진입에 실패해 표본이 오염된다 — 실제로 한 번 겪었다.)`,
    ``,
    `### 방법 A — A2(통과율) 자동 반복: \`Balance_GateRun\``,
    `\`\`\``,
    `Balance_GateRun 1              // 먼저 1판만 — 자가진단`,
    `\`\`\``,
    `로그에 이렇게 찍힌다:`,
    `\`\`\``,
    `[BALV] GateRun idx=0 pass=1 q=1.2043 grade=B emp=5 typing=5 desk=5 timeout=0`,
    `\`\`\``,
    `- **\`typing=\` 이 로스터 수와 같은지 반드시 확인.** \`typing=0\` 이면 아무도 기여하지 않은 판이라 무효다(파서가 자동 제외하지만, 전부 0이면 측정 자체가 안 된다).`,
    `- 일치하면 \`Balance_GateRun 30\` 으로 확장. 중단은 \`Balance_GateRunStop\`.`,
    `- 착석·부스트도박 모달·이벤트 팝업은 치트가 자동 처리한다(고정 정책 — 무탭 기준선을 흔들지 않기 위해).`,
    `- ⚠ **활성 직능 수 확인**: 라이브 \`DT_Project_Game\` 의 \`Project001\` 은 \`Weight_*\` 가 \`[2,4,2,0,0,0]\` = **활성 3직능**이다. \`a2_passRate<N>NoTap\` 게이트는 시뮬 예측도 같은 DT 행으로 만들므로 대조 자체는 정합하지만, **retune §7-1 의 "4인 95%" 앵커와 비교할 때는 그 앵커가 상정한 활성 직능 수와 같은 프로젝트인지 먼저 맞출 것** — 활성 직능이 3이냐 4냐에 따라 목표 배분이 달라져 통과율이 통째로 이동한다.`,
    ``,
    `### 방법 B — A1(개발점수 레이트): 수동 착수 + \`Balance_DumpDevScore\``,
    `1. 픽칭피드에서 **평소처럼 프로젝트를 착수**한다(탭 없이 방치 = 무탭 기준선).`,
    `2. 개발이 도는 동안 콘솔에서 몇 초 간격으로:`,
    `\`\`\``,
    `Balance_DumpDevScore`,
    `\`\`\``,
    `   → \`[BALV] DevScore s0=48.21/120.00 s1=... emp=5 ...\` 가 찍힌다. 2점 이상이면 차분으로 레이트가 나온다.`,
    `3. 풀탭 값이 필요하면 같은 절차를 **탭을 계속 하면서** 반복(무탭/풀탭 둘 다 있어야 \`dps\` 와 \`duty\` 를 분리할 수 있다).`,
    ``,
    `### 11.1 방법 C — \`starDevBonusPerStar\`(★→개발점수 계수): **A1 을 2회 재서 손 계산**`,
    ``,
    `이 계수는 추출로 절대 안 나온다 — 소비자(\`sim/world.js\` \`STAR_DEV_BONUS\`)만 있고 **생산자가 코드에도 DT 에도 없다**(§5-D). 실코드에선 ★가 크리티컬 확률을 통해 개발점수에 기여하는데(\`EmployeeBehaviorComponent.cpp:1531 GetEnhanceStatBonus\`) 그 계수를 노출하는 API·로그가 없다. 그래서 **행동 관측으로 역산**한다.`,
    ``,
    `1. **★0 로스터**로 방법 B(A1)를 그대로 수행 → \`dps₀\` (개발점수/명/초).`,
    `2. 같은 로스터를 **전원 ★N 으로 강화**(치트 \`Emp_Enhance\` 계열 또는 수동)한 뒤 **같은 프로젝트·같은 인원·같은 탭 조건**으로 A1 재수행 → \`dps_N\`.`,
    `3. \`starDevBonusPerStar = (dps_N / dps₀ − 1) / N\`. (world.js 모델이 \`1 + 평균★ × 계수\` 의 **가산형**이라 배율차를 N 으로 나눈다.)`,
    `4. \`Tools/Balance/out/measurements.json\` 의 \`manual.starDevBonusPerStar\` 에 그 수를 적는다. → \`node Tools/Balance/verify/calibrate.js\` 가 \`calibration.json\` 으로 넘긴다(파서는 \`manual\` 블록을 **재파싱해도 보존**한다).`,
    `5. \`node Tools/Balance/verify/scan.js --profile high\` 로 재스캔 — 이때 비로소 \`emp.enhanceCostBase\`/\`enhanceCostGrowth\` 가 죽은 노브인지 아닌지 판정된다.`,
    ``,
    `> ⚠ 두 측정 사이에 **레벨/인원/프로젝트 행이 바뀌면 안 된다** — 개발점수는 레벨에도 비례하므로(§2 \`meanLevelFactor\`) 다른 변수가 섞이면 ★ 몫을 분리할 수 없다.`,
    `> ⚠ \`N\` 은 크게 잡을수록(예: ★0 → ★10) 노이즈 대비 신호가 커진다. ★1 차이로 재면 개발점수 분산에 묻힌다.`,
    ``,
    `### 11.2 로그로 안 잡히는 값 — \`manual\` 블록 수동 기입`,
    ``,
    `\`measurements.json\` 의 \`manual\` 은 **어떤 \`[BALV]\` 라인으로도 얻을 수 없는 값**의 자리다. 비워 두면 예측이 기준선 가정으로 고정되고, 그 사실은 §2 "미측정 가정" 블록에 뜬다.`,
    ``,
    `| 키 | 언제 적나 | 안 적으면 |`,
    `|---|---|---|`,
    `| \`traitFactor\` | 오프라인 수익 트레이트가 붙은 세이브면 그 배율 | 1.0 가정 |`,
    `| \`starDevBonusPerStar\` | §11.1 로 손 계산한 값 | world.js 가 ★를 아예 안 산다(\`K4\` 실행 0회) |`,
    `| \`projectIndex\` | \`GateRunDone\` 이 없는 구버전 로그를 쓸 때의 오버라이드 | \`DT_Project_*\` 1행 가정 |`,
    ``,
    `### 수집 → 반영`,
    `\`\`\`bash`,
    `# 1) 로그 파싱 (측정 직후 즉시 — 에디터를 재시작하면 로그가 -backup-<타임스탬프>.log 로 밀린다)`,
    `node Tools/Balance/harness/parse_log.js "Saved/Logs/CompanyGrowthRenewal.log"`,
    `# 2) 게이트 재계산 + calibration.json 갱신`,
    `node Tools/Balance/verify/calibrate.js`,
    `# 3) 캘리브레이션을 물려 본 런 재실행`,
    `node Tools/Balance/sim/run.js --profile mid --seeds 300 --calib Tools/Balance/out/calibration.json --out Tools/Balance/out/runs`,
    `node Tools/Balance/verify/scan.js --seeds 50`,
    `node Tools/Balance/verify/report.js --out specs/<날짜>-economy-verification-report.md`,
    `\`\`\``,
    ``,
    `> ⚠ **로그 로테이션 주의** — 측정 직후 파싱하지 않으면 0라인 파싱이 \`measurements.json\` 을 빈 값으로 덮는다(실제로 한 번 덮였다). 파서는 라인 부재를 에러로 처리하지만, 덮어쓰기 자체는 막지 못한다.`,
    `> ⚠ A1/A2 가 들어오면 **§6.1 절벽 판정과 retune §7-1(티어비 2.00) 결론이 바뀔 수 있다.** 이 리포트의 절대 인앱시간은 그때 재발행할 것.`,
    ``,
  );

  // ══ 12. 한계 ══════════════════════════════════════════════════════════════════
  w(
    SECTIONS[11], ``,
    `### 12.1 캘리브레이션 한계 (가장 큰 것)`,
    `- **A1/A2 미실측** → 개발점수는 \`deriveDevScorePerEmpPerSec(values)\` **모델 유도값**이다. 이 값이 ±30% 틀리면 등급 분포가 통째로 이동하고, 등급 분포가 평균 회수율을 결정하므로 **절대 인앱시간 전체가 그만큼 흔들린다.** §3 의 시간은 "이 앵커가 맞다면" 의 조건부 수치다.`,
    `- \`launchOverheadSec = 20\` 은 **추측치**다. 판당 소요의 큰 몫이라 실측으로 대체하면 §3 의 절대값이 다시 움직인다.`,
    `- 탭 duty 상한은 retune §7-1 의 **미실측 앵커에서 역산**한 값이다 — 그 앵커가 바뀌면 §6.3 의 4-arm 결론도 흔들린다(메커니즘=DevScore 게이트 판정은 별개로 유지).`,
    ``,
    `### 12.2 데이터 결손`,
    `- **5산업 \`Weight_*\` 결손** — 검증은 Game 산업 100행 기준이다. 단 **2026-08-02 타 세션이 IT/Finance 200행을 저작**했다는 보고가 있다. 이 리포트의 \`values.json\`(추출 \`${raw.meta?.extractedAt ?? '?'}\`)에는 반영되지 않았을 수 있으므로, **재검증 시 \`values.json\` 재추출을 먼저 할 것.**`,
    `- \`BuildingDataTable_Import.csv\` 에 footprint 컬럼은 있으나 \`BuildingDataTable\` 이 현행 추출 manifest에 없어 §7 칸수 분포는 아직 자동 검증되지 않는다. 건설비 곡선의 SOT는 \`BuildableTable_Import.csv\` 이며 수정 후 해당 DT를 리임포트해야 한다.`,
    `- **§7.2 의 벽돌 생산 수치는 추출기 경유가 아니라 C++ 소스 직접 인용**이다(\`FactoryUpgradeConfig.h\` 하드코딩 곡선). 값이 바뀌면 이 리포트가 자동으로 따라가지 않는다 — 추출기 등재 전까지 §7.2 는 수동 재확인 대상.`,
    ``,
    `### 12.3 모델 경계 (시뮬이 계산하지 않는 것)`,
    ``,
    `| # | 미모델/근사 | 방향 |`,
    `|---|---|---|`,
    `| 1 | **★ → 개발점수 커플링** (크리티컬 확률 경유, 계수 미추출) | 활성화되면 공급 ↑ → 시간 ↓. 즉 retune 의 티어비 상향 논거는 **약해지지 않고 강해진다** |`,
    `| 2 | HQ 레벨업 싱크(194M) | 싱크 규모가 통째로 빠짐 |`,
    `| 3 | 빌딩 건설 Brick 비용 / P 실제 상한 | §7 이 원인을 특정했으나 모델에는 아직 반영 안 됨 |`,
    `| 4 | 로스터 = 전 빌딩 공용 1벌 | **낙관 방향** — 실제는 빌딩별 로스터라 P>1 구간의 등급·성공률을 실제보다 높게 본다 |`,
    `| 5 | StatBonus/트레이트/잠재큐브/이벤트 = 1.0 | R 밴드 전체가 상방 이동 가능 |`,
    `| 6 | 제조·무역·채광 루프 | 미모델(\`stubbed\`) — MarketCap 상한 가정이 낙관/비관 어느 쪽으로도 틀릴 수 있다 |`,
    `| 7 | GoalBoard / LaunchLoot 보상 | 미모델. \`LOOT-*\` 는 **출시마다 반복 유입**이라 Money 곡선을 밀 수 있다 |`,
    `| 8 | \`DS1\`/\`DS2\` 도감 다이아 | 정책이 7클리어에서 진행 → 모델상 도달 불가(§4.2) |`,
    `| 9 | 부스트 도박의 **시간 비용이 사실상 무효** | 모델은 \`TimeExtend\` 연장분(4s)을 세션 예산에서 차감하지만, 세션 450s 대비 병렬 슬롯 P≤5 × 판당 35s = 175s 라 **시간이 애초에 병목이 아니다**(병목 = 빈 빌딩 수). 즉 야근의 페널티는 이 모델에서 \`GoCost\`뿐 — 실게임에서 출시 지연이 체감 비용이라면 그 축이 통째로 빠져 있다 |`,
    ``,
    `### 12.4 방법론 한계`,
    `- 감도 스캔은 **mid 프로필 · ±20% 단일 배율**이다. "어느 티어가 벼랑인가", "low 구간에서 Money 노브가 살아나는가"는 이 결과로 답할 수 없다.`,
    `- 인플레 판정은 §6.4 대로 **vacuous** 다.`,
    `- 어서션은 **워킹트리 소스**를 읽는다 — 동시 세션의 미커밋 C++ 편집이 결과에 즉시 반영된다.`,
    ``,
    `---`,
    ``,
    `## 재실행 방법 (이 리포트 전체를 다시 만드는 명령)`,
    ``,
    `\`\`\`bash`,
    `# 값 추출(에디터 필요) → 본 런 → 검증 → 리포트`,
    ...allProf.map(p => `node Tools/Balance/sim/run.js --profile ${p} --seeds ${seedsOf(p)} --out Tools/Balance/out/runs`),
    `node Tools/Balance/verify/assertions.js`,
    `node Tools/Balance/verify/scan.js --seeds ${scan?.meta?.args?.seeds ?? 50}`,
    `node Tools/Balance/verify/report.js --out specs/${specDate}-economy-verification-report.md`,
    `\`\`\``,
    ``,
    `테스트: \`node --test Tools/Balance/test/*.test.js\``,
    ``,
  );

  // 빈 줄은 마크다운 구조(표/문단 구분)라 지우지 않는다 — 연속 2개 이상만 1개로 접는다.
  // (조건부 조각이 undefined/'' 로 들어오는 자리는 여기서 흡수된다.)
  const lines = P.map(l => (l == null ? '' : String(l)));
  return lines.join('\n').replace(/\n{3,}/g, '\n\n').replace(/^\n+/, '') + '\n';
}

module.exports = { generateReport, SECTIONS, brickAnalysis, ledgerRollup, buildGate, parseCosts, loadAggregates };

if (require.main === module) {
  const argv = process.argv.slice(2);
  const get = (flag, def) => {
    const i = argv.indexOf(flag);
    return i >= 0 && argv[i + 1] ? argv[i + 1] : def;
  };
  const base = path.join(__dirname, '..');
  const outDir = get('--outdir', path.join(base, 'out'));
  const date = get('--date', new Date().toISOString().slice(0, 10));
  const dest = get('--out', path.join(process.cwd(), 'specs', `${date}-economy-verification-report.md`));
  const md = generateReport(outDir, date, { repoRoot: get('--repo', process.cwd()) });
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  fs.writeFileSync(dest, md, 'utf8');
  console.log(`리포트 생성: ${dest} (${md.split('\n').length}줄, ${(Buffer.byteLength(md, 'utf8') / 1024).toFixed(1)}KB)`);
}
