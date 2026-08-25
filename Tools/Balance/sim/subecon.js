// 서브 경제(무역/도시인수/모뉴먼트/벽돌공장/채광/가챠/Diamond예산/MarketCap배율) 닫힌식/EV 모델.
// SOT: specs/2026-08-02-economy-map.md §2(무역)/§4(Diamond)/§5~§8(Brick/MarketCap/원자재/아이템).
//
// ⚠ S8/S9(도시 인수 드립) — 2026-08-03 실코드 재확인판. 드립은 지갑이 아니라 "팟"에만 쌓인다:
//   Tick()/SettleOffline() 은 R.RRemaining 만 차감(StoreResource 호출 없음). 지갑 유입은
//   Demolish()(CityAcquisitionManager.cpp:224~225) 의 StoreResource(Money, Pot) **1곳뿐**이고,
//   파산(EAcqState::Busted) 상태면 Pot=0 으로 전액 소멸한다. 파산 판정은 Tick():196~206 에 배선돼 있다.
//   → 이 모델의 walletIncome 이 항상 0인 것은 "캐시아웃 미구현"이어서가 아니라 **드립 자체가 지갑 유입이 아니어서**다.
//   evReturn(팟 총액 EV)을 그대로 지갑 유입으로 합산하면 파산 소멸분과 미철거분을 과대계상한다.
//   (2026-08-02 Task9 판정 "캐시아웃 경로 미구현"은 리팩터 중간 스냅샷을 본 오탐 — 리포트 §5-B' 참조.)
function clamp(v, lo, hi) {
  return Math.min(hi, Math.max(lo, v));
}

// ── 도시 회사 인수 (CityAcquisitionManager.cpp) ──────────────────────────

// GetDripInfo(:100-111)/Tick(:147-176)/SettleOffline(:358-374) 이식.
// evReturn = 원금 × 균등[YieldMin,YieldMax] 기댓값(중간값) — GetEvPct()(CityCompanyData.h:93)와 동일 평균.
// evDurationSec = evReturn ÷ 초당 기대 드립(Chunk×DripChance×CritFactor) — 브리핑 지정 공식에 크리트 틱만 확장
// (CritFactor=1이면 브리핑 원 공식과 정확히 일치). 끝물에 Chunk가 RRemaining으로 클램프되는 효과(:160)는
// 무시한 EV 근사 — 브리핑 원 공식도 동일 근사를 취한다(그 이상 정밀화는 이 태스크 범위 밖).
function cityAcquisitionEV(values, companyRow) {
  const { AcquisitionCost, YieldMinPct, YieldMaxPct, DripChance, DripChunkPct } = companyRow;
  const evReturn = (AcquisitionCost * (YieldMinPct + YieldMaxPct)) / 2 / 100;
  const chunk = (AcquisitionCost * DripChunkPct) / 100;

  // DT_CityCompany CSV(DataImport/DT_CityCompany_Import.csv)엔 CritChance/CritMult 컬럼이 없다 —
  // companyRow가 실 DT 행을 그대로 옮긴 것이면 이 필드가 없고, 실서버는 FCityCompanyData 구조체 기본값
  // (CityCompanyData.h:74,77 = 0.12/4.0)을 그대로 쓴다. undefined를 0으로 죽이면 실제론 있는 12% 크리트를
  // 조용히 없는 셈 치게 되므로, 미지정 시 구조체 기본값으로 폴백한다(값은 city.critChanceDefault/critMultDefault
  // 로 추출 — extract_src manifest 2026-08-02 Task9 추가분).
  const critChance = companyRow.CritChance ?? values.get('city.critChanceDefault');
  const critMult = companyRow.CritMult ?? values.get('city.critMultDefault');
  const critFactor = 1 + critChance * (critMult - 1);

  const expectedPerSec = chunk * DripChance * critFactor;
  const evDurationSec = expectedPerSec > 0 ? evReturn / expectedPerSec : Infinity;

  // 신동작 실측 — 위 파일 헤더 주석 참고. Task10 등 후속 소비 시 evReturn을 "지갑 유입 Money"로 합산 금지.
  return { evReturn, evDurationSec, walletIncome: 0 };
}

// ── 무역 판매 (TradePort.cpp) ────────────────────────────────────────────

