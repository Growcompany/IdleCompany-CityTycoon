#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PostToolUse 훅 진입점.

Claude Code 가 Edit/Write 류 도구를 실행한 뒤 이 스크립트를 호출한다(stdin 으로 훅 JSON 전달).
편집된 파일이 CGCheatManager.h / .cpp 일 때만 verify_cheat_docs.py 를 실행해
치트 문서(CHEAT_COMMANDS.md / .html) 동기화 누락을 검사한다.

- 동기화 OK  -> exit 0 (조용)
- 드리프트   -> 누락 목록을 stderr 로 출력하고 exit 2 (PostToolUse 가 Claude 에게 피드백)
- 무관 파일  -> exit 0 (아무것도 안 함)

settings.json 의 PostToolUse(matcher: Edit|Write|MultiEdit) 훅에서 호출하도록 배선한다.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
VERIFIER = HERE / "verify_cheat_docs.py"

# 이 파일들이 편집됐을 때만 검사. (단일 진실 소스 = 치트 매니저)
TRIGGER_RE = re.compile(r"CGCheatManager\.(h|cpp)$", re.IGNORECASE)


def extract_file_paths(data: dict):
    """훅 JSON 에서 편집 대상 파일 경로를 가능한 키들로부터 수집."""
    paths = []
    ti = data.get("tool_input") or data.get("toolInput") or {}
    if isinstance(ti, dict):
        for key in ("file_path", "filePath", "path", "notebook_path"):
            v = ti.get(key)
            if isinstance(v, str) and v:
                paths.append(v)
        # MultiEdit 등 edits 배열 형태 대비
        edits = ti.get("edits")
        if isinstance(edits, list):
            for e in edits:
                if isinstance(e, dict):
                    v = e.get("file_path") or e.get("filePath")
                    if isinstance(v, str) and v:
                        paths.append(v)
    return paths


def main() -> int:
    # stdin 을 raw 바이트로 읽어 utf-8-sig 로 디코딩한다.
    # (Windows 콘솔 코드페이지가 cp949 면 sys.stdin.read() 가 UTF-8 BOM/한글을 깨뜨림.
    #  utf-8-sig 는 선행 BOM 도 자동 제거.)
    raw = ""
    try:
        if not sys.stdin.isatty():
            raw = sys.stdin.buffer.read().decode("utf-8-sig", errors="replace")
    except Exception:
        raw = ""

    raw = raw.strip()

    data = {}
    if raw:
        try:
            data = json.loads(raw)
        except Exception:
            data = {}

    paths = extract_file_paths(data)

    # 경로를 못 알아냈으면(포맷 변경 등) 조용히 통과 — 무관 편집에서 떠들지 않도록.
    if not paths:
        return 0

    if not any(TRIGGER_RE.search(p.replace("\\", "/")) for p in paths):
        return 0

    # 치트 매니저 편집 확정 -> 동기화 검사
    if not VERIFIER.exists():
        return 0

    result = subprocess.run(
        [sys.executable, str(VERIFIER)],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
