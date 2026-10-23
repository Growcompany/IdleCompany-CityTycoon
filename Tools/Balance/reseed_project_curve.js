#!/usr/bin/env node
'use strict';
// DT_Project_*.csv 요구치/개발비 리시드 — 순수 계산 + 명시적 CLI.
//   node Tools/Balance/reseed_project_curve.js                 리포트만(파일 안 씀)
//   node Tools/Balance/reseed_project_curve.js --write         변경된 CSV 만 기록
//   node Tools/Balance/reseed_project_curve.js --params '{"ANCHOR":140}' --write
//
// 곡선 파라미터의 SOT = project_curve_params.json (2026-08-04 이전엔 이 파일의 PARAMS 상수였다).
// require() 는 계산 함수만 노출한다 — 읽기/쓰기/출력은 planProjectCurve/main 을 명시적으로 부를 때만 일어난다.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const BALANCE_DIR = __dirname;
const REPO_ROOT = path.resolve(BALANCE_DIR, '..', '..');
const DEFAULT_CSV_DIR = path.join(REPO_ROOT, 'DataImport');
const PARAMS_PATH = path.join(BALANCE_DIR, 'project_curve_params.json');

// 곡선 대상은 프로젝트형 3산업뿐. 제조업(Semiconductor/Automobile/Electronics)은 Weight_* 저작이 선행돼야 해서 절대 리시드하지 않는다.
const PROJECT_CURVE_TARGETS = Object.freeze([
  Object.freeze({ table: 'DT_Project_Game', csvName: 'DT_Project_Game_Import.csv' }),
  Object.freeze({ table: 'DT_Project_Finance', csvName: 'DT_Project_Finance_Import.csv' }),
  Object.freeze({ table: 'DT_Project_IT', csvName: 'DT_Project_IT_Import.csv' }),
]);

// 파라미터 의미(값의 근거는 project_curve_params.json 을 미는 쪽 문서 — TUNING.md / 밸런스 스펙):
//   요구 곡선  ΣTargetScore(idx) = ANCHOR × TIER_RATIO^(T-1) × INTRA_RATIO^(i-1),  T=티어(1~10), i=티어 내 위치(1~10)
//   개발비      max(COST_FLOOR, round(COST_COEF × idx²))
const PARAM_KEYS = Object.freeze(['ANCHOR', 'TIER_RATIO', 'INTRA_RATIO', 'COST_COEF', 'COST_FLOOR']);

const STEP_COLUMNS = Object.freeze(['RequiredScore_Step1', 'RequiredScore_Step2', 'RequiredScore_Step3', 'RequiredScore_Step4']);
const WEIGHT_COLUMNS = Object.freeze(['Weight_Plan', 'Weight_Dev', 'Weight_Graphics', 'Weight_Sound', 'Weight_Server', 'Weight_QA']);
const REQUIRED_COLUMNS = Object.freeze(['ProjectIndex', 'Duration', 'DevelopmentCost', ...STEP_COLUMNS, ...WEIGHT_COLUMNS]);
const PROJECT_ROWS = 100;

function normalizeParams(input) {
  if (!input || typeof input !== 'object' || Array.isArray(input)) {
    throw new Error(`곡선 params 는 ${PARAM_KEYS.join('/')} 5개를 담은 객체여야 합니다.`);
  }
  const missing = PARAM_KEYS.filter(k => !(k in input));
  if (missing.length) throw new Error(`곡선 params 키 누락: ${missing.join(', ')}`);
  const unknown = Object.keys(input).filter(k => !PARAM_KEYS.includes(k));
  if (unknown.length) throw new Error(`곡선 params 미지 키: ${unknown.join(', ')} — 허용 키는 ${PARAM_KEYS.join(', ')} 뿐입니다.`);
  const out = {};
  for (const k of PARAM_KEYS) {
    const v = input[k];
    if (typeof v !== 'number' || !Number.isFinite(v) || v <= 0) {
      throw new Error(`곡선 params ${k} 는 0 보다 큰 유한 수여야 합니다 (받은 값: ${JSON.stringify(v)})`);
    }
    out[k] = v;
  }
  return Object.freeze(out);
}

function readParamsFile(paramsPath = PARAMS_PATH) {
  let text;
  try {
    text = fs.readFileSync(paramsPath, 'utf8');
  } catch (e) {
    throw new Error(`곡선 params 파일을 못 읽었습니다: ${paramsPath} — ${e.message}`);
  }
  let json;
  try {
    json = JSON.parse(text);
  } catch (e) {
    throw new Error(`곡선 params JSON 파싱 실패: ${paramsPath} — ${e.message}`);
  }
  return normalizeParams(json);
}

