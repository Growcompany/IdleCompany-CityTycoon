#!/usr/bin/env node
'use strict';
// 정합성 어서션 스위트 — design §7.2 "파트 간 맞물림" 전 항목 + 커버리지 재표기 2건.
//   node Tools/Balance/verify/assertions.js [--values <values.json>] [--repo <repoRoot>] [--json]
//
// 3상태 결과: pass=true / pass=false(차단) / pass=false + known=true(알려진 결함 — 리포트에는 실리되
// 파이프라인은 죽이지 않는다). exit 1 은 "차단 실패"가 하나라도 있을 때만.
//
// 원칙:
//  - 게임 수치를 이 파일에 심지 않는다. 값은 전부 values.json/DT/소스에서 읽고, 여기 상수는
//    "검사 기준"(기대 곱, 베이스라인 목록)만 근거 주석과 함께 둔다.
//  - grep 형 어서션은 대상 파일/패턴을 못 찾으면 pass=false + 사유. 조용한 통과 금지.
const fs = require('node:fs');
const path = require('node:path');
const { loadValues } = require('../sim/values.js');
const { checkCoverage } = require('./coverage_check.js');
const { diamondBudget, marketCapMultiplier } = require('../sim/subecon.js');
const { buildingEffect } = require('../sim/enhance.js');
const { idlePerSecond } = require('../sim/idle.js');
const { runWorld } = require('../sim/world.js');

// ── 어서션 정의 상수 (게임 밸런스 수치가 아니라 "검사 기준") ──────────────────
// design §7.2: "OutputUnit(5.2) ↔ IdleIncomeScale(1.875) 역수 상쇄쌍 실효 곱 = 기대값".
// 두 값은 retune 에서 한쪽을 ×N, 다른 쪽을 ×1/N 한 쌍이라 곱이 불변량이어야 한다(둘 중 하나만
// 리튠되면 방치수익이 조용히 N배 틀어진다). 기대 곱은 그 불변량 자체 — values 에서 유도할 수 없다.
const IDLE_OFFSET_PRODUCT = 9.75;
const IDLE_OFFSET_TOL = 1e-9;

// A-UNWIRED 베이스라인 — 2026-08-02 실측(소비 경로 0건인 EItemType). 이 집합이 바뀌면(배선되거나
// 새 미배선 아이템이 생기면) 어서션이 실패해 모델/커버리지 갱신을 강제한다.
const UNWIRED_ITEM_BASELINE = [
  'AdditionalReroll', 'DepartmentTicket', 'DestructionShield',
  'EnhanceScroll', 'LuckyEnhanceTicket', 'ProtectionScroll',
];

// A-EXPGATE / A-MCSAT 은 시뮬 런에서 관측한다 — 재현성 위해 시드 고정.
const RUN_PROFILES = ['mid', 'low'];
const RUN_SEEDS = 5;

const SRC = 'Source/CompanyGrowthRenewal';
const P = {
  behavior: `${SRC}/Private/Entity/Officeworker/EmployeeBehaviorComponent.cpp`,
  operation: `${SRC}/Private/Manager/ProjectOperationManager.cpp`,
  qualityGrade: `${SRC}/Public/Enum/QualityGrade.h`,
  itemType: `${SRC}/Public/Enum/ItemType.h`,
  tradePort: `${SRC}/Private/Manager/TradePort.cpp`,
  worldMap: `${SRC}/Private/Manager/WorldMapManager.cpp`,
  productionOrder: `${SRC}/Private/Manager/ProductionOrderManager.cpp`,
};

// ── 소스 읽기 / C++ 근사 파서 ────────────────────────────────────────────────
class SourceMissing extends Error {}

function readSource(repoRoot, rel) {
  const p = path.join(repoRoot, rel);
  if (!fs.existsSync(p)) throw new SourceMissing(`소스 파일을 찾을 수 없음: ${rel}`);
  return fs.readFileSync(p, 'utf8');
}
function readLines(repoRoot, rel) { return readSource(repoRoot, rel).split(/\r?\n/); }

