# BACKEND_ARCHITECTURE.md — 백엔드 구조 단일 진실 원천

- **작성일**: 2026-06-21
- **상태**: 초판 (실제 소스 정독 기반, grep 검증 완료)
- **목적**: `server-engineer` 에이전트가 작업 시작 전 반드시 읽을 "무엇이 어디서" SOT. 모든 백엔드 식별자(통계명/UserData 키/엔드포인트/API)는 실제 소스에서 추출·검증된 리터럴.

---

## 0. 백엔드 경계 + 서버 검증 권위

### 0.1 책임 분담 원칙 (사용자 확정 — 불변 가드레일)

- **Firebase = 채팅 전용.** 플레이어 정보/데이터/경제를 Firebase 에 두지 않는다.
- **PlayFab = 그 외 서버 측 전부.** 인증 + 플레이어 정보/데이터 + 랭킹/리더보드 + 서버시간.
- 같은 데이터를 두 백엔드에 분산 금지(정합 붕괴 방지). 신규 기능은 이 경계로 라우팅.

**가장 큰 잠재 위험**: 경제(구매·가챠·재화)가 전부 클라이언트 신뢰 모델이라 메모리 조작에 무방비. 현재는 프로토타입 단계라 허용되지만, 라이브 전환 시 서버 권위 마이그레이션이 필수가 된다.

### 0.2 플레이어 데이터 권위 — 목표 모델 = "서버 검증 권위" (사용자 보안 우선 확정)

사용자 우선순위는 **치팅/해킹 방어**. 핵심 인식: **로컬 캐시 유무는 치트 벡터가 아니다.** 진짜 취약점은 "클라이언트가 계산한 결과를 서버에 그대로 쓰는 것"이며, **PlayFab 클라이언트 API(`UpdateUserData`/`UpdatePlayerStatistics`)는 클라이언트가 직접 쓸 수 있으므로 "데이터를 PlayFab에 둔다"만으로는 치팅이 막히지 않는다.**

**목표(북극성) 모델 — 서버 검증 권위:**

| 구성요소 | 안전한 형태 |
|---|---|
| 진실의 원천 | PlayFab (서버) |
| 민감 데이터(재화/인벤토리/구매한도/가챠피티) | **서버 전용 쓰기** — 클라 read-only, 직접 쓰기 차단 |
| 경제 변경(구매/가챠/지급/IAP) | 클라는 **의도만 전송** → **서버 함수(CloudScript/Functions)가 검증·적용·결과반환** |
| 로컬 데이터 | **읽기 캐시 + 비민감 진행 버퍼만.** 재접속 시 서버 상태가 권위(서버가 조정) |
| 비민감 예측 | 낙관적 표시 OK, 서버가 권위값 재계산 (오프라인 보상 = 이미 PlayFab 서버시간 검증 방식) |

이 모델에선 로컬 캐시가 있어도 안전하다(표시용일 뿐 권위/검증은 서버). 목표는 "캐시 회피"가 아니라 **"클라가 민감 데이터를 직접 못 씀 + 경제 변경을 서버가 검증"**.

**현재 vs 목표 갭**: 현재는 로컬 `USaveGame_GameData` 권위 + 경제 전부 클라 처리(서버 검증 0) = 프로토타입 상태. 목표(서버 검증 권위)로 가려면 **외부 서버 코드(PlayFab CloudScript / Azure / Firebase Functions) 작성이 필요** = 본 스펙 범위 밖의 "서버 권위 마이그레이션"(별도 스펙). 이 문서는 **현재 상태와 목표 모델을 함께 명시**하며, `server-engineer` 에이전트는 신규 작업이 목표 모델에 무재작성 정합하도록 강제한다.

---

## 1. 시스템 맵

