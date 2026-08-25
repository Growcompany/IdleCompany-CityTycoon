// 월드 러너 — Task1~9 모듈을 한 세계로 조립한다.
// 캘린더 일 단위로 세션(7.5분×3.5회/일)을 배치하고, 세션 안에서 수거→착수→투자→서브경제를,
// 세션 사이 갭에서 offlineGains 정산을 돌린다. 티어 진입 = 7첫클리어 AND 빌딩Lv ≥ (T−1)×2.
//
// ── 모델 경계(해석 시 반드시 감안) ────────────────────────────────────────
// 1) 산업은 Game 하나만. 나머지 5산업은 DT_Project_* 의 Weight_* 가 전부 0(결손, retune §7-10)이라
//    개발 시뮬 자체가 성립하지 않는다.
// 2) 로스터는 전 빌딩 공용 1벌 — **낙관 편향**. 실코드는 빌딩별 로스터 + 빌딩별 TierProgress
//    (GetCurrentStudioLevel = "관리 중인 빌딩")라, 실제 효과는 "채용비를 P배 아낀다"가 아니라
//    **P개 빌딩의 모든 프로젝트가 각각 전체 로스터(최대 31인)의 개발점수 산출을 통째로 받는다**는 것이다.
//    실게임에서 31인을 P개 빌딩에 나눠 앉히면 빌딩당 31/P 인분 산출뿐이므로, 이 모델은 부속 슬롯의
//    개발 성공률·등급을 실제보다 높게 본다. retune §3 도 P를 "병렬 운영 슬롯"으로만 셌으므로 비교는 대등.
//    티어 진행(첫클리어)은 주 스튜디오(빌딩0)에서만 하고, 게이트도 빌딩0 레벨로 판정한다.
// 3) 빌딩 수 P 는 정책 램프(retune §3). 건물 Brick 건설비·HQ RequiredBuildingCount 미모델(retune §7-7).
// 4) StatBonus(★→개발/수익)·잠재큐브·트레이트·이벤트는 1.0 고정 (retune §7-6 과 동일 전제).
// 5) ★강화는 실코드에서 크리티컬 확률 경유로 개발점수에 기여하지만(EmployeeBehaviorComponent.cpp:1531
//    GetEnhanceStatBonus), 그 계수가 values 로 추출돼 있지 않아 이 모델은 커플링을 0으로 둔다.
//    calib.starDevBonusPerStar 가 주어질 때만 ★를 사고, 기본값(0)에서는 아예 사지 않는다
//    — 효과 없는 싱크에 돈을 태우면 프로필 비교가 왜곡되기 때문.
const fs = require('node:fs');
const { makeRng } = require('./rng.js');
const { Ledger } = require('./ledger.js');
const { loadValues } = require('./values.js');
const { simulateDevelopment, gradeOf, deriveDevScorePerEmpPerSec } = require('./project.js');
const { operationTotals, vaultCapacity } = require('./operation.js');
const { idlePerSecond, offlineGains, calculateBaseOutput } = require('./idle.js');
const { starAttempt, starExpectedCost, buildingUpgradeCost, buildingEffect } = require('./enhance.js');
const { gachaPull } = require('./subecon.js');
const { makePolicy } = require('./policy.js');

const INDUSTRY = 'Game';
const DAY_SECONDS = 86400;
const PROJECTS_PER_TIER = 10; // TierConstants::PROJECTS_PER_TIER — 구조 상수(밸런스 값 아님)
const MAX_TIER = 10;          // TierConstants::MAX_TIER
const GRADE_ORDER = ['F', 'D', 'C', 'B', 'A', 'S']; // exp.qualityMult 배열 순서

