// 노브 탐색 — 찾고, 미리보고, 확인하고, 적용한다.
// 여기의 renderChangePanel 은 DataTable 화면도 함께 쓴다(같은 knob 문법 → 같은 안전장치).
import {
  api, pollJob, el, clear, fmtNum, fmtPct, fmtPp, fmtSignedPct, fmtWhen,
  sensChip, ROUTE_LABEL, ROUTE_CLASS, ROUTE_HELP, ApiError,
} from '../api.mjs';

const PROFILES = ['low', 'lowTap', 'mid', 'midNoTap', 'high', 'skilled'];

// ── 확인창 문구 (순수 함수 — DOM 없이 테스트한다) ─────────────────────────────
export function formatConfirmation({ knob, before, after, affectedTables = [], affectedFiles = [], backupRequired = true, expiresAt = null }) {
  const lines = [
    `${knob} 을 적용합니다.`,
    `현재 ${fmtNum(before, 4)} → 적용 후 ${fmtNum(after, 4)}`,
  ];
  if (affectedTables.length) lines.push(`영향 표: ${affectedTables.join(', ')}`);
  if (affectedFiles.length) lines.push(`수정 파일: ${affectedFiles.join(', ')}`);
  lines.push(backupRequired
    ? '적용 전에 원본을 백업하고, 실패하면 백업으로 되돌립니다.'
    : '백업 없이 진행합니다.');
  if (expiresAt) lines.push(`이 계획은 ${fmtWhen(expiresAt)} 까지만 유효합니다.`);
  return lines.join('\n');
}

// ── 미리보기 차트 ───────────────────────────────────────────────────────────
function lineChart(before, after, { width = 460, height = 170 } = {}) {
  const rows = (before || []).map(b => ({ tier: b.tier, before: b.day, after: (after || []).find(a => a.tier === b.tier)?.day }));
  if (!rows.length) return el('p', { class: 'note', text: '티어 진입일을 비교할 만큼 완주한 런이 없습니다 — 완주율부터 보세요.' });

  const pad = { l: 38, r: 10, t: 10, b: 22 };
  const days = rows.flatMap(r => [r.before, r.after]).filter(Number.isFinite);
  const maxDay = Math.max(...days, 1);
  const tiers = rows.map(r => r.tier);
  const x = t => pad.l + ((t - Math.min(...tiers)) / Math.max(1, Math.max(...tiers) - Math.min(...tiers))) * (width - pad.l - pad.r);
  const y = d => height - pad.b - (d / maxDay) * (height - pad.t - pad.b);
  const pathOf = key => rows.filter(r => Number.isFinite(r[key])).map((r, i) => `${i ? 'L' : 'M'}${x(r.tier).toFixed(1)},${y(r[key]).toFixed(1)}`).join(' ');

  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', `0 0 ${width} ${height}`);
  svg.setAttribute('width', '100%');
  svg.setAttribute('role', 'img');
  svg.setAttribute('aria-label', '티어별 진입일 before/after 비교');
  const mk = (tag, attrs, text) => {
    const n = document.createElementNS('http://www.w3.org/2000/svg', tag);
    for (const [k, v] of Object.entries(attrs)) n.setAttribute(k, v);
    if (text !== undefined) n.textContent = text;
    return n;
  };
  for (let i = 0; i <= 2; i++) {
    const gy = pad.t + ((height - pad.t - pad.b) / 2) * i;
    svg.append(mk('line', { x1: pad.l, x2: width - pad.r, y1: gy, y2: gy, stroke: '#e2d9c9' }));
    svg.append(mk('text', { x: 4, y: gy + 4, 'font-size': 10, fill: '#8a97a6' }, String(Math.round(maxDay * (1 - i / 2)))));
  }
  for (const r of rows) svg.append(mk('text', { x: x(r.tier), y: height - 6, 'font-size': 10, fill: '#8a97a6', 'text-anchor': 'middle' }, `T${r.tier}`));
  svg.append(mk('path', { d: pathOf('before'), fill: 'none', stroke: '#8a97a6', 'stroke-width': 2, 'stroke-dasharray': '4 3' }));
  svg.append(mk('path', { d: pathOf('after'), fill: 'none', stroke: '#b9761a', 'stroke-width': 2.5 }));
  for (const r of rows) {
    if (Number.isFinite(r.after)) svg.append(mk('circle', { cx: x(r.tier), cy: y(r.after), r: 3, fill: '#b9761a' }));
  }
  return el('figure', { class: 'chart' }, svg, el('figcaption', {},
    el('span', { class: 'legend before' }, '현재'), ' ', el('span', { class: 'legend after' }, '적용 후'),
    ' · 낮을수록 빨리 도달합니다'));
}

