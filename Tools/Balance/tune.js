#!/usr/bin/env node
'use strict';
// 빠른 밸런스 튜닝 — 플레이하며 노브를 찾고(list), 30초 미리보고(preview), DT 는 바로 적용(apply).
//   node Tools/Balance/tune.js list
//   node Tools/Balance/tune.js preview quality.gradeLifespanMin x1.2
//   node Tools/Balance/tune.js apply dt:DT_HQLevel.MoneyCost@3 12000
//
// 원칙(검증 파이프라인과 동일):
//  - 게임 수치를 이 파일에 심지 않는다. 현재값은 out/values.json, 감도는 out/scan.json,
//    위치는 extract/manifest_*.json 에서만 온다. 여기 상수는 CLI 기본값뿐이다.
//  - 시뮬 오버라이드는 verify/scan.js 의 overrideValues 를 그대로 쓴다(원본 values.json 불변).
//  - 없는 노브/파싱 실패는 조용히 넘기지 않고 즉시 throw — 오타는 "효과 없음"으로 위장한다.
//  - Source/ 는 읽기만 한다. src 라우트 노브의 apply 는 거부하고 파일:라인만 알려준다.
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { overrideValues, knobPatches, KNOBS, DEAD_THRESHOLD } = require('./verify/scan.js');
const { runWorld } = require('./sim/world.js');
const { quantiles } = require('./sim/run.js');

const BALANCE_DIR = __dirname;
const REPO_ROOT = path.resolve(BALANCE_DIR, '..', '..');
const DEFAULT_VALUES = path.join(BALANCE_DIR, 'out', 'values.json');
const DEFAULT_SCAN = path.join(BALANCE_DIR, 'out', 'scan.json');
const MANIFEST_SRC = path.join(BALANCE_DIR, 'extract', 'manifest_src.json');
const MANIFEST_EDITOR = path.join(BALANCE_DIR, 'extract', 'manifest_editor.json');
const RESEED_JS = path.join(BALANCE_DIR, 'reseed_project_curve.js');
const CURVE_PARAMS_JSON = path.join(BALANCE_DIR, 'project_curve_params.json');
const REIMPORT_PY = path.join(BALANCE_DIR, 'reimport_project_dt.py');
const REMOTE_CMD = path.join(REPO_ROOT, 'Tools', 'RemoteExec', 'remote_cmd.py');
const TMP_DIR = path.join(BALANCE_DIR, 'out', 'tmp');
const SCRIPT_DIR = 'C:/tmp';

// CLI 기본값 — 밸런스 수치가 아니라 "얼마나 돌려볼 것인가". scan.js 의 런 파라미터와 같은 축.
const DEFAULT_PROFILE = 'mid';
const DEFAULT_SEEDS = 30;
const SEED_START = 1000;
const UNTIL_TIER = 10;
const MAX_CALENDAR_DAYS = 120;
const EXEC_TIMEOUT_MS = 60000; // 원격/프로세스 호출이 매달리면 CLI 가 통째로 멈춘다

class TuneError extends Error {}

// ── 노브 식별 ───────────────────────────────────────────────────────────────
// dt:<Table>.<Column>       = 그 컬럼의 수치 행 전부
// dt:<Table>.<Column>@<Row> = 그 행 한 칸
const DT_KNOB_RE = /^dt:([^.@]+)\.([^@]+?)(?:@(.+))?$/;

// scan.js KNOBS 중 패치 키가 전부 dt: 인 것 = DT 곡선 파생 노브(devCostCoef/reqCurve).
// 이름을 박지 않고 판정해야 scan.js 가 노브를 늘려도 여기가 따라온다.
function isCurveKnob(name) {
  const f = KNOBS[name];
  if (!f) return false;
  const keys = Object.keys(f(0));
  return keys.length > 0 && keys.every(k => k.startsWith('dt:'));
}

function parseKnob(name) {
  if (typeof name !== 'string' || !name.trim()) throw new TuneError('노브 이름이 비었습니다.');
  const m = DT_KNOB_RE.exec(name);
  if (m) return { kind: 'dt-cell', name, table: m[1], column: m[2], row: m[3] ?? null };
  if (isCurveKnob(name)) return { kind: 'dt-curve', name };
  if (name.startsWith('dt:')) throw new TuneError(`DT 노브 형식 오류: ${name} — dt:<Table>.<Column>[@<Row>] 이어야 합니다.`);
  return { kind: 'values', name };
}

// 라우트 = 이 노브를 실제로 바꾸려면 어디를 만져야 하는가.
function routeOf(parsed, valuesDoc) {
  if (parsed.kind === 'dt-cell') return 'dt';
  if (parsed.kind === 'dt-curve') return 'dt-curve';
  const e = valuesDoc.values[parsed.name];
  if (e === undefined) {
    throw new TuneError(`알 수 없는 노브: ${parsed.name}\n  values.json 에 없는 키입니다. \`node Tools/Balance/tune.js list\` 로 확인하세요.`);
  }
  return e.src; // 'src' | 'cdo'
}

// ── 값 파싱 ─────────────────────────────────────────────────────────────────
// x1.2 / *1.2 / +20% / -20% = 배율, 그 외 숫자 = 절대값.
// 곡선 노브(devCostCoef/reqCurve)는 절대값 개념이 없어 맨숫자도 배율로 읽는다.
function parseValueSpec(raw, { multOnly = false } = {}) {
  if (raw === undefined || raw === null || `${raw}`.trim() === '') throw new TuneError('값이 비었습니다. 예: 1.8 / x1.2 / +20%');
  const s = `${raw}`.trim();
  let m = /^[x*×](-?[0-9.]+)$/i.exec(s);
  if (m) return finiteSpec({ mult: parseFloat(m[1]) }, s);
  m = /^([+-]?[0-9.]+)%$/.exec(s);
  if (m) return finiteSpec({ mult: 1 + parseFloat(m[1]) / 100 }, s);
  m = /^(-?[0-9.]+)$/.exec(s);
  if (m) {
    const n = parseFloat(m[1]);
    return finiteSpec(multOnly ? { mult: n } : { set: n }, s);
  }
  throw new TuneError(`값 형식 오류: ${s} — 절대값(1.8) 또는 배율(x1.2 / +20%) 이어야 합니다.`);
}
function finiteSpec(spec, s) {
  const v = 'mult' in spec ? spec.mult : spec.set;
  if (!Number.isFinite(v)) throw new TuneError(`값 형식 오류: ${s}`);
  return spec;
}
const specLabel = spec => ('mult' in spec ? `×${spec.mult}` : `= ${spec.set}`);

// ── values / scan / manifest 로드 ───────────────────────────────────────────
function readJson(p, what) {
  if (!fs.existsSync(p)) throw new TuneError(`${what} 없음: ${p}`);
  try {
    return JSON.parse(fs.readFileSync(p, 'utf8'));
  } catch (e) {
    throw new TuneError(`${what} 파싱 실패: ${p} — ${e.message}`);
  }
}
function loadValuesDoc(valuesPath) {
  const doc = readJson(valuesPath, 'values.json');
  if (!doc.values || !doc.dt) throw new TuneError(`values.json 형식 오류(values/dt 없음): ${valuesPath}`);
  return doc;
}

