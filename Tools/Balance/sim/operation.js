// 운영 수익 닫힌식 + 금고 — ProjectOperationManager.cpp(:138,:326,:748-926) 이식.
// 감쇠(:330)는 결정론적 지수감쇠라 몬테카를로 없이 닫힌형 정적분으로 계산 가능(0~T).
// 변동성(sin, :336)은 25초 주기 결정론적 진동 — 전체 정수 주기에 대한 평균은 1.0.
// Volatility>=1 이면 max(0,...) 클리핑이 걸려 평균이 1.0에서 벗어나는데, 이 모델은 그 보정항을
// 구현하지 않았으므로 조용히 근사하지 않고 명시적으로 에러를 던진다(브리핑 지시).

function clamp(v, lo, hi) {
  return Math.min(hi, Math.max(lo, v));
}

function getIndustryProfile(values, industry) {
  const rows = values.dt('DT_IndustryProfile');
  const row = rows.find(r => r.Industry === industry || r.Name === industry);
  if (!row) throw new Error(`DT_IndustryProfile 누락 행: ${industry}`);
  return row;
}

function getProjectRow(values, industry, projectIdx) {
  const rows = values.dt(`DT_Project_${industry}`);
  const row = rows.find(r => r.ProjectIndex === projectIdx);
  if (!row) throw new Error(`DT_Project_${industry} 누락 ProjectIndex: ${projectIdx}`);
  return row;
}

// ComputePeakSpreadMult(ProjectOperationManager.cpp:786) 이식.
// 궁합 등급 없음(Grade.IsNone())이면 스프레드 무효(1.0) 그대로.
// leverage(=DT_IndustryProfile.ReviewLeverage)는 산업마다 다른 값(Game 1.5/IT 0.6/...)이라 필수 인자로
// 강제한다 — 조용한 기본값을 두면 산업을 안 넘긴 호출이 엉뚱한 산업 값으로 30%+ 틀린 결과를 무경고 반환한다
// (코드 리뷰 지적, DT_IndustryProfile에 이미 여러 산업 행이 존재해 현재도 활성 함정이었음).
function spreadMult(values, affinityGrade, q, leverage) {
  if (leverage === undefined || leverage === null) {
    throw new Error('spreadMult: leverage 필수 — 산업별 ReviewLeverage를 호출자가 해석해서 전달');
  }
  if (!affinityGrade) return 1.0;
  const [lo, hi] = values.get(`spread.band.${affinityGrade}`);
  const qNorm = clamp((clamp(q, 0.5, 2.0) - 0.5) / 1.5, 0, 1);
  const band = lo + (hi - lo) * qNorm;
  return Math.max(0.1, 1 + leverage * (band - 1));
}

// CalculateScoreMultiplier(ProjectOperationManager.cpp:776) 이식.
function scoreMult(values, q) {
  const center = values.get('score.multCenter');
  const slope = values.get('score.multSlope');
  const min = values.get('score.multMin');
  const max = values.get('score.multMax');
  return clamp(1 + (q - center) * slope, min, max);
}

// quality.gradeLifespanMin 추출 순서 = [S,A,B,C] (QualityGrade.h:122-125 GetQualityGradeBaseLifespanMinutes 그대로).
const GRADE_LIFESPAN_ORDER = ['S', 'A', 'B', 'C'];
function gradeLifespanMinutes(values, grade) {
  const arr = values.get('quality.gradeLifespanMin');
  const idx = GRADE_LIFESPAN_ORDER.indexOf(grade);
  if (idx < 0) throw new Error(`알 수 없는 등급: ${grade}`);
  return arr[idx];
}

// 감쇠 적분 닫힌식: ∫[0,T] 0.5^(t/(T·HLF)) dt = T·HLF/ln2 · (1 − 0.5^(1/HLF))
// (ProjectOperationManager.cpp:330 DecayHalfLife = max(1, T×HLF) 대상 지수감쇠의 정적분 — T×HLF>=1 전제,
// 이 시뮬 범위의 idx/등급수명 조합에선 항상 성립해 max(1,·) 클램프는 반영하지 않음)
function decayIntegral(totalSeconds, halfLifeFrac) {
  return (totalSeconds * halfLifeFrac / Math.LN2) * (1 - Math.pow(0.5, 1 / halfLifeFrac));
}

