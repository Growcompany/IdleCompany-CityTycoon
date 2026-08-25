// 플레이어 정책 4프로필 — specs/2026-07-29-balance-grind-retune.md §3 계승.
// ⚠ 이 파일의 숫자는 전부 "플레이어 행동" 파라미터다(세션 길이/탭 duty/투자 우선순위/궁합 취향).
// 게임 밸런스 수치는 하나도 없다 — 그건 전부 values.json 경유(world.js)로만 읽는다.
//
// 등급 mix(C/B/A/S)는 강제 배정이 아니다. 실제 등급은 개발 시뮬(q)에서 자연 발생하고,
// 정책은 그걸 **탭 duty·로스터 투자·궁합 선택**으로 유도만 한다. targetGradeMix()는
// retune §3 표의 "그 프로필이 지향하는 분포"로, 실현 분포(RunResult.gradeDist)와 대조하는 기준선이다.

// 세션 모델 — retune §3 "세션 7.5분 × 하루 3.5회". values 가 아니라 정책 상수(브리프 지시).
const SESSION_MINUTES = 7.5;
const SESSIONS_PER_DAY = 3.5;
// 착수 1건당 개발 시간(=DT Duration) 외에 드는 UI 조작 시간(피드 열람/착수/결산/수령).
// 실측 없음 — retune 이 "순조작 시간"을 판수의 함수로만 잡은 것과 같은 수준의 가정치.
const LAUNCH_OVERHEAD_SEC = 20;

// 탭 duty 상한 — retune §7-1 의 개발 앵커(무탭 123.2 / 완벽탭 216)와 project.js 의 무탭 duty 0.41 을
// 연결하면 완벽탭 duty = 0.41 × 216/123.2 ≈ 0.72 다. 프로필 duty 는 이 천장 아래에 둔다.
// ⚠ 두 앵커 모두 PIE 실측이 없는 값(retune §7-1 최대 불확실성) — Task12 캘리브레이션의 1순위 교체 대상.

// 병렬 운영 슬롯 P — retune §3 가정(T1~2:1 / T3~4:2 / T5~6:3 / T7~8:4 / T9~10:5).
// ⚠ 실제 상한(건물 Brick 건설비·HQ RequiredBuildingCount)은 retune §7-7 미확인 항목 — 여기서도 미모델.
function slotsForTier(tier) {
  return Math.min(5, Math.floor((tier + 1) / 2));
}

const PROFILES = {
  // 순수 C — 무탭(project.js 의 calib.noTapDuty 사용), 최소 투자, 안전 궁합만.
  low: {
    tapDuty: 0,
    feedSize: 2,
    affinityPreference: ['S', 'A', 'B', 'C'], // 낮은 q + 넓은 밴드 = 재앙 → 좁은 밴드(S/A) 선호
    // 단일 축 = "최소 투자" arm 의 정의. 오프라인 정산률이 상수라 저접속 플레이어의
    // 방치 수익을 실제로 묶는 건 금고 상한이므로 그 축을 대표로 둔다.
    invest: ['building:VaultCapacity'],
    reserveMult: 1.2,
    boostGamble: false,
    // 채용은 "투자"가 아니라 진행 필수축(로스터 인원이 개발점수 공급의 주축)이라 최소투자 프로필도 한다.
    // 단 Money 상점 일반권까지만 — Diamond 가챠/고급권은 안 씀.
    hire: { shopNormal: true, shopAdvanced: false, diamondGacha: false },
    gradeMix: { C: 1.00, B: 0, A: 0, S: 0 },
  },
  mid: {
    tapDuty: 0.52,
    feedSize: 3,
    affinityPreference: ['A', 'B', 'S', 'C'],
    invest: ['building:VaultCapacity', 'building:MarketingPower', 'building:ProjectYield'],
    reserveMult: 2,
    boostGamble: false,
    hire: { shopNormal: true, shopAdvanced: false, diamondGacha: true },
    gradeMix: { C: 0.65, B: 0.22, A: 0.13, S: 0 },
  },
  // 진단용 단일변수 프로필 — mid 와 **duty 만** 다르다(투자셋/궁합/채용/reserve 전부 mid 동일).
  // Task10 리뷰 F3: "무탭이라 완주 못 한다"와 "저투자라 완주 못 한다"가 low 프로필에서 교락돼 있었다.
  // low = 무탭 + 저투자셋이라 두 변수가 동시에 다르다 → 이 프로필이 duty 축을 단독 분리한다.
  // (mid 대비 결과 차이 = 순수 탭 효과. 투자셋 축은 low 와 mid 의 차이가 담당.)
  midNoTap: {
    tapDuty: 0,
    feedSize: 3,
    affinityPreference: ['A', 'B', 'S', 'C'],
    invest: ['building:VaultCapacity', 'building:MarketingPower', 'building:ProjectYield'],
    reserveMult: 2,
    boostGamble: false,
    hire: { shopNormal: true, shopAdvanced: false, diamondGacha: true },
    gradeMix: { C: 1.00, B: 0, A: 0, S: 0 }, // 진단 arm — 관심사는 실현 분포(gradeDist)지 이 지향치가 아니다
  },
  // 진단용 단일변수 프로필 — low 와 **duty 만** 다르다(투자셋/궁합/채용/reserve 전부 low 동일).
  // midNoTap 의 거울상: midNoTap 이 "mid 에서 탭만 뺀 arm" 이라면 이건 "low 에 탭만 넣은 arm" 이다.
  // 두 arm 이 함께 있어야 "탭이 원인이냐 투자셋이 원인이냐"를 가를 수 있다 — 한쪽만 보면
  // 교락된 결론(예: "원인은 탭 duty")이 나온다(2026-08-02 Task14 리뷰 F1).
  lowTap: {
    tapDuty: 0.52,
    feedSize: 2,
    affinityPreference: ['S', 'A', 'B', 'C'],
    invest: ['building:VaultCapacity'], // low 와 투자셋을 맞춘다 — 이 arm 은 duty 축만 단독 분리한다
    reserveMult: 1.2,
    boostGamble: false,
    hire: { shopNormal: true, shopAdvanced: false, diamondGacha: false },
    gradeMix: { C: 1.00, B: 0, A: 0, S: 0 }, // 진단 arm — 관심사는 실현 분포지 이 지향치가 아니다
  },
  // 상위 — 높은 duty 로 q 를 끌어올리므로 넓은 밴드(C/B 궁합)의 상방을 노리는 게 이득.
  high: {
    tapDuty: 0.70,
    feedSize: 4,
    affinityPreference: ['C', 'B', 'A', 'S'],
    invest: ['building:ProjectYield', 'building:ProjectGrade', 'building:MarketingPower',
      'building:VaultCapacity', 'star'],
    reserveMult: 3,
    boostGamble: true,
    hire: { shopNormal: true, shopAdvanced: true, diamondGacha: true },
    gradeMix: { C: 0.25, B: 0.40, A: 0.25, S: 0.10 },
  },
  // 숙련(순수 B) — 로스터(인원/레벨) 성장에 집중, 도박은 안 함.
  skilled: {
    tapDuty: 0.60,
    feedSize: 3,
    affinityPreference: ['B', 'A', 'C', 'S'],
    invest: ['building:ProjectYield', 'building:MarketingPower'],
    reserveMult: 2.5,
    boostGamble: false,
    hire: { shopNormal: true, shopAdvanced: true, diamondGacha: true },
    gradeMix: { C: 0, B: 1.00, A: 0, S: 0 },
  },
};