// scan.json → 노브별 { maxEffect, dead, cliff }
function indexScan(scan) {
  const idx = new Map();
  if (!scan) return { idx, has: false, baseline: null, threshold: DEAD_THRESHOLD };
  for (const r of scan.rows || []) {
    const e = Number.isFinite(r.effect) ? r.effect : 0;
    const cur = idx.get(r.knob) || { maxEffect: 0, dead: false, cliff: null };
    cur.maxEffect = Math.max(cur.maxEffect, e);
    idx.set(r.knob, cur);
  }
  for (const d of scan.dead || []) if (idx.has(d.knob)) idx.get(d.knob).dead = true;
  for (const c of scan.cliffs || []) if (idx.has(c.knob)) idx.get(c.knob).cliff = c;
  return {
    idx, has: idx.size > 0,
    baseline: scan.baseline || null,
    threshold: Number.isFinite(scan.meta && scan.meta.deadThreshold) ? scan.meta.deadThreshold : DEAD_THRESHOLD,
  };
}
function sensOf(scanIdx, knob) {
  const e = scanIdx.idx.get(knob);
  if (!e) return { state: 'unscanned', effect: null };
  if (e.cliff) return { state: 'cliff', effect: e.maxEffect, cliff: e.cliff };
  if (e.dead || e.maxEffect < scanIdx.threshold) return { state: 'dead', effect: e.maxEffect };
  return { state: 'live', effect: e.maxEffect };
}

// ── src 라우트 위치(파일:라인) ──────────────────────────────────────────────
// manifest 의 pattern/anchor 를 실제 파일에서 다시 찾아 라인을 낸다. 읽기 전용 — Source/ 는 절대 안 건드린다.
function srcLocations(manifestSrc, repoRoot) {
  const cache = new Map();
  const out = new Map();
  for (const e of manifestSrc.entries || []) {
    const file = e.file;
    if (!cache.has(file)) {
      const abs = path.join(repoRoot, file);
      cache.set(file, fs.existsSync(abs) ? fs.readFileSync(abs, 'utf8') : null);
    }
    const text = cache.get(file);
    out.set(e.key, { file, line: text === null ? null : findLine(text, e) });
  }
  return out;
}
function findLine(text, entry) {
  let at = -1;
  if (entry.mode === 'regex') {
    const m = new RegExp(entry.pattern).exec(text);
    at = m ? m.index : -1;
  } else if (entry.mode === 'anchored-window') {
    at = text.indexOf(entry.anchor);
  }
  if (at < 0) return null;
  return text.slice(0, at).split('\n').length;
}

function cdoLocations(manifestEditor) {
  const out = new Map();
  for (const e of manifestEditor.cdoProps || []) {
    const cls = String(e.class).split('.').pop();
    out.set(e.key, { class: cls, prop: e.prop, path: e.class });
  }
  return out;
}

// ── 카탈로그 ────────────────────────────────────────────────────────────────
function buildCatalog({
  valuesPath = DEFAULT_VALUES, scanPath = DEFAULT_SCAN,
  manifestSrcPath = MANIFEST_SRC, manifestEditorPath = MANIFEST_EDITOR,
  repoRoot = REPO_ROOT,
} = {}) {
  const valuesDoc = loadValuesDoc(valuesPath);
  const scanIdx = indexScan(fs.existsSync(scanPath) ? readJson(scanPath, 'scan.json') : null);
  const srcLoc = srcLocations(readJson(manifestSrcPath, 'manifest_src.json'), repoRoot);
  const cdoLoc = cdoLocations(readJson(manifestEditorPath, 'manifest_editor.json'));

  const rows = [];
  for (const [key, e] of Object.entries(valuesDoc.values)) {
    const route = e.src;
    let how;
    if (route === 'cdo') {
      const c = cdoLoc.get(key);
      how = c ? `에디터 BP: ${c.class}.${c.prop}` : `에디터 BP: ${e.loc}`;
    } else {
      const l = srcLoc.get(key);
      const file = l ? l.file : e.loc;
      how = `코드+빌드: ${file}:${l && l.line ? l.line : '?'}`;
    }
    rows.push({ key, route, value: e.v, sens: sensOf(scanIdx, key), how });
  }
  for (const name of Object.keys(KNOBS)) {
    if (!isCurveKnob(name)) continue;
    rows.push({
      key: name, route: 'dt-curve',
      value: curveKnobCurrent(name, valuesDoc),
      sens: sensOf(scanIdx, name),
      how: 'project_curve_params.json + reseed 2커맨드 (tune.js apply 가 안내)',
    });
  }
  return { rows: sortCatalog(rows), scanned: scanIdx.has, threshold: scanIdx.threshold, dtPaths: dtPathMap(manifestEditorPath) };
}

// 곡선 노브의 "현재값" — 배율 노브라 절대값이 없다. 대신 그 노브가 미는 DT 컬럼의 범위를 보여준다.
function curveKnobCurrent(name, valuesDoc) {
  const keys = Object.keys(knobPatches(name, 0));
  const nums = [];
  for (const k of keys) {
    const m = /^dt:([^.]+)\.(.+)$/.exec(k);
    if (!m) continue;
    for (const r of valuesDoc.dt[m[1]] || []) if (typeof r[m[2]] === 'number') nums.push(r[m[2]]);
  }
  if (!nums.length) return null;
  return { range: [Math.min(...nums), Math.max(...nums)], columns: keys.length };
}

const STATE_ORDER = { cliff: 0, live: 1, dead: 2, unscanned: 3 };
function sortCatalog(rows) {
  return rows.slice().sort((a, b) => {
    const s = STATE_ORDER[a.sens.state] - STATE_ORDER[b.sens.state];
    if (s !== 0) return s;
    if (a.sens.state !== 'unscanned') return (b.sens.effect || 0) - (a.sens.effect || 0);
    return a.route === b.route ? a.key.localeCompare(b.key) : a.route.localeCompare(b.route);
  });
}
function dtPathMap(manifestEditorPath) {
  try {
    return readJson(manifestEditorPath, 'manifest_editor.json').dtPaths || {};
  } catch {
    return {};
  }
}

// ── 표시 ────────────────────────────────────────────────────────────────────
const pct = v => (Number.isFinite(v) ? `${(v * 100).toFixed(1)}%` : '-');
const sgnPct = v => (Number.isFinite(v) ? `${v >= 0 ? '+' : ''}${(v * 100).toFixed(1)}%` : '-');
const num = (v, d = 2) => (Number.isFinite(v) ? Number(v).toFixed(d) : '-');

function fmtValue(v) {
  if (v === null || v === undefined) return '-';
  if (Array.isArray(v)) {
    const head = v.slice(0, 4).map(x => (typeof x === 'number' ? trimNum(x) : String(x)));
    return `[${head.join(',')}${v.length > 4 ? `,+${v.length - 4}` : ''}]`;
  }
  if (typeof v === 'object' && Array.isArray(v.range)) return `${trimNum(v.range[0])}~${trimNum(v.range[1])}(${v.columns}컬럼)`;
  if (typeof v === 'number') return trimNum(v);
  return String(v);
}
const trimNum = x => (Number.isInteger(x) ? String(x) : String(Number(x.toFixed(4))));