function runWorld({ seed, profile = 'mid', valuesPath, calibPath = null, untilTier = MAX_TIER, maxCalendarDays = 120 }) {
  const values = loadValues(valuesPath);
  const calib = calibPath ? JSON.parse(fs.readFileSync(calibPath, 'utf8')) : {};
  const policy = makePolicy(profile, values);

  // 서브스트림 분리 — 정책이 뽑기 횟수를 바꿔도 개발 시뮬 난수열이 밀리지 않게 한다.
  const root = makeRng(seed);
  const rng = {
    dev: root.fork('dev'), feed: root.fork('feed'), trend: root.fork('trend'),
    star: root.fork('star'), gacha: root.fork('gacha'), gamble: root.fork('gamble'),
  };

  const ledger = new Ledger();
  const gradeDist = { S: 0, A: 0, B: 0, C: 0, gateFail: 0 };
  const tiers = [];
  let invalid;

  // ── values 캐시 ────────────────────────────────────────────────────
  const projects = values.dt(`DT_Project_${INDUSTRY}`).slice().sort((a, b) => a.ProjectIndex - b.ProjectIndex);
  const byIndex = new Map(projects.map(r => [r.ProjectIndex, r]));
  const profileRow = values.dt('DT_IndustryProfile').find(r => (r.Industry ?? r.Name) === INDUSTRY);
  if (!profileRow) throw new Error(`DT_IndustryProfile 누락 행: ${INDUSTRY}`);
  const affinity = new Map();
  for (const r of tryDt('DT_ProjectAffinity')) {
    if ((r.Industry ?? INDUSTRY) === INDUSTRY) affinity.set(`${r.Genre}|${r.Material}`, r.Grade);
  }
  const buildingLevels = tryDt('DT_BuildingLevel')
    .map(r => ({ level: parseInt(r.Name, 10), requiredExp: r.RequiredEXP, maxWorkstations: r.MaxWorkstations }))
    .filter(r => Number.isFinite(r.level)).sort((a, b) => a.level - b.level);
  const enhanceRows = new Map(values.dt('DT_BuildingEnhancementDefinition').map(r => [r.EnhancementType ?? r.Name, r]));
  const shopRows = tryDt('DT_ShopItem');
  const gambleRow = tryDt('DT_BoostGamble').find(r => (r.Industry ?? r.Name) === INDUSTRY) ?? null;

  const CLEAR_TO_UNLOCK = values.get('gate.clearToUnlock');
  const REQ_LV_PER_TIER = values.get('gate.reqBuildingLvPerTier');
  const STEP_EXP = values.get('exp.stepBase');                       // [step1, step2, step3]
  const LAUNCH_EXP_BASE = values.get('exp.launchBuildingBase');
  const LAUNCH_EXP_BONUS = values.get('exp.launchBuildingGradeBonus'); // [C,B,A,S]
  const OP_EXP_MIN = values.get('exp.operationBuildingMin');
  const OP_EXP_RATE = values.get('exp.operationBuildingRevenueRate');
  const OP_EMP_EXP = values.get('exp.operationEmployeeBase');
  const QUALITY_EXP_MULT = values.get('exp.qualityMult');            // [F,D,C,B,A,S]
  const EMP_LEVEL_EXP_BASE = values.get('emp.levelExpBase');
  const EMP_LEVEL_EXP_COEF = values.get('emp.levelExpCoef');
  const BASE_DPS = calib.devScorePerEmpPerSec ?? deriveDevScorePerEmpPerSec(values);
  const STAR_DEV_BONUS = calib.starDevBonusPerStar ?? 0;

  function tryDt(name) {
    try { return values.dt(name); } catch { return []; }
  }

  // ── 상태 ───────────────────────────────────────────────────────────
  const S = {
    timeSec: 0, inAppSec: 0, sessions: 0, launches: 0,
    tier: 1, cleared: new Set(), repeats: 0, gateFails: 0,
    buildings: [], roster: [], hqLevel: 1,
    trend: { material: null, launchesLeft: 0 },
    gacha: { tier: 'Advanced', pity: 0, mileage: 0 },
    tickets: { normal: 0, advanced: 0 },
    shop: { day: 0, week: 0, normalToday: 0, advancedThisWeek: 0 },
    codexRewarded: new Set(), codexFullRewarded: false,
    vaultLost: 0, onboarded: false,
  };
  let cur = newTierRecord(1, 1);

  function newTierRecord(tier, day) {
    // grades = 그 티어에서 실제로 나온 등급 분포(정책 targetGradeMix 와 대조할 실현치).
    // expGateBlocks = 7첫클리어를 채웠는데 빌딩Lv 게이트에 막힌 착수 횟수(EXP 병목 관측점, verify/assertions.js A-EXPGATE).
    return { tier, clears: 0, repeats: 0, sessions: 0, inAppMin: 0, calendarDay: day, expGateBlocks: 0, grades: { S: 0, A: 0, B: 0, C: 0, gateFail: 0 } };
  }
  function day() { return Math.floor(S.timeSec / DAY_SECONDS) + 1; }
  function money() { return ledger.balance('Money'); }
  function chk(v, label) {
    if (!Number.isFinite(v)) throw new Error(`비수치 상태 ${label}: ${v}`);
    return v;
  }
  function post(currency, amount, srcId) {
    chk(amount, `ledger ${srcId}`);
    if (amount === 0) return;
    ledger.post(day(), currency, amount, srcId);
  }

  function addBuilding() {
    S.buildings.push({ level: 1, exp: 0, enh: new Map(), op: null, storedOnline: 0, storedOffline: 0 });
  }
  function enhLv(b, axis) { return b.enh.get(axis) ?? 0; }
  function effect(b, axis) { return buildingEffect(values, axis, enhLv(b, axis)); }
  function workstationCap(level) {
    const row = buildingLevels.filter(r => r.level <= level).pop();
    return row ? row.maxWorkstations : 2;
  }
  function requiredExpFor(level) {
    const row = buildingLevels.find(r => r.level === level);
    return row ? row.requiredExp : Infinity;
  }
  function addBuildingExp(b, amount) {
    b.exp += chk(amount, 'buildingExp');
    // CheckBuildingLevelUp — 누적 EXP 비차감, 연쇄 레벨업 (SaveLoadManager.cpp:1283)
    while (b.level < buildingLevels.length && b.exp >= requiredExpFor(b.level + 1)) b.level++;
  }
  function addEmployeeExp(b, baseAmount, grade) {
    const mult = QUALITY_EXP_MULT[GRADE_ORDER.indexOf(grade)] ?? 1.0;
    const growth = effect(b, 'EmployeeGrowth');
    const amount = baseAmount * mult * growth;
    for (const e of S.roster) {
      e.exp += amount;
      // GetMaxExperienceForLevel = floor(base × Lv × coef), 초과분 이월 (EmployeeManager.cpp:1292)
      for (;;) {
        const need = Math.floor(EMP_LEVEL_EXP_BASE * e.level * EMP_LEVEL_EXP_COEF);
        if (need <= 0 || e.exp < need) break;
        e.exp -= need;
        e.level++;
      }
    }
  }

  // ── 운영(감쇠) ─────────────────────────────────────────────────────
  // ProjectOperationManager 의 지수감쇠를 구간 적분으로 — operation.js 의 닫힌식과 같은 커브를
  // 부분 구간에도 쓸 수 있게 로컬 구현(세션/갭이 운영을 쪼개므로 전구간 합계만으론 부족).
  function decayAt(op, e) {
    return op.stableRate * Math.pow(0.5, e / (op.totalSeconds * profileRow.HalfLifeFrac));
  }
  function decayIntegral(op, from, to) {
    const hl = op.totalSeconds * profileRow.HalfLifeFrac;
    const k = Math.LN2 / hl;
    return (op.stableRate / k) * (Math.exp(-k * from) - Math.exp(-k * to));
  }
  // countLoss=false 로 부르는 경로(오프라인)는 offlineGains 가 이미 같은 cap/stored 로 클리핑을 끝낸
  // 뒤라 여기서 다시 빼면 손실이 항상 0으로 나온다 — 손실은 호출자가 (raw − gain) 으로 직접 센다.
  function storeRevenue(b, amount, offline, countLoss = true) {
    const cap = vaultCapacity(values, b.op.stableRate, enhLv(b, 'VaultCapacity'));
    const space = Math.max(0, cap - b.storedOnline - b.storedOffline);
    const gain = Math.min(amount, space);
    if (countLoss) S.vaultLost += amount - gain;
    if (offline) b.storedOffline += gain; else b.storedOnline += gain;
    b.op.earned += gain;
    return gain;
  }
  function finishOperationIfDone(b) {
    if (!b.op || b.op.elapsed < b.op.totalSeconds) return;
    addBuildingExp(b, Math.max(OP_EXP_MIN, b.op.earned * OP_EXP_RATE));
    addEmployeeExp(b, OP_EMP_EXP, b.op.grade);
    b.op = null;
  }

  // ── 세션 ───────────────────────────────────────────────────────────
  const sessionSec = policy.sessionMinutes * 60;
  const intervalSec = DAY_SECONDS / policy.sessionsPerDay;
  const gapSec = intervalSec - sessionSec;

  function onboarding() {
    if (S.onboarded) return;
    S.onboarded = true;
    // DT_Mission 온보딩 체인 보상(S11) — 첫 30분 안에 소진되는 1회성이라 시작 시 일괄 계상.
    let m = 0, d = 0;
    for (const row of tryDt('DT_Mission')) {
      for (const rw of row.Rewards || []) {
        const res = /ResourceType=(\w+),Amount=(-?\d+)/.exec(rw);
        if (res && res[1] === 'Money') m += parseInt(res[2], 10);
        if (res && res[1] === 'Diamond') d += parseInt(res[2], 10);
        const item = /ItemType=(\w+),ItemAmount=(\d+)/.exec(rw);
        if (item && item[1] === 'RecruitTicketNormal') S.tickets.normal += parseInt(item[2], 10);
      }
    }
    post('Money', m, 'S11');
    post('Diamond', d, 'DS4');
    addBuilding();
    // 시작 로스터 = 빌딩Lv1 워크스테이션 2석(튜토리얼이 2인까지 채워준다)
    for (let i = 0; i < workstationCap(1); i++) S.roster.push({ level: 1, exp: 0, star: 0, disciplinePts: [] });
  }

  // 원장 계상 시점 = **수령(금고→지갑)** 이다. 적립(S1 ProcessRevenue)이 아니라 여기서 세는 이유는
  // 금고 초과분이 실제로 지갑에 안 들어오기 때문 — 적립 시점에 세면 vaultLost 만큼 과대계상된다.
  // 그래서 srcId 'S1' 은 "운영수익 유입"을 뜻하되 **수령 창구(S2 MainMap / S3 OfficeMap)를 통합한 단일 계상**이다.
  // S2/S3 를 따로 post 하지 않는 것은 누락이 아니라 이중계상 방지 — coverage.json 의 S2/S3 pipelineReason 참조.
  function collect() {
    for (const b of S.buildings) {
      if (b.storedOnline > 0) { post('Money', b.storedOnline, 'S1'); b.storedOnline = 0; }
      if (b.storedOffline > 0) { post('Money', b.storedOffline, 'S10'); b.storedOffline = 0; }
    }
  }

  function accrueOnline(seconds) {
    for (const b of S.buildings) {
      if (!b.op) continue;
      const to = Math.min(b.op.totalSeconds, b.op.elapsed + seconds);
      storeRevenue(b, decayIntegral(b.op, b.op.elapsed, to), false);
      b.op.elapsed = to;
      finishOperationIfDone(b);
    }
    // S4 방치 직원 수익 — 오피스 상주(=인앱) 중에만 발생. EmployeeBehaviorComponent 는 오프라인 미정산.
    let idle = 0;
    for (const e of S.roster) idle += idlePerSecond(values, e) * seconds;
    post('Money', idle, 'S4');
  }

  function settleGap(seconds) {
    for (const b of S.buildings) {
      if (!b.op) continue;
      const remaining = b.op.totalSeconds - b.op.elapsed;
      const cap = vaultCapacity(values, b.op.stableRate, enhLv(b, 'VaultCapacity'));
      const res = offlineGains(
        values,
        { actualRatePerSec: decayAt(b.op, b.op.elapsed), remainingSeconds: remaining, vaultCap: cap, stored: b.storedOnline + b.storedOffline },
        seconds, 1.0
      );
      storeRevenue(b, res.gain, true, false);
      S.vaultLost += Math.max(0, res.raw - res.gain); // 금고 초과로 날린 오프라인 수익
      b.op.elapsed = Math.min(b.op.totalSeconds, b.op.elapsed + res.opElapsedAdvance);
      finishOperationIfDone(b);
    }
    S.timeSec += seconds;
  }

  // ── 착수 ───────────────────────────────────────────────────────────
  function tierBand(tier) {
    return { start: (tier - 1) * PROJECTS_PER_TIER + 1, end: tier * PROJECTS_PER_TIER };
  }
  function affinityOf(row) { return affinity.get(`${row.Genre}|${row.Material}`) ?? null; }
  function rotateTrendIfNeeded() {
    if (S.trend.launchesLeft > 0) return;
    const pool = [...new Set(projects.map(r => r.Material))].filter(m => m !== S.trend.material);
    // TrendManagerSubsystem: 착수 3회마다 교체, 직전 소재 제외 후 균등 추첨.
    S.trend.material = pool.length ? pool[Math.floor(rng.trend.next() * pool.length)] : null;
    S.trend.launchesLeft = 3;
  }
  // allowFirstClear=false → 이미 클리어한 프로젝트만(재개발 파밍). 실코드의 TierProgress 는 빌딩별이라
  // 부속 빌딩 클리어는 주 스튜디오 티어에 기여하지 않는다 — 그 구조를 이 필터로 근사한다.
  function buildFeed(allowFirstClear) {
    const { start, end } = tierBand(S.tier);
    let band = [];
    for (let i = start; i <= end; i++) if (byIndex.has(i)) band.push(byIndex.get(i));
    if (!allowFirstClear) band = band.filter(r => S.cleared.has(r.ProjectIndex));
    if (band.length === 0) return [];
    const pick = [];
    const pool = band.slice();
    for (let i = 0; i < policy.feedSize && pool.length; i++) {
      pick.push(pool.splice(Math.floor(rng.feed.next() * pool.length), 1)[0]);
    }
    return pick.map(row => ({
      row,
      affinityGrade: affinityOf(row),
      trendMatched: S.trend.material !== null && row.Material === S.trend.material,
      isFirstClear: !S.cleared.has(row.ProjectIndex),
      cost: row.DevelopmentCost,
    }));
  }
  function meanLevelFactor() {
    if (S.roster.length === 0) return 1;
    const unit = calculateBaseOutput(values, 1);
    let s = 0;
    for (const e of S.roster) s += calculateBaseOutput(values, e.level) / unit;
    return s / S.roster.length;
  }
  function meanStarBonus() {
    if (STAR_DEV_BONUS <= 0 || S.roster.length === 0) return 1;
    let s = 0;
    for (const e of S.roster) s += e.star;
    return 1 + (s / S.roster.length) * STAR_DEV_BONUS;
  }

  function launch(b, card) {
    const row = card.row;
    post('Money', -row.DevelopmentCost, 'K1');
    S.launches++;
    S.trend.launchesLeft--;

    // 부스트 도박(K2) — GoCost 는 결과와 무관한 확정 지출. 효과는 EffectType 별로
    // ResolveBoostGamble(OfficeStageProgressManager.cpp:1680~1745) 그대로 **절대 가감**이다:
    //   TimeExtend   RemainingTime += TimeSeconds  + 점수 ±(TargetSum×Frac)/ActiveCount
    //   TimeCut      RemainingTime -= TimeSeconds (0 클램프), 성공=무페널티 / 실패만 점수 −
    //   ScoreSwing   시간 불변, 점수 ±
    //   PayoffGamble 개발 점수 무관, 운영 수익 배율 EventRewardMultiplier ×= (1±Frac)
    // ⚠ 구모델은 이걸 effDps × (1±Frac) 곱셈으로 근사했는데, 그러면 목표 대비 달성률이 통째로 스케일돼
    //   개발점수 거품이 생긴다(2026-08-03 리뷰 F1). 실코드는 목표합 기준 절대량 가감이다.
    let gambleTimeSec = 0, gambleScoreFrac = 0, eventRewardMult = 1;
    if (gambleRow && money() >= gambleRow.GoCost
      && policy.useBoostGamble({ money: money(), nextDevCost: row.DevelopmentCost })) {
      post('Money', -gambleRow.GoCost, 'K2');
      const ok = rng.gamble.next() < gambleRow.SuccessChance;
      const t = gambleRow.TimeSeconds ?? 0;
      switch (gambleRow.EffectType) {
        case 'TimeExtend':
          gambleTimeSec = t;
          gambleScoreFrac = ok ? gambleRow.SuccessFrac : -gambleRow.FailFrac;
          break;
        case 'TimeCut':
          gambleTimeSec = -t;
          if (!ok) gambleScoreFrac = -gambleRow.FailFrac; // 성공은 페널티 없음(:1699-1703)
          break;
        case 'PayoffGamble':
          eventRewardMult = ok ? 1 + gambleRow.SuccessFrac : 1 - gambleRow.FailFrac;
          break;
        default: // ScoreSwing (및 미지정)
          gambleScoreFrac = ok ? gambleRow.SuccessFrac : -gambleRow.FailFrac;
          break;
      }
    }

    const effDps = BASE_DPS * meanLevelFactor() * meanStarBonus() * effect(b, 'ProjectYield');
    const dev = simulateDevelopment(
      rng.dev, values, { ...calib, devScorePerEmpPerSec: effDps }, S.roster, row,
      { tapDuty: policy.tapDuty, durationBonusSec: gambleTimeSec, boostScoreFrac: gambleScoreFrac }
    );

    // 스텝 EXP 는 게이트 통과 여부와 무관하게 스텝 완료 시점에 지급된다.
    const stepExpTotal = STEP_EXP.reduce((a, v) => a + v, 0);
    addBuildingExp(b, stepExpTotal);
    addEmployeeExp(b, stepExpTotal, 'B'); // DistributeStepExperience 는 등급 B 고정으로 호출

    if (!dev.passGate) {
      gradeDist.gateFail++;
      cur.grades.gateFail++;
      S.gateFails++;
      return gambleTimeSec; // 게이트 실패여도 늘어난 개발 시간은 세션에서 이미 소모됐다
    }

    // ProjectGrade 강화축은 q 에 곱해진 뒤 [0.5,2.0] 클램프 (ProjectOperationManager.cpp:112)
    const q = Math.min(2.0, Math.max(0.5, dev.q * effect(b, 'ProjectGrade')));
    const grade = gradeOf(q, values);
    gradeDist[grade]++;
    cur.grades[grade]++;

    const ctx = {
      grade, q, affinityGrade: card.affinityGrade, trendMatched: card.trendMatched,
      buildingIncomeMult: effect(b, 'MarketingPower'),
      statBonus: 1, industry: INDUSTRY, eventRewardMult,
    };
    const tot = operationTotals(values, row.ProjectIndex, ctx);
    b.op = {
      idx: row.ProjectIndex, grade,
      stableRate: chk(tot.stableRatePerSec, 'stableRate'),
      totalSeconds: chk(tot.totalSeconds * effect(b, 'ProjectLifespan'), 'opSeconds'),
      elapsed: 0, earned: 0,
    };
    addBuildingExp(b, LAUNCH_EXP_BASE + (LAUNCH_EXP_BONUS[['C', 'B', 'A', 'S'].indexOf(grade)] ?? 0));

    // 첫클리어만 티어 카운트 (재개발 기여 0)
    if (!S.cleared.has(row.ProjectIndex)) {
      S.cleared.add(row.ProjectIndex);
      cur.clears++;
      grantCodex(row.ProjectIndex);
    } else {
      S.repeats++;
      cur.repeats++;
    }
    checkTierUnlock();
    return gambleTimeSec;
  }

  function clearsInTier(tier) {
    const { start, end } = tierBand(tier);
    let n = 0;
    for (const i of S.cleared) if (i >= start && i <= end) n++;
    return n;
  }
  function grantCodex(idx) {
    const tier = Math.floor((idx - 1) / PROJECTS_PER_TIER) + 1;
    if (!S.codexRewarded.has(tier) && clearsInTier(tier) >= PROJECTS_PER_TIER) {
      S.codexRewarded.add(tier);
      post('Diamond', values.get('diamond.codexTier'), 'DS1');
    }
    if (!S.codexFullRewarded && S.cleared.size >= MAX_TIER * PROJECTS_PER_TIER) {
      S.codexFullRewarded = true;
      post('Diamond', values.get('diamond.codexFull'), 'DS2');
    }
  }
  function checkTierUnlock() {
    while (S.tier < MAX_TIER
      && clearsInTier(S.tier) >= CLEAR_TO_UNLOCK
      && S.buildings[0].level >= S.tier * REQ_LV_PER_TIER) {
      tiers.push(cur);
      S.tier++;
      cur = newTierRecord(S.tier, day());
      while (S.buildings.length < policy.slotsForTier(S.tier)) addBuilding();
    }
    // 루프를 빠져나온 이유가 "클리어는 찼는데 레벨이 모자람" 이면 이번 착수는 EXP 게이트에 막힌 것.
    if (S.tier < MAX_TIER && clearsInTier(S.tier) >= CLEAR_TO_UNLOCK
      && S.buildings[0].level < S.tier * REQ_LV_PER_TIER) {
      cur.expGateBlocks++;
    }
  }

  function runLaunches() {
    let remaining = sessionSec;
    for (let guard = 0; guard < 200; guard++) {
      const free = S.buildings.filter(b => !b.op);
      if (free.length === 0) break;
      // 주 스튜디오(빌딩0)가 비어 있으면 거기서 티어 진행, 아니면 부속 슬롯에서 파밍(재개발).
      const b = free.includes(S.buildings[0]) ? S.buildings[0] : free[0];
      rotateTrendIfNeeded();
      const feed = buildFeed(b === S.buildings[0]);
      if (feed.length === 0) break;
      const card = policy.pickProject({ money: money(), tier: S.tier }, feed);
      if (!card) break;
      const devSec = card.row.Duration + policy.launchOverheadSec;
      if (devSec > remaining) break;
      // 도박의 시간 효과는 착수 후에야 결정되므로(모달이 개발 중에 뜬다) 반환값으로 사후 정산한다 —
      // 야근(TimeExtend)이 세션 예산을 실제로 더 먹는다는 사실이 빠지면 high 프로필이 공짜로 빨라진다.
      remaining -= devSec + (launch(b, card) ?? 0);
    }
  }

  // ── 투자 / 서브경제 ────────────────────────────────────────────────
  function nextDevCost() {
    const { end } = tierBand(S.tier);
    const row = byIndex.get(end) ?? projects[projects.length - 1];
    return row ? row.DevelopmentCost : 0;
  }
  function cheapestBuildingFor(axis) {
    return S.buildings.slice().sort((a, b) => enhLv(a, axis) - enhLv(b, axis))[0];
  }
  function invest() {
    const reserve = nextDevCost() * policy.reserveMult;
    const priority = policy.investPriority({ tier: S.tier, money: money() });
    for (let guard = 0; guard < 500; guard++) {
      let bought = false;
      for (const token of priority) {
        if (token === 'star') {
          // ★ 커플링(크리티컬 경유 개발점수)이 values 에 없어 기본은 미투자 — 파일 헤더 5) 참고.
          if (STAR_DEV_BONUS <= 0 || S.roster.length === 0) continue;
          const e = S.roster.slice().sort((x, y) => x.star - y.star)[0];
          if (e.star >= 15) continue;
          // 시도 1회 비용은 항상 기대비용 이하(h[E] = cost/p ≥ cost)라, 기대비용을 여유 판정에 쓰면
          // 롤을 굴린 뒤 잔액이 모자라는 상황이 생기지 않는다(원장 음수 방지).
          if (money() - starExpectedCost(values, e.star, e.star + 1) < reserve) continue;
          const att = starAttempt(rng.star, values, e.star);
          post('Money', -att.cost, 'K4');
          e.star = att.newStar;
          bought = true;
          continue;
        }
        if (token.startsWith('building:')) {
          const axis = token.slice('building:'.length);
          // 조용히 건너뛰면 리네임 한 번에 프로필이 저투자 arm 으로 바뀌어도 티가 안 난다.
          // enhance.js findBuildingRow 와 같은 정책(loud fail)으로 맞춘다.
          const defRow = enhanceRows.get(axis);
          if (!defRow) throw new Error(`정책 투자축 '${axis}' 이 DT_BuildingEnhancementDefinition 에 없다 — 프로필(sim/policy.js) 또는 values.json 재추출 확인`);
          const b = cheapestBuildingFor(axis);
          const lv = enhLv(b, axis);
          if (defRow.MaxLevel > 0 && lv >= defRow.MaxLevel) continue; // enhance.js 는 상한 미클램프(호출자 책임)
          const { amount, currency } = buildingUpgradeCost(values, axis, lv);
          if (currency !== 'Money') continue; // Brick 축(BuildingFloor)은 이 스파인에서 미모델
          if (money() - amount < reserve) continue;
          post('Money', -amount, 'K3');
          b.enh.set(axis, lv + 1);
          bought = true;
        }
      }
      if (!bought) break;
    }
  }

  function shopPrice(name) {
    const row = shopRows.find(r => r.Name === name);
    return row ? { price: row.Price, limit: row.LimitCount, currency: row.Currency } : null;
  }
  function hire() {
    const cap = workstationCap(S.buildings[0].level);
    const want = policy.useSubEconomy({ roster: S.roster.length, rosterCap: cap, money: money() });
    const reserve = nextDevCost() * policy.reserveMult;
    const addEmployee = () => S.roster.push({ level: 1, exp: 0, star: 0, disciplinePts: [] });

    // 보유 채용권 우선 소진
    while (S.roster.length < cap && S.tickets.normal > 0) { S.tickets.normal--; addEmployee(); }
    while (S.roster.length < cap && S.tickets.advanced > 0) { S.tickets.advanced--; addEmployee(); }

    const normal = shopPrice('Daily_RecruitNormal');
    while (want.buyNormalTicket && normal && S.roster.length < cap
      && S.shop.normalToday < normal.limit && money() - normal.price >= reserve) {
      post('Money', -normal.price, 'K12');
      S.shop.normalToday++;
      addEmployee();
    }
    const adv = shopPrice('Weekly_RecruitAdv');
    while (want.buyAdvancedTicket && adv && S.roster.length < cap
      && S.shop.advancedThisWeek < adv.limit && money() - adv.price >= reserve) {
      post('Money', -adv.price, 'K12');
      S.shop.advancedThisWeek++;
      addEmployee();
    }
    while (want.diamondGacha && S.roster.length < cap) {
      const res = gachaPull(rng.gacha, S.gacha, values);
      const cost = res.diamondCost ?? 0; // mileageExchange 등 diamondCost 미보유 반환 대비
      if (cost > ledger.balance('Diamond')) break;
      post('Diamond', -cost, 'DK1');
      S.gacha = res.newState;
      addEmployee();
    }
  }

  function rollDailyWeeklyLimits() {
    const d = day();
    if (d !== S.shop.day) { S.shop.day = d; S.shop.normalToday = 0; }
    const w = Math.floor((d - 1) / 7);
    if (w !== S.shop.week) { S.shop.week = w; S.shop.advancedThisWeek = 0; }
  }

  function runSession() {
    onboarding();
    rollDailyWeeklyLimits();
    S.sessions++;
    cur.sessions++;
    S.inAppSec += sessionSec;
    cur.inAppMin += sessionSec / 60;
    collect();
    accrueOnline(sessionSec);
    runLaunches();
    // 채용이 강화보다 먼저 — invest() 는 잔액을 reserve 까지 훑어 쓰므로 순서가 뒤면 채용권 구매가
    // 자기 reserve 검사에 걸려 영구 기아 상태가 된다(로스터 인원 = 개발점수 공급의 주축인데 성장이 멈춤).
    hire();
    invest();
    S.timeSec += sessionSec;
  }

  // ── 메인 루프 ──────────────────────────────────────────────────────
  try {
    while (S.tier < untilTier && day() <= maxCalendarDays) {
      runSession();
      settleGap(gapSec);
    }
    ledger.assertConsistent();
  } catch (e) {
    invalid = `${e.constructor.name}: ${e.message}`;
  }
  if (cur) tiers.push(cur);

  const avgLevel = S.roster.length ? S.roster.reduce((a, e) => a + e.level, 0) / S.roster.length : 0;
  return {
    profile, seed, valuesHash: values.hash,
    tiers,
    ledger: ledger.toJSON(),
    gradeDist,
    endState: {
      reachedTier: S.tier,
      // 미완주(캘린더 캡 소진) 런의 inAppMin 은 "캡 × 세션수"라 완주 런과 같은 척도가 아니다 —
      // 시간 비교는 반드시 completed=true 만 모아서 할 것.
      completed: S.tier >= untilTier,
      calendarDay: day(),
      inAppMin: S.inAppSec / 60,
      sessions: S.sessions,
      money: money(),
      diamond: ledger.balance('Diamond'),
      launches: S.launches,
      firstClears: S.cleared.size,
      repeats: S.repeats,
      gateFails: S.gateFails,
      primaryBuildingLevel: S.buildings[0] ? S.buildings[0].level : 0,
      primaryBuildingExp: S.buildings[0] ? S.buildings[0].exp : 0,
      buildings: S.buildings.length,
      roster: S.roster.length,
      avgEmployeeLevel: avgLevel,
      hqLevel: S.hqLevel,
      vaultLostMoney: S.vaultLost,
      enhancements: S.buildings[0] ? Object.fromEntries(S.buildings[0].enh) : {},
    },
    ...(invalid ? { invalid } : {}),
  };
}

// PROJECTS_PER_TIER/INDUSTRY 는 구조 상수 — verify/scan.js 가 티어 밴드를 역산할 때 쓴다(중복 정의 방지).
module.exports = { runWorld, PROJECTS_PER_TIER, INDUSTRY };