// 문자열/문자 리터럴 + 주석 제거 — 중괄호/패턴 카운트의 오탐을 막는다.
// 주석 제거가 특히 중요한 이유: 주석에 적힌 식별자가 "코드에 그 참조가 있다"로 오독되면
// `// TODO: LastOperationRatePerSecond` 한 줄로 어서션 판정이 뒤집힌다.
function stripLiterals(line) {
  return line
    .replace(/\\./g, '')
    .replace(/"[^"]*"/g, '""')
    .replace(/'[^']*'/g, "''")
    .replace(/\/\*.*?\*\//g, '') // 한 줄 안에서 닫히는 블록 주석
    .replace(/\/\/.*$/, '');
}

// 여러 줄에 걸친 블록 주석은 줄 단위 처리로 못 지운다 — 전문(text) 단위로 먼저 걷어낸다.
function stripBlockComments(text) {
  return text.replace(/\/\*[\s\S]*?\*\//g, '');
}

// 전문 정리: 블록 주석 → 줄 단위 리터럴/줄주석. 코드만 남은 텍스트를 돌려준다.
function stripCommentsAndLiterals(text) {
  return stripBlockComments(text).split('\n').map(stripLiterals).join('\n');
}

// from 위치 이후의 분기 본문만 잘라낸다 — `{...}` 블록이면 중괄호 균형으로, 아니면 단일 문장(`;` 까지).
// 분기 "안"을 판정 범위로 못박기 위한 것(함수 전문 substring 검사 금지).
function branchBodyAfter(text, from) {
  let i = from;
  while (i < text.length && /\s/.test(text[i])) i++;
  if (text[i] !== '{') {
    const end = text.indexOf(';', i);
    return end < 0 ? text.slice(i) : text.slice(i, end + 1);
  }
  let depth = 0;
  for (let j = i; j < text.length; j++) {
    if (text[j] === '{') depth++;
    else if (text[j] === '}' && --depth === 0) return text.slice(i + 1, j);
  }
  return text.slice(i);
}

// targetIdx 라인을 감싸는 블록 목록(바깥→안쪽). text = 그 블록을 여는 제어문/시그니처 원문.
// UE 코드 스타일(중괄호 단독 라인)을 전제로 한 근사 파서. 여는 줄을 못 찾으면 text='' 로 남겨
// "조건 불명"이 조용히 통과로 둔갑하지 않게 한다(호출자가 빈 text 를 검사).
function enclosingBlocks(lines, targetIdx) {
  const stack = [];
  for (let i = 0; i < targetIdx; i++) {
    const code = stripLiterals(lines[i]);
    for (const ch of code) {
      if (ch === '{') stack.push({ line: i + 1, text: blockOpenerText(lines, i) });
      else if (ch === '}') stack.pop();
    }
  }
  return stack;
}

// 중괄호가 열린 줄에서 제어문 원문을 복원 — `{` 단독 줄이면 위로 최대 4줄 거슬러 올라가며
// 비어있지 않은 코드 줄을 이어 붙인다(다중 라인 조건 대응).
function blockOpenerText(lines, idx) {
  const self = stripLiterals(lines[idx]).trim();
  if (self !== '{' && self !== '') return self;
  const parts = [];
  for (let i = idx - 1; i >= 0 && i >= idx - 4; i--) {
    const t = stripLiterals(lines[i]).trim();
    if (t === '' || t === '{') continue;
    parts.unshift(t);
    if (/^(if|else|while|for|switch|case|do)\b/.test(t) || /\)\s*$/.test(t) === false) break;
  }
  return parts.join(' ');
}

// 함수 본문 추출 — `<name>(` 가 나오는 정의부부터 중괄호 균형이 맞을 때까지.
function functionBody(lines, name) {
  const startIdx = lines.findIndex(l => l.includes(`::${name}(`) && !l.trim().startsWith('//'));
  if (startIdx < 0) return null;
  let depth = 0, started = false;
  const out = [];
  for (let i = startIdx; i < lines.length; i++) {
    const code = stripLiterals(lines[i]);
    out.push(lines[i]);
    for (const ch of code) {
      if (ch === '{') { depth++; started = true; }
      else if (ch === '}') depth--;
    }
    if (started && depth === 0) break;
  }
  return { startLine: startIdx + 1, text: out.join('\n') };
}

// repoRoot 아래 .cpp 전수 (커버리지 스캐너와 같은 범위).
function allCppFiles(repoRoot) {
  const root = path.join(repoRoot, SRC);
  const out = [];
  if (!fs.existsSync(root)) return out;
  (function walk(dir) {
    for (const f of fs.readdirSync(dir, { withFileTypes: true })) {
      const p = path.join(dir, f.name);
      if (f.isDirectory()) { walk(p); continue; }
      if (f.name.endsWith('.cpp')) out.push(p);
    }
  })(root);
  return out;
}

const fmt = n => (Number.isFinite(n) ? n.toLocaleString('en-US', { maximumFractionDigits: 4 }) : String(n));

// ── A-OFFSET ────────────────────────────────────────────────────────────────
function assertOffsetPair(values) {
  const unit = values.get('emp.outputUnit');
  const scale = values.get('emp.idleIncomeScale');
  const product = unit * scale;

  // 곱이 맞아도 한쪽이 방치수익 공식에서 빠져 있으면 상쇄쌍은 사문 — 실제 공식으로 되짚는다.
  // idlePerSecond(Lv1) = base × wander × max(outputUnit×1 × idleIncomeScale, 1)
  const base = values.get('idle.baseIncomePerSecond');
  const wander = values.get('emp.wanderIncomeMult');
  const idleFormulaProduct = idlePerSecond(values, { level: 1 }) / (base * wander);
  const reachesIdleFormula = Math.abs(idleFormulaProduct - product) <= IDLE_OFFSET_TOL;

  const pass = Math.abs(product - IDLE_OFFSET_PRODUCT) <= IDLE_OFFSET_TOL && reachesIdleFormula;
  return {
    pass,
    detail: `emp.outputUnit ${unit} × emp.idleIncomeScale ${scale} = ${fmt(product)} (기대 ${IDLE_OFFSET_PRODUCT})`
      + ` / 방치수익 공식 역산 Lv1 실효곱 ${fmt(idleFormulaProduct)}`
      + (reachesIdleFormula ? '' : ' ← 상쇄쌍 중 한쪽이 공식에 도달하지 않음'),
    data: { unit, scale, product, idleFormulaProduct, reachesIdleFormula },
  };
}

// ── A-DOUBLEPAY ─────────────────────────────────────────────────────────────
const STAGE_EQ = /CurrentBehaviorMode\s*==\s*EEmployeeBehaviorMode::Stage/;
const STAGE_NE = /CurrentBehaviorMode\s*!=\s*EEmployeeBehaviorMode::Stage/;

function assertNoStagePayout(repoRoot) {
  const lines = readLines(repoRoot, P.behavior);
  const sites = [];
  lines.forEach((l, i) => {
    const code = stripLiterals(l);
    if (code.includes('StoreResource(') && !code.includes('::StoreResource')) sites.push(i);
  });
  if (sites.length === 0) {
    return { pass: false, detail: `${P.behavior} 에서 StoreResource 호출을 찾지 못함 — 파서/경로 확인 필요(조용한 통과 차단)` };
  }

  const stagePayouts = [];
  const unknownScope = [];
  for (const i of sites) {
    const blocks = enclosingBlocks(lines, i);
    if (blocks.some(b => STAGE_EQ.test(b.text))) stagePayouts.push(i + 1);
    if (blocks.length === 0) unknownScope.push(i + 1);
  }

  // 적립(AccumulatedIncome +=) 쪽 Stage 제외 가드도 함께 확인 — 지급만 막고 적립을 남기면
  // 개발 중 쌓인 금액이 다음 Operation 첫 지급에 통째로 새어나간다(retune #11 의 실제 회귀 형태).
  const accrueIdx = lines.findIndex(l => /AccumulatedIncome\s*\+=/.test(stripLiterals(l)));
  const accrueGuarded = accrueIdx >= 0
    && enclosingBlocks(lines, accrueIdx).some(b => STAGE_NE.test(b.text));

  const pass = stagePayouts.length === 0 && unknownScope.length === 0 && accrueGuarded;
  const notes = [
    `StoreResource 호출 ${sites.length}건 — Stage 분기 내 지급 ${stagePayouts.length}건`,
    accrueIdx < 0 ? 'AccumulatedIncome += 를 찾지 못함' : `적립 Stage 제외 가드 ${accrueGuarded ? '있음' : '없음'}(:${accrueIdx + 1})`,
    unknownScope.length ? `스코프 불명 라인 ${unknownScope.join(',')}` : null,
    stagePayouts.length ? `위반 라인 ${stagePayouts.join(',')}` : null,
  ].filter(Boolean);
  return { pass, detail: notes.join(' / '), data: { sites: sites.map(i => i + 1), stagePayouts, accrueGuarded } };
}

// ── A-VAULTFALLBACK (알려진 결함) ────────────────────────────────────────────
function assertVaultFallback(repoRoot, values) {
  const lines = readLines(repoRoot, P.operation);
  const fn = functionBody(lines, 'CalculateWarehouseCapacity');
  if (!fn) {
    return { pass: false, detail: `${P.operation} 에서 CalculateWarehouseCapacity 정의를 찾지 못함 — 리네임/이동 여부 확인` };
  }
  const baseCap = values.get('vault.baseCapacity');
  // 주석/리터럴을 걷어낸 코드만 본다 — 주석 속 식별자로 판정이 뒤집히면 안 된다.
  const body = stripCommentsAndLiterals(fn.text);

  // 판정 범위 = "운영 없음(StableRate <= 0)" 분기의 **본문 안**. 함수 전문 substring 검사는
  // 다른 분기(빌딩 미발견 폴백 — 이건 retune #15 와 무관한 정상 가드)까지 삼켜 오판한다.
  const guard = /if\s*\(\s*StableRate\s*<=\s*0(?:\.\d*)?f?\s*\)/.exec(body);
  if (!guard) {
    return {
      pass: false,
      detail: `${P.operation}:${fn.startLine} CalculateWarehouseCapacity 에서 무운영 분기(StableRate <= 0)를 찾지 못함 — `
        + '함수 구조가 바뀌었다. 폴백 분기가 진짜 영속 레이트를 반환하는지 직접 확인한 뒤 어서션 기대치를 갱신할 것.',
      data: { baseCapacity: baseCap, guardFound: false },
    };
  }
  const branch = branchBodyAfter(body, guard.index + guard[0].length);
  // 핵심 어서트: 무운영 경로가 상수 하한(FloorCapacity/BaseVaultCapacity)을 그대로 돌려주면 결함 잔존.
  const returnsConstantFloor = /return\s+[^;]*\b(?:FloorCapacity|BaseVaultCapacity)\b[^;]*;/.test(branch);
  // 수정본이라면 세이브에 영속된 직전 운영 레이트를 되살려 쓴다(design §9 후보) — 분기 본문 안에서만 인정.
  const usesPersistedRate = /\b\w*Last\w*RatePerSecond\b|\bPersisted\w*Rate\w*\b/.test(branch);
  // 혼합 반환(`return FMath::Max(PersistedCap, FloorCapacity);`) = 영속 레이트를 쓰면서 상수를 하한으로만
  // 남긴 형태 → 정상 수정일 가능성이 높다. "결함 잔존"으로 단정하면 틀린 리포트가 나가므로 별도 상태로 뺀다.
  const mixedReturn = returnsConstantFloor && usesPersistedRate;
  const branchSnippet = branch.replace(/\s+/g, ' ').trim().slice(0, 160);

  const pass = !returnsConstantFloor;
  let detail;
  if (pass) {
    detail = `무운영 분기(StableRate <= 0) 본문이 상수 하한을 반환하지 않음 — 영속 레이트 참조 ${usesPersistedRate}, 본문 "${branchSnippet}"`;
  } else if (mixedReturn) {
    detail = `혼합 반환 — 수동 확인 필요. 무운영 분기가 영속 레이트를 참조하면서 상수 하한(vault.baseCapacity ${baseCap})도 함께 반환한다`
      + ` (${P.operation}:${fn.startLine} 부근, 분기 본문 "${branchSnippet}").`
      + ' 상수를 하한(Max)으로만 쓰는 정상 수정일 수 있고, 영속 레이트가 실효 없이 상수로 떨어지는 미완 수정일 수도 있다 —'
      + ' 자동 판정 불가이므로 사람이 분기 본문을 읽고 어서션 기대치를 갱신할 것.';
  } else {
    detail = `retune #15 잔존 — 무운영 빌딩 금고 용량이 vault.baseCapacity(${baseCap}) 상수 폴백 (${P.operation}:${fn.startLine} 부근, `
      + `분기 본문 "${branchSnippet}"). Task12 실측에서 cap=200.00 재현 확인. `
      + '분기가 영속 레이트 기반 용량을 반환하도록 고치면 이 어서션은 자동으로 pass 로 뒤집힌다(주석만 추가해서는 안 뒤집힌다).';
  }
  return {
    pass, detail,
    data: { baseCapacity: baseCap, guardFound: true, returnsConstantFloor, usesPersistedRate, mixedReturn, branchSnippet },
  };
}

// ── A-DFENUM ────────────────────────────────────────────────────────────────
// 헤더 인라인 함수의 본문만 중괄호 균형으로 잘라낸다 — 고정 길이 슬라이스를 쓰면 바로 아래
// 함수(GetQualityGradeBaseLifespanMinutes)의 case 값이 섞여 D/F 배율을 오독한다.
function inlineBody(text, anchor) {
  const start = text.indexOf(anchor);
  if (start < 0) return null;
  const open = text.indexOf('{', start);
  if (open < 0) return null;
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === '{') depth++;
    else if (text[i] === '}' && --depth === 0) return text.slice(open, i + 1);
  }
  return null;
}