function fmtSens(s) {
  switch (s.state) {
    case 'cliff': return `절벽⚠ ${pct(s.effect)}`;
    case 'live': return `살아있음 ±${pct(s.effect)}`;
    case 'dead': return `죽음 ${pct(s.effect)}`;
    default: return '미스캔';
  }
}
const ROUTE_LABEL = { dt: 'dt', 'dt-curve': 'dt*', cdo: 'cdo', src: 'src' };

function formatCatalogTable(cat) {
  const cols = [
    { h: '노브', get: r => r.key },
    { h: '현재값', get: r => fmtValue(r.value) },
    { h: '라우트', get: r => ROUTE_LABEL[r.route] || r.route },
    { h: '감도(±20%)', get: r => fmtSens(r.sens) },
    { h: '변경 방법', get: r => r.how },
  ];
  const w = cols.map(c => Math.max(dispLen(c.h), ...cat.rows.map(r => dispLen(c.get(r)))));
  const line = cells => cells.map((s, i) => pad(s, w[i])).join('  ').trimEnd();
  const out = [line(cols.map(c => c.h)), line(w.map(n => '─'.repeat(Math.min(n, 40))))];
  for (const r of cat.rows) out.push(line(cols.map(c => c.get(r))));
  return out.join('\n');
}
// 한글 = 2칸 폭. 표가 어긋나면 노브를 눈으로 훑을 수 없다.
function dispLen(s) {
  let n = 0;
  for (const ch of String(s)) n += /[\u1100-\u11FF\u3000-\u303F\u3130-\u318F\uAC00-\uD7AF\uFF00-\uFF60]/.test(ch) ? 2 : 1;
  return n;
}
const pad = (s, n) => String(s) + ' '.repeat(Math.max(0, n - dispLen(s)));

// ── 시뮬 오버라이드 ─────────────────────────────────────────────────────────
// 반환 = overrideValues 에 넘길 패치맵 + (행 단위는 별도) rowPatches.
function knobOverride(parsed, spec, valuesDoc) {
  if (parsed.kind === 'values') {
    if (valuesDoc.values[parsed.name] === undefined) throw new TuneError(`values.json 누락 키: ${parsed.name}`);
    return { valuesPatches: { [parsed.name]: spec }, rowPatches: [] };
  }
  if (parsed.kind === 'dt-curve') {
    if (!('mult' in spec)) throw new TuneError(`${parsed.name} 은 배율 노브입니다 — x1.2 또는 +20% 형태로 주세요.`);
    return { valuesPatches: knobPatches(parsed.name, spec.mult - 1), rowPatches: [] };
  }
  const rows = valuesDoc.dt[parsed.table];
  if (!Array.isArray(rows)) throw new TuneError(`values.json 누락 DT: ${parsed.table} (노브 ${parsed.name})`);
  if (parsed.row === null) return { valuesPatches: { [`dt:${parsed.table}.${parsed.column}`]: spec }, rowPatches: [] };
  return { valuesPatches: {}, rowPatches: [{ table: parsed.table, column: parsed.column, row: parsed.row, spec }] };
}

// overrideValues 로 사본을 만들고, 행 단위 패치만 그 위에 얹는다(원본 values.json 은 불변).
function writePatchedValues(srcPath, override, dest) {
  // scan.js 는 평범한 Error 를 던진다 — TuneError 로 감싸야 CLI 가 스택 대신 메시지를 보여준다.
  let p;
  try {
    p = overrideValues(srcPath, override.valuesPatches, dest);
  } catch (e) {
    throw e instanceof TuneError ? e : new TuneError(e.message);
  }
  if (!override.rowPatches.length) return p;
  const raw = JSON.parse(fs.readFileSync(p, 'utf8'));
  for (const rp of override.rowPatches) {
    const rows = raw.dt[rp.table];
    if (!Array.isArray(rows)) throw new TuneError(`values.json 누락 DT: ${rp.table}`);
    const hit = rows.filter(r => String(r.Name) === String(rp.row));
    if (!hit.length) throw new TuneError(`DT 행 없음: ${rp.table}@${rp.row} — 행 이름 오타?`);
    let n = 0;
    for (const r of hit) {
      if (typeof r[rp.column] !== 'number') continue;
      r[rp.column] = 'set' in rp.spec ? rp.spec.set : r[rp.column] * rp.spec.mult;
      n++;
    }
    if (n === 0) throw new TuneError(`DT 셀이 수치가 아님: ${rp.table}.${rp.column}@${rp.row} — 컬럼명 오타?`);
  }
  fs.writeFileSync(p, JSON.stringify(raw));
  return p;
}

// ── preview ─────────────────────────────────────────────────────────────────
function median(arr) {
  const s = arr.filter(Number.isFinite).slice().sort((a, b) => a - b);
  return s.length ? quantiles(s).median : null;
}

function summarizeRuns(results, untilTier) {
  const valid = results.filter(r => !r.invalid);
  const done = valid.filter(r => r.endState.completed);
  let launches = 0, gateFails = 0;
  for (const r of valid) {
    launches += r.endState.launches || 0;
    gateFails += r.endState.gateFails || 0;
  }
  const tierDay = new Map();
  for (let t = 2; t <= untilTier; t++) {
    const days = [];
    for (const r of valid) {
      const rec = (r.tiers || []).find(x => x.tier === t);
      if (rec && Number.isFinite(rec.calendarDay)) days.push(rec.calendarDay);
    }
    // 절반 미만만 도달한 티어는 중앙값이 생존편향이라 비교표에 올리지 않는다.
    if (days.length >= Math.ceil(valid.length / 2)) tierDay.set(t, median(days));
  }
  return {
    n: valid.length,
    invalid: results.length - valid.length,
    completionRate: valid.length ? done.length / valid.length : 0,
    medianInAppH: median(done.map(r => r.endState.inAppMin / 60)),
    gateFailShare: launches > 0 ? gateFails / launches : null,
    tierDay,
  };
}

function runSet({ valuesPath, seeds, seedStart, profile, untilTier, maxCalendarDays }) {
  try {
    return Array.from({ length: seeds }, (_, i) => runWorld({
      seed: seedStart + i, profile, valuesPath, untilTier, maxCalendarDays,
    }));
  } catch (e) {
    // values 결손·프로필 오타는 사용자 입력 문제지 버그가 아니다 — 스택 대신 메시지로.
    throw e instanceof TuneError ? e : new TuneError(`시뮬 실행 실패 (profile=${profile}): ${e.message}`);
  }
}