function metricRow(label, before, after, format, delta) {
  const same = before === after;
  return el('tr', {},
    el('th', { scope: 'row', text: label }),
    el('td', { class: 'n', text: format(before) }),
    el('td', { class: 'n', text: format(after) }),
    el('td', { class: `n ${same ? 'ro' : ''}`, text: delta }));
}

function previewTable(p) {
  const b = p.before;
  const a = p.after;
  const tbody = el('tbody', {},
    metricRow('완주율', b.completionRate, a.completionRate, fmtPct, fmtPp(b.completionRate, a.completionRate)),
    metricRow('인앱 시간(중앙)', b.medianInAppH, a.medianInAppH, v => (Number.isFinite(v) ? `${fmtNum(v, 2)}h` : '—'),
      Number.isFinite(b.medianInAppH) && Number.isFinite(a.medianInAppH) && b.medianInAppH
        ? fmtSignedPct((a.medianInAppH - b.medianInAppH) / b.medianInAppH) : '—'),
    metricRow('게이트 실패율', b.gateFailShare, a.gateFailShare, fmtPct, fmtPp(b.gateFailShare, a.gateFailShare)));
  return el('table', { class: 'grid' },
    el('thead', {}, el('tr', {},
      el('th', { text: '지표' }), el('th', { class: 'n', text: '현재' }),
      el('th', { class: 'n', text: '적용 후' }), el('th', { class: 'n', text: '차이' }))),
    tbody);
}

// 결과가 같아도 "영향 없음" 이라고 말하지 않는다 — 모델 밖일 수 있다.
function coverageNotes(change, preview) {
  const out = [];
  const consumed = (preview && preview.consumed) || change.consumed || {};
  if (consumed.uncertain) {
    out.push(el('div', { class: 'note warn' }, el('b', {}, '시뮬 소비 여부 불확실'),
      ' — sim 의 동적 표 이름을 정적으로 풀지 못했습니다. before/after 가 같아도 효과 없음으로 결론 내리지 마세요.'));
  } else if (consumed.column === false) {
    out.push(el('div', { class: 'note warn' }, el('b', {}, '모델 미소비 · 실플레이 확인 필요'),
      ` — 시뮬레이션이 ${consumed.table ? '이 컬럼을' : '이 표를'} 읽지 않습니다. 아래 before/after 가 같은 것이 정상이며 효과가 없다는 뜻이 아닙니다.`));
  }
  if (change.route === 'dt-curve') {
    out.push(el('div', { class: 'note info' }, el('b', {}, '곡선은 3표를 함께 바꿉니다'),
      ' — Game/Finance/IT. 시뮬은 Game 산업만 모델링하므로 Finance/IT 효과는 이 수치에 반영되지 않습니다.'));
  }
  if (change.sens && change.sens.state === 'cliff') {
    out.push(el('div', { class: 'note danger' }, el('b', {}, '절벽 노브'),
      ' — 한쪽 방향으로 크게 밀면 완주율이 통째로 무너집니다. 작은 폭으로 나눠 확인하세요.'));
  } else if (change.sens && change.sens.state === 'dead') {
    out.push(el('div', { class: 'note' }, el('b', {}, '죽은 노브'), ' — ±20% 를 밀어도 곡선이 거의 안 움직입니다.'));
  } else if (change.sens && change.sens.state === 'unscanned') {
    out.push(el('div', { class: 'note' }, el('b', {}, '미스캔 노브'), ' — 감도 근거가 없어 아래 before/after 만이 판단 재료입니다.'));
  }
  return out;
}

