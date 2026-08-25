#!/usr/bin/env node
'use strict';
// 파탄 스캐너 — 노브 감도 스캔 + 소프트락/인플레/죽은 노브/절벽 탐지.
//   node Tools/Balance/verify/scan.js --seeds 50
//   → out/scan.json + 콘솔 표 (수 분 소요, 진행 로그 출력)
//
// 원칙:
//  - 게임 수치를 이 파일에 심지 않는다. 노브는 values.json 키/DT 컬럼을 **배율로** 흔들 뿐이고,
//    여기 상수는 판정 기준(임계값·비대칭비)과 스캔 파라미터(시드/버킷)뿐이다.
//  - values.json 은 읽기 전용 계약 — 오버라이드는 사본을 out/tmp/ 에 만들어 주입하고 즉시 지운다.
//  - 노브 키 오타는 조용한 "죽은 노브" 오판이 되므로, 없는 키/DT/컬럼은 즉시 throw.
//  - 결정성: 시드는 고정 시작점(seedStart+i). 같은 인자 = 같은 rows.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { runWorld, PROJECTS_PER_TIER, INDUSTRY } = require('../sim/world.js');
const { quantiles } = require('../sim/run.js');
const { loadValues } = require('../sim/values.js');

const DEFAULT_VALUES = path.resolve(__dirname, '..', 'out', 'values.json');
const DEFAULT_TMP = path.resolve(__dirname, '..', 'out', 'tmp');
const DEFAULT_OUT = path.resolve(__dirname, '..', 'out', 'scan.json');

// 판정 기준 — 게임 밸런스 수치가 아니라 "얼마나 움직여야 살아있다고 볼 것인가"의 분석 파라미터.
const DEAD_THRESHOLD = 0.01;   // ±20% 를 흔들어도 최대 효과 1% 미만 = 죽은 노브
const CLIFF_RATIO = 3.0;       // +20% 효과가 −20% 효과의 3배 이상 = 절벽(비선형 단애)
const SCAN_SEED_START = 1000;  // 재실행 동일 결과를 위한 고정 시작 시드

// DT_Project_Game 요구점수 컬럼 — reqCurve 노브는 이 4개를 한 배율로 함께 민다(티어비 대용).
const REQ_SCORE_COLUMNS = ['RequiredScore_Step1', 'RequiredScore_Step2', 'RequiredScore_Step3', 'RequiredScore_Step4'];

// 스캔 노브 = (delta) → overrideValues 패치맵. 숫자 = 배율(벡터는 원소별).
const KNOBS = {
  'emp.enhanceCostBase': d => ({ 'emp.enhanceCostBase': 1 + d }),
  'trend.peakMult': d => ({ 'trend.peakMult': 1 + d }),
  'score.multSlope': d => ({ 'score.multSlope': 1 + d }),
  'quality.gradeLifespanMin': d => ({ 'quality.gradeLifespanMin': 1 + d }),
  'offline.progressRate': d => ({ 'offline.progressRate': 1 + d }),
  'emp.idleIncomeScale': d => ({ 'emp.idleIncomeScale': 1 + d }),
  // DT 파생 노브 — values 키가 아니라 DT 컬럼을 직접 스케일한다.
  devCostCoef: d => ({ 'dt:DT_Project_Game.DevelopmentCost': 1 + d }),
  reqCurve: d => Object.fromEntries(REQ_SCORE_COLUMNS.map(c => [`dt:DT_Project_Game.${c}`, 1 + d])),
};

function knobPatches(name, delta) {
  const f = KNOBS[name];
  if (!f) throw new Error(`알 수 없는 스캔 노브: ${name} (가능: ${Object.keys(KNOBS).join('|')})`);
  return f(delta);
}

// ── values 오버라이드 ───────────────────────────────────────────────────────
function applyPatch(v, spec, label) {
  const s = typeof spec === 'number' ? { mult: spec } : spec;
  if (!s || typeof s !== 'object') throw new Error(`패치 형식 오류(${label}): ${JSON.stringify(spec)}`);
  if ('set' in s) return s.set;
  if (typeof s.mult !== 'number' || !Number.isFinite(s.mult)) {
    throw new Error(`패치 형식 오류(${label}): mult 또는 set 필요 — ${JSON.stringify(spec)}`);
  }
  if (Array.isArray(v)) {
    return v.map((x, i) => {
      if (typeof x !== 'number') throw new Error(`비수치 원소 패치 불가(${label}[${i}]): ${x}`);
      return x * s.mult;
    });
  }
  if (typeof v !== 'number') throw new Error(`비수치 값 패치 불가(${label}): ${v}`);
  return v * s.mult;
}

