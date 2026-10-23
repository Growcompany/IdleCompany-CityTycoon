'use strict';
// 미리본 것에 붙는 1회성 실행권(plan). (Balance Studio 에서 추출)
//
// 실행 요청은 { planId, confirmed } 만 받는다 — 내용을 다시 받지 않는 것이 핵심이다.
// 다시 받으면 "확인한 것" 과 "실행하는 것" 이 갈라질 수 있고, 그 틈이 곧 계획 바꿔치기다.
// 계획 내용은 오직 서버 메모리에서만 나온다.
const crypto = require('node:crypto');

const DEFAULT_TTL_MS = 600_000; // 10분

class PlanError extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.name = 'PlanError';
    this.code = code;
    this.details = details;
  }
}

function stableJson(value) {
  if (value === null || typeof value !== 'object') return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map(stableJson).join(',')}]`;
  return `{${Object.keys(value).sort().map(k => `${JSON.stringify(k)}:${stableJson(value[k])}`).join(',')}}`;
}

// 깊은 동결 — 발급 뒤 호출부가 입력 객체를 고쳐도 계획은 그대로여야 한다.
function deepFreeze(value) {
  if (value === null || typeof value !== 'object') return value;
  for (const k of Object.keys(value)) deepFreeze(value[k]);
  return Object.freeze(value);
}
const deepCopy = value => (value === undefined ? undefined : JSON.parse(JSON.stringify(value)));

class PlanStore {
  constructor({ now = () => Date.now(), ttlMs = DEFAULT_TTL_MS, randomId = () => crypto.randomBytes(16).toString('hex') } = {}) {
    this.now = now;
    this.ttlMs = ttlMs;
    this.randomId = randomId;
    this.plans = new Map();
  }

  issue({ change, preview, fingerprint }) {
    if (!change || !fingerprint) throw new PlanError('PLAN_INPUT', 'plan 발급에는 change 와 fingerprint 가 필요합니다.');
    const issuedAt = this.now();
    // 내용(change/preview)만 깊게 언다 — used 는 뒤집혀야 하므로 레코드 자체는 열어 둔다.
    const stored = {
      id: this.randomId(),
      change: deepFreeze(deepCopy(change)),
      preview: deepFreeze(deepCopy(preview)),
      fingerprint,
      hash: crypto.createHash('sha256').update(stableJson({ change, fingerprint })).digest('hex'),
      issuedAt,
      expiresAtMs: issuedAt + this.ttlMs,
      expiresAt: new Date(issuedAt + this.ttlMs).toISOString(),
      used: false,
      usedAt: null,
    };
    this.#prune();
    this.plans.set(stored.id, stored);
    return stored;
  }

  // 쓴 계획과 만료된 계획을 무한히 들고 있지 않는다. 진단에 쓸 만큼만(2×TTL) 남긴다.
  #prune() {
    const cutoff = this.now() - this.ttlMs * 2;
    for (const [id, p] of this.plans) if (p.issuedAt < cutoff) this.plans.delete(id);
  }

  peek(id) {
    return this.plans.get(id) || null;
  }

  size() {
    return this.plans.size;
  }

  // 검증 순서는 사용자에게 가장 고칠 수 있는 사유부터다:
  //   확인 안 함 → 계획 없음 → 이미 씀 → 만료 → 원본 변경(stale).
  consume(id, { confirmed, fingerprint } = {}) {
    if (confirmed !== true) {
      throw new PlanError('PLAN_CONFIRMATION_REQUIRED', '적용 확인이 필요합니다 — 확인창에서 대상과 before/after 를 확인한 뒤 실행하세요.', { planId: id });
    }
    const plan = this.plans.get(id);
    if (!plan) {
      throw new PlanError('PLAN_NOT_FOUND', '계획을 찾을 수 없습니다 — 데이터 재추출이나 다른 적용으로 폐기되었을 수 있습니다. 미리보기를 다시 실행하세요.', { planId: id });
    }
    if (plan.used) {
      throw new PlanError('PLAN_USED', '이미 사용한 계획입니다 — 같은 미리보기를 두 번 적용할 수 없습니다. 미리보기를 다시 실행하세요.', { planId: id });
    }
    if (this.now() > plan.expiresAtMs) {
      this.plans.delete(id);
      throw new PlanError('PLAN_EXPIRED', `계획이 만료되었습니다(발급 후 ${Math.round(this.ttlMs / 60000)}분). 미리보기를 다시 실행하세요.`, { planId: id, expiresAt: plan.expiresAt });
    }
    if (plan.fingerprint !== fingerprint) {
      // 폐기하지 않는다 — 원본이 되돌아오면 그대로 쓸 수 있고, 남겨 둬야 무엇이 달라졌는지 보여줄 수 있다.
      throw new PlanError('STALE_PLAN', '원본이 미리보기 이후 변경되었습니다 — 지금 적용하면 미리 본 것과 다른 결과가 됩니다. 미리보기를 다시 실행하세요.', {
        planId: id, expected: plan.fingerprint, actual: fingerprint,
      });
    }
    // 즉시 used 처리 — 지우지 않는 이유는, 두 번째 시도에 "없음" 대신 "이미 사용함" 이라고
    // 정확히 답해야 사용자가 자기 상태를 알 수 있어서다.
    plan.used = true;
    plan.usedAt = this.now();
    return plan;
  }

  invalidateAll(reason = '') {
    const n = this.plans.size;
    this.plans.clear();
    return { invalidated: n, reason };
  }
}

module.exports = { PlanStore, PlanError, DEFAULT_TTL_MS };
