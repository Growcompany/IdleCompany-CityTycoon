# 백엔드 작업 플레이북 (BACKEND_PLAYBOOK) — SOT

- **작성일**: 2026-06-21
- **상태**: 초판
- **목적**: 백엔드 작업 전 [BACKEND_ARCHITECTURE.md](./BACKEND_ARCHITECTURE.md) 와 함께 필독. "어떻게" 의 정본.

---

## 1. 새 백엔드 기능 추가 표준 절차

### 1.1 백엔드 경계 판단

신규 기능이 어느 백엔드에 속하는지 먼저 결정한다.

- 채팅(메시지 전송·수신·방) → Firebase Cloud Functions (HTTP 폴링 경로)
- 그 외 서버 측 데이터(인증·플레이어 정보·통계·랭킹·서버시간·UserData) → PlayFab
- 동일 데이터를 두 백엔드에 분산 금지 — 정합 붕괴 원인

불명확하면 ARCHITECTURE §0 책임 분담 원칙을 기준으로 판단. 여전히 모호하면 작업 착수 전 사용자에게 확인.

### 1.2 라이브·재사용 확인

작업할 기능을 이미 구현한 매니저 API가 있는지 먼저 확인한다.

1. ARCHITECTURE §3 매니저 인벤토리 스캔
2. `docs/07_Reference/CAPABILITIES_MAP.md` 확인
3. 주요 헤더를 통째로 Read 해 public 메서드 목록 확인
4. 동사+명사 Grep (`Send|Fetch|Upload|Load|Save|Login|Get|Update|Start|Stop`)

같은 일을 하는 기존 함수가 있으면 새로 짜지 말고 호출·확장. 새 재사용 API를 추가했으면 CAPABILITIES_MAP.md 에도 한 줄 추가.

### 1.3 계약 정의 (코드 작성 전)

신규 백엔드 계약은 코드를 짜기 전에 ARCHITECTURE §4 계약 레지스트리에 먼저 등록한다.

- Firebase 신규 엔드포인트: §4.1 표에 URL suffix / 메서드 / 요청·응답 JSON 필드 추가
- PlayFab 신규 통계명: §4.2 표에 C++ 상수명 + 리터럴 + 모드 추가
- PlayFab 신규 UserData 키: §4.3 표에 키 문자열 + 데이터 타입 + 쓰기·읽기 주체 추가

계약 문서가 정본 — 코드의 리터럴은 이 문서와 반드시 일치해야 한다.

### 1.4 클라 구현 (매니저 레이어)

**매니저 신설 판단 기준**: 기존 4개 백엔드 매니저(`PlayFabManagerSubsystem` / `ChatManagerSubsystem` / `RankingManagerSubsystem` / `SaveLoadManager`)로 책임이 맞지 않을 때만 신설. 그 외는 기존 매니저 확장.

구현 체크리스트:
- 요청 함수와 성공/실패 델리게이트 **쌍** 정의 (`OnXxxComplete` / `OnXxxError` 또는 `OnError`)
- 실패 시 `UE_LOG(LogTemp, Error, ...)` — 빈 콜백 금지 (loud failure 원칙)
- HTTP 콜백 내 `this` 수명 가드 (`TWeakObjectPtr`/`IsValid()` 패턴)
- 타임아웃·재시도 정책 명시 (기본: HTTP 30초, 실패 시 1회 재시도 후 에러 델리게이트)
- UI 가 PlayFab/Firebase 직접 호출 금지 — 반드시 매니저 경유
- 로그인 상태(`EPlayFabLoginState`) 가드 — 미로그인 상태 API 호출 방어

### 1.5 세이브·동기화 설계

로컬 세이브(`USaveGame_GameData`)와 PlayFab 클라우드 동기화의 관계를 먼저 결정한다.

| 케이스 | 설계 |
|---|---|
| 로컬 권위 + 비민감(미션 진행 등) | 로컬 세이브만, PlayFab 동기화 없음 |
| 로컬 캐시 + 랭킹 표시(시총/스냅샷 등) | 로컬에서 빌드 → PlayFab에 비동기 업로드, 쓰로틀 적용 |
| 민감 데이터(재화/인벤토리/피티) | 현재 로컬 권위(프로토타입), 신규 치트 표면 추가 금지 (목표: 서버 전용 쓰기 — §2.7 참조) |
| 서버 권위 데이터(인증/LastSync) | PlayFab이 정본, 로컬은 표시용 읽기 캐시만 |