// GetPortPriceMultiplier(:26-36)/GetBulkBonus(:48-54)/QualityGrade.h:97-109 GetQualityGradeRevenueMultiplier
// 이식 — 전부 WorldMapConstants/switch 하드코딩(DT 아님, 구조 상수)이라 문자 그대로 옮긴다.
const USA_PRICE_BONUS = 1.35; // WorldMapConstants::USAPriceBonus, WorldMapTypes.h:294
function portPriceMultiplier(countryType) {
  return countryType === 'USA' ? USA_PRICE_BONUS : 1.0;
}
function bulkBonus(qty) {
  if (qty >= 100) return 1.5;  // WorldMapConstants::BulkBonus100
  if (qty >= 50) return 1.25;  // BulkBonus50
  if (qty >= 10) return 1.10;  // BulkBonus10
  return 1.0;
}
const GRADE_REVENUE_MULT = { S: 2.0, A: 1.5, B: 1.2, C: 1.0, D: 0.8, F: 0.5 }; // QualityGrade.h:97-109
function gradeMultiplier(grade) {
  return GRADE_REVENUE_MULT[grade] ?? 1.0; // switch default: return 1.0f;
}

function findCountryInfo(values, type) {
  const row = values.dt('DT_CountryInfo').find(r => r.CountryType === type || r.Name === type);
  if (!row) throw new Error(`DT_CountryInfo 누락 행: ${type}`);
  return row;
}
function findCountryDemand(values, type, industry) {
  // 셀 미정의 시 1.0 폴백 — TradePort.cpp:176 그대로(Loud failure는 DT 에디터 쪽 책임, 여기선 조용한 중립값이 원본과 동일 동작).
  return values.dt('DT_CountryDemand').find(r => r.Country === type && r.Industry === industry) ?? null;
}

// tradeSale(values, rng, recipeRow, qty, country, order) — ComputeSell(TradePort.cpp:111-275) 이식.
//   recipeRow: {ProjectIndex, BaseMarketCap, CompanyType, Grade?} — FProductRecipeTable 필드 + 판매 시점 품질등급
//     컨텍스트(Grade)를 얹은 합성 객체. Game/IT/Finance처럼 레시피가 없는 산업은 {ProjectIndex, BaseMarketCap:10}
//     최소 형태로 호출(코드의 bRecipeOk=false → MarketCapPerUnit 폴백 10과 동일).
//   country: {Type, DemandRatio?} — Type은 DT_CountryInfo.CountryType(예:'USA'). DemandRatio(0~1)는 시장 포화
//     상태값(CountryMarketManager 내부 Current/Capacity 비율)이라 DT에 없는 런타임 상태 — 호출자가 직접 공급.
//   order: null | {RewardMultiplier, DiamondBonus, RemainingQuantity} — 주문 미매칭이면 null.
//   rng: 현재 내부에서 소비하지 않음(무역 판매 공식 자체는 결정론적) — 이 모듈의 다른 함수들과 시그니처를
//     통일하고, 상위 몬테카를로 계층(주문/국가 선택)이 같은 rng를 전달해 재현성을 유지할 수 있도록 자리만 확보.
function tradeSale(values, rng, recipeRow, qty, country, order) {
  const idx = Math.max(1, recipeRow.ProjectIndex);
  const basePrice = Math.max(100, 100 + 50 * idx * idx); // GetBasePrice, TradePort.cpp:18-24

  const countryInfo = findCountryInfo(values, country.Type);
  const isHub = !!countryInfo.bIsTradeHub;
  const portPriceMul = isHub ? portPriceMultiplier(country.Type) : 1.0;
  const bulkMul = isHub ? bulkBonus(qty) : 1.0;
  const matchMul = order && order.RewardMultiplier > 0 ? order.RewardMultiplier : 1.0;
  const moneyBiasMul = countryInfo.MoneyBias;
  const marketCapBiasMul = countryInfo.MarketCapBias;

  const demandRatio = clamp(country.DemandRatio ?? 1.0, 0, 1);
  const demandMul = 0.4 + 0.8 * demandRatio; // GetDemandMul, CountryMarketManager.cpp:185-190

  const demandRow = findCountryDemand(values, country.Type, recipeRow.CompanyType);
  const industryPriceMul = demandRow ? demandRow.PriceMul : 1.0;
  const gradeMul = gradeMultiplier(recipeRow.Grade);

  const money = Math.round(
    basePrice * qty * portPriceMul * bulkMul * matchMul * moneyBiasMul * demandMul * industryPriceMul
  );

  const marketCapPerUnit = recipeRow.BaseMarketCap > 0 ? recipeRow.BaseMarketCap : 10;
  const marketCap = Math.round(marketCapPerUnit * qty * gradeMul * matchMul * marketCapBiasMul * demandMul);

  // Diamond는 주문서 완전충족(이번 판매 수량이 잔여 수량 이상) 시에만 — TradePort.cpp:234-239.
  const diamond = order && order.DiamondBonus > 0 && qty >= (order.RemainingQuantity ?? Infinity)
    ? order.DiamondBonus
    : 0;

  return { money, marketCap, diamond };
}

