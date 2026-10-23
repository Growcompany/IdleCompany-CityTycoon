'use strict';
// 변경/실행 기록 — append-only JSON Lines. (Balance Studio 에서 추출)
//
// 한 줄이 깨져도(쓰다 만 tail) 나머지를 못 읽으면 사후 복구의 실마리가 통째로 사라진다.
// 그래서 읽기는 줄 단위로 실패를 격리하고 경고로만 남긴다.
//
// 허용 상태 집합(statuses)은 앱마다 다르므로 생성자 옵션이다. 넘기지 않으면
// 비어 있지 않은 문자열이면 받는다 — "성공처럼 보이는 새 상태" 를 막는 건 앱의 책임이 된다.
const fs = require('node:fs');
const path = require('node:path');

class HistoryStore {
  constructor({ historyPath, now = () => new Date(), statuses = null, maxBytes = null, maxFiles = 5 } = {}) {
    if (!historyPath) throw new Error('HistoryStore: historyPath 가 필요합니다.');
    this.historyPath = historyPath;
    this.now = now;
    this.statuses = statuses;
    this.maxBytes = maxBytes;
    this.maxFiles = maxFiles;
    this.seq = 0;
  }

  append(record) {
    const status = record && record.status;
    const ok = this.statuses ? this.statuses.has(status) : (typeof status === 'string' && status !== '');
    if (!ok) {
      const allowed = this.statuses ? ` (허용: ${[...this.statuses].join(', ')})` : '';
      throw new Error(`history status 가 허용 목록 밖입니다: ${JSON.stringify(status)}${allowed}`);
    }
    if (this.seq === 0) this.seq = this.#countExisting();
    const saved = { seq: ++this.seq, at: this.now().toISOString(), ...record };
    fs.mkdirSync(path.dirname(this.historyPath), { recursive: true });
    this.#rotateIfNeeded();
    fs.appendFileSync(this.historyPath, `${JSON.stringify(saved)}\n`, 'utf8');
    return saved;
  }

  list({ limit = 50 } = {}) {
    let text;
    try {
      text = fs.readFileSync(this.historyPath, 'utf8');
    } catch (e) {
      if (e.code === 'ENOENT') return { records: [], warnings: [] };
      throw e;
    }
    const records = [];
    const warnings = [];
    const lines = text.split('\n');
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line) continue;
      try {
        records.push(JSON.parse(line));
      } catch {
        warnings.push(`history ${i + 1}번째 줄을 읽지 못해 건너뜁니다(쓰다 만 기록일 수 있습니다): ${this.historyPath}`);
      }
    }
    records.reverse(); // 최신순
    return { records: records.slice(0, limit), warnings, total: records.length };
  }

  // maxBytes 를 넘기면 .1 .2 ... 로 밀어내고 새 파일에 이어 쓴다.
  // 회전해도 seq 는 메모리 카운터로 이어진다 — 재기동 후에는 현재 파일 기준으로 다시 센다(문서화된 한계).
  #rotateIfNeeded() {
    if (!this.maxBytes) return;
    let size = 0;
    try {
      size = fs.statSync(this.historyPath).size;
    } catch {
      return; // 아직 없으면 회전할 것도 없다
    }
    if (size < this.maxBytes) return;
    const ext = path.extname(this.historyPath);
    const base = this.historyPath.slice(0, this.historyPath.length - ext.length);
    const nameAt = i => `${base}.${i}${ext}`;
    try { fs.rmSync(nameAt(this.maxFiles - 1), { force: true }); } catch { /* 없으면 그만 */ }
    for (let i = this.maxFiles - 2; i >= 1; i--) {
      try { fs.renameSync(nameAt(i), nameAt(i + 1)); } catch { /* 없으면 그만 */ }
    }
    try { fs.renameSync(this.historyPath, nameAt(1)); } catch { /* 회전 실패는 기록을 막지 않는다 */ }
  }

  #countExisting() {
    try {
      return fs.readFileSync(this.historyPath, 'utf8').split('\n').filter(l => l.trim()).length;
    } catch {
      return 0;
    }
  }
}

module.exports = { HistoryStore };
