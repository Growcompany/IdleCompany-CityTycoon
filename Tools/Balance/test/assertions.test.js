const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { loadValues } = require('../sim/values.js');
const { runWorld } = require('../sim/world.js');
const {
  runAssertions, hasBlockingFailure, ASSERTION_IDS,
  assertOffsetPair, assertDiamondBudget, enclosingBlocks, assertMarketCapSaturation,
} = require('../verify/assertions.js');

const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const REAL_VALUES = path.join(__dirname, '..', 'out', 'values.json');

// 순수 계산형 어서션(A-OFFSET/A-DIAMOND)용 최소 fake values — loadValues 인터페이스를 그대로 태운다.
function fakeValues(overrides = {}) {
  const base = {
    'emp.outputUnit': 5.2,
    'emp.idleIncomeScale': 1.875,
    'emp.levelFactorCurve': 0.1,
    'emp.wanderIncomeMult': 0.5,
    'idle.baseIncomePerSecond': 1,
    'diamond.codexTier': 20,
    'diamond.codexFull': 200,
    'diamond.slotCosts': [30, 60, 120, 240, 480],
    'launch.retryDiamondCost': 50,
    'emp.disciplineResetDiamond': 50,
    'gacha.advancedDiamondCost': 30,
    'gacha.premiumDiamondCost': 50,
    'vault.baseCapacity': 200,
  };
  const values = {};
  for (const [k, v] of Object.entries({ ...base, ...overrides.values })) values[k] = { v, src: 'fake', loc: 'fake' };
  const raw = {
    meta: { srcHash: 'fake', editorHash: 'fake' },
    values,
    dt: {
      DT_TradeOrderBalance: [{ Name: 'Default', VIP_DiamondMin: 30, VIP_DiamondMax: 80 }],
      DT_Mission: [
        { Name: 'M1', Rewards: ['(ResourceType=Diamond,Amount=100,ItemType=None,ItemAmount=0)'] },
        { Name: 'M2', Rewards: ['(ResourceType=Diamond,Amount=350,ItemType=None,ItemAmount=0)'] },
      ],
      ...overrides.dt,
    },
  };
  const p = path.join(os.tmpdir(), `balance_assert_fixture_${process.pid}_${Math.random().toString(36).slice(2)}.json`);
  fs.writeFileSync(p, JSON.stringify(raw));
  try { return loadValues(p); } finally { fs.unlinkSync(p); }
}

// ── A-OFFSET (순수 계산) ────────────────────────────────────────────────
test('A-OFFSET: 상쇄쌍 곱이 기대값이면 pass', () => {
  const r = assertOffsetPair(fakeValues());
  assert.strictEqual(r.pass, true, r.detail);
  assert.strictEqual(r.data.product, 9.75);
});

test('A-OFFSET: 한쪽만 리튠되어 곱이 깨지면 fail', () => {
  const r = assertOffsetPair(fakeValues({ values: { 'emp.outputUnit': 6.0 } }));
  assert.strictEqual(r.pass, false);
  assert.match(r.detail, /9\.75/);
});

test('A-OFFSET: 상쇄쌍이 실제 방치수익 공식에 도달하는지도 검사(한쪽이 공식에서 빠지면 fail)', () => {
  // idlePerSecond(Lv1) / (base × wander) = outputUnit × idleIncomeScale 이어야 한다.
  const r = assertOffsetPair(fakeValues());
  assert.strictEqual(r.data.idleFormulaProduct, 9.75);
  assert.strictEqual(r.data.reachesIdleFormula, true);
});

// ── A-DIAMOND (순수 계산) ───────────────────────────────────────────────
test('A-DIAMOND: 확정유입 적자여도 반복유입(VIP)이 있으면 pass + 적자폭/필요 주문수 산출', () => {
  const r = assertDiamondBudget(fakeValues(), null);
  // codex 10×20+200 = 400, 미션 450 → 확정유입 850 / 슬롯 30+60+120+240+480 = 930
  assert.strictEqual(r.data.codex, 400);
  assert.strictEqual(r.data.missionTotal, 450);
  assert.strictEqual(r.data.fixedInflow, 850);
  assert.strictEqual(r.data.slotTotal, 930);
  assert.strictEqual(r.data.fixedNet, -80);
  assert.strictEqual(r.data.vipPerOrderEV, 55);
  assert.strictEqual(r.data.vipOrdersToCoverDeficit, 2);
  assert.strictEqual(r.pass, true, r.detail);
});