// patches = { '<values 키>': 배율|{mult}|{set}, 'dt:<Table>.<Column>': 동일 }
// 원본 파일은 절대 수정하지 않는다 — 복제·패치본 경로를 돌려준다.
function overrideValues(srcPath, patches, outPath = null) {
  const raw = JSON.parse(fs.readFileSync(srcPath, 'utf8'));
  const applied = {};
  for (const [key, spec] of Object.entries(patches)) {
    if (key.startsWith('dt:')) {
      const m = /^dt:([^.]+)\.(.+)$/.exec(key);
      if (!m) throw new Error(`DT 패치 키 형식 오류: ${key} (dt:<Table>.<Column> 이어야 함)`);
      const [, table, col] = m;
      const rows = raw.dt ? raw.dt[table] : undefined;
      if (!Array.isArray(rows)) throw new Error(`values.json 누락 DT: ${table} (패치 키 ${key})`);
      let n = 0;
      for (const r of rows) {
        if (typeof r[col] !== 'number') continue;
        r[col] = applyPatch(r[col], spec, key);
        n++;
      }
      if (n === 0) throw new Error(`DT 컬럼에 수치 행이 없음: ${table}.${col} (패치 키 ${key}) — 컬럼명 오타?`);
      applied[key] = { rows: n, spec };
    } else {
      const e = raw.values ? raw.values[key] : undefined;
      if (e === undefined) throw new Error(`values.json 누락 키: ${key}`);
      e.v = applyPatch(e.v, spec, key);
      applied[key] = { spec };
    }
  }
  // 패치 내역을 meta 에 남긴다(hash 는 srcHash/editorHash 기반이라 불변 — 원본 추적 유지).
  raw.meta = Object.assign({}, raw.meta, { scanPatch: applied });
  const dest = outPath || path.join(DEFAULT_TMP, `values-${hashOf(patches)}.json`);
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  fs.writeFileSync(dest, JSON.stringify(raw));
  return dest;
}

function hashOf(obj) {
  return crypto.createHash('sha1').update(JSON.stringify(obj)).digest('hex').slice(0, 10);
}

// 티어 밴드 끝 행(= 그 티어에서 가장 비싼 프로젝트)의 개발비.
// world.js 의 nextDevCost() 가 reserve 기준으로 쓰는 바로 그 행이라, "이 티어를 굴리려면 얼마가 필요한가"의
// 시뮬 내부 기준과 일치한다. 밴드 행이 없으면(DT 결손) null → 호출자가 폴백을 쓴다.
function tierBandDevCost(values, tier, industry = INDUSTRY) {
  let rows;
  try { rows = values.dt(`DT_Project_${industry}`); } catch { return null; }
  const end = tier * PROJECTS_PER_TIER;
  const row = rows.find(r => r.ProjectIndex === end);
  return row && Number.isFinite(row.DevelopmentCost) ? row.DevelopmentCost : null;
}

// ── 통계 헬퍼 ───────────────────────────────────────────────────────────────
function median(arr) {
  const s = arr.filter(Number.isFinite).slice().sort((a, b) => a - b);
  return s.length ? quantiles(s).median : null;
}
function maxAbs(list) {
  let m = 0;
  for (const v of list) if (Number.isFinite(v)) m = Math.max(m, Math.abs(v));
  return m;
}
// 시드 노이즈 상쇄 — 같은 시드끼리 짝지어 상대차를 낸다.
// ⚠ 대표값은 **평균**이다. 인앱시간은 세션(7.5분) 단위로 양자화돼 있어, 노브 효과가 세션 1개
// 미만이면 과반 시드의 페어드 차가 정확히 0 이 되고 중앙값이 0 으로 고정된다(사각지대).
// 2026-08-02 실측: devCostCoef +20% 는 시드 1000 에서 89→91세션(+2.2%)인데 중앙값은 0.0% 였다
// — 중앙값만 보면 살아있는 노브를 죽은 노브로 오판한다. 중앙값은 median 필드로 함께 남긴다.
function pairedDiff(baseBySeed, varBySeed, sel, completedOnly) {
  const diffs = [];
  for (const [seed, b] of baseBySeed) {
    const v = varBySeed.get(seed);
    if (!v) continue;
    if (completedOnly && !(b.completed && v.completed)) continue;
    const bv = sel(b);
    const vv = sel(v);
    if (!Number.isFinite(bv) || !Number.isFinite(vv) || bv === 0) continue;
    diffs.push((vv - bv) / bv);
  }
  const mean = diffs.length ? diffs.reduce((a, x) => a + x, 0) / diffs.length : null;
  return { value: mean, median: median(diffs), n: diffs.length };
}
// 완주율은 베이스가 0 이면 상대차가 정의되지 않는다 → 그때만 절대차(0→0.4 = +0.4).
function relOrAbs(base, v) {
  if (!Number.isFinite(base) || !Number.isFinite(v)) return null;
  return base > 0 ? (v - base) / base : v - base;
}