// ── 적용 확인 서랍 ──────────────────────────────────────────────────────────
function openConfirm(ctx, { change, planId, expiresAt, onApplied }) {
  const targets = change.targets || [];
  const first = targets[0] || {};
  const files = change.route === 'dt-curve'
    ? [`${change.source.paramsPath} + 리시드된 CSV`]
    : [change.source.csvRelPath || change.source.uassetPath || ''].filter(Boolean);
  const tables = change.route === 'dt-curve' ? change.source.tables : [change.table].filter(Boolean);

  const summary = formatConfirmation({
    knob: change.knob,
    before: change.route === 'dt-curve' ? change.curveBefore : first.before,
    after: change.route === 'dt-curve' ? change.curveAfter : first.after,
    affectedTables: tables,
    affectedFiles: files,
    backupRequired: true,
    expiresAt,
  });

  const check = el('input', { type: 'checkbox', id: 'confirm-check' });
  const runBtn = el('button', { class: 'btn primary', disabled: true }, '적용 실행');
  check.addEventListener('change', () => { runBtn.disabled = !check.checked; });

  const dialog = el('dialog', { class: 'drawer' },
    el('form', { method: 'dialog' },
      el('h3', { text: '이 변경을 실제로 적용합니다' }),
      el('pre', { class: 'confirm-summary', text: summary }),
      targets.length > 1
        ? el('div', { class: 'note warn' }, `${targets.length}개 행이 한꺼번에 바뀝니다.`)
        : null,
      change.allRows
        ? el('div', { class: 'note danger' }, el('b', {}, '전체 행 절대값'), ' — 이 컬럼의 곡선이 평평해집니다.')
        : null,
      ctx.boot.editorCount !== 1
        ? el('div', { class: 'note danger' }, `에디터가 ${ctx.boot.editorCount}개입니다 — 적용은 정확히 1개일 때만 가능합니다.`)
        : null,
      el('label', { class: 'confirm-check' }, check, ' 위 내용을 확인했고, 실패 시 백업으로 되돌린다는 것을 이해했습니다.'),
      el('div', { class: 'row', style: 'justify-content:flex-end;margin-top:10px' },
        el('button', { class: 'btn ghost', value: 'cancel' }, '취소'),
        runBtn)));

  runBtn.addEventListener('click', async event => {
    event.preventDefault();
    dialog.close();
    await applyPlan(ctx, planId, onApplied);
  });
  document.body.append(dialog);
  dialog.addEventListener('close', () => dialog.remove());
  dialog.showModal();
}

async function applyPlan(ctx, planId, onApplied) {
  ctx.setStatus('적용 중입니다 — 백업 → 파일 교체 → 리임포트 → 리드백. 끝날 때까지 에디터를 닫지 마세요.', 'busy');
  try {
    const { jobId } = await api('/api/apply', { method: 'POST', body: { planId, confirmed: true } });
    const job = await pollJob(jobId, { onTick: j => ctx.setStatus(j.status === 'queued' ? '적용 대기 중…' : '적용 중입니다…', 'busy') });
    if (job.status !== 'completed') {
      throw new ApiError(job.error?.code || 'APPLY_FAILED', job.error?.message || '적용이 실패했습니다.');
    }
    await ctx.reloadAll();
    onApplied?.(job.result);
    showApplyResult(ctx, job.result);
  } catch (e) {
    ctx.reportError(e);
  }
}

