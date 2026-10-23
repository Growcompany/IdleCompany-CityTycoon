// 셸 — 내비게이션, 공유 상태, 상태줄. 화면별 내용은 views/*.mjs 가 그린다.
// index.html 의 뼈대를 찾아 채우는 방식이라 데이터가 늦어도 레이아웃이 흔들리지 않는다.
import { api, pollJob, el, clear, fmtWhen } from './api.mjs';
import * as knobs from './views/knobs.mjs';
import * as tables from './views/tables.mjs';
import * as historyView from './views/history.mjs';
import * as verify from './views/verify.mjs';
import * as profiles from './views/profiles.mjs';
import * as environment from './views/environment.mjs';

const VIEWS = {
  knobs: { mod: knobs, count: s => s.boot.catalog.rows.length },
  tables: { mod: tables, count: s => s.tables.length },
  history: { mod: historyView },
  verify: { mod: verify },
  profiles: { mod: profiles, count: s => (s.boot.profiles || []).length },
  environment: { mod: environment, count: s => s.boot.missing.length + s.boot.warnings.length },
};

const state = {
  boot: null,
  tables: [],
  view: 'knobs',
  // 화면을 옮기거나 새 작업을 걸면 이전 폴링을 끊는다. 안 끊으면 지난 결과가 뒤늦게 화면을 덮어쓴다.
  abort: null,
};

const root = document.getElementById('app');
const mainEl = document.querySelector('main');
const statusEl = document.querySelector('.statusbar');
const navEl = document.querySelector('.nav');
const chipsEl = document.querySelector('.top-chips');

function setStatus(text, kind = 'idle', extra = null) {
  clear(statusEl);
  statusEl.className = `statusbar${kind === 'error' ? ' err' : kind === 'busy' ? ' busy' : ''}`;
  if (kind === 'busy') statusEl.append(el('span', { class: 'spinner' }));
  statusEl.append(el('span', { text }));
  statusEl.append(el('span', { class: 'spacer' }));
  if (extra) statusEl.append(extra);
}

// 오류는 code 별로 "다음에 뭘 하면 되는지" 까지 말한다. 코드만 띄우면 사용자는 멈춘다.
const NEXT_STEP = {
  STALE_PLAN: '미리보기를 다시 실행하세요.',
  PLAN_EXPIRED: '미리보기를 다시 실행하세요.',
  PLAN_USED: '미리보기를 다시 실행하세요.',
  PLAN_NOT_FOUND: '미리보기를 다시 실행하세요.',
  EDITOR_NONE: '언리얼 에디터를 켠 뒤 다시 시도하세요.',
  EDITOR_MULTIPLE: '여분의 에디터 인스턴스를 종료한 뒤 다시 시도하세요.',
  REIMPORT_NO_MARKER: '에디터의 Python 원격 실행(bRemoteExecution)이 켜져 있는지 확인하세요.',
  REMOTE_EXEC_FAILED: '에디터가 켜져 있고 원격 실행이 활성인지 확인하세요.',
  OFFLINE: '도구 콘솔 창이 닫혔다면 BalanceStudio.cmd 를 다시 실행하세요.',
};

export function reportError(e) {
  const next = NEXT_STEP[e && e.code];
  setStatus(`${(e && e.message) || e}${next ? ` — ${next}` : ''}`, 'error');
  return e;
}

async function loadBoot() {
  state.boot = await api('/api/bootstrap');
  state.tables = (await api('/api/tables')).tables;
}

function renderTop() {
  const boot = state.boot;
  document.getElementById('project-name').textContent = boot.project.name;
  clear(chipsEl);
  const editors = boot.editorCount;
  chipsEl.append(
    editors === 1
      ? el('span', { class: 'chip ok', title: '적용 가능한 상태입니다' }, el('span', { class: 'dot' }), '에디터 1개')
      : editors === 0
        ? el('span', { class: 'chip warn', title: '미리보기는 가능하지만 적용은 할 수 없습니다' }, el('span', { class: 'dot' }), '에디터 없음')
        : el('span', { class: 'chip danger', title: 'uasset 저장이 실패합니다 — 여분을 종료하세요' }, el('span', { class: 'dot' }), `에디터 ${editors}개`),
    el('span', { class: 'chip', title: '이 시각의 에디터 스냅샷을 보고 있습니다', text: `스냅샷 ${fmtWhen(boot.data.valuesMeta.extractedAt)}` }),
    el('span', { class: 'chip', title: '시뮬레이션이 실제로 읽는 표 / 전체 표', text: `모델 ${boot.coverage.consumedTables}/${boot.coverage.totalTables}표` }),
    boot.calibration.gateOk ? null : el('span', { class: 'chip warn', title: '절대시간 지표는 보정되지 않았습니다 — 상대 비교만 신뢰하세요', text: '미보정' }),
  );
}