// makePolicy(profile, values) — values 는 현재 정책 결정에 쓰이지 않지만(정책 상수만으로 결정),
// Task12 캘리브레이션이 values 의존 정책(예: 개발비 대비 예산 규칙)을 넣을 자리로 시그니처를 유지한다.
function makePolicy(profile, values) {
  const P = PROFILES[profile];
  if (!P) throw new Error(`알 수 없는 정책 프로필: ${profile} (가능: ${Object.keys(PROFILES).join('|')})`);

  return {
    profile,
    sessionMinutes: SESSION_MINUTES,
    sessionsPerDay: SESSIONS_PER_DAY,
    launchOverheadSec: LAUNCH_OVERHEAD_SEC,
    tapDuty: P.tapDuty,
    feedSize: P.feedSize,
    reserveMult: P.reserveMult,
    slotsForTier,

    // feed = [{row, affinityGrade, trendMatched, isFirstClear, cost}] — 픽칭 피드에 뜬 카드들.
    // 규칙: (1) 티어 진행에 기여하는 첫클리어 우선 (2) 궁합 취향 순 (3) 트렌드 매칭 (4) 높은 idx.
    pickProject(state, feed) {
      const affordable = feed.filter(c => c.cost <= state.money);
      if (affordable.length === 0) return null;
      const wantFirst = affordable.some(c => c.isFirstClear);
      const pool = wantFirst ? affordable.filter(c => c.isFirstClear) : affordable;
      const rank = c => {
        const ai = P.affinityPreference.indexOf(c.affinityGrade ?? 'C');
        return [ai < 0 ? 99 : ai, c.trendMatched ? 0 : 1, -c.row.ProjectIndex];
      };
      return pool.slice().sort((a, b) => {
        const ra = rank(a), rb = rank(b);
        for (let i = 0; i < ra.length; i++) if (ra[i] !== rb[i]) return ra[i] - rb[i];
        return 0;
      })[0];
    },

    // 투자 우선순위 — 토큰 'building:<축>' | 'star' | 'hq'.
    // ⚠ 'hq' 는 어느 프로필에도 없다: HQ 레벨업의 실효(산업 해금·빌딩 수 상한)가 이 모델 범위 밖이라
    //    (Game 산업만 시뮬, 빌딩 수는 정책 P 램프) 순수 싱크가 되어 정책적으로 사는 게 비합리적이다.
    //    retune 이 "필수 싱크 194M"으로 잡은 축이므로 Task14/15 해석 시 이 미모델을 반드시 명시할 것.
    investPriority(state) {
      return P.invest;
    },

    // 부스트 도박(K2, DT_BoostGamble) — GoCost 확정 지출 + 성공/실패로 개발 시간 ±.
    useBoostGamble(state) {
      return P.boostGamble && state.money > state.nextDevCost * P.reserveMult;
    },

    // 서브경제 — 현재는 채용 경로만(상점 채용권 Money 구매 / Diamond 가챠).
    // 무역·채광·도시인수는 이 스파인(Game 산업 단일 오피스)에 배선되지 않아 미사용:
    // 도시인수는 Task9 실측상 Money 유입 0(캐시아웃 경로 미구현)이라 정책적으로도 사는 게 손해다.
    useSubEconomy(state) {
      const needRoster = state.roster < state.rosterCap;
      return {
        buyNormalTicket: P.hire.shopNormal && needRoster,
        buyAdvancedTicket: P.hire.shopAdvanced && needRoster,
        diamondGacha: P.hire.diamondGacha && needRoster,
        useTrade: false,
        useCityAcquisition: false,
      };
    },

    targetGradeMix() {
      return { ...P.gradeMix };
    },
  };
}

module.exports = { makePolicy, PROFILES, SESSION_MINUTES, SESSIONS_PER_DAY };
