// Task5 공유 산출물 — retune 확정 수치(브리프 기준)를 심은 Values 목.
// loadValues()를 임시 파일 경유로 재사용해 인터페이스(get/dt/hash)를 실제 반환값과 완전히 동일하게 보장한다.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { loadValues } = require('../../sim/values.js');

// DT_Project_Game 컬럼명은 Tools/Balance/out/values.json 실측 그대로(다른 이름이면 리임포트 시 조용히 깨짐).
// Weight_*=[2,4,3,2,0,2](기획/개발/그래픽/사운드/서버/QA), RequiredScore_Step1~3=18/18/19 → TargetBase 36.667,
// ΣTarget 119.17 ≈ gate_passrate.js 의 실측 SIGMA_TARGET 119.2 (§ ComputeDisciplineTargetBase 역산).
function makeGameProjectRow(idx, developmentCost) {
  return {
    Name: `Project${String(idx).padStart(3, '0')}`,
    CompanyType: 'Game',
    ProjectIndex: idx,
    ProjectName: `테스트프로젝트${idx}`,
    SubName: '',
    Icon: 'None',
    DropItemId: 'None',
    VariantKey: '아케이드',
    Genre: '아케이드',
    Material: '토이',
    Duration: 15,
    DevelopmentCost: developmentCost,
    RequiredScore_Step1: 18,
    RequiredScore_Step2: 18,
    RequiredScore_Step3: 19,
    RequiredScore_Step4: 10,
    Weight_Plan: 2,
    Weight_Dev: 4,
    Weight_Graphics: 3,
    Weight_Sound: 2,
    Weight_Server: 0,
    Weight_QA: 2,
  };
}

// Task10 — 월드 러너는 "티어 밴드(10개 프로젝트)"가 있어야 7첫클리어 게이트를 돌 수 있다.
// 기존 3행(idx 1/10/55)은 다른 테스트의 앵커라 손대지 않고, T1~T3 밴드(idx 2~9, 11~30)만 채운다.
// 요구치 곡선 = 실 DT_Project_Game 실측 비율(idx1 ≈ 10.7 → idx100 ≈ 6272, 즉 idx당 ×1.0664)을
// 그대로 쓴 기하곡선이라 idx1(10/12/10)·idx10(18/18/19) 실행과도 매끄럽게 이어진다.
const BAND_MATERIALS = ['토이', 'SF', '판타지'];
const BAND_GENRES = ['아케이드', '퍼즐', '캐주얼'];
function makeBandRow(idx) {
  const req = Math.round(11 * Math.pow(1.0664, idx - 1));
  return {
    ...makeGameProjectRow(idx, Math.max(700, 180 * idx * idx)),
    Genre: BAND_GENRES[idx % BAND_GENRES.length],
    VariantKey: BAND_GENRES[idx % BAND_GENRES.length],
    Material: BAND_MATERIALS[idx % BAND_MATERIALS.length],
    Duration: 15 + Math.floor((idx - 1) / 10),
    RequiredScore_Step1: req,
    RequiredScore_Step2: req,
    RequiredScore_Step3: req + 1,
    RequiredScore_Step4: req,
  };
}
function bandRows() {
  const out = [];
  for (let i = 2; i <= 30; i++) if (i !== 10) out.push(makeBandRow(i));
  return out;
}
// 궁합 등급 — 9개 조합에 S/A/B/C 를 순환 배정(정책의 궁합 선택 분기를 실제로 태우기 위함).
function affinityRows() {
  const grades = ['S', 'A', 'B', 'C'];
  const out = [];
  let n = 0;
  for (const g of BAND_GENRES) {
    for (const m of BAND_MATERIALS) {
      out.push({ Name: `Game_Aff_${++n}`, Industry: 'Game', Genre: g, Material: m, Grade: grades[n % grades.length], Image: 'None' });
    }
  }
  return out;
}

