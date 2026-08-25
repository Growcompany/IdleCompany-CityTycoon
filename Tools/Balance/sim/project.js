// 프로젝트 개발점수/게이트/품질 몬테카를로 모델.
// Tools/Balance/gate_passrate.js (타 세션 미커밋 감사 스크립트, 읽기 전용 참고)의 로직을
// Values/calib 기반 데이터 주도로 이식 — 목표 배분/어피니티 가중 픽/히트당 점수/q 계산/게이트 판정은 그대로.

const DISCIPLINE_SLOTS = ['Weight_Plan', 'Weight_Dev', 'Weight_Graphics', 'Weight_Sound', 'Weight_Server', 'Weight_QA'];

// 직능 어피니티 — OfficeStageProgressManager.h:224 DisciplineAffinity(Points) 그대로 (포인트 1당 +0.1, 기본 0.5).
function affinityOf(pt) {
  return 0.5 + pt * 0.1;
}

function weightsOf(projectRow) {
  return DISCIPLINE_SLOTS.map(k => projectRow[k] ?? 0);
}

// 목표 배분 — StageProgressData.h:297-306 ComputeDisciplineTargetBase/ComputeDisciplineTarget 그대로 이식.
function disciplineTargets(values, projectRow) {
  const weights = weightsOf(projectRow);
  const maxW = Math.max(1, ...weights);
  const targetBase = Math.max(1, (projectRow.RequiredScore_Step1 + projectRow.RequiredScore_Step2 + projectRow.RequiredScore_Step3) / 3) * 2;
  return weights.map(w => (w > 0 ? (targetBase * w) / maxW : 0));
}

// gate_passrate.js: DPS_PER_EMP = 8.36 * 1.09 ("감사 유도식" — 유래 불명, 아마 이전 세션의 실측 캘리브레이션).
// OutputUnit×ScoreNormInterval 기준선(5.2×1.5=7.8)에 대한 배율로 환원해, 그 두 값이 리튠돼도 같이 스케일되게 한다.
// 계수 자체(≈1.1683)는 어피니티 재선택 가중 등 이 모델이 생략한 효과의 경험적 보정치 — Task12 calib 실측이 나오면 대체될 대상.
const DPS_AUDIT_CALIB_FACTOR = (8.36 * 1.09) / (5.2 * 1.5);
function deriveDevScorePerEmpPerSec(values) {
  return values.get('emp.outputUnit') * values.get('work.scoreNormInterval') * DPS_AUDIT_CALIB_FACTOR;
}

// 무탭 duty — values 가 아니라 calib 대상 파라미터(캘리브레이션 파일이 채우기 전 기본값).
const DEFAULT_NO_TAP_DUTY = 0.41;

// 게이트 클리어 기준 — 활성 직능 스텝이 각자 목표의 50% 이상 (StageProgressData.h:242-255 MeetsMinimumClearScore 취지).
// ⚠ 실제 프로덕션 코드는 스텝별 flat MinimumScore=60(가중 비례 아님)이라 완전히 동일하진 않음 — 이 감사 모델은
// gate_passrate.js 와 동일하게 "목표의 절반" 근사를 쓴다.
const GATE_MIN_CLEAR_RATIO = 0.5;

// q 캡 — 스텝별 달성률(got/target)을 이 값에서 자른다 (gate_passrate.js 동일).
const Q_CONTRIBUTION_CAP = 2.0;

// 최종 q 클램프 — StageProgressData.h:226-227 CalculateQualityScore() 반환 계약 그대로: FMath::Clamp(WeightedSum/WeightTotal, 0.5f, 2.0f).
// 주석 원문 "경제 절연 불변" — q가 이 구간 밖으로 못 나가야 다운스트림(보상/등급) 계산이 안전하다. 상한은 Q_CONTRIBUTION_CAP과 값이 같지만
// 계약상 별개(스텝별 캡은 got/target 각각을, 이 클램프는 가중평균 전체를 자름) — 이름을 분리해 둔다.
const Q_FLOOR = 0.5;
const Q_CEIL = Q_CONTRIBUTION_CAP;

function gradeOf(q, values) {
  const [sT, aT, bT] = values.get('quality.gradeThresholds');
  if (q >= sT) return 'S';
  if (q >= aT) return 'A';
  if (q >= bT) return 'B';
  return 'C';
}