```
[클라이언트 — UE5.4]
      |
      +--[로컬 세이브]--------> USaveGame_GameData("GameSlot")
      |                         - 게임 진행 상태 권위 (재화/인벤토리/미션/가챠)
      |                         - 트리거: TransitionToLevel(저장) / LoadAfterStart(로드)
      |
      +--[PlayFab SDK]---------> PlayFab(Azure)
      |   UPlayFabManagerSubsystem  - 인증 (게스트 CustomID / Google OAuth)
      |   URankingManagerSubsystem  - Classic Statistics 리더보드
      |                             - UserData (CitySnapshot / RankingProfile / LastSync)
      |                             - 서버 시간 (오프라인 보상 안티치트)
      |
      +--[FHttpModule]---------> Firebase Cloud Functions(GCP)
          UChatManagerSubsystem     - 글로벌 채팅 (HTTP 폴링 3초)
                                    - 엔드포인트: https://YOUR_SERVICE_URL.example.com
```

**기술 스택 요약**

| 구분 | 기술 | 역할 |
|---|---|---|
| 로컬 | `USaveGame` + `UGameplayStatics` | 게임 상태 권위 저장소 |
| 클라우드 A | PlayFab(Azure) SDK | 인증/통계/UserData/서버시간 |
| 클라우드 B | Firebase Cloud Functions(GCP) | 채팅 전용 REST API |
| 통신 | `FHttpModule` + `Json` | Firebase HTTP 폴링 |

실제 서버 코드(Firebase Functions JS/TS, PlayFab CloudScript)는 이 UE 프로젝트 밖에 있다.

---

## 2. 권위 경계표 (현재 vs 목표)

| 데이터 영역 | 현재 권위 | 현재 동기화 방식 | 목표 권위 | 비고 |
|---|---|---|---|---|
| 재화 (Money/Diamond/Brick 등) | **클라이언트** (로컬 SaveGame) | 없음(서버 검증 0) | 서버(PlayFab CloudScript 검증) | HIGH: 메모리 조작 무방비 |
| 아이템 인벤토리 (채용권/강화재료 등) | **클라이언트** (로컬 SaveGame) | 없음 | 서버(CloudScript 검증) | HIGH |
| 상점 구매 한도/리셋 | **클라이언트** (로컬 SaveGame) | 없음 | 서버 | HIGH |
| 가챠 피티/마일리지 | **클라이언트** (로컬 SaveGame) | 없음 | 서버 | HIGH |
| 플레이어 인증/세션 | **서버** (PlayFab) | 로그인 시 동기화 | 유지 | 현재도 서버 권위 |
| 디스플레이 이름 | **서버** (PlayFab) | `UpdateDisplayName()` → PlayFab | 유지 | |
| 랭킹/리더보드 | **서버** (PlayFab Statistics) | `UploadMarketCap` → PlayFab 통계명 `"MarketCap"` / `UploadWeeklyRevenueDelta` → PlayFab 통계명 `"WeeklyRevenue"` (클라 직접 씀) | 서버 검증 후 기록 | 현재 클라 직접 쓰기 = 치트 가능. 통계명은 §4.2 참조 |
| 도시 스냅샷 | 로컬 빌드 → **서버** (PlayFab UserData) | `UploadCitySnapshot()` 60초 쓰로틀 | 유지(비민감) | 방문 모드용, 조작해도 무해 |
| 랭킹 프로필 | 로컬 빌드 → **서버** (PlayFab UserData) | `UploadRankingProfile()` | 유지(비민감) | |
| LastSync (오프라인 기준점) | **서버** (PlayFab UserData) | 로그인/하트비트(10분)/로그아웃 | 유지 | 오프라인 보상 안티치트 핵심 |
| 서버 시간 | **서버** (PlayFab GetTime) | `RequestServerTime()` — 로그인 시 + 10분 하트비트 | 유지 | 12시간 캡 적용 |
| 채팅 메시지 | **서버** (Firebase Firestore) | HTTP 폴링 3초 + 로컬 캐시(MaxCachedMessages=100) | 유지 | Firebase 전용 영역 |
| 미션 진행 | **클라이언트** (로컬 SaveGame) | 없음 | 유지(비민감 진행) | |