const DEFAULT_PARAMS = readParamsFile();

// ── CSV: 손 안 댄 바이트를 그대로 두는 최소 파서 ─────────────────────────────
// 따옴표 안의 개행까지 살려야 재직렬화가 무손실이다. 필드는 raw(따옴표 포함)로 들고 있다가 그대로 join 한다.
function splitLines(text) {
  const out = [];
  let start = 0;
  let inQ = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '"') inQ = !inQ;
    else if (c === '\n' && !inQ) {
      const cr = i > start && text[i - 1] === '\r';
      out.push({ text: text.slice(start, cr ? i - 1 : i), eol: cr ? '\r\n' : '\n' });
      start = i + 1;
    }
  }
  if (start < text.length) out.push({ text: text.slice(start), eol: '' });
  return out;
}

function splitFields(line) {
  const out = [];
  let cur = '';
  let inQ = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (inQ) {
      cur += c;
      if (c === '"') {
        if (line[i + 1] === '"') { cur += '"'; i++; } else inQ = false;
      }
    } else if (c === '"') { cur += c; inQ = true; } else if (c === ',') { out.push(cur); cur = ''; } else cur += c;
  }
  out.push(cur);
  return out;
}

const decodeField = f => (f.length >= 2 && f.startsWith('"') && f.endsWith('"') ? f.slice(1, -1).replace(/""/g, '"') : f);
const serializeLines = lines => lines.map(l => l.text + l.eol).join('');
const sha256 = text => crypto.createHash('sha256').update(text, 'utf8').digest('hex');

// 빈 문자열은 Number('') === 0 이라 숫자로 새 들어온다 — 명시적으로 걸러야 "값 없음"이 0 으로 둔갑하지 않는다.
function toNumber(raw) {
  const s = decodeField(raw === undefined ? '' : raw).trim();
  if (s === '') return NaN;
  const n = Number(s);
  return Number.isFinite(n) ? n : NaN;
}

// ── 한 행의 곡선 계산 (2026-07-29 구현을 그대로 옮긴 것 — 수식을 바꾸지 말 것) ──
function computeRow(idx, weights, oldSteps, params) {
  const T = Math.ceil(idx / 10);
  const i = idx - (T - 1) * 10;

  const sigmaTarget = params.ANCHOR
    * Math.pow(params.TIER_RATIO, T - 1)
    * Math.pow(params.INTRA_RATIO, i - 1);

  // ΣTargetScore = TargetBase × ΣW / maxW  (OfficeStageProgressManager.cpp:254-256)
  // TargetBase   = ((S1+S2+S3)/3) × 2      → S1+S2+S3 = TargetBase × 1.5
  const sumW = weights.reduce((a, b) => a + b, 0);
  const maxW = Math.max(...weights);
  const targetBase = sigmaTarget * maxW / sumW;
  const sumS = targetBase * 1.5;

  // 기존 Step1~3 상대비를 유지하며 합만 맞춘다 (프로젝트별 개성 보존).
  // 최대잔여법(largest remainder) — 독립 반올림은 합이 어긋나 ΣTarget 오차가 최대 18% 났다.
  const oldSum = oldSteps.reduce((a, b) => a + b, 0);
  const targetSum = Math.max(3, Math.round(sumS));      // 스텝당 최소 1 보장
  const raw = oldSteps.map(v => targetSum * v / oldSum);
  const newS = raw.map(v => Math.max(1, Math.floor(v)));
  let rem = targetSum - newS.reduce((a, b) => a + b, 0);
  const order = raw.map((v, k) => ({ k, frac: v - Math.floor(v) }))
    .sort((a, b) => b.frac - a.frac);
  for (let n = 0; rem > 0; n++, rem--) { newS[order[n % 3].k] += 1; }
  // Step4 = 죽은 컬럼이지만 UI 4곳이 표시 → 평균값으로 채운다 (m-1)
  const s4 = Math.max(1, Math.round((newS[0] + newS[1] + newS[2]) / 3));

  const cost = Math.max(params.COST_FLOOR, Math.round(params.COST_COEF * idx * idx));
  // 실제 반영된 ΣTargetScore 역산 (반올림 후)
  const actualBase = (newS[0] + newS[1] + newS[2]) / 3 * 2;
  return { T, i, want: sigmaTarget, got: actualBase * sumW / maxW, steps: [newS[0], newS[1], newS[2], s4], cost };
}