// ── 모뉴먼트 패시브 (BuildingBaseActor.cpp / BuildingData.h FKeystoneAuraData) ─────────

// GetMonumentPassiveOutput(BuildingBaseActor.cpp:284) = BasePassiveOutput + (Lv-1)×PassivePerLevel.
// ⚠ 이 두 계수는 BuildingDataTable(에디터 전용 DataTable — CSV 시드 없음, DataImport/에 대응 파일 없음)에
// 있는데 Tools/Balance/extract/manifest_editor.json의 dtPaths에 미등록 — 이번 태스크에서 실측 불가 확인
// (coverage.json S6을 stubbed로 낮춘 사유). values.get()으로 계수를 받게 해 loud fail은 유지하되,
// fake_values.js의 monument.* 값은 "미검증 placeholder"로 명시(§ 해당 파일 주석 참고).
function monumentPassivePerSec(values, keystoneLv) {
  const basePassiveOutput = values.get('monument.basePassiveOutput');
  const passivePerLevel = values.get('monument.passivePerLevel');
  const lv = Math.max(1, keystoneLv);
  return basePassiveOutput + (lv - 1) * passivePerLevel;
}

// ── 벽돌공장 (BrickFactory.cpp + FFactoryUpgradeConfig::CalculateUpgradeValue) ────────

// CalculateUpgradeValue(FactoryUpgradeConfig.h:22-97) 이식 — DT 비의존 구간별 하드코딩(주석 원문:
// "복잡한 구간별 값 계산은 코드에 유지"). 이 태스크가 다루는 3축만 이식(HoldProductionSpeed는 수동 홀드
// 전용이라 자동 생산 처리량과 무관 — brickFactoryPerHour에 미포함).
function factoryUpgradeValue(type, level) {
  switch (type) {
    case 'HoldProductionAmount':
      return level; // Lv = 개수 그대로 (:52-54)
    case 'AutoCollection': {
      if (level <= 1) return 10.0;                       // Lv1 = 10초 (:65-68, Lv<=0 클램프 :59-62)
      if (level <= 91) return 10.0 - (level - 1) * 0.1;   // Lv2~91 (:69-73)
      if (level <= 541) return 1.0 - (level - 91) * 0.002; // Lv92~541 (:74-78)
      if (level <= 1000) return 0.1 - (level - 541) * (0.099 / 459.0); // Lv542~1000 (:79-83)
      return 0.001; // 만렙 (:84-88)
    }
    case 'AutoCollectionCapacity':
      return level; // Lv = 용량 그대로 (:91-93)
    default:
      return 0.0;
  }
}

// AutoCollectResources(BrickFactory.cpp:527-567) 이식 — 매 Interval초마다 ProductionAmount만큼 적립,
// MaxCapacity에서 정지(수동 CollectAutoResources 전까지). perHour는 "제때 수거한다" 가정의 상한 처리량
// (uncapped steady-state) — capacityPerCycle을 별도로 노출해 호출자가 수거 주기 제약을 스스로 반영하게 한다.
const MIN_AUTO_COLLECTION_INTERVAL_KEY = 'brick.minAutoCollectionInterval'; // BrickFactory.h:149, 0.25s
function brickFactoryPerHour(values, upgradeLvs) {
  const { holdAmountLv = 1, autoCollectionLv = 1, autoCollectionCapacityLv = 1 } = upgradeLvs || {};

  const productionAmount = Math.max(1, Math.round(factoryUpgradeValue('HoldProductionAmount', holdAmountLv)));
  const rawInterval = factoryUpgradeValue('AutoCollection', autoCollectionLv);
  const minInterval = values.get(MIN_AUTO_COLLECTION_INTERVAL_KEY);
  const interval = Math.max(minInterval, rawInterval);
  const capacityPerCycle = Math.max(1, Math.round(factoryUpgradeValue('AutoCollectionCapacity', autoCollectionCapacityLv)));

  const perHour = (productionAmount / interval) * 3600;
  return { perHour, capacityPerCycle };
}