---

## 3. 매니저 인벤토리

### 3.1 PlayFabManagerSubsystem

- **파일**: `Public/Manager/PlayFabManagerSubsystem.h` / `Private/Manager/PlayFabManagerSubsystem.cpp`
- **종류**: `UGameInstanceSubsystem`
- **책임**: PlayFab 인증(게스트/구글), UserData 저장·로드, Classic Statistics 리더보드 업로드·조회, 서버 시간 조회, 오프라인 보상 시퀀스, 하트비트

**Public API**

| 함수 | 시그니처 | 설명 |
|---|---|---|
| `LoginAsGuest` | `void LoginAsGuest()` | 디바이스 ID 기반 CustomID 로그인 |
| `LoginWithGoogle` | `void LoginWithGoogle(const FString& ServerAuthCode)` | 구글 OAuth 로그인 |
| `Logout` | `void Logout()` | 상태 초기화 + LastSync 저장 |
| `UpdateDisplayName` | `void UpdateDisplayName(const FString& NewDisplayName)` | PlayFab 디스플레이 이름 변경 |
| `SavePlayerData` | `void SavePlayerData(const FString& Key, const FString& Value)` | PlayFab UserData 저장 (Public 권한) |
| `LoadPlayerData` | `void LoadPlayerData(const FString& Key)` | PlayFab UserData 로드 |
| `IsLoggedIn` | `bool IsLoggedIn() const` | 로그인 여부 |
| `GetUserInfo` | `const FPlayFabUserInfo& GetUserInfo() const` | 현재 유저 정보 |
| `GetLoginState` | `EPlayFabLoginState GetLoginState() const` | 로그인 상태 enum |
| `UpdateLeaderboardScore` | `void UpdateLeaderboardScore(const FString& LeaderboardName, int64 Score)` | Classic UpdatePlayerStatistics |
| `GetLeaderboard` | `void GetLeaderboard(const FString& LeaderboardName, int32 PageSize)` | 상위 N명 조회 |
| `GetLeaderboardAroundPlayer` | `void GetLeaderboardAroundPlayer(const FString& LeaderboardName, int32 MaxResults)` | 내 주변 순위 조회 |
| `GetOtherPlayerData` | `void GetOtherPlayerData(const FString& PlayFabId, const TArray<FString>& Keys)` | 타 플레이어 UserData 조회 |
| `GetEntityId` | `const FString& GetEntityId() const` | v2 API Entity ID |
| `GetEntityType` | `const FString& GetEntityType() const` | v2 API Entity Type |
| `RequestServerTime` | `void RequestServerTime()` | PlayFab GetTime 호출 |
| `UpdateLastSync` | `void UpdateLastSync(int64 ServerUtcTimestamp)` | LastSync UserData 갱신 |
| `StartHeartbeat` | `void StartHeartbeat()` | 10분 주기 하트비트 시작 |
| `StopHeartbeat` | `void StopHeartbeat()` | 하트비트 중지 |
| `StartOfflineGainsSequence` | `void StartOfflineGainsSequence()` | 로그인 직후 오프라인 보상 시퀀스 |

**델리게이트**

| 델리게이트 | 시그니처 | 발화 시점 |
|---|---|---|
| `OnLoginComplete` | `FOnPlayFabLoginComplete(bool bSuccess)` | 로그인 성공/실패 |
| `OnDataLoaded` | `FOnPlayFabDataLoaded(const FString& Key, const FString& Value)` | 일반 LoadPlayerData 응답 |
| `OnError` | `FOnPlayFabError(const FString& ErrorMsg)` | 범용 API 에러 |
| `OnLeaderboardResult` | `FOnLeaderboardResult(const FString& LeaderboardName, const TArray<FPlayFabLeaderboardEntry>&)` | 리더보드 조회 결과 |
| `OnOtherPlayerDataLoaded` | `FOnOtherPlayerDataLoaded(const FString& PlayFabId, const FStringStringMap&)` | 타 플레이어 데이터 응답 |
| `OnServerTimeReceived` | `FOnPlayFabServerTimeReceived(int64 UtcUnixTimestamp)` | 서버 시간 수신 |
| `OnOfflineGainsRequested` | `FOnPlayFabOfflineGainsRequested(float OfflineSeconds)` | 오프라인 보상 계산 요청 |