// ── 한 표 리시드 (순수: text in, text out — 파일을 열지 않는다) ──────────────
function reseedProjectCsv({ text, params = DEFAULT_PARAMS, table = null, csvName = null } = {}) {
  if (typeof text !== 'string') throw new Error('reseedProjectCsv: text(CSV 문자열)가 필요합니다.');
  const P = normalizeParams(params);
  const label = table || csvName || 'CSV';
  const fail = msg => { throw new Error(`${label}: ${msg}`); };

  const lines = splitLines(text);
  if (!lines.length) fail('빈 파일입니다.');
  // BOM 은 이름 비교에서만 벗긴다 — raw 필드에는 그대로 남아 재직렬화 때 되돌아간다.
  const header = splitFields(lines[0].text).map((f, i) => decodeField(i === 0 ? f.replace(/^\uFEFF/, '') : f));
  const colOf = name => header.indexOf(name);
  const missing = REQUIRED_COLUMNS.filter(c => colOf(c) < 0);
  if (missing.length) fail(`필수 컬럼 없음: ${missing.join(', ')} — 이 산업은 컬럼 저작이 선행되어야 합니다.`);

  const cIdx = colOf('ProjectIndex');
  const cCost = colOf('DevelopmentCost');
  const cStep = STEP_COLUMNS.map(colOf);
  const cW = WEIGHT_COLUMNS.map(colOf);
  const cDur = colOf('Duration');

  // ── 검증 먼저 (계산/기록 전에 전량 확인 — 반쯤 리시드된 CSV 를 만들지 않는다) ──
  const parsed = [];
  const seen = new Map();
  for (let r = 1; r < lines.length; r++) {
    if (lines[r].text === '') continue;              // 후행 빈 줄
    const fields = splitFields(lines[r].text);
    const rowNo = r + 1;
    const idx = toNumber(fields[cIdx]);
    if (!Number.isInteger(idx)) fail(`${rowNo}행 ProjectIndex 가 정수가 아닙니다: ${JSON.stringify(decodeField(fields[cIdx] || ''))}`);
    if (idx < 1 || idx > PROJECT_ROWS) fail(`${rowNo}행 ProjectIndex ${idx} 가 1~100 범위 밖입니다.`);
    if (seen.has(idx)) fail(`ProjectIndex ${idx} 가 중복입니다 (${seen.get(idx)}행, ${rowNo}행).`);
    seen.set(idx, rowNo);

    const steps = [0, 1, 2].map(k => toNumber(fields[cStep[k]]));
    steps.forEach((v, k) => {
      if (!Number.isFinite(v) || v <= 0) fail(`${rowNo}행 ${STEP_COLUMNS[k]} 는 양수여야 합니다 (받은 값: ${JSON.stringify(decodeField(fields[cStep[k]] || ''))}).`);
    });

    const weights = cW.map((c, k) => {
      const v = toNumber(fields[c]);
      // 0 은 정상 — 그 프로젝트가 안 쓰는 직능이다. 음수/비수치만 거부한다.
      if (!Number.isFinite(v) || v < 0) fail(`${rowNo}행 ${WEIGHT_COLUMNS[k]} 는 0 이상의 수여야 합니다 (받은 값: ${JSON.stringify(decodeField(fields[c] || ''))}).`);
      return v;
    });
    const sumW = weights.reduce((a, b) => a + b, 0);
    if (sumW <= 0) fail(`${rowNo}행 Weight_* 합이 0 입니다 — 직능 가중치 저작이 선행되어야 합니다.`);

    const dur = toNumber(fields[cDur]);
    if (!Number.isFinite(dur) || dur <= 0) fail(`${rowNo}행 Duration 은 양수여야 합니다.`);
    if (!Number.isFinite(toNumber(fields[cCost]))) fail(`${rowNo}행 DevelopmentCost 가 수치가 아닙니다.`);

    parsed.push({ line: r, fields, idx, steps, weights, dur });
  }
  if (parsed.length !== PROJECT_ROWS) fail(`데이터 행이 ${parsed.length}개입니다 — 정확히 ${PROJECT_ROWS}개여야 합니다.`);

  // ── 계산 + 대상 셀만 교체 ────────────────────────────────────────────────
  const outLines = lines.slice();
  const report = [];
  let changedCells = 0;
  for (const row of parsed) {
    const c = computeRow(row.idx, row.weights, row.steps, P);
    const fields = row.fields.slice();
    const put = (col, value) => {
      const next = String(value);
      if (fields[col] !== next) changedCells++;
      fields[col] = next;
    };
    cStep.forEach((col, k) => put(col, c.steps[k]));
    put(cCost, c.cost);
    outLines[row.line] = { text: fields.join(','), eol: lines[row.line].eol };
    report.push({ idx: row.idx, tier: c.T, want: c.want, got: c.got, cost: c.cost, duration: row.dur });
  }
  report.sort((a, b) => a.idx - b.idx);

  const afterText = serializeLines(outLines);
  return {
    table, csvName,
    beforeText: text, afterText,
    beforeSha256: sha256(text), afterSha256: sha256(afterText),
    rowCount: parsed.length, changedCells,
    status: afterText === text ? 'unchanged' : 'changed',
    rows: report,
    maxErrorPct: Math.max(...report.map(x => Math.abs((x.got - x.want) / x.want * 100))),
  };
}