function parseGradeSwitch(text, fnName) {
  const body = inlineBody(text, `${fnName}(EQualityGrade`);
  if (body === null) return null;
  const out = {};
  for (const m of body.matchAll(/case\s+EQualityGrade::(\w+)\s*:\s*return\s+([-\d.]+)f?\s*;/g)) {
    out[m[1]] = parseFloat(m[2]);
  }
  const def = /default\s*:\s*return\s+([-\d.]+)f?\s*;/.exec(body);
  return { cases: out, defaultValue: def ? parseFloat(def[1]) : null };
}

function assertDFEnum(repoRoot) {
  const qg = readSource(repoRoot, P.qualityGrade);
  const sw = parseGradeSwitch(qg, 'GetQualityGradeRevenueMultiplier');
  if (!sw) return { pass: false, detail: `${P.qualityGrade} 에서 GetQualityGradeRevenueMultiplier 를 찾지 못함` };

  const missing = ['D', 'F'].filter(g => !Number.isFinite(sw.cases[g]));
  const collapsed = ['D', 'F'].filter(g => sw.defaultValue !== null && sw.cases[g] === sw.defaultValue);

  // 호출부 2곳이 살아 있어야 D/F 배율이 실제 경제에 닿는다.
  const callers = {};
  let callerErr = null;
  try {
    for (const [k, rel] of [['TradePort', P.tradePort], ['WorldMapManager', P.worldMap]]) {
      callers[k] = readLines(repoRoot, rel)
        .filter(l => stripLiterals(l).includes('GetQualityGradeRevenueMultiplier(')).length;
    }
  } catch (e) { callerErr = e.message; }
  const deadCallers = Object.entries(callers).filter(([, n]) => n === 0).map(([k]) => k);

  const pass = !callerErr && missing.length === 0 && collapsed.length === 0 && deadCallers.length === 0;
  return {
    pass,
    detail: callerErr
      ? `호출부 확인 실패: ${callerErr}`
      : `D=${sw.cases.D} F=${sw.cases.F} (default ${sw.defaultValue}) / 호출부 TradePort ${callers.TradePort}건, WorldMapManager ${callers.WorldMapManager}건`
        + (missing.length ? ` ← case 누락 ${missing.join(',')}` : '')
        + (collapsed.length ? ` ← default(1.0) 로 붕괴 ${collapsed.join(',')}` : '')
        + (deadCallers.length ? ` ← 호출부 소실 ${deadCallers.join(',')}` : ''),
    data: { cases: sw.cases, defaultValue: sw.defaultValue, callers },
  };
}

