// 서브 경제(무역/도시인수/모뉴먼트/벽돌공장/채광/가챠/Diamond예산/MarketCap배율) 검증.
// SOT: specs/2026-08-02-economy-map.md §2(무역)/§4(Diamond)/§5~§8(Brick/MarketCap/원자재/아이템).
// ⚠ S8/S9(도시 인수 드립)는 동시 세션 커밋 144e42fc가 "지갑 직접지급 → 금고(팟) 적립"으로 바꿔
//   economy-map 원문과 실코드가 어긋난 상태였다 — 이 파일은 CityAcquisitionManager.cpp 신동작 기준으로 작성.
const { test } = require('node:test');
const assert = require('node:assert');
const {
  tradeSale, cityAcquisitionEV, monumentPassivePerSec, brickFactoryPerHour,
  mineYield, gachaPull, mileageExchange, diamondBudget, marketCapMultiplier,
} = require('../sim/subecon.js');
const { makeRng } = require('../sim/rng.js');
const { fakeValues } = require('./helpers/fake_values.js');

function approx(actual, expected, eps = 1e-6) {
  assert.ok(Math.abs(actual - expected) < eps, `${actual} !== ${expected} (±${eps})`);
}

// ── 브리프 Step 1 기대값 테스트 (4건, 문구 그대로) ──────────────────────────

test('도시 인수 EV = 원금 × 175% (균등 [50,300])', () => {
  const V = fakeValues();
  const ev = cityAcquisitionEV(V, { AcquisitionCost: 10000, YieldMinPct: 50, YieldMaxPct: 300, DripChance: 0.5, DripChunkPct: 5 });
  assert.ok(Math.abs(ev.evReturn - 17500) < 1);
});

test('MarketCap 배율: clamp(BaseMul + Coef×log10(1+MC/Pivot), BaseMul, MaxMul)', () => {
  const V = fakeValues();
  assert.ok(Math.abs(marketCapMultiplier(V, 0) - 0.02) < 1e-9);       // 하한
  assert.strictEqual(marketCapMultiplier(V, 1e12), 3.0);               // 포화
});

test('가챠 하드피티 60: 59연속 꽝이어도 60번째 최고등급', () => {
  const V = fakeValues();
  let state = { pity: 59 };
  const r = gachaPull(makeRng(1), state, V);
  assert.strictEqual(r.rarity, 'Highest');
});

test('Diamond 예산: 코덱스 = 20×10 + 200 = 400', () => {
  assert.strictEqual(diamondBudget(fakeValues()).codex, 400);
});

// ── 신동작(144e42fc) EV 테스트 — 도시 인수 드립 = 지갑 직접지급 폐지, 팟 적립+크리트 틱 ──

test('도시 인수 신동작: 크리트 틱이 있으면 소진 소요시간이 짧아지고, EV 총액(evReturn)은 불변', () => {
  const V = fakeValues();
  const base = { AcquisitionCost: 10000, YieldMinPct: 50, YieldMaxPct: 300, DripChance: 0.5, DripChunkPct: 5 };
  const noCrit = cityAcquisitionEV(V, { ...base, CritChance: 0, CritMult: 1 });
  const withCrit = cityAcquisitionEV(V, { ...base, CritChance: 0.12, CritMult: 4.0 });
  assert.ok(withCrit.evDurationSec < noCrit.evDurationSec);
  approx(withCrit.evReturn, noCrit.evReturn);
});

test('도시 인수 신동작: CritChance/CritMult 미지정 시 DT CSV에 컬럼이 없어 구조체 기본값(0.12/4.0)으로 폴백', () => {
  const V = fakeValues();
  const base = { AcquisitionCost: 10000, YieldMinPct: 50, YieldMaxPct: 300, DripChance: 0.5, DripChunkPct: 5 };
  const implicit = cityAcquisitionEV(V, base); // CritChance/CritMult 필드 없음 — companyRow가 실 DT_CityCompany CSV 그대로인 케이스
  const explicit = cityAcquisitionEV(V, { ...base, CritChance: V.get('city.critChanceDefault'), CritMult: V.get('city.critMultDefault') });
  approx(implicit.evDurationSec, explicit.evDurationSec);
});

