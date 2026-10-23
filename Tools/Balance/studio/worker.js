'use strict';
// 자식 프로세스 진입점 — 한 프로세스가 한 job 만 처리하고 끝난다.
//
// 여기서 도는 것은 CPU 를 수 초간 통째로 먹는 계산뿐이다(30시드 월드 시뮬 = preview).
// apply/refresh 는 서버 프로세스에 남는다 — 계획 소비/history/캐시 무효화가 그쪽 메모리에 있어서다.
//
// require 만으로는 아무 listener 도 달지 않는다. 테스트가 dispatch 를 직접 부를 수 있어야 하고,
// 실수로 import 한 곳에서 IPC 를 가로채면 안 된다.
const { serializePreview } = require('./jobs.js');

const WORKER_TYPES = new Set(['preview']);

async function dispatch({ type, payload } = {}) {
  if (!WORKER_TYPES.has(type)) {
    const e = new Error(`worker 가 모르는 작업 타입입니다: ${type} (가능: ${[...WORKER_TYPES].join(', ')})`);
    e.code = 'UNKNOWN_JOB_TYPE';
    throw e;
  }
  // tune.js 는 여기서만 로드한다 — require('worker.js') 가 시뮬 전체를 끌고 오지 않게.
  const tune = require('../tune.js');
  return serializePreview(tune.runPreview(payload));
}

if (require.main === module) {
  process.on('message', async msg => {
    let reply;
    try {
      reply = { ok: true, result: await dispatch(msg) };
    } catch (e) {
      reply = { ok: false, error: { code: e.code || 'WORKER_FAILED', message: e.message, details: e.details, stack: e.stack } };
    }
    // send 콜백을 기다렸다 종료한다 — 바로 exit 하면 IPC 메시지가 잘려 부모가 "응답 없이 죽음" 으로 읽는다.
    process.send(reply, () => process.exit(0));
  });
}

module.exports = { dispatch, WORKER_TYPES };
