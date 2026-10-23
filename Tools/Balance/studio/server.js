'use strict';
// Balance Studio 서버 — HTTP 셸(loopback bind · 접속 키 · CSRF · 정적 서빙 · 드레인 종료)은
// Tools/_Shared/webshell/server.js 로 옮겼다. 여기 남은 것은 Balance 고유 조립뿐이다:
// 서비스 그래프, 정적 파일 목록, 쿠키 이름, 라우터 연결.
const { createWebShellServer, openInBrowser } = require('../../_Shared/webshell/server.js');

const { createStudioPaths } = require('./paths.js');
const { createStudioModel } = require('./model.js');
const { HistoryStore } = require('./history.js');
const { PlanStore } = require('./plans.js');
const { JobQueue, runChildJob } = require('./jobs.js');
const { createApplyService } = require('./apply.js');
const { createRefreshService } = require('./refresh.js');
const { createApiRouter, statusOf, MAX_BODY_BYTES } = require('./api.js');

// 열어 주는 정적 파일 전체 목록. 요청 경로를 파일 경로로 바꾸지 않는다 — 이 표에 없으면 404 다.
const STATIC_FILES = [
  'index.html', 'app.css', 'app.mjs', 'api.mjs',
  'views/knobs.mjs', 'views/tables.mjs', 'views/history.mjs',
  'views/verify.mjs', 'views/profiles.mjs', 'views/environment.mjs',
];
const COOKIE_NAME = 'balv_session';
const TOKEN_HEADER = 'x-balance-token';

// ── 서비스 조립 ─────────────────────────────────────────────────────────────
function createStudioServices(options = {}) {
  const paths = options.paths || createStudioPaths();
  const model = options.model || createStudioModel({ paths, editorCheck: options.editorCheck });
  const plans = options.plans || new PlanStore();
  const history = options.history || new HistoryStore({ historyPath: paths.historyPath });
  const editorCheck = options.editorCheck || model.editorCount;
  const applyService = options.applyService || createApplyService({
    paths, model, plans, history, editorCheck, exec: options.exec, tuneExec: options.tuneExec,
  });
  const refreshService = options.refreshService || createRefreshService({
    paths, model, plans, editorCheck, exec: options.exec,
  });
  const jobs = options.jobs || new JobQueue({ executors: {} });
  const previewRunner = options.previewRunner
    || ((payload, ctx) => runChildJob({ workerPath: paths.workerPath, type: 'preview', payload }, ctx));
  return { paths, model, plans, history, jobs, applyService, refreshService, previewRunner, editorCheck };
}

// ── 서버 ────────────────────────────────────────────────────────────────────
function createStudioServer({ host = '127.0.0.1', port = 0, token, services, openBrowser = false, log = () => {} }) {
  if (!token) throw new Error('createStudioServer: token 이 필요합니다.');
  if (!services) throw new Error('createStudioServer: services 가 필요합니다.');

  return createWebShellServer({
    host,
    port,
    token,
    appName: 'Balance Studio',
    publicDir: services.paths.publicDir,
    staticFiles: STATIC_FILES,
    cookieName: COOKIE_NAME,
    tokenHeader: TOKEN_HEADER,
    statusOf,
    maxBodyBytes: MAX_BODY_BYTES,
    createRouter: shellCtx => createApiRouter({ ...services, ...shellCtx }),
    onDrain: () => services.jobs.close(),
    openBrowser,
    log,
  });
}

if (require.main === module) {
  const crypto = require('node:crypto');
  const token = crypto.randomBytes(32).toString('hex');
  const services = createStudioServices();
  const studio = createStudioServer({ port: 0, token, services, openBrowser: true, log: console.log });
  studio.start().catch(e => {
    console.error(`Balance Studio 를 시작하지 못했습니다: ${e.message}`);
    process.exitCode = 1;
  });
  for (const sig of ['SIGINT', 'SIGTERM']) process.on(sig, () => { studio.close().finally(() => process.exit(0)); });
}

module.exports = { createStudioServer, createStudioServices, STATIC_FILES, COOKIE_NAME, openInBrowser };
