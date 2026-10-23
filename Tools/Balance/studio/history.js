'use strict';
// Balance Studio 변경 기록 — 저장소 자체는 Tools/_Shared/webshell/history.js 로 옮겼다.
// 여기 남은 것은 Balance 고유 지식뿐이다: 허용 상태 4종.
//
// 초기 버전에는 원클릭 되돌리기가 없다. 그래서 "무엇을 언제 어디에 썼고 백업이 어디 있는지" 가
// 사후 복구의 유일한 실마리다.
const { HistoryStore: SharedHistoryStore } = require('../../_Shared/webshell/history.js');

// 최종 상태는 이 4개뿐. "성공처럼 보이는 새 상태" 가 조용히 늘어나는 것을 막는다.
const HISTORY_STATUS = new Set(['applied', 'refused', 'rolled-back', 'recovery-required']);

class HistoryStore extends SharedHistoryStore {
  constructor(options = {}) {
    super({ statuses: HISTORY_STATUS, ...options });
  }
}

module.exports = { HistoryStore, HISTORY_STATUS };