// ── 채광 (MineManager.cpp wall-clock lazy 모델) ─────────────────────────

// ComputeBaseRatePerSecondByIndex(:478-483) 이식 — "임시 룰"(주석 원문) 하드코딩: 국가 자원 풀의 0번(주력)
// = 2/min, 그 외(보조) = 1/min. resourceType은 실제 EResourceType이 아니라 이 풀 인덱스 구분을 나타내는
// 'primary'|'secondary' 문자열 — 실제로 어떤 EResourceType이 0번인지는 국가별 DT_CountryInfo.MinableResources
// 순서에 달려있고 이 함수의 관심사가 아니다(호출자가 판단해서 넘김).
const PRIMARY_RATE_PER_SEC = 2 / 60;
const SECONDARY_RATE_PER_SEC = 1 / 60;

function findMineUpgradeRow(values, upgradeType) {
  const row = values.dt('DT_MineUpgradeDefinition').find(r => r.UpgradeType === upgradeType || r.Name === upgradeType);
  if (!row) throw new Error(`DT_MineUpgradeDefinition 누락 행: ${upgradeType}`);
  return row;
}

// GetLineState(:252-276) 닫힌식 버전 — wall-clock 경과×유효속도, LineMax(=GetMaxStorage)에서 클램프+floor.
function mineYield(values, resourceType, elapsedSec, upgradeLvs) {
  const { miningRateLv = 0, miningStorageLv = 0 } = upgradeLvs || {};

  const baseRatePerSec = resourceType === 'primary' ? PRIMARY_RATE_PER_SEC : SECONDARY_RATE_PER_SEC;
  const rateRow = findMineUpgradeRow(values, 'MiningRate');
  const rateMul = 1.0 + rateRow.PerLevel * miningRateLv; // GetRateMultiplier :341-349

  const storageRow = findMineUpgradeRow(values, 'MiningStorage');
  const cap = storageRow.BaseValue + storageRow.PerLevel * miningStorageLv; // GetMaxStorage :351-360

  const effectiveRatePerSec = baseRatePerSec * rateMul;
  const raw = Math.max(0, elapsedSec) * effectiveRatePerSec;
  return Math.min(Math.floor(raw), cap);
}

// ── 가챠(직원 채용) — RecruitmentManagerSubsystem.cpp ───────────────────

// GetProbabilityTable(:395-460) 이식 — 전부 하드코딩 확률표(DT 아님). 'Legendary'는 이 시뮬 표기에서
// 'Highest'로 통일한다: Advanced/Premium 두 티어 모두 천장(하드/소프트 피티) 보장 등급이 Legendary 하나뿐이라
// "피티로 도달하는 최고 등급"과 "표에서 자연 롤로 뽑은 최고 등급"을 같은 라벨로 묶는 게 개념상 더 정확하고,
// 브리핑 테스트도 이 라벨('Highest')을 기대한다.
const GACHA_PROB_TABLES = {
  Advanced: [['Common', 0.10], ['Unusual', 0.40], ['Rare', 0.35], ['Epic', 0.13], ['Highest', 0.02]], // :440-444
  Premium: [['Common', 0.25], ['Unusual', 0.40], ['Rare', 0.22], ['Epic', 0.12], ['Highest', 0.01]],  // :450-454
};
function normalProbTable(hrPower) {
  // :404-433 — Normal 티어는 HR파워 구간별 표. Legendary 슬롯 자체가 없어 천장 개념이 없다.
  if (hrPower >= 50) return [['Common', 0.55], ['Unusual', 0.30], ['Rare', 0.13], ['Epic', 0.02]];
  if (hrPower >= 30) return [['Common', 0.63], ['Unusual', 0.25], ['Rare', 0.11], ['Epic', 0.01]];
  if (hrPower >= 20) return [['Common', 0.73], ['Unusual', 0.20], ['Rare', 0.07]];
  if (hrPower >= 10) return [['Common', 0.85], ['Unusual', 0.12], ['Rare', 0.03]];
  return [['Common', 1.00]];
}
const PITY_TIERS = new Set(['Advanced', 'Premium']); // RollPotentialRarity — Normal은 PityDataPtr=nullptr(:482-491)