// values = 그 런 집합이 실제로 쓴 Values(패치본이면 패치본) — 소프트락 Money 기준선 산출용.
function summarize(results, { values = null } = {}) {
  const valid = results.filter(r => !r.invalid);
  const done = valid.filter(r => r.endState.completed);
  const bySeed = new Map();
  for (const r of valid) {
    bySeed.set(r.seed, { completed: r.endState.completed, inAppH: r.endState.inAppMin / 60, tier: r.endState.reachedTier });
  }
  const causes = {};
  let softlocked = 0, inflating = 0;
  let sample = null;
  for (const r of valid) {
    const s = detectSoftlock(r, { values });
    if (s.softlocked) {
      softlocked++;
      const c = s.cause || 'Unknown';
      causes[c] = (causes[c] || 0) + 1;
      if (!sample) sample = { seed: r.seed, cause: c, detail: s.detail };
    }
    if (detectInflation(r).inflating) inflating++;
  }
  return {
    medianInAppH: median(done.map(r => r.endState.inAppMin / 60)),
    medianTierReached: median(valid.map(r => r.endState.reachedTier)),
    completionRate: valid.length ? done.length / valid.length : 0,
    invalidRuns: results.length - valid.length,
    softlockRate: valid.length ? softlocked / valid.length : 0,
    softlockCauses: causes,
    softlockSample: sample,
    inflationRate: valid.length ? inflating / valid.length : 0,
    bySeed,
  };
}

function publicStats(s) {
  return {
    medianInAppH: s.medianInAppH, medianTierReached: s.medianTierReached,
    completionRate: s.completionRate, invalidRuns: s.invalidRuns,
    softlockRate: s.softlockRate, softlockCauses: s.softlockCauses, inflationRate: s.inflationRate,
  };
}

// ── 감도 스캔 ───────────────────────────────────────────────────────────────
function sensitivityScan({
  knobs = Object.keys(KNOBS), deltas = [-0.2, 0.2], seeds = 50, profile = 'mid',
  valuesPath = DEFAULT_VALUES, calibPath = null, seedStart = SCAN_SEED_START,
  untilTier = 10, maxCalendarDays = 120, tmpDir = DEFAULT_TMP, onProgress = null,
} = {}) {
  knobs.forEach(k => knobPatches(k, 0)); // 노브 오타는 베이스라인 50런을 태우기 전에 터뜨린다
  const seedList = Array.from({ length: seeds }, (_, i) => seedStart + i);
  const total = (1 + knobs.length * deltas.length) * seeds;
  let doneRuns = 0;
  const tick = label => {
    doneRuns += seeds;
    if (onProgress) onProgress({ done: doneRuns, total, label });
  };
  const runSet = vp => seedList.map(seed => runWorld({
    seed, profile, valuesPath: vp, calibPath, untilTier, maxCalendarDays,
  }));

  const base = summarize(runSet(valuesPath), { values: loadValues(valuesPath) });
  tick('baseline');

  const rows = [];
  for (const knob of knobs) {
    for (const delta of deltas) {
      // 파일명에 pid — 동시 스캔 두 프로세스가 같은 임시 파일을 밟지 않게(결과 rows 는 파일명과 무관).
      const dest = path.join(tmpDir, `values-${knob.replace(/[^\w.-]/g, '_')}-${deltaTag(delta)}-${process.pid}.json`);
      overrideValues(valuesPath, knobPatches(knob, delta), dest);
      let stats;
      try {
        // 패치본 values 로 판정한다 — devCostCoef/reqCurve 처럼 개발비를 바꾸는 노브는
        // 소프트락 Money 기준선도 같이 움직여야 한다.
        stats = summarize(runSet(dest), { values: loadValues(dest) });
      } finally {
        fs.rmSync(dest, { force: true });
      }
      rows.push(makeRow(knob, delta, stats, base, seeds));
      tick(`${knob} ${deltaTag(delta)}`);
    }
  }
  // 빈 임시 폴더는 남기지 않는다(Content/out 오염 방지 규칙).
  try { if (fs.existsSync(tmpDir) && fs.readdirSync(tmpDir).length === 0) fs.rmdirSync(tmpDir); } catch { /* 동시 사용 중이면 그대로 둔다 */ }
  return rows;
}

