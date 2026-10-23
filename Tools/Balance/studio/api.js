'use strict';
// HTTP 계약 — 요청 검증과 JSON 응답만. 파일도 프로세스도 여기서 직접 건드리지 않는다.
//
// 모든 오류는 같은 봉투로 나간다: { error: { code, message, details } }
// code 는 안정적이어야 UI 가 상황별 안내(재미리보기/에디터 종료/복구 절차)를 고를 수 있다.
const { JOB_TYPES } = require('./jobs.js');
// 오류 봉투(ApiError/errorBody)는 공용이다 — 로컬 도구들이 같은 모양으로 실패를 말해야
// UI 가 code 로 안내를 고르는 규약이 도구를 넘어 유지된다.
const { ApiError, BASE_STATUS_BY_CODE, errorBody, makeStatusOf } = require('../../_Shared/webshell/api-error.js');

const MAX_BODY_BYTES = 65_536;
const PROFILES = ['low', 'lowTap', 'mid', 'midNoTap', 'high', 'skilled'];
const MAX_SEEDS = 200;
const MAX_UNTIL_TIER = 10;

// code → HTTP status. 공용 표 위에 Balance 고유 code 를 얹는다.
// 여기 없는 code 는 500 이다(모르는 실패를 200 으로 흘리지 않는다).
const STATUS_BY_CODE = {
  ...BASE_STATUS_BY_CODE,
  CURVE_MULT_ONLY: 400, ALL_ROWS_REQUIRED: 400, COLUMN_NOT_NUMERIC: 400, ROW_NOT_FOUND: 400,
  TABLE_NOT_FOUND: 404, VALUES_MISSING: 404, PLAN_NOT_FOUND: 404,
  PLAN_USED: 409, PLAN_EXPIRED: 409, STALE_PLAN: 409, PLAN_CONFIRMATION_REQUIRED: 409,
};

const statusOf = makeStatusOf(STATUS_BY_CODE);

// ── 입력 검증 ───────────────────────────────────────────────────────────────
function onlyKeys(body, allowed, what) {
  const extra = Object.keys(body || {}).filter(k => !allowed.includes(k));
  if (extra.length) {
    throw new ApiError('BAD_FIELD', `${what} 에 허용되지 않은 항목이 있습니다: ${extra.join(', ')}`, { allowed, extra });
  }
}

function validatePreviewBody(body) {
  onlyKeys(body, ['knob', 'value', 'profile', 'seeds', 'seedStart', 'untilTier', 'allRows'], 'preview 요청');
  const knob = body.knob;
  if (typeof knob !== 'string' || !knob.trim()) throw new ApiError('BAD_FIELD', 'knob 이 필요합니다.');
  if (body.value === undefined || body.value === null || `${body.value}`.trim() === '') {
    throw new ApiError('BAD_FIELD', 'value 가 필요합니다. 예: 1.8 / x1.2 / +20%');
  }
  const profile = body.profile === undefined ? 'mid' : body.profile;
  if (!PROFILES.includes(profile)) {
    throw new ApiError('BAD_FIELD', `알 수 없는 프로필: ${profile} (가능: ${PROFILES.join(', ')})`, { profiles: PROFILES });
  }
  const seeds = body.seeds === undefined ? 30 : Number(body.seeds);
  if (!Number.isInteger(seeds) || seeds < 1 || seeds > MAX_SEEDS) {
    throw new ApiError('BAD_FIELD', `seeds 는 1~${MAX_SEEDS} 사이의 정수여야 합니다.`);
  }
  const seedStart = body.seedStart === undefined ? 1000 : Number(body.seedStart);
  if (!Number.isInteger(seedStart) || seedStart < 0) throw new ApiError('BAD_FIELD', 'seedStart 는 0 이상의 정수여야 합니다.');
  const untilTier = body.untilTier === undefined ? MAX_UNTIL_TIER : Number(body.untilTier);
  if (!Number.isInteger(untilTier) || untilTier < 2 || untilTier > MAX_UNTIL_TIER) {
    throw new ApiError('BAD_FIELD', `untilTier 는 2~${MAX_UNTIL_TIER} 사이의 정수여야 합니다.`);
  }
  return { knob: knob.trim(), value: `${body.value}`.trim(), profile, seeds, seedStart, untilTier, allRows: body.allRows === true };
}

function validateApplyBody(body) {
  // ⚠ knob/value 를 다시 받지 않는다 — 받으면 "미리본 것" 과 "적용하는 것" 이 갈라질 수 있다.
  onlyKeys(body, ['planId', 'confirmed'], 'apply 요청');
  if (typeof body.planId !== 'string' || !body.planId) throw new ApiError('BAD_FIELD', 'planId 가 필요합니다.');
  if (body.confirmed !== true) {
    throw new ApiError('PLAN_CONFIRMATION_REQUIRED', '적용 확인이 필요합니다 — 확인창에서 대상과 before/after 를 확인한 뒤 실행하세요.');
  }
  return { planId: body.planId, confirmed: true };
}

