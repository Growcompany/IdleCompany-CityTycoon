'use strict';
// Studio 가 만지는 모든 경로를 한 곳에서 계산한다.
//
// 두 가지 이유로 이 파일이 따로 있다:
//  1) production 경로는 전부 balanceDir/repoRoot 상대다 — 사용자 홈을 코드에 박으면 다른 머신에서 죽는다.
//  2) 테스트는 이 전부를 mkdtemp 아래로 갈아끼워야 한다 — override 지점이 한 군데여야 빠뜨리지 않는다.
const path = require('node:path');

const DEFAULT_BALANCE_DIR = path.resolve(__dirname, '..');

// 경로를 두 부류로 나눈다. 테스트는 DATA 를 전부 temp 아래로 갈아끼워야 하고,
// SOURCE 는 도구 자신의 소스라 공유해도 된다(읽기 전용).
// 새 경로를 추가하면 둘 중 하나에 반드시 넣는다 — studio_model.test.js 가 누락을 잡는다.
const DATA_PATH_KEYS = Object.freeze([
  'valuesPath', 'scanPath', 'calibrationPath', 'measurementsPath',
  'csvDir', 'contentRoot', 'curveParamsPath',
  'scriptDir', 'backupRoot', 'extractOutputPath', 'tmpDir', 'historyPath',
  'manifestSrcPath', 'manifestEditorPath',
]);
const SOURCE_PATH_KEYS = Object.freeze([
  'balanceDir', 'repoRoot', 'coveragePath',
  'reimportScriptPath', 'extractScriptPath', 'remoteCmdPath', 'workerPath', 'publicDir',
]);

function createStudioPaths(balanceDir = DEFAULT_BALANCE_DIR, overrides = {}) {
  const repoRoot = overrides.repoRoot || path.resolve(balanceDir, '..', '..');
  const base = {
    balanceDir,
    repoRoot,

    // 읽기 — 검증 파이프라인 산출물
    valuesPath: path.join(balanceDir, 'out', 'values.json'),
    scanPath: path.join(balanceDir, 'out', 'scan.json'),
    calibrationPath: path.join(balanceDir, 'out', 'calibration.json'),
    measurementsPath: path.join(balanceDir, 'out', 'measurements.json'),
    coveragePath: path.join(balanceDir, 'verify', 'coverage.json'),
    manifestSrcPath: path.join(balanceDir, 'extract', 'manifest_src.json'),
    manifestEditorPath: path.join(balanceDir, 'extract', 'manifest_editor.json'),

    // 쓰기 대상 — 권위 파일
    csvDir: path.join(repoRoot, 'DataImport'),
    contentRoot: path.join(repoRoot, 'Content'),
    curveParamsPath: path.join(balanceDir, 'project_curve_params.json'),

    // 도구 소스
    reimportScriptPath: path.join(balanceDir, 'reimport_project_dt.py'),
    extractScriptPath: path.join(balanceDir, 'extract', 'extract_editor.py'),
    remoteCmdPath: path.join(repoRoot, 'Tools', 'RemoteExec', 'remote_cmd.py'),
    workerPath: path.join(balanceDir, 'studio', 'worker.js'),
    publicDir: path.join(balanceDir, 'studio', 'public'),

    // 작업 공간 — 기존 tune.js 정책(C:/tmp)을 그대로 잇는다.
    scriptDir: 'C:/tmp',
    backupRoot: 'C:/tmp/balv_tune_backup',
    extractOutputPath: 'C:/tmp/balv_editor.json',
    tmpDir: path.join(balanceDir, 'out', 'tmp'),
    historyPath: path.join(balanceDir, 'out', 'studio', 'history.jsonl'),
  };
  return Object.freeze({ ...base, ...overrides });
}

module.exports = { createStudioPaths, DEFAULT_BALANCE_DIR, DATA_PATH_KEYS, SOURCE_PATH_KEYS };
