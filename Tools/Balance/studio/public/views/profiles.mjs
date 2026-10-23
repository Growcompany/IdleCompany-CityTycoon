// 프로필 비교 — 6개 플레이 유형이 각각 어디까지 가는지. 값은 out/scan.json 의 profileSweep 결과다.
import { el, fmtPct, fmtNum } from '../api.mjs';

const HELP = {
  low: '저숙련 · 탭 적음',
  lowTap: '저숙련 · 탭 많음',
  mid: '표준',
  midNoTap: '표준 · 탭 없음',
  high: '고숙련',
  skilled: '숙련 + 투자셋',
};

export function render(ctx, mount) {
  const profiles = ctx.boot.profiles || [];

  mount.append(
    el('div', { class: 'view-head' },
      el('h2', { text: '프로필 비교' }),
      el('p', { text: '역량 차이가 "시간 차이" 가 아니라 "완주 가능/불가" 로 갈리는지 보는 화면입니다. 완주율 0% 인 프로필은 중앙 시간이 비어 있는 것이 정상입니다.' })));

  if (!profiles.length) {
    mount.append(el('div', { class: 'card' }, el('div', { class: 'empty', text: 'out/scan.json 에 프로필 결과가 없습니다 — verify/scan.js 를 먼저 돌리세요.' })));
    return;
  }

  const rows = profiles.map(p => el('tr', {},
    el('th', { scope: 'row' }, p.profile, el('div', { style: 'font-weight:400;color:var(--ink-3);font-size:11.5px', text: HELP[p.profile] || '' })),
    el('td', { class: 'n', text: String(p.seeds ?? '—') }),
    el('td', { class: 'n' }, completionChip(p.completionRate)),
    el('td', { class: 'n', text: Number.isFinite(p.medianInAppH) ? `${fmtNum(p.medianInAppH, 2)}h` : '—' }),
    el('td', { class: 'n', text: p.medianTierReached ?? '—' }),
    el('td', { class: 'n', text: fmtPct(p.softlockRate) })));

  mount.append(el('div', { class: 'card' },
    el('header', {}, el('h3', { text: '6개 플레이 프로필' }),
      el('span', { class: 'spacer' }),
      ctx.boot.calibration.gateOk ? null : el('span', { class: 'chip warn', title: '절대시간은 실측 보정되지 않았습니다', text: '시간 미보정' })),
    el('div', { class: 'grid-wrap' },
      el('table', { class: 'grid' },
        el('thead', {}, el('tr', {},
          el('th', { text: '프로필' }), el('th', { class: 'n', text: '시드' }),
          el('th', { class: 'n', text: '완주율' }), el('th', { class: 'n', text: '인앱 시간(중앙)' }),
          el('th', { class: 'n', text: '도달 티어(중앙)' }), el('th', { class: 'n', text: '정체율' }))),
        el('tbody', {}, ...rows)))));

  const stuck = profiles.filter(p => (p.completionRate || 0) === 0);
  if (stuck.length) {
    mount.append(el('div', { class: 'note warn' },
      el('b', {}, `완주하지 못하는 프로필 ${stuck.length}개`),
      ` — ${stuck.map(p => p.profile).join(', ')}. 이 프로필들은 시간이 더 걸리는 것이 아니라 게이트에서 막혀 끝내지 못합니다.`));
  }
}

function completionChip(rate) {
  if (!Number.isFinite(rate)) return el('span', { text: '—' });
  const kind = rate >= 0.9 ? 'ok' : rate > 0 ? 'warn' : 'danger';
  return el('span', { class: `chip ${kind}`, text: fmtPct(rate) });
}
