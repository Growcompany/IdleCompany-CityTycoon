'use strict';
// 에디터에서 값을 다시 뽑아 out/values.json 을 갈아끼운다.
//
// values.json 은 Studio 가 보여주는 모든 수치의 원본이다. 반쯤 뽑힌 결과로 덮으면 도구 전체가
// 조용히 거짓말을 한다 → 추출 성공 마커 + 병합 결과 스키마를 둘 다 확인한 뒤에만 교체한다.
const fs = require('node:fs');
const path = require('node:path');
const { execFile } = require('node:child_process');
const { promisify } = require('node:util');

const pExecFile = promisify(execFile);

const MARKER_OK = '[BALV-EXTRACT] OK';
const MARKER_FAIL = '[BALV-EXTRACT] FAIL ';
const DEFAULT_TIMEOUT_MS = 300_000; // 34표 전량 export + CDO — tune 왕복보다 오래 걸린다

class RefreshError extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.name = 'RefreshError';
    this.code = code;
    this.details = details;
  }
}

const safeId = id => String(id).replace(/[^\w.-]/g, '_');
const pyPath = p => String(p).replace(/\\/g, '/');

async function defaultExec({ command, args, cwd, timeoutMs }) {
  const { stdout, stderr } = await pExecFile(command, args, {
    encoding: 'utf8', cwd, timeout: timeoutMs, maxBuffer: 8 * 1024 * 1024,
    env: { ...process.env, PYTHONIOENCODING: 'utf-8' },
  });
  return `${stdout}\n${stderr}`;
}