---

### 3.2 ChatManagerSubsystem

- **파일**: `Public/Manager/ChatManagerSubsystem.h` / `Private/Manager/ChatManagerSubsystem.cpp`
- **종류**: `UGameInstanceSubsystem`
- **책임**: Firebase Cloud Functions REST API 연동, 글로벌 채팅 전송·수신, HTTP 폴링 관리

**Public API**

| 함수 | 시그니처 | 설명 |
|---|---|---|
| `SendMessage` | `void SendMessage(const FString& Content, const FString& Channel = TEXT("global"))` | HTTP POST 전송 |
| `FetchRecentMessages` | `void FetchRecentMessages(const FString& Channel = TEXT("global"), int32 Limit = 30)` | HTTP GET 수동 조회 |
| `StartPolling` | `void StartPolling(float IntervalSeconds = 3.0f)` | 폴링 시작 (기본 3초) |
| `StopPolling` | `void StopPolling()` | 폴링 중지 |
| `GetCachedMessages` | `const TArray<FChatMessage>& GetCachedMessages() const` | 캐시된 메시지 반환 |
| `IsPolling` | `bool IsPolling() const` | 폴링 중 여부 |

**델리게이트**

| 델리게이트 | 시그니처 | 발화 시점 |
|---|---|---|
| `OnMessagesReceived` | `FOnChatMessagesReceived(const TArray<FChatMessage>&)` | 새 메시지 수신 시 |
| `OnSendResult` | `FOnChatSendResult(bool bSuccess)` | 전송 결과 |
| `OnChatError` | `FOnChatError(const FString& ErrorMsg)` | 채팅 에러 |

**내부 상수**: `MaxCachedMessages = 100`, 기본 폴링 채널 `"global"`

---

### 3.3 RankingManagerSubsystem

- **파일**: `Public/Manager/RankingManagerSubsystem.h` / `Private/Manager/RankingManagerSubsystem.cpp`
- **종류**: `UGameInstanceSubsystem`
- **책임**: 시총/주간매출 리더보드 업로드, 리더보드 조회, 도시 스냅샷 업로드·다운로드, 방문 모드 진입·퇴출, 랭킹 프로필 관리

**Public API**

| 함수 | 시그니처 | 설명 |
|---|---|---|
| `UploadMarketCap` | `void UploadMarketCap(int64 MarketCapValue)` | StatName_AllTime("MarketCap") 업로드 |
| `UploadWeeklyRevenueDelta` | `void UploadWeeklyRevenueDelta(int64 Delta)` | StatName_Weekly("WeeklyRevenue") 업로드, Delta>0 시만 |
| `UploadRevenueScore` | `void UploadRevenueScore()` | 레거시 호환 — 내부에서 UploadMarketCap+UploadRankingProfile |
| `ForceUploadRevenueScore` | `void ForceUploadRevenueScore()` | 쓰로틀 무시 즉시 업로드 |
| `FetchLeaderboard` | `void FetchLeaderboard(int32 TabIndex, int32 Count = 100)` | TabIndex: 0=시총, 1=주간매출 |
| `FetchLeaderboardAroundPlayer` | `void FetchLeaderboardAroundPlayer(int32 TabIndex, int32 Count = 20)` | 내 주변 순위 |
| `GetCachedLeaderboard` | `const TArray<FRankingEntry>& GetCachedLeaderboard() const` | 캐시된 리더보드 |
| `UploadCitySnapshot` | `void UploadCitySnapshot()` | UserData "CitySnapshot"에 JSON 업로드 |
| `FetchCitySnapshot` | `void FetchCitySnapshot(const FString& TargetPlayFabId)` | 타 플레이어 "CitySnapshot" 조회 |
| `EnterVisitMode` | `void EnterVisitMode(const FString& TargetPlayFabId, const FString& TargetDisplayName)` | 스냅샷 다운로드 후 방문 맵 전환 |
| `ExitVisitMode` | `void ExitVisitMode()` | 방문 모드 종료, MainMap 복귀 |
| `UploadRankingProfile` | `void UploadRankingProfile()` | UserData "RankingProfile"에 compact JSON 업로드 |