`SaveLoadManager` 세이브·로드 트리거 규칙 준수:
- 저장: `TransitionToLevel` 호출 시점
- 로드: `LoadAfterStart()` 내부 — 여기서 `SaveGameData()`를 끼우면 빈 메모리 덮어쓰기 (§3 함정 참조)
- 랭킹 업로드: 60초 쓰로틀 (`UploadThrottleSeconds`) 패턴 준수

### 1.6 쿠킹·설정 등록

- **소프트 참조 신규 추가 시**: 에셋 경로 폴더를 Project Settings `Additional Asset Directories to Cook` 에 등록 + `CLAUDE.md` "현재 등록 필요 폴더" 목록 갱신
- **런타임 에셋 로드**: `TSoftObjectPtr<T>` + `LoadSynchronous()` 사용 — `LoadObject<T>(TEXT(...))` 문자열 직접 로드 금지
- **API 키·엔드포인트·Title ID**: `Config/*.ini` 에 관리, 코드 하드코딩 커밋 주의 (현재 Firebase URL 중복 상태 → §3 함정 참조)

### 1.7 빌드

- 빌드 도구: `UnrealBuildTool.exe CompanyGrowthRenewalEditor Win64 Development` 직접 실행 (CLAUDE.md 빌드 명령 참조)
- 에디터가 켜져 있으면 Live Coding 잠금(exit 6) → 에디터 종료 후 빌드
- 새 `UCLASS` 추가 또는 멤버 레이아웃 변경 시 풀빌드 필수
- 멤버 셰도잉 지역변수(`Slot`/`Owner`/`World`/`Children` 등) 금지 (`-WarningsAsErrors`)
- 빌드 후 `stale UnrealBuildTool` 프로세스가 lock 잡는 경우 kill 후 재시도

### 1.8 검증

- PIE에서 PlayFab 로그인 → 기능 흐름 → Output Log 서버 응답 확인
- Firebase 채팅: 폴링 시작·중지, 전송, 수신 각각 확인
- Android/iOS 실기기: 소프트 참조 에셋 누락 없는지 확인
- 에러 경로: 네트워크 끊기·오프라인 상태에서 크래시 없는지 확인
- 완료 후 변경 파일 코드 Read 로 사용자에게 제시

---

## 2. 비협상 체크리스트

다음 12개 항목은 모든 백엔드 작업에서 반드시 충족해야 한다. 항목을 건너뛰는 것은 허용되지 않는다.

### 2.1 라이브 확인

손댈 매니저/경로가 실제 호출되는지 Grep 확인. 죽은·중복 경로 금지. 같은 일을 하는 기존 매니저 우선 재사용·확장 (CLAUDE.md "구현 전 기존 기능 탐색" 규칙). 실제 호출되지 않는 코드를 수정하거나, 이미 있는 기능을 재구현하지 않는다.

### 2.2 백엔드 경계 (불변 가드레일)

채팅 외 데이터를 Firebase 에 두지 말 것. 채팅을 PlayFab 에 두지 말 것. 플레이어 정보/데이터/경제는 PlayFab(또는 로컬 세이브) 경로로만. 신규 기능이 어느 백엔드인지 모호하면 ARCHITECTURE §0 기준으로 판단 후 사용자 확인.

### 2.3 PlayFab 비동기 규약

요청마다 성공/실패 델리게이트 **쌍** (`OnComplete`/`OnError`) 정의. 타임아웃·재시도 정책 명시. 실패 시 `UE_LOG` loud 로그 (silent 실패 금지). 로그인 상태(`EPlayFabLoginState`) 가드 — 미로그인 상태에서 API 호출하지 않도록 방어. UI 가 PlayFab 직접 호출 금지 — 반드시 매니저 경유.

### 2.4 HTTP·폴링 비용

폴링 간격·백오프·앱 백그라운드 시 정지·중복 요청 억제. 응답 페이로드 최소화. `FHttpModule` 콜백 내 `this` 수명 가드 (`TWeakObjectPtr<ThisClass> WeakThis = this; if (!WeakThis.IsValid()) return;` 패턴). 폴링 타이머는 반드시 `StopPolling()`으로 명시 해제.

### 2.5 JSON 직렬화

라운드트립(serialize → deserialize) 대칭 검증. 누락 필드·타입 불일치 방어적 파싱(서버 스키마 변화 내성, `TryGetField`/`HasField` 사용). 키 문자열은 ARCHITECTURE §4 계약 레지스트리와 일치. 알 수 없는 필드는 무시·스킵 (하드 실패 금지).

### 2.6 세이브 동기화 타이밍