// gachaPull(rng, state, values) — ExecuteGachaPull(:148-233)+RollPotentialRarity(:476-573) 이식.
//   state: {tier?='Advanced', pity?=0, hrPower?=0, mileage?=0, hasTicket?=false}
//     - pity = PullsSinceLastGuaranteed(이번 뽑기 "이전"까지 누적, 0-base).
//     - hasTicket=true면 해당 티어 채용권을 소비(Diamond 비용 0) — DeductGachaCost의 "채용권 우선" 경로(:132-136).
function gachaPull(rng, state, values) {
  const tier = state.tier || 'Advanced'; // 피티 있는 티어가 기본(브리핑 테스트가 tier 미지정)
  const hrPower = state.hrPower || 0;
  const pity = state.pity || 0;
  const mileage = state.mileage || 0;
  const hasPity = PITY_TIERS.has(tier);

  const hardPity = values.get('gacha.hardPity');
  const softPityStart = values.get('gacha.softPityStart');
  const softPityBonus = values.get('gacha.softPityBonus');

  let rarity;
  if (hasPity && pity + 1 >= hardPity) {
    rarity = 'Highest'; // Hard Pity 확정 (:493-502)
  } else {
    const table = (tier === 'Normal' ? normalProbTable(hrPower) : GACHA_PROB_TABLES[tier]).map(row => [...row]);
    if (hasPity) {
      const pullCount = pity + 1;
      if (pullCount > softPityStart) {
        // Soft Pity 보정(:507-548): 보장등급 슬롯에 가산, 나머지는 동일 비율(min(0.5,bonus))로 감산.
        const bonus = (pullCount - softPityStart) * softPityBonus;
        let targetIdx = table.findIndex(([r]) => r === 'Highest');
        if (targetIdx < 0) { table.push(['Highest', 0]); targetIdx = table.length - 1; }
        table[targetIdx][1] += bonus;
        const shrink = Math.min(0.5, bonus);
        for (let i = 0; i < table.length; i++) {
          if (i !== targetIdx) table[i][1] -= table[i][1] * shrink;
        }
      }
    }
    const total = table.reduce((s, [, p]) => s + p, 0);
    const roll = rng.next() * total;
    let acc = 0;
    rarity = table[0][0];
    for (const [r, p] of table) {
      acc += p;
      if (roll < acc) { rarity = r; break; }
    }
  }

  // Pity 카운터 업데이트 — Highest 획득 시에만 리셋(IncrementPull 후 ResetPity, :173-197과 동치).
  const newPity = hasPity ? (rarity === 'Highest' ? 0 : pity + 1) : pity;
  // 마일리지는 Premium 전용, 1뽑당 +1(:194-196).
  const newMileage = tier === 'Premium' ? mileage + 1 : mileage;

  let diamondCost = 0;
  if (!state.hasTicket) {
    if (tier === 'Advanced') diamondCost = values.get('gacha.advancedDiamondCost');
    else if (tier === 'Premium') diamondCost = values.get('gacha.premiumDiamondCost');
    // Normal은 Diamond 폴백이 없다(DeductGachaCost :105-106) — diamondCost 0 유지.
  }

  return {
    rarity,
    diamondCost,
    newState: { ...state, tier, pity: newPity, mileage: newMileage },
  };
}

