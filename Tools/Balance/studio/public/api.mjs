// 서버 통신 + 화면 공용 헬퍼.
//
// 접속 키는 URL 로 한 번만 온다. 바로 sessionStorage 로 옮기고 주소창에서 지운다 —
// 주소창에 남으면 새로고침·북마크·어깨너머로 새어나간다. 이후 모든 요청은 헤더로 보낸다
// (쿠키가 아니라 헤더라서 다른 사이트가 이 API 를 대신 호출할 수 없다).
const KEY = 'balv.token';

// node 에서 순수 함수(formatConfirmation 등)를 import 해 테스트할 수 있어야 한다 — 브라우저 전역이 없으면 조용히 넘어간다.
const inBrowser = typeof location !== 'undefined' && typeof sessionStorage !== 'undefined';

function takeToken() {
  if (!inBrowser) return '';
  const url = new URL(location.href);
  const fromUrl = url.searchParams.get('key');
  if (fromUrl) {
    sessionStorage.setItem(KEY, fromUrl);
    url.searchParams.delete('key');
    history.replaceState(null, '', url.pathname + (url.search || '') + url.hash);
  }
  return sessionStorage.getItem(KEY) || '';
}

export const token = takeToken();

export class ApiError extends Error {
  constructor(code, message, details) {
    super(message);
    this.code = code;
    this.details = details || {};
  }
}

export async function api(path, { method = 'GET', body, signal } = {}) {
  let response;
  try {
    response = await fetch(path, {
      method,
      signal,
      headers: { 'x-balance-token': token, ...(body === undefined ? {} : { 'content-type': 'application/json' }) },
      body: body === undefined ? undefined : JSON.stringify(body),
    });
  } catch (e) {
    if (e.name === 'AbortError') throw e;
    throw new ApiError('OFFLINE', '도구 서버에 연결할 수 없습니다 — 콘솔 창이 닫혔는지 확인하세요.');
  }
  const text = await response.text();
  let json = null;
  try { json = text ? JSON.parse(text) : null; } catch { /* 아래에서 상태로 판단한다 */ }
  if (!response.ok) {
    const e = (json && json.error) || {};
    throw new ApiError(e.code || `HTTP_${response.status}`, e.message || `요청이 실패했습니다 (${response.status}).`, e.details);
  }
  return json;
}

// 작업 폴링 — 500ms 간격. 탭을 옮기거나 새 미리보기를 걸면 AbortController 로 이전 폴링을 끊는다.
export async function pollJob(jobId, { signal, onTick, intervalMs = 500 } = {}) {
  for (;;) {
    const job = await api(`/api/jobs/${encodeURIComponent(jobId)}`, { signal });
    if (onTick) onTick(job);
    if (job.status !== 'queued' && job.status !== 'running') return job;
    await new Promise((resolve, reject) => {
      const t = setTimeout(resolve, intervalMs);
      if (signal) signal.addEventListener('abort', () => { clearTimeout(t); reject(new DOMException('aborted', 'AbortError')); }, { once: true });
    });
  }
}

// ── DOM 헬퍼 ────────────────────────────────────────────────────────────────
export function el(tag, props = null, ...children) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(props || {})) {
    if (v === null || v === undefined || v === false) continue;
    if (k === 'class') node.className = v;
    else if (k === 'text') node.textContent = v;
    else if (k === 'html') node.innerHTML = v;
    else if (k.startsWith('on')) node.addEventListener(k.slice(2).toLowerCase(), v);
    else if (k === 'dataset') Object.assign(node.dataset, v);
    else node.setAttribute(k, v === true ? '' : String(v));
  }
  for (const child of children.flat(9)) {
    if (child === null || child === undefined || child === false) continue;
    node.append(child instanceof Node ? child : document.createTextNode(String(child)));
  }
  return node;
}

export const clear = node => { while (node.firstChild) node.removeChild(node.firstChild); return node; };

// ── 표시 서식 ───────────────────────────────────────────────────────────────
export const fmtNum = (v, digits = 2) => (Number.isFinite(v)
  ? (Number.isInteger(v) ? v.toLocaleString('ko-KR') : Number(v.toFixed(digits)).toLocaleString('ko-KR'))
  : '—');
export const fmtPct = v => (Number.isFinite(v) ? `${(v * 100).toFixed(1)}%` : '—');
export const fmtSignedPct = v => (Number.isFinite(v) ? `${v >= 0 ? '+' : ''}${(v * 100).toFixed(1)}%` : '—');
export const fmtPp = (a, b) => (Number.isFinite(a) && Number.isFinite(b) ? `${b - a >= 0 ? '+' : ''}${((b - a) * 100).toFixed(1)}%p` : '—');

export function fmtCell(v) {
  if (v === null || v === undefined) return '—';
  if (Array.isArray(v)) return v.length ? `[${v.length}개] ${v.slice(0, 3).join(', ')}${v.length > 3 ? '…' : ''}` : '[]';
  if (typeof v === 'object') return JSON.stringify(v).slice(0, 60);
  if (typeof v === 'boolean') return v ? '참' : '거짓';
  if (typeof v === 'number') return fmtNum(v, 4);
  return String(v);
}

export function fmtWhen(iso) {
  if (!iso) return '—';
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return String(iso);
  const pad = n => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

export const ROUTE_LABEL = { dt: 'dt', 'dt-curve': 'dt*', cdo: 'cdo', src: 'src' };
export const ROUTE_CLASS = { dt: 'dt', 'dt-curve': 'dtcurve', cdo: 'cdo', src: 'src' };
export const ROUTE_HELP = {
  dt: 'DataTable — 이 도구가 직접 적용합니다.',
  'dt-curve': '프로젝트 곡선 — params 를 밀고 3표를 리시드합니다.',
  cdo: '블루프린트 Class Defaults — 에디터에서 직접 고쳐야 합니다.',
  src: 'C++ 상수 — 코드 수정과 빌드가 필요합니다.',
};

export function sensChip(sens) {
  if (!sens) return el('span', { class: 'sens unscanned', text: '미스캔' });
  switch (sens.state) {
    case 'cliff': return el('span', { class: 'sens cliff', title: '한쪽으로 밀면 완주율이 통째로 무너집니다', text: `절벽 ${fmtPct(sens.effect)}` });
    case 'live': return el('span', { class: 'sens live', text: `살아있음 ±${fmtPct(sens.effect)}` });
    case 'dead': return el('span', { class: 'sens dead', title: '±20% 를 밀어도 곡선이 거의 안 움직입니다', text: `죽음 ${fmtPct(sens.effect)}` });
    default: return el('span', { class: 'sens unscanned', text: '미스캔' });
  }
}
