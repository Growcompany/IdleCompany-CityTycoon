'use strict';
// CSV 를 고친 뒤 에디터에 밀어넣는 어댑터.
//
// 유일한 성공 조건: "요청한 표 집합" == "에디터가 처리했다고 보고한 표 집합" + readback 존재.
// 종료 코드는 증거가 못 된다 — 원격 py 의 실패는 log_error 로 삼켜져 exit 0 으로 돌아온다.
// (2026-07-29 사고: Game 만 리임포트하면서 3표 CSV 를 바꿔 Finance/IT 가 어긋난 채 남았다.)
const fs = require('node:fs');
const path = require('node:path');
const { execFile } = require('node:child_process');
const { promisify } = require('node:util');

const pExecFile = promisify(execFile);

const MARKER = '[BALV-REIMPORT] RESULT ';
const DEFAULT_TIMEOUT_MS = 180_000; // 34표 중 큰 표는 왕복이 길다 — tune.js 의 60초보다 넉넉히

class ReimportError extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.name = 'ReimportError';
    this.code = code;
    this.details = details;
  }
}

// ── request ─────────────────────────────────────────────────────────────────
// 진짜 allowlist 는 manifest 의 dtPaths 다. 표 이름이 여기 없으면 어떤 경로도 만들지 않는다.
function buildReimportRequest({ csvDir, dtPaths, tables, expect = [], readbackColumns = [] }) {
  if (!csvDir) throw new ReimportError('REIMPORT_INPUT', 'csvDir 이 필요합니다.');
  if (!Array.isArray(tables) || !tables.length) throw new ReimportError('REIMPORT_INPUT', '리임포트 대상 표가 비었습니다.');

  const targets = tables.map(table => {
    const assetPath = dtPaths && Object.prototype.hasOwnProperty.call(dtPaths, table) ? dtPaths[table] : null;
    if (!assetPath) {
      throw new ReimportError('REIMPORT_TABLE_NOT_ALLOWED', `manifest_editor.json 에 없는 표는 리임포트할 수 없습니다: ${table}`, { table });
    }
    const csvName = `${table}_Import.csv`;
    const csvPath = path.join(csvDir, csvName);
    if (!fs.existsSync(csvPath)) {
      throw new ReimportError('REIMPORT_CSV_MISSING', `CSV 가 없어 리임포트할 것이 없습니다: ${csvPath}`, { table, csvPath });
    }
    return { table, csvName, assetPath };
  });

  return { csvDir, targets, expect, readbackColumns };
}

// ── runner ──────────────────────────────────────────────────────────────────
const safeId = id => String(id).replace(/[^\w.-]/g, '_');
const pyPath = p => String(p).replace(/\\/g, '/');

// 100행 요청을 커맨드라인에 실으면 Windows 인자 길이와 인용 규칙에 걸린다 — 파일로 넘긴다.
function writeReimportRunner({ scriptDir, jobId, request, reimportScriptPath }) {
  fs.mkdirSync(scriptDir, { recursive: true });
  const base = `balv_reimport_${safeId(jobId)}`;
  const requestPath = path.join(scriptDir, `${base}.json`);
  const runnerPath = path.join(scriptDir, `${base}.py`);

  fs.writeFileSync(requestPath, JSON.stringify(request), 'utf8');
  const runner = `# -*- coding: utf-8 -*-
# Balance Studio 생성 — job ${safeId(jobId)}. 요청 본문은 옆의 .json 에 있다.
import json, runpy

REQUEST_PATH = r"${pyPath(requestPath)}"
REIMPORT_PATH = r"${pyPath(reimportScriptPath)}"

with open(REQUEST_PATH, "r", encoding="utf-8") as handle:
    request = json.load(handle)
# run_path 는 __name__ 을 __main__ 으로 두지 않는다 — 스크립트가 제 기본 대상을 자동 실행하지 않는다.
module = runpy.run_path(REIMPORT_PATH)
module["reimport_targets"](request)
`;
  fs.writeFileSync(runnerPath, runner, 'utf8');
  return { runnerPath, requestPath };
}