const deltaTag = d => `${d >= 0 ? '+' : ''}${(d * 100).toFixed(0)}%`;

function makeRow(knob, delta, s, base, seeds) {
  // 인앱시간은 완주 런끼리만 비교(미완주 런의 inAppMin 은 캘린더 캡에 걸린 값 — run.js 와 같은 규약).
  const h = pairedDiff(base.bySeed, s.bySeed, x => x.inAppH, true);
  const t = pairedDiff(base.bySeed, s.bySeed, x => x.tier, false);
  const c = relOrAbs(base.completionRate, s.completionRate);
  return {
    knob, delta, seeds,
    medianInAppH: s.medianInAppH,
    medianTierReached: s.medianTierReached,
    completionRate: s.completionRate,
    diffPct: h.value,               // 평균 페어드 상대차(감도 대표값)
    diffPctMedian: h.median,        // 중앙값 — 양자화 사각지대 있음(pairedDiff 주석 참조)
    diffPctTier: t.value,
    diffPctTierMedian: t.median,
    diffPctCompletion: c,
    // 죽은 노브 판정축 — 인앱시간만 보면 시간축만 미는 노브(gradeLifespan 류)를 오판한다.
    effect: maxAbs([h.value, t.value, c]),
    pairedSeeds: h.n,
    invalidRuns: s.invalidRuns,
    softlockRate: s.softlockRate,
    softlockCauses: s.softlockCauses,
    inflationRate: s.inflationRate,
    base: publicStats(base),
  };
}

// ── 판정: 죽은 노브 / 절벽 ──────────────────────────────────────────────────
function rowEffect(r) {
  if (Number.isFinite(r.effect)) return r.effect;
  return maxAbs([r.diffPct, r.diffPctTier, r.diffPctCompletion]);
}
function groupByKnob(rows) {
  const m = new Map();
  for (const r of rows) {
    if (!m.has(r.knob)) m.set(r.knob, []);
    m.get(r.knob).push(r);
  }
  return m;
}

function detectDeadKnobs(rows, threshold = DEAD_THRESHOLD) {
  const out = [];
  for (const [knob, rs] of groupByKnob(rows)) {
    const effects = rs.map(rowEffect);
    const maxEffect = Math.max(...effects);
    if (maxEffect < threshold) {
      out.push({ knob, maxEffect, threshold, deltas: rs.map((r, i) => ({ delta: r.delta, effect: effects[i] })) });
    }
  }
  return out;
}

// 절벽 = ±델타의 효과 크기가 ratio 배 이상 비대칭(한쪽으로만 급격히 무너지거나 튀는 노브).
// 양쪽 다 무효과(죽은 노브)면 비율이 아무리 커도 절벽이 아니다 → minEffect 하한.
function detectCliffs(rows, ratio = CLIFF_RATIO, minEffect = DEAD_THRESHOLD) {
  const out = [];
  for (const [knob, rs] of groupByKnob(rows)) {
    const neg = rs.filter(r => r.delta < 0);
    const pos = rs.filter(r => r.delta > 0);
    if (!neg.length || !pos.length) continue;
    const eNeg = Math.max(...neg.map(rowEffect));
    const ePos = Math.max(...pos.map(rowEffect));
    const hi = Math.max(eNeg, ePos);
    const lo = Math.min(eNeg, ePos);
    if (hi < minEffect) continue;
    const asymmetry = lo > 0 ? hi / lo : Infinity;
    if (asymmetry >= ratio) {
      out.push({ knob, effectMinus: eNeg, effectPlus: ePos, asymmetry, ratio, steepSide: ePos >= eNeg ? '+' : '-' });
    }
  }
  return out;
}