test('A-DIAMOND: 확정유입 적자 + 반복유입 EV 0 = 소프트락 → fail', () => {
  const v = fakeValues({ dt: { DT_TradeOrderBalance: [{ Name: 'Default', VIP_DiamondMin: 0, VIP_DiamondMax: 0 }] } });
  const r = assertDiamondBudget(v, null);
  assert.strictEqual(r.pass, false);
  assert.match(r.detail, /소프트락/);
});

test('A-DIAMOND: 확정유입이 필수 유출을 덮으면 반복유입과 무관하게 pass', () => {
  const v = fakeValues({
    values: { 'diamond.codexFull': 2000 },
    dt: { DT_TradeOrderBalance: [{ Name: 'Default', VIP_DiamondMin: 0, VIP_DiamondMax: 0 }] },
  });
  const r = assertDiamondBudget(v, null);
  assert.strictEqual(r.pass, true, r.detail);
  assert.ok(r.data.fixedNet > 0);
});

test('A-DIAMOND: 시뮬 실측 유입을 넘기면 이론치와의 차이를 detail 에 명시', () => {
  const r = assertDiamondBudget(fakeValues(), { total: 450, bySrc: { DS4: 450 }, runs: 20 });
  assert.match(r.detail, /DS1|DS2/);
  assert.strictEqual(r.data.simInflowTotal, 450);
});

// ── C++ 스코프 파서 (grep형 어서션의 토대) ──────────────────────────────
test('enclosingBlocks: 중첩 if 조건을 순서대로 되돌린다', () => {
  const src = [
    'void Foo()',
    '{',
    '    if (Mode == EMode::Stage)',
    '    {',
    '        Pay();',
    '    }',
    '    Other();',
    '}',
  ];
  const inner = enclosingBlocks(src, 4).map(b => b.text);
  assert.strictEqual(inner.length, 2);
  assert.match(inner[1], /Mode == EMode::Stage/);
  const outer = enclosingBlocks(src, 6).map(b => b.text);
  assert.strictEqual(outer.length, 1); // 함수 본문만
});

test('enclosingBlocks: 문자열 리터럴 안의 중괄호는 스코프로 세지 않는다', () => {
  const src = [
    'void Foo()',
    '{',
    '    UE_LOG(LogTemp, Log, TEXT("{0} }} {"));',
    '    Pay();',
    '}',
  ];
  assert.strictEqual(enclosingBlocks(src, 3).length, 1);
});

// ── 양성 대조(positive control) — 어서션이 실제로 판정하는지 합성 소스로 검증 ──
function withFakeRepo(files, fn) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'assert-repo-'));
  try {
    for (const [rel, text] of Object.entries(files)) {
      const p = path.join(tmp, rel);
      fs.mkdirSync(path.dirname(p), { recursive: true });
      fs.writeFileSync(p, text);
    }
    return fn(tmp);
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}

const BEHAVIOR_REL = 'Source/CompanyGrowthRenewal/Private/Entity/Officeworker/EmployeeBehaviorComponent.cpp';
const OPERATION_REL = 'Source/CompanyGrowthRenewal/Private/Manager/ProjectOperationManager.cpp';

test('A-DOUBLEPAY: Stage 분기 안에 지급이 되살아나면 fail (양성 대조)', () => {
  const src = [
    'void UEmployeeBehaviorComponent::GenerateIncome(float DeltaTime)',
    '{',
    '    if (CurrentBehaviorMode != EEmployeeBehaviorMode::Stage)',
    '    {',
    '        AccumulatedIncome += IncomeThisFrame;',
    '    }',
    '    if (CurrentBehaviorMode == EEmployeeBehaviorMode::Stage)',
    '    {',
    '        ResourceMgr->StoreResource(EResourceType::Money, Payout, false);',
    '    }',
    '}',
  ].join('\n');
  const { assertNoStagePayout } = require('../verify/assertions.js');
  const r = withFakeRepo({ [BEHAVIOR_REL]: src }, repo => assertNoStagePayout(repo));
  assert.strictEqual(r.pass, false);
  assert.deepStrictEqual(r.data.stagePayouts, [9]);
});

// 무운영 분기가 상수 하한을 그대로 돌려주는 현행(결함) 형태.
const VAULT_BROKEN = [
  'float UProjectOperationManager::CalculateWarehouseCapacity(int32 BuildingID) const',
  '{',
  '    const float FloorCapacity = ABuildingBaseActor::BaseVaultCapacity;',
  '    if (!bFoundBuilding)',
  '    {',
  '        return ABuildingBaseActor::BaseVaultCapacity;  // 빌딩 미발견 — 이건 별개의 정상 가드',
  '    }',
  '    const float StableRate = GetVaultStableRatePerSecond(BuildingID);',
  '    if (StableRate <= 0.0f)',
  '    {',
  '        return FloorCapacity;',
  '    }',
  '    return StableRate * UBuildingEnhancementHelper::CalculateVaultSeconds(VaultLevel);',
  '}',
].join('\n');