function runPreview({
  knob, value, valuesPath = DEFAULT_VALUES, scanPath = DEFAULT_SCAN,
  profile = DEFAULT_PROFILE, seeds = DEFAULT_SEEDS, seedStart = SEED_START,
  untilTier = UNTIL_TIER, maxCalendarDays = MAX_CALENDAR_DAYS, tmpDir = TMP_DIR,
} = {}) {
  const valuesDoc = loadValuesDoc(valuesPath);
  const parsed = parseKnob(knob);
  const route = routeOf(parsed, valuesDoc);
  const spec = parseValueSpec(value, { multOnly: parsed.kind === 'dt-curve' });
  const override = knobOverride(parsed, spec, valuesDoc);

  const dest = path.join(tmpDir, `tune-${knob.replace(/[^\w.-]/g, '_')}-${process.pid}.json`);
  const runOpts = { seeds, seedStart, profile, untilTier, maxCalendarDays };
  const before = summarizeRuns(runSet({ ...runOpts, valuesPath }), untilTier);
  let after;
  try {
    writePatchedValues(valuesPath, override, dest);
    after = summarizeRuns(runSet({ ...runOpts, valuesPath: dest }), untilTier);
  } finally {
    fs.rmSync(dest, { force: true });
    try { if (fs.existsSync(tmpDir) && fs.readdirSync(tmpDir).length === 0) fs.rmdirSync(tmpDir); } catch { /* 동시 스캔 중이면 그대로 둔다 */ }
  }
  const scanIdx = indexScan(fs.existsSync(scanPath) ? readJson(scanPath, 'scan.json') : null);
  // 시뮬이 안 읽는 컬럼은 before/after 가 반드시 같다 — "영향 없음"으로 오독하지 않게 미리 말한다.
  const consumed = parsed.kind === 'dt-cell' ? simConsumes(parsed.table, parsed.column) : { table: true, column: true };
  return {
    knob, route, spec, profile, seeds, seedStart, untilTier, consumed,
    sens: sensOf(scanIdx, parsed.kind === 'dt-cell' ? knob : parsed.name),
    before, after,
  };
}

function formatPreview(p) {
  const out = [];
  out.push(`미리보기 — ${p.knob} ${specLabel(p.spec)}`);
  out.push(`  라우트 ${ROUTE_LABEL[p.route] || p.route} · profile=${p.profile} · ${p.seeds}시드(${p.seedStart}~${p.seedStart + p.seeds - 1})`);
  if (p.sens.state === 'cliff') {
    const c = p.sens.cliff;
    out.push(`  ⚠ 절벽 노브입니다 — ±비대칭 ×${num(c.asymmetry)}(가파른 쪽 ${c.steepSide}). 한 방향으로 크게 밀면 완주율이 통째로 무너집니다.`);
  } else if (p.sens.state === 'dead') {
    out.push(`  · 죽은 노브입니다(최대효과 ${pct(p.sens.effect)}) — 곡선이 거의 안 움직입니다.`);
  } else if (p.sens.state === 'unscanned') {
    out.push('  · 미스캔 노브입니다 — 감도 근거가 없어 아래 before/after 만이 판단 재료입니다.');
  }
  if (p.consumed && p.consumed.uncertain) {
    out.push('  ⚠ 시뮬 소비 여부가 불확실합니다 — sim/*.js 의 동적 표 이름을 정적으로 못 풀었습니다.');
    out.push('    before/after 가 같아도 "영향 없음"으로 결론 내리지 마세요. 인게임 실플레이로 확인하세요.');
  } else if (p.consumed && !p.consumed.column) {
    const what = p.consumed.table ? '이 컬럼을' : '이 표를';
    out.push(`  ⚠ 시뮬 모델 밖입니다 — sim/*.js 가 ${what} 읽지 않습니다.`);
    out.push('    아래 before/after 가 동일한 것이 정상이며, 효과 없음을 뜻하지 않습니다. 인게임 실플레이로만 확인할 수 있습니다.');
  }
  out.push('');

  const rows = [
    ['완주율', pct(p.before.completionRate), pct(p.after.completionRate), ppDelta(p.before.completionRate, p.after.completionRate)],
    ['인앱h(중앙)', num(p.before.medianInAppH), num(p.after.medianInAppH), relDelta(p.before.medianInAppH, p.after.medianInAppH)],
    ['게이트실패%', pct(p.before.gateFailShare), pct(p.after.gateFailShare), ppDelta(p.before.gateFailShare, p.after.gateFailShare)],
  ];
  for (const [t, d] of p.before.tierDay) {
    const a = p.after.tierDay.get(t);
    rows.push([`T${t} 진입일`, num(d, 0), num(a, 0), absDelta(d, a)]);
  }
  const head = ['지표', 'before', 'after', 'Δ'];
  const w = head.map((h, i) => Math.max(dispLen(h), ...rows.map(r => dispLen(r[i]))));
  out.push(head.map((h, i) => pad(h, w[i])).join('  ').trimEnd());
  out.push(w.map(n => '─'.repeat(n)).join('  '));
  for (const r of rows) out.push(r.map((c, i) => pad(c, w[i])).join('  ').trimEnd());

  if (p.before.invalid || p.after.invalid) out.push(`\n무효 런: before ${p.before.invalid} / after ${p.after.invalid}`);
  return out.join('\n');
}
const ppDelta = (b, a) => (Number.isFinite(b) && Number.isFinite(a) ? `${a - b >= 0 ? '+' : ''}${((a - b) * 100).toFixed(1)}%p` : '-');
const relDelta = (b, a) => (Number.isFinite(b) && Number.isFinite(a) && b !== 0 ? sgnPct((a - b) / b) : '-');
const absDelta = (b, a) => (Number.isFinite(b) && Number.isFinite(a) ? `${a - b >= 0 ? '+' : ''}${(a - b).toFixed(0)}일` : '-');

// ── apply: DT 패치 스크립트 생성 ────────────────────────────────────────────
const pyStr = s => (s === null || s === undefined ? 'None' : JSON.stringify(String(s)));

function uassetPathOf(dtPath, repoRoot) {
  const m = /^\/Game\/(.+)$/.exec(dtPath);
  if (!m) throw new TuneError(`DT 경로 형식 오류: ${dtPath} — /Game/... 이어야 합니다.`);
  return path.join(repoRoot, 'Content', ...m[1].split('/')) + '.uasset';
}