// ⚠ 이 테스트명은 2026-08-02 판에서 "Demolish 에도 캐시아웃이 없다"였는데 그건 오탐이었다(리포트 §5-B').
// walletIncome=0 의 진짜 근거는 "드립(Tick/SettleOffline)이 팟에만 적립한다" 이지, 캐시아웃 부재가 아니다.
test('도시 인수: walletIncome=0 — 드립(Tick/SettleOffline)은 팟 적립만이고 지갑 유입은 Demolish 캐시아웃 시점 1회', () => {
  const V = fakeValues();
  const ev = cityAcquisitionEV(V, { AcquisitionCost: 10000, YieldMinPct: 50, YieldMaxPct: 300, DripChance: 0.5, DripChunkPct: 5 });
  assert.strictEqual(ev.walletIncome, 0);
});

// ── 무역 판매 (economy-map §2 "무역 판매 공식") ──────────────────────────

test('무역 판매(허브+주문매칭): Money/MarketCap/Diamond 전 배율 체인', () => {
  const V = fakeValues();
  const recipeRow = { ProjectIndex: 5, BaseMarketCap: 500, CompanyType: 'Semiconductor', Grade: 'S' };
  const country = { Type: 'USA', DemandRatio: 1.0 };
  const order = { RewardMultiplier: 1.5, DiamondBonus: 40, RemainingQuantity: 60 };
  const r = tradeSale(V, makeRng(1), recipeRow, 60, country, order);

  const basePrice = 100 + 50 * 5 * 5; // GetBasePrice(TradePort.cpp:18-24) = 1350
  const expectedMoney = Math.round(basePrice * 60 * 1.35 * 1.25 * 1.5 * 1 * 1.2 * 1.35);
  const expectedMarketCap = Math.round(500 * 60 * 2.0 * 1.5 * 1 * 1.2);
  assert.strictEqual(r.money, expectedMoney);
  assert.strictEqual(r.marketCap, expectedMarketCap);
  assert.strictEqual(r.diamond, 40); // 주문 완전충족(qty>=RemainingQuantity)
});

test('무역 판매(비허브+주문없음): PortPriceMul/BulkMul 무효화, Diamond 없음', () => {
  const V = fakeValues();
  const recipeRow = { ProjectIndex: 1, BaseMarketCap: 10, CompanyType: 'Semiconductor' };
  const country = { Type: 'Korea', DemandRatio: 0.5 };
  const r = tradeSale(V, makeRng(1), recipeRow, 5, country, null);

  const expectedMoney = Math.round(150 * 5 * 1 * 1 * 1 * 1 * 0.8 * 1.3);
  const expectedMarketCap = Math.round(10 * 5 * 1 * 1 * 1.3 * 0.8);
  assert.strictEqual(r.money, expectedMoney);
  assert.strictEqual(r.marketCap, expectedMarketCap);
  assert.strictEqual(r.diamond, 0);
});

// ── 모뉴먼트 패시브 ────────────────────────────────────────────────────

test('모뉴먼트 패시브: BasePassiveOutput + (Lv-1)×PassivePerLevel 닫힌식(선형)', () => {
  const V = fakeValues();
  approx(monumentPassivePerSec(V, 1), V.get('monument.basePassiveOutput'));
  approx(monumentPassivePerSec(V, 5), V.get('monument.basePassiveOutput') + 4 * V.get('monument.passivePerLevel'));
  // Lv0 이하는 Lv1 취급(BuildingBaseActor.cpp:284 호출측이 항상 Level>=1을 넘긴다는 전제)
  approx(monumentPassivePerSec(V, 0), monumentPassivePerSec(V, 1));
});

// ── 벽돌공장 (FFactoryUpgradeConfig 구간별 닫힌식) ────────────────────────