function showApplyResult(ctx, result) {
  const kind = result.status === 'applied' ? 'idle' : 'error';
  const text = {
    applied: `적용 완료 — ${(result.affectedTables || []).join(', ')} 리임포트와 리드백까지 확인했습니다.`,
    refused: `적용하지 않았습니다 — ${result.reason}`,
    'rolled-back': `되돌렸습니다 — ${result.reason}`,
    'recovery-required': `복구 필요 — ${result.reason}`,
  }[result.status] || `결과: ${result.status}`;
  ctx.setStatus(text, kind);

  if (result.status === 'recovery-required') {
    const panel = el('div', { class: 'note danger sticky-recovery' },
      el('b', {}, '복구 필요 — 자동으로 되돌리지 못했습니다'),
      el('ol', {}, ...(result.recovery || []).map(step => el('li', { text: step }))),
      el('p', {}, '백업: ', el('code', { class: 'mono', text: (result.backups || []).join(', ') })));
    document.querySelector('main')?.prepend(panel);
  }
}

// ── 공용 변경 패널 (노브 화면 + DataTable 화면) ─────────────────────────────
export function renderChangePanel(ctx, mount, { knob, allRows = false, defaultValue = '' } = {}) {
  clear(mount);
  if (!knob) {
    mount.append(el('div', { class: 'empty', text: '왼쪽에서 노브나 셀을 고르면 여기서 미리보고 적용할 수 있습니다.' }));
    return;
  }

  const valueInput = el('input', { type: 'text', value: defaultValue, placeholder: '예: 12000 · x1.2 · +20%', 'aria-label': '변경할 값' });
  const profileSel = el('select', { 'aria-label': '플레이 프로필' }, ...PROFILES.map(p => el('option', { value: p, selected: p === 'mid' || null, text: p })));
  const seedsSel = el('select', { 'aria-label': '시드 수' }, ...[5, 10, 30, 50].map(n => el('option', { value: n, selected: n === 30 || null, text: `${n}시드` })));
  const allRowsBox = el('input', { type: 'checkbox', checked: allRows || null });
  const runBtn = el('button', { class: 'btn primary' }, '미리보기');
  const result = el('div', { class: 'stack' });

  const form = el('form', { class: 'stack' },
    el('div', { class: 'row' },
      el('label', { class: 'field', style: 'flex:1' }, '값', valueInput),
      el('label', { class: 'field' }, '프로필', profileSel),
      el('label', { class: 'field' }, '시드', seedsSel)),
    el('label', { class: 'row', style: 'font-size:12.5px;color:var(--ink-2)' }, allRowsBox, ' 컬럼 전체에 같은 절대값 쓰기(곡선이 평평해집니다)'),
    el('div', { class: 'row' }, runBtn, el('span', { class: 'chip', text: knob })));

  form.addEventListener('submit', async event => {
    event.preventDefault();
    const signal = ctx.newAbort();
    clear(result);
    runBtn.disabled = true;
    ctx.setStatus('미리보기를 계산 중입니다 — 시뮬레이션을 두 번 돌립니다(현재/적용 후).', 'busy');
    try {
      const { jobId } = await api('/api/preview', {
        method: 'POST',
        signal,
        body: {
          knob, value: valueInput.value, profile: profileSel.value,
          seeds: Number(seedsSel.value), untilTier: 10, allRows: allRowsBox.checked,
        },
      });
      const job = await pollJob(jobId, {
        signal,
        onTick: j => ctx.setStatus(j.status === 'queued' ? '앞선 작업이 끝나기를 기다리는 중…' : '시뮬레이션 실행 중…', 'busy'),
      });
      if (job.status !== 'completed') throw new ApiError(job.error?.code || 'PREVIEW_FAILED', job.error?.message || '미리보기가 실패했습니다.');
      renderPreviewResult(ctx, result, job.result);
      ctx.setStatus('미리보기 완료 — 아래에서 영향을 확인한 뒤 적용하세요.');
    } catch (e) {
      if (e.name === 'AbortError') return;
      ctx.reportError(e);
      result.append(el('div', { class: 'note danger', text: e.message }));
    } finally {
      runBtn.disabled = false;
    }
  });

  mount.append(form, result);
  valueInput.focus();
}