**델리게이트**

| 델리게이트 | 시그니처 | 발화 시점 |
|---|---|---|
| `OnLeaderboardLoaded` | `FOnLeaderboardLoaded(int32 TabIndex, const TArray<FRankingEntry>&)` | 리더보드 조회 완료 |
| `OnCitySnapshotLoaded` | `FOnCitySnapshotLoaded(const FString& PlayFabId, const FCitySnapshot&)` | 도시 스냅샷 수신 |
| `OnRankingError` | `FOnRankingError(const FString& ErrorMsg)` | 에러 |

**내부 상수**: `UploadThrottleSeconds = 60.0` (업로드 최소 간격)

---

### 3.4 SaveLoadManager

- **파일**: `Public/Manager/SaveLoadManager.h` / `Private/Manager/SaveLoadManager.cpp`
- **종류**: `UGameInstanceSubsystem`
- **책임**: 로컬 SaveGame 저장·로드, 건물 EXP/레벨업, HQ 레벨, 회사 등급, 오프라인 보상 적용

**주요 API (백엔드 연동 관련)**

| 함수 | 설명 |
|---|---|
| `SaveGameData()` | `USaveGame_GameData`를 `"GameSlot"` 슬롯에 저장 |
| `LoadGameData()` | `"GameSlot"` 슬롯에서 로드, `OnGameDataLoaded` 브로드캐스트 |
| `GetCurrentSaveData()` | 캐시 활용 세이브 데이터 반환 |
| `CalculateOfflineGains(float OfflineSeconds)` | PlayFab `OnOfflineGainsRequested` 리스너 — 빌딩별 수익 누적 |

**델리게이트**

| 델리게이트 | 발화 시점 |
|---|---|
| `OnGameDataLoaded` | LoadGameData 완료 후 |
| `OnOfflineGainsApplied(float TotalGained, float OfflineSeconds)` | 오프라인 보상 적용 완료 |
| `OnTierUnlocked(int32 NewTier)` | 빌딩 티어 승급 (⚠ `OfficeStageProgressManager` = WorldSubsystem, 오피스 맵 전용) |
| `OnHQLevelUp(int32 NewLevel)` | 본사 레벨업 |
| `OnCompanyTitleChanged(ECompanyTitle NewTitle)` | 회사 등급 변경 |

---

### 3.5 경제 매니저 5종 (클라이언트 권위, 서버 검증 0)

다음 매니저들은 현재 전부 로컬 처리이며 서버 검증이 없다. 라이브 전환 시 마이그레이션 대상.

| 매니저 | 책임 | 보안 상태 |
|---|---|---|
| `ShopManagerSubsystem` | 상점 구매, 한도/리셋 | 클라 권위, 서버 검증 0 |
| `ResourceItemManager` | 재화(Money/Diamond/Brick 등) 입출력 | 클라 권위, 서버 검증 0 |
| `RecruitmentManagerSubsystem` | 가챠 뽑기, 피티/마일리지, 직원 채용 | 클라 권위, 서버 검증 0 |
| `BuildingSkinManagerSubsystem` | 스킨 가챠, 스킨 소유권 | 클라 권위, 서버 검증 0 |
| `ItemInventoryManager` | 아이템 인벤토리 (채용권/강화재료 등) | 클라 권위, 서버 검증 0 |

---

## 4. 계약 레지스트리 (정본)

### 4.1 Firebase 엔드포인트