// ── 판정: 소프트락 ──────────────────────────────────────────────────────────
// 정체 = 미완주 + 마지막 티어에 눌러앉음. "눌러앉음"의 판정은 두 갈래(OR):
//   (a) 전체 세션의 stallShare 이상을 그 티어가 먹음
//   (b) 앞선 티어들의 전형(중앙값) 체류 대비 overrunRatio 배 이상 — 스케일 프리.
// (a) 만 쓰면 앞 티어에서도 오래 막혔던 런(예: T8 200세션 뒤 T9 169세션 = 40%)을 놓친다(2026-08-02 실측).
// 어느 쪽이든 최소 minStallSessions 는 넘겨야 한다(캘린더 캡 직전 진입한 티어를 정체로 오판 방지).
function detectSoftlock(runResult, {
  minStallSessions = 20, stallShare = 0.5, overrunRatio = 5, gateFailShare = 0.5,
  values = null, tierDevCost = null,
} = {}) {
  const es = runResult.endState || {};
  const tiers = runResult.tiers || [];
  const idle = extra => Object.assign({ softlocked: false, cause: null, tier: null, data: {} }, extra);
  if (es.completed) return idle({ detail: `완주(T${es.reachedTier}) — 정체 없음` });
  if (!tiers.length) return idle({ detail: '티어 기록 없음 — 판정 불가' });

  const stall = tiers[tiers.length - 1];
  const totalSessions = Number.isFinite(es.sessions) ? es.sessions : tiers.reduce((a, t) => a + (t.sessions || 0), 0);
  const share = totalSessions > 0 ? (stall.sessions || 0) / totalSessions : 0;
  const typicalTierSessions = median(tiers.slice(0, -1).map(t => t.sessions || 0).filter(n => n > 0));
  const overrun = typicalTierSessions > 0 ? (stall.sessions || 0) / typicalTierSessions : Infinity;
  const g = stall.grades || {};
  const gateFail = g.gateFail || 0;
  const launches = (stall.clears || 0) + (stall.repeats || 0) + gateFail;
  // Money 기준선 = **정체 티어의 실제 개발비**(밴드 끝 행). 이건 world.js 의 reserve 기준
  // (nextDevCost = 현 티어 밴드 끝 행)과 같은 참조라, 시뮬이 실제로 요구하는 금액과 일치한다.
  // ⚠ 폴백인 "런 전체 평균 착수비(K1/착수수)"는 앞 티어의 싼 카드까지 섞여 정체 티어 실비보다
  //    항상 낮다(실측 −35~40%) → Money 원인을 구조적으로 과소검출한다(2026-08-02 리뷰 F2).
  const k1 = Math.abs((runResult.ledger && runResult.ledger.bySrc && runResult.ledger.bySrc['Money|K1']) || 0);
  const avgDevCost = es.launches > 0 ? k1 / es.launches : 0;
  const costFn = tierDevCost || (values ? t => tierBandDevCost(values, t) : null);
  const tierCost = costFn ? costFn(stall.tier) : null;
  const useTierCost = Number.isFinite(tierCost) && tierCost > 0;
  const refCost = useTierCost ? tierCost : avgDevCost;
  const costBasis = useTierCost ? 'tierDevCost' : 'avgDevCost(과소검출 편향)';
  const moneyStarved = refCost > 0 && Number.isFinite(es.money) && es.money < refCost;
  const data = {
    tier: stall.tier, stallSessions: stall.sessions, totalSessions, sessionShare: share,
    typicalTierSessions, overrun,
    launches, gateFail, expGateBlocks: stall.expGateBlocks || 0,
    endMoney: es.money, avgDevCost, tierDevCost: useTierCost ? tierCost : null,
    refCost, costBasis, moneyStarved, calendarDay: es.calendarDay,
  };

  if (!((stall.sessions || 0) >= minStallSessions && (share >= stallShare || overrun >= overrunRatio))) {
    return idle({
      tier: stall.tier, data,
      detail: `T${stall.tier} 체류 ${stall.sessions}세션(전체의 ${(share * 100).toFixed(0)}%, 전형 티어의 ${fmt(overrun)}배)`
        + ` — 정체 기준(${minStallSessions}세션 + [비중 ${stallShare * 100}% | 전형 대비 ×${overrunRatio}]) 미달`,
    });
  }

  let cause, why;
  if ((stall.expGateBlocks || 0) > 0) {
    cause = 'BuildingEXP';
    why = `7첫클리어를 채우고도 빌딩Lv 게이트에 ${stall.expGateBlocks}회 막힘`;
  } else if (launches > 0 && gateFail / launches >= gateFailShare) {
    cause = 'DevScore';
    why = `착수 ${launches}회 중 ${gateFail}회(${((gateFail / launches) * 100).toFixed(0)}%)가 개발점수 게이트 실패`;
  } else if (moneyStarved) {
    cause = 'Money';
    why = `잔액 ${Math.round(es.money)} < T${stall.tier} 개발비 ${Math.round(refCost)}(${costBasis}) — 착수비 미충당`;
  } else if (launches === 0) {
    cause = 'Content';
    why = '착수 0회인데 잔액은 충분 — 피드에 착수 가능한 카드가 없음(DT 결손 의심)';
  } else {
    cause = 'Unknown';
    why = `착수 ${launches}회·게이트실패 ${gateFail}회·잔액 ${Math.round(es.money)} — 단일 병목 식별 실패`;
  }
  return {
    softlocked: true, cause, tier: stall.tier, data,
    detail: `T${stall.tier} 정체(${stall.sessions}/${totalSessions}세션 = 전형 티어의 ${fmt(overrun)}배, day ${es.calendarDay}) — ${why}`,
  };
}