test('벽돌공장 시간당 생산: Lv1 기준(10초당 1개) = 360/시간', () => {
  const V = fakeValues();
  const r = brickFactoryPerHour(V, { holdAmountLv: 1, autoCollectionLv: 1, autoCollectionCapacityLv: 1 });
  approx(r.perHour, 360);
  assert.strictEqual(r.capacityPerCycle, 1);
});

test('벽돌공장 시간당 생산: 레벨 상승 시 처리량 증가(홀드생산량↑ + 자동생산 간격↓)', () => {
  const V = fakeValues();
  const r = brickFactoryPerHour(V, { holdAmountLv: 50, autoCollectionLv: 91, autoCollectionCapacityLv: 200 });
  // AutoCollection Lv91 = 10-((91-1)*0.1) = 1.0초, HoldProductionAmount Lv50 = 50개
  approx(r.perHour, (50 / 1.0) * 3600);
  assert.strictEqual(r.capacityPerCycle, 200);
});

test('벽돌공장: 자동생산 간격은 MinAutoCollectionInterval(0.25s) 밑으로 못 내려간다', () => {
  const V = fakeValues();
  const r = brickFactoryPerHour(V, { holdAmountLv: 1, autoCollectionLv: 1000, autoCollectionCapacityLv: 1 });
  // AutoCollection Lv1000 값 자체는 0.001초까지 내려가지만 MinAutoCollectionInterval(0.25)에 하한 클램프
  approx(r.perHour, (1 / 0.25) * 3600);
});

// ── 채광 (MineManager wall-clock lazy 모델) ───────────────────────────

test('채광: 주력(index0)이 보조 자원의 2배 속도, Lv0 기준 30초 경과', () => {
  const V = fakeValues();
  const primary = mineYield(V, 'primary', 30, { miningRateLv: 0, miningStorageLv: 0 });
  const secondary = mineYield(V, 'secondary', 30, { miningRateLv: 0, miningStorageLv: 0 });
  assert.strictEqual(primary, 1);   // 30 × (2/60) = 1.0
  assert.strictEqual(secondary, 0); // 30 × (1/60) = 0.5 → floor 0
});

test('채광: MiningRate 강화 레벨이 속도를 선형 가산 배율로 올린다', () => {
  const V = fakeValues();
  const boosted = mineYield(V, 'primary', 30, { miningRateLv: 10, miningStorageLv: 0 });
  assert.strictEqual(boosted, 2); // rateMul = 1+0.15×10 = 2.5 → 30×(2/60)×2.5 = 2.5 → floor 2
});

test('채광: 저장 한도(MiningStorage)를 넘으면 클램프된다', () => {
  const V = fakeValues();
  const capped = mineYield(V, 'primary', 1e6, { miningRateLv: 0, miningStorageLv: 0 });
  assert.strictEqual(capped, 100); // BaseValue(100) + PerLevel×0
});

// ── 가챠(소프트/하드 피티 + 마일리지) ─────────────────────────────────

test('가챠: 소프트피티(40회+) 구간에선 보정 없는 롤보다 최고등급 확률이 높아진다', () => {
  const V = fakeValues();
  // 결정론적 rng: 항상 같은 [0,1) 값을 굴려 소프트피티 보정 유무만으로 결과가 갈리는지 확인.
  // Advanced 무보정 누적확률(Common~Epic)=0.98 → roll=0.90은 무보정이면 Epic(0.85~0.98 구간)에 떨어져 Highest 미당첨,
  // 피티 50(=pullCount 51 > SoftPityStart 40) 보정이 Highest 슬롯을 넓혀 같은 roll이 Highest로 당첨된다.
  const roll = { next: () => 0.90 };
  const noBoost = gachaPull(roll, { tier: 'Advanced', pity: 0 }, V);     // pity+1=1 ≤ SoftPityStart(40)
  const boosted = gachaPull(roll, { tier: 'Advanced', pity: 50 }, V);    // pity+1=51 > 40 → 보정 활성
  assert.notStrictEqual(noBoost.rarity, 'Highest');
  assert.strictEqual(boosted.rarity, 'Highest');
});

