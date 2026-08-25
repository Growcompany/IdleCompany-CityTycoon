#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
치트 명령어 문서 드리프트 검증기.

단일 진실 소스(SOT) = Source/.../Public/Player/CGCheatManager.h 의 UFUNCTION(Exec) 목록.
이 스크립트는 헤더의 모든 Exec 명령이 다음 두 문서에 등록돼 있는지 확인한다:
  - docs/07_Reference/CHEAT_COMMANDS.md     (` `Name` ` 형식의 표 항목)
  - docs/07_Reference/CHEAT_COMMANDS.html   (cmd:"Name ..." 형식의 COMMANDS 항목)

하나라도 빠지면 누락 목록을 stderr에 출력하고 exit code 2(= Claude Code PostToolUse 훅이
피드백으로 surface)로 종료한다. 모두 동기화돼 있으면 한 줄 요약 후 exit 0.

수동 실행:  py -3 Tools/CheatDoc/verify_cheat_docs.py
"""

import re
import sys
from pathlib import Path

# Windows 콘솔(cp949)에서도 한글/특수문자 출력이 깨지지 않도록 UTF-8 강제.
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8")
    except (AttributeError, ValueError):
        pass

# 이 파일: <repo>/Tools/CheatDoc/verify_cheat_docs.py  ->  repo = parents[2]
REPO = Path(__file__).resolve().parents[2]

HEADER = REPO / "Source" / "CompanyGrowthRenewal" / "Public" / "Player" / "CGCheatManager.h"
DOC_MD = REPO / "docs" / "07_Reference" / "CHEAT_COMMANDS.md"
DOC_HTML = REPO / "docs" / "07_Reference" / "CHEAT_COMMANDS.html"

# 문서화 면제 명령 (의도적으로 문서에 안 싣는 내부/디버그 전용). 비워두는 게 기본.
EXEMPT = set()  # 예: {"CheatPing"} 처럼 추가 가능


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def parse_exec_commands(header_text: str):
    """UFUNCTION(Exec) 바로 뒤(주석 허용)에 오는 `void FuncName(` 의 FuncName 들을 순서대로 반환."""
    commands = []
    # UFUNCTION( ... Exec ... ) 다음에 나오는 첫 'void Name(' 캡처.
    # 사이에 주석/공백/줄바꿈 허용. Exec 지정자는 다른 지정자와 함께 올 수 있음.
    pattern = re.compile(
        # UFUNCTION( ... Exec ... ) — Meta=(...) 등 한 단계 중첩 괄호 지정자 허용
        r"UFUNCTION\s*\((?:[^()]|\([^()]*\))*\bExec\b(?:[^()]|\([^()]*\))*\)"
        r"(?:\s|//[^\n]*\n|/\*.*?\*/)*"             # 사이 공백/한 줄·블록 주석
        r"(?:virtual\s+)?void\s+(\w+)\s*\(",        # void Name(
        re.DOTALL,
    )
    for m in pattern.finditer(header_text):
        name = m.group(1)
        if name not in commands:
            commands.append(name)
    return commands


def present_in_md(name: str, md: str) -> bool:
    # 표 항목: `Name` 또는 `Name [Args]`. 백틱 + 이름 + (이름-문자 아닌 경계).
    # 부정 전방탐색이라 인자 구분자가 공백/대괄호/'(' 무엇이든 허용하면서도
    # 접두어 오탐은 차단 (Day != DayX, SetHQ != SetHQLv).
    return re.search(r"`" + re.escape(name) + r"(?![A-Za-z0-9_])", md) is not None


def present_in_html(name: str, html: str) -> bool:
    # COMMANDS 항목: cmd:"Name" 또는 cmd:"Name [Args]" (동일한 경계 규칙)
    return re.search(r'cmd:"' + re.escape(name) + r'(?![A-Za-z0-9_])', html) is not None


def main() -> int:
    missing_files = [p for p in (HEADER, DOC_MD, DOC_HTML) if not p.exists()]
    if missing_files:
        # 파일 자체가 없으면 검증 불가 — 조용히 통과(다른 작업 방해 금지).
        sys.stderr.write(
            "[verify_cheat_docs] 경고: 파일 없음 -> "
            + ", ".join(str(p) for p in missing_files)
            + "\n"
        )
        return 0

    header_text = read_text(HEADER)
    md = read_text(DOC_MD)
    html = read_text(DOC_HTML)

    commands = parse_exec_commands(header_text)
    if not commands:
        sys.stderr.write("[verify_cheat_docs] 경고: 헤더에서 Exec 명령을 하나도 못 찾음 (파서 점검 필요)\n")
        return 0

    missing = []  # (name, [빠진 문서들])
    for name in commands:
        if name in EXEMPT:
            continue
        gaps = []
        if not present_in_md(name, md):
            gaps.append("MD")
        if not present_in_html(name, html):
            gaps.append("HTML")
        if gaps:
            missing.append((name, gaps))

    total = len([c for c in commands if c not in EXEMPT])
    if not missing:
        print(f"[verify_cheat_docs] OK - Exec 명령 {total}개 전부 CHEAT_COMMANDS.md/.html 에 동기화됨")
        return 0

    sys.stderr.write(
        "\n[verify_cheat_docs] 치트 문서 동기화 누락 감지 "
        f"({len(missing)}/{total}개 명령)\n"
        "  단일 진실 소스: Public/Player/CGCheatManager.h\n"
        "  갱신 대상: docs/07_Reference/CHEAT_COMMANDS.md + CHEAT_COMMANDS.html\n\n"
    )
    for name, gaps in missing:
        sys.stderr.write(f"  - {name:<22} 누락: {', '.join(gaps)}\n")
    sys.stderr.write(
        "\n  -> 위 명령들을 두 문서에 추가해 동기화할 것 (HTML 은 COMMANDS 배열, "
        "MD 는 요약 표 + 상세 섹션).\n"
    )
    return 2


if __name__ == "__main__":
    sys.exit(main())