test('A-VAULTFALLBACK: 분기 본문이 영속 레이트 기반 용량을 반환하면 pass 로 뒤집힌다 (3상태 검증)', () => {
  const { assertVaultFallback } = require('../verify/assertions.js');
  const values = fakeValues();
  // 진짜 수정 = 폴백 "분기 본문"이 영속 레이트로 용량을 계산해 반환한다(상수 반환 제거).
  const fixed = VAULT_BROKEN.replace(
    '        return FloorCapacity;',
    [
      '        const float PersistedRate = SaveData->GameData.Buildings[Idx].LastOperationBaseRatePerSecond;',
      '        return PersistedRate * UBuildingEnhancementHelper::CalculateVaultSeconds(VaultLevel);',
    ].join('\n')
  );
  const a = withFakeRepo({ [OPERATION_REL]: VAULT_BROKEN }, repo => assertVaultFallback(repo, values));
  const b = withFakeRepo({ [OPERATION_REL]: fixed }, repo => assertVaultFallback(repo, values));
  assert.strictEqual(a.pass, false, '미수정본이 pass');
  assert.strictEqual(a.data.returnsConstantFloor, true);
  assert.strictEqual(b.pass, true, `수정본이 fail: ${b.detail}`);
  assert.strictEqual(b.data.usesPersistedRate, true);
});

test('A-VAULTFALLBACK: 주석만 단 가짜 수정본은 여전히 fail (음성 케이스)', () => {
  const { assertVaultFallback } = require('../verify/assertions.js');
  const values = fakeValues();
  for (const [label, fake] of [
    ['줄주석', VAULT_BROKEN.replace('        return FloorCapacity;',
      '        // TODO: LastOperationBaseRatePerSecond 를 영속화해서 쓸 것\n        return FloorCapacity;')],
    ['블록주석', VAULT_BROKEN.replace('        return FloorCapacity;',
      '        /* PersistedRatePerSecond 도입 예정\n           LastOperationRatePerSecond */\n        return FloorCapacity;')],
  ]) {
    const r = withFakeRepo({ [OPERATION_REL]: fake }, repo => assertVaultFallback(repo, values));
    assert.strictEqual(r.pass, false, `${label} 주석만으로 pass 로 뒤집힘`);
    assert.strictEqual(r.data.usesPersistedRate, false, `${label}: 주석 속 식별자를 코드 참조로 오독`);
  }
});

test('A-VAULTFALLBACK: 무운영 분기를 못 찾으면 조용히 통과하지 않는다', () => {
  const { assertVaultFallback } = require('../verify/assertions.js');
  const values = fakeValues();
  const restructured = [
    'float UProjectOperationManager::CalculateWarehouseCapacity(int32 BuildingID) const',
    '{',
    '    return GetSomethingElse(BuildingID);',
    '}',
  ].join('\n');
  const r = withFakeRepo({ [OPERATION_REL]: restructured }, repo => assertVaultFallback(repo, values));
  assert.strictEqual(r.pass, false);
  assert.match(r.detail, /찾지 못함/);
  assert.strictEqual(r.data.guardFound, false);
});

