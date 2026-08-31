# 프로젝트 아키텍처

> **2026-08-31 레거시 맵 제거 반영** — 런타임 구조는 플레이 맵 3개 + 부팅 전용 `LoadingMap`이다. 대부분의 장기 도메인 상태는 `UGameInstanceSubsystem` 서비스가 소유하고, `UCGGameInstance`는 범용 레벨 전환·제한된 진입 컨텍스트와 저장 복원되는 `NextBuildingIndex` 발급을 담당한다. 레벨별 `GameMode`와 `ActorComponent`는 해당 월드에 종속된 표현·행동 계층이다.

## Level Flow (레벨 흐름도)

게임은 부팅 전용 맵 1개와 플레이 맵 3개로 구성된다. `UCGGameInstance::TransitionToLevel`이 범용 전환을 담당하며, 지속되는 게임 도메인 상태는 원칙적으로 각 `UGameInstanceSubsystem`에 남는다. 현행 예외는 `UCGGameInstance`가 저장·복원하는 다음 건물 ID(`NextBuildingIndex`)다.

### 런타임 레벨 (`Content/CompanyGrowth/Level/` 실측, 2026-08-31)

```
Content/CompanyGrowth/Level/
├── LoadingMap.umap                 # 부팅 전용 로딩 씬
├── MainMap_TheRiverwalkCity.umap   # 도시·건물·부지·회사 관리
├── OfficeMap.umap                  # 직원·오피스·프로젝트 개발
├── WorldMap.umap                   # 채광·공장·무역
└── (개발/테스트 맵 — 런타임 플레이·MapsToCook 대상 아님)
```

> 구 문서의 `MainMenuMap.umap`은 **존재하지 않음** (Content 레벨 폴더 실측) — 메인 메뉴 없이 LoadingMap → MainMap_TheRiverwalkCity로 부팅한다. 단 `AMainMenuGameModeBase` 클래스 파일은 `Private/GameMode/MainMenuGameModeBase.h/.cpp`에 **외부 참조 0건 고아**로 잔존 (Config 히트는 에디터 뷰포트 북마크뿐, 게임플레이 무관) — CLAUDE.md 레거시 제거 규칙에 따라 사용자 확인 후 삭제 후보.

### 레벨 전환 흐름

```
LoadingMap
    ↓
MainMap_TheRiverwalkCity ⇄ OfficeMap
             ⇅
          WorldMap
```

- 직원 채용은 `OfficeMap` 내부의 `UEmployeeGachaPresentationWidget`과 공유 `AGachaCaptureStage`가 2D 리빌·라이브 컷아웃을 담당한다.
- 건물 특성·스킨 가챠도 `MainMap_TheRiverwalkCity` 안의 현행 2D 프레젠테이션 흐름으로 끝나며 별도 맵으로 전환하지 않는다.

### 레벨별 GameMode (실측)

| 레벨 | GameMode | 위치 |
|------|----------|------|
| MainMap_TheRiverwalkCity | `ACGGameModeBase` | `Private/GameMode/CGGameModeBase.h` |
| LoadingMap | `ALoadingGameMode` | `Public/GameMode/` |
| OfficeMap | `AOfficeGameMode` | `Public/GameMode/` |
| WorldMap | `AWorldMapGameMode` | `Public/GameMode/` |

### 레벨 전환 방법 (`Private/Core/CGGameInstance.h`)

```cpp
// 오피스 진입에 필요한 제한된 내비게이션 컨텍스트를 설정한다.
GameInstance->SetCurrentManagedBuilding(Building);
GameInstance->SetOfficeMode(EOfficeMode::Training);
GameInstance->TransitionToLevel(TEXT("OfficeMap"), 0);
```

`TransitionToLevel`은 `BlueprintCallable`인 범용 API다. 맵별 전용 전환 래퍼나 가챠 결과 임시 필드를 만들지 않고, 도메인 데이터는 해당 서브시스템이 계속 소유한다. 단, 저장 복원되는 `NextBuildingIndex` 발급은 현재 `UCGGameInstance`에 남아 있다.

### 상태 소유권과 월드 표현 경계

| 계층 | 책임 | 수명 |
|------|------|------|
| `UGameInstanceSubsystem` 도메인 서비스 | 직원·재화·미션·건물 특성/스킨·생산·무역 등 대부분의 지속 도메인 상태와 규칙 | 레벨 전환을 넘어 유지 |
| `UCGGameInstance` | 범용 레벨 전환, 저장/로드 전환 훅, 오피스·방문 진입 컨텍스트, 저장 복원되는 다음 건물 ID 발급 | 게임 인스턴스 |
| 레벨별 `GameMode` | 해당 월드의 스폰·초기화·진행 연결 | 현재 월드 |
| `Actor` / `ActorComponent` | 배치, 이동, 캡처, 상호작용 등 월드에 묶인 표현·행동 | 소유 월드/액터 |

