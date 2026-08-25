# 프로젝트 아키텍처

> **2026-07-22 감사 반영 재작성** — 레벨 3→6종/GameMode 실측, 매니저 7→31+α, 소스 트리 실측, 저장 흐름(RequestDeferredSave) 신설. `MainMenuMap.umap`은 존재하지 않아 삭제 (클래스 `AMainMenuGameModeBase`는 고아로 잔존 — 아래 참조).

## Level Flow (레벨 흐름도)

게임은 여러 레벨(맵)로 구성되며, `CGGameInstance`를 통해 레벨 간 데이터 전달.

### 핵심 레벨 (`Content/CompanyGrowth/Level/` 실측, 2026-07-22)

```
Content/CompanyGrowth/Level/
├── LoadingMap.umap                 # 부팅 로딩 씬 (게임 첫 맵. BGM 무음 가드 — OnPostLoadMapWithWorld)
├── MainMap_TheRiverwalkCity.umap   # 메인 게임 맵 (도시, 건물 배치, 부지 인수)
├── OfficeMap.umap                  # 오피스 씬 (직원 배치/행동, 프로젝트 개발)
├── LootBoxMap.umap                 # 뽑기(가챠) 3D 연출 씬
├── RecruitmentMap.umap             # 채용 씬 (사원증 발급 리빌 연출)
├── WorldMap.umap                   # 세계지도 씬 (11개국 채광/공장/무역)
└── (기타 테스트 맵 다수 — TestMap, CameraTestMap, EmployeeTestLevel 등, 쿠킹 대상 아님)
```

> 구 문서의 `MainMenuMap.umap`은 **존재하지 않음** (Content 레벨 폴더 실측) — 메인 메뉴 없이 LoadingMap → MainMap으로 부팅한다. 단 `AMainMenuGameModeBase` 클래스 파일은 `Private/GameMode/MainMenuGameModeBase.h/.cpp`에 **외부 참조 0건 고아**로 잔존 (Config 히트는 에디터 뷰포트 북마크뿐, 게임플레이 무관) — CLAUDE.md 레거시 제거 규칙에 따라 사용자 확인 후 삭제 후보.

### 레벨 전환 흐름

```
LoadingMap (부팅)
    ▼
┌─────────────────────────────────────────────────────────────┐
│                     MainMap (메인 게임)                      │
│  - 도시 전체 뷰, 건물 배치/관리, 부지·회사 인수              │
│  - Factory 클릭, 건물 관리 패널, HQ 패널                     │
└─────────────────────────────────────────────────────────────┘
     │ 가챠(뽑기)        │ 건물 → 오피스 진입      │ 세계지도
     ▼                  ▼                        ▼
┌──────────────┐   ┌──────────────────────┐   ┌──────────────┐
│  LootBoxMap  │   │      OfficeMap       │   │   WorldMap   │
│  3D 뽑기 연출│   │  직원 3D/프로젝트 개발│   │  채광/공장/  │
└──────────────┘   │  모드: EOfficeMode 4 │   │  무역 (11국) │
     │             └──────────────────────┘   └──────────────┘
     │                  │        ▲ ReturnFromRecruitmentMap
     │                  ▼        │
     │             ┌──────────────────────┐
     │             │   RecruitmentMap     │
     │             │  사원증 발급 연출     │
     │             └──────────────────────┘
     └────────────────► MainMap으로 복귀
```

### 레벨별 GameMode (실측)

| 레벨 | GameMode | 위치 |
|------|----------|------|
| MainMap | `ACGGameModeBase` | `Private/GameMode/CGGameModeBase.h` |
| LoadingMap | `LoadingGameMode` | `Public/GameMode/` |
| OfficeMap | `OfficeGameMode` | `Public/GameMode/` |
| LootBoxMap | `LootBoxMapGameMode` | `Public/GameMode/` |
| RecruitmentMap | `RecruitmentGameMode` | `Public/GameMode/` |
| WorldMap | `WorldMapGameMode` | `Public/GameMode/` |

### 레벨 전환 방법 (`Private/Core/CGGameInstance.h`)

```cpp
// CGGameInstance를 통한 레벨 전환 (전환 전 저장 포함)
GameInstance->TransitionToLevel("LootBoxMap", BackgroundIndex);

// 루트박스 전환 (카테고리 저장 후 전환)
GameInstance->TransitionToLootBoxMap(ELootBoxCategory::BuildingSkin, 0);

// 채용맵 전환 (가챠 결과 저장 후 전환) / 복귀
GameInstance->TransitionToRecruitmentMap(GachaResult, 0);
GameInstance->ReturnFromRecruitmentMap();   // → OfficeMap 복귀

// 오피스 전환 시 데이터 저장
GameInstance->SetCurrentManagedBuilding(Building);
GameInstance->SetOfficeMode(EOfficeMode::Training);
```

### 레벨 간 데이터 전달 (CGGameInstance 보관 상태)

| 변수 | 타입 | 용도 |
|------|------|------|
| `CurrentLootBoxCategory` | `ELootBoxCategory` | 루트박스 카테고리 |
| `CurrentManagedBuildingIndex` | `int32` | 관리 중인 건물 인덱스 (구 문서의 FString EntityID 아님) |
| `CurrentOfficeMode` | `EOfficeMode` | 오피스 진입 모드 — Normal / PromotionTest / Training / FreeView (4모드) |
| `CurrentBuildingCompanyType` | `ECompanyType` | 관리 중인 건물의 산업 |
| `CurrentMapType` | `ECurrentMapType` | 현재 맵 (None/MainMap/OfficeMap/LootBoxMap/RecruitmentMap/WorldMap) |
| `PendingGachaResult` | `FGachaResultData` | 채용맵으로 넘길 가챠 결과 |
| `VisitCitySnapshot` 외 | `FCitySnapshot` 등 | 랭킹 도시 방문 모드 데이터 (`SetVisitData`/`IsVisitMode`) |

---

## Source Structure (실측, 2026-07-22)

```
Source/CompanyGrowthRenewal/
├── Public/
│   ├── Builder/      # 빌더 패턴 클래스
│   ├── Data/         # 데이터 구조체 (EmployeeTypes, FatigueConfig 등)
│   ├── Entity/       # 게임 엔티티 (직원, 건물 등)
│   ├── Enum/         # 열거형 정의
│   ├── GameMode/     # 레벨별 게임 모드 (5종)
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

핵심 매니저는 대부분 `UGameInstanceSubsystem`으로 구현되어 레벨 전환 시에도 유지된다 (개별 상속은 각 헤더 확인).

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

*문서 버전: 2.0*
*최종 수정: 2026-07-22 (감사 반영 재작성)*