// ── 판정: 인플레 ────────────────────────────────────────────────────────────
// 일별 (유입 / 유출) 비율을 등분 버킷으로 접어 단조 증가 + 발산 배수로 본다.
// 비율이 계속 커진다 = 싱크가 유입을 못 따라간다(돈이 쌓이기만 하는 구간 진입).
function detectInflation(runResult, { currency = 'Money', buckets = 4, growthRatio = 2.0, minDays = 8 } = {}) {
  const byDay = runResult.ledger && runResult.ledger.byDay;
  if (!byDay || typeof byDay !== 'object') {
    throw new Error('ledger.byDay 없음 — 일별 흐름 없이는 인플레 판정 불가(구 RunResult 재생성 필요)');
  }
  const days = [];
  for (const [k, v] of Object.entries(byDay)) {
    const [d, cur] = k.split('|');
    if (cur !== currency) continue;
    const inflow = v.in || 0;
    const outflow = v.out || 0;
    if (inflow === 0 && outflow === 0) continue;
    days.push({ day: parseInt(d, 10), in: inflow, out: outflow });
  }
  days.sort((a, b) => a.day - b.day);
  const totalIn = days.reduce((a, d) => a + d.in, 0);
  const totalOut = days.reduce((a, d) => a + d.out, 0);
  const endBal = Number.isFinite(runResult.endState && runResult.endState[currency.toLowerCase()])
    ? runResult.endState[currency.toLowerCase()]
    : ((runResult.ledger.balances && runResult.ledger.balances[currency]) || 0);
  const unspentShare = totalIn > 0 ? endBal / totalIn : 0;

  if (days.length < minDays) {
    return { inflating: false, ratios: [], growth: null, monotone: false, days: days.length, unspentShare, totalIn, totalOut, detail: `관측 ${days.length}일 < 최소 ${minDays}일 — 판정 보류` };
  }
  const ratios = [];
  for (let i = 0; i < buckets; i++) {
    const seg = days.slice(Math.floor(days.length * i / buckets), Math.floor(days.length * (i + 1) / buckets));
    const si = seg.reduce((a, d) => a + d.in, 0);
    const so = seg.reduce((a, d) => a + d.out, 0);
    ratios.push(so > 0 ? si / so : (si > 0 ? Infinity : 0));
  }
  const monotone = ratios.every((r, i) => i === 0 || r > ratios[i - 1]);
  const first = ratios[0];
  const last = ratios[ratios.length - 1];
  const growth = first > 0 ? last / first : (last > 0 ? Infinity : 1);
  const inflating = monotone && growth >= growthRatio;
  return {
    inflating, ratios, growth, monotone, days: days.length, unspentShare, totalIn, totalOut,
    detail: inflating
      ? `유입/싱크 비율 ${fmt(first)}→${fmt(last)} 단조 발산(×${fmt(growth)}), 미소진 ${(unspentShare * 100).toFixed(1)}%`
      : `유입/싱크 비율 ${fmt(first)}→${fmt(last)}${monotone ? ' 단조' : ' 비단조'}(×${fmt(growth)}) — 발산 기준 ×${growthRatio} 미달`,
  };
}

// ── 프로필 스윕(소프트락/인플레 관측) ───────────────────────────────────────
function profileSweep({
  profiles, seeds, profileSeeds, valuesPath, calibPath, seedStart = SCAN_SEED_START,
  untilTier = 10, maxCalendarDays = 120, onProgress = null,
}) {
  const n = profileSeeds || seeds;
  const values = loadValues(valuesPath);
  return profiles.map(profile => {
    const runs = Array.from({ length: n }, (_, i) => runWorld({
      seed: seedStart + i, profile, valuesPath, calibPath, untilTier, maxCalendarDays,
    }));
    const s = summarize(runs, { values });
    if (onProgress) onProgress({ label: `profile ${profile}` });
    return Object.assign({ profile, seeds: n }, publicStats(s), { softlockSample: s.softlockSample });
  });
}