// UE 원격 라운드트립 패치 py. 백업 → export → patch → fill → save → mtime/size 검증.
// ⚠ 검증 없는 save 는 에디터 이중 실행 시 조용히 실패하고 종료할 때 유실된다(실사고 이력).
function buildPatchScript({ table, dtPath, column, row, spec, uassetPath, backupPath }) {
  const op = 'mult' in spec ? 'mult' : 'set';
  const value = 'mult' in spec ? spec.mult : spec.set;
  return `# -*- coding: utf-8 -*-
# Tools/Balance/tune.js 생성 — DT 셀 패치(export→patch→fill→save + 백업 + 저장 검증)
# 실행: PYTHONIOENCODING=utf-8 py "Tools/RemoteExec/remote_cmd.py" "exec(open(r'<이 파일>').read())"
import unreal, json, os, shutil, traceback

DT_NAME = ${pyStr(table)}
DT_PATH = ${pyStr(dtPath)}
UASSET  = ${pyStr(uassetPath)}
BACKUP  = ${pyStr(backupPath)}
COLUMN  = ${pyStr(column)}
ROW     = ${pyStr(row)}
OP      = ${pyStr(op)}
VALUE   = ${value}

# 복구 안내는 실패 지점에 따라 정반대다 — 원격은 traceback 을 안 돌려주므로 메시지에 실어 보낸다.
#  · 검증 실패 = 메모리가 손상본 → 되돌려야 한다(백업 복원이 정답).
#  · 저장 실패 = 메모리는 검증을 통과한 의도값 → 되돌리면 패치를 버리는 것(재시도가 정답).
RECOVER_VERIFY = ("\\n  복구: 에디터를 끄고 " + BACKUP + " 를 " + UASSET + " 에 덮어쓴 뒤 에디터를 다시 켜세요."
                  "\\n  ⚠ 롤백 실패 시: 이 DT 를 **저장하지 마세요**(Save All 금지) — 메모리의 손상본이 디스크에 굳습니다."
                  " 에디터를 저장 없이 종료한 뒤 위 백업을 복원하세요.")
RECOVER_SAVE = ("\\n  메모리의 표는 이미 검증을 통과한 **의도한 값**입니다 — 디스크 저장만 실패했습니다."
                "\\n  복구: 여분의 에디터 인스턴스를 종료하고 같은 명령을 다시 실행하세요."
                "\\n  ⚠ 배율(x1.2)/퍼센트(-20%) 패치였다면 재실행이 **이중 적용**됩니다(메모리엔 이미 곱해진 값이 있음)."
                " 패치를 들고 있는 에디터를 저장 없이 종료하고 다시 실행하거나, 절대값으로 지정해 실행하세요."
                "\\n  ⚠ 이 경우 백업(" + BACKUP + ") 복원은 **패치를 폐기**하는 것입니다 — 되돌리고 싶을 때만 쓰세요.")

try:
    dt = unreal.load_object(None, DT_PATH)
    if dt is None:
        raise RuntimeError("DT 로드 실패: " + DT_PATH)
    if not os.path.isfile(UASSET):
        raise RuntimeError("uasset 파일 없음: " + UASSET)

    # 사전 백업 — 라운드트립(export/fill)이 컬럼을 잃어도 되돌릴 수 있어야 한다.
    os.makedirs(os.path.dirname(BACKUP), exist_ok=True)
    shutil.copy2(UASSET, BACKUP)
    before_mtime = os.path.getmtime(UASSET)
    before_size = os.path.getsize(UASSET)

    # 원본 문자열을 그대로 보관한다 — 검증 실패 시 이걸로 메모리 DT 를 되돌린다.
    original_json = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt)
    rows = json.loads(original_json)
    if not isinstance(rows, list):
        rows = [dict(v, Name=k) for k, v in rows.items()]
        original_json = json.dumps(rows)

    # fill 은 이미 메모리 DT 를 바꿔 놓는다. 디스크는 아직 무사해도 패키지가 dirty 라
    # 누군가 Save All 하면 손상본이 굳는다 → 검증 실패는 반드시 원본으로 롤백하고 나서 raise.
    def verify_fail(msg):
        try:
            rolled = "성공" if unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, original_json) else "실패(fill 이 False 반환)"
        except Exception as ex:
            rolled = "실패(%s)" % ex
        unreal.log("[BALV-TUNE] 검증 실패 → 메모리 롤백 " + rolled)
        raise RuntimeError(msg + " / 메모리 롤백 " + rolled + RECOVER_VERIFY)

    hits = []
    for r in rows:
        if ROW is not None and str(r.get("Name")) != ROW:
            continue
        if COLUMN not in r:
            continue
        old = r[COLUMN]
        if isinstance(old, bool) or not isinstance(old, (int, float)):
            continue
        new = (old * VALUE) if OP == "mult" else VALUE
        if isinstance(old, int) and float(new).is_integer():
            new = int(new)
        r[COLUMN] = new
        hits.append((str(r.get("Name")), old, new))

    if not hits:
        raise RuntimeError("패치 대상 0행: %s.%s%s — 컬럼/행 이름 오타?" % (DT_NAME, COLUMN, ("@" + ROW) if ROW else ""))

    # ⚠ fill 이 False 여도 표는 이미 비워졌을 수 있다(UE 는 임포트 전에 행을 지우고 시작한다)
    #    → 그냥 raise 하면 빈/부분 상태가 메모리에 남는다. 반드시 원본으로 되돌린다.
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows)):
        verify_fail("fill_data_table_from_json_string 실패: " + DT_NAME)

    # ── 라운드트립 무손실 검증 (저장 **전**) ──────────────────────────────────
    # export→fill 왕복은 컬럼을 조용히 잃을 수 있다. 손실된 표를 디스크에 굳히기 전에 되읽어 대조한다.
    back = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
    if not isinstance(back, list):
        back = [dict(v, Name=k) for k, v in back.items()]
    if len(back) != len(rows):
        verify_fail("라운드트립 행 수 불일치 %d -> %d" % (len(rows), len(back)))
    if rows and back:
        lost = sorted(set(rows[0].keys()) - set(back[0].keys()))
        if lost:
            verify_fail("라운드트립 컬럼 유실 %s" % lost)
    by_name = {}
    for r in back:
        by_name[str(r.get("Name"))] = r
    for n, o, v in hits:
        got = by_name.get(n, {}).get(COLUMN)
        if got is None or abs(float(got) - float(v)) > max(1e-6, abs(float(v)) * 1e-6):
            verify_fail("패치 셀 미반영 %s.%s@%s: 기대 %s 실제 %s" % (DT_NAME, COLUMN, n, v, got))

    if not unreal.EditorAssetLibrary.save_asset(DT_PATH, only_if_is_dirty=False):
        raise RuntimeError("save_asset 실패: " + DT_PATH + RECOVER_SAVE)

    # 저장 검증 — 메모리만 바뀌고 디스크 저장이 실패하면 에디터 종료 시 통째로 날아간다.
    after_mtime = os.path.getmtime(UASSET)
    after_size = os.path.getsize(UASSET)
    if after_mtime <= before_mtime and after_size == before_size:
        raise RuntimeError("uasset mtime/size 무변화 — 디스크 저장 실패(에디터 이중 실행 의심): " + UASSET + RECOVER_SAVE)

    verify = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, COLUMN)
    unreal.log("[BALV-TUNE] OK %s.%s rows=%d mtime %.3f->%.3f size %d->%d" % (DT_NAME, COLUMN, len(hits), before_mtime, after_mtime, before_size, after_size))
    unreal.log("[BALV-TUNE] backup=" + BACKUP)
    for n, o, v in hits[:5]:
        unreal.log("[BALV-TUNE]   %s: %s -> %s" % (n, o, v))
    if verify:
        unreal.log("[BALV-TUNE] verify %s[0:3]=%s" % (COLUMN, str(verify[:3])))
except Exception:
    unreal.log_error(traceback.format_exc())
`;
}

// 개수를 그대로 돌려준다 — 2개 이상이면 uasset 저장이 실패하고(Error Code 32) remote 가 엉뚱한
// 프로젝트 에디터에 붙을 수 있어, 호출부가 그 사실을 알아야 한다.
// 구현은 Tools/_Shared/webshell/proc.js — QA Control Center 와 같은 판정을 써야 하므로 공용이다.
const { editorProcessCount } = require('../_Shared/webshell/proc.js');

// DataImport/<표>_Import.csv 가 있으면 그 표는 CSV 가 소스다 — 리임포트 한 번이면 DT 직접 패치가 지워진다.
function csvSourceOf(table, repoRoot = REPO_ROOT) {
  const p = path.join(repoRoot, 'DataImport', `${table}_Import.csv`);
  return fs.existsSync(p) ? p : null;
}

