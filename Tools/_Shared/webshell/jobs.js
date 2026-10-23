'use strict';
// 단일 직렬 작업 큐 + 자식 프로세스 IPC. (Balance Studio 에서 추출 — 도메인 지식 없음)
//
// 왜 큐인가: 긴 작업이 겹치면 서로의 원본을 밟는다.
// 왜 자식인가: 수 초간 CPU 를 통째로 먹는 작업을 서버 프로세스에서 돌리면 그동안 UI 가
//              작업 상태를 못 물어보고, 사용자는 "멈췄다" 고 읽는다.
//
// 앱마다 작업 타입이 다르므로 JOB_TYPES 는 모듈 상수가 아니라 생성자 옵션(jobTypes)이다.
// 넘기지 않으면 executor 가 등록된 타입이면 무엇이든 받는다.
const { fork: childFork } = require('node:child_process');

const JOB_STATUS = new Set(['queued', 'running', 'completed', 'failed', 'cancelled']);
const DEFAULT_TIMEOUT_MS = 300_000;

class JobError extends Error {
  constructor(code, message, details = undefined, stack = undefined) {
    super(message);
    this.name = 'JobError';
    this.code = code;
    if (details !== undefined) this.details = details;
    if (stack) this.remoteStack = stack;
  }
}

const iso = ms => new Date(ms).toISOString();

// 폴링 응답에 실릴 수 있는 형태로만 남긴다 — 함수/순환 참조가 섞이면 작업 조회가 통째로 터진다.
function jsonSafe(value) {
  if (value === undefined) return null;
  try {
    return JSON.parse(JSON.stringify(value));
  } catch {
    return { unserializable: String(value) };
  }
}

function errorSnapshot(e) {
  const out = { code: (e && e.code) || 'JOB_FAILED', message: (e && e.message) || String(e) };
  if (e && e.details !== undefined) out.details = jsonSafe(e.details);
  return out;
}

// ── 자식 실행 ────────────────────────────────────────────────────────────────
// 한 번만 settle 한다. message / error / exit / timeout 이 앞다투어 도착해도 결과는 하나여야 한다.
function runChildJob({ workerPath, type, payload, timeoutMs = DEFAULT_TIMEOUT_MS, fork = childFork }, ctx = null) {
  return new Promise((resolve, reject) => {
    let settled = false;
    let child = null;
    let stderr = '';

    const kill = () => { try { if (child) child.kill(); } catch { /* 이미 죽었으면 그만 */ } };
    const settle = (fn, arg) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      kill();
      fn(arg);
    };
    const timer = setTimeout(() => {
      settle(reject, new JobError('JOB_TIMEOUT', `작업이 ${Math.round(timeoutMs / 1000)}초 안에 끝나지 않아 중단했습니다.${stderr ? ` (worker 출력: ${stderr.slice(-300)})` : ''}`));
    }, timeoutMs);

    if (ctx && typeof ctx.onCancel === 'function') {
      ctx.onCancel(() => settle(reject, new JobError('JOB_CANCELLED', '작업이 취소되었습니다.')));
    }

    try {
      child = fork(workerPath, [], { stdio: ['ignore', 'ignore', 'pipe', 'ipc'] });
    } catch (e) {
      return settle(reject, new JobError('WORKER_SPAWN_FAILED', `worker 를 실행하지 못했습니다: ${e.message}`));
    }
    if (child.stderr) child.stderr.on('data', d => { stderr += String(d); });

    child.on('message', msg => {
      if (msg && msg.ok) return settle(resolve, msg.result);
      const err = (msg && msg.error) || {};
      settle(reject, new JobError(err.code || 'WORKER_FAILED', err.message || 'worker 가 실패를 알렸습니다.', err.details, err.stack));
    });
    child.on('error', e => settle(reject, new JobError('WORKER_SPAWN_FAILED', `worker 오류: ${e.message}`)));
    // 응답 없이 종료 = 실패다. 종료코드 0 이어도 결과가 없으면 성공으로 읽지 않는다.
    child.on('exit', (code, signal) => settle(reject, new JobError(
      'WORKER_CRASHED',
      `worker 가 결과 없이 종료했습니다 (exit=${code} signal=${signal}).${stderr ? ` 출력: ${stderr.slice(-300)}` : ''}`,
    )));

    child.send({ type, payload });
  });
}

// ── 큐 ──────────────────────────────────────────────────────────────────────
class JobQueue {
  #queue = [];
  #jobs = new Map();
  #running = null;
  #closed = false;
  #idleWaiters = [];
  #jobTypes = null;

  constructor({ executors = {}, id, jobTypes = null, timeoutMs = DEFAULT_TIMEOUT_MS, now = () => Date.now(), maxCompleted = 200 } = {}) {
    this.executors = executors;
    this.timeoutMs = timeoutMs;
    this.now = now;
    this.maxCompleted = maxCompleted;
    this.#jobTypes = jobTypes;
    let n = 0;
    this.id = id || (() => `job-${Date.now().toString(36)}-${++n}`);
  }