### `UCGGameInstance`의 진입 컨텍스트와 현행 ID 예외

| 변수 | 타입 | 용도 |
|------|------|------|
| `CurrentManagedBuildingIndex` | `int32` | `OfficeMap`에서 관리할 건물 인덱스 |
| `CurrentOfficeMode` | `EOfficeMode` | 오피스 진입 모드 — Normal / PromotionTest / Training / FreeView (4모드) |
| `CurrentBuildingCompanyType` | `ECompanyType` | 오피스 진입 대상 건물의 산업 |
| `CurrentMapType` | `ECurrentMapType` | None / MainMap / OfficeMap / WorldMap |
| `VisitCitySnapshot` 외 | `FCitySnapshot` 등 | 랭킹 도시 방문용 읽기 컨텍스트 (`SetVisitData`/`IsVisitMode`) |
| `NextBuildingIndex` | `int32` | 새 건물 ID 발급과 저장·복원 — 현행 도메인 상태 예외 |

---

## Source Structure (실측, 2026-07-22)

```
Source/CompanyGrowthRenewal/
├── Public/
│   ├── Builder/      # 빌더 패턴 클래스
│   ├── Data/         # 데이터 구조체 (EmployeeTypes, FatigueConfig 등)
│   ├── Entity/       # 게임 엔티티 (직원, 건물 등)
│   ├── Enum/         # 열거형 정의
│   ├── GameMode/     # 공개 GameMode (Loading/Office/World)
│   ├── Global/       # 전역 설정/상수 (CGDevSettings 등)
│   ├── Input/        # Enhanced Input 처리
│   ├── Interfaces/   # 인터페이스 정의
│   ├── Manager/      # 매니저 시스템 (31 헤더 — 아래 표)
│   ├── Office/       # 오피스/사무실 로직
│   ├── Player/       # 플레이어/카메라/치트 (CGCheatManager)
│   ├── Table/        # DataTable 행 구조체
│   ├── TimeCycle/    # 낮/밤 사이클 (TimeCycleSky 등)
│   ├── UI/           # CommonUI 기반 위젯 (Element/HUD/Panel/...)
│   └── Utils/        # 유틸리티
└── Private/
    ├── (위 폴더들의 구현) +
    ├── Core/         # CGGameInstance + Level/(CGLevelScriptBase, InGameLevel) — 헤더도 Private 쪽에 있음
    ├── GameMode/     # 구현 + Private 전용 헤더 (CGGameModeBase.h — 위 GameMode 표 참조. 고아 MainMenuGameModeBase.h/.cpp 잔존)
    ├── Manager/      # 구현 + Private 전용 매니저 헤더 (TableManagerSubsystem 등)
    └── Util/, Utils/ # 유틸 (두 폴더 공존 — 정리 후보)
```

> 주의: **`Public/`에는 `Core/`가 없다.** `CGGameInstance.h`, `CGGameModeBase.h`, `TableManagerSubsystem.h`, `UIManagerSubsystem.h`는 **Private 트리의 헤더**다. include 경로를 짐작하지 말고 실제 위치를 확인할 것.

---

## Manager System

대부분의 지속 도메인 상태는 `UGameInstanceSubsystem` 매니저가 소유해 레벨 전환 시에도 유지한다. 현행 `NextBuildingIndex` 예외는 `UCGGameInstance`에 남아 있다. 레벨별 `GameMode`와 `ActorComponent`는 상태를 현재 월드에 표현하며, 장기 상태의 권위가 아니다 (개별 상속은 각 헤더 확인).

### 주요 매니저 (한 줄 설명)