// "결함 잔존(KNOWN)" 과 "판정 불가(FAIL)" 와 "혼합 반환(REVIEW)" 이 한 상태로 뭉개지면
// 구조 변경이 KNOWN 으로 흡수돼 조용해지고, 정상 수정이 "결함 잔존"으로 오보된다.
test('A-VAULTFALLBACK 상태 분리: 판정 불가 = 차단, 혼합 반환 = REVIEW', () => {
  const { assertVaultFallback, ASSERTIONS } = require('../verify/assertions.js');
  const values = fakeValues();
  // 정의에 실린 진짜 classify 를 태운다(테스트가 로직 복사본을 검사하지 않게).
  const { classify } = ASSERTIONS.find(a => a.id === 'A-VAULTFALLBACK');
  // 혼합 반환 = 영속 레이트를 쓰면서 상수를 하한(Max)으로만 남긴 형태.
  const mixed = VAULT_BROKEN.replace(
    '        return FloorCapacity;',
    [
      '        const float PersistedCap = SaveData->LastOperationBaseRatePerSecond * 7200.0f;',
      '        return FMath::Max(PersistedCap, FloorCapacity);',
    ].join('\n')
  );
  const m = withFakeRepo({ [OPERATION_REL]: mixed }, repo => assertVaultFallback(repo, values));
  assert.strictEqual(m.pass, false);
  assert.strictEqual(m.data.mixedReturn, true, '혼합 반환을 감지하지 못함');
  assert.match(m.detail, /혼합 반환 — 수동 확인 필요/);
  assert.doesNotMatch(m.detail, /retune #15 잔존/, '정상 수정 가능성을 결함 잔존으로 단정');

  const broken = withFakeRepo({ [OPERATION_REL]: VAULT_BROKEN }, repo => assertVaultFallback(repo, values));
  const gone = withFakeRepo({ [OPERATION_REL]: 'float UProjectOperationManager::CalculateWarehouseCapacity(int32 B) const\n{\n    return 1.0f;\n}' },
    repo => assertVaultFallback(repo, values));
  assert.strictEqual(classify(broken), 'KNOWN', '결함 잔존은 KNOWN');
  assert.strictEqual(classify(m), 'REVIEW', '혼합 반환은 REVIEW');
  assert.strictEqual(classify(gone), 'FAIL', '판정 불가(가드 소실)는 차단 실패로 승격');

  // 차단 여부까지 잠근다 — 판정 불가만 exit 1.
  const asResult = (out, state) => ({ pass: out.pass, state, known: state === 'KNOWN' });
  assert.strictEqual(hasBlockingFailure([asResult(broken, 'KNOWN')]), false);
  assert.strictEqual(hasBlockingFailure([asResult(m, 'REVIEW')]), false);
  assert.strictEqual(hasBlockingFailure([asResult(gone, 'FAIL')]), true);
});

// ── 실 리포지토리 스모크 (grep/런 결합형) ───────────────────────────────
const SMOKE_SKIP = !fs.existsSync(REAL_VALUES);

test('runAssertions: 스펙 §7.2 전 항목 + 추가 2건이 결과 계약을 지킨다', { skip: SMOKE_SKIP }, () => {
  const r = runAssertions(REAL_VALUES, REPO_ROOT);
  assert.deepStrictEqual(r.results.map(x => x.id), ASSERTION_IDS);
  for (const x of r.results) {
    assert.strictEqual(typeof x.pass, 'boolean', `${x.id}.pass 비불리언`);
    assert.strictEqual(typeof x.known, 'boolean', `${x.id}.known 비불리언`);
    assert.ok(['PASS', 'KNOWN', 'REVIEW', 'FAIL'].includes(x.state), `${x.id}.state 미정의: ${x.state}`);
    assert.strictEqual(x.pass, x.state === 'PASS', `${x.id}: pass 와 state 불일치`);
    assert.ok(typeof x.detail === 'string' && x.detail.length > 0, `${x.id}.detail 비어 있음(조용한 통과 금지)`);
    assert.ok(typeof x.desc === 'string' && x.desc.length > 0);
    assert.ok(!(x.pass && x.known), `${x.id}: pass=true 인데 known 마킹`);
  }
});

// A-MCSAT 은 시총 배율 상한을 DT 행에서 읽는다. 행을 못 찾았을 때 배율 1 로 조용히 폴백하면
// "배율을 무시한 채 PASS" 가 나온다 — 그 경로가 실패로 잡히는지 두 갈래 모두 고정한다.
function mcsatValues(enhRows) {
  return {
    get: k => (k === 'marketCap.baseMul' ? 1 : 1),
    dt: name => {
      if (name === 'DT_MarketBalance') return [{ Name: 'Default', BaseMul: 1, MaxMul: 3, Coef: 0.5, PivotMC: 1000 }];
      if (name === 'DT_BuildingEnhancementDefinition') return enhRows;
      return [];
    },
  };
}
const MCSAT_RUNS = [{ tiers: [{ tier: 1, grades: { S: 1, A: 0, B: 0, C: 0, gateFail: 0 } }] }];

test('A-MCSAT: MarketCapMultiplier 행이 없으면 배율 1 폴백이 아니라 실패한다', () => {
  const r = assertMarketCapSaturation(MCSAT_RUNS, mcsatValues([]), REPO_ROOT);
  assert.strictEqual(r.pass, false);
  assert.match(r.detail, /MarketCapMultiplier/);
});

test('A-MCSAT: MarketCapMultiplier 행이 있으면 그 상한 배율이 실제로 상한 계산에 들어간다', () => {
  const row = { Name: 'MarketCapMultiplier', EnhancementType: 'MarketCapMultiplier', EffectPerLevel: 0.0005, MaxLevel: 5000 };
  const r = assertMarketCapSaturation(MCSAT_RUNS, mcsatValues([row]), REPO_ROOT);
  assert.strictEqual(r.data.mcMax, 1 + 5000 * 0.0005); // 가산형 그대로
  assert.ok(r.data.mcMax > 1, '배율이 1 폴백으로 뭉개졌다');
});

test('스모크: 현행 코드에서 grep형 어서션이 기대 결과를 낸다', { skip: SMOKE_SKIP }, () => {
  const byId = Object.fromEntries(runAssertions(REAL_VALUES, REPO_ROOT).results.map(x => [x.id, x]));
  for (const id of ['A-OFFSET', 'A-DOUBLEPAY', 'A-DFENUM', 'A-PRODGRADE', 'A-DIAMOND',
    'A-EXPGATE', 'A-MCSAT', 'A-UNWIRED', 'A-COVERAGE', 'A-MILEAGE-SHOP']) {
    assert.strictEqual(byId[id].pass, true, `${id} 실패: ${byId[id].detail}`);
  }
  // retune #15 미수정 상태 — 수정되면 이 테스트가 먼저 깨져서 "고쳤다"를 알려준다.
  const vf = byId['A-VAULTFALLBACK'];
  const why = `실코드가 바뀌었다 — 폴백 분기(StableRate <= 0)가 진짜 영속 레이트 기반 용량을 반환하는지 직접 확인한 뒤 기대치를 갱신할 것. detail=${vf.detail}`;
  assert.strictEqual(vf.pass, false, why);
  assert.strictEqual(vf.state, 'KNOWN', why);
  assert.strictEqual(vf.known, true);
  // "결함 잔존"과 "판정 불가"가 같은 상태로 뭉개지지 않게 — 둘 다 명시적으로 어서트한다.
  assert.strictEqual(vf.data.guardFound, true, `무운영 분기를 찾지 못했다(판정 불가) — KNOWN 으로 흡수되면 안 된다. ${why}`);
  assert.strictEqual(vf.data.returnsConstantFloor, true, why);
  assert.strictEqual(vf.data.mixedReturn, false, why);
});

test('알려진 결함(KNOWN)/수동확인(REVIEW)은 exit 1 을 유발하지 않고, 일반 실패는 유발한다', () => {
  assert.strictEqual(hasBlockingFailure([{ pass: false, state: 'KNOWN', known: true }]), false);
  assert.strictEqual(hasBlockingFailure([{ pass: false, state: 'REVIEW', known: false }]), false);
  assert.strictEqual(hasBlockingFailure([{ pass: true, state: 'PASS', known: false }]), false);
  assert.strictEqual(hasBlockingFailure([{ pass: false, state: 'FAIL', known: false }]), true);
  assert.strictEqual(hasBlockingFailure([
    { pass: false, state: 'KNOWN', known: true }, { pass: false, state: 'FAIL', known: false },
  ]), true);
});

test('grep형 어서션은 대상 파일이 없으면 조용히 통과하지 않는다', { skip: SMOKE_SKIP }, () => {
  const empty = fs.mkdtempSync(path.join(os.tmpdir(), 'assert-norepo-'));
  try {
    const r = runAssertions(REAL_VALUES, empty);
    const grepIds = ['A-DOUBLEPAY', 'A-VAULTFALLBACK', 'A-DFENUM', 'A-PRODGRADE', 'A-UNWIRED', 'A-COVERAGE'];
    for (const id of grepIds) {
      const x = r.results.find(e => e.id === id);
      assert.strictEqual(x.pass, false, `${id}: 소스 부재인데 pass`);
      assert.ok(/없|실패|불가|찾/.test(x.detail), `${id}: 원인 미기재 detail=${x.detail}`);
    }
  } finally {
    fs.rmSync(empty, { recursive: true, force: true });
  }
});

// A-EXPGATE 의 관측점 — 시뮬이 "7클리어 충족 + 레벨게이트 미충족" 을 세지 못하면 어서션이 무의미해진다.
test('world 런 결과에 expGateBlocks 관측 필드가 있다', { skip: SMOKE_SKIP }, () => {
  const r = runWorld({ seed: 1, profile: 'mid', valuesPath: REAL_VALUES, untilTier: 3, maxCalendarDays: 30 });
  assert.ok(r.tiers.length >= 1);
  for (const t of r.tiers) assert.ok(Number.isFinite(t.expGateBlocks), `T${t.tier} expGateBlocks 누락`);
});