  #knownType(type) {
    return this.#jobTypes ? this.#jobTypes.has(type) : true;
  }

  #typeList() {
    return this.#jobTypes ? [...this.#jobTypes] : Object.keys(this.executors);
  }

  enqueue(type, payload) {
    if (this.#closed) throw new JobError('QUEUE_CLOSED', '도구가 종료 중이라 새 작업을 받지 않습니다.');
    if (!this.#knownType(type) || typeof this.executors[type] !== 'function') {
      throw new JobError('UNKNOWN_JOB_TYPE', `알 수 없는 작업 타입: ${type} (가능: ${this.#typeList().join(', ')})`, { type });
    }
    const job = {
      id: this.id(), type, payload,
      status: 'queued', createdAt: iso(this.now()),
      startedAt: null, finishedAt: null, result: null, error: null,
      _hooks: [], _cancelled: false, _finished: false, _timer: null,
    };
    this.#jobs.set(job.id, job);
    this.#queue.push(job);
    queueMicrotask(() => this.#pump());
    return this.#snapshot(job);
  }

  get(id) {
    const job = this.#jobs.get(id);
    return job ? this.#snapshot(job) : null;
  }

  cancel(id) {
    const job = this.#jobs.get(id);
    if (!job) throw new JobError('JOB_NOT_FOUND', `작업을 찾을 수 없습니다: ${id}`, { id });
    if (job._finished) return this.#snapshot(job);
    job._cancelled = true;
    this.#fireHooks(job);
    this.#finish(job, 'cancelled');
    return this.#snapshot(job);
  }

  async close() {
    this.#closed = true;
    for (const job of this.#queue) if (!job._finished) this.#finish(job, 'cancelled');
    this.#queue.length = 0;
    await this.waitForIdle();
  }

  waitForIdle() {
    if (this.#isIdle()) return Promise.resolve();
    return new Promise(resolve => this.#idleWaiters.push(resolve));
  }

  get running() {
    return this.#running ? this.#snapshot(this.#running) : null;
  }

  #isIdle() {
    return !this.#running && !this.#queue.some(j => j.status === 'queued');
  }

  #settleIdle() {
    if (!this.#isIdle()) return;
    const waiters = this.#idleWaiters;
    this.#idleWaiters = [];
    for (const r of waiters) r();
  }

  #fireHooks(job) {
    for (const fn of job._hooks.splice(0)) {
      try { fn(); } catch { /* hook 실패가 큐를 세우면 안 된다 */ }
    }
  }

  // 멱등 종료 — timeout 과 executor 의 늦은 settle 이 겹쳐도 상태는 한 번만 확정된다.
  #finish(job, status, extra = {}) {
    if (job._finished) return;
    if (!JOB_STATUS.has(status)) throw new JobError('BAD_STATUS', `허용 밖 job status: ${status}`);
    job._finished = true;
    job.status = status;
    Object.assign(job, extra);
    job.finishedAt = iso(this.now());
    if (job._timer) { clearTimeout(job._timer); job._timer = null; }
    if (this.#running === job) {
      this.#running = null;
      queueMicrotask(() => this.#pump());
    }
    this.#trim();
  }

  #pump() {
    if (this.#running) return;
    let job = this.#queue.shift();
    while (job && job.status !== 'queued') job = this.#queue.shift();
    if (!job) { this.#settleIdle(); return; }

    this.#running = job;
    job.status = 'running';
    job.startedAt = iso(this.now());

    const ctx = { jobId: job.id, onCancel: fn => job._hooks.push(fn) };
    job._timer = setTimeout(() => {
      // hook 을 먼저 불러 자식을 회수하고, executor 가 영영 안 끝나도 큐는 진행시킨다.
      this.#fireHooks(job);
      this.#finish(job, 'failed', {
        error: { code: 'JOB_TIMEOUT', message: `작업이 ${Math.round(this.timeoutMs / 1000)}초 안에 끝나지 않아 중단했습니다.` },
      });
    }, this.timeoutMs);

    Promise.resolve()
      .then(() => this.executors[job.type](job.payload, ctx))
      .then(result => this.#finish(job, 'completed', { result: jsonSafe(result) }))
      .catch(e => {
        if (job._cancelled) this.#finish(job, 'cancelled');
        else this.#finish(job, 'failed', { error: errorSnapshot(e) });
      });
  }

  // 완료 작업이 무한히 쌓이면 장시간 켜 둔 도구가 메모리를 먹는다. 진단에 쓸 만큼만 남긴다.
  #trim() {
    const finished = [...this.#jobs.values()].filter(j => j._finished);
    const excess = finished.length - this.maxCompleted;
    for (let i = 0; i < excess; i++) this.#jobs.delete(finished[i].id);
  }

  #snapshot(job) {
    return {
      id: job.id, type: job.type, status: job.status,
      createdAt: job.createdAt, startedAt: job.startedAt, finishedAt: job.finishedAt,
      result: job.result, error: job.error,
    };
  }
}

module.exports = { JobQueue, JobError, runChildJob, jsonSafe, JOB_STATUS, DEFAULT_TIMEOUT_MS };
