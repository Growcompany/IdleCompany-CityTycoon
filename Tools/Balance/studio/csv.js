'use strict';
// DataImport CSV 의 수치 셀 하나(또는 한 컬럼)만 고치는 계획 계층 — 파일을 쓰지 않는다.
//
// 이 표들은 UE 가 임포트하는 권위 파일이다. 범용 CSV 라이브러리로 파싱→재직렬화하면 인용 방식·EOL·BOM 이
// 조용히 정규화되어, 다음 리임포트에 "내가 안 한 변경"이 통째로 실린다. 그래서 필드를 raw(따옴표 포함)로
// 들고 있다가 손댄 필드만 다시 인코딩한다 — 나머지는 원문 바이트 그대로 join 된다.
const crypto = require('node:crypto');

class CsvPatchError extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.name = 'CsvPatchError';
    this.code = code;
    this.details = details;
  }
}

// ── 파싱 ────────────────────────────────────────────────────────────────────
// 따옴표 안의 개행은 줄바꿈이 아니다 — inQ 를 안 보면 UE 의 여러 줄 설명 필드에서 표가 통째로 어긋난다.
function splitPhysicalLines(text) {
  const out = [];
  let start = 0;
  let inQ = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '"') inQ = !inQ;
    else if (c === '\n' && !inQ) {
      const cr = i > start && text[i - 1] === '\r';
      out.push({ text: text.slice(start, cr ? i - 1 : i), eol: cr ? '\r\n' : '\n' });
      start = i + 1;
    }
  }
  if (start < text.length) out.push({ text: text.slice(start), eol: '' });
  return out;
}

function splitFields(line) {
  const out = [];
  let cur = '';
  let inQ = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (inQ) {
      cur += c;
      if (c === '"') {
        if (line[i + 1] === '"') { cur += '"'; i++; } else inQ = false;
      }
    } else if (c === '"') { cur += c; inQ = true; } else if (c === ',') { out.push(cur); cur = ''; } else cur += c;
  }
  out.push(cur);
  return out;
}

const decodeCsvField = raw =>
  (raw.length >= 2 && raw.startsWith('"') && raw.endsWith('"') ? raw.slice(1, -1).replace(/""/g, '"') : raw);