// BaseRevenuePerSecond(:138)~ActualRevenuePerSecond(:326) 감쇠 적분을 닫힌식으로 직접 계산.
// 변동성은 전 주기 평균 1.0으로 처리(위 주석 참고) — Volatility>=1 은 즉시 에러.
function operationTotals(values, projectIdx, ctx) {
  // eventRewardMult = ProgressData.EventRewardMultiplier (부스트 도박 PayoffGamble 결과). economy-map
  // 운영 공식대로 BaseRevenuePerSecond 단에 곱한다. 미지정이면 1.0(도박 미실행/다른 EffectType).
  const { grade, q, affinityGrade, trendMatched, buildingIncomeMult, statBonus, industry, eventRewardMult = 1 } = ctx;
  const profile = getIndustryProfile(values, industry);
  if (!(profile.Volatility < 1)) {
    throw new Error(`DT_IndustryProfile[${industry}].Volatility(${profile.Volatility}) >= 1 — sin 클리핑으로 주기 평균이 1.0이 아니게 되어 이 닫힌식 모델로 근사 불가`);
  }

  const trendMult = trendMatched ? values.get('trend.peakMult') : 1.0;
  const spread = spreadMult(values, affinityGrade, q, profile.ReviewLeverage);
  const baseRatePerSec = projectIdx * values.get('op.baseRevenuePerProject') * eventRewardMult * profile.PeakMult * trendMult * spread;

  const score = scoreMult(values, q);
  const stableRatePerSec = baseRatePerSec * score * buildingIncomeMult * statBonus;

  const totalSeconds = gradeLifespanMinutes(values, grade) * projectIdx * 60;
  const totalRevenue = stableRatePerSec * decayIntegral(totalSeconds, profile.HalfLifeFrac);

  return { totalRevenue, totalSeconds, baseRatePerSec, stableRatePerSec };
}

// CalculateVaultSeconds(BuildingEnhancementData.cpp:168) 이식: Lv0=2h 고정, 이후 k=DT VaultCapacity
// EffectPerLevel(폴백 0.005는 프로덕션 DT 미초기화 대비 — 시뮬은 DT 행 필수, 누락 시 loud fail)로 점근 12h.
const VAULT_BASE_HOURS = 2;
const VAULT_MAX_BONUS_HOURS = 10;
function vaultSeconds(values, vaultLv) {
  if (vaultLv <= 0) return 3600 * VAULT_BASE_HOURS;
  const row = values.dt('DT_BuildingEnhancementDefinition').find(
    r => r.EnhancementType === 'VaultCapacity' || r.Name === 'VaultCapacity'
  );
  if (!row) throw new Error('DT_BuildingEnhancementDefinition 누락 행: VaultCapacity');
  const k = row.EffectPerLevel;
  return 3600 * (VAULT_BASE_HOURS + VAULT_MAX_BONUS_HOURS * (1 - Math.exp(-vaultLv * k)));
}

// CalculateWarehouseCapacity(ProjectOperationManager.cpp:888) 이식 — StableRate×VaultSeconds, 하한 vault.baseCapacity.
// (StableRate<=0 이면 TimeCapacity=0 이라 max(0,floor)=floor 로 자연히 하한과 일치 — 별도 분기 불필요)
function vaultCapacity(values, stableRatePerSec, vaultLv) {
  const floor = values.get('vault.baseCapacity');
  return Math.max(stableRatePerSec * vaultSeconds(values, vaultLv), floor);
}

// 회수율 = 총수익 / 개발비 (DT행 DevelopmentCost).
function recoveryRatio(values, projectIdx, ctx) {
  const { totalRevenue } = operationTotals(values, projectIdx, ctx);
  const row = getProjectRow(values, ctx.industry, projectIdx);
  return totalRevenue / row.DevelopmentCost;
}

module.exports = { spreadMult, scoreMult, operationTotals, vaultCapacity, recoveryRatio };