| 매니저 | 역할 |
|--------|------|
| `OfficeStageProgressManager` | 프로젝트 개발 페이즈 진행 (직능 스텝/점수/개발 이벤트/피버) |
| `ProjectOperationManager` | 출시 후 운영 — 수명·수익을 출시 순간 lock, 금고(Vault) 적립 |
| `TrendManagerSubsystem` | 산업 트렌드 롤링 (트렌드 일치 프로젝트 수익 배율) |
| `CityAcquisitionManager` | 도시 부지/입주 회사 인수 (오즈 공개 도박형) |
| `MissionManagerSubsystem` | 튜토리얼/미션 체인 (`DT_Mission` 주도) |
| `BuildingTraitManagerSubsystem` | 건물 특성 가챠/장착 (천장·마일리지 포함) |
| `WorldFactoryManager` | 세계지도 공장 라인 생산/강화 |
| `MineManager` | 채광소 자동 채취/강화/오프라인 캐치업 |
| `PlayFabManagerSubsystem` | 백엔드 (로그인, 플레이어 데이터, 랭킹 통계) |
| `EmployeeManager` | 직원 생성/성장 3축(직능 SP·강화 ★·잠재 큐브)/조회 |
| `EntityManager` | 엔티티 생성/관리 (건물, Factory 등) |
| `SaveLoadManager` | 저장/불러오기 + 지연 저장 (아래 저장 흐름) |
| `TableManagerSubsystem` (Private) | DataTable 로드/조회 (생성자 FObjectFinder 로드 패턴) |
| `UIManagerSubsystem` (Private) | UI 위젯 생성/스택 관리 (`GetWidgetClass(EWidgetType)`) |

### 전체 목록 (`Public/Manager/` 31 헤더, 이름만 — 개별 설명은 각 헤더/담당 문서 참조)

BuildingSkinManagerSubsystem, BuildingTraitManagerSubsystem, ChatManagerSubsystem, CityAcquisitionManager, CityCompanyDirector, CountryMarketManager, EmployeeManager, EntityManager, ItemInventoryManager, KeystoneAuraSubsystem, MineManager, MissionManagerSubsystem, OfficeStageProgressManager, PlayFabManagerSubsystem, ProductionOrderManager, ProjectOperationManager, ProjectTraitEventHandler, RankingManagerSubsystem, RecruitmentManagerSubsystem, ResourceItemManager, SaveLoadManager, SettingsManagerSubsystem, ShopManagerSubsystem, SoundManagerSubsystem, SpawnManager, TradeOrderManager, TradePort, TrendManagerSubsystem, WindowLightManagerSubsystem, WorldFactoryManager, WorldMapManager

추가로 `Private/Manager/`에 TableManagerSubsystem, UIManagerSubsystem, LevelManager, SystemManager, ManagerBase가 있다.

---

## 저장 흐름 (2026-07-10 히칭 수술 반영)

`SaveLoadManager` (`Public/Manager/SaveLoadManager.h`) 기준:

- **즉시 저장** `SaveGameData()` — 동기 전체 직렬화 + 디스크 쓰기. 레벨 전환/종료 등 저빈도 지점 전용. **클릭마다 부르면 프레임 히칭.**
- **지연 저장** `RequestDeferredSave()` — 고빈도 변경(강화 클릭/홀드 등) 전용 스로틀: 첫 요청 후 **3초** 뒤 1회 저장(`DeferredSaveDelaySeconds = 3.0f`), 대기 중 재요청은 무시. 고빈도 경로는 `SpendResource(..., bShouldSave=false)` + `RequestDeferredSave()` 조합이 정석 (예: `EmployeeManager::EnhanceEmployee`).
- **레벨 전환 쌍**: `CGGameInstance::SaveGameBeforeLevelTransition()` (떠나기 전 저장) ↔ `LoadGameAfterLevelStart()` (도착 후 로드). **주의: LoadGameAfterLevelStart 경로에 Save를 끼우면 아직 로드 전인 매니저들의 빈 메모리가 디스크 세이브를 덮는다** — `bInitialLoadComplete` 가드가 이 사고를 막는 안전핀 (SaveLoadManager.h 주석 참조).

---

## 코드 패턴 참고

새로운 기능 구현 시 기존 코드 패턴 참고:

| 새로 만들 것 | 참고할 기존 코드 |
|--------------|-------------------|
| 매니저 클래스 | `Manager/` 폴더 |
| UI 위젯 | `UI/` 폴더 내 비슷한 위젯 (Element/HUD/Panel) |
| 엔티티 클래스 | `Entity/` 폴더 내 기존 엔티티 |
| DataTable 구조체 | `Table/` 폴더 내 FTableRowBase |
| 서브시스템 | `SoundManagerSubsystem` |
| 빌더 패턴 | `Builder/` 폴더 |
| 낮/밤·시간 연출 | `TimeCycle/` 폴더 |

---

*문서 버전: 2.1*
*최종 수정: 2026-08-31 (레거시 맵 제거 및 상태 소유권 경계 반영)*