// ── A-PRODGRADE ─────────────────────────────────────────────────────────────
function gradeReturns(text, fnName) {
  const body = inlineBody(text, `${fnName}(float`);
  if (body === null) return null;
  return [...new Set([...body.matchAll(/return\s+EQualityGrade::(\w+)\s*;/g)].map(m => m[1]))];
}

function assertProductionGrade(repoRoot) {
  const qg = readSource(repoRoot, P.qualityGrade);
  const prod = gradeReturns(qg, 'ProductionScoreToGrade');
  const proj = gradeReturns(qg, 'QualityScoreToGrade');
  if (!prod || !proj) {
    return { pass: false, detail: `${P.qualityGrade} 에서 ProductionScoreToGrade/QualityScoreToGrade 를 찾지 못함` };
  }
  const sixKept = ['S', 'A', 'B', 'C', 'D', 'F'].every(g => prod.includes(g));
  const projFour = proj.length === 4 && ['S', 'A', 'B', 'C'].every(g => proj.includes(g));

  // 제조 경로 파일이 프로젝트용 변환을 부르면 D=3/F=1 수량이 사문화된다(2026-07-31 회귀).
  const prodFiles = allCppFiles(repoRoot).filter(f => /Product(ion)?/i.test(path.basename(f)));
  const leaks = [];
  let usesProduction = 0;
  for (const f of prodFiles) {
    const src = fs.readFileSync(f, 'utf8');
    const rel = path.relative(repoRoot, f).split(path.sep).join('/');
    if (src.includes('QualityScoreToGrade(')) leaks.push(rel);
    if (src.includes('ProductionScoreToGrade(')) usesProduction++;
  }
  const orderMgrOk = (() => {
    try { return readSource(repoRoot, P.productionOrder).includes('ProductionScoreToGrade('); } catch { return false; }
  })();

  const pass = sixKept && projFour && leaks.length === 0 && orderMgrOk && prodFiles.length > 0;
  return {
    pass,
    detail: `ProductionScoreToGrade 반환 ${prod.join('/')} (6등급 ${sixKept}) / QualityScoreToGrade 반환 ${proj.join('/')} (4등급 ${projFour})`
      + ` / 제조계열 파일 ${prodFiles.length}개 중 ProductionScoreToGrade 사용 ${usesProduction}개, ProductionOrderManager 배선 ${orderMgrOk}`
      + (leaks.length ? ` ← 제조 경로가 QualityScoreToGrade 호출: ${leaks.join(',')}` : '')
      + (prodFiles.length === 0 ? ' ← 제조 경로 파일을 하나도 찾지 못함(스캔 범위 확인)' : ''),
    data: { production: prod, project: proj, leaks, prodFiles: prodFiles.length },
  };
}

// ── A-DIAMOND ───────────────────────────────────────────────────────────────
// simInflow = {total, bySrc, runs} — 시뮬 실측 Diamond 유입(없으면 null).
function assertDiamondBudget(values, simInflow) {
  const b = diamondBudget(values);
  const missionTotal = b.missionTotal;
  const fixedInflow = b.codex + missionTotal;          // DS1+DS2(도감) + DS4(미션) — 1회성 확정
  const slotCosts = values.get('diamond.slotCosts');
  const slotTotal = slotCosts.reduce((s, v) => s + v, 0); // DK3 특성 슬롯 3칸 = 유일한 대형 1회성 필수 유출 (0번은 항상 무료라 실지출은 -30)
  const retry = values.get('launch.retryDiamondCost');    // DK2 컨티뉴 / 건
  const reset = values.get('emp.disciplineResetDiamond'); // DK4 직능 리셋 / 건
  const gachaAdv = values.get('gacha.advancedDiamondCost');
  const gachaPrem = values.get('gacha.premiumDiamondCost');
  const fixedNet = fixedInflow - slotTotal;
  const vip = b.vipPerOrderEV;
  const vipOrdersToCoverDeficit = fixedNet >= 0 ? 0 : (vip > 0 ? Math.ceil(-fixedNet / vip) : Infinity);

  // 확정 수지가 적자여도 반복 유입(VIP 무역 DS3)이 살아 있으면 닫힌 경제가 막히지 않는다.
  // 적자 + 반복 유입 0 = 진짜 소프트락.
  const pass = fixedNet >= 0 || vip > 0;

  const simNote = simInflow
    ? ` / 시뮬 실측 유입 ${fmt(simInflow.total)} (${Object.entries(simInflow.bySrc).map(([k, v]) => `${k} ${fmt(v)}`).join(', ')}`
      + `, ${simInflow.runs}런 평균) — 이론과 차이나는 이유: 정책이 티어당 7첫클리어에서 다음 티어로 넘어가 도감 10/10 을`
      + ` 채우지 않으므로 DS1/DS2(코덱스 ${fmt(b.codex)})가 실런에서 미도달, 실유입은 DS4(미션)뿐이다`
    : '';

  return {
    pass,
    detail: `확정유입 ${fmt(fixedInflow)} (코덱스 ${fmt(b.codex)} + 미션 ${fmt(missionTotal)})`
      + ` vs 1회성 필수유출 슬롯 ${fmt(slotTotal)} → 순 ${fmt(fixedNet)}`
      + `; 건당 유출 컨티뉴 ${fmt(retry)} / 리셋 ${fmt(reset)} / 가챠 ${fmt(gachaAdv)}·${fmt(gachaPrem)}`
      + `; 반복유입 VIP EV ${fmt(vip)}/건 → 적자 보전에 ${vipOrdersToCoverDeficit === Infinity ? '보전 불가(소프트락)' : `${vipOrdersToCoverDeficit}건`}`
      + (pass ? '' : ' ← 확정 적자 + 반복유입 0 = 소프트락')
      + simNote,
    data: {
      codex: b.codex, missionTotal, fixedInflow, slotTotal, fixedNet,
      retry, reset, gachaAdv, gachaPrem, vipPerOrderEV: vip, vipOrdersToCoverDeficit,
      simInflowTotal: simInflow ? simInflow.total : null,
    },
  };
}