function buildRaw() {
  return {
    meta: { extractedAt: 'fake', srcHash: 'fake', editorHash: 'fake' },
    values: {
      'emp.outputUnit': { v: 5.2, src: 'src', loc: 'fake' },
      // Task7 — 실 out/values.json 추출값 그대로. levelFactorCurve=LevelGrowth(EmployeeTypes.cpp), wanderIncomeMult=GetIncomeMultiplier의 Wander case(EmployeeBehaviorComponent.cpp).
      'emp.idleIncomeScale': { v: 1.875, src: 'src', loc: 'fake' },
      'emp.levelFactorCurve': { v: 0.10, src: 'src', loc: 'fake' },
      'emp.wanderIncomeMult': { v: 0.5, src: 'src', loc: 'fake' },
      'idle.baseIncomePerSecond': { v: 1.0, src: 'cdo', loc: 'fake' },
      'offline.minSeconds': { v: 60, src: 'src', loc: 'fake' },
      'offline.progressRate': { v: 0.2, src: 'src', loc: 'fake' },
      'offline.capSeconds': { v: 43200, src: 'src', loc: 'fake' },
      'work.baseInterval': { v: 0.75, src: 'src', loc: 'fake' },
      'work.minInterval': { v: 0.2, src: 'src', loc: 'fake' },
      'work.scoreNormInterval': { v: 1.5, src: 'src', loc: 'fake' },
      'quality.gradeThresholds': { v: [1.625, 1.25, 0.875], src: 'src', loc: 'fake' },
      'gate.clearToUnlock': { v: 7, src: 'src', loc: 'fake' },
      'gate.reqBuildingLvPerTier': { v: 2, src: 'src', loc: 'fake' },
      // Task6 — 실 out/values.json 추출값 그대로(specs/2026-08-02-economy-map.md §2 대조 완료).
      'spread.band.S': { v: [1.10, 1.32], src: 'src', loc: 'fake' },
      'spread.band.A': { v: [0.88, 1.54], src: 'src', loc: 'fake' },
      'spread.band.B': { v: [0.62, 1.80], src: 'src', loc: 'fake' },
      'spread.band.C': { v: [0.38, 2.10], src: 'src', loc: 'fake' },
      'score.multCenter': { v: 1.25, src: 'src', loc: 'fake' },
      'score.multSlope': { v: 0.20, src: 'src', loc: 'fake' },
      'score.multMin': { v: 0.85, src: 'src', loc: 'fake' },
      'score.multMax': { v: 1.15, src: 'src', loc: 'fake' },
      'quality.gradeLifespanMin': { v: [1.80, 1.50, 1.25, 1.00], src: 'src', loc: 'fake' }, // [S,A,B,C]
      'trend.peakMult': { v: 1.25, src: 'src', loc: 'fake' },
      'op.baseRevenuePerProject': { v: 10, src: 'src', loc: 'fake' },
      'vault.baseCapacity': { v: 200, src: 'src', loc: 'fake' },
      // Task8 — 실 out/values.json 추출값 그대로(EmployeeManager.h:317/321, EmployeeManager.cpp:120-133 대조 완료).
      'emp.enhanceCostBase': { v: 2500, src: 'src', loc: 'fake' },
      'emp.enhanceCostGrowth': { v: 1.45, src: 'src', loc: 'fake' },
      'enhance.chanceBase': { v: 0.95, src: 'src', loc: 'fake' },
      'enhance.chanceStep': { v: 0.055, src: 'src', loc: 'fake' },
      'enhance.chanceMin': { v: 0.18, src: 'src', loc: 'fake' },
      // Task9 — 실 out/values.json 추출값 그대로.
      'diamond.codexTier': { v: 20, src: 'src', loc: 'fake' },
      'diamond.codexFull': { v: 200, src: 'src', loc: 'fake' },
      'gacha.softPityStart': { v: 40, src: 'src', loc: 'fake' },
      'gacha.hardPity': { v: 60, src: 'src', loc: 'fake' },
      'gacha.softPityBonus': { v: 0.02, src: 'src', loc: 'fake' },
      'gacha.advancedDiamondCost': { v: 30, src: 'src', loc: 'fake' },
      'gacha.premiumDiamondCost': { v: 50, src: 'src', loc: 'fake' },
      // Task9 fix — 리뷰 지적(마일리지 교환 200 미모델링) 반영, GachaRecruitmentData.h FGachaMileageData::ExchangeCost 그대로.
      'gacha.mileageExchangeCost': { v: 200, src: 'src', loc: 'fake' },
      // Task9 — 2026-08-02 신규 extract_src manifest 엔트리(CityCompanyData.h/BrickFactory.h) 그대로.
      'city.critChanceDefault': { v: 0.12, src: 'src', loc: 'fake' },
      'city.critMultDefault': { v: 4.0, src: 'src', loc: 'fake' },
      'brick.minAutoCollectionInterval': { v: 0.25, src: 'src', loc: 'fake' },
      // Task9 — ⚠미검증 placeholder: BuildingDataTable(모뉴먼트 FKeystoneAuraData.BasePassiveOutput/PassivePerLevel)은
      // 에디터 전용 DataTable이라 CSV 시드가 없고 extract_editor.py manifest에도 미등록 — 실측 불가(coverage.json S6
      // stubbed 사유). 닫힌식 자체(BasePassiveOutput+(Lv-1)×PassivePerLevel)만 검증하는 합성값.
      'monument.basePassiveOutput': { v: 50, src: 'unverified-placeholder', loc: 'fake' },
      'monument.passivePerLevel': { v: 20, src: 'unverified-placeholder', loc: 'fake' },
      // Task10 — EXP 상수(빌딩/직원). 전부 C++ 하드코딩이라 manifest_src.json 신규 등재분 실측값 그대로.
      // (OfficeStageProgressManager.cpp DistributeStepExperience/AwardLaunchEXP,
      //  ProjectOperationManager.cpp OnOperationComplete/DistributeOperationExperience,
      //  EmployeeManager.cpp GetMaxExperienceForLevel/GetQualityMultiplier)
      'exp.stepBase': { v: [20, 30, 25], src: 'src', loc: 'fake' },
      'exp.launchBuildingBase': { v: 50, src: 'src', loc: 'fake' },
      'exp.launchBuildingGradeBonus': { v: [10, 25, 50, 100], src: 'src', loc: 'fake' }, // [C,A,B,S] 순서 아님 — [C,B,A,S]
      'exp.operationBuildingMin': { v: 50, src: 'src', loc: 'fake' },
      'exp.operationBuildingRevenueRate': { v: 0.01, src: 'src', loc: 'fake' },
      'exp.operationEmployeeBase': { v: 100, src: 'src', loc: 'fake' },
      'exp.qualityMult': { v: [0.3, 0.5, 0.8, 1.0, 1.3, 1.5], src: 'src', loc: 'fake' }, // [F,D,C,B,A,S]
      'emp.levelExpBase': { v: 100, src: 'src', loc: 'fake' },
      'emp.levelExpCoef': { v: 1.5, src: 'src', loc: 'fake' },
    },
    dt: {
      DT_Project_Game: [
        {
          Name: 'Project001',
          CompanyType: 'Game',
          ProjectIndex: 1,
          ProjectName: '탭탭코인',
          SubName: '누를수록 쌓이는 동전',
          Icon: '/Game/CompanyGrowth/UI/Textures/ProjectCover/T_ProjectCover_101.T_ProjectCover_101',
          DropItemId: 'None',
          VariantKey: '아케이드',
          Genre: '아케이드',
          Material: '토이',
          Duration: 15,
          DevelopmentCost: 700,
          RequiredScore_Step1: 18,
          RequiredScore_Step2: 18,
          RequiredScore_Step3: 19,
          RequiredScore_Step4: 10,
          Weight_Plan: 2,
          Weight_Dev: 4,
          Weight_Graphics: 3,
          Weight_Sound: 2,
          Weight_Server: 0,
          Weight_QA: 2,
        },
        // Task6 회수율 재현용 — 브리핑 지정 개발비 곡선 max(700, 180·idx²) (project_balance_redesign 개발비 N² 계열).
        makeGameProjectRow(10, Math.max(700, 180 * 10 * 10)),
        makeGameProjectRow(55, Math.max(700, 180 * 55 * 55)),
        ...bandRows(), // Task10 — T1~T3 밴드 채움(위 makeBandRow 주석 참고)
      ],
      // Task10 — 궁합/빌딩레벨/상점/도박 DT. 월드 러너가 소비하는 최소 집합.
      DT_ProjectAffinity: affinityRows(),
      // 실 out/values.json DT_BuildingLevel 앞 12행 그대로(RequiredEXP=누적, MaxWorkstations=Lv+1).
      DT_BuildingLevel: [
        { Name: '1', RequiredEXP: 0, MaxWorkstations: 2, UnlockedEnhancements: ['BuildingFloor', 'MarketingPower'] },
        { Name: '2', RequiredEXP: 100, MaxWorkstations: 3, UnlockedEnhancements: [] },
        { Name: '3', RequiredEXP: 250, MaxWorkstations: 4, UnlockedEnhancements: ['EmployeeGrowth'] },
        { Name: '4', RequiredEXP: 450, MaxWorkstations: 5, UnlockedEnhancements: [] },
        { Name: '5', RequiredEXP: 700, MaxWorkstations: 6, UnlockedEnhancements: ['VaultCapacity'] },
        { Name: '6', RequiredEXP: 1000, MaxWorkstations: 7, UnlockedEnhancements: [] },
        { Name: '7', RequiredEXP: 1400, MaxWorkstations: 8, UnlockedEnhancements: [] },
        { Name: '8', RequiredEXP: 1900, MaxWorkstations: 9, UnlockedEnhancements: ['ProjectYield'] },
        { Name: '9', RequiredEXP: 2500, MaxWorkstations: 10, UnlockedEnhancements: [] },
        { Name: '10', RequiredEXP: 3200, MaxWorkstations: 11, UnlockedEnhancements: ['MarketCapMultiplier'] },
        { Name: '11', RequiredEXP: 4000, MaxWorkstations: 12, UnlockedEnhancements: [] },
        { Name: '12', RequiredEXP: 5000, MaxWorkstations: 13, UnlockedEnhancements: ['ProjectGrade'] },
      ],
      // 실 out/values.json DT_ShopItem 중 Money 채용권 2종(로스터 성장의 유일한 Money 경로).
      DT_ShopItem: [
        { Name: 'Daily_RecruitNormal', Tab: 'Daily', Item: 'RecruitTicketNormal', Quantity: 1, Currency: 'Money', Price: 30000, LimitCount: 2, SortOrder: 10 },
        { Name: 'Weekly_RecruitAdv', Tab: 'Weekly', Item: 'RecruitTicketAdvanced', Quantity: 1, Currency: 'Money', Price: 300000, LimitCount: 2, SortOrder: 10 },
      ],
      // 실 out/values.json DT_BoostGamble[Game] 행 그대로.
      DT_BoostGamble: [
        { Name: 'Game', Industry: 'Game', EffectType: 'TimeExtend', SuccessChance: 0.6, SuccessFrac: 0.22, FailFrac: 0.16, TimeSeconds: 4, GoCost: 500 },
      ],
      // Task6 — 실 out/values.json DT_IndustryProfile[Game] 행 그대로(Volatility=0.15 <1, 조용한 근사 금지 규칙 준수 확인용).
      DT_IndustryProfile: [
        {
          Name: 'Game',
          Industry: 'Game',
          DevCostPips: 2,
          PeakPips: 2,
          TailPips: 0,
          VolatilityPips: 1,
          DevCostMult: 2,
          PeakMult: 2.2,
          HalfLifeFrac: 0.28,
          Volatility: 0.15,
          ReviewLeverage: 1.5,
          CommissionAppeal: 0.5,
        },
      ],
      // Task6 — 실 out/values.json DT_BuildingEnhancementDefinition[VaultCapacity] 행 그대로.
      // Task8 — BuildingFloor(가산형·Brick 화폐, 커런시 분기 검증용) 추가.
      // MarketCapMultiplier 는 시총 상한 어서션(verify/assertions.js)이 이 행을 필수로 찾는다.
      // 네 행 전부 실 out/values.json 그대로(리임포트 시 조용히 깨지지 않도록 컬럼명 대조 완료).
      DT_BuildingEnhancementDefinition: [
        {
          Name: 'VaultCapacity',
          EnhancementType: 'VaultCapacity',
          Category: 'Common',
          ValueUnit: '시간',
          bIsInteger: false,
          SortOrder: 80,
          CostResourceType: 'Money',
          BaseCost: 800,
          CostGrowthRate: 1.003,
          EffectPerLevel: 0.000625,
          MaxLevel: 0,
        },
        {
          Name: 'BuildingFloor',
          EnhancementType: 'BuildingFloor',
          Category: 'Common',
          ValueUnit: '층',
          bIsInteger: true,
          SortOrder: 10,
          CostResourceType: 'Brick',
          BaseCost: 1000,
          CostGrowthRate: 1.15,
          EffectPerLevel: 1,
          MaxLevel: 0,
        },
        {
          Name: 'MarketCapMultiplier',
          EnhancementType: 'MarketCapMultiplier',
          Category: 'Common',
          ValueUnit: '배',
          bIsInteger: false,
          SortOrder: 120,
          CostResourceType: 'Money',
          BaseCost: 2000,
          CostGrowthRate: 1.005,
          EffectPerLevel: 0.0005,
          MaxLevel: 5000,
        },
        // policy.js 프로필이 투자하는 축은 전부 여기 있어야 한다 — 빠지면 world.js 가 loud fail 한다.
        // (구 continue 폴백 시절엔 픽스처가 프로덕션과 다른 투자셋으로 조용히 돌고 있었다.)
        {
          Name: 'MarketingPower',
          EnhancementType: 'MarketingPower',
          Category: 'Common',
          ValueUnit: '원',
          bIsInteger: true,
          SortOrder: 11,
          CostResourceType: 'Money',
          BaseCost: 1000,
          CostGrowthRate: 1.004,
          EffectPerLevel: 0.0005,
          MaxLevel: 6000,
        },
        {
          Name: 'ProjectYield',
          EnhancementType: 'ProjectYield',
          Category: 'Common',
          ValueUnit: '배',
          bIsInteger: false,
          SortOrder: 100,
          CostResourceType: 'Money',
          BaseCost: 1000,
          CostGrowthRate: 1.004,
          EffectPerLevel: 0.0004,
          MaxLevel: 6000,
        },
        {
          Name: 'ProjectGrade',
          EnhancementType: 'ProjectGrade',
          Category: 'Common',
          ValueUnit: '배',
          bIsInteger: false,
          SortOrder: 140,
          CostResourceType: 'Money',
          BaseCost: 1400,
          CostGrowthRate: 1.004,
          EffectPerLevel: 0.0004,
          MaxLevel: 6000,
        },
      ],
      // Task8 — 실 docs/Data/DT_HQLevel.csv(=out/values.json DT_HQLevel) 앞 5행 그대로.
      DT_HQLevel: [
        { Name: '1', MoneyCost: 0, RequiredBuildingCount: 0, RequiredBuildingLevel: 0, RequiredEmployeeCount: 0, RequiredMaxStageCleared: 0, RequiredMarketCap: 0, UnlockDescription: '게임 시작' },
        { Name: '2', MoneyCost: 500, RequiredBuildingCount: 0, RequiredBuildingLevel: 0, RequiredEmployeeCount: 0, RequiredMaxStageCleared: 0, RequiredMarketCap: 0, UnlockDescription: 'IT 산업 해금' },
        { Name: '3', MoneyCost: 1500, RequiredBuildingCount: 1, RequiredBuildingLevel: 0, RequiredEmployeeCount: 0, RequiredMaxStageCleared: 0, RequiredMarketCap: 0, UnlockDescription: '빌딩 3개 해금' },
        { Name: '4', MoneyCost: 3000, RequiredBuildingCount: 1, RequiredBuildingLevel: 0, RequiredEmployeeCount: 3, RequiredMaxStageCleared: 0, RequiredMarketCap: 0, UnlockDescription: '금융 산업 해금' },
        { Name: '5', MoneyCost: 6000, RequiredBuildingCount: 2, RequiredBuildingLevel: 2, RequiredEmployeeCount: 5, RequiredMaxStageCleared: 0, RequiredMarketCap: 0, UnlockDescription: '빌딩 5개 해금' },
      ],
      // Task9 — 실 out/values.json DT_MarketBalance[Default] 행 그대로.
      DT_MarketBalance: [
        { Name: 'Default', BaseMul: 0.02, Coef: 0.6, PivotMC: 50000, MaxMul: 3 },
      ],
      // Task9 — 실 out/values.json DT_TradeOrderBalance[Default] 행 그대로(VIP_DiamondMin/Max만 diamondBudget 소비).
      DT_TradeOrderBalance: [
        {
          Name: 'Default', TierWeightBase: 2, MaxActive_Normal: 4, MaxActive_Urgent: 2, MaxActive_VIP: 1, MaxAcceptedSlots: 3,
          Normal_QtyMin: 10, Normal_QtyMax: 30, Normal_RewardMultMin: 1.3, Normal_RewardMultMax: 1.8,
          Normal_DurationHoursMin: 6, Normal_DurationHoursMax: 12,
          Urgent_QtyMin: 20, Urgent_QtyMax: 50, Urgent_RewardMultMin: 2, Urgent_RewardMultMax: 3,
          Urgent_DurationHoursMin: 1, Urgent_DurationHoursMax: 3,
          VIP_QtyMin: 3, VIP_QtyMax: 10, VIP_RewardMultMin: 3, VIP_RewardMultMax: 4, VIP_DurationHours: 24,
          VIP_DiamondMin: 30, VIP_DiamondMax: 80,
          ComboMul_2: 1.2, ComboMul_3: 1.5, ComboMul_4: 1.8, ComboMul_5Plus: 2,
        },
      ],
      // Task9 — 실 out/values.json DT_Mission 19행 그대로(온보딩 체인 M1~M21, Rewards는 실 ExportText 문자열 형식).
      // Diamond 보상 행만 diamondBudget().missionTotal 합산 대상 — M9/M10/M11/M12/M13/M21 = 100+50+50+50+100+100=450.
      DT_Mission: [
        { Name: 'M1_CollectBricks', ConditionType: 'CollectBricks', ConditionAmount: 50, Rewards: ['(ResourceType=Brick,Amount=50,ItemType=None,ItemAmount=0)'], NextMissionID: 'M2_BuildFirstCompany', bAutoClaim: false },
        { Name: 'M2_BuildFirstCompany', ConditionType: 'BuildFirstCompany', ConditionAmount: 1, Rewards: ['(ResourceType=Brick,Amount=30,ItemType=None,ItemAmount=0)', '(ResourceType=Money,Amount=5000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M3_EnterOffice', bAutoClaim: false },
        { Name: 'M3_EnterOffice', ConditionType: 'EnterOffice', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=1000,ItemType=None,ItemAmount=0)', '(ResourceType=None,Amount=0,ItemType=RecruitTicketNormal,ItemAmount=1)'], NextMissionID: 'M4_FirstRecruit', bAutoClaim: false },
        { Name: 'M4_FirstRecruit', ConditionType: 'FirstRecruit', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=5000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M7_PlaceDesk', bAutoClaim: false },
        { Name: 'M7_PlaceDesk', ConditionType: 'PlaceDesk', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=2000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M8_PlacePainting', bAutoClaim: false },
        { Name: 'M8_PlacePainting', ConditionType: 'PlacePainting', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=2000,ItemType=None,ItemAmount=0)', '(ResourceType=None,Amount=0,ItemType=RecruitTicketNormal,ItemAmount=1)'], NextMissionID: 'M8b_StaffNewDesk', bAutoClaim: false },
        { Name: 'M8b_StaffNewDesk', ConditionType: 'StaffNewDesk', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=4000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M10_InHouseProject', bAutoClaim: false },
        { Name: 'M9_EnhanceStat', ConditionType: 'EnhanceStat', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=6000,ItemType=None,ItemAmount=0)', '(ResourceType=Diamond,Amount=100,ItemType=None,ItemAmount=0)'], NextMissionID: 'M10b_CollectRevenue', bAutoClaim: false },
        { Name: 'M10_InHouseProject', ConditionType: 'InHouseProject', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=5000,ItemType=None,ItemAmount=0)', '(ResourceType=Diamond,Amount=50,ItemType=None,ItemAmount=0)', '(ResourceType=None,Amount=0,ItemType=BuildingTraitTicketNormal,ItemAmount=1)'], NextMissionID: 'M9_EnhanceStat', bAutoClaim: false },
        { Name: 'M10b_CollectRevenue', ConditionType: 'CollectRevenue', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=3000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M11_ExitAndTrait', bAutoClaim: false },
        { Name: 'M11_ExitAndTrait', ConditionType: 'ExitAndTrait', ConditionAmount: 1, Rewards: ['(ResourceType=Diamond,Amount=50,ItemType=None,ItemAmount=0)', '(ResourceType=None,Amount=0,ItemType=SkinTicketNormal,ItemAmount=1)'], NextMissionID: 'M12_EquipSkin', bAutoClaim: false },
        { Name: 'M12_EquipSkin', ConditionType: 'EquipSkin', ConditionAmount: 1, Rewards: ['(ResourceType=Diamond,Amount=50,ItemType=None,ItemAmount=0)', '(ResourceType=Brick,Amount=10000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M13_RaiseFloor', bAutoClaim: false },
        { Name: 'M13_RaiseFloor', ConditionType: 'RaiseFloor', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=10000,ItemType=None,ItemAmount=0)', '(ResourceType=Diamond,Amount=100,ItemType=None,ItemAmount=0)'], NextMissionID: 'M14_EnhanceBuildingMoney', bAutoClaim: false },
        { Name: 'M14_EnhanceBuildingMoney', ConditionType: 'EnhanceBuildingMoney', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=2000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M15_LevelUpHQ', bAutoClaim: false },
        { Name: 'M15_LevelUpHQ', ConditionType: 'LevelUpHQ', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=3000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M17_BuildSecondCompany', bAutoClaim: false },
        { Name: 'M17_BuildSecondCompany', ConditionType: 'BuildSecondCompany', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=5000,ItemType=None,ItemAmount=0)', '(ResourceType=None,Amount=0,ItemType=RecruitTicketNormal,ItemAmount=1)'], NextMissionID: 'M19_AcquireCompany', bAutoClaim: false },
        { Name: 'M19_AcquireCompany', ConditionType: 'AcquireCompany', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=5000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M20_DemolishCompany', bAutoClaim: false },
        { Name: 'M20_DemolishCompany', ConditionType: 'DemolishCompany', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=10000,ItemType=None,ItemAmount=0)'], NextMissionID: 'M21_BuildOnClearedPlot', bAutoClaim: false },
        { Name: 'M21_BuildOnClearedPlot', ConditionType: 'BuildOnClearedPlot', ConditionAmount: 1, Rewards: ['(ResourceType=Money,Amount=20000,ItemType=None,ItemAmount=0)', '(ResourceType=Diamond,Amount=100,ItemType=None,ItemAmount=0)'], NextMissionID: 'None', bAutoClaim: false },
      ],
      // Task9 — 실 out/values.json DT_MineUpgradeDefinition 5행 그대로.
      DT_MineUpgradeDefinition: [
        { Name: 'LineExpansion', UpgradeType: 'LineExpansion', ValueUnit: '라인', bIsInteger: true, SortOrder: 5, CostResourceType: 'Money', MaxLevel: 4, BaseValue: 1, PerLevel: 1, BaseCost: 5000, CostGrowthRate: 2 },
        { Name: 'MiningRate', UpgradeType: 'MiningRate', ValueUnit: '%', bIsInteger: false, SortOrder: 10, CostResourceType: 'Money', MaxLevel: 0, BaseValue: 0, PerLevel: 0.15, BaseCost: 100, CostGrowthRate: 1.1 },
        { Name: 'MiningStorage', UpgradeType: 'MiningStorage', ValueUnit: '개', bIsInteger: true, SortOrder: 20, CostResourceType: 'Money', MaxLevel: 0, BaseValue: 100, PerLevel: 100, BaseCost: 80, CostGrowthRate: 1.1 },
        { Name: 'OfflineCatchupBoost', UpgradeType: 'OfflineCatchupBoost', ValueUnit: '%', bIsInteger: false, SortOrder: 40, CostResourceType: 'Money', MaxLevel: 0, BaseValue: 0, PerLevel: 0.005, BaseCost: 500, CostGrowthRate: 1.15 },
        { Name: 'AutoClaim', UpgradeType: 'AutoClaim', ValueUnit: '', bIsInteger: true, SortOrder: 50, CostResourceType: 'Money', MaxLevel: 1, BaseValue: 0, PerLevel: 1, BaseCost: 5000, CostGrowthRate: 1 },
      ],
      // Task9 — 실 out/values.json DT_CountryInfo 중 무역 테스트에 쓰는 2행(허브 USA / 비허브 Korea) 그대로.
      DT_CountryInfo: [
        { Name: 'USA', CountryType: 'USA', bSupportsFactory: true, bSupportsMine: false, bSupportsTrade: true, bIsTradeHub: true, MoneyBias: 1, MarketCapBias: 1, SupportedIndustries: ['Electronics', 'Automobile', 'Semiconductor'], MinableResources: [] },
        { Name: 'Korea', CountryType: 'Korea', bSupportsFactory: true, bSupportsMine: false, bSupportsTrade: true, bIsTradeHub: false, MoneyBias: 1, MarketCapBias: 1.3, SupportedIndustries: ['Semiconductor'], MinableResources: [] },
      ],
      // Task9 — 실 out/values.json DT_CountryDemand 중 위 2국가×Semiconductor 행 그대로.
      DT_CountryDemand: [
        { Name: 'USA_Semiconductor', Country: 'USA', Industry: 'Semiconductor', Capacity: 25000, RecoveryPerMin: 167, PriceMul: 1.35 },
        { Name: 'Korea_Semiconductor', Country: 'Korea', Industry: 'Semiconductor', Capacity: 10000, RecoveryPerMin: 125, PriceMul: 1.3 },
      ],
    },
  };
}

// Task10 등 파일 형태 소비용 — 실제 loadValues()가 읽는 것과 동일한 raw JSON 포맷으로 덤프.
function writeFixture(filePath) {
  fs.writeFileSync(filePath, JSON.stringify(buildRaw(), null, 2));
  return filePath;
}

function fakeValues() {
  const p = path.join(os.tmpdir(), `balance_fake_values_${process.pid}_${Date.now()}_${Math.random().toString(36).slice(2)}.json`);
  writeFixture(p);
  const V = loadValues(p);
  fs.unlinkSync(p);
  return V;
}

module.exports = { fakeValues, writeFixture };