function renderPreviewResult(ctx, mount, out) {
  clear(mount);
  const { change, preview, planId, expiresAt } = out;
  mount.append(...coverageNotes(change, preview));

  mount.append(el('div', { class: 'card' },
    el('header', {}, el('h3', { text: '시뮬레이션 비교' }),
      el('span', { class: 'spacer' }),
      el('span', { class: 'chip', text: `${preview.profile} · ${preview.seeds}시드` }),
      ctx.boot.calibration.gateOk ? null : el('span', { class: 'chip warn', title: '절대시간은 보정되지 않았습니다', text: '미보정' })),
    el('div', { class: 'body stack' },
      previewTable(preview),
      lineChart(preview.before.tierDay, preview.after.tierDay))));

  // 적용 검토 — 무엇이 어디로, 백업은 어디에.
  const review = el('div', { class: 'stack' });
  if (!change.appliable) {
    review.append(el('div', { class: 'note info' },
      el('b', {}, ROUTE_HELP[change.route]),
      change.source.location ? el('p', { class: 'mono', text: change.source.location }) : null,
      el('p', {}, '이 도구는 이 값을 직접 바꾸지 않습니다 — 위 위치를 열어 직접 수정하세요.')));
    const copy = el('button', { class: 'btn sm' }, '위치 복사');
    copy.addEventListener('click', () => navigator.clipboard?.writeText(change.source.location || change.knob));
    review.append(copy);
  } else {
    const files = change.route === 'dt-curve'
      ? [change.source.paramsPath, ...change.source.tables.map(t => `DataImport/${t}_Import.csv`)]
      : [change.source.csvRelPath || change.source.uassetPath];
    review.append(el('div', { class: 'card' },
      el('header', {}, el('h3', { text: '적용 검토' })),
      el('div', { class: 'body' }, el('dl', { class: 'kv' },
        el('dt', { text: '권위 파일' }), el('dd', { class: 'mono', text: files.join('\n') }),
        el('dt', { text: '영향 표' }), el('dd', { text: (change.route === 'dt-curve' ? change.source.tables : [change.table]).join(', ') }),
        el('dt', { text: '대상 행' }), el('dd', { text: change.route === 'dt-curve' ? '100행 × 3표 리시드' : `${(change.targets || []).length}행` }),
        el('dt', { text: '백업' }), el('dd', { text: '적용 직전 원본과 uasset 을 백업 폴더에 복사합니다.' }),
        el('dt', { text: '계획 만료' }), el('dd', { text: fmtWhen(expiresAt) })))));

    const applyBtn = el('button', { class: 'btn primary', disabled: ctx.boot.editorCount !== 1 || null }, '적용 검토하기');
    applyBtn.addEventListener('click', () => openConfirm(ctx, { change, planId, expiresAt }));
    review.append(el('div', { class: 'row' }, applyBtn,
      ctx.boot.editorCount === 1 ? null : el('span', { class: 'chip danger', text: '에디터가 정확히 1개일 때만 적용할 수 있습니다' })));
  }
  mount.append(review);
}

