#!/usr/bin/env node
// 몬테카를로 러너 CLI —
//   node Tools/Balance/sim/run.js --profile mid --seeds 300 --out Tools/Balance/out/runs
// 개별 런 JSON + agg-<profile>.json(중앙값/사분위) 을 --out 폴더에 쓴다.
const fs = require('node:fs');
const path = require('node:path');
const { runWorld } = require('./world.js');

function parseArgs(argv) {
  const a = {
    profile: 'mid', seeds: 100, seedStart: 1,
    values: 'Tools/Balance/out/values.json', calib: null,
    out: 'Tools/Balance/out/runs', untilTier: 10, maxCalendarDays: 120,
    writeRuns: true,
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
      case '--until-tier': a.untilTier = parseInt(v, 10); i++; break;
      case '--max-days': a.maxCalendarDays = parseInt(v, 10); i++; break;
      case '--no-runs': a.writeRuns = false; break;
      default: break;
    }
  }
  return a;
}

// 분위수 = R-7(Excel/numpy 기본): 위치 (n−1)p 에서 선형보간.
function quantiles(sorted) {
  const q = p => {
    if (sorted.length === 0) return null;
    const pos = (sorted.length - 1) * p;
    const lo = Math.floor(pos), hi = Math.ceil(pos);
    return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - lo);
  };
  return { min: sorted[0] ?? null, p25: q(0.25), median: q(0.5), p75: q(0.75), max: sorted[sorted.length - 1] ?? null };
}

function aggregate(results, profile) {
  const valid = results.filter(r => !r.invalid);
  const num = sel => valid.map(sel).filter(Number.isFinite).sort((a, b) => a - b);
  const gradeTotals = { S: 0, A: 0, B: 0, C: 0, gateFail: 0 };
  for (const r of valid) {
    for (const k of Object.keys(gradeTotals)) gradeTotals[k] += (r.gradeDist && r.gradeDist[k]) || 0;
  }
  const graded = gradeTotals.S + gradeTotals.A + gradeTotals.B + gradeTotals.C;
  // 미완주 런의 인앱시간은 캘린더 캡에 걸린 값이라 완주 런과 섞으면 무의미 — 분리 집계.
  const done = valid.filter(r => r.endState.completed);
  const numDone = sel => done.map(sel).filter(Number.isFinite).sort((a, b) => a - b);
  return {
    profile,
    runs: results.length,
    invalidRuns: results.length - valid.length,
    invalidSeeds: results.filter(r => r.invalid).map(r => ({ seed: r.seed, reason: r.invalid })),
    completionRate: valid.length ? done.length / valid.length : 0,
    inAppHoursCompleted: quantiles(numDone(r => r.endState.inAppMin / 60)),
    calendarDaysCompleted: quantiles(numDone(r => r.endState.calendarDay)),
    inAppHours: quantiles(num(r => r.endState.inAppMin / 60)),
    calendarDays: quantiles(num(r => r.endState.calendarDay)),
    reachedTier: quantiles(num(r => r.endState.reachedTier)),
    launches: quantiles(num(r => r.endState.launches)),
    repeats: quantiles(num(r => r.endState.repeats)),
    gateFails: quantiles(num(r => r.endState.gateFails)),
    roster: quantiles(num(r => r.endState.roster)),
    avgEmployeeLevel: quantiles(num(r => r.endState.avgEmployeeLevel)),
    primaryBuildingLevel: quantiles(num(r => r.endState.primaryBuildingLevel)),
    moneyEnd: quantiles(num(r => r.endState.money)),
    vaultLostMoney: quantiles(num(r => r.endState.vaultLostMoney)),
    gradeShare: graded > 0
      ? { S: gradeTotals.S / graded, A: gradeTotals.A / graded, B: gradeTotals.B / graded, C: gradeTotals.C / graded }
      : null,
    gateFailShare: graded + gradeTotals.gateFail > 0 ? gradeTotals.gateFail / (graded + gradeTotals.gateFail) : null,
    tierEntryDayMedian: medianPerTier(valid, t => t.calendarDay),
    tierInAppMinMedian: medianPerTier(valid, t => t.inAppMin),
    tierRepeatsMedian: medianPerTier(valid, t => t.repeats),
  };
}

function medianPerTier(results, sel) {
  const out = {};
  for (let tier = 1; tier <= 10; tier++) {
    const vals = results.map(r => r.tiers.find(t => t.tier === tier)).filter(Boolean).map(sel)
      .filter(Number.isFinite).sort((a, b) => a - b);
    if (vals.length) out[tier] = quantiles(vals).median;
  }
  return out;
}

function main(argv) {
  const a = parseArgs(argv);
  fs.mkdirSync(a.out, { recursive: true });
  const results = [];
  for (let i = 0; i < a.seeds; i++) {
    const seed = a.seedStart + i;
    const r = runWorld({
      seed, profile: a.profile, valuesPath: a.values, calibPath: a.calib,
      untilTier: a.untilTier, maxCalendarDays: a.maxCalendarDays,
    });
    results.push(r);
    if (a.writeRuns) {
      fs.writeFileSync(path.join(a.out, `${a.profile}-${seed}.json`), JSON.stringify(r, null, 1));
    }
  }
  const agg = aggregate(results, a.profile);
  // 메타(실행 시각/인자)는 집계 파일에만 — 시뮬 상태에는 절대 안 들어간다(결정성 유지).
  agg.meta = { generatedAt: new Date().toISOString(), args: a, valuesHash: results[0] ? results[0].valuesHash : null };
  fs.writeFileSync(path.join(a.out, `agg-${a.profile}.json`), JSON.stringify(agg, null, 1));
  const h = agg.inAppHoursCompleted;
  console.log(`[${a.profile}] runs=${agg.runs} invalid=${agg.invalidRuns} 완주율=${pct(agg.completionRate)} `
    + `완주 인앱h p25/med/p75 = ${fmt(h.p25)}/${fmt(h.median)}/${fmt(h.p75)} `
    + `도달티어 med=${fmt(agg.reachedTier.median)} 캘린더일 med=${fmt(agg.calendarDaysCompleted.median)}`);
  if (agg.gradeShare) {
    console.log(`  등급분포 C/B/A/S = ${pct(agg.gradeShare.C)}/${pct(agg.gradeShare.B)}/${pct(agg.gradeShare.A)}/${pct(agg.gradeShare.S)}`
      + ` (게이트실패 ${pct(agg.gateFailShare)})`);
  }
  return agg;
}

const fmt = v => (v === null || v === undefined ? '-' : Number(v).toFixed(2));
const pct = v => (v === null || v === undefined ? '-' : `${(v * 100).toFixed(1)}%`);

if (require.main === module) main(process.argv.slice(2));

module.exports = { aggregate, quantiles, parseArgs, main };
