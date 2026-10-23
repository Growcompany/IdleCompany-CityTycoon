// DataTable — 스프레드시트로 보고, 한 번에 셀 하나 또는 컬럼 하나만 변경 계획으로 잡는다.
import { api, el, clear, fmtCell, fmtNum } from '../api.mjs';
import { renderChangePanel } from './knobs.mjs';

export function render(ctx, mount) {
  let current = null;   // 열려 있는 표
  let draft = null;     // { column, row|null } — 동시에 하나만
  // 기본 켬 — DT_BoostGamble 같은 표는 NSLOCTEXT 컬럼 5개가 앞을 막아 정작 고칠 수치가 화면 밖으로 밀린다.
  let numericOnly = true;

  const search = el('input', { type: 'search', placeholder: '표 이름 검색', 'aria-label': '표 검색' });
  const list = el('div', { class: 'list' });
  const head = el('div', { class: 'stack' });
  const gridWrap = el('div', { class: 'grid-wrap' });
  const panel = el('div', { class: 'stack' });

  function paintList() {
    clear(list);
    const q = search.value.trim().toLowerCase();
    const shown = ctx.tables.filter(t => !q || t.name.toLowerCase().includes(q));
    if (!shown.length) list.append(el('div', { class: 'empty', text: '조건에 맞는 표가 없습니다.' }));
    for (const t of shown) {
      const btn = el('button', { type: 'button', 'aria-pressed': current && current.name === t.name ? 'true' : 'false' },
        el('div', { class: 'k' }, el('b', { text: t.name }), el('span', { class: 'v', text: `${t.rowCount}행` })),
        el('div', { class: 'meta' },
          el('span', { class: `route ${t.source === 'csv' ? 'dt' : 'cdo'}`, text: t.source === 'csv' ? 'CSV 소스' : 'uasset' }),
          el('span', { text: t.simConsumed ? '모델 소비' : '모델 밖' })));
      btn.addEventListener('click', () => open(t.name));
      list.append(btn);
    }
  }

  async function open(name) {
    ctx.setStatus(`${name} 을 여는 중입니다…`, 'busy');
    try {
      current = await api(`/api/tables/${encodeURIComponent(name)}`);
      draft = null;
      paintList();
      paintTable();
      renderChangePanel(ctx, panel, {});
      ctx.setStatus(`${name} — ${current.rowCount}행 · 수치 컬럼만 편집할 수 있습니다.`);
    } catch (e) {
      ctx.reportError(e);
    }
  }

  function pick(column, row) {
    draft = { column, row };
    paintTable();
    renderChangePanel(ctx, panel, {
      knob: `dt:${current.name}.${column}${row === null ? '' : `@${row}`}`,
      allRows: row === null,
    });
  }

  function paintTable() {
    clear(head);
    clear(gridWrap);
    if (!current) {
      gridWrap.append(el('div', { class: 'empty', text: '왼쪽에서 표를 고르세요.' }));
      return;
    }

    const allCols = current.columns.filter(c => c.name !== current.rowKeyColumn);
    const numericCols = allCols.filter(c => c.editable);
    const toggle = el('input', { type: 'checkbox', checked: numericOnly || null });
    toggle.addEventListener('change', () => { numericOnly = toggle.checked; paintTable(); });

    head.append(el('div', { class: 'row' },
      el('h3', { text: current.name }),
      el('span', { class: `chip ${current.source === 'csv' ? 'info' : 'warn'}` },
        current.source === 'csv' ? 'CSV 가 권위' : 'uasset 만 존재'),
      current.consumed.table ? el('span', { class: 'chip ok', text: '시뮬 소비' }) : el('span', { class: 'chip', text: '시뮬 모델 밖' }),
      el('span', { class: 'spacer', style: 'flex:1' }),
      el('label', { class: 'row', style: 'font-size:12.5px;color:var(--ink-2);gap:5px' },
        toggle, ` 수치 컬럼만 (${numericCols.length}/${allCols.length})`)));
    head.append(el('div', { class: 'note' },
      current.source === 'csv'
        ? el('span', {}, '원본: ', el('code', { class: 'mono', text: current.csvRelPath }), ' — 이 도구는 CSV 를 고치고 같은 표를 리임포트합니다.')
        : el('span', {}, 'CSV 가 없어 에디터 라운드트립으로 uasset 을 직접 고칩니다.')));
    if (!current.consumed.table) {
      head.append(el('div', { class: 'note warn' }, el('b', {}, '시뮬 모델 밖'),
        ' — 미리보기의 before/after 가 같은 것이 정상입니다. 실제 효과는 인게임에서만 확인할 수 있습니다.'));
    }

    // 수치 컬럼이 하나도 없는 표에서 토글을 켜면 빈 표가 된다 — 그럴 땐 전체를 보여준다.
    const shownCols = numericOnly && numericCols.length ? numericCols : allCols;
    const table = el('table', { class: 'grid' });
    const headRow = el('tr', {}, el('th', { scope: 'col', text: current.rowKeyColumn }));
    for (const c of shownCols) {
      const picked = draft && draft.column === c.name;
      const th = el('th', {
        scope: 'col',
        class: picked ? 'picked' : null,
        title: c.editable
          ? `${c.type} · ${fmtNum(c.min, 3)} ~ ${fmtNum(c.max, 3)} — 컬럼 이름을 누르면 전체 행이 대상이 됩니다`
          : c.readOnlyReason,
      }, c.name, c.editable ? null : el('span', { class: 'chip', style: 'margin-left:6px', text: '읽기' }));
      if (c.editable) {
        th.style.cursor = 'pointer';
        th.addEventListener('click', () => pick(c.name, null));
      }
      headRow.append(th);
    }
    table.append(el('thead', {}, headRow));

    const tbody = el('tbody', {});
    for (const row of current.rows) {
      const tr = el('tr', {}, el('th', { scope: 'row', text: row.name }));
      for (const c of shownCols) {
        const value = row.cells[c.name];
        const picked = draft && draft.column === c.name && (draft.row === row.name || draft.row === null);
        const numeric = typeof value === 'number';
        const shown = fmtCell(value);
        const td = el('td', {
          class: [
            numeric ? 'n' : 't',
            c.editable ? 'editable' : 'ro',
            picked ? 'picked' : '',
          ].filter(Boolean).join(' '),
          // 읽기 전용 셀은 잘려 보이므로 전문을 title 로 준다. 편집 가능 셀은 무엇이 일어날지 알려준다.
          title: c.editable ? '누르면 이 셀 하나가 대상이 됩니다' : `${shown}\n(${c.readOnlyReason})`,
          text: shown,
        });
        if (c.editable) td.addEventListener('click', () => pick(c.name, row.name));
        tr.append(td);
      }
      tbody.append(tr);
    }
    table.append(tbody);
    gridWrap.append(table);
  }

  search.addEventListener('input', paintList);

  mount.append(
    el('div', { class: 'view-head' },
      el('h2', { text: 'DataTable' }),
      el('p', { text: `표 ${ctx.tables.length}종. 수치 컬럼만 편집할 수 있고, 배열·텍스트·불리언 컬럼은 초기 버전에서 읽기 전용입니다. 한 번에 셀 하나 또는 컬럼 하나만 계획으로 잡습니다.` })),
    el('div', { class: 'cols2' },
      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '표' })),
        el('div', { class: 'body' }, search),
        list),
      el('div', { class: 'stack' },
        el('div', { class: 'card' }, el('div', { class: 'body stack' }, head), gridWrap),
        el('div', { class: 'card' }, el('header', {}, el('h3', { text: '미리보기' })), el('div', { class: 'body' }, panel)))));

  paintList();
  paintTable();
  renderChangePanel(ctx, panel, {});
  // 빈 화면보다 "무엇을 만질 수 있는지" 를 먼저 보여준다. 알파벳 첫 표는 하필 텍스트만 있는
  // uasset 표라, 실제로 튜닝할 수 있는 표(CSV 권위 + 시뮬 소비 + 수치 컬럼)를 먼저 연다.
  const first = ctx.tables.find(t => t.source === 'csv' && t.simConsumed && t.numericColumns.length) || ctx.tables[0];
  if (first) open(first.name);
}
