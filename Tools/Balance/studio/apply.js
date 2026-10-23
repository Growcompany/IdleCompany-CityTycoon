'use strict';
// 실제로 권위 파일을 바꾸는 유일한 계층.
//
// 순서는 어떤 라우트에서도 같다:
//   Editor 정확히 1개 → 지문 재확인 → temp 에 쓰고 되읽어 검증 → 백업 → 순차 교체
//   → 대상 한정 리임포트(리드백 포함) → history
//
// 실패는 반드시 되돌린다. 되돌리기까지 실패하면 성공 배지를 달지 않고 '복구 필요' 를 남긴다.
// ⚠ 실행 중인 에디터 아래의 uasset 을 OS 복사로 덮지 않는다 — 에디터 메모리가 그걸 다시 덮어써
//    "복구했다" 는 보고가 거짓이 된다. 디스크 백업 위치와 수동 절차만 알려준다.
const fs = require('node:fs');
const path = require('node:path');

const { planNumericCsvPatch, parseCsvDocument } = require('./csv.js');
const { runReimport } = require('./reimport.js');
const { CURVE_PARAM } = require('./model.js');
const reseedDefault = require('../reseed_project_curve.js');
const tuneDefault = require('../tune.js');

const TUNE_TIMEOUT_MS = 180_000;

class ApplyError extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.name = 'ApplyError';
    this.code = code;
    this.details = details;
  }
}

const defaultFsOps = {
  read: p => fs.readFileSync(p, 'utf8'),
  write: (p, text) => { fs.mkdirSync(path.dirname(p), { recursive: true }); fs.writeFileSync(p, text, 'utf8'); },
  replace: (from, to) => fs.renameSync(from, to),
  copy: (from, to) => { fs.mkdirSync(path.dirname(to), { recursive: true }); fs.copyFileSync(from, to); },
  exists: p => fs.existsSync(p),
  remove: p => { try { fs.rmSync(p, { force: true }); } catch { /* 이미 없으면 그만 */ } },
  mkdir: p => fs.mkdirSync(p, { recursive: true }),
};

const safeId = id => String(id).replace(/[^\w.-]/g, '_');
const pyPath = p => String(p).replace(/\\/g, '/');
const stampOf = d => d.toISOString().replace(/[-:T]/g, '').slice(0, 14);

// 배율 누적으로 생기는 부동소수 꼬리(120×1.2 = 144.00000000000003)를 params SOT 에 굳히지 않는다.
const cleanNumber = n => Number(Number(n).toPrecision(12));

// 리드백 대조용 표본 — 100행을 다 실으면 요청이 커지고, 0행이면 리임포트가 헛돌아도 모른다.
function sampleCurveExpect(target, columns) {
  const doc = parseCsvDocument(target.afterText);
  const picks = [0, Math.floor(doc.rows.length / 2), doc.rows.length - 1].filter((v, i, a) => v >= 0 && a.indexOf(v) === i);
  const out = [];
  for (const i of picks) {
    const row = doc.rows[i];
    if (!row) continue;
    for (const column of columns) {
      const at = doc.header.indexOf(column);
      if (at < 0) continue;
      const value = Number(row.fields[at].value);
      if (Number.isFinite(value)) out.push({ table: target.table, row: row.key, column, value });
    }
  }
  return out;
}

