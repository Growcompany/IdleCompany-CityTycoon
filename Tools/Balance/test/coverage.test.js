const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { scanSites, matchSites, validateEntries, checkCoverage, worldLedgerSrcIds, checkPipelineWired } = require('../verify/coverage_check.js');

test('스캔된 호출 지점이 엔트리에 없으면 unmatched', () => {
  const sites = [{ file: 'Private/Manager/NewManager.cpp', line: 10, api: 'StoreResource' }];
  const entries = [{ id: 'S1', file: 'Private/Manager/ProjectOperationManager.cpp', status: 'modeled' }];
  const r = matchSites(sites, entries);
  assert.strictEqual(r.unmatched.length, 1);
});

test('stubbed인데 reason 없으면 실패', () => {
  const r = validateEntries([{ id: 'K7', file: 'x.cpp', status: 'stubbed' }]);
  assert.strictEqual(r.ok, false);
});

test('n/a인데 reason 없으면 실패, id가 missingReason에 담김', () => {
  const r = validateEntries([{ id: 'S14', file: 'x.cpp', status: 'n/a' }]);
  assert.strictEqual(r.ok, false);
  assert.deepStrictEqual(r.missingReason, ['S14']);
});

test('stubbed/n-a도 reason 있으면 통과, modeled는 pipelineWired만 있으면 reason 없어도 통과', () => {
  const r = validateEntries([
    { id: 'K7', file: 'x.cpp', status: 'stubbed', reason: '경제 영향 미미' },
    { id: 'S14', file: 'x.cpp', status: 'n/a', reason: '플레이어 경로 아님' },
    { id: 'S1', file: 'x.cpp', status: 'modeled', pipelineWired: true },
  ]);
  assert.strictEqual(r.ok, true);
  assert.deepStrictEqual(r.missingReason, []);
});

// ── pipelineWired (2026-08-03 리뷰 C3): modeled 라벨이 "본 런 도달"을 뜻하지 않는다는 사실을 강제 표면화 ──

test('modeled인데 pipelineWired 필드가 없으면 실패', () => {
  const r = validateEntries([{ id: 'S7', file: 'x.cpp', status: 'modeled' }]);
  assert.strictEqual(r.ok, false);
  assert.deepStrictEqual(r.missingPipelineWired, ['S7']);
});

test('pipelineWired가 boolean이 아니면(문자열 등) 실패', () => {
  const r = validateEntries([{ id: 'S7', file: 'x.cpp', status: 'modeled', pipelineWired: 'true' }]);
  assert.strictEqual(r.ok, false);
  assert.deepStrictEqual(r.missingPipelineWired, ['S7']);
});

test('pipelineWired:false인데 pipelineReason 없으면 실패', () => {
  const r = validateEntries([{ id: 'S7', file: 'x.cpp', status: 'modeled', pipelineWired: false }]);
  assert.strictEqual(r.ok, false);
  assert.deepStrictEqual(r.missingPipelineReason, ['S7']);
});

test('pipelineWired:false + pipelineReason 있으면 통과', () => {
  const r = validateEntries([
    { id: 'S7', file: 'x.cpp', status: 'modeled', pipelineWired: false, pipelineReason: '구현·검증됨, world 미배선' },
  ]);
  assert.strictEqual(r.ok, true);
  assert.deepStrictEqual(r.missingPipelineReason, []);
});

test('worldLedgerSrcIds: sim/world.js 의 post() srcId 를 실제로 뽑는다', () => {
  const repoRoot = path.join(__dirname, '..', '..', '..');
  const ids = worldLedgerSrcIds(repoRoot);
  assert.ok(ids.size > 0, 'world.js 에서 srcId 를 하나도 못 뽑았다 — 정규식이 코드 형태 변화를 못 따라감');
  for (const must of ['S1', 'S10', 'K1', 'K3']) {
    assert.ok(ids.has(must), `world.js 가 post 하는 ${must} 를 못 뽑음`);
  }
});

test('checkPipelineWired: 선언 true인데 world.js가 post 안 하면 falsePositive', () => {
  const repoRoot = path.join(__dirname, '..', '..', '..');
  const r = checkPipelineWired(repoRoot, [
    { id: 'S7', status: 'modeled', pipelineWired: true }, // world.js 에 post('...','S7') 없음
  ]);
  assert.deepStrictEqual(r.falsePositive, ['S7']);
});

test('checkPipelineWired: world.js가 post 하는데 false라 적으면 falseNegative', () => {
  const repoRoot = path.join(__dirname, '..', '..', '..');
  const r = checkPipelineWired(repoRoot, [
    { id: 'S1', status: 'modeled', pipelineWired: false, pipelineReason: 'x' },
  ]);
  assert.deepStrictEqual(r.falseNegative, ['S1']);
});

test('실 coverage.json: modeled 전건 pipelineWired 선언 + world.js post() 실호출과 100% 일치', () => {
  const repoRoot = path.join(__dirname, '..', '..', '..');
  const coveragePath = path.join(__dirname, '..', 'verify', 'coverage.json');
  const r = checkCoverage(repoRoot, coveragePath);
  assert.deepStrictEqual(r.missingPipelineWired, []);
  assert.deepStrictEqual(r.missingPipelineReason, []);
  assert.deepStrictEqual(r.pipeline.falsePositive, [], 'pipelineWired:true 인데 world.js 가 post 하지 않는 id');
  assert.deepStrictEqual(r.pipeline.falseNegative, [], 'world.js 가 post 하는데 pipelineWired:false 인 id');
});