// 시뮬이 실제로 읽는 DT — sim/*.js 를 스캔한다(명시 목록은 sim 이 바뀌면 조용히 낡는다).
//
// 동적 표 이름(`DT_Project_${INDUSTRY}`)은 접두사로 뭉뚱그리면 안 된다 — world.js 는 INDUSTRY='Game'
// 고정이라 나머지 5산업 표는 **안 읽는데**, 접두사 매칭은 그 60여 칸을 전부 "소비"로 오판한다.
// 그래서 변수의 고정값을 sim 소스에서 실독해 실제 표 이름으로 해석한다.
// 못 풀면 접두사를 uncertain 으로 남긴다 — 조용히 소비/미소비 어느 쪽으로도 단정하지 않는다.
function simConsumption(simDir = path.join(BALANCE_DIR, 'sim')) {
  let text = '';
  for (const f of fs.readdirSync(simDir)) if (f.endsWith('.js')) text += `${fs.readFileSync(path.join(simDir, f), 'utf8')}\n`;

  // const NAME = '리터럴' — 이름은 대소문자 무시로 찾는다(상수 INDUSTRY ↔ 파라미터 industry 관례).
  const consts = new Map();
  for (const m of text.matchAll(/\bconst\s+([A-Za-z_$][\w$]*)\s*=\s*['"]([^'"]*)['"]\s*;/g)) {
    consts.set(m[1].toLowerCase(), m[2]);
  }

  const tables = new Set();
  const uncertain = [];
  for (const m of text.matchAll(/(?:values\.dt|tryDt)\(\s*['"`]([^'"`]+)['"`]/g)) {
    const raw = m[1];
    if (!raw.includes('${')) {
      tables.add(raw);
      continue;
    }
    const resolved = raw.replace(/\$\{\s*([A-Za-z_$][\w$]*)\s*\}/g, (_, v) => {
      const hit = consts.get(v.toLowerCase());
      return hit === undefined ? '\0' : hit;
    });
    if (resolved.includes('\0')) uncertain.push(raw.slice(0, raw.indexOf('${')));
    else tables.add(resolved);
  }
  return { tables, uncertain, text };
}
const escapeRe = s => String(s).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

function simConsumes(table, column, c = simConsumption()) {
  if (!c.tables.has(table)) {
    // 접두사가 걸리면 "안 읽는다"고 단정할 수 없다 — 불확실로 표시한다.
    if ((c.uncertain || []).some(p => p && table.startsWith(p))) return { table: false, column: false, uncertain: true };
    return { table: false, column: false, uncertain: false };
  }
  return { table: true, column: new RegExp(`\\b${escapeRe(column)}\\b`).test(c.text), uncertain: false };
}

// 곡선 params 현재값 — 안내에 "지금 몇인지"를 같이 띄우려고 SOT 파일에서 직접 읽는다.
// JS 소스를 정규식으로 긁던 방식은 Studio 가 값을 바꾸는 순간 조용히 낡는다 → params JSON 이 SOT.
const CURVE_PARAM_KEYS = ['ANCHOR', 'TIER_RATIO', 'INTRA_RATIO', 'COST_COEF', 'COST_FLOOR'];
function reseedParams(paramsPath = CURVE_PARAMS_JSON) {
  const out = {};
  if (!fs.existsSync(paramsPath)) return out;
  let json;
  try {
    json = JSON.parse(fs.readFileSync(paramsPath, 'utf8'));
  } catch {
    return out; // 깨진 JSON 은 안내에서 ⚠ 로 드러난다 — 표시용 헬퍼가 CLI 를 죽이지 않게.
  }
  if (!json || typeof json !== 'object') return out;
  for (const k of CURVE_PARAM_KEYS) if (Number.isFinite(json[k])) out[k] = json[k];
  return out;
}

// 곡선 노브 → 곡선 params 이름. 값이 아니라 "어느 노브를 만지는가"의 매핑.
const CURVE_PARAM = { devCostCoef: 'COST_COEF', reqCurve: 'ANCHOR' };

const relRepo = p => path.relative(REPO_ROOT, p).replace(/\\/g, '/');

function curveGuidance(knob, spec) {
  const P = reseedParams();
  const param = CURVE_PARAM[knob];
  const cur = param ? P[param] : undefined;
  const mult = 'mult' in spec ? spec.mult : null;
  const next = Number.isFinite(cur) && mult !== null ? Number((cur * mult).toPrecision(6)) : null;
  const lines = [
    `${knob} 은 100행 곡선 리시드 노브입니다 — tune.js 가 직접 쓰지 않고 기존 파이프라인으로 넘깁니다.`,
    '',
    `1) ${relRepo(CURVE_PARAMS_JSON)} 의 값을 고칩니다(곡선 SOT — JS 소스는 더 이상 손대지 않습니다).`,
  ];
  if (param) {
    lines.push(`     ${param}: ${cur ?? '?'}  →  ${next ?? `${cur ?? '?'} × ${mult ?? '?'}`}`);
    if (!Number.isFinite(cur)) lines.push(`     ⚠ ${relRepo(CURVE_PARAMS_JSON)} 에서 ${param} 을 못 읽었습니다 — 파일이 없거나 JSON 이 깨졌습니다.`);
    if (knob === 'reqCurve') lines.push('     (ANCHOR 는 곡선 전체를 균일 스케일합니다. 티어 기울기를 바꾸려면 TIER_RATIO 를 만지세요.)');
  }
  lines.push(
    '',
    `2) CSV 리시드 — 먼저 리포트로 확인하고(쓰기 없음), 그 다음 --write 로 기록합니다. 대상은 Game/Finance/IT 3표뿐입니다:`,
    `     node ${relRepo(RESEED_JS)}`,
    `     node ${relRepo(RESEED_JS)} --write`,
    '',
    '3) 에디터에 리임포트(에디터 실행 중이어야 합니다):',
    `     PYTHONIOENCODING=utf-8 py "Tools/RemoteExec/remote_cmd.py" "exec(open(r'${REIMPORT_PY.replace(/\\/g, '/')}').read())"`,
    '',
    '4) 확인 후 PIE 재시작 시 반영됩니다. (TableManager 는 생성자 로드 — 에디터 재시작 권장)',
    '',
    '※ Balance Studio 는 1~3 을 백업·리드백·롤백이 붙은 한 트랜잭션으로 실행합니다 — 손으로 돌릴 땐 위 순서를 지키세요.',
  );
  return lines.join('\n');
}

function refuseGuidance(knob, route, cat) {
  const row = cat.rows.find(r => r.key === knob);
  const where = row ? row.how : '(카탈로그 미등록)';
  if (route === 'cdo') {
    return [
      `${knob} 은 cdo 라우트입니다 — tune.js 로 못 바꿉니다.`,
      `  에디터 BP 프로퍼티: ${where.replace(/^에디터 BP: /, '')}`,
      '  블루프린트를 열어 Class Defaults 에서 값을 고친 뒤 저장하세요.',
      '  (바꾼 뒤 값 재추출: extract/extract_editor.py → extract/merge_values.js)',
    ].join('\n');
  }
  return [
    `${knob} 은 src 라우트입니다 — tune.js 로 못 바꿉니다.`,
    `  코드 수정 필요: ${where.replace(/^코드\+빌드: /, '')}`,
    '  C++ 상수라 수정 후 빌드가 동반됩니다(Live Coding 또는 풀빌드).',
    '  자주 만질 값이면 UPROPERTY(config) 승격을 먼저 검토하세요 — Tools/Balance/TUNING.md 의 승격 후보표 참조.',
  ].join('\n');
}

function runApply({
  knob, value, valuesPath = DEFAULT_VALUES, repoRoot = REPO_ROOT,
  scriptDir = SCRIPT_DIR, manifestEditorPath = MANIFEST_EDITOR,
  manifestSrcPath = MANIFEST_SRC, scanPath = DEFAULT_SCAN,
  force = false, allRows = false,
  editorCheck = editorProcessCount, exec = defaultExec, log = console.log,
} = {}) {
  const valuesDoc = loadValuesDoc(valuesPath);
  const parsed = parseKnob(knob);
  const route = routeOf(parsed, valuesDoc);
  const spec = parseValueSpec(value, { multOnly: parsed.kind === 'dt-curve' });

  if (route === 'dt-curve') {
    log(curveGuidance(parsed.name, spec));
    return { status: 'delegated', route };
  }
  if (route !== 'dt') {
    log(refuseGuidance(knob, route, buildCatalog({ valuesPath, scanPath, manifestSrcPath, manifestEditorPath, repoRoot })));
    return { status: 'refused', route };
  }

  const dtPaths = readJson(manifestEditorPath, 'manifest_editor.json').dtPaths || {};
  const dtPath = dtPaths[parsed.table];
  if (!dtPath) throw new TuneError(`manifest_editor.json 에 없는 DT: ${parsed.table} — dtPaths 에 등록된 표만 패치할 수 있습니다.`);
  const rows = valuesDoc.dt[parsed.table];
  if (!Array.isArray(rows)) throw new TuneError(`values.json 누락 DT: ${parsed.table}`);
  const targets = parsed.row === null ? rows : rows.filter(r => String(r.Name) === String(parsed.row));
  if (!targets.length) throw new TuneError(`DT 행 없음: ${parsed.table}@${parsed.row} — 행 이름 오타?`);
  const numeric = targets.filter(r => typeof r[parsed.column] === 'number');
  if (!numeric.length) throw new TuneError(`수치 셀 없음: ${parsed.table}.${parsed.column} — 컬럼명 오타?`);

  // 다행 + 절대값 = 곡선을 평평하게 만든다. 오타(@ 빠뜨림)로 일어나기 쉬워 명시 동의를 받는다.
  if (numeric.length > 1 && 'set' in spec && !allRows) {
    throw new TuneError([
      `${parsed.table}.${parsed.column} 의 ${numeric.length}개 행에 같은 절대값(${spec.set})을 씁니다 — 곡선이 평평해집니다.`,
      `  행 하나만 바꾸려면: dt:${parsed.table}.${parsed.column}@<행이름> ${spec.set}`,
      '  전 행을 상대 조정하려면 배율을 쓰세요(x1.2 / -20%) — 배율은 이 제한이 없습니다.',
      '  정말 전 행을 같은 값으로 덮으려면 --all-rows 를 붙이세요.',
    ].join('\n'));
  }

  const uassetPath = uassetPathOf(dtPath, repoRoot);
  const stamp = new Date().toISOString().replace(/[-:T]/g, '').slice(0, 14);
  const safe = `${parsed.table}-${parsed.column}${parsed.row ? `-${parsed.row}` : ''}`.replace(/[^\w.-]/g, '_');
  const dir = String(scriptDir).replace(/\\/g, '/').replace(/\/+$/, '');
  const scriptPath = `${dir}/balv_tune_${safe}.py`;
  const backupPath = `${dir}/balv_tune_backup/${parsed.table}-${stamp}.uasset`;
  const script = buildPatchScript({
    table: parsed.table, dtPath, column: parsed.column, row: parsed.row, spec, uassetPath, backupPath,
  });
  fs.mkdirSync(scriptDir, { recursive: true });
  fs.writeFileSync(scriptPath, script, 'utf8');

  log(`적용 대상 — ${parsed.table}.${parsed.column}${parsed.row ? `@${parsed.row}` : ''} ${specLabel(spec)} (수치 행 ${numeric.length}개)`);
  for (const r of numeric.slice(0, 5)) {
    const before = r[parsed.column];
    const after = 'set' in spec ? spec.set : before * spec.mult;
    log(`  ${r.Name}: ${trimNum(before)} → ${trimNum(after)}`);
  }
  if (numeric.length > 5) log(`  ... 외 ${numeric.length - 5}행`);
  log(`패치 스크립트: ${scriptPath}`);

  const csvPath = csvSourceOf(parsed.table, repoRoot);
  if (csvPath) {
    log('');
    log(`⚠ ${parsed.table} 은 CSV 가 소스입니다 — 리임포트 시 이 튜닝은 되돌아갑니다.`);
    log(`  소스: ${path.relative(repoRoot, csvPath).replace(/\\/g, '/')}`);
    log('  값을 확정하면 CSV 에도 같은 수정을 반영하세요(안 하면 다음 리임포트에 조용히 사라집니다).');
  }

  const editorCount = Number(editorCheck()) || 0;
  if (editorCount >= 2 && !force) {
    throw new TuneError([
      `UnrealEditor 프로세스가 ${editorCount}개 떠 있습니다 — 패치를 중단했습니다.`,
      '  ① uasset 저장이 실패합니다(다른 인스턴스가 파일을 잡음 — Error Code 32).',
      '  ② 원격 연결이 어느 에디터에 붙을지 보장되지 않습니다(다른 프로젝트일 수 있음).',
      '  안 쓰는 인스턴스를 종료한 뒤 다시 실행하세요. 의도한 상황이면 --force 를 붙이세요.',
      `  스크립트는 이미 만들어 뒀습니다: ${scriptPath}`,
    ].join('\n'));
  }

  if (editorCount < 1) {
    log('');
    log('에디터(UnrealEditor)가 실행 중이 아닙니다 — 패치는 실행하지 않았습니다.');
    log('에디터를 실행한 뒤 같은 명령을 다시 실행하거나, 콘솔(Python)에 아래 한 줄을 붙여넣으세요:');
    log(`  exec(open(r"${scriptPath}").read())`);
    return { status: 'pending-editor', route, scriptPath, backupPath, script, csvBacked: Boolean(csvPath) };
  }

  log('');
  log(`에디터 감지${editorCount > 1 ? ` (${editorCount}개 — --force)` : ''} — 원격 실행합니다.`);
  const out = exec(scriptPath);
  log(out);

  // py 의 실패는 전부 log_error 로 삼켜져 종료코드에 안 나온다 — 성공 마커가 유일한 증거다.
  if (!/\[BALV-TUNE\] OK\b/.test(String(out))) {
    throw new TuneError([
      '원격 패치가 성공 마커([BALV-TUNE] OK)를 내지 않았습니다 — 적용되지 않았을 가능성이 큽니다.',
      '  위 원격 출력의 traceback 이 실제 원인입니다(py 는 log_error 로만 찍습니다).',
      `  백업: ${backupPath}`,
      `  복구: 에디터를 끄고 위 백업 파일을 ${uassetPath} 에 덮어쓴 뒤 에디터를 다시 켜세요.`,
      `  스크립트: ${scriptPath}`,
    ].join('\n'));
  }

  log('');
  log(`백업: ${backupPath}`);
  log('PIE 재시작 시 반영됩니다. (TableManager 는 생성자 로드 — 값이 안 보이면 에디터 재시작)');
  return { status: 'applied', route, scriptPath, backupPath, script, output: out, csvBacked: Boolean(csvPath) };
}

function defaultExec(scriptPath) {
  try {
    return String(execFileSync('py', [REMOTE_CMD, `exec(open(r'${scriptPath}').read())`], {
      encoding: 'utf8',
      cwd: REPO_ROOT,
      timeout: EXEC_TIMEOUT_MS,
      env: { ...process.env, PYTHONIOENCODING: 'utf-8' },
    }));
  } catch (e) {
    const why = e.killed || e.signal ? `${EXEC_TIMEOUT_MS / 1000}초 타임아웃` : (e.code === 'ENOENT' ? '`py` 실행 파일을 못 찾음' : e.message);
    throw new TuneError([
      `원격 실행 실패 — ${why}`,
      `  커맨드: py "${REMOTE_CMD}" "exec(open(r'${scriptPath}').read())"`,
      '  에디터가 켜져 있고 Python 원격 실행(bRemoteExecution)이 활성인지 확인하세요.',
      String(e.stdout || '').trim() && `  원격 출력: ${String(e.stdout).trim()}`,
    ].filter(Boolean).join('\n'));
  }
}

// ── CLI ─────────────────────────────────────────────────────────────────────
const USAGE = `빠른 밸런스 튜닝

  node Tools/Balance/tune.js list [필터] [--dt <Table>]
  node Tools/Balance/tune.js preview <노브> <값> [--profile mid] [--seeds 30]
  node Tools/Balance/tune.js apply <노브> <값> [--all-rows] [--force]

노브
  <values 키>                 예: quality.gradeLifespanMin  (list 로 확인)
  devCostCoef | reqCurve      DT 곡선 배율 노브(리시드 위임)
  dt:<Table>.<Column>[@<Row>] DT 셀/컬럼 직접 지정 — apply 가 실제로 쓰는 형태

값
  1.8      절대값 (다행 대상이면 --all-rows 필요)
  x1.2     배율 (곡선 노브는 맨숫자도 배율)
  +20%     배율(퍼센트)

종료 코드
  0 적용/위임 · 1 거부·오류 · 2 미적용(에디터 없음 — 스크립트만 생성)
`;

function parseArgs(argv) {
  const a = {
    cmd: argv[0], rest: [], profile: DEFAULT_PROFILE, seeds: DEFAULT_SEEDS,
    values: DEFAULT_VALUES, scan: DEFAULT_SCAN, dt: null, force: false, allRows: false,
  };
  for (let i = 1; i < argv.length; i++) {
    const k = argv[i];
    const v = argv[i + 1];
    switch (k) {
      case '--profile': a.profile = v; i++; break;
      case '--seeds': a.seeds = parseInt(v, 10); i++; break;
      case '--values': a.values = v; i++; break;
      case '--scan': a.scan = v; i++; break;
      case '--dt': a.dt = v; i++; break;
      case '--force': a.force = true; break;
      case '--all-rows': a.allRows = true; break;
      default: a.rest.push(k); break;
    }
  }
  if (a.cmd === 'preview' && (!Number.isFinite(a.seeds) || a.seeds < 1)) throw new TuneError('--seeds 는 1 이상의 정수여야 합니다.');
  return a;
}

// --dt <Table> — apply 에 쓸 컬럼 이름을 찾는 용도(값은 values.json 기준).
function formatDtColumns(table, valuesPath) {
  const doc = loadValuesDoc(valuesPath);
  const rows = doc.dt[table];
  if (!Array.isArray(rows) || !rows.length) throw new TuneError(`values.json 누락 DT: ${table} — 표 이름을 확인하세요.`);
  const cols = new Map();
  for (const r of rows) {
    for (const [k, v] of Object.entries(r)) {
      if (typeof v !== 'number') continue;
      const c = cols.get(k) || { min: v, max: v, n: 0 };
      c.min = Math.min(c.min, v); c.max = Math.max(c.max, v); c.n++;
      cols.set(k, c);
    }
  }
  if (!cols.size) throw new TuneError(`${table} 에 수치 컬럼이 없습니다.`);
  const out = [`${table} — 수치 컬럼 ${cols.size}종 / 행 ${rows.length}개 (행 이름 예: ${rows.slice(0, 3).map(r => r.Name).join(', ')})`, ''];
  const w = Math.max(...[...cols.keys()].map(dispLen));
  for (const [k, c] of cols) out.push(`  dt:${table}.${pad(k, w)}  ${trimNum(c.min)} ~ ${trimNum(c.max)}  (${c.n}행)`);
  return out.join('\n');
}

function main(argv) {
  try {
    const a = parseArgs(argv); // try 안 — 인자 검증 TuneError 가 스택으로 새면 안 된다
    if (a.cmd === 'list') {
      if (a.dt) {
        console.log(formatDtColumns(a.dt, a.values));
        return 0;
      }
      const cat = buildCatalog({ valuesPath: a.values, scanPath: a.scan });
      const filter = a.rest[0];
      if (filter) cat.rows = cat.rows.filter(r => r.key.toLowerCase().includes(filter.toLowerCase()));
      console.log(formatCatalogTable(cat));
      console.log('');
      if (!cat.scanned) console.log('⚠ out/scan.json 이 없어 감도가 전부 미스캔입니다 — node Tools/Balance/verify/scan.js 로 채우세요.');
      console.log(`노브 ${cat.rows.length}종 · 라우트: dt=즉시 적용(tune.js apply) / dt*=곡선 리시드 / cdo=에디터 BP / src=코드+빌드`);
      console.log(`DT 셀 직접 지정: dt:<Table>.<Column>[@<Row>] — 컬럼 목록은 \`tune.js list --dt <Table>\` (표 ${Object.keys(cat.dtPaths).length}종)`);
      return 0;
    }
    if (a.cmd === 'preview') {
      const [knob, value] = a.rest;
      if (!knob || value === undefined) throw new TuneError('사용법: tune.js preview <노브> <값>');
      const p = runPreview({ knob, value, valuesPath: a.values, scanPath: a.scan, profile: a.profile, seeds: a.seeds });
      console.log(formatPreview(p));
      return 0;
    }
    if (a.cmd === 'apply') {
      const [knob, value] = a.rest;
      if (!knob || value === undefined) throw new TuneError('사용법: tune.js apply <노브> <값>');
      const r = runApply({ knob, value, valuesPath: a.values, force: a.force, allRows: a.allRows });
      if (r.status === 'refused') return 1;
      return r.status === 'pending-editor' ? 2 : 0; // 2 = 미적용(스크립트만) — 스크립트가 성공으로 안 읽히게
    }
    console.log(USAGE);
    return a.cmd === undefined || a.cmd === '--help' || a.cmd === '-h' ? 0 : 1;
  } catch (e) {
    if (e instanceof TuneError) {
      console.error(`\n오류: ${e.message}\n`);
      return 1;
    }
    throw e;
  }
}

if (require.main === module) process.exitCode = main(process.argv.slice(2));

module.exports = {
  TuneError, parseKnob, routeOf, parseValueSpec, isCurveKnob,
  buildCatalog, formatCatalogTable, sensOf, indexScan, srcLocations,
  knobOverride, writePatchedValues, summarizeRuns, runPreview, formatPreview,
  buildPatchScript, uassetPathOf, curveGuidance, refuseGuidance, runApply,
  formatDtColumns, reseedParams, parseArgs, main,
  csvSourceOf, simConsumption, simConsumes, editorProcessCount,
  DEFAULT_VALUES, DEFAULT_SCAN, DEFAULT_PROFILE, DEFAULT_SEEDS,
};