// ── 라우터 ──────────────────────────────────────────────────────────────────
function createApiRouter({
  paths, model, history, jobs, plans, applyService, refreshService, previewRunner,
  isShuttingDown = () => false, requestShutdown = null,
}) {
  // 큐의 executor 는 여기서 등록한다 — 계획 소비/지문 재계산은 API 의 orchestration 이지 큐의 일이 아니다.
  if (previewRunner) {
    jobs.executors.preview = async (payload, ctx) => {
      const preview = await previewRunner(payload.runOpts, ctx);
      const change = payload.change;
      // 적용 가능한 라우트만 실행권을 발급한다. src/cdo 는 위치 안내라 계획이 없다.
      if (change.appliable) {
        const fingerprint = model.fingerprintChange(change);
        const plan = plans.issue({ change, preview, fingerprint });
        return { preview, change, planId: plan.id, expiresAt: plan.expiresAt };
      }
      return { preview, change, planId: null, expiresAt: null };
    };
  }
  if (applyService) {
    jobs.executors.apply = async (payload, ctx) => {
      const peeked = plans.peek(payload.planId);
      if (!peeked) throw new ApiError('PLAN_NOT_FOUND', '계획을 찾을 수 없습니다 — 데이터 재추출이나 다른 적용으로 폐기되었을 수 있습니다. 미리보기를 다시 실행하세요.');
      // 큐에서 차례가 온 "지금" 의 원본으로 다시 판정한다 — 대기 중에 원본이 바뀌었을 수 있다.
      model.invalidate();
      const fingerprint = model.fingerprintChange(peeked.change);
      const plan = plans.consume(payload.planId, { confirmed: true, fingerprint });
      return applyService.apply(plan, { jobId: ctx.jobId });
    };
  }
  if (refreshService) {
    jobs.executors.refresh = async (payload, ctx) => refreshService.run({ jobId: ctx.jobId });
  }

  const enqueue = (type, payload) => {
    if (isShuttingDown()) throw new ApiError('SHUTTING_DOWN', '도구가 종료 중이라 새 작업을 받지 않습니다.');
    const job = jobs.enqueue(type, payload);
    return { status: 202, body: { jobId: job.id, status: job.status } };
  };

  const routes = {
    'GET /api/bootstrap': () => ({ status: 200, body: model.bootstrap() }),
    'GET /api/tables': () => ({ status: 200, body: { tables: model.listTables() } }),
    'GET /api/history': (ctx) => {
      const limit = Math.min(Number(ctx.query.get('limit')) || 50, 500);
      return { status: 200, body: history.list({ limit }) };
    },

    'POST /api/preview': (ctx) => {
      const input = validatePreviewBody(ctx.body);
      // 노브/표/컬럼/행 allowlist 는 큐에 넣기 전에 통과해야 한다 — 오타로 자식 프로세스를 띄우지 않는다.
      const change = model.validateChange({ knob: input.knob, value: input.value, allRows: input.allRows });
      return enqueue('preview', {
        change,
        runOpts: {
          knob: input.knob, value: input.value,
          valuesPath: paths.valuesPath, scanPath: paths.scanPath, tmpDir: paths.tmpDir,
          profile: input.profile, seeds: input.seeds, seedStart: input.seedStart, untilTier: input.untilTier,
        },
      });
    },

    'POST /api/apply': (ctx) => {
      const input = validateApplyBody(ctx.body);
      if (!plans.peek(input.planId)) {
        throw new ApiError('PLAN_NOT_FOUND', '계획을 찾을 수 없습니다 — 미리보기를 다시 실행하세요.', { planId: input.planId });
      }
      return enqueue('apply', { planId: input.planId });
    },

    'POST /api/refresh': (ctx) => {
      onlyKeys(ctx.body, [], 'refresh 요청');
      return enqueue('refresh', {});
    },

    'POST /api/shutdown': (ctx) => {
      onlyKeys(ctx.body, [], 'shutdown 요청');
      if (requestShutdown) requestShutdown();
      return { status: 200, body: { ok: true, message: '도구를 종료합니다. 이 창은 닫아도 됩니다.' } };
    },
  };

  // 파라미터 라우트 — decode 한 뒤 model/queue 의 allowlist 를 통과해야만 값을 준다.
  function paramRoute(method, pathname, ctx) {
    let m = /^\/api\/tables\/(.+)$/.exec(pathname);
    if (m && method === 'GET') {
      let name;
      try { name = decodeURIComponent(m[1]); } catch { throw new ApiError('BAD_REQUEST', '표 이름을 해석하지 못했습니다.'); }
      return { status: 200, body: model.getTable(name) };
    }
    m = /^\/api\/jobs\/(.+)$/.exec(pathname);
    if (m && method === 'GET') {
      let id;
      try { id = decodeURIComponent(m[1]); } catch { throw new ApiError('BAD_REQUEST', '작업 id 를 해석하지 못했습니다.'); }
      const job = jobs.get(id);
      if (!job) throw new ApiError('JOB_NOT_FOUND', `작업을 찾을 수 없습니다: ${id}`, { id });
      return { status: 200, body: job };
    }
    return null;
  }

  function handle(ctx) {
    const key = `${ctx.method} ${ctx.pathname}`;
    const route = routes[key];
    if (route) return route(ctx);
    const param = paramRoute(ctx.method, ctx.pathname, ctx);
    if (param) return param;
    throw new ApiError('NOT_FOUND', `없는 API 입니다: ${ctx.method} ${ctx.pathname}`);
  }

  return {
    handle,
    routes,
    deps: { paths, model, history, jobs, plans, applyService, refreshService, previewRunner },
  };
}

module.exports = {
  createApiRouter, ApiError, errorBody, statusOf,
  validatePreviewBody, validateApplyBody,
  MAX_BODY_BYTES, PROFILES, STATUS_BY_CODE,
};