클라우드 업로드(랭킹/스냅샷)와 로컬 세이브의 순서·쓰로틀 준수 — `RankingManagerSubsystem` 60초 쓰로틀 패턴 참조. `SaveLoadManager` 트리거(저장=`TransitionToLevel` / 로드=`LoadAfterStart` / 액션 시점) 규칙 준수. **로드 직후 `SaveGameData()` 호출 금지** — 빈 메모리로 세이브를 덮어쓰는 치명 함정.

### 2.7 서버 검증 권위 / 안티치트 (ARCHITECTURE §0.2)

민감 데이터(재화/인벤토리/구매한도/가챠 피티)에 **클라 → PlayFab 직접 쓰기 추가 금지** (서버 전용 쓰기로 잠글 수 있게 경로 격리). 신규 경제 변경(구매/가챠/지급)은 "의도 전송 → 서버 검증" 형태로 무재작성 전환 가능하게 설계. **로컬 캐시 자체는 금기가 아님** — 금기는 "클라가 계산한 민감값을 서버에 그대로 쓰는 것". 새로운 치트 표면을 늘리지 말 것. 전면 마이그레이션은 별도 스펙 + 사용자 승인 후.

### 2.8 모바일 쿠킹

신규 소프트 참조(`TSoftObjectPtr`/`FSoftObjectPath`/문자열 경로) 추가 시 CLAUDE.md "현재 등록 필요 폴더" 에 해당 폴더 추가 + Project Settings `Additional Asset Directories to Cook` 등록. 런타임 에셋 로드는 `TSoftObjectPtr` + `LoadSynchronous()` — `LoadObject(TEXT(...))` 문자열 직접 로드 및 `ConstructorHelpers` 런타임 사용 금지.

### 2.9 세이브 구조

프로토타입 단계 — 역호환·마이그레이션 고려 금지, 항상 정석·클린 구조. "구 세이브가 깨질까봐" 어색한 구조를 유지하거나, deprecated 필드를 남기거나, 마이그레이션 코드를 추가하는 것 전부 금지 (CLAUDE.md 세이브 호환성 규칙). 출시(라이브) 전환 시 이 정책이 폐기되며 마이그레이션이 필수가 된다.

### 2.10 데이터 주도

서버/밸런스 관련 표시값·상수는 가능한 DataTable 또는 PlayFab TitleData 로 관리. 코드 하드코딩 분기(`switch` 산업별 이름 반환 등) 금지 (CLAUDE.md 데이터 주도 원칙). 단, 불변 시스템 상수·로그 문자열은 코드 상수 허용. 외부 서버 상수(폴링 간격·캡·쓰로틀 등)는 ini 또는 헤더 `static constexpr`로 단일 위치 관리.

### 2.11 빌드

UBT 직접 실행. 에디터가 켜져 있으면 Live Coding 잠금(exit 6) → 종료 후 풀빌드. 새 `UCLASS` 추가 또는 멤버 레이아웃 변경은 풀빌드 필수. 멤버 셰도잉 지역변수(`Slot`/`Owner`/`World`/`Children` 등) 금지 (`-WarningsAsErrors`). 빌드 전 stale `UnrealBuildTool` 프로세스 잠금 여부 확인 (exit 6 = 락 충돌, 컴파일 에러 아님).

### 2.12 보안 비밀값

API 키·엔드포인트·Title ID 등 비밀값은 `Config/*.ini` 또는 적절한 외부 위치에 관리. 코드 하드코딩 비밀 커밋 주의. 현재 Firebase Base URL 이 `ChatManagerSubsystem.cpp` 하드코딩 + `DefaultGame.ini` 두 곳에 존재(정리 대상 — §3 함정 참조).

---

## 3. 프로젝트 특유 함정

작업하며 발견되는 함정을 이 절에 누적 추가한다. 새 함정 발견 시 작업 완료 보고에 "PLAYBOOK §3 추가 제안" 항목으로 기재.

### 3.1 PlayFab 클라 API 직접 쓰기 오해

`UpdateUserData()`/`UpdatePlayerStatistics()` 는 PlayFab **클라이언트 API** — 클라가 서버에 직접 쓸 수 있다. "데이터를 PlayFab에 둔다"만으로 치팅이 막히지 않는다. ARCHITECTURE §0.2 서버 검증 권위 모델을 항상 기준으로 삼을 것.

### 3.2 헤더 주석과 .cpp 리터럴 불일치

헤더 주석의 통계명/엔드포인트 이름과 실제 `.cpp` 리터럴이 다를 수 있다. 리터럴의 정본은 `.cpp` 파일의 실제 할당값이다 (예: PlayFab 통계명은 `RankingManagerSubsystem.cpp` L14~16 할당이 정본, 헤더 주석 아님). 계약 레지스트리(ARCHITECTURE §4)와 `.cpp` 리터럴을 대조해서 확인. → HQLevel 빈 문자열 폐기 케이스는 §3.7 참조.