// out/runs/<profile>-<seed>.json 의 ledger.bySrc 에서 Diamond 유입만 평균낸다.
function readSimDiamondInflow(repoRoot) {
  const dir = path.join(repoRoot, 'Tools', 'Balance', 'out', 'runs');
  if (!fs.existsSync(dir)) return null;
  const files = fs.readdirSync(dir).filter(f => /^mid-\d+\.json$/.test(f));
  if (files.length === 0) return null;
  const bySrc = {};
  let total = 0;
  for (const f of files) {
    const r = JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8'));
    for (const [k, v] of Object.entries(r.ledger.bySrc || {})) {
      if (!k.startsWith('Diamond|') || v <= 0) continue;
      const src = k.split('|')[1];
      bySrc[src] = (bySrc[src] || 0) + v / files.length;
      total += v / files.length;
    }
  }
  return { total, bySrc, runs: files.length };
}

// ── 시뮬 런 (A-EXPGATE / A-MCSAT 공용) ──────────────────────────────────────
function simRuns(valuesPath) {
  const out = [];
  for (const profile of RUN_PROFILES) {
    for (let seed = 1; seed <= RUN_SEEDS; seed++) {
      out.push(runWorld({ seed, profile, valuesPath }));
    }
  }
  return out;
}

function assertExpGate(runs, values) {
  const invalid = runs.filter(r => r.invalid);
  if (runs.length === 0 || invalid.length) {
    return { pass: false, detail: `유효한 시뮬 런 없음(invalid ${invalid.length}/${runs.length}: ${invalid.map(r => r.invalid)[0] || '-'})` };
  }
  if (runs.some(r => r.tiers.some(t => !Number.isFinite(t.expGateBlocks)))) {
    return { pass: false, detail: 'RunResult.tiers[].expGateBlocks 관측 필드 부재 — sim/world.js 계측 확인' };
  }
  const reqPerTier = values.get('gate.reqBuildingLvPerTier');
  const clearToUnlock = values.get('gate.clearToUnlock');
  const perTier = {};
  for (const r of runs) for (const t of r.tiers) perTier[t.tier] = (perTier[t.tier] || 0) + t.expGateBlocks;
  const blocked = Object.entries(perTier).filter(([, n]) => n > 0).map(([t, n]) => `T${t}:${n}`);

  // 레벨 여유 = 마지막으로 넘은 게이트 요구치 대비 최종 빌딩 레벨.
  const margins = runs.map(r => {
    const reached = r.endState.reachedTier;
    return r.endState.primaryBuildingLevel - Math.max(0, reached - 1) * reqPerTier;
  });
  const minMargin = Math.min(...margins);

  const pass = blocked.length === 0;
  return {
    pass,
    detail: `${runs.length}런(${RUN_PROFILES.join('/')} × ${RUN_SEEDS}시드) × 티어 진행에서 `
      + `"${clearToUnlock}첫클리어 충족 + 빌딩Lv<티어×${reqPerTier}" 차단 ${blocked.length ? blocked.join(', ') : '0건'}`
      + ` / 최종 레벨 여유 최소 +${minMargin}`,
    data: { perTier, minMargin, runs: runs.length },
  };
}