// ── marker ──────────────────────────────────────────────────────────────────
function parseReimportResult(output, expectedTables) {
  const text = String(output || '');
  const at = text.lastIndexOf(MARKER); // 직전 실행의 낡은 마커에 속지 않는다
  if (at < 0) {
    throw new ReimportError('REIMPORT_NO_MARKER', '에디터가 결과 마커를 내지 않았습니다 — 리임포트가 실행되지 않았을 가능성이 큽니다. 에디터가 켜져 있고 Python 원격 실행이 활성인지 확인하세요.', {
      outputTail: text.slice(-500),
    });
  }
  const lineEnd = text.indexOf('\n', at);
  const json = text.slice(at + MARKER.length, lineEnd < 0 ? undefined : lineEnd).trim();
  let result;
  try {
    result = JSON.parse(json);
  } catch (e) {
    throw new ReimportError('REIMPORT_BAD_MARKER', `결과 마커의 JSON 을 읽지 못했습니다: ${e.message}`, { json: json.slice(0, 500) });
  }

  if (result.ok !== true) {
    throw new ReimportError('REIMPORT_FAILED', `에디터가 리임포트에 실패했습니다: ${result.error || '(사유 없음)'}`, {
      error: result.error || null, rollback: result.rollback || null, traceback: result.traceback || null,
    });
  }

  const want = [...expectedTables].sort();
  const got = [...(result.tables || [])].sort();
  if (want.length !== got.length || want.some((t, i) => t !== got[i])) {
    throw new ReimportError(
      'REIMPORT_TABLE_SET_MISMATCH',
      `리임포트 대상 집합이 어긋납니다 — 요청 [${want.join(', ')}], 처리 [${got.join(', ')}]. CSV 는 바뀌었는데 일부 표가 에디터에 안 실린 상태입니다.`,
      { expected: want, actual: got },
    );
  }
  if (!result.readback || Object.keys(result.readback).length !== want.length) {
    throw new ReimportError('REIMPORT_NO_READBACK', '저장 후 리드백이 없어 값이 실제로 들어갔는지 확인할 수 없습니다.', { readback: result.readback || null });
  }
  return result;
}

// ── run ─────────────────────────────────────────────────────────────────────
async function defaultExec({ command, args, cwd, timeoutMs }) {
  const { stdout, stderr } = await pExecFile(command, args, {
    encoding: 'utf8',
    cwd,
    timeout: timeoutMs,
    maxBuffer: 8 * 1024 * 1024,
    env: { ...process.env, PYTHONIOENCODING: 'utf-8' },
  });
  return `${stdout}\n${stderr}`;
}

async function runReimport({
  csvDir, dtPaths, tables, expect = [], readbackColumns = [],
  scriptDir, jobId, remoteCmdPath, reimportScriptPath, repoRoot,
  exec = defaultExec, timeoutMs = DEFAULT_TIMEOUT_MS,
}) {
  const request = buildReimportRequest({ csvDir, dtPaths, tables, expect, readbackColumns });
  const { runnerPath, requestPath } = writeReimportRunner({ scriptDir, jobId, request, reimportScriptPath });

  // shell 을 쓰지 않는다 — 인자는 배열로 그대로 넘어간다.
  const call = {
    command: 'py',
    args: [remoteCmdPath, `exec(compile(open(r'${pyPath(runnerPath)}').read(), r'${pyPath(runnerPath)}', 'exec'))`],
    cwd: repoRoot,
    timeoutMs,
  };

  let output;
  try {
    output = await exec(call);
  } catch (e) {
    // 원격이 죽어도 stdout 에 마커가 남았을 수 있다 — 그게 훨씬 구체적인 사유다.
    const salvaged = `${e.stdout || ''}\n${e.stderr || ''}`;
    if (salvaged.includes(MARKER)) return parseReimportResult(salvaged, tables);
    const why = e.killed || e.signal ? `${Math.round(timeoutMs / 1000)}초 타임아웃` : (e.code === 'ENOENT' ? '`py` 실행 파일을 못 찾음' : e.message);
    throw new ReimportError('REMOTE_EXEC_FAILED', `원격 실행 실패 — ${why}. 에디터가 켜져 있고 Python 원격 실행(bRemoteExecution)이 활성인지 확인하세요.`, {
      runnerPath, requestPath, outputTail: salvaged.slice(-500),
    });
  }
  const result = parseReimportResult(output, tables);
  return { ...result, runnerPath, requestPath, output };
}

module.exports = {
  buildReimportRequest, writeReimportRunner, parseReimportResult, runReimport,
  ReimportError, MARKER, DEFAULT_TIMEOUT_MS,
};