// ── CLI ─────────────────────────────────────────────────────────────────────
function parseArgs(argv) {
  const a = {
    profile: 'mid', seeds: 50, seedStart: SCAN_SEED_START,
    values: DEFAULT_VALUES, calib: null, out: DEFAULT_OUT, tmpDir: DEFAULT_TMP,
    knobs: Object.keys(KNOBS), deltas: [-0.2, 0.2],
    untilTier: 10, maxCalendarDays: 120,
    // 진단 4-arm(mid/midNoTap/lowTap/low)이 항상 함께 나와야 탭 duty ↔ 투자셋 교락이 풀린다.
    profiles: ['low', 'lowTap', 'mid', 'midNoTap', 'high', 'skilled'], profileSeeds: null,
    deadThreshold: DEAD_THRESHOLD, cliffRatio: CLIFF_RATIO, json: false,
  };
  for (let i = 0; i < argv.length; i++) {
    const k = argv[i];
    const v = argv[i + 1];
    switch (k) {
      case '--profile': a.profile = v; i++; break;
      case '--seeds': a.seeds = parseInt(v, 10); i++; break;
      case '--seed-start': a.seedStart = parseInt(v, 10); i++; break;
      case '--values': a.values = v; i++; break;
      case '--calib': a.calib = v; i++; break;
      case '--out': a.out = v; i++; break;
      case '--tmp': a.tmpDir = v; i++; break;
      case '--knobs': a.knobs = v.split(',').map(s => s.trim()).filter(Boolean); i++; break;
      case '--deltas': a.deltas = v.split(',').map(s => parseFloat(s.trim())).filter(Number.isFinite); i++; break;
      case '--until-tier': a.untilTier = parseInt(v, 10); i++; break;
      case '--max-days': a.maxCalendarDays = parseInt(v, 10); i++; break;
      case '--profiles': a.profiles = v.split(',').map(s => s.trim()).filter(Boolean); i++; break;
      case '--profile-seeds': a.profileSeeds = parseInt(v, 10); i++; break;
      case '--no-profiles': a.profiles = []; break;
      case '--dead-threshold': a.deadThreshold = parseFloat(v); i++; break;
      case '--cliff-ratio': a.cliffRatio = parseFloat(v); i++; break;
      case '--json': a.json = true; break;
      default: break;
    }
  }
  return a;
}

const fmt = v => (v === null || v === undefined || !Number.isFinite(v) ? (v === Infinity ? '∞' : '-') : Number(v).toFixed(2));
const pct = v => (v === null || v === undefined || !Number.isFinite(v) ? '-' : `${(v * 100).toFixed(1)}%`);
const sgnPct = v => (v === null || v === undefined || !Number.isFinite(v) ? '-' : `${v >= 0 ? '+' : ''}${(v * 100).toFixed(1)}%`);

function printTable(rows, base) {
  const w = Math.max(6, ...rows.map(r => r.knob.length));
  console.log('');
  console.log(`${'노브'.padEnd(w)}  ${'델타'.padStart(5)}  ${'인앱h중앙'.padStart(9)}  ${'Δ인앱평균'.padStart(9)}  ${'Δ인앱중앙'.padStart(9)}  ${'티어'.padStart(5)}  ${'Δ티어'.padStart(7)}  ${'완주율'.padStart(7)}  ${'Δ완주'.padStart(7)}  ${'효과'.padStart(6)}`);
  console.log(`${'(baseline)'.padEnd(w)}  ${''.padStart(5)}  ${fmt(base.medianInAppH).padStart(9)}  ${''.padStart(9)}  ${''.padStart(9)}  ${fmt(base.medianTierReached).padStart(5)}  ${''.padStart(7)}  ${pct(base.completionRate).padStart(7)}`);
  for (const r of rows) {
    console.log(`${r.knob.padEnd(w)}  ${deltaTag(r.delta).padStart(5)}  ${fmt(r.medianInAppH).padStart(9)}  ${sgnPct(r.diffPct).padStart(9)}  ${sgnPct(r.diffPctMedian).padStart(9)}  `
      + `${fmt(r.medianTierReached).padStart(5)}  ${sgnPct(r.diffPctTier).padStart(7)}  ${pct(r.completionRate).padStart(7)}  ${sgnPct(r.diffPctCompletion).padStart(7)}  ${pct(r.effect).padStart(6)}`);
  }
}