**기본 URL**: `https://YOUR_SERVICE_URL.example.com` — SOT = `Config/DefaultGame.ini` `[/Script/CompanyGrowthRenewal.ChatManagerSubsystem]` `FirebaseBaseUrl`. 소스는 ini 읽기 실패 시 폴백만 보유. **ini 값은 반드시 큰따옴표로 감쌀 것** (PLAYBOOK §3.10)

| 엔드포인트 | 메서드 | URL suffix | 요청 필드 (JSON Body) | 응답 필드 (JSON) | 비고 |
|---|---|---|---|---|---|
| 메시지 전송 | POST | `/chat/send` | `senderId`, `senderName`, `content`, `channel`, `profileImageId` | HTTP 200 OK | 발신자 PlayFabId·이름·프로필 이미지 포함 |
| 메시지 조회 | GET | `/chat/messages?channel={ch}&limit={n}` | (쿼리 파라미터) | `{ "messages": [...] }` | `after={timestamp}` 선택 파라미터 — `LastMessageTimestamp`가 비어있을 때(첫 조회)는 생략, 이후 폴링에서만 첨부. 값은 ISO 8601 특수문자 URL 인코딩 필요 (`+`→`%2B`, `:`→`%3A`) |

**응답 메시지 객체 JSON 필드**

| 필드명 | 타입 | C++ 매핑 |
|---|---|---|
| `messageId` | string | `FChatMessage::MessageId` |
| `senderId` | string | `FChatMessage::SenderId` |
| `senderName` | string | `FChatMessage::SenderName` |
| `content` | string | `FChatMessage::Content` |
| `channel` | string | `FChatMessage::Channel` |
| `createdAt` | string (ISO 8601) | `FChatMessage::CreatedAt` |
| `profileImageId` | number | `FChatMessage::ProfileImageID` — `DT_ProfileImage.ImageID`. 2026-07-29 추가, 그 이전 문서엔 필드 없음 → 클라·서버 양쪽 `0`(기본 아이콘) 폴백 |

루트 응답 배열 키: `"messages"`

---

### 4.2 PlayFab Statistics 이름 (리더보드)

소스 파일 `RankingManagerSubsystem.cpp` L14~16 리터럴:

| C++ 상수 | 리터럴 값 | 모드 | 탭 인덱스 | 비고 |
|---|---|---|---|---|
| `StatName_AllTime` | `"MarketCap"` | Last (현재 시총) | TabIndex=0 | `UploadMarketCap()` 호출 |
| `StatName_Weekly` | `"WeeklyRevenue"` | Sum (Weekly Reset) | TabIndex=1 | `UploadWeeklyRevenueDelta(Delta)` 호출 |
| `StatName_HQLevel` | `""` (빈 문자열) | — | — | **폐기됨** (2026-04-24, 2탭 구조 전환) |

**주의**: 헤더 주석에 "MarketCap"/"WeeklyRevenue"라고 보이는 부분은 API 함수명이고, 실제 PlayFab 통계 이름 리터럴은 위 .cpp 할당값이 정본이다.

---

### 4.3 PlayFab UserData 키

소스에서 확인된 `SavePlayerData()`/`LoadPlayerData()` 첫 인자 리터럴:

| UserData 키 | 데이터 타입 | 쓰기 주체 | 읽기 주체 | 설명 |
|---|---|---|---|---|
| `"CitySnapshot"` | JSON 문자열 (`FCitySnapshot` 직렬화) | `RankingManagerSubsystem::UploadCitySnapshot()` | `FetchCitySnapshot()` → `HandleOtherPlayerData()` | 방문 모드용 빌딩 배치 스냅샷 |
| `"RankingProfile"` | Compact JSON (`dn/hq/ct/bc/mt/ec/rev/pi` 필드) | `RankingManagerSubsystem::UploadRankingProfile()` | `HandleOtherPlayerData()` | 리더보드 프로필 정보 |
| `"LastSync"` | Unix Timestamp 정수 문자열 | `PlayFabManagerSubsystem::UpdateLastSync()` | `StartOfflineGainsSequence()` 내부 | 오프라인 보상 기준점 |