function createRefreshService({
  paths, model, plans, editorCheck, exec = defaultExec,
  writeValuesJson = require('../extract/merge_values.js').writeValuesJson,
  timeoutMs = DEFAULT_TIMEOUT_MS,
  now = () => new Date(),
}) {
  function writeRunner(jobId) {
    fs.mkdirSync(paths.scriptDir, { recursive: true });
    const runnerPath = path.join(paths.scriptDir, `balv_extract_${safeId(jobId)}.py`);
    fs.writeFileSync(runnerPath, `# -*- coding: utf-8 -*-
# Balance Studio 생성 — job ${safeId(jobId)}. 읽기 전용 추출.
import runpy

BALV_CONFIG = {
    "manifestPath": r"${pyPath(paths.manifestEditorPath)}",
    "outputPath": r"${pyPath(paths.extractOutputPath)}",
}
# run_path 는 __name__ 을 __main__ 으로 두지 않는다 — 스크립트가 제 기본 경로로 자동 실행되지 않는다.
module = runpy.run_path(r"${pyPath(paths.extractScriptPath)}")
module["extract_editor"](BALV_CONFIG)
`, 'utf8');
    return runnerPath;
  }

  function assertMarker(output) {
    const text = String(output || '');
    const failAt = text.lastIndexOf(MARKER_FAIL);
    if (failAt >= 0) {
      const lineEnd = text.indexOf('\n', failAt);
      const raw = text.slice(failAt + MARKER_FAIL.length, lineEnd < 0 ? undefined : lineEnd).trim();
      let why = raw;
      try { why = JSON.parse(raw); } catch { /* 따옴표가 깨졌으면 원문 그대로 */ }
      throw new RefreshError('EXTRACT_FAILED', `에디터 값 추출이 실패했습니다 — ${why}`, { outputTail: text.slice(-500) });
    }
    if (!text.includes(MARKER_OK)) {
      throw new RefreshError('EXTRACT_NO_MARKER', `에디터가 추출 성공 마커(${MARKER_OK})를 내지 않았습니다 — 추출이 실행되지 않았을 가능성이 큽니다. 기존 값을 그대로 둡니다.`, {
        outputTail: text.slice(-500),
      });
    }
  }

  // 병합 결과가 "쓸 수 있는 스냅샷" 인지 본다. 여기서 통과 못 하면 기존 파일은 손도 대지 않는다.
  function assertMergedSchema(merged) {
    if (!merged || typeof merged !== 'object') throw new RefreshError('MERGE_SHAPE', '병합 결과가 객체가 아닙니다.');
    if (!merged.values || Object.keys(merged.values).length === 0) {
      throw new RefreshError('MERGE_NO_VALUES', '병합 결과에 values 가 비었습니다 — 소스 추출이 실패했을 수 있습니다. 기존 값을 그대로 둡니다.');
    }
    if (!merged.dt || typeof merged.dt !== 'object') {
      throw new RefreshError('MERGE_NO_DT', '병합 결과에 dt 가 없습니다.');
    }
    const manifest = JSON.parse(fs.readFileSync(paths.manifestEditorPath, 'utf8'));
    const expected = Object.keys(manifest.dtPaths || {});
    const missing = expected.filter(t => !Array.isArray(merged.dt[t]) || merged.dt[t].length === 0);
    if (missing.length) {
      throw new RefreshError('MERGE_MISSING_TABLES', `표가 빠졌거나 비었습니다: ${missing.join(', ')} — 부분 추출본으로 덮지 않습니다.`, { missing });
    }
    return expected.length;
  }

  async function run({ jobId = 'refresh' } = {}) {
    const editorCount = Number(editorCheck()) || 0;
    if (editorCount === 0) {
      throw new RefreshError('EDITOR_NONE', 'UnrealEditor 가 실행 중이 아닙니다 — 값을 다시 뽑으려면 에디터가 켜져 있어야 합니다.');
    }
    if (editorCount >= 2) {
      throw new RefreshError('EDITOR_MULTIPLE', `UnrealEditor 가 ${editorCount}개 실행 중입니다 — 어느 에디터의 값인지 알 수 없어 추출하지 않습니다. 여분을 종료하세요.`);
    }

    const runnerPath = writeRunner(jobId);
    let output;
    try {
      output = await exec({
        command: 'py',
        args: [paths.remoteCmdPath, `exec(compile(open(r'${pyPath(runnerPath)}').read(), r'${pyPath(runnerPath)}', 'exec'))`],
        cwd: paths.repoRoot,
        timeoutMs,
      });
    } catch (e) {
      const salvaged = `${e.stdout || ''}\n${e.stderr || ''}`;
      if (salvaged.includes(MARKER_OK) || salvaged.includes(MARKER_FAIL)) output = salvaged;
      else {
        const why = e.killed || e.signal ? `${Math.round(timeoutMs / 1000)}초 타임아웃` : (e.code === 'ENOENT' ? '`py` 실행 파일을 못 찾음' : e.message);
        throw new RefreshError('REMOTE_EXEC_FAILED', `원격 실행 실패 — ${why}. 에디터가 켜져 있고 Python 원격 실행(bRemoteExecution)이 활성인지 확인하세요.`, {
          outputTail: salvaged.slice(-500),
        });
      }
    }
    assertMarker(output);

    // 병합은 temp 로 먼저 — 여기서 실패해도 현재 values.json 은 그대로다.
    const tempValuesPath = path.join(paths.scriptDir, `balv_values_${safeId(jobId)}.json`);
    let merged;
    try {
      merged = writeValuesJson(tempValuesPath, paths.manifestSrcPath, paths.repoRoot, paths.extractOutputPath);
    } catch (e) {
      throw new RefreshError('MERGE_FAILED', `추출본 병합에 실패했습니다 — 기존 값을 그대로 둡니다. ${e.message}`);
    }
    let tables;
    try {
      tables = assertMergedSchema(merged);
    } catch (e) {
      fs.rmSync(tempValuesPath, { force: true });
      throw e;
    }

    // 교체 — 이전 스냅샷을 남겨 둔다(재추출이 값을 이상하게 만들었을 때 되돌릴 유일한 수단).
    const stamp = now().toISOString().replace(/[-:T]/g, '').slice(0, 14);
    const backupPath = path.join(paths.backupRoot, `values-${stamp}.json`);
    fs.mkdirSync(path.dirname(backupPath), { recursive: true });
    if (fs.existsSync(paths.valuesPath)) fs.copyFileSync(paths.valuesPath, backupPath);
    fs.copyFileSync(tempValuesPath, paths.valuesPath);
    fs.rmSync(tempValuesPath, { force: true });

    // 새 스냅샷 위에서 옛 계획은 전부 무효다 — before/after 가 이미 달라졌다.
    model.invalidate();
    plans.invalidateAll('데이터 재추출');

    return {
      ok: true,
      tables,
      backupPath,
      valuesPath: paths.valuesPath,
      extractedAt: (merged.meta && merged.meta.extractedAt) || null,
      meta: merged.meta || null,
    };
  }

  return { run };
}

module.exports = { createRefreshService, RefreshError, MARKER_OK, MARKER_FAIL };