function encodeCsvField(value) {
  const s = String(value);
  return /[",\r\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
}

function parseCsvDocument(text) {
  if (typeof text !== 'string') throw new CsvPatchError('CSV_INPUT', 'CSV 텍스트(문자열)가 필요합니다.');
  const bom = text.startsWith('\uFEFF') ? '\uFEFF' : '';
  const body = bom ? text.slice(1) : text;
  const physical = splitPhysicalLines(body);
  if (!physical.length) throw new CsvPatchError('CSV_EMPTY', '빈 CSV 입니다.');

  const lines = physical.map(l => ({
    fields: splitFields(l.text).map(raw => ({ raw, value: decodeCsvField(raw) })),
    eol: l.eol,
    blank: l.text === '',
  }));

  const header = lines[0].fields.map(f => f.value);
  const rows = [];
  for (let i = 1; i < lines.length; i++) {
    if (lines[i].blank) continue; // 후행 빈 줄은 데이터가 아니다 — 그대로 남긴다
    rows.push({ lineIndex: i, key: lines[i].fields[0] ? lines[i].fields[0].value : '', fields: lines[i].fields });
  }
  return {
    bom,
    header,
    rowKeyColumn: header[0],
    lines,
    rows,
    rowNames: rows.map(r => r.key),
    finalNewline: lines[lines.length - 1].eol !== '',
  };
}

const serializeCsvDocument = doc =>
  doc.bom + doc.lines.map(l => l.fields.map(f => f.raw).join(',') + l.eol).join('');

const sha256 = text => crypto.createHash('sha256').update(text, 'utf8').digest('hex');

// ── 수치 판정 ───────────────────────────────────────────────────────────────
// 빈칸은 Number('') === 0 이라 그냥 두면 "값 없음"이 0 으로 둔갑한다. 불리언도 별도 사유로 막아
// "컬럼명 오타" 와 "이 컬럼은 원래 수치가 아님" 을 사용자가 구분할 수 있게 한다.
function numericOf(field) {
  const s = String(field.value).trim();
  if (s === '') return { ok: false, reason: '빈 칸' };
  if (/^(true|false)$/i.test(s)) return { ok: false, reason: '불리언' };
  const n = Number(s);
  if (!Number.isFinite(n)) return { ok: false, reason: '비수치' };
  return { ok: true, value: n, integer: Number.isInteger(n) && !/[.eE]/.test(s) };
}

// 부동소수 잡음(0.1×3 = 0.30000000000000004)이 권위 CSV 에 굳지 않게 유효숫자로 정리한다.
function formatNumber(n) {
  const s = String(Number(n.toPrecision(15)));
  return s.includes('e') ? n.toFixed(6).replace(/\.?0+$/, '') : s;
}

// ── 계획 ────────────────────────────────────────────────────────────────────
function planNumericCsvPatch(text, { column, row = null, spec, allRows = false } = {}) {
  if (!spec || (!('set' in spec) && !('mult' in spec))) {
    throw new CsvPatchError('CSV_SPEC', 'spec 은 { set } 또는 { mult } 여야 합니다.', { spec });
  }
  const doc = parseCsvDocument(text);
  const colIndex = doc.header.indexOf(column);
  if (colIndex < 0) {
    throw new CsvPatchError('CSV_COLUMN_NOT_FOUND', `CSV 에 없는 컬럼: ${column}`, { column, header: doc.header });
  }

  let targets;
  if (row === null || row === undefined) {
    targets = doc.rows;
  } else {
    const key = String(row);
    const hits = doc.rows.filter(r => r.key === key);
    if (!hits.length) {
      throw new CsvPatchError('CSV_ROW_NOT_FOUND', `CSV 에 없는 행: ${key} — 행 이름 오타?`, { row: key });
    }
    if (hits.length > 1) {
      throw new CsvPatchError('CSV_DUPLICATE_ROW', `행 key 가 ${hits.length}개 중복입니다: ${key} — 어느 행인지 확정할 수 없습니다.`, { row: key, count: hits.length });
    }
    targets = hits;
  }

  // 컬럼 전체일 때 비수치 행은 tune.js 와 같이 건너뛴다. 행을 지목했는데 비수치면 오타이므로 즉시 세운다.
  const picked = [];
  for (const t of targets) {
    const n = numericOf(t.fields[colIndex] || { value: '' });
    if (n.ok) { picked.push({ target: t, n }); continue; }
    if (row !== null && row !== undefined) {
      throw new CsvPatchError('CSV_NOT_NUMERIC', `수치 셀이 아닙니다: ${column}@${t.key} (${n.reason})`, { column, row: t.key, reason: n.reason });
    }
  }
  if (!picked.length) {
    throw new CsvPatchError('CSV_NOT_NUMERIC', `수치 행이 없는 컬럼입니다: ${column} — 컬럼명 오타?`, { column });
  }

  // 다행 + 절대값 = 곡선 평탄화. @행 을 빠뜨린 오타로 일어나기 쉬워 명시 동의를 요구한다(tune.js 와 동일 규칙).
  if ('set' in spec && picked.length > 1 && allRows !== true) {
    throw new CsvPatchError(
      'CSV_ALL_ROWS_REQUIRED',
      `${column} 의 ${picked.length}개 행에 같은 절대값(${spec.set})을 씁니다 — 곡선이 평평해집니다. 행을 지정하거나 배율을 쓰거나, 정말 전 행이면 '전체 행'을 확인하세요.`,
      { column, rows: picked.length, value: spec.set },
    );
  }

  const outLines = doc.lines.map(l => ({ ...l, fields: l.fields.slice() }));
  const cells = [];
  for (const { target, n } of picked) {
    let after = 'set' in spec ? spec.set : n.value * spec.mult;
    if (!Number.isFinite(after)) {
      throw new CsvPatchError('CSV_RESULT_NOT_FINITE', `계산 결과가 유한수가 아닙니다: ${column}@${target.key}`, { column, row: target.key, before: n.value, spec });
    }
    let rounded = false;
    if (n.integer && !Number.isInteger(after)) { after = Math.round(after); rounded = true; }

    const beforeRaw = target.fields[colIndex].raw;
    const afterRaw = encodeCsvField(formatNumber(after));
    const changed = afterRaw !== beforeRaw;
    if (changed) {
      outLines[target.lineIndex].fields[colIndex] = { raw: afterRaw, value: decodeCsvField(afterRaw) };
    }
    cells.push({ row: target.key, column, before: n.value, after, beforeRaw, afterRaw, rounded, changed });
  }

  const afterText = serializeCsvDocument({ ...doc, lines: outLines });
  return {
    column,
    row: row === undefined ? null : row,
    spec,
    allRows: allRows === true,
    header: doc.header,
    rowNames: doc.rowNames,
    rowCount: doc.rows.length,
    cells,
    changedCells: cells.filter(c => c.changed),
    beforeText: text,
    afterText,
    beforeSha256: sha256(text),
    afterSha256: sha256(afterText),
    status: afterText === text ? 'unchanged' : 'changed',
  };
}

// 기록 직후 되읽어 계획과 대조한다 — "쓴 줄 알았는데 안 써졌다" 를 커밋 전에 잡는 유일한 방법이다.
function verifyCsvPatch(text, expected) {
  const doc = parseCsvDocument(text);
  if (doc.header.length !== expected.header.length || doc.header.some((h, i) => h !== expected.header[i])) {
    throw new CsvPatchError('CSV_VERIFY_HEADER', `헤더가 계획과 다릅니다: ${doc.header.join(',')}`, { expected: expected.header, actual: doc.header });
  }
  if (doc.rowNames.length !== expected.rowNames.length || doc.rowNames.some((r, i) => r !== expected.rowNames[i])) {
    throw new CsvPatchError(
      'CSV_VERIFY_ROWS',
      `행 집합/순서가 계획과 다릅니다 (기대 ${expected.rowNames.length}행, 실제 ${doc.rowNames.length}행).`,
      { expectedRows: expected.rowNames.length, actualRows: doc.rowNames.length },
    );
  }
  const colIndex = doc.header.indexOf(expected.column);
  for (const cell of expected.cells) {
    const hit = doc.rows.find(r => r.key === cell.row);
    const n = hit ? numericOf(hit.fields[colIndex] || { value: '' }) : { ok: false };
    if (!n.ok || Math.abs(n.value - cell.after) > Math.max(1e-9, Math.abs(cell.after) * 1e-9)) {
      throw new CsvPatchError(
        'CSV_VERIFY_CELL',
        `셀이 계획대로 기록되지 않았습니다: ${expected.column}@${cell.row} — 기대 ${cell.after}, 실제 ${n.ok ? n.value : '(수치 아님)'}`,
        { column: expected.column, row: cell.row, expected: cell.after, actual: n.ok ? n.value : null },
      );
    }
  }
  return { ok: true, rowCount: doc.rows.length };
}

module.exports = {
  CsvPatchError,
  parseCsvDocument,
  serializeCsvDocument,
  planNumericCsvPatch,
  verifyCsvPatch,
  encodeCsvField,
  decodeCsvField,
};