**RankingProfile Compact JSON 필드 목록**

| 필드 | 의미 | C++ 소스 |
|---|---|---|
| `dn` | DisplayName | `GetUserInfo().DisplayName` |
| `hq` | HQ 레벨 | `GD.HQLevel` |
| `ct` | CompanyTitle (int) | `static_cast<int32>(GD.CompanyTitle)` |
| `bc` | BuildingCount | `GD.Buildings.Num()` |
| `mt` | MaxTier | 보유 빌딩 중 최고 프로젝트 티어. ⚠ 구 `mbl`(빌딩 레벨 1~30)과 스케일이 달라 **키를 재사용하지 않았다** — 재사용하면 구 데이터가 "최고 T27" 로 조용히 오독된다 |
| `ec` | EmployeeCount | OfficeDataMap 합산 |
| `rev` | TotalRevenueEarned | `GD.TotalRevenueEarned` |
| `pi` | ProfileImageID | `GD.ProfileImageID` |

---

### 4.4 설정 값

| 항목 | 값 | 위치 |
|---|---|---|
| PlayFab TitleId | `<PLAYFAB_TITLE_ID>` | 공개본에서 제외한 런타임 `Config`에 주입 |
| Firebase Base URL | `https://YOUR_SERVICE_URL.example.com` | `Config/DefaultGame.ini` `[/Script/CompanyGrowthRenewal.ChatManagerSubsystem]` `FirebaseBaseUrl` — **단일 진실 원천** (코드 하드코딩 제거 완료 2026-06-21) |
| 오프라인 보상 캡 | 12시간 (43200초) | `PlayFabManagerSubsystem.cpp` L14 `OfflineCapSeconds = 12 * 60 * 60` |
| 하트비트 주기 | 600초 (10분) | `PlayFabManagerSubsystem.h` L224 `static constexpr float HeartbeatIntervalSeconds = 600.0f` — 헤더에 static constexpr로 정의(유일 정의 위치, .cpp에 별도 할당 없음) |
| 채팅 폴링 기본 간격 | 3.0초 | `Config/DefaultGame.ini` `[/Script/CompanyGrowthRenewal.ChatManagerSubsystem]` `PollIntervalSeconds` — ini 단일 진실 원천, `ChatManagerSubsystem::ConfigPollIntervalSeconds` 멤버에 저장됨 (2026-06-21) |
| 채팅 캐시 상한 | 100건 | `ChatManagerSubsystem.h` `MaxCachedMessages = 100` |
| 업로드 쓰로틀 | 60초 | `RankingManagerSubsystem.h` `UploadThrottleSeconds = 60.0` |
| 로컬 세이브 슬롯 | `"GameSlot"` | `SaveLoadManager.h` L148 + `GameSaveData.h` L250 |

**공개본 주의**: 실제 Firebase Base URL과 런타임 `Config`는 제외했습니다. 소스의 URL은 설정 누락을 드러내는 비동작 플레이스홀더이며, 배포 환경에서는 외부 설정으로 주입합니다.

---

## 5. 데이터 흐름

### 5.1 로그인 + 오프라인 보상 시퀀스

```
[게임 시작]
    → LoginAsGuest() / LoginWithGoogle()
        → PlayFab LoginWithCustomID / LoginWithGoogleAccount
            → OnLoginSuccess()
                → OnLoginComplete.Broadcast(true)
                → StartOfflineGainsSequence()           -- PendingTimeRequest = Login
                    → RequestServerTime()
                        → PlayFab::GetTime
                            → OnGetTimeSuccess()
                                → PendingServerNow = ServerUnix
                                → bAwaitingLastSyncResponse = true
                                → LoadPlayerData("LastSync")
                                    → PlayFab::GetUserData("LastSync")
                                        → OnDataGetSuccess() [bAwaitingLastSyncResponse 분기]
                                            → OfflineSeconds = min(ServerNow - LastSync, 43200)
                                            → OnOfflineGainsRequested.Broadcast(OfflineSeconds)
                                                → SaveLoadManager::CalculateOfflineGains()
                                                    → OnOfflineGainsApplied.Broadcast()
                                            → UpdateLastSync(ServerNow)  -- LastSync 갱신
                → StartHeartbeat()                      -- 10분 주기 LastSync 갱신
```

