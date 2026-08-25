'use strict';
const fs = require('node:fs');
const path = require('node:path');

// 재화 게이트웨이(UResourceItemManager) + 아이템 게이트웨이(UItemInventoryManager) 실제 public API.
// AddItem/SpendItem은 Public/Manager/ItemInventoryManager.h 실측(2026-08-02) 기준 — Add/Consume 계열 함수명이 그대로 AddItem/SpendItem.
const APIS = ['StoreResource(', 'SpendResource(', 'ExtractResource(', 'AddItem(', 'SpendItem('];

// UMG TListView<T>::AddItem(엔트리 위젯 데이터 추가)가 동명이라 오탐 — ItemInventoryManager와 무관.
const LISTVIEW_RECEIVER = 'ListView->';

function scanSites(repoRoot) {
  const root = path.join(repoRoot, 'Source', 'CompanyGrowthRenewal');
  const sites = [];
  (function walk(dir) {
    for (const f of fs.readdirSync(dir, { withFileTypes: true })) {
      const p = path.join(dir, f.name);
      if (f.isDirectory()) { walk(p); continue; }
      if (!f.name.endsWith('.cpp')) continue;
      const rel = path.relative(root, p).split(path.sep).join('/');
      fs.readFileSync(p, 'utf8').split('\n').forEach((line, i) => {
        const t = line.trim();
        if (t.startsWith('//')) return; // 주석 스킵
        if (t.includes('::StoreResource') || t.includes('::SpendResource') || t.includes('::ExtractResource')
          || t.includes('::AddItem') || t.includes('::SpendItem')) return; // 정의부 시그니처 스킵
        for (const api of APIS) {
          if (!t.includes(api)) continue;
          if ((api === 'AddItem(' || api === 'SpendItem(') && t.includes(LISTVIEW_RECEIVER + api)) continue; // ListView 오탐 스킵
          sites.push({ file: rel, line: i + 1, api: api.slice(0, -1) });
        }
      });
    }
  })(root);
  return sites;
}

// entry.file이 상위 경로를 포함해도 되게(Public/... 또는 Private/... 부분부터) 정규화.
function normalizeFile(f) {
  const idx = Math.max(f.lastIndexOf('Private/'), f.lastIndexOf('Public/'));
  return idx >= 0 ? f.slice(idx) : f;
}

function matchSites(sites, entries) {
  const unmatched = sites.filter(s => !entries.some(e => e.file && s.file.endsWith(normalizeFile(e.file))));
  return { unmatched };
}

function validateEntries(entries) {
  const bad = entries.filter(e => (e.status === 'stubbed' || e.status === 'n/a') && !e.reason);
  // scanExempt(스캐너 API 패턴으로 원천 관측 불가 — 필드 직접 증감/BP 전용 호출 등)는 사유 없이 조용히 빠지는 것 금지.
  const badExempt = entries.filter(e => e.scanExempt && !e.scanExemptReason);
  // modeled 는 "어딘가에 모델이 있다"일 뿐이라 본 런 도달 여부를 따로 못 박지 않으면 리포트가 61건 전부
  // 계산에 반영된 것처럼 읽힌다(2026-08-03 실측: 원장 도달은 11건). pipelineWired 를 필수화한다.
  const missingWired = entries.filter(e => e.status === 'modeled' && typeof e.pipelineWired !== 'boolean');
  const missingWiredReason = entries.filter(e => e.pipelineWired === false && !e.pipelineReason);
  return {
    ok: bad.length === 0 && badExempt.length === 0
      && missingWired.length === 0 && missingWiredReason.length === 0,
    missingReason: bad.map(e => e.id),
    scanExemptMissingReason: badExempt.map(e => e.id),
    missingPipelineWired: missingWired.map(e => e.id),
    missingPipelineReason: missingWiredReason.map(e => e.id),
  };
}

// sim/world.js 소스에서 원장 post 의 srcId 를 뽑는다 — pipelineWired 의 기계 판정 근거.
// (out/runs 는 .gitignore 라 테스트가 의존할 수 없다. 추적되는 소스만으로 판정한다.)
const WORLD_POST_RE = /post\(\s*'[^']*'\s*,[^,]*,\s*'([^']+)'\s*\)/g;
function worldLedgerSrcIds(repoRoot) {
  const p = path.join(repoRoot, 'Tools', 'Balance', 'sim', 'world.js');
  const src = fs.readFileSync(p, 'utf8');
  const ids = new Set();
  for (const m of src.matchAll(WORLD_POST_RE)) ids.add(m[1]);
  return ids;
}

// pipelineWired 선언 ↔ world.js 실제 post 호출 대조. 선언만 바꾸고 코드를 안 바꾸는(또는 그 반대) 표류를 막는다.
// modeled 가 아닌 엔트리(S11/DS4 처럼 stubbed 인데 post 되는 것)는 pipelineWired 필드 자체가 없으므로 제외한다.
function checkPipelineWired(repoRoot, entries) {
  const posted = worldLedgerSrcIds(repoRoot);
  const modeled = entries.filter(e => e.status === 'modeled');
  const declaredTrue = modeled.filter(e => e.pipelineWired === true).map(e => e.id);
  const shouldBeTrue = modeled.filter(e => posted.has(e.id)).map(e => e.id);
  const falsePositive = declaredTrue.filter(id => !posted.has(id));   // true 라 적었는데 world 에 post 없음
  const falseNegative = shouldBeTrue.filter(id => !declaredTrue.includes(id)); // post 하는데 false 라 적음
  return { posted: [...posted].sort(), declaredTrue: declaredTrue.sort(), falsePositive, falseNegative };
}

function checkCoverage(repoRoot, coveragePath) {
  const cov = JSON.parse(fs.readFileSync(coveragePath, 'utf8'));
  const v = validateEntries(cov.entries);
  const m = matchSites(scanSites(repoRoot), cov.entries);
  const pw = checkPipelineWired(repoRoot, cov.entries);
  // 문서화된 예외 목록 — 조용히 통과시키지 않고 매 실행 결과에 항상 노출.
  const exempt = cov.entries
    .filter(e => e.scanExempt)
    .map(e => ({ id: e.id, reason: e.scanExemptReason || null }));
  // ⚠ 스프레드 순서 주의 — v 에도 ok 가 있어 `{ ok, ...v }` 로 쓰면 validateEntries 의 ok 가 덮어써
  // unmatched 가 몇 건이든 ok=true 로 나온다(2026-08-02 Task13 발견, 미매칭 4건이 조용히 통과 중이었음).
  return {
    ...v, ...m, exempt, pipeline: pw,
    ok: v.ok && m.unmatched.length === 0
      && pw.falsePositive.length === 0 && pw.falseNegative.length === 0,
  };
}

module.exports = { scanSites, matchSites, validateEntries, checkCoverage, worldLedgerSrcIds, checkPipelineWired, APIS };
