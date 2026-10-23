// 변경 기록 — 되돌리기 버튼은 없다. 무엇을 언제 어디에 썼고 백업이 어디 있는지가 전부다.
import { api, el, clear, fmtNum, fmtWhen } from '../api.mjs';

const STATUS = {
  applied: { chip: 'ok', label: '적용됨' },
  refused: { chip: '', label: '거부됨' },
  'rolled-back': { chip: 'warn', label: '되돌림' },
  'recovery-required': { chip: 'danger', label: '복구 필요' },
};

export function render(ctx, mount) {
  const body = el('div', { class: 'stack' });

  mount.append(
    el('div', { class: 'view-head' },
      el('h2', { text: '변경 기록' }),
      el('p', { text: '적용을 시도할 때마다 한 줄씩 쌓입니다. 원클릭 되돌리기는 없습니다 — 백업 경로와 이전 값으로 직접 되돌립니다.' })),
    body);

  load();

  async function load() {
    clear(body);
    try {
      const { records, warnings, total } = await api('/api/history?limit=200');
      for (const w of warnings || []) body.append(el('div', { class: 'note warn', text: w }));
      if (!records.length) {
        body.append(el('div', { class: 'card' }, el('div', { class: 'empty', text: '아직 적용 기록이 없습니다.' })));
        return;
      }
      body.append(el('div', { class: 'row' }, el('span', { class: 'chip', text: `${records.length} / 총 ${total}건` })));
      for (const r of records) body.append(recordCard(r));
    } catch (e) {
      ctx.reportError(e);
      body.append(el('div', { class: 'note danger', text: e.message }));
    }
  }
}

function recordCard(r) {
  const s = STATUS[r.status] || { chip: '', label: r.status };
  const targets = r.targets || [];
  return el('div', { class: 'card' },
    el('header', {},
      el('span', { class: `chip ${s.chip}`, text: s.label }),
      el('h3', { text: r.knob || '(노브 없음)' }),
      el('span', { class: 'spacer' }),
      el('span', { class: 'chip', text: fmtWhen(r.at) })),
    el('div', { class: 'body' }, el('dl', { class: 'kv' },
      el('dt', { text: '변경' }),
      el('dd', { text: targets.length
        ? targets.slice(0, 3).map(t => `${t.row}: ${fmtNum(t.before, 4)} → ${fmtNum(t.after, 4)}`).join(' · ') + (targets.length > 3 ? ` 외 ${targets.length - 3}행` : '')
        : '—' }),
      el('dt', { text: '영향 표' }), el('dd', { text: (r.affectedTables || []).join(', ') || '—' }),
      el('dt', { text: '파일' }), el('dd', { class: 'mono', text: (r.files || []).join('\n') || '—' }),
      el('dt', { text: '백업' }), el('dd', { class: 'mono', text: (r.backups || []).join('\n') || '—' }),
      r.reason ? el('dt', { text: '사유' }) : null, r.reason ? el('dd', { text: r.reason }) : null,
      el('dt', { text: '리임포트' }),
      el('dd', { text: r.reimport ? (r.reimport.ok ? `성공 — ${(r.reimport.tables || []).join(', ')}` : `실패 — ${r.reimport.error || ''}`) : '—' }),
      el('dt', { text: '작업 / 계획' }), el('dd', { class: 'mono', text: `${r.jobId || '—'} / ${r.planId || '—'}` }))));
}