### 3.3 로드 직후 SaveGameData 덮어쓰기

`LoadAfterStart()` 내부에서 `SaveGameData()`를 끼우면 빈 메모리 상태로 세이브가 덮어써진다. 로드 완료(`OnGameDataLoaded`) 이후에만 세이브를 쓸 것. 상세: 메모리 `save_load_architecture` 문서 및 ARCHITECTURE §3.4 참조.

### 3.4 HTTP 콜백 내 this 수명

비동기 HTTP 응답 콜백에서 `this` 가 이미 파괴돼 있을 수 있다. `TWeakObjectPtr<ThisClass> WeakThis = this;` 캡처 후 콜백 진입 즉시 `if (!WeakThis.IsValid()) return;` 가드 필수.

### 3.5 채팅 폴링 배터리·비용

3초 폴링은 모바일 배터리와 Firebase Cloud Functions 비용에 직접 영향. 앱 백그라운드 진입 시 `StopPolling()` 호출 필수. 향후 푸시 알림(FCM) 경로로 전환 예정 — 폴링 로직은 `ChatManagerSubsystem` 안으로만 격리해 향후 교체를 용이하게 유지.

### 3.6 Firebase Base URL 중복 — 해소 완료 (2026-06-21)

`ChatManagerSubsystem.cpp` 의 하드코딩이 제거되고 `LoadFirebaseConfig()` 가 `GConfig->GetString` 으로 `DefaultGame.ini` `[/Script/CompanyGrowthRenewal.ChatManagerSubsystem]` `FirebaseBaseUrl` 을 읽도록 변경됨. ini 읽기 실패 시에만 코드 폴백. 단일 진실 원천 = ini. URL 변경 시 ini 만 수정.

### 3.10 ini URL 값은 따옴표 필수 — `//` 가 주석으로 잘림 (2026-07-29 실측)

`FirebaseBaseUrl=https://api-...` 처럼 **따옴표 없이** URL 을 쓰면 UE config 파서가 **따옴표 밖의 `//` 를 주석으로 보고 잘라낸다**. 근거 = `ConfigCacheIni.cpp:1253` 주석 *"it contains unquoted '//' (interpreted as a comment when importing)"*. 실제 결과: 런타임 값이 `https:` → URL 이 `https:/chat/send` → libcurl 이 `chat` 을 호스트로 파싱 → **`libcurl error 6 (Could not resolve host: chat)` 무한 반복**. 3.6 에서 ini 로 이관한 `dc7d8ef0`(2026-06-21) 이후 채팅이 한 번도 동작하지 않았다.

- **정답**: `FirebaseBaseUrl="https://YOUR_SERVICE_URL.example.com"` — 따옴표로 시작하면 `FParse::QuotedString` 경로를 타서 주석 스트립을 건너뛴다(`ConfigCacheIni.cpp:972`). 엔진 기본 ini 의 URL 값이 **전부** 따옴표로 감싸져 있는 이유.
- **왜 한 달간 안 드러났나**: 잘린 `https:` 도 `IsEmpty()` 는 false 라 `SendMessage`/`FetchRecentMessages` 의 빈 값 가드를 그대로 통과했다. 값 가드는 "비었나"가 아니라 **"쓸 수 있는 값인가"**(스킴 검사)로 짤 것 — loud failure 원칙(§2.3).

### 3.7 죽은 StatName_HQLevel — 제거 완료 (2026-06-21)

헤더 선언 + `.cpp` 빈 문자열 할당 모두 삭제됨. Grep 확인: Source 전체 실참조 0건. ARCHITECTURE §4.2 에 폐기 이력 유지.

### 3.8 에디터 이중 실행 빌드 충돌

에디터가 이미 켜져 있을 때 빌드하면 exit 6 (lock 충돌). 빌드 전 `Get-Process UnrealEditor` 로 기존 프로세스 확인 후 종료. 빌드 후 에디터 자동 실행 전에도 동일하게 확인 (이중 인스턴스 사고 이력).

### 3.9 stale UnrealBuildTool 프로세스

빌드 실패 후 `UnrealBuildTool.exe` 가 백그라운드에 잔류해 다음 빌드에서 락 충돌(exit 6)이 날 수 있다. 빌드 재시도 전 `Get-Process UnrealBuildTool` 로 잔류 프로세스 kill 후 진행.