function renderNav() {
  for (const btn of navEl.querySelectorAll('button[data-view]')) {
    const id = btn.dataset.view;
    btn.setAttribute('aria-current', state.view === id ? 'page' : 'false');
    const tag = btn.querySelector('.tag');
    const counter = VIEWS[id] && VIEWS[id].count;
    if (counter && state.boot) {
      tag.textContent = String(counter(state));
      tag.hidden = false;
    } else {
      tag.hidden = true;
    }
  }
}

export function go(id) {
  if (state.abort) { state.abort.abort(); state.abort = null; }
  state.view = VIEWS[id] ? id : 'knobs';
  // 새로고침해도 보던 화면으로 돌아온다. 주소에 남는 건 화면 이름뿐 — 접속 키는 여기 없다.
  if (location.hash.slice(1) !== state.view) history.replaceState(null, '', `#${state.view}`);
  renderNav();
  clear(mainEl);
  setStatus('준비되었습니다.');
  try {
    VIEWS[state.view].mod.render(ctx, mainEl);
  } catch (e) {
    mainEl.append(el('div', { class: 'note danger', text: `화면을 그리지 못했습니다: ${e.message}` }));
    reportError(e);
  }
}

export async function refreshData() {
  setStatus('에디터에서 값을 다시 뽑는 중입니다 — 에디터가 잠시 멈춘 것처럼 보일 수 있습니다.', 'busy');
  try {
    const { jobId } = await api('/api/refresh', { method: 'POST', body: {} });
    const job = await pollJob(jobId, { onTick: j => setStatus(j.status === 'queued' ? '앞선 작업을 기다리는 중…' : '값을 다시 뽑는 중…', 'busy') });
    if (job.status !== 'completed') {
      throw Object.assign(new Error((job.error && job.error.message) || '재추출이 실패했습니다.'), { code: job.error && job.error.code });
    }
    await reloadAll();
    setStatus(`값을 다시 뽑았습니다 — 표 ${job.result.tables}종. 이전 스냅샷 백업: ${job.result.backupPath}`);
  } catch (e) {
    reportError(e);
  }
}

// 적용/재추출 뒤 화면을 최신 데이터로 다시 그린다.
export async function reloadAll() {
  await loadBoot();
  renderTop();
  go(state.view);
}

const ctx = {
  get state() { return state; },
  get boot() { return state.boot; },
  get tables() { return state.tables; },
  setStatus,
  reportError,
  reloadAll,
  refreshData,
  go,
  newAbort() {
    if (state.abort) state.abort.abort();
    state.abort = new AbortController();
    return state.abort.signal;
  },
};

async function boot() {
  navEl.addEventListener('click', event => {
    const btn = event.target.closest('button[data-view]');
    if (btn) go(btn.dataset.view);
  });
  document.getElementById('refresh-data').addEventListener('click', refreshData);

  try {
    await loadBoot();
  } catch (e) {
    clear(mainEl);
    mainEl.append(
      el('div', { class: 'note danger', text: `데이터를 불러오지 못했습니다: ${e.message}` }),
      e.code === 'UNAUTHORIZED'
        ? el('div', { class: 'note', text: '도구가 출력한 주소(키 포함)로 다시 여세요 — 콘솔 창에 주소가 있습니다.' })
        : el('div', { class: 'note', text: 'Tools/Balance/out/values.json 이 있는지, 에디터 값 추출을 한 적이 있는지 확인하세요.' }));
    setStatus(e.message, 'error');
    return;
  }

  root.dataset.boot = 'ready';
  renderTop();
  go(location.hash.slice(1) || 'knobs');
}

boot();