function assertMarketCapSaturation(runs, values, repoRoot) {
  const B = values.dt('DT_MarketBalance').find(r => r.Name === 'Default');
  if (!B) return { pass: false, detail: 'DT_MarketBalance Default 행 없음' };
  // clamp(BaseMul + Coef×log10(1+MC/Pivot), BaseMul, MaxMul) 을 MC 에 대해 역산.
  const satMC = Math.max(1, B.PivotMC) * (Math.pow(10, (B.MaxMul - B.BaseMul) / B.Coef) - 1);

  // 프로젝트 결산 MarketCap = BaseMarketCapReward × QualityScore × MarketCapMultiplier배율 × 특성계수.
  // 상수는 소스에서 직접 읽는다(하드코딩 금지 — 값이 바뀌면 여기서 loud fail).
  let baseReward, qMax;
  try {
    const opSrc = readSource(repoRoot, P.operation);
    const m = /constexpr\s+int64\s+BaseMarketCapReward\s*=\s*(\d+)/.exec(opSrc);
    if (!m) return { pass: false, detail: `${P.operation} 에서 BaseMarketCapReward 상수를 찾지 못함` };
    baseReward = parseInt(m[1], 10);
    // QualityScore 클램프 상한(= MarketCap 계수의 상한). 앵커를 NewOperation.QualityScore 로 좁혀
    // 같은 파일의 다른 QualityScore 클램프(재계산 경로 등)를 집지 않게 한다. 못 찾으면 loud fail.
    const c = /FMath::Clamp\(\s*NewOperation\.QualityScore\b[^,]*,\s*([\d.]+)f?\s*,\s*([\d.]+)f?\s*\)/.exec(opSrc);
    if (!c) return { pass: false, detail: `${P.operation} 에서 NewOperation.QualityScore 클램프 상한을 찾지 못함 — 상한 가정 불가` };
    qMax = parseFloat(c[2]);
  } catch (e) { return { pass: false, detail: e.message }; }

  // 행이 없으면 배율 1 로 조용히 폴백하면 안 된다 — 상한 어서션이 배율을 무시한 채 PASS 를 찍는다.
  const mcRow = values.dt('DT_BuildingEnhancementDefinition').find(r => (r.EnhancementType ?? r.Name) === 'MarketCapMultiplier');
  if (!mcRow) return { pass: false, detail: 'DT_BuildingEnhancementDefinition 에 MarketCapMultiplier 행 없음 — 시총 배율 상한을 가정할 수 없다(values.json 재추출 필요 의심)' };
  const mcMax = buildingEffect(values, 'MarketCapMultiplier', mcRow.MaxLevel);

  // 티어별 누적 상한 = (그 티어까지의 등급 부여 착수 수) × baseReward × qMax × mcMax.
  const perTierLaunches = {};
  for (const r of runs) {
    for (const t of r.tiers) {
      const graded = t.grades.S + t.grades.A + t.grades.B + t.grades.C;
      perTierLaunches[t.tier] = Math.max(perTierLaunches[t.tier] || 0, graded);
    }
  }
  let cum = 0, satTier = null;
  const curve = [];
  for (let tier = 1; tier <= 10; tier++) {
    cum += (perTierLaunches[tier] || 0) * baseReward * qMax * mcMax;
    curve.push({ tier, mcUpper: cum, mul: marketCapMultiplier(values, cum) });
    if (satTier === null && marketCapMultiplier(values, cum) >= B.MaxMul - 1e-9) satTier = tier;
  }
  const end = curve[curve.length - 1];
  const moves = end.mul > B.BaseMul + 1e-9;

  const pass = satTier === null && moves;
  return {
    pass,
    detail: `MaxMul ${B.MaxMul} 포화 필요 MC = ${fmt(Math.round(satMC))} (BaseMul ${B.BaseMul}, Coef ${B.Coef}, Pivot ${fmt(B.PivotMC)})`
      + ` / 상한 가정(등급착수 × ${fmt(baseReward)} × q${qMax} × 시총배율상한 ${fmt(mcMax)}) T10 누적 ${fmt(Math.round(end.mcUpper))} → 배율 ${fmt(end.mul)}`
      + ` / 포화 도달 티어 ${satTier === null ? '없음' : `T${satTier}`}`
      + (satTier === null ? ` ← MaxMul ${B.MaxMul} 은 도달 필요 MC 가 상한의 ${fmt(Math.round(satMC / Math.max(1, end.mcUpper)))}배라 사실상 죽은 상한(노브 권고 대상)` : '')
      + (moves ? '' : ' ← 배율이 BaseMul 에서 움직이지 않음(죽은 노브)'),
    data: { satMC, satTier, curve, baseReward, qMax, mcMax },
  };
}

// ── A-UNWIRED ───────────────────────────────────────────────────────────────
function itemEnumNames(repoRoot) {
  const src = readSource(repoRoot, P.itemType);
  const body = /enum\s+class\s+EItemType\s*:\s*uint8\s*\{([\s\S]*?)\};/.exec(src);
  if (!body) return null;
  const names = [];
  for (const line of body[1].split('\n')) {
    const t = stripLiterals(line).trim();
    const m = /^(\w+)\s*(=\s*\d+\s*)?(UMETA|,|$)/.exec(t);
    if (m && !['None', 'Count'].includes(m[1])) names.push(m[1]);
  }
  return names;
}