test('실 coverage.json: modeled 총계 > 파이프라인 배선 수 (라벨 부풀림이 다시 숨지 않게 수치로 고정)', () => {
  const cov = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'verify', 'coverage.json'), 'utf8'));
  const modeled = cov.entries.filter(e => e.status === 'modeled');
  const wired = modeled.filter(e => e.pipelineWired === true);
  assert.ok(modeled.length > wired.length,
    `modeled(${modeled.length}) 가 배선(${wired.length}) 과 같아졌다 — 실제로 전건 배선됐다면 이 테스트를 갱신할 것`);
  // 배선된 것들은 화폐 흐름 id 여야 한다(확률 R* / 아이템 Item* 는 원장 계상 대상이 아니다).
  for (const e of wired) {
    assert.ok(/^(S|K|DS|DK)\d/.test(e.id), `${e.id} 가 pipelineWired:true 인데 화폐 흐름 id 형태가 아니다`);
  }
});

test('scanExempt인데 scanExemptReason 없으면 실패, id가 scanExemptMissingReason에 담김', () => {
  const r = validateEntries([{ id: 'ITEM-DUST-S1', file: 'x.cpp', status: 'stubbed', reason: 'ok', scanExempt: true }]);
  assert.strictEqual(r.ok, false);
  assert.deepStrictEqual(r.scanExemptMissingReason, ['ITEM-DUST-S1']);
});

test('scanExempt + scanExemptReason 있으면 통과', () => {
  const r = validateEntries([{ id: 'ITEM-DUST-S1', file: 'x.cpp', status: 'stubbed', reason: 'ok', scanExempt: true, scanExemptReason: '필드 직접 증감' }]);
  assert.strictEqual(r.ok, true);
  assert.deepStrictEqual(r.scanExemptMissingReason, []);
});

test('matchSites: file suffix 일치하면 매칭 (하위 경로 접두사 무시)', () => {
  const sites = [{ file: 'Private/Manager/ProjectOperationManager.cpp', line: 357, api: 'StoreResource' }];
  const entries = [{ id: 'S1', file: 'Private/Manager/ProjectOperationManager.cpp', status: 'modeled' }];
  const r = matchSites(sites, entries);
  assert.strictEqual(r.unmatched.length, 0);
});

// scanSites 전용 fixture — 정의부/주석/ListView 오탐(TListView<T>::AddItem 이름 충돌) 필터 검증
function withFixture(fn) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'covtest-'));
  const srcDir = path.join(tmp, 'Source', 'CompanyGrowthRenewal', 'Private', 'Manager');
  fs.mkdirSync(srcDir, { recursive: true });
  fs.writeFileSync(path.join(srcDir, 'Fixture.cpp'), [
    'void UFixtureManager::StoreResource(EResourceType Type, int64 Amount, bool bShouldSave)', // 정의부 — 스킵
    '{',
    '    // ResourceMgr->StoreResource(EResourceType::Money, 100); 주석 — 스킵',
    '    ResourceMgr->StoreResource(EResourceType::Money, Amount, false);', // 실호출 — 수집
    '    ResourceMgr->SpendResource(EResourceType::Diamond, Cost);', // 실호출 — 수집
    '    ItemMgr->AddItem(EItemType::Foo, 1);', // 실호출 — 수집
    '    FooListView->AddItem(CardData);', // ListView 오탐 — 스킵
    '}',
  ].join('\n'));
  try {
    return fn(tmp);
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}

test('scanSites: 정의부/주석/ListView 오탐 제외, 실 API 호출만 수집', () => {
  withFixture((tmp) => {
    const sites = scanSites(tmp);
    assert.strictEqual(sites.length, 3);
    assert.deepStrictEqual(sites.map(s => s.api).sort(), ['AddItem', 'SpendResource', 'StoreResource']);
    assert.ok(sites.every(s => s.file === 'Private/Manager/Fixture.cpp'));
  });
});

test('checkCoverage: 실 리포지토리 스캔 결과가 coverage.json 매트릭스와 100% 일치', () => {
  const repoRoot = path.join(__dirname, '..', '..', '..');
  const coveragePath = path.join(__dirname, '..', 'verify', 'coverage.json');
  const r = checkCoverage(repoRoot, coveragePath);
  assert.strictEqual(r.ok, true, `unmatched=${JSON.stringify(r.unmatched)} missingReason=${JSON.stringify(r.missingReason)} scanExemptMissingReason=${JSON.stringify(r.scanExemptMissingReason)}`);
});

test('checkCoverage: scanExempt 문서화 예외(DismantleDust/가챠 마일리지)가 조용히 빠지지 않고 exempt 목록에 노출된다', () => {
  const repoRoot = path.join(__dirname, '..', '..', '..');
  const coveragePath = path.join(__dirname, '..', 'verify', 'coverage.json');
  const r = checkCoverage(repoRoot, coveragePath);
  const exemptIds = r.exempt.map(e => e.id).sort();
  // Task9 fix — GACHA-MILEAGE-S1/K1(마일리지 적립/교환, DismantleDust와 동일 계열의 게이트웨이 우회 풀) 추가.
  // Task13 — 상점 Mileage 결제(전용 API)와 스킨/특성의 별도 마일리지 풀 4종을 추가 등재.
  assert.deepStrictEqual(exemptIds, [
    'GACHA-MILEAGE-K1', 'GACHA-MILEAGE-S1', 'ITEM-DUST-K1', 'ITEM-DUST-K2', 'ITEM-DUST-S1',
    'SHOP-MILEAGE-K1', 'SKIN-MILEAGE-K1', 'SKIN-MILEAGE-S1', 'TRAIT-MILEAGE-K1', 'TRAIT-MILEAGE-S1',
  ]);
  assert.ok(r.exempt.every(e => typeof e.reason === 'string' && e.reason.length > 0));
});
