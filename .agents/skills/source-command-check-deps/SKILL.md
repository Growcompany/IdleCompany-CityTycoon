---
name: "source-command-check-deps"
description: "헤더/클래스 의존성 분석"
---

# source-command-check-deps

Use this skill when the user asks to run the migrated source command `check-deps`.

## Command Template

지정한 헤더 파일 또는 클래스의 의존성을 분석해줘:

1. **이 파일을 include하는 파일들** (역방향 의존성)
2. **이 파일이 include하는 파일들** (순방향 의존성)
3. **이 클래스를 사용하는 곳** (UPROPERTY, 함수 호출, Cast 등)
4. **이 클래스가 의존하는 매니저/서브시스템**
5. **삭제/리네임 시 영향받는 파일 목록**

결과를 의존성 깊이별로 정리해줘.
