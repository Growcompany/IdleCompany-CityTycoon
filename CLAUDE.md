# CLAUDE.md — Public Portfolio Edition

이 파일은 CompanyGrowthRenewal에서 Claude Code를 역할별 생산성 도구로 운영한 규칙의 공개용 버전입니다. 공통 엔지니어링 기준은 [AGENTS.md](AGENTS.md)를 우선합니다.

## 책임 경계

- 개발자: 제품 방향, 아키텍처 제약, 완료 조건, 최종 채택·기각
- Claude: 기존 코드 탐색, 설계 대안, 구현 초안, 리뷰 후보, 테스트·문서 자동화
- 검증: 실제 코드, UBT, Automation Test, PIE, Android 실기기 증거

Claude의 높은 확신은 증거를 대체하지 않습니다. 로그나 측정값과 충돌하는 가설은 기각합니다.

## 역할별 Agent

- `.claude/agents/architect.md`: 코드를 직접 작성하기 전에 책임과 수명주기를 설계
- `.claude/agents/implementer.md`: 기존 패턴을 탐색하고 최소 범위로 구현
- `.claude/agents/reviewer.md`: 구현과 독립적으로 회귀·수명주기·보안 검토
- `.claude/agents/optimizer.md`: 모바일 CPU·GPU·RHI 병목과 A/B 측정

## 재사용 Command와 Skill

- `.claude/commands/`: 분석, 빌드 사전 점검, 의존성, 기존 패턴, 전체 리뷰, 세션 복원
- `.claude/skills/`: UE5 클래스, 컴포넌트, DataTable, Subsystem, Widget 제작 체크리스트
- `.agents/skills/`: Codex와 공유하는 분석·리뷰 워크플로

자동 Push 명령과 개인 세션 상태는 공개본에서 제외했습니다.

## Hook 기반 가드

- `pre-csv-guard.js`: CSV 편집 시 비어 있는 필드가 의도적인지 경고
- `prompt-hint.js`: 작업 문맥에 맞는 프로젝트 규칙과 Skill을 안내
- `post-edit.js`: 헤더 의존성, 위젯 델리게이트, 저장 대칭성, DataTable Reimport를 점검

설치 예시는 `.claude/settings.example.json`에 있으며 상대 경로만 사용합니다.

## 작업 원칙

1. 관련 헤더와 기존 구현을 먼저 읽는다.
2. 문제를 코드·엔진·데이터·플랫폼 차이로 분리한다.
3. 복수 가설을 만든 뒤 최소 재현과 측정으로 기각한다.
4. 구현 담당과 리뷰 담당을 분리한다.
5. 빌드·테스트·PIE·실기기 중 필요한 증거가 없으면 완료로 보고하지 않는다.
6. 반복 실패는 테스트, Skill, Hook, 플레이북으로 전환한다.

## 공개 안전

- `${PROJECT_ROOT}`와 `${UE_ROOT}` 외의 개인 절대 경로를 사용하지 않는다.
- 실제 Firebase URL, PlayFab 설정, 서비스 계정과 인증정보를 출력하지 않는다.
- 외부 에셋과 비공개 기획 문서를 공개 저장소로 복사하지 않는다.
- 현재 브랜치와 diff를 확인하지 않은 자동 Push를 금지한다.