function main(argv) {
  const a = parseArgs(argv);
  const t0 = Date.now();
  const el = () => `${((Date.now() - t0) / 1000).toFixed(0)}s`;
  console.log(`파탄 스캐너 — profile=${a.profile} seeds=${a.seeds} 노브 ${a.knobs.length}종 × 델타 [${a.deltas.map(deltaTag).join(', ')}]`);
  console.log(`  values=${a.values}${a.calib ? ` calib=${a.calib}` : ''} 시드 ${a.seedStart}~${a.seedStart + a.seeds - 1}`);

  const rows = sensitivityScan({
    knobs: a.knobs, deltas: a.deltas, seeds: a.seeds, profile: a.profile,
    valuesPath: a.values, calibPath: a.calib, seedStart: a.seedStart,
    untilTier: a.untilTier, maxCalendarDays: a.maxCalendarDays, tmpDir: a.tmpDir,
    onProgress: p => console.log(`  [${String(p.done).padStart(String(p.total).length)}/${p.total} 런] ${p.label} (${el()})`),
  });
  const base = rows.length ? rows[0].base : null;
  const dead = detectDeadKnobs(rows, a.deadThreshold);
  const cliffs = detectCliffs(rows, a.cliffRatio, a.deadThreshold);

  let profiles = [];
  if (a.profiles.length) {
    console.log(`\n프로필 스윕(소프트락/인플레) — ${a.profiles.join(', ')} × ${a.profileSeeds || a.seeds}시드`);
    profiles = profileSweep({
      profiles: a.profiles, seeds: a.seeds, profileSeeds: a.profileSeeds,
      valuesPath: a.values, calibPath: a.calib, seedStart: a.seedStart,
      untilTier: a.untilTier, maxCalendarDays: a.maxCalendarDays,
      onProgress: p => console.log(`  ${p.label} 완료 (${el()})`),
    });
  }

  const report = {
    meta: {
      generatedAt: new Date().toISOString(), args: a,
      elapsedSec: (Date.now() - t0) / 1000,
      deadThreshold: a.deadThreshold, cliffRatio: a.cliffRatio,
    },
    baseline: base, rows, dead, cliffs, profiles,
  };
  fs.mkdirSync(path.dirname(a.out), { recursive: true });
  fs.writeFileSync(a.out, JSON.stringify(report, null, 1));

  if (a.json) {
    console.log(JSON.stringify(report, null, 1));
    return 0;
  }
  if (base) printTable(rows, base);

  console.log(`\n죽은 노브(최대효과 < ${pct(a.deadThreshold)}): ${dead.length ? dead.map(d => `${d.knob}(${pct(d.maxEffect)})`).join(', ') : '없음'}`);
  console.log(`절벽(±비대칭 ≥ ×${a.cliffRatio}): ${cliffs.length ? cliffs.map(c => `${c.knob}(−${pct(c.effectMinus)} vs +${pct(c.effectPlus)}, ×${fmt(c.asymmetry)})`).join(', ') : '없음'}`);

  const slRows = rows.filter(r => r.softlockRate > 0);
  console.log(`\n노브 변형 중 소프트락 유발: ${slRows.length ? slRows.map(r => `${r.knob}${deltaTag(r.delta)} ${pct(r.softlockRate)}[${Object.entries(r.softlockCauses).map(([k, v]) => `${k}:${v}`).join(' ')}]`).join(', ') : '없음'}`);
  const infRows = rows.filter(r => r.inflationRate > 0);
  console.log(`노브 변형 중 인플레 검출: ${infRows.length ? infRows.map(r => `${r.knob}${deltaTag(r.delta)} ${pct(r.inflationRate)}`).join(', ') : '없음'}`);

  if (profiles.length) {
    console.log('');
    const pw = Math.max(6, ...profiles.map(p => p.profile.length));
    console.log(`${'프로필'.padEnd(pw)}  ${'완주율'.padStart(7)}  ${'인앱h'.padStart(7)}  ${'티어'.padStart(5)}  ${'소프트락'.padStart(8)}  ${'인플레'.padStart(7)}  원인`);
    for (const p of profiles) {
      const causes = Object.entries(p.softlockCauses).map(([k, v]) => `${k}:${v}`).join(' ') || '-';
      console.log(`${p.profile.padEnd(pw)}  ${pct(p.completionRate).padStart(7)}  ${fmt(p.medianInAppH).padStart(7)}  ${fmt(p.medianTierReached).padStart(5)}  ${pct(p.softlockRate).padStart(8)}  ${pct(p.inflationRate).padStart(7)}  ${causes}`);
      if (p.softlockSample) console.log(`${''.padEnd(pw)}    └ ${p.softlockSample.detail}`);
    }
  }
  console.log(`\n총 ${el()} — 결과: ${a.out}`);
  return 0;
}

if (require.main === module) process.exitCode = main(process.argv.slice(2));

module.exports = {
  overrideValues, knobPatches, KNOBS, tierBandDevCost,
  sensitivityScan, profileSweep, summarize,
  detectDeadKnobs, detectCliffs, detectSoftlock, detectInflation,
  parseArgs, main,
  DEAD_THRESHOLD, CLIFF_RATIO,
};
