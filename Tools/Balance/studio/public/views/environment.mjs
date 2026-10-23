// 환경 상태 — 지금 적용이 가능한 상태인지, 없는 산출물은 무엇인지, 그리고 도구 종료.
import { api, el, clear, fmtWhen } from '../api.mjs';

export function render(ctx, mount) {
  const boot = ctx.boot;
  const editors = boot.editorCount;

  const editorNote = editors === 1
    ? el('div', { class: 'note ok', text: '에디터가 정확히 1개 실행 중입니다 — 적용할 수 있습니다.' })
    : editors === 0
      ? el('div', { class: 'note warn' }, el('b', {}, '에디터가 실행 중이 아닙니다'), ' — 미리보기는 가능하지만 적용과 재추출은 할 수 없습니다.')
      : el('div', { class: 'note danger' }, el('b', {}, `에디터가 ${editors}개 실행 중입니다`),
        ' — 어느 인스턴스에 붙을지 보장되지 않고 uasset 저장이 실패합니다(Error Code 32). 여분을 종료하세요.');

  const shutdownBtn = el('button', { class: 'btn danger' }, '도구 종료');
  shutdownBtn.addEventListener('click', async () => {
    shutdownBtn.disabled = true;
    try {
      await api('/api/shutdown', { method: 'POST', body: {} });
      ctx.setStatus('도구를 종료했습니다. 이 탭은 닫아도 됩니다.', 'idle');
      document.querySelector('main')?.prepend(el('div', { class: 'note ok', text: '서버가 종료되었습니다. 다시 쓰려면 BalanceStudio.cmd 를 실행하세요.' }));
    } catch (e) {
      ctx.reportError(e);
      shutdownBtn.disabled = false;
    }
  });

  mount.append(
    el('div', { class: 'view-head' },
      el('h2', { text: '환경 상태' }),
      el('p', { text: '적용은 에디터가 정확히 1개일 때만 가능합니다. 없는 산출물은 숨기지 않고 그대로 보여줍니다.' })),

    el('div', { class: 'stack' },
      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '언리얼 에디터' })),
        el('div', { class: 'body stack' }, editorNote,
          el('button', { class: 'btn', onClick: () => ctx.reloadAll() }, '상태 다시 확인'))),

      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '데이터 스냅샷' })),
        el('div', { class: 'body stack' },
          el('dl', { class: 'kv' },
            el('dt', { text: '추출 시각' }), el('dd', { text: fmtWhen(boot.data.valuesMeta.extractedAt) }),
            el('dt', { text: '불러온 시각' }), el('dd', { text: fmtWhen(boot.loadedAt) }),
            el('dt', { text: '프로젝트' }), el('dd', { class: 'mono', text: boot.project.repoRoot }),
            el('dt', { text: '표 / 노브' }), el('dd', { text: `${boot.tables.total}종 / ${boot.catalog.rows.length}종` }),
            el('dt', { text: 'CSV 권위 표' }), el('dd', { text: `${boot.tables.csvBacked}종 (uasset 전용 ${boot.tables.uassetOnly}종)` })),
          el('button', { class: 'btn', onClick: () => ctx.refreshData() }, '에디터에서 값 다시 뽑기'))),

      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '없는 산출물' }),
          el('span', { class: 'spacer' }),
          el('span', { class: `chip ${boot.missing.length ? 'warn' : 'ok'}`, text: boot.missing.length ? `${boot.missing.length}건` : '없음' })),
        el('div', { class: 'body stack' },
          boot.missing.length
            ? el('div', { class: 'stack' }, ...boot.missing.map(m => el('div', { class: 'note warn' },
              el('b', { text: m.what }), ' — ', el('code', { class: 'mono', text: m.path }))))
            : el('div', { class: 'note ok', text: '필요한 산출물이 모두 있습니다.' }))),

      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '경고' })),
        el('div', { class: 'body stack' },
          ...boot.warnings.map(w => el('div', { class: `note ${w.code === 'EDITOR_MULTIPLE' ? 'danger' : 'warn'}` },
            el('b', { text: w.code }), ' — ', w.message)))),

      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '종료' })),
        el('div', { class: 'body stack' },
          el('div', { class: 'note', text: '종료하면 서버가 닫히고 이 페이지는 더 이상 동작하지 않습니다. 진행 중인 적용이 있으면 최대 5초 기다립니다.' }),
          shutdownBtn))));
}
