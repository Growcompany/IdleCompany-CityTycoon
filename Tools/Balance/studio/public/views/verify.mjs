// 검증 결과 — 다시 계산하지 않고 기존 산출물을 읽어 보여준다.
// 미보정·모델 밖·미스캔을 성공처럼 칠하지 않는 것이 이 화면의 전부다.
import { el, fmtPct, fmtWhen } from '../api.mjs';

export function render(ctx, mount) {
  const boot = ctx.boot;
  const scan = boot.scan;

  mount.append(
    el('div', { class: 'view-head' },
      el('h2', { text: '검증 결과' }),
      el('p', { text: '기존 파이프라인(out/scan.json · out/calibration.json)의 결과를 그대로 읽습니다. 이 화면은 계산하지 않습니다.' })),

    el('div', { class: 'stack' },
      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '보정(calibration) 게이트' }),
          el('span', { class: 'spacer' }),
          el('span', { class: `chip ${boot.calibration.gateOk ? 'ok' : 'warn'}`, text: boot.calibration.gateOk ? '통과' : '미완료' })),
        el('div', { class: 'body stack' },
          boot.calibration.gateOk ? null : el('div', { class: 'note warn' },
            el('b', {}, '절대시간 지표는 미보정입니다'),
            ' — 인앱 시간과 티어 진입일의 절대값은 실측으로 보정되지 않았습니다. before/after 의 상대 변화만 판단 재료로 쓰세요.'),
          el('dl', { class: 'kv' },
            el('dt', { text: '생성 시각' }), el('dd', { text: fmtWhen(boot.calibration.generatedAt) })),
          ...(boot.calibration.assumptions || []).map(a => el('div', { class: 'note', text: a })))),

      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '감도 스캔' }),
          el('span', { class: 'spacer' }),
          el('span', { class: 'chip', text: `기준선 ${fmtWhen(boot.data.scanGeneratedAt)}` })),
        el('div', { class: 'body stack' },
          !scan
            ? el('div', { class: 'note warn', text: 'out/scan.json 이 없어 모든 노브가 미스캔입니다 — 죽은 노브와 절벽을 구분할 수 없습니다.' })
            : el('div', { class: 'row' },
              el('span', { class: 'chip danger', text: `절벽 ${scan.cliffs.length}` }),
              el('span', { class: 'chip', text: `죽음 ${scan.dead.length}` }),
              el('span', { class: 'chip ok', text: `측정 ${scan.rows.length}건` })),
          ...(scan ? scan.cliffs.map(c => el('div', { class: 'note danger' },
            el('b', {}, `절벽: ${c.knob}`),
            ` — ± 비대칭 ×${Number(c.asymmetry).toFixed(1)} (가파른 쪽 ${c.steepSide}). 한 방향으로 크게 밀면 완주율이 통째로 무너집니다.`)) : []),
          ...(scan ? scan.dead.map(d => el('div', { class: 'note', text: `죽은 노브: ${d.knob} — 밀어도 곡선이 거의 안 움직입니다.` })) : []))),

      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '시뮬레이션이 실제로 읽는 범위' })),
        el('div', { class: 'body stack' },
          el('div', { class: 'row' },
            el('span', { class: 'chip info', text: `표 ${boot.coverage.consumedTables} / ${boot.coverage.totalTables}` }),
            el('span', { class: 'chip warn', text: `프로젝트 산업 ${boot.coverage.industry || '?'} 단독` })),
          el('div', { class: 'note' },
            '나머지 표의 미리보기가 before/after 동일하게 나오는 것은 정상입니다 — 모델이 그 표를 읽지 않을 뿐이며 효과가 없다는 뜻이 아닙니다.'),
          boot.coverage.uncertain.length
            ? el('div', { class: 'note warn', text: `소비 여부 불확실: ${boot.coverage.uncertain.join(', ')}` })
            : null,
          el('details', {}, el('summary', { text: `모델이 읽는 표 ${boot.coverage.consumed.length}종 보기` }),
            el('p', { class: 'mono', text: boot.coverage.consumed.join(', ') })))),

      el('div', { class: 'card' },
        el('header', {}, el('h3', { text: '스냅샷' })),
        el('div', { class: 'body' }, el('dl', { class: 'kv' },
          el('dt', { text: '추출 시각' }), el('dd', { text: fmtWhen(boot.data.valuesMeta.extractedAt) }),
          el('dt', { text: 'source hash' }), el('dd', { class: 'mono', text: boot.data.valuesMeta.srcHash || '—' }),
          el('dt', { text: 'editor hash' }), el('dd', { class: 'mono', text: boot.data.valuesMeta.editorHash || '—' }),
          el('dt', { text: '경로' }), el('dd', { class: 'mono', text: boot.data.valuesRelPath }))))));
}
