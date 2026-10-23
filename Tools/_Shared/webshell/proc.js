'use strict';
// 언리얼 에디터 프로세스 감지. (Balance Studio tune.js 에서 추출)
//
// 이건 "에디터가 떠 있나" 만 답한다. PIE 여부도, 원격 파이썬에 닿는지도 알려주지 않는다 —
// 세 가지는 서로 다른 축이라 각각 따로 물어야 한다. 프로세스는 멀쩡한데 원격 노드가 안 잡히는
// 상황(ini 활성 전에 켜진 구 인스턴스)이 실제로 있다.
const { execFileSync } = require('node:child_process');

const EXEC_TIMEOUT_MS = 15_000;

function editorProcessCount() {
  try {
    const out = execFileSync('powershell', [
      '-NoProfile', '-NonInteractive', '-Command',
      '(Get-Process UnrealEditor -ErrorAction SilentlyContinue | Measure-Object).Count',
    ], { encoding: 'utf8', timeout: EXEC_TIMEOUT_MS });
    const n = parseInt(String(out).trim(), 10);
    return Number.isFinite(n) ? n : 0;
  } catch {
    return 0;
  }
}

module.exports = { editorProcessCount, EXEC_TIMEOUT_MS };