test('가챠: Highest 당첨/하드피티 시 pity 리셋, 그 외엔 pity 누적', () => {
  const V = fakeValues();
  const hard = gachaPull(makeRng(1), { tier: 'Premium', pity: 59 }, V);
  assert.strictEqual(hard.newState.pity, 0);
  const miss = gachaPull({ next: () => 0.0 }, { tier: 'Advanced', pity: 3 }, V); // roll=0 → 항상 테이블 첫 항목(Common)
  assert.strictEqual(miss.rarity, 'Common');
  assert.strictEqual(miss.newState.pity, 4);
});

test('가챠: Normal 티어는 천장이 없다(피티 무관 Highest 불가) — HR파워 0이면 Common 고정', () => {
  const V = fakeValues();
  const r = gachaPull(makeRng(7), { tier: 'Normal', pity: 999, hrPower: 0 }, V);
  assert.strictEqual(r.rarity, 'Common');
  assert.strictEqual(r.diamondCost, 0); // Normal은 Diamond 폴백 없음(티켓 전용)
});

test('가챠: Premium 1뽑당 마일리지 +1 누적, 티켓 보유 시 Diamond 비용 0', () => {
  const V = fakeValues();
  const r1 = gachaPull(makeRng(2), { tier: 'Premium', pity: 0, mileage: 0 }, V);
  assert.strictEqual(r1.newState.mileage, 1);
  assert.strictEqual(r1.diamondCost, V.get('gacha.premiumDiamondCost'));
  const r2 = gachaPull(makeRng(2), { tier: 'Premium', pity: 0, mileage: 0, hasTicket: true }, V);
  assert.strictEqual(r2.diamondCost, 0);
});

// ── 마일리지 교환 (ExchangeMileage — 리뷰 지적: "교환 200" 미모델링 fix) ─────────────

test('마일리지 교환: 200 미만이면 교환 불가, 상태 불변', () => {
  const V = fakeValues();
  const r = mileageExchange({ mileage: 150, pity: 12 }, V);
  assert.strictEqual(r.canExchange, false);
  assert.strictEqual(r.rarity, null);
  assert.strictEqual(r.newState.mileage, 150);
});

test('마일리지 교환: 200 도달 시 교환 성공 — 200 차감 + Legendary 확정, Pity는 전혀 안 건드림', () => {
  const V = fakeValues();
  const r = mileageExchange({ mileage: 200, pity: 37 }, V);
  assert.strictEqual(r.canExchange, true);
  assert.strictEqual(r.rarity, 'Highest'); // ExchangeMileage는 확률표 없이 Legendary 직행 — gachaPull과 라벨 통일
  assert.strictEqual(r.newState.mileage, 0);
  assert.strictEqual(r.newState.pity, 37); // AdvancedPity/PremiumPity 어느 쪽도 미접촉(IncrementPull/ResetPity 없음)
});

test('마일리지 교환: 초과분은 이월(200 고정 차감, 잔여 소각 아님)', () => {
  const V = fakeValues();
  const r = mileageExchange({ mileage: 250 }, V);
  assert.strictEqual(r.canExchange, true);
  assert.strictEqual(r.newState.mileage, 50);
});

// ── Diamond 예산(코덱스/VIP주문/미션) ─────────────────────────────────

test('Diamond 예산: VIP 주문 1건 기대값 = (Min+Max)/2', () => {
  const V = fakeValues();
  const b = diamondBudget(V);
  approx(b.vipPerOrderEV, 55); // (30+80)/2
});

test('Diamond 예산: 온보딩 미션 체인(DT_Mission) 전체 Diamond 보상 합계', () => {
  const V = fakeValues();
  const b = diamondBudget(V);
  assert.strictEqual(b.missionTotal, 450); // M9(100)+M10(50)+M11(50)+M12(50)+M13(100)+M21(100)
});