// ── 3-target 계획 (읽기만 한다 — 기록은 main --write 또는 Studio 트랜잭션이 한다) ──
function planProjectCurve({ csvDir = DEFAULT_CSV_DIR, params = null, paramsPath = PARAMS_PATH } = {}) {
  const P = params ? normalizeParams(params) : readParamsFile(paramsPath);
  const targets = PROJECT_CURVE_TARGETS.map(t => {
    const csvPath = path.join(csvDir, t.csvName);
    let text;
    try {
      text = fs.readFileSync(csvPath, 'utf8');
    } catch (e) {
      throw new Error(`곡선 대상 CSV 를 못 읽었습니다: ${t.csvName} (${csvPath}) — ${e.message}`);
    }
    return { ...reseedProjectCsv({ text, params: P, table: t.table, csvName: t.csvName }), csvPath };
  });
  return {
    csvDir, params: P, targets,
    affectedTargets: targets.filter(t => t.status === 'changed'),
  };
}

// ── 리포트 ──────────────────────────────────────────────────────────────────
const pad = (s, n) => String(s).padStart(n);
// 운영: 시간=등급기본분×N (QualityGrade.h:92), RPS=10N, 감쇠적분 0.541(HLF 0.5)
const opRevC = N => 10 * N * (60 * 1.0 * N) * 0.541;

function formatTarget(t) {
  const out = [];
  const R = t.rows;
  out.push(`=== ${t.table || t.csvName} — ${t.rowCount}행 리시드 (${t.status === 'changed' ? `변경 셀 ${t.changedCells}개` : '변경 없음'}) ===`);
  out.push('idx  T    ΣTarget  오차%    C필요   S필요      개발비   운영(C)  운영수익(C)  회수율');
  for (const s of [1, 5, 10, 11, 25, 41, 50, 51, 75, 91, 100]) {
    const x = R.find(v => v.idx === s);
    if (!x) continue;
    const err = ((x.got - x.want) / x.want * 100).toFixed(1);
    const rev = opRevC(x.idx);
    const opMin = 1.0 * x.idx;
    out.push([
      pad(x.idx, 3), pad(x.tier, 2),
      pad(x.got.toFixed(1), 10), pad(err, 6),
      pad(x.got.toFixed(0), 8), pad((x.got * 1.8).toFixed(0), 8),
      pad(x.cost, 11),
      pad(opMin < 60 ? `${opMin.toFixed(0)}분` : `${(opMin / 60).toFixed(1)}시간`, 8),
      pad(rev.toFixed(0), 12), pad((rev / x.cost).toFixed(2), 7),
    ].join(' '));
  }
  const f = R[0];
  const l = R[R.length - 1];
  out.push(`총 배수: ΣTarget ×${(l.got / f.got).toFixed(1)} / 개발비 ×${(l.cost / f.cost).toFixed(1)} / 요구DPS ×${((l.got / l.duration) / (f.got / f.duration)).toFixed(1)}`);
  out.push(`반올림 최대 오차: ${t.maxErrorPct.toFixed(1)}%`);
  return out.join('\n');
}

