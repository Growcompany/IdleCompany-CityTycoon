# AGENTS.md — Public Portfolio Edition

이 문서는 CompanyGrowthRenewal의 실제 AI 협업 규칙을 공개용으로 정제한 버전입니다. 개인 경로, 배포 설정, 원격 저장소 정보와 비공개 기획 문서는 제거했습니다.

## 프로젝트 경계

- Unreal Engine 5.4 C++ 개인 클라이언트 프로젝트
- Azure DevOps 비공개 저장소가 전체 프로젝트의 단일 진실 원천
- 이 GitHub 저장소는 에셋과 실제 설정을 제외한 단방향 공개 스냅샷
- 기본 아키텍처, 제약, 완료 조건과 최종 채택·기각은 개발자가 결정
- AI 출력은 코드·빌드·테스트·PIE·실기기 증거를 통과해야 반영

## 역할 라우팅

| 작업 | 역할 | 책임 |
|---|---|---|
| 시스템·클래스 구조 | Architect | 책임 경계, 수명주기, 데이터 흐름 제안 |
| UE5 C++ 구현 | Implementer | 기존 패턴 탐색, 최소 변경, 빌드 가능 코드 |
| 코드 리뷰 | Reviewer | 버그, 회귀, 보안, 수명주기 검토 |
| 모바일 성능 | Optimizer | CPU·GPU·RHI 병목 측정과 A/B 제안 |
| UI/CommonUI | UI specialist | Widget Stack, 입력 모드, 좌표, 델리게이트 검증 |
| PlayFab·Firebase | Backend specialist | 계약, 인증, 서버 권위, HTTP/JSON 검증 |

역할별 예시는 `.claude/agents/`와 `.codex/agents/`에 있다. 구현과 리뷰는 가능한 한 분리한다.

## 표준 작업 순서

1. 관련 헤더와 공개 API를 먼저 읽는다.
2. 같은 일을 하는 기존 함수와 프로젝트 패턴을 검색한다.
3. 책임·수명주기·데이터 소유자를 명시한다.
4. 완료 조건과 검증 방법을 먼저 고정한다.
5. 필요한 최소 범위만 구현한다.
6. 독립 리뷰에서 영향 범위와 실패 경로를 확인한다.
7. UBT 빌드, 관련 Automation Test, PIE, 플랫폼 실측 중 작업에 필요한 증거를 수집한다.
8. 근본 원인을 Skill, Hook, 테스트 또는 문서 규칙으로 남긴다.

## 기존 기능 탐색

- 주요 클래스의 헤더 전체를 읽어 public 메서드를 먼저 확인한다.
- `Focus|Zoom|Move|Track|Spawn|Open|Push|Show|Apply|Begin|End|Get|Set`과 대상 명사를 함께 검색한다.
- `docs/07_Reference/CAPABILITIES_MAP.md`에서 재사용 API를 확인한다.
- 기존 함수가 있으면 새로 만들지 않고 호출하거나 확장한다.

## UE5.4 C++ 규칙

- Unreal 리플렉션과 객체 수명주기를 우선한다.
- `UPROPERTY`, `UFUNCTION`, 델리게이트 바인딩과 해제를 대칭으로 관리한다.
- `NativeConstruct`의 바인딩은 `NativeDestruct`에서 제거한다.
- `BeginPlay`와 `EndPlay`, `NativeOnActivated`와 `NativeOnDeactivated`도 쌍으로 본다.
- 부모 멤버를 가리는 `Slot`, `Owner`, `Outer`, `World` 같은 지역변수를 피한다.
- 변경 이력을 코드 주석으로 남기지 않고, 주석은 로직의 이유를 설명한다.

## 데이터 주도 설계

- 표시 이름, 밸런스, 위젯 클래스, 타입별 값은 DataTable을 단일 진실 원천으로 사용한다.
- 산업·타입별 문자열을 코드 `switch`로 중복 하드코딩하지 않는다.
- Row가 없으면 빈 값과 경고로 드러내고 코드 폴백으로 감추지 않는다.
- 빈 DataTable 셀은 의도적인 없음으로 존중한다.
- 새 DataTable은 `FTableRowBase` 구조체 → 생성자 로드 → Initialize 캐시 → Getter 순서를 따른다.

## CommonUI 규칙

- 새 화면은 `EWidgetType → DT_WidgetClass → TableManager → CommonUI Stack` 경로로 생성한다.
- 주요 화면, 모달, 하단 시트는 Main·Prompt·Bottom Stack으로 분리한다.
- 탭의 배타적 선택은 `UCommonButtonGroupBase`를 사용한다.
- 패널을 열고 닫을 때 UI/Normal 입력 모드 복원을 쌍으로 관리한다.
- 3D·Viewport·중첩 위젯 좌표는 `LocalToAbsolute → AbsoluteToLocal`로 통일한다.

## 저장 규칙

- 저장과 로드 경로를 같은 데이터 구조 기준으로 대칭 검토한다.
- 최초 로드 완료 전 저장을 차단한다.
- 고빈도 변경은 즉시 디스크 쓰기 대신 지연 저장한다.
- 현재 프로토타입 단계에서는 구 세이브 마이그레이션보다 깨끗한 구조를 우선한다.

## 모바일·쿠킹 규칙

- 쿠커가 참조를 볼 수 있는지가 하드·소프트 참조 여부보다 중요하다.
- 직렬화된 에셋 참조는 의존성 그래프를 따르지만, C++ 런타임 문자열 경로는 별도 쿠킹 진입점이 필요하다.
- 에디터 성공만으로 모바일 성공을 주장하지 않는다.
- Android는 Development/Test 실기기 로그, Vulkan/GLES A/B, 프레임 측정으로 검증한다.

## 백엔드 규칙

- 클라이언트가 직접 쓸 수 있는 데이터와 서버 권위 데이터의 경계를 분리한다.
- 인증 토큰, 서비스 계정, 비밀번호, 실제 배포 URL을 저장소에 넣지 않는다.
- 비동기 콜백은 객체 수명과 실패 경로를 함께 검토한다.
- 폴링은 중복 타이머, 화면 비활성화, 비용과 rate limit을 고려한다.

## 자동 검증

- `.agents/skills/`: 분석, 의존성, 기존 패턴, 리뷰, 빌드 확인 워크플로
- `.claude/hooks/pre-csv-guard.js`: CSV 빈 필드 경고
- `.claude/hooks/post-edit.js`: 헤더 영향 범위·UI 수명주기·SaveLoad 대칭성 리마인더
- `Tools/CheatDoc/verify_cheat_docs.py`: Exec 치트 선언과 문서 동기화
- `Source/CompanyGrowthRenewal/Private/Tests/`: UE Automation Test 선언

## 공개 저장소 안전 규칙

- `Content`, `Plugins`, 실제 `Config`, `Saved`, `node_modules`, 인증정보를 추가하지 않는다.
- `${PROJECT_ROOT}`와 `${UE_ROOT}`를 사용하고 개인 절대 경로를 기록하지 않는다.
- 원격 Push는 현재 브랜치와 변경 범위를 확인하고 명시적 승인 뒤 수행한다.
- 외부 에셋이나 엔진 코드를 복제하지 않는다.
- 공개본은 전체 게임 실행 빌드가 아니라 기술 검토용 소스 스냅샷임을 유지한다.

## 환경 변수 예시

```text
PROJECT_ROOT=<public repository root>
UE_ROOT=<Unreal Engine 5.4 installation root>
```

실제 인증·배포 값은 환경 변수 또는 비공개 설정으로만 주입한다.
