'use strict';
// Balance Studio 의 작업 큐 — 큐 자체는 Tools/_Shared/webshell/jobs.js 로 옮겼다.
// 여기 남은 것은 Balance 고유 지식뿐이다: 작업 타입 3종과 preview 결과 직렬화.
//
// apply/refresh 는 자식으로 보내지 않는다. 계획 소비(PlanStore)·history·캐시 무효화가 서버 메모리에 있어
// 자식으로 넘기면 상태가 두 프로세스로 갈라진다. 대신 그 안의 긴 호출(RemoteExec)을 비동기로 돌려
// 루프를 막지 않는다 — 불변식("HTTP 루프는 블로킹 계산을 하지 않는다")은 그대로 지켜진다.
const shared = require('../../_Shared/webshell/jobs.js');

const JOB_TYPES = new Set(['preview', 'apply', 'refresh']);

// 공용 큐는 타입 집합을 주입받는다. Balance 호출부는 예전처럼 그냥 new JobQueue({executors}) 를 쓴다.
class JobQueue extends shared.JobQueue {
  constructor(options = {}) {
    super({ jobTypes: JOB_TYPES, ...options });
  }
}

// tune.runPreview 의 tierDay 는 Map 이라 JSON 으로 나가면 {} 가 된다 — 티어 진입일 비교표가 통째로 사라진다.
function serializeSummary(summary) {
  if (!summary) return null;
  const tierDay = summary.tierDay instanceof Map
    ? [...summary.tierDay.entries()].map(([tier, day]) => ({ tier, day })).sort((a, b) => a.tier - b.tier)
    : (summary.tierDay || []);
  return {
    n: summary.n,
    invalid: summary.invalid,
    completionRate: summary.completionRate,
    medianInAppH: summary.medianInAppH,
    gateFailShare: summary.gateFailShare,
    tierDay,
  };
}

function serializePreview(preview) {
  return { ...preview, before: serializeSummary(preview.before), after: serializeSummary(preview.after) };
}

module.exports = {
  JobQueue,
  JobError: shared.JobError,
  runChildJob: shared.runChildJob,
  serializePreview,
  serializeSummary,
  JOB_TYPES,
  JOB_STATUS: shared.JOB_STATUS,
  DEFAULT_TIMEOUT_MS: shared.DEFAULT_TIMEOUT_MS,
};