// SpendItem 인자가 리터럴이 아니면(TicketType/CubeType 등) 같은 파일에서 그 식별자와 함께 등장하는
// EItemType:: 값을 전부 후보로 본다 — 과다 수집 쪽으로 치우치므로 "미배선" 판정이 느슨해지지 않는다.
function resolveConsumedItems(repoRoot) {
  const consumed = new Set();
  const unresolved = [];
  for (const f of allCppFiles(repoRoot)) {
    const lines = fs.readFileSync(f, 'utf8').split(/\r?\n/);
    lines.forEach((raw, i) => {
      const code = stripLiterals(raw);
      if (!code.includes('SpendItem(') || code.includes('::SpendItem') || code.includes('ListView->SpendItem(')) return;
      const arg = /SpendItem\(\s*([^,]+?)\s*,/.exec(code);
      if (!arg) return;
      const lit = /EItemType::(\w+)/.exec(arg[1]);
      if (lit) { consumed.add(lit[1]); return; }
      const ident = /^[A-Za-z_]\w*$/.exec(arg[1].trim());
      if (!ident) { unresolved.push(`${path.basename(f)}:${i + 1}`); return; }
      const re = new RegExp(`\\b${ident[0]}\\b`);
      let hit = 0;
      for (let j = 0; j < lines.length; j++) {
        if (!re.test(lines[j])) continue;
        // 다중 라인 대입/삼항(`= bAdvanced` ⏎ `? EItemType::X` ⏎ `: EItemType::Y;`)까지 한 문장으로 본다.
        // 문장 끝(`;`) 또는 블록 경계(`{`/`}`)에서 끊어 무관한 다음 문장을 삼키지 않는다.
        let win = '';
        for (let k = j; k < lines.length && k <= j + 3; k++) {
          const t = stripLiterals(lines[k]);
          if (k > j && ['{', '}'].includes(t.trim())) break;
          win += `${t}\n`;
          if (t.includes(';')) break;
        }
        for (const m of win.matchAll(/EItemType::(\w+)/g)) {
          if (m[1] !== 'None') { consumed.add(m[1]); hit++; }
        }
      }
      if (hit === 0) unresolved.push(`${path.basename(f)}:${i + 1}(${ident[0]})`);
    });
  }
  return { consumed, unresolved };
}

function assertUnwiredItems(repoRoot, values) {
  const names = itemEnumNames(repoRoot);
  if (!names || names.length === 0) {
    return { pass: false, detail: `${P.itemType} 에서 EItemType 목록을 파싱하지 못함` };
  }
  const { consumed, unresolved } = resolveConsumedItems(repoRoot);
  if (consumed.size === 0) {
    return { pass: false, detail: 'SpendItem 소비 경로를 하나도 해석하지 못함 — 파서/경로 확인 필요(조용한 통과 차단)' };
  }
  const unwired = names.filter(n => !consumed.has(n)).sort();
  const baseline = [...UNWIRED_ITEM_BASELINE].sort();
  const newlyWired = baseline.filter(n => !unwired.includes(n));
  const newlyDead = unwired.filter(n => !baseline.includes(n));

  // 죽은 싱크 경고 — 소비 경로가 없는데 상점에서 팔리면 플레이어가 재화를 태우고 효과가 0이다.
  let shopErr = null;
  const shopItems = new Set((() => {
    try { return values.dt('DT_ShopItem').map(r => r.Item); }
    catch (e) { shopErr = e.message; return []; } // 조용히 빈 집합으로 넘기지 않고 detail 에 사유를 남긴다
  })());
  const soldButDead = unwired.filter(n => shopItems.has(n));

  const pass = newlyWired.length === 0 && newlyDead.length === 0 && unresolved.length === 0;
  return {
    pass,
    detail: `EItemType ${names.length}종 중 소비 경로 0건 ${unwired.length}종: ${unwired.join(', ')}`
      + (soldButDead.length ? ` / 그중 상점 판매 중(효과 없는 죽은 싱크) ${soldButDead.length}종: ${soldButDead.join(', ')}` : '')
      + (newlyWired.length ? ` ← 베이스라인 대비 신규 배선 ${newlyWired.join(',')} (시뮬 모델 갱신 필요)` : '')
      + (newlyDead.length ? ` ← 신규 미배선 아이템 ${newlyDead.join(',')}` : '')
      + (unresolved.length ? ` ← 인자 해석 실패 ${unresolved.join(',')}` : '')
      + (shopErr ? ` / ⚠ 상점 판매 대조 생략(DT_ShopItem 조회 실패: ${shopErr}) — 죽은 싱크 목록은 미검증` : ''),
    data: { unwired, soldButDead, newlyWired, newlyDead, unresolved, shopErr },
  };
}

// ── A-COVERAGE / A-MILEAGE-SHOP ─────────────────────────────────────────────
const COVERAGE_PATH = 'Tools/Balance/verify/coverage.json';
const MILEAGE_IDS = ['SHOP-MILEAGE-K1', 'SKIN-MILEAGE-S1', 'SKIN-MILEAGE-K1', 'TRAIT-MILEAGE-S1', 'TRAIT-MILEAGE-K1'];

function loadCoverage(repoRoot) {
  const p = path.join(repoRoot, COVERAGE_PATH);
  if (!fs.existsSync(p)) throw new SourceMissing(`커버리지 매트릭스를 찾을 수 없음: ${COVERAGE_PATH}`);
  return { path: p, json: JSON.parse(fs.readFileSync(p, 'utf8')) };
}

function assertCoverage(repoRoot) {
  const { path: p, json } = loadCoverage(repoRoot);
  const r = checkCoverage(repoRoot, p);
  const k5 = json.entries.find(e => e.id === 'K5');
  // K5 = 직원 고용 랜덤박스(Money 결제) — Task10 리뷰에서 시뮬 미모델로 판명. modeled 로 남으면 과대표기.
  const k5ok = !!k5 && k5.status !== 'modeled' && typeof k5.reason === 'string' && k5.reason.length > 0;
  const pass = r.ok && k5ok;
  return {
    pass,
    detail: `checkCoverage ok=${r.ok} (엔트리 ${json.entries.length}, 미매칭 스캔지점 ${r.unmatched.length}`
      + `${r.unmatched.length ? `: ${r.unmatched.map(u => `${u.file}:${u.line}`).join(', ')}` : ''}`
      + `, reason 누락 ${r.missingReason.length}, scanExempt ${r.exempt.length})`
      + ` / K5 status=${k5 ? k5.status : '엔트리 없음'}${k5ok ? '' : ' ← Money 랜덤박스 미모델인데 modeled 표기(또는 사유 없음)'}`,
    data: { ok: r.ok, unmatched: r.unmatched, exempt: r.exempt.length, k5Status: k5 ? k5.status : null },
  };
}

function assertMileageCoverage(repoRoot) {
  const { json } = loadCoverage(repoRoot);
  const byId = new Map(json.entries.map(e => [e.id, e]));
  const missing = MILEAGE_IDS.filter(id => !byId.has(id));
  const badReason = MILEAGE_IDS.filter(id => byId.has(id)
    && !(typeof byId.get(id).reason === 'string' && byId.get(id).reason.length > 0));
  const badExempt = MILEAGE_IDS.filter(id => byId.has(id)
    && !(byId.get(id).scanExempt === true && typeof byId.get(id).scanExemptReason === 'string'));
  // 상점 Mileage 결제가 실제로 존재하는지도 확인 — 코드가 사라졌는데 엔트리만 남는 반대 방향 오차 차단.
  let shopSpend = 0;
  try {
    shopSpend = readLines(repoRoot, `${SRC}/Private/Manager/ShopManagerSubsystem.cpp`)
      .filter(l => stripLiterals(l).includes('SpendMileage(')).length;
  } catch (e) { return { pass: false, detail: e.message }; }

  const pass = missing.length === 0 && badReason.length === 0 && badExempt.length === 0 && shopSpend > 0;
  return {
    pass,
    detail: `마일리지 풀 엔트리 ${MILEAGE_IDS.length - missing.length}/${MILEAGE_IDS.length} 등재`
      + ` / ShopManagerSubsystem SpendMileage 호출 ${shopSpend}건`
      + (missing.length ? ` ← 미등재 ${missing.join(',')}` : '')
      + (badReason.length ? ` ← 사유 없음 ${badReason.join(',')}` : '')
      + (badExempt.length ? ` ← scanExempt/사유 누락 ${badExempt.join(',')}` : '')
      + (shopSpend === 0 ? ' ← 상점 Mileage 결제 경로 소실' : ''),
    data: { missing, badReason, badExempt, shopSpend },
  };
}

// ── 스위트 ──────────────────────────────────────────────────────────────────
const ASSERTIONS = [
  {
    id: 'A-OFFSET', desc: 'OutputUnit × IdleIncomeScale 상쇄쌍 보존',
    run: ctx => assertOffsetPair(ctx.values),
  },
  {
    id: 'A-DOUBLEPAY', desc: 'Stage 모드 방치 지급 삭제(개발 중 이중 계상 0)',
    run: ctx => assertNoStagePayout(ctx.repoRoot),
  },
  {
    id: 'A-VAULTFALLBACK', desc: '금고 폴백이 상수 200 이 아닌 영속 레이트 사용', knownDefect: true,
    run: ctx => assertVaultFallback(ctx.repoRoot, ctx.values),
    // knownDefect 를 정의 레벨에서만 붙이면 "판정 불가"까지 KNOWN 으로 흡수돼 조용해진다 —
    // 실제 판정 신호(data)를 보고 상태를 가른다.
    classify: out => {
      const d = out.data || {};
      // 소스 부재/함수 소실/가드 소실 = 결함 잔존이 아니라 **판정 불가** → 차단(exit 1)해서 사람을 부른다.
      if (d.guardFound !== true) return 'FAIL';
      // 혼합 반환 = 정상 수정일 수 있어 "결함 잔존(KNOWN)"으로 기록하면 틀린 리포트가 된다.
      if (d.mixedReturn === true) return 'REVIEW';
      return 'KNOWN';
    },
  },
  {
    id: 'A-DFENUM', desc: 'GetQualityGradeRevenueMultiplier D/F 배율 잔존 + 호출부 생존',
    run: ctx => assertDFEnum(ctx.repoRoot),
  },
  {
    id: 'A-PRODGRADE', desc: 'ProductionScoreToGrade 6등급 분리 유지',
    run: ctx => assertProductionGrade(ctx.repoRoot),
  },
  {
    id: 'A-DIAMOND', desc: 'Diamond 총유입 vs 필수 유출 수지',
    run: ctx => assertDiamondBudget(ctx.values, ctx.simDiamond),
  },
  {
    id: 'A-EXPGATE', desc: '티어별 7클리어 vs 빌딩Lv 게이트 — EXP 병목 티어',
    run: ctx => assertExpGate(ctx.runs, ctx.values),
  },
  {
    id: 'A-MCSAT', desc: 'MarketCap 성장배율 MaxMul 포화 도달 티어',
    run: ctx => assertMarketCapSaturation(ctx.runs, ctx.values, ctx.repoRoot),
  },
  {
    id: 'A-UNWIRED', desc: '미배선 아이템(LuckyEnhanceTicket 등) 소비 경로 0건 유지',
    run: ctx => assertUnwiredItems(ctx.repoRoot, ctx.values),
  },
  {
    id: 'A-COVERAGE', desc: '재화 커버리지 매트릭스 정합(K5 재표기 포함)',
    run: ctx => assertCoverage(ctx.repoRoot),
  },
  {
    id: 'A-MILEAGE-SHOP', desc: '상점 Mileage 결제 + 스킨/특성 마일리지 풀 커버리지 등재',
    run: ctx => assertMileageCoverage(ctx.repoRoot),
  },
];

const ASSERTION_IDS = ASSERTIONS.map(a => a.id);

function runAssertions(valuesPath, repoRoot) {
  const values = loadValues(valuesPath);
  const ctx = { valuesPath, repoRoot, values, runs: null, simDiamond: null };
  // 런 기반 어서션이 하나라도 있으면 시뮬을 한 번만 돌려 공유한다.
  try { ctx.runs = simRuns(valuesPath); } catch (e) { ctx.runs = []; ctx.runError = e.message; }
  try { ctx.simDiamond = readSimDiamondInflow(repoRoot); } catch { ctx.simDiamond = null; }

  const results = ASSERTIONS.map(def => {
    let out;
    try {
      out = def.run(ctx);
    } catch (e) {
      out = { pass: false, detail: `어서션 실행 실패(${e.constructor.name}): ${e.message}` };
    }
    const pass = out.pass === true;
    // 상태 4종: PASS / KNOWN(알려진 결함, 비차단) / REVIEW(자동 판정 불가, 비차단·수동 확인) / FAIL(차단).
    // knownDefect 정의만으로 KNOWN 을 붙이지 않는다 — classify 가 어서션이 실제로 관측한 data 를 보고 가른다.
    let state = 'PASS';
    if (!pass) {
      state = def.classify ? def.classify(out) : (def.knownDefect === true ? 'KNOWN' : 'FAIL');
      if (!['KNOWN', 'REVIEW', 'FAIL'].includes(state)) state = 'FAIL';
    }
    return {
      id: def.id, desc: def.desc, pass, state,
      known: state === 'KNOWN',
      detail: out.detail || '(detail 없음 — 어서션 구현 확인 필요)',
      data: out.data,
    };
  });
  return { results, valuesHash: values.hash, generatedAt: new Date().toISOString() };
}

// 차단 = 실패인데 KNOWN(알려진 결함)도 REVIEW(수동 확인 대상)도 아닌 것.
// state 없는 결과(구 호출부/테스트용 리터럴)는 known 플래그로 폴백.
const NON_BLOCKING_STATES = ['KNOWN', 'REVIEW'];
function hasBlockingFailure(results) {
  return results.some(r => !r.pass
    && !NON_BLOCKING_STATES.includes(r.state)
    && r.known !== true);
}

// ── CLI ─────────────────────────────────────────────────────────────────────
function parseArgs(argv) {
  const a = {
    values: path.resolve(__dirname, '..', 'out', 'values.json'),
    repo: path.resolve(__dirname, '..', '..', '..'),
    json: false,
  };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--values') { a.values = argv[++i]; continue; }
    if (argv[i] === '--repo') { a.repo = argv[++i]; continue; }
    if (argv[i] === '--json') { a.json = true; }
  }
  return a;
}