function createApplyService({
  paths, model, plans, history,
  editorCheck,
  exec,                      // 리임포트용 원격 실행 (async)
  tuneExec,                  // uasset 라운드트립용 원격 실행 (async)
  tune = tuneDefault,
  reseed = reseedDefault,
  now = () => new Date(),
}) {
  const CURVE_READBACK = ['DevelopmentCost', 'RequiredScore_Step1'];

  function record(base, extra) {
    const rec = { ...base, ...extra };
    history.append({
      jobId: rec.jobId, planId: rec.planId, status: rec.status, route: rec.route, knob: rec.knob,
      table: rec.table, column: rec.column, row: rec.row, spec: rec.spec, targets: rec.targets,
      reason: rec.reason || null, affectedTables: rec.affectedTables || [], backups: rec.backups || [],
      files: rec.files || [], fingerprint: rec.fingerprint, reimport: rec.reimport || null,
      editorCount: rec.editorCount, recovery: rec.recovery || null,
    });
    // 어떤 결과든 원본 상태가 달라졌을 수 있다 — 남은 계획과 읽기 캐시를 전부 버린다.
    plans.invalidateAll(`apply ${rec.status}`);
    model.invalidate();
    return rec;
  }

  function restoreFromBackup(files, backupDir, fsOps) {
    for (const f of files) {
      const backup = path.join(backupDir, path.basename(f.path));
      if (!fsOps.exists(backup)) throw new ApplyError('RESTORE_MISSING_BACKUP', `백업이 없어 되돌릴 수 없습니다: ${backup}`);
      const tmp = `${f.path}.balvrestore`;
      fsOps.copy(backup, tmp);
      fsOps.replace(tmp, f.path);
    }
  }

  // ── 라우트별 파일 계획 ────────────────────────────────────────────────────
  function planCsvCell(change, fsOps) {
    const text = fsOps.read(change.source.csvPath);
    const patch = planNumericCsvPatch(text, {
      column: change.column, row: change.row, spec: change.spec, allRows: change.allRows,
    });
    if (patch.status === 'unchanged') return null;
    return {
      files: [{ path: change.source.csvPath, afterText: patch.afterText, kind: 'csv', rowCount: patch.rowCount, header: patch.header }],
      tables: [change.table],
      expect: patch.cells.slice(0, 200).map(c => ({ table: change.table, row: c.row, column: c.column, value: c.after })),
      readbackColumns: [change.column],
    };
  }

  function planCurve(change, fsOps) {
    const param = CURVE_PARAM[change.knob];
    if (!param) throw new ApplyError('CURVE_UNKNOWN_PARAM', `곡선 노브에 대응하는 파라미터가 없습니다: ${change.knob}`);
    const params = JSON.parse(fsOps.read(paths.curveParamsPath));
    const next = { ...params, [param]: cleanNumber(params[param] * change.spec.mult) };
    const curve = reseed.planProjectCurve({ csvDir: paths.csvDir, params: next });
    if (!curve.affectedTargets.length) return null;

    const files = [
      { path: paths.curveParamsPath, afterText: `${JSON.stringify(next, null, 2)}\n`, kind: 'json' },
      ...curve.affectedTargets.map(t => ({
        path: t.csvPath, afterText: t.afterText, kind: 'csv', rowCount: t.rowCount,
        header: parseCsvDocument(t.afterText).header,
      })),
    ];
    return {
      files,
      tables: curve.affectedTargets.map(t => t.table),
      expect: curve.affectedTargets.flatMap(t => sampleCurveExpect(t, CURVE_READBACK)),
      readbackColumns: CURVE_READBACK,
      curve: { param, before: params[param], after: next[param] },
    };
  }

  // ── CSV 없는 DT: 에디터 라운드트립 ────────────────────────────────────────
  // tune.js 의 패치 스크립트를 그대로 쓴다(라운드트립 무손실 검증·저장 검증이 이미 그 안에 있다).
  // 다만 runApply 는 execFileSync 라 HTTP 루프를 최대 60초 세운다 — 스크립트만 빌려 비동기로 돌린다.
  async function applyUasset(change, base, { jobId, fsOps }) {
    const stamp = stampOf(now());
    const backupDir = path.join(paths.backupRoot, `${stamp}-${safeId(jobId)}`);
    const backupPath = path.join(backupDir, `${change.table}.uasset`);
    const scriptPath = path.join(paths.scriptDir, `balv_tune_${safeId(`${change.table}-${change.column}${change.row ? `-${change.row}` : ''}`)}.py`);

    const script = tune.buildPatchScript({
      table: change.table, dtPath: change.source.assetPath, column: change.column,
      row: change.row, spec: change.spec, uassetPath: change.source.uassetPath, backupPath,
    });
    fsOps.write(scriptPath, script);

    let out;
    try {
      out = await tuneExec({
        command: 'py',
        args: [paths.remoteCmdPath, `exec(open(r'${pyPath(scriptPath)}').read())`],
        cwd: paths.repoRoot,
        timeoutMs: TUNE_TIMEOUT_MS,
      });
    } catch (e) {
      out = `${e.stdout || ''}\n${e.stderr || ''}\n${e.message || ''}`;
    }
    // 원격 py 의 실패는 전부 log_error 로 삼켜져 종료코드에 안 나온다 — 마커가 유일한 증거다.
    if (!/\[BALV-TUNE\] OK\b/.test(String(out))) {
      return record(base, {
        status: 'refused',
        reason: `원격 패치가 성공 마커([BALV-TUNE] OK)를 내지 않았습니다 — 적용되지 않았을 가능성이 큽니다. 백업: ${backupPath}`,
        source: 'uasset', backups: [backupPath], files: [change.source.uassetPath], affectedTables: [change.table],
        output: String(out).slice(-2000),
      });
    }
    return record(base, {
      status: 'applied', source: 'uasset', backups: [backupPath], files: [change.source.uassetPath],
      affectedTables: [change.table], needsRefresh: true, verifiedCells: change.targets,
      output: String(out).slice(-2000), expect: [],
    });
  }

  // ── 본체 ──────────────────────────────────────────────────────────────────
  async function apply(plan, { jobId = 'job', fsOps = defaultFsOps } = {}) {
    const change = plan.change;
    const editorCount = Number(editorCheck()) || 0;
    const base = {
      jobId, planId: plan.id, route: change.route, knob: change.knob,
      table: change.table, column: change.column, row: change.row, spec: change.spec,
      targets: change.targets, fingerprint: plan.fingerprint, editorCount,
      backups: [], files: [], affectedTables: [],
    };
    const refuse = reason => record(base, { status: 'refused', reason });

    if (!change.appliable) return refuse(`${change.route} 라우트는 자동 적용 대상이 아닙니다 — 위치 안내만 제공합니다.`);
    if (editorCount === 0) {
      return refuse('UnrealEditor 가 실행 중이 아닙니다 — 에디터를 켠 뒤 다시 시도하세요. (리임포트 없이 CSV 만 고치면 게임에는 반영되지 않습니다)');
    }
    if (editorCount >= 2) {
      return refuse(`UnrealEditor 가 ${editorCount}개 실행 중이라 여러 인스턴스 중 어디에 붙을지 보장되지 않고 uasset 저장이 실패합니다(Error Code 32). 여분을 종료한 뒤 다시 시도하세요.`);
    }

    // 미리보기 이후 원본이 바뀌었으면 지금 적용해도 미리 본 것과 다른 결과가 된다.
    model.invalidate();
    if (model.fingerprintChange(change) !== plan.fingerprint) {
      return refuse('원본이 미리보기 이후 변경되었습니다 — 미리보기를 다시 실행하세요. (stale plan)');
    }

    if (change.route === 'dt' && change.source.kind === 'uasset') {
      return applyUasset(change, base, { jobId, fsOps });
    }

    let planned;
    try {
      planned = change.route === 'dt-curve' ? planCurve(change, fsOps) : planCsvCell(change, fsOps);
    } catch (e) {
      return refuse(`변경 계획을 만들지 못했습니다 — ${e.message}`);
    }
    if (!planned) return refuse('계산 결과가 현재 값과 같아 변경할 것이 없습니다.');

    const { files, tables, expect, readbackColumns } = planned;
    const filePaths = files.map(f => f.path);
    const withPaths = { ...base, files: filePaths, affectedTables: tables.slice(), expect };

    // 1) 스테이징 — 같은 폴더의 temp 에 쓰고 되읽어 대조한다. 여기서 실패하면 원본은 무사하다.
    const staged = [];
    try {
      for (const f of files) {
        const tmp = `${f.path}.balvtmp`;
        fsOps.write(tmp, f.afterText);
        const back = fsOps.read(tmp);
        if (back !== f.afterText) throw new ApplyError('STAGE_MISMATCH', `임시 파일 내용이 계획과 다릅니다: ${f.path}`);
        if (f.kind === 'csv') {
          const doc = parseCsvDocument(back);
          if (doc.rows.length !== f.rowCount) throw new ApplyError('STAGE_ROWS', `행 수가 어긋납니다: ${f.path} (${f.rowCount} → ${doc.rows.length})`);
          if (f.header && doc.header.join(',') !== f.header.join(',')) throw new ApplyError('STAGE_HEADER', `헤더가 어긋납니다: ${f.path}`);
        } else {
          JSON.parse(back);
        }
        staged.push({ ...f, tmp });
      }
    } catch (e) {
      for (const s of staged) fsOps.remove(s.tmp);
      return record(withPaths, { status: 'refused', reason: `스테이징 검증 실패 — 원본은 그대로입니다. ${e.message}` });
    }

    // 2) 백업 — CSV/params 원본과 대상 표의 uasset 을 job 폴더에 모은다.
    const backupDir = path.join(paths.backupRoot, `${stampOf(now())}-${safeId(jobId)}`);
    const backups = [];
    try {
      fsOps.mkdir(backupDir);
      for (const f of files) {
        const dest = path.join(backupDir, path.basename(f.path));
        fsOps.copy(f.path, dest);
        backups.push(dest);
      }
      const dtPaths = model.dtPaths();
      for (const t of tables) {
        const u = model.uassetPathOf(dtPaths[t]);
        if (u && fsOps.exists(u)) {
          const dest = path.join(backupDir, `${t}.uasset`);
          fsOps.copy(u, dest);
          backups.push(dest);
        }
      }
    } catch (e) {
      for (const s of staged) fsOps.remove(s.tmp);
      return record({ ...withPaths, backups }, { status: 'refused', reason: `백업 실패 — 적용하지 않았습니다. ${e.message}` });
    }

    // 3) 교체 — 실패하면 이미 바꾼 파일을 백업에서 되돌린다.
    const committed = [];
    try {
      for (const s of staged) {
        fsOps.replace(s.tmp, s.path);
        committed.push(s);
      }
    } catch (e) {
      for (const s of staged) if (!committed.includes(s)) fsOps.remove(s.tmp);
      let restored = true;
      try { restoreFromBackup(committed, backupDir, fsOps); } catch { restored = false; }
      return record({ ...withPaths, backups }, restored ? {
        status: 'rolled-back',
        reason: `파일 교체가 중간에 실패해 되돌렸습니다 — 원본 상태입니다. ${e.message}`,
      } : {
        status: 'recovery-required',
        reason: `파일 교체가 실패했고 되돌리기도 실패했습니다. ${e.message}`,
        recovery: manualRecovery(backupDir, filePaths),
        uassetOverwritten: false,
      });
    }

    // 4) 리임포트 — 요청 표 집합 == 처리 표 집합 + 리드백일 때만 성공이다.
    let reimport;
    try {
      reimport = await runReimport({
        csvDir: paths.csvDir, dtPaths: model.dtPaths(), tables, expect, readbackColumns,
        scriptDir: paths.scriptDir, jobId, remoteCmdPath: paths.remoteCmdPath,
        reimportScriptPath: paths.reimportScriptPath, repoRoot: paths.repoRoot, exec,
      });
    } catch (cause) {
      return compensate(cause, { withPaths, files, backupDir, backups, tables, jobId, fsOps });
    }

    return record({ ...withPaths, backups }, {
      status: 'applied',
      source: change.route === 'dt-curve' ? 'curve' : 'csv',
      reimport: { ok: true, tables: reimport.tables, readback: reimport.readback },
      verifiedCells: change.targets,
      curve: planned.curve || null,
      needsRefresh: true,
    });
  }

  // 리임포트 실패 = CSV 는 바뀌었는데 uasset 은 안 바뀐 상태. 그대로 두면 다음 리임포트에 조용히 실린다.
  async function compensate(cause, { withPaths, files, backupDir, backups, tables, jobId, fsOps }) {
    let restored = true;
    try { restoreFromBackup(files, backupDir, fsOps); } catch { restored = false; }
    if (!restored) {
      return record({ ...withPaths, backups }, {
        status: 'recovery-required',
        reason: `리임포트 실패 후 CSV 되돌리기까지 실패했습니다 — ${cause.message}`,
        recovery: manualRecovery(backupDir, withPaths.files),
        uassetOverwritten: false,
        reimport: { ok: false, error: cause.message, code: cause.code },
      });
    }
    try {
      await runReimport({
        csvDir: paths.csvDir, dtPaths: model.dtPaths(), tables, expect: [], readbackColumns: [],
        scriptDir: paths.scriptDir, jobId: `${jobId}-rollback`, remoteCmdPath: paths.remoteCmdPath,
        reimportScriptPath: paths.reimportScriptPath, repoRoot: paths.repoRoot, exec,
      });
      return record({ ...withPaths, backups }, {
        status: 'rolled-back',
        reason: `리임포트가 실패해 CSV 와 에디터를 모두 이전 상태로 되돌렸습니다 — ${cause.message}`,
        reimport: { ok: false, error: cause.message, code: cause.code },
      });
    } catch (second) {
      return record({ ...withPaths, backups }, {
        status: 'recovery-required',
        reason: `리임포트가 실패했고 되돌리기 리임포트도 실패했습니다 — CSV 는 이전 상태로 돌렸지만 에디터 메모리는 확인되지 않았습니다. ${cause.message} / ${second.message}`,
        recovery: manualRecovery(backupDir, withPaths.files),
        // 실행 중인 에디터 아래 uasset 을 OS 복사로 덮으면 에디터가 그걸 다시 덮어써 상태가 더 꼬인다.
        uassetOverwritten: false,
        reimport: { ok: false, error: cause.message, code: cause.code },
      });
    }
  }

  function manualRecovery(backupDir, filePaths) {
    return [
      '1) 에디터에서 이 DataTable 을 저장하지 마세요 (Save All 금지) — 메모리의 상태가 디스크에 굳습니다.',
      '2) 에디터를 저장 없이 종료하세요.',
      `3) 백업 폴더의 파일을 원위치에 덮어쓰세요: ${backupDir}`,
      `   대상: ${filePaths.join(', ')}`,
      '4) 에디터를 다시 켜고 Balance Studio 에서 [데이터 재추출] 을 실행해 현재 값을 확인하세요.',
    ];
  }

  return { apply, restoreFromBackup, manualRecovery };
}

module.exports = { createApplyService, ApplyError, defaultFsOps };