// ── 화면 ────────────────────────────────────────────────────────────────────
export function render(ctx, mount) {
  const rows = ctx.boot.catalog.rows;
  let selected = rows[0] || null;

  const search = el('input', { type: 'search', placeholder: '노브 이름 검색', 'aria-label': '노브 검색' });
  const routeSel = el('select', { 'aria-label': '라우트 필터' },
    el('option', { value: '', text: '라우트 전체' }),
    ...['dt', 'dt-curve', 'cdo', 'src'].map(r => el('option', { value: r, text: `${ROUTE_LABEL[r]} — ${ROUTE_HELP[r].split(' —')[0]}` })));
  const sensSel = el('select', { 'aria-label': '감도 필터' },
    el('option', { value: '', text: '감도 전체' }),
    ...[['cliff', '절벽'], ['live', '살아있음'], ['dead', '죽음'], ['unscanned', '미스캔']].map(([v, t]) => el('option', { value: v, text: t })));

  const list = el('div', { class: 'list' });
  const detail = el('div', { class: 'stack' });
  const panel = el('div', { class: 'stack' });

  function paint() {
    clear(list);
    const q = search.value.trim().toLowerCase();
    const shown = rows.filter(r => (!q || r.key.toLowerCase().includes(q))
      && (!routeSel.value || r.route === routeSel.value)
      && (!sensSel.value || r.sens.state === sensSel.value));
    if (!shown.length) list.append(el('div', { class: 'empty', text: '조건에 맞는 노브가 없습니다.' }));
    for (const r of shown) {
      const btn = el('button', { type: 'button', 'aria-pressed': selected && selected.key === r.key ? 'true' : 'false' },
        el('div', { class: 'k' }, el('b', { text: r.key }), el('span', { class: 'v', text: fmtValue(r.value) })),
        el('div', { class: 'meta' }, el('span', { class: `route ${ROUTE_CLASS[r.route]}`, text: ROUTE_LABEL[r.route] }), sensChip(r.sens)));
      btn.addEventListener('click', () => { selected = r; paint(); paintDetail(); });
      list.append(btn);
    }
  }

  function paintDetail() {
    clear(detail);
    if (!selected) return;
    detail.append(el('div', { class: 'card' },
      el('header', {}, el('h3', { text: selected.key }),
        el('span', { class: 'spacer' }),
        el('span', { class: `route ${ROUTE_CLASS[selected.route]}`, text: ROUTE_LABEL[selected.route] })),
      el('div', { class: 'body' }, el('dl', { class: 'kv' },
        el('dt', { text: '현재값' }), el('dd', { text: fmtValue(selected.value) }),
        el('dt', { text: '감도(±20%)' }), el('dd', {}, sensChip(selected.sens)),
        el('dt', { text: '바꾸는 법' }), el('dd', { class: 'mono', text: selected.how }),
        el('dt', { text: '라우트' }), el('dd', { text: ROUTE_HELP[selected.route] })))));
    renderChangePanel(ctx, panel, { knob: selected.key });
  }

  for (const control of [search, routeSel, sensSel]) control.addEventListener('input', paint);

  mount.append(
    el('div', { class: 'view-head' },
      el('h2', { text: '노브 탐색' }),
      el('p', { text: `노브 ${rows.length}종. dt 는 이 도구가 직접 적용하고, cdo/src 는 위치만 알려줍니다. 감도는 out/scan.json 의 ±20% 스캔 결과입니다.` })),
    el('div', { class: 'cols3' },
      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '노브' })),
        el('div', { class: 'body stack' }, search, el('div', { class: 'row' }, routeSel, sensSel)),
        list),
      el('div', { class: 'stack' }, detail, el('div', { class: 'card' }, el('header', {}, el('h3', { text: '미리보기' })), el('div', { class: 'body' }, panel))),
      el('div', { class: 'stack' }, ...ctx.boot.warnings.map(w => el('div', { class: `note ${w.code === 'EDITOR_MULTIPLE' ? 'danger' : 'warn'}`, text: w.message })))));

  paint();
  paintDetail();
}

function fmtValue(v) {
  if (v === null || v === undefined) return '—';
  if (Array.isArray(v)) return `[${v.slice(0, 4).map(x => fmtNum(x, 3)).join(', ')}${v.length > 4 ? `, +${v.length - 4}` : ''}]`;
  if (typeof v === 'object' && Array.isArray(v.range)) return `${fmtNum(v.range[0], 2)} ~ ${fmtNum(v.range[1], 2)} (${v.columns}컬럼)`;
  if (typeof v === 'number') return fmtNum(v, 4);
  return String(v);
}