function main(argv) {
  const a = parseArgs(argv);
  const r = runAssertions(a.values, a.repo);
  if (a.json) {
    console.log(JSON.stringify(r, null, 1));
  } else {
    const w = Math.max(...r.results.map(x => x.id.length));
    console.log(`정합성 어서션 ${r.results.length}종 — values ${r.valuesHash}`);
    for (const x of r.results) {
      console.log(`[${x.state.padEnd(6)}] ${x.id.padEnd(w)}  ${x.desc}`);
      console.log(`           ${x.detail}`);
    }
    const count = s => r.results.filter(x => x.state === s).length;
    console.log(`\nPASS ${count('PASS')} / FAIL ${count('FAIL')} / KNOWN ${count('KNOWN')} / REVIEW ${count('REVIEW')}`);
  }
  return hasBlockingFailure(r.results) ? 1 : 0;
}

if (require.main === module) process.exitCode = main(process.argv.slice(2));

module.exports = {
  runAssertions, hasBlockingFailure, ASSERTION_IDS, ASSERTIONS, main,
  assertOffsetPair, assertDiamondBudget, assertNoStagePayout, assertVaultFallback,
  assertDFEnum, assertProductionGrade, assertExpGate, assertMarketCapSaturation,
  assertUnwiredItems, assertCoverage, assertMileageCoverage,
  enclosingBlocks, functionBody, stripLiterals, stripCommentsAndLiterals, branchBodyAfter,
  itemEnumNames, resolveConsumedItems,
};