### 5.2 세이브·부분 클라우드 동기화

```
[TransitionToLevel 호출 시]
    → SaveLoadManager::SaveGameData()               -- 로컬 USaveGame 저장
    → RankingManagerSubsystem::UploadRevenueScore() -- (60초 쓰로틀)
        → UploadMarketCap(MarketCapValue)           -- PlayFab "MarketCap" 통계 갱신
        → UploadRankingProfile()                    -- PlayFab "RankingProfile" UserData 갱신

[매출(Money 자원) 증가 시 — SaveLoadManager::OnResourceChangedHandler()]
    → RankingManagerSubsystem::UploadWeeklyRevenueDelta(Delta)   -- Delta = 증가분만
        → PlayFabManagerSubsystem::UpdateLeaderboardScore("WeeklyRevenue", Delta)

[랭킹 패널 진입 시]
    → ForceUploadRevenueScore()                    -- 쓰로틀 무시 즉시 업로드
```

### 5.3 채팅 폴링

```
[채팅 패널 열기]
    → ChatManagerSubsystem::StartPolling(3.0f)
        → FetchRecentMessages("global", 30)     -- 즉시 1회 조회
        → SetTimer(3초 반복)
            → PollMessages()
                → FetchRecentMessages("global")
                    → GET /chat/messages?channel=global&limit=30
                    → (LastMessageTimestamp 비어있지 않으면) &after={URL인코딩된LastTimestamp}
                        → OnFetchMessagesResponse()
                            → ParseMessagesFromJson()   -- "messages" 배열 파싱
                            → OnMessagesReceived.Broadcast(NewMessages)

[채팅 패널 닫기]
    → StopPolling()     -- 타이머 제거

[메시지 전송]
    → SendMessage(Content, "global")
        → POST /chat/send {senderId, senderName, content, channel}
        → 로컬 즉시 표시 (낙관적 UI)
        → OnMessagesReceived.Broadcast({LocalMsg})
```

### 5.4 도시 방문 모드

```
[리더보드에서 방문 버튼 클릭]
    → EnterVisitMode(TargetPlayFabId, TargetDisplayName)
        → FetchCitySnapshot(TargetPlayFabId)
            → GetOtherPlayerData(TargetPlayFabId, ["CitySnapshot"])
                → PlayFab::GetUserData
                    → HandleOtherPlayerData()
                        → Data.Find("CitySnapshot") → FCitySnapshot 파싱
                        → CGGameInstance::SetVisitMode(true)
                        → CGGameInstance::SetVisitData(Snapshot, ...)
                        → TransitionToLevel(TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"))  -- 방문 맵 전환
                    → OnCitySnapshotLoaded.Broadcast()

[방문 종료]
    → ExitVisitMode()
        → SetVisitMode(false) + ClearVisitData()
        → GI->TransitionToLevel(TEXT("/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity"))  -- 정상 MainMap 복귀
```

---

## 부록: 관련 데이터 구조체 파일

| 구조체 | 파일 | 용도 |
|---|---|---|
| `FGameSaveData` / `USaveGame_GameData` | `Public/Data/GameSaveData.h` | 로컬 세이브 루트 |
| `FChatMessage` | `Public/Data/ChatMessageData.h` | Firebase 채팅 메시지 |
| `FPlayFabUserInfo` / `EPlayFabLoginState` | `Public/Data/PlayFabUserData.h` | PlayFab 유저 런타임 정보 |
| `FRankingEntry` / `FCitySnapshot` / `FCitySnapshotBuilding` | `Public/Data/RankingData.h` | 랭킹·방문 데이터 |