// mileageExchange(state, values) -> {canExchange, newState, rarity} — ExchangeMileage(:315-344) 이식.
// 실코드 대조:
//   - 차감량 = FGachaMileageData::ExchangeCost(200) 고정, 남는 마일리지는 이월(여러 번 교환 가능 — 정산은
//     호출자가 반복 호출).
//   - 확정 등급 = Legendary(RollPotentialRarity/확률표를 전혀 거치지 않고 GenerateGachaEmployee(Legendary) 직행).
//     이 시뮬 표기는 gachaPull과 동일하게 'Highest'로 통일한다 — ExchangeMileage가 실제로 만드는 등급이
//     gachaPull의 피티 보장 등급과 게임상 같은 개념(Legendary)이라, 두 경로가 서로 다른 라벨('Legendary' vs
//     'Highest')을 반환하면 Task10 등 소비 측이 "최고 등급 획득"을 한쪽만 검사해 나머지를 놓치는 함정이 된다.
//   - 피티 카운터: AdvancedPity/PremiumPity 어느 쪽도 건드리지 않는다(IncrementPull/ResetPity 호출 자체가
//     없음) — 마일리지 교환은 피티 진행에 영향이 전혀 없다. newState.pity는 입력 그대로 보존.
//   - rng 불필요(RollPotentialRarity를 안 거치므로 결정론적) — gachaPull과 달리 rng 파라미터가 없다.
function mileageExchange(state, values) {
  const cost = values.get('gacha.mileageExchangeCost');
  const mileage = state.mileage || 0;

  if (mileage < cost) {
    return { canExchange: false, newState: { ...state }, rarity: null };
  }

  return {
    canExchange: true,
    newState: { ...state, mileage: mileage - cost }, // pity 등 나머지 필드 불변(ExchangeMileage 미접촉)
    rarity: 'Highest', // Legendary 확정 — gachaPull과 라벨 통일(위 주석 참고)
  };
}

// ── Diamond 예산 ─────────────────────────────────────────────────────

// CheckAndGrantCodexMilestone(OfficeStageProgressManager.cpp:1549-1594) 이식.
const MAX_CODEX_TIER = 10; // TierConstants::MAX_TIER, ProjectBoardData.h:106 — 구조 상수(도감 티어 개수)

// DT_Mission.Rewards[] 문자열("(ResourceType=Diamond,Amount=100,...)")에서 Diamond 보상만 합산.
// 현재 추출된 DT_Mission 19행이 곧 온보딩 미션 체인 전체(M1~M21, NextMissionID로 연결된 단일 선형 체인)라
// design §10에서 이미 "첫 30분 온보딩 마일스톤만 모델" 범위로 확정된 대상과 정확히 일치 — 별도 축소 불필요.
function missionDiamondTotal(values) {
  let total = 0;
  for (const row of values.dt('DT_Mission')) {
    for (const rewardStr of row.Rewards || []) {
      const m = /ResourceType=(\w+),Amount=(-?\d+)/.exec(rewardStr);
      if (m && m[1] === 'Diamond') total += parseInt(m[2], 10);
    }
  }
  return total;
}

// diamondBudget(values) -> {codex, vipPerOrderEV, missionTotal}
function diamondBudget(values) {
  const codex = MAX_CODEX_TIER * values.get('diamond.codexTier') + values.get('diamond.codexFull');

  const tob = values.dt('DT_TradeOrderBalance').find(r => r.Name === 'Default');
  if (!tob) throw new Error('DT_TradeOrderBalance 누락 행: Default');
  const vipPerOrderEV = (tob.VIP_DiamondMin + tob.VIP_DiamondMax) / 2;

  const missionTotal = missionDiamondTotal(values);

  return { codex, vipPerOrderEV, missionTotal };
}

// ── MarketCap 배율 (CountryMarketManager.cpp) ───────────────────────────

// CalculateGrowthMultiplier(:11-25) 이식: clamp(BaseMul + Coef×log10(1+MC/Pivot), BaseMul, MaxMul).
function marketCapMultiplier(values, mc) {
  const B = values.dt('DT_MarketBalance').find(r => r.Name === 'Default');
  if (!B) throw new Error('DT_MarketBalance 누락 행: Default');
  const pivot = Math.max(1, B.PivotMC);
  const ratio = Math.max(0, mc) / pivot;
  const raw = B.BaseMul + B.Coef * Math.log10(1 + ratio);
  return clamp(raw, B.BaseMul, B.MaxMul);
}

module.exports = {
  tradeSale,
  cityAcquisitionEV,
  monumentPassivePerSec,
  brickFactoryPerHour,
  mineYield,
  gachaPull,
  mileageExchange,
  diamondBudget,
  marketCapMultiplier,
};
