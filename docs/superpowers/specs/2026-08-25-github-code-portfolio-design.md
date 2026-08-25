# CompanyGrowthRenewal GitHub 코드 포트폴리오 설계

## 목표

Azure DevOps 비공개 저장소를 전체 프로젝트의 단일 진실 원천으로 유지하면서, 채용 검토자가 직접 작성한 C++ 구조와 검증 체계를 살펴볼 수 있는 공개 GitHub 저장소 `Growcompany/CompanyGrowthRenewal-Portfolio`를 만든다.

## 공개 기준선

- 원본 저장소: `CompanyGrowthRenewal`
- 기준 커밋: `7df820f2469822c17f2ce1e5c60f4c46093a27dc`
- 기준 날짜: `2026-08-25`
- 원본 작업트리의 미커밋 변경은 공개 스냅샷에서 제외한다.
- 공개 저장소는 기존 Azure Git 히스토리를 복제하지 않고 깨끗한 새 이력으로 시작한다.

## 저장소 역할 분리

- Azure DevOps: 전체 `Content`, 외부 에셋, 실제 설정, 전체 개발 히스토리를 보관하는 비공개 원본
- GitHub: 직접 작성한 코드, 테스트, 제작 도구, 데이터 주도 예시, AI 협업 규칙을 보여주는 공개 기술 포트폴리오

## 공개 범위

### 그대로 공개

- `Source/CompanyGrowthRenewal/` 전체 C++ 및 모듈 빌드 파일
- `Source/CompanyGrowthRenewal/Private/Tests/` 전체 자동화 테스트 소스
- `CompanyGrowthRenewal.uproject` 플러그인·플랫폼 의존성 목록
- 선별된 제작·검증 도구
- 선별된 DataTable import CSV 예시
- 아키텍처·입력·UI·백엔드·모바일 최적화에 관한 선별된 기술 문서

### 정제 후 공개

- `AGENTS.md`, `CLAUDE.md`
- `.agents/skills/`
- `.claude/agents/`, `.claude/commands/`, `.claude/skills/`, 안전한 `.claude/hooks/`
- `.codex/agents/`와 안전한 `.codex/hooks/`
- Firebase 공개 엔드포인트가 포함된 C++ 클라이언트 기본값
- 개인 절대 경로를 포함한 제작 스크립트

정제 규칙은 개인 사용자명·절대 경로·Azure 주소를 일반 변수로 바꾸고, 자동 Push 명령과 세션 상태·로컬 권한 설정은 제외하는 것이다.

### 제외

- `Content/`, `Plugins/`, `.uasset`, `.umap` 및 모든 외부/Fab/Marketplace 에셋
- 실제 `Config/`와 인증·배포 설정
- `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`
- `node_modules/`, 컴파일된 Firebase `lib/`와 source map
- `firebase-chat-server/` 전체 구현은 인증·rate limit 강화 전까지 제외
- `.claude/settings.json`, `.claude/STATE.md`, 백업 파일
- `.codex/config.toml`, `.codex/hooks.json`
- Azure 자동 Push 명령·Hook
- 원본 설계 전체와 미공개 게임 기획 문서

## 공개 저장소 구조

```text
CompanyGrowthRenewal-Portfolio/
├─ README.md
├─ NOTICE.md
├─ .gitignore
├─ CompanyGrowthRenewal.uproject
├─ Source/CompanyGrowthRenewal/
├─ DataImport/Samples/
├─ Tools/
├─ AGENTS.md
├─ CLAUDE.md
├─ .agents/skills/
├─ .claude/{agents,commands,skills,hooks}/
├─ .codex/{agents,hooks}/
└─ docs/
   ├─ AI_WORKFLOW.md
   ├─ ARCHITECTURE.md
   ├─ TROUBLESHOOTING.md
   └─ PUBLICATION_NOTES.md
```

## README 탐색 구조

README는 다음 여덟 영역에서 실제 상대 경로로 바로 이동하게 한다.

1. Subsystem 중심 아키텍처
2. DataTable 콘텐츠 파이프라인
3. CommonUI Stack 라우팅
4. 저장·오프라인 정산
5. PC·모바일 입력과 카메라
6. PlayFab·Firebase 연동 구조
7. Automation Test와 제작 도구
8. 대표 트러블슈팅

각 영역은 문제·설계 판단·대표 파일·검증 방법을 짧게 설명한다. 저장소가 에셋을 제외한 코드 포트폴리오이며 전체 실행 빌드를 배포하지 않는다는 점을 README 상단에 명시한다.

## AI 활용 공개 원칙

- 기본 아키텍처, 제약, 완료 조건, 최종 채택·기각은 개발자가 소유한다.
- AI는 탐색, 구현 초안, 독립 리뷰, 반복 검증과 문서 동기화를 가속한다.
- 역할별 에이전트, Skill, Hook의 실제 파일을 공개한다.
- 모델 대화 기록, 개인 상태, 토큰, 계정 설정은 공개하지 않는다.
- 높은 확신보다 UBT, Automation Test, PIE, Android 실기기 증거를 우선한다.

## 라이선스와 배포

- 공개 열람을 위한 포트폴리오 저장소이며 오픈소스 권한을 자동 부여하지 않는다.
- 외부 에셋과 엔진 코드는 포함하지 않는다.
- `NOTICE.md`에 저작권과 제외된 의존성을 명시한다.
- Cropout은 학습 기준선으로만 설명하며 Epic Games와의 제휴·공식 파생 프로젝트로 오해될 표현을 사용하지 않는다.

## 검증 조건

- 공개 트리에 `.uasset`, `.umap`, 실행 바이너리, `node_modules`가 0개여야 한다.
- 고신뢰 비밀 패턴 검색 결과가 0개여야 한다.
- 개인 경로, Azure URL, 실제 Firebase 런타임 URL이 0개여야 한다.
- README의 모든 상대 링크가 실제 파일을 가리켜야 한다.
- `Source/CompanyGrowthRenewal`은 기준 커밋의 추적 파일과 개수·해시가 일치해야 한다. 공개본에서 의도적으로 정제하는 Firebase 기본 URL 한 곳만 예외로 기록한다.
- 공개한 Node·Python 제작 도구의 독립 테스트가 통과해야 한다.
- GitHub 공개 상태와 기본 브랜치 `main`을 확인한 뒤 Notion 프로젝트 URL에 연결한다.