// durationBonusSec — 부스트 도박의 시간 효과(TimeExtend +T / TimeCut −T). 실코드가 RemainingTime 을
//   **절대 가감**하므로(ResolveBoostGamble :1684, :1697) 개발 시간 자체가 늘/줄고 그만큼 기여도 따라간다.
// boostScoreFrac — 같은 함수의 점수 스윙(:1717-1740). 각 활성 스텝에 (TargetSum×Frac)/ActiveCount 를
//   **절대 가감**하고 0 에서 클램프한다. dps 곱셈이 아니다 — 곱셈 모델은 목표 대비 비율을 왜곡한다.
function simulateDevelopment(rng, values, calib, roster, projectRow, { tapDuty = 0, durationBonusSec = 0, boostScoreFrac = 0 } = {}) {
  const weights = weightsOf(projectRow);
  const targets = disciplineTargets(values, projectRow);
  const active = weights.map((w, i) => (w > 0 ? i : -1)).filter(i => i >= 0);
  const sumW = active.reduce((a, i) => a + weights[i], 0);

  const dps = calib.devScorePerEmpPerSec ?? deriveDevScorePerEmpPerSec(values);
  const duty = tapDuty > 0 ? tapDuty : (calib.noTapDuty ?? DEFAULT_NO_TAP_DUTY);
  const seconds = Math.max(0, projectRow.Duration + durationBonusSec); // TimeCut 은 FMath::Max(0,...) 클램프(:1697)
  // 케이던스(초당 히트 수) = 1 / BaseWorkInterval. 정규화가 ScoreNormInterval(케이던스와 분리된 고정 상수)로
  // 이뤄지므로 평균 총점은 이 값과 무관 — 히트 입자화(그래뉼래러티)만 좌우해 MC 분산에 영향을 준다.
  const hitsPerSec = 1 / values.get('work.baseInterval');

  const got = new Array(weights.length).fill(0);
  for (const emp of roster) {
    const pts = emp.disciplinePts;
    const nHits = Math.max(1, Math.round(hitsPerSec * seconds * duty));
    const cand = active.map(i => ({ i, w: affinityOf(pts[i] ?? 0) }));
    const totalW = cand.reduce((a, c) => a + c.w, 0);
    for (let h = 0; h < nHits; h++) {
      let roll = rng.next() * totalW;
      let acc = 0;
      let pick = cand[cand.length - 1];
      for (const c of cand) {
        acc += c.w;
        if (roll < acc) { pick = c; break; }
      }
      // 개당 점수 = DPS/히트수 (DPS 는 케이던스와 무관하게 보존됨) × 랜덤팩터(0.8~1.2, RandRange 그대로).
      got[pick.i] += ((dps * seconds * duty) / nHits) * (0.8 + 0.4 * rng.next());
    }
  }

  // 부스트 도박 점수 반영 — ResolveBoostGamble(:1717-1740) 그대로. 활성 스텝 목표합에 Frac 을 곱해
  // 활성 수로 균등 배분한 절대량을 각 스텝에 가감하고 0 에서 클램프한다(게이트/q 판정 **전**).
  if (boostScoreFrac !== 0 && active.length > 0) {
    const targetSum = active.reduce((a, i) => a + targets[i], 0);
    const perStep = (targetSum * boostScoreFrac) / active.length;
    for (const i of active) got[i] = Math.max(0, got[i] + perStep);
  }

  const passGate = active.every(i => got[i] >= targets[i] * GATE_MIN_CLEAR_RATIO);
  // WeightTotal<=0 폴백도 실제 코드(StageProgressData.h:226) 그대로: 활성 직능이 없으면 바로 Q_FLOOR.
  const qRaw = sumW > 0
    ? active.reduce((a, i) => a + Math.min(got[i] / targets[i], Q_CONTRIBUTION_CAP) * weights[i], 0) / sumW
    : Q_FLOOR;
  const q = Math.min(Q_CEIL, Math.max(Q_FLOOR, qRaw));
  const grade = gradeOf(q, values);
  return { passGate, q, grade, stepScores: got };
}

// deriveDevScorePerEmpPerSec 도 공개 — world.js 가 여기에 로스터 레벨/ProjectYield 배율을 곱해
// calib.devScorePerEmpPerSec 로 되먹이려면 기준선(1.1683 보정계수 포함)을 알아야 한다.
// DEFAULT_NO_TAP_DUTY 도 공개 — verify/calibrate.js 가 실측 전 예측 기준선으로 쓴다(값 복제 방지).
module.exports = { simulateDevelopment, gradeOf, disciplineTargets, deriveDevScorePerEmpPerSec, DEFAULT_NO_TAP_DUTY };