function formatReseedReport(plan) {
  const out = [];
  for (const t of plan.targets) {
    out.push('');
    out.push(formatTarget(t));
  }
  // 티어별 소요시간(7판 클리어, C등급, 건물 1채 순차) — 표와 무관한 공통 축이라 한 번만 낸다.
  let cum = 0;
  const lines = [];
  for (let T = 1; T <= 10; T++) {
    let tierMin = 0;
    for (let k = 0; k < 7; k++) { const N = (T - 1) * 10 + 1 + k; tierMin += 1.0 * N; }
    cum += tierMin;
    lines.push(`T${T}:${(tierMin / 60).toFixed(1)}h`);
  }
  out.push('');
  out.push(`티어당 운영시간(7판·C·1채): ${lines.join(' ')}`);
  out.push(`전체 운영시간 합계: ${(cum / 60).toFixed(0)}시간 (건물 3채 병렬 시 ${(cum / 180).toFixed(0)}시간)`);
  out.push('');
  out.push(`곡선 params: ${PARAM_KEYS.map(k => `${k}=${plan.params[k]}`).join(' / ')}`);
  return out.join('\n');
}

// ── CLI ─────────────────────────────────────────────────────────────────────
const USAGE = `프로젝트 곡선 리시드 (DT_Project_Game / Finance / IT 3표만)

  node Tools/Balance/reseed_project_curve.js [--csv-dir <dir>] [--params <json|파일>] [--write]

  --params   곡선 파라미터. JSON 문자열('{"ANCHOR":140}') 은 현재 SOT 위에 얹는 부분 override,
             파일 경로는 5키 전체를 담은 JSON 이어야 한다.
             생략하면 Tools/Balance/project_curve_params.json (곡선 SOT).
  --csv-dir  CSV 폴더. 생략하면 <repo>/DataImport.
  --write    실제로 기록. 없으면 리포트만 내고 파일은 그대로 둔다.`;

function parseArgs(argv) {
  const a = { csvDir: DEFAULT_CSV_DIR, params: null, write: false };
  for (let i = 0; i < argv.length; i++) {
    const k = argv[i];
    if (k === '--write') { a.write = true; continue; }
    if (k === '--csv-dir' || k === '--params') {
      const v = argv[i + 1];
      if (v === undefined) throw new Error(`${k} 에 값이 없습니다.`);
      if (k === '--csv-dir') a.csvDir = v; else a.params = v;
      i++;
      continue;
    }
    throw new Error(`알 수 없는 인자: ${k}\n${USAGE}`);
  }
  return a;
}

// --params 는 인라인 JSON 도 파일 경로도 받는다. 인라인은 부분 override 라 현재 SOT 위에 얹는다.
function resolveParams(raw) {
  if (raw === null) return readParamsFile();
  const s = String(raw).trim();
  if (s.startsWith('{')) {
    let json;
    try {
      json = JSON.parse(s);
    } catch (e) {
      throw new Error(`--params JSON 파싱 실패: ${e.message}`);
    }
    return normalizeParams({ ...readParamsFile(), ...json });
  }
  return readParamsFile(s);
}

function main(argv = []) {
  try {
    const a = parseArgs(argv);
    const plan = planProjectCurve({ csvDir: a.csvDir, params: resolveParams(a.params) });
    console.log(formatReseedReport(plan));
    console.log('');
    if (!plan.affectedTargets.length) {
      console.log('변경 대상 없음 — 현재 CSV 가 이미 이 곡선입니다.');
      return 0;
    }
    const names = plan.affectedTargets.map(t => `${t.table}(${t.changedCells}셀)`).join(', ');
    if (!a.write) {
      console.log(`변경 예정: ${names}`);
      console.log('※ 미적용입니다 — 실제로 쓰려면 --write 를 붙이세요.');
      return 0;
    }
    for (const t of plan.affectedTargets) fs.writeFileSync(t.csvPath, t.afterText, 'utf8');
    console.log(`기록 완료: ${names}`);
    console.log('에디터 리임포트(에디터 실행 중이어야 합니다):');
    console.log(`  PYTHONIOENCODING=utf-8 py "Tools/RemoteExec/remote_cmd.py" "exec(open(r'${path.join(BALANCE_DIR, 'reimport_project_dt.py').replace(/\\/g, '/')}').read())"`);
    return 0;
  } catch (e) {
    console.error(`\n오류: ${e.message}\n`);
    return 1;
  }
}

if (require.main === module) process.exitCode = main(process.argv.slice(2));

module.exports = {
  DEFAULT_PARAMS,
  PROJECT_CURVE_TARGETS,
  normalizeParams,
  reseedProjectCsv,
  planProjectCurve,
  formatReseedReport,
  main,
};
