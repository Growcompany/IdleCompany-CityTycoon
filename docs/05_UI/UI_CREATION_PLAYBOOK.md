# UI_CREATION_PLAYBOOK — UI 위젯 추가 표준 절차

> **새 UI 위젯/요소/오버레이를 만들기 전에 이 문서를 우선 참조.** 흩어진 규칙(CLAUDE.md 곳곳)과 실전 함정을 "순서형 체크리스트" 하나로 묶음. `ui-builder` 에이전트는 이 문서를 단일 진실 원천으로 따른다.
>
> **디자인/부품 선택은 자매 문서 `UI_STYLE_CATALOG.md` 가 단일 진실** — 공용 부품 카탈로그(버튼/공통/카드), 스타일 토큰(시그니처색/크림/등급색/Border 자원), 모달 골격 패턴. 새 위젯·브러시 발명 전 카탈로그의 기존 부품 먼저.

UE 5.4 / CommonUI 기반. 모든 경로는 프로젝트 루트 `CompanyGrowthRenewal/` 기준.

---

## 0. 시작 전 — 라이브 시스템 먼저 확인 (함정 #1)

비슷한 기능이 **이미 있는지, 그리고 그게 실제로 쓰이는지** 먼저 확인. 죽은/중복 시스템에 붙이지 말 것.

```
Grep 으로 "호출자"를 찾아라. 정의만 있고 외부 호출자가 0이면 죽은 코드다.
예: Builder/PlacementActor 계열은 Placement_Start 호출자가 폴더 밖에 0개 = dormant.
    실제 MainMap 배치는 UPlacementHandler(Player 컴포넌트)가 구동.
```
- 같은 이름 위젯이 2개일 수 있다(예: `BuildPlacementWidget`=추적형 / `BuildPlacementPanelWidget`=하단고정). **어느 쪽이 push 되는지** push 사이트를 grep으로 확정하고 붙여라.

---

## 0.5 디자인 브리프 — 목업/구현 전 질문 세트 (2026-06-06 도입)

> 첫 프롬프트/요청에는 요구사항이 다 담기지 않는다. **목업을 만들기 전에** 아래 질문을 채워 합의하고 시작할 것 (답이 자명하면 건너뛰되, 추측으로 메우지 말고 사용자에게 물을 것 — "알아서 해줘" 답변 허용).

| 질문 | 선택지 | 결정되는 것 |
|---|---|---|
| 패널 유형 | 중앙 모달(PushPrompt) / 바텀시트(PushBottom) / HUD 상주 / 월드추적 오버레이(AddToViewport) | §1 베이스 클래스, 딤/입력모드, 스택 레벨 |
| 데이터 밀도 | 스파스(CTA 중심) / 밸런스드 / 정보밀집(표·리스트) | 레이아웃 골격, 시맨틱 텍스트 스케일 |
| 재사용 부품 | UI_STYLE_CATALOG에서 **쓸 부품 목록** + 새로 만들 부품(있다면 등록 대상) | 발명 vs 재사용 경계 |
| CTA | 개수(0~2)·위치(하단 중앙/우하단)·파급(확정/취소 대칭?) | 모달 골격 패턴 픽 |
| 갱신 모델 | 정적 / 델리게이트 구독(OnResourceChanged 등) / self-clocking(NativeTick) | C++ 갱신 코드 구조 |

**변형 비교 원칙**: 스타일/레이아웃 갈림길이 있으면 텍스트로 묻지 말고 **같은 레이아웃 × 2~3 변형 목업을 나란히** 제시해 픽 받기 (dark_glass vs corp_light 선례). 비주얼 결정은 실물 비교가 항상 빠르다.

**목업 매체 = Artifact 우선 (2026-07-05 사용자 확정)**: 새 UI 목업/변형 픽시트는 **Artifact로 발행**(claude.ai 호스팅·사이드패널 인라인 렌더·버전관리·공유링크 — 로컬 `HTML→Chrome`보다 우선, 보기·이터레이션 훨씬 빠름). 발행 전 **`artifact-design` 스킬 로드 필수**.
- ① **반복 튜닝은 인터랙티브로**: 색/알파/크기/패딩을 스샷 왕복하며 맞추지 말고 **슬라이더·토글**로 만들어 사용자가 직접 dial-in → 확정 숫자만 받아 T3D 이식(스샷 루프 제거). ★ 텍스처 기반 요소(그리드/패턴/글로우) 알파는 CSS와 UMG가 다르게 나오니 실기 1회 보정은 여전히 필요.
- ② **아카이브**: 확정 목업 HTML은 `docs/`(`04_ArtDirection/UIChromePrompts/output/MOCKUP_*` 또는 `05_UI/`)에도 저장 — **Artifact는 repo 파일이 아님**, SOT는 repo.
- ③ **CSP 한계**: Artifact는 외부/로컬 이미지·웹폰트·스크립트 차단 → 실제 게임 스샷/텍스처 배경이 필요하면 **base64 임베드** 또는 로컬 HTML. 순수 CSS + 시스템폰트 목업은 Artifact 완승.

**배치 코멘트 수정 루프**: 목업/PIE 스크린샷 피드백은 한 건씩 왕복하지 말고 **번호 목록으로 모아 받아 일괄 수리** ("①폰트 작음 ②여백 과다 ③색 틀림" → 한 세션 일괄). 사용자에게도 이 형식을 권할 것.

**컨텍스트 발췌 원칙**: 목업/구현 시 카탈로그·타이포·크롬 스펙 문서를 통째로 컨텍스트에 넣지 말고 **해당 패널에 필요한 항목만 발췌 참조** — 통째 주입은 토큰 낭비 + 규칙 드리프트를 오히려 키운다 (디자인 시스템 통째 업로드가 드리프트를 만든 Claude Design 실측 교훈).

---

## 1. 위젯 타입 / 베이스 클래스 / 네이밍 결정

| 용도 | C++ 베이스 | WBP 접두사 |
|---|---|---|
| 스택에 push 하는 패널/모달 | `UCommonActivatableWidget` (또는 `UAnimatedActivatableWidget` = Show/Hide 애니) | `UI_` (레이어/패널) |
| 재사용 요소/오버레이 (AddToViewport, 추적) | `UCommonUserWidget` | `UIE_` (요소) |
| 기존 범용 | — | `WBP_` |

- **C++ 클래스명과 WBP 이름은 접두사만 다르고 나머지 일치.** 예: `UFloatingNumberWidget` ↔ `UIE_FloatingNumber`.
- 공통 디자인이 N곳에 복붙되는 anti-pattern 보이면 → 변형 WBP(Class Defaults baked-in) 추출 (`UIE_TabButton`/`UIE_PillTabButton` 식).
- **⚠ 단일 자식 래퍼 WBP 금지 — 정답은 Reparent.** 기존 위젯 하나만 품는 `UUserWidget` HAS-A 래퍼(자식 1개짜리 WBP)를 만들면 두 가지가 동시에 깨진다: ① 부모가 그 자리에 기대하는 BindWidget 타입과 불일치(예: `CommonButtonBase` 자리에 `UUserWidget` 합성 래퍼 → "바인딩 못 찾음" 컴파일 실패), ② 감싼 원본의 **Class Defaults(CDO 스타일/패딩/아이콘)가 래퍼에 막혀** 인스턴스에서 손댈 수 없음. 변형이 필요하면 래퍼를 씌우지 말고 **대상 WBP를 Reparent**(부모 클래스 교체)하거나 변형 WBP로 복제할 것. **왜**: 래퍼는 타입 체인과 CDO 상속 체인을 한 번에 끊는다.

---

## 2. 등록 — EWidgetType + DataTable (위젯 로드 방식, 필수)

위젯 클래스를 **멤버로 직접 들고 있지 말 것.** 반드시:
1. `Public/Enum/WidgetType.h` 에 `EWidgetType` 값 추가 (관련 행 근처에).
2. `DT_WidgetClass` (`/Game/CompanyGrowth/Table/UI/DT_WidgetClass`) 에 행 추가: `WidgetType=<enum>`, `WidgetClass=<WBP>`. **매핑은 RowName 아니라 `WidgetType` 필드 기준** (`TableManagerSubsystem::InitializeWidgetTable`).
3. 로드: `TableMgr->GetWidgetClass(EWidgetType::Xxx)` → `CreateWidget`.

```cpp
// ❌ TSubclassOf<UMyWidget> MyClass;  // 직접 저장 금지
// ✅
UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::Xxx);
```

---

## 3. C++ 위젯 규칙

### BindWidget
- `UPROPERTY(meta=(BindWidget))` → WBP에 **동일 이름** 자식 필수(없으면 컴파일 에러). 선택적이면 `BindWidgetOptional`(런타임 null 허용, 항상 null 체크).
- 모든 BindWidget 포인터는 사용 전 null 체크.

### 델리게이트 바인딩은 쌍 (함정)
`NativeConstruct`에서 `Add*` 하면 `NativeDestruct`에서 반드시 해제. 안 하면 재진입 시 누적되어 N번 호출.
```cpp
// NativeConstruct:  Btn->OnClicked().AddUObject(this, &U::H);   // 또는 OnClicked.AddDynamic (UButton)
// NativeDestruct:   Btn->OnClicked().RemoveAll(this);
```
예외(쌍 불필요): 생성자 / BeginPlay·EndPlay / NativeOnActivated·NativeOnDeactivated / CreateWidget 매 신규 인스턴스.

### 실시간 갱신 위젯 — 스냅샷 금지, 매니저 이벤트 구독 (함정)
시시각각 변하는 값(**수익/진행도/회수율/타이머/카운트** 등)을 보여주는 위젯은 `Configure*()`/`Setup*()` 호출 때 **한 번만 읽고 끝내면 안 된다** — 패널이 열려 있는 동안 매니저가 값을 갱신해도 화면은 **얼어붙어(stale)** 재오픈 전까지 옛값을 보여준다.
- **정석 = 매니저 멀티캐스트 델리게이트 구독.** 값을 소유한 매니저(서브시스템)의 `OnXxxChanged` 델리게이트를 구독하고, 브로드캐스트마다 **표시만 다시 채우는 경량 refresh** 를 호출.
  - 구독/해제는 **쌍**(위 규칙): 활성화형 패널이면 `NativeOnActivated`↔`NativeOnDeactivated`, 키별 재바인딩이 필요한 재사용/lazy 위젯이면 `Configure*()` 에서 `RemoveAll(this)` 가드 후 `AddUObject` ↔ `NativeDestruct` 에서 해제 (`feedback_dynamic_widget_rebind`).
  - **매니저는 실제 값이 바뀔 때만 브로드캐스트** (매 Tick 금지). 위젯 핸들러는 **부분 갱신**(전체 rebuild 금지) + **키/타입 필터**(내 대상만): `if (ChangedKey != CurrentKey) return;`.
  - 정적/불변 필드(이름·아이콘 등)는 구독 refresh 에서 제외하고 최초 1회만 채움 — 갱신 함수와 1회 셋업을 분리.
- **NativeTick 폴링은 이벤트 소스가 전혀 없을 때만** 최후수단. 매니저에 적당한 델리게이트가 없으면 **있는 키 필터 OneParam 델리게이트를 새로 추가**하는 게 정석(기존 `FOnCompanyCleared`/`FOnEmployeeStatsChanged` 형태 미러). 이 코드베이스에 데이터 폴링용 NativeTick 선례는 0.

```cpp
// Configure: 정적 1회 + 동적 refresh + 구독(재바인딩 가드)
void U::ConfigureForKey(int32 InKey) {
    Key = InKey;                       // 필터 키
    /* 이름/아이콘 등 불변값 1회 세팅 */
    RefreshFromProgress();             // 동적값 최초 채움
    Mgr->OnXxxChanged.RemoveAll(this); // 중복구독 가드(재사용 위젯)
    Mgr->OnXxxChanged.AddUObject(this, &U::HandleChanged);
}
void U::HandleChanged(int32 ChangedKey) { if (ChangedKey != Key) return; RefreshFromProgress(); }
// NativeDestruct: Mgr->OnXxxChanged.RemoveAll(this);
```
- 선례: `CityCompanyManageWidget`(`OnCompanyProgressChanged` → 1초 드립마다 링/퍼센트/3칩/철거버튼만 `RefreshFromProgress`, 키 필터) · `HQManagePanelWidget`(고빈도 `OnResourceChanged` → Money 만 필터해 `RefreshAffordability`, 전체 `UpdateHQInfo` 금지) · `WorkstationInfoWidget`(`OnEmployeeStatsChanged` → 현재 표시 EmployeeID 만 필터 후 부분 refresh).

### Player / PlayerController 획득 (정석)
```cpp
UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
APlayerController* PC = GI->GetCurrentPlayerController();
if (AMainMapPlayerController* M = Cast<AMainMapPlayerController>(PC)) {
    if (AOfficeCameraPawn* O = Cast<AOfficeCameraPawn>(M->GetPawn())) Player = O;
    else Player = Cast<APlayerCamera>(M->GetPawn());
}
// 위젯에서 PC 가 필요하면 GetOwningPlayer() 도 가능
```

### 좌표 변환 — AbsoluteToLocal 패턴 (3D→위젯 / 위젯→위젯)
`ProjectWorldLocationToScreen` 직접 사용 / `ScreenPos ÷ ViewportScale` **금지**(물리픽셀·SafeZone 미대응).
```cpp
FVector2D VP; UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, WorldPos, VP, true);
FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
FVector2D Abs = ViewportGeo.LocalToAbsolute(VP);
FVector2D CanvasLocal = MyCanvasGeo.AbsoluteToLocal(Abs);   // ✅ SafeZone/DPI 자동보정
```

### 월드 앵커 UI 배치 — 슬롯 필수, 루트 NativePaint 금지 (함정 #10)
위 변환으로 얻은 `CanvasLocal` 은 **반드시 `CanvasPanelSlot->SetPosition()`(레이아웃 공간)으로 배치**할 것.
같은 좌표를 **루트/레이어 위젯의 `NativePaint` 에서 절대좌표 역변환으로 직접 그리면** 레이아웃↔페인트 공간 불일치(조상 렌더 트랜스폼)로 어긋남 — 픽셀 고정 오차라 줌아웃일수록 커져 "줌 의존 버그"처럼 보임 (2026-06-04 선택 셰브론에서 실측·진단).
- **월드 앵커**(건물/액터 추적) → 슬롯 배치 (버블/벽돌 플라이아웃/FloatingNumber/SelectionChevron 전부 이 방식)
- **Slate "절대좌표"는 두 종류** (2026-06-04 탭링 실측·엔진 소스 검증): `OnPaint` 의 `AllottedGeometry` 는 **윈도우 공간**, `GetCachedGeometry()`/`GetTickSpaceGeometry()`/포인터 이벤트는 **OS 스크린 공간**(엔진 용어 "desktop" — 모바일에선 풀스크린이라 둘이 일치, PC/PIE 에선 창 위치+타이틀바만큼 다름). 근거: `SWidget.cpp:1456` `DesktopGeometry = AllottedGeometry + WindowToDesktopTransform`.
- **포인터 기준**(터치/커서 링) → 루트 NativePaint 로 그려도 되지만: ① 좌표 변환은 `GetTickSpaceGeometry().AbsoluteToLocal(스크린좌표)`(스크린 공간 지오메트리), 드로잉만 페인트 `AllottedGeometry.ToPaintGeometry()` — **두 지오메트리의 로컬 공간은 동일**해서 이 조합이 0오차. 페인트 AllottedGeometry 로 AbsoluteToLocal 하면 창 위치만큼 어긋남. ② 좌표 소스는 `GetCursorPos()` 단독 금지 — **모바일 터치는 OS 커서를 안 움직임**(`FAndroidCursor` 갱신은 하드웨어 마우스 이벤트뿐 → 디바이스에서 (0,0) 고정). `FSlateUser::GetPointerPosition(0)` 가 nonzero 면 그 값(터치는 눌린 동안만 등록, release 시 제거), 아니면 `GetCursorPos()` 폴백. `IsTouchPointerActive` 는 `SLATE_SCOPE`(모듈 외부 protected)라 사용 불가. 실례: `InGameLayerWidget::GetPointerAbsolute()`.
- **요지: 변환은 입력 좌표와 같은 공간의 지오메트리로** — 슬롯(레이아웃 단일 시스템)이거나, 페인트 직접 드로잉이면 스크린좌표→`GetTickSpaceGeometry`→로컬→페인트 지오메트리 순.
- 진단법(재사용): ① `DrawDebugBox`(3D, 초록) vs UI(빨강) → 바운드/투영 분리 ② 투영 결과 `Deproject` 역투영 레이가 대상 관통? → 투영행렬/UI매핑 분리 ③ 포인터 이펙트는 화면 구석 클릭으로 어긋남 노출

### Slate 직접 드로잉 (MakeLines) — 텍스처/WBP 0개 벡터 도형
링·호·V 같은 단순 도형은 에셋 없이 `FSlateDrawElement::MakeLines`(폴리라인, 안티앨리어싱+굵기) 로 그리는 게 가장 가볍고 깨끗하다.
```cpp
// NativePaint 안에서 — 원=32각형, V=점3개, 호=각도 구간 분할
FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geo.ToPaintGeometry(), Points,
    ESlateDrawEffect::None, Color, /*AA*/true, Thickness);
```
- 애니메이션(팝/페이드/회전)은 NativeTick 에서 상태 갱신, NativePaint(const)는 읽기만.
- **모양과 위치를 분리**: 도형은 *전용 소형 위젯*의 NativePaint 로 자기 로컬 공간에 그리고(자가 트리 루트 = `USizeBox` 고정 크기), 배치는 슬롯이 담당 → 위 좌표 함정 원천 차단. 실례: `USelectionChevronWidget`(이중 V), InGameLayer 탭링/홀드게이지(포인터 기준이라 레이어 직접 페인트).
- `TArray` 함정: 폴리라인 닫을 때 `Pts.Add(Pts[0])` 금지(재할당 시 dangling assert → 에디터 크래시 실측) — 복사본으로 Add.
- **미터 한계 함정 (2026-06-04 셰브론 꼭짓점 실측)**: AA 굵은 라인은 `FLineBuilder`(`ElementBatcher.cpp`) 규칙 2개를 넘으면 꼭짓점에서 **스트립이 분할**(미터 대신 캡 2개)돼 모양이 깨짐. ① **꺾임각 > 76.5°** (`cos(꺾임각/2) ≥ 0.7854` 조건) — V/화살표 등 뾰족 도형은 그리기 전 `2·atan(반폭/깊이)` 계산해서 한계 안인지 확인. 셰브론 36/28(75.8°)→48/38(76.74°) 비균등 확대로 0.0013 차 탈락 실측 — **비율은 숨은 불변량, 확대 시 정확히 유지**. ② **짧은 세그먼트 + 굵은 선** — 미터가 세그먼트 절반을 넘으면 분할(DPI 스케일 의존)이라 둥근 모서리用 잘게 쪼갠 호도 위험. 우회(팔 분리+겹침/디스크 조인)는 반투명에서 알파 중첩 얼룩(1-(1-a)²)을 만드니 **한 줄 폴리라인 + 각도를 한계 안으로**가 정답.

### 탭 배타선택
`UCommonButtonGroupBase` 사용, 초기화(AddWidget/SelectButtonAtIndex/콜백/ContentSwitcher)는 **전부 NativeConstruct**. `SetIsSelected` 수동 토글 금지.

### 입력 모드 쌍 (push 패널)
`PushPromptClass` → `PC->GoToUIMode()`. 닫을 때 `NativeOnDeactivated`에서 `GetCurrentInputMode()==UI`면 `GoToNormalMode()`. (다음 위젯 연속 오픈이면 UI 유지)

### 문자열
이모지 금지. em-dash/화살표/심볼 등 특수문자 허용하되 **in-game 폰트 글리프 검증**(박스 뜨면 ASCII/UImage 대체). 로마자화는 `Tab`(Tap 아님). 폰트/사이즈/텍스트 스타일 = `UI_TYPOGRAPHY.md` SOT.
- **NEXON 대시 실측 (2026-07-04, ttf cmap 확인)**: 메인폰트 NEXON Lv1 Gothic엔 `—`(em-dash U+2014)·`–`(en-dash U+2013)가 **없음 → 박스(두부)**. 대신 `―`(U+2015 전각 줄표)·`…`·`→`·`★`·`·`(middot)은 **있음**. **한글 문장 대시는 반드시 `―`(U+2015)** — 목업이 CSS라 U+2014를 쓰면 이식 때 U+2015로 치환. NEXON엔 Fallback Font Family도 비어 있어 없는 글리프에 폴백이 안 붙음(박스 확정). 상세 = `UI_TYPOGRAPHY.md` §6.

### UI 안내톤 — 기본 종결은 "~습니다" (사용자 확정 2026-07-21, 비협상)

**시스템이 사용자에게 말하는 모든 문구(빈 상태 / 힌트 / 부제 / 토스트 / 경고)의 기본 레지스터는 정중한 안내체 "~습니다".**

| 구분 | 규칙 | 예 |
|---|---|---|
| 빈 상태 | "~없습니다" / "~습니다" | `"아직 진행 중인 프로젝트가 없습니다"` |
| 힌트·부제 | 서술형 안내 | `"직원을 배치하면 개발이 시작됩니다"` |
| 버튼 라벨 | **기능어**(명사 또는 "~하기") | `"확인"` `"닫기"` `"강화하기"` |

- **금지**: 반말 청유(`"해보자"` `"눌러봐"`), 큐트체, 감탄 남발(`"내일 다시!"` `"다음 주에!"`), 이모티콘성 어미.
- **게임톤 위트는 서사/캐릭터 대사에만** — 미션 멘토 대사, 이벤트 선택지 플레이버, 크리틱 코멘트 등 **발화 주체가 캐릭터인 곳**에 한정. 시스템 안내에는 쓰지 않는다.
- **왜**: 시스템 안내와 캐릭터 발화의 레지스터가 섞이면 "지금 게임이 나에게 사실을 알려주는 건가, 연기하는 건가"가 흐려져 UI 신뢰도가 떨어진다.
- ⚠ **구 문서의 예시 문구를 그대로 복사하지 말 것.** 이 규칙 확정 이전에 작성된 UI 스펙에는 위반 문구가 예시로 남아 있다(대표: `SHOP_GACHA_UI_SPEC.md` §46 의 `"내일 다시!"` / `"다음 주에!"`). 스펙에서 문구를 가져올 때는 이 절의 톤으로 다시 쓸 것.

### 스택 전환 = 닫기 먼저, push 나중

위젯 스택에서 A를 닫고 B를 여는 전환은 **순서가 고정**이다: `DeactivateWidget()`(현재 위젯 닫기) → **그 다음** push용 델리게이트 `Broadcast`.

```cpp
// ✅ 닫기 먼저
DeactivateWidget();
OnRequestOpenNext.Broadcast(NextKey);   // 리스너가 PushPromptClass

// ❌ push 먼저 — 아래 창이 스택에 잔류
OnRequestOpenNext.Broadcast(NextKey);
DeactivateWidget();
```

- **왜**: CommonUI 스택의 `DeactivateWidget()`은 **top 일 때만** 스택에서 빠진다. 먼저 push 해서 top 자리를 뺏긴 뒤 deactivate 하면 그 위젯은 **스택에 잔류**하고, 위에 있던 창을 닫는 순간 **되살아난다**(닫은 줄 알았던 패널이 다시 뜨는 증상).

### 정적 스타일 변경은 WBP, C++ 세터 금지

색/패딩/크기/폰트/브러시 교체 같은 **정적 스타일**은 C++ 세터로 박지 말고 **WBP 에서 처리**한다. Claude가 하는 일 = T3D 트리(또는 원격 Python 스크립트) 제공, 사용자가 하는 일 = 디자이너 paste/실행.

- C++ 브러시·색 조작은 **런타임 데이터 주입**에만 — 등급색 틴트, 진행률, 재화 아이콘 스왑처럼 **값이 데이터에 따라 달라지는 것**. 이때도 개별 필드를 찔러 고치지 말고 **통브러시 교체**(브러시 구조체를 만들어 `SetBrush`)로.
- **왜**: C++ 세터로 스타일을 박으면 ① 디자이너에서 보이는 것과 런타임이 달라 미리보기가 거짓이 되고 ② 같은 값이 WBP와 코드 두 곳에 생겨 어느 쪽이 SOT 인지 잃는다. (`UI_STYLE_CATALOG.md` "브러시는 전부 WBP 쪽에서 지정 — C++ SetBrush 코드 0" 과 동일 원칙.)

### 기타
- C++ 우선. 위젯 외부 API / 매니저 API만 `BlueprintCallable`. 내부 헬퍼/델리게이트 핸들러는 빼기.
- 주석은 WHY만 1줄, 자명한 건 0줄.
- 지역변수에 `Slot`/`Owner`/`Player` 등 상속 멤버명 쓰지 말 것 → `-WarningsAsErrors`로 C4458(셰도잉) 빌드 실패.

---

## 4. WBP 작성 (에디터)

- **★ 기준 해상도 2560×1440 (QHD)** — `DesignScreenSize` ini 설정 + DPI 커브 1440→1.0. 디자이너 Screen Size·모든 치수/폰트 스펙은 이 기준 (상세 치수감은 UI_STYLE_CATALOG §1, 폰트/스타일은 UI_TYPOGRAPHY.md). 1600/1920 감각으로 잡으면 실기에서 작아 보임.
- **텍스트엔 폰트 직접 지정 금지** — CommonTextBlock + `CUI_Style_Text_*` 시맨틱 스케일 지정 (`UI_TYPOGRAPHY.md` §2). 무스타일 방치 = 엔진 Roboto 폴백 = 한글 깨짐.
- **분류 하위폴더에 저장.** C++ 위치 미러: `Source/.../UI/Element/Common/Foo` → `Content/CompanyGrowth/UI/Elements/Common/UIE_Foo`. 패널은 `UI/Panels/<Map>/`. **카테고리 폴더 루트에 던지지 말 것.**
- 부모 클래스 = 해당 C++ 클래스. BindWidget 자식 이름 정확히.
- 화면 추적 오버레이(AddToViewport)면 **루트 CanvasPanel Visibility = `SelfHitTestInvisible`** (빈 영역이 맵 터치 안 먹게). 클릭 받을 자식(버튼)만 `Visible`.
- **⚠ CommonButtonBase 파생 WBP는 루트 `Style` 지정 필수** — Style=None 이면 엔진 기본 버튼 배경(회백 사각)이 위젯 트리 **뒤에** 그려져 커스텀 배경(RoundedBox 글래스 등)과 충돌 (2026-07-09 UIE_EmployeeRosterCard 실측). 자체 배경을 트리로 그리는 카드형은 **`CUI_Style2_Btn_CardClear`**(normal/selected 완전 투명 + pressed 검정 12% — 2026-07-09 신설, 카탈로그 §1 버튼 표) 지정. ⚠ `CUI_Style_Button_Trait_Common` 류는 이름과 달리 **불투명 머티리얼 배경**이라 카드 히트용 아님 — 스타일 재사용 전 normal_base 브러시(draw/tintA/resource)를 **리드백으로 실측**할 것. **WBP 생성 자동화 스크립트에 CDO `style` 세팅을 포함**할 것 — T3D paste 는 인스턴스 Style 오버라이드를 silent-drop 하므로 paste 후 python 적용이 정석.
- **⚠ T3D 트리 파일에서 임베드 유저위젯(WBP_C 인스턴스)은 `Content=` 참조만으로 부족** — 반드시 `Begin Object Class=/Game/.../UIE_Foo.UIE_Foo_C Name="..."` **인스턴스 선언 블록**이 파일 안에 있어야 함. 없으면 paste 가 해당 위젯을 통째로 드롭(에러 없이) → required BindWidget "not found" 컴파일 실패 (2026-07-09 StatRow0~7 실측). 트리 파일 작성 후 `Content="/Game/` 참조 목록과 `Begin Object Class=/Game/` 선언 목록을 **대조 검증**할 것.
  - **⚠⚠ 선언 블록이 있어도 드롭될 수 있다 (2026-07-20 UIE_YieldRangeBar 실측)**: 같은 세션에서 Python 으로 갓 생성한 WBP(빈 트리)는 선언 블록·에셋 모두 정상인데도 paste 가 **위젯만 드롭하고 슬롯은 남겼다**(`get_all_children()` 에 `None` 이 섞여 나옴 = 이 증상의 지문). 부모 컴파일은 통과하므로(BindWidgetOptional) **조용히 없는 채로 넘어간다.** ⇒ **paste 후 검증 필수**: `load_object(None, "...Asset.Asset_C:WidgetTree.<이름>")` 전수 확인. 누락 시 재-paste 말고 **원격 Python 주입**이 빠름 — 빈 슬롯 `remove_child_at(i)` → `new_object(WBP_C, tree, "정확한이름")` → 뒤 형제 `remove_child`/`add_child` 로 순서 복원(중간 삽입 API 없음) → 슬롯 패딩 재적용 → `compile_blueprint` → `save_asset(only_if_is_dirty=False)`.
- 스케일/회전 애니 거는 위젯은 **Render Transform Pivot (0.5,0.5)**.
- 위치/회전/크기를 코드가 매 프레임 세팅하는 설계면 WBP엔 위젯만 놓고 좌표는 비워둬도 됨.

---

## 4.5 화면 비율 대응 — 16:10 ~ 19.5:9 (사용자 지시 2026-07-28, 비협상)

**모든 UI는 16:10(태블릿) 부터 19.5:9(긴 폰) 까지를 한 트리로 커버해야 한다.** 특정 비율에 맞춘 치수 박기 금지.
(4:3 은 사실상 사라진 비율이라 **고려 대상 아님** — 사용자 확인 2026-07-28.)

### 이 프로젝트에서 그게 뜻하는 것 (설정에서 도출한 숫자)

`DefaultEngine.ini` 에 `UIScaleRule` 이 없으므로 엔진 기본값 **`ShortestSide`**, 커브는 1440→1.0.
가로 게임에서 짧은 변은 세로이므로 **스케일 입력은 화면 높이**다. 결과:

| | 가상 캔버스 |
|---|---|
| 세로 | **항상 ≈1440 고정** (비율 무관) |
| 가로 | **2304(16:10) ~ 3120(19.5:9)** — 디자인 기준 2560(16:9)은 이 구간의 **중간** |

**하한을 정하는 실기기 = 갤럭시 탭 S8** (2560×1600, 16:10). `ShortestSide` 라 짧은 변 1600 이 스케일 입력 →
커브 1440→1.0 / 2160→1.5 보간으로 스케일 ≈1.11 → **가상 2304×1440**.
이 한 대가 통과하면 더 긴 폰들은 가로가 남으므로 자동 통과한다. **검증 기기를 하나만 고른다면 이것.**

⇒ ⚠⚠ **디자인 기준 2560 은 최악의 경우가 아니다.** 16:10 태블릿에서는 가로가 **2304 로 256px 더 좁다.**
   풀스크린 요소를 2560 에 꽉 채워 설계하면 **태블릿에서 256px 가 화면 밖으로 나간다.**
   ⇒ **가로 안전 예산 = 2304.** 2560 은 "여기까지 늘어날 수 있다"는 상한이지 설계 기준선이 아니다.
⇒ **세로는 어떤 기기에서도 1440** — 여유가 늘지 않는 유일한 축이고, 넘치면 어느 기기에서나 똑같이 깨진다.
⇒ 정리하면 **가로는 2304 안에 들어가야 하고, 세로는 1440 안에 들어가야 한다.** 둘 다 여유가 없다.

### 규칙

1. **풀스크린 레이어·HUD·도크는 앵커 기반 배치.** 가로가 최대 560px 늘어나므로 절대좌표·고정폭·"2560을 다 쓴다"는 가정 금지. 좌/우 가장자리에 붙을 것은 각 가장자리에 앵커하고, 중앙 정렬은 중앙 앵커로.
2. **중앙 모달은 폭 고정 대신 하한(`MinDesiredWidth`).** 최소 가상폭 2560 안에서 여유는 충분하지만, 고정하면 콘텐츠가 넘칠 때 갈 곳이 없다.
3. **세로 예산을 먼저 검산한다.** 1440 에서 프레임 여백·헤더·CTA 를 뺀 값이 콘텐츠 예산이다. 세로가 모자라면 비율 대응 이전에 설계가 틀린 것이다.
4. **⚠⚠ "안쪽이 못 줄고 바깥이 못 느는" 조합 금지 (2026-07-28 실측 사고).**
   고정 자체가 나쁜 게 아니라 이 조합이 겹침을 만든다. 컨테이너에 `WidthOverride`/`HeightOverride` 를 걸었는데
   그 안 자식이 `MinDesiredWidth`/`MinDesiredHeight` 로 최소 크기를 주장하면, 합이 안 맞는 순간
   **Slate 는 기본 클리핑이 없어 넘친 만큼 이웃을 침범**한다(줄지도 늘지도 못하므로).
   - 실사고: `Col3Box` `WidthOverride=260` − 패딩 48 = 212 사용가능인데, 안의 행이 라벨 100 + 값 `MinDesiredWidth=120` = 220 → 8px 초과 → 겹침.
   - 판정법: **컨테이너 고정폭 − 패딩 ≥ 자식 최소폭 합** 인지 손으로 계산할 것. 안 맞으면 컨테이너를 `MinDesired*` 로.
   - 고정을 유지해도 되는 경우 = **안에 최소 크기를 주장하는 콘텐츠가 없을 때** (이미지 비율 박스, 글자 하나짜리 배지, 행 높이 규격 등).
5. **검증은 두 극단에서.** 디자이너 Screen Size 를 **2304×1440(16:10 = 갤탭 S8)** 과 **3120×1440(19.5:9)** 로 번갈아 놓고 확인. 기본값 2560 만 보고 끝내면 **가로가 가장 좁은 경우를 통째로 건너뛴다.**
6. **⚠ `AutoWrapText` 는 고정폭 컨테이너 안에서만 동작한다 (2026-07-28 실측).**
   위 2번을 따라 컨테이너를 `MinDesiredWidth` 로 바꾸면 **그 안의 `AutoWrapText` 텍스트가 영영 안 감긴다.**
   `AutoWrapText` 는 직전 프레임의 할당 폭을 기준으로 감는데, 폭을 **제한하지 않는**(하한만 주는) 컨테이너에서는
   텍스트의 desired 폭 = 안 감긴 한 줄 폭 → 컨테이너가 그만큼 넓어짐 → 할당 폭도 넓음 → 감길 이유가 없음, 이 되어 되먹임이 성립하지 않는다.
   ⇒ **`MinDesired*` 컨테이너 안의 긴 텍스트는 `WrapTextAt=<폭>` 으로 감을 지점을 명시할 것.**
   (`WidthOverride` 컨테이너에서는 `AutoWrapText` 가 정상 동작한다 — 폭이 실제로 제한되므로.)
7. **⚠⚠ 크기 고정은 최후수단 — 기본은 `MinDesired*` (사용자 지시 2026-07-29, 비협상).**
   `SizeBox` 를 놓기 전에 반드시 물을 것: **"이건 정말 이 크기여야 하나, 아니면 최소 크기인가?"**
   - `WidthOverride`/`HeightOverride` = **desired 를 그 값에 못박는다.** 콘텐츠가 커져도 안 늘어(자식이 밖으로 삐져
     이웃 침범) 부모가 좁아져도 줄었다고 알리지 못한다 — **양방향 실패**다. "최소폭 역할" 을 의도했다면 잘못된 선언.
   - `MinDesiredWidth`/`MinDesiredHeight` = `desired = max(기준, 콘텐츠)`. 같은 하한을 보장하면서 위로 열려 있다.
   - **고정이 정당한 경우만** 고정: ① 이미지/썸네일 비율 박스 ② 자체 desired 크기가 없는 위젯의 **그 축만**
     (`UProgressBar` 는 높이가 0 — `UI_STYLE_CATALOG.md` §4-A) ③ 글자 하나짜리 배지·행 높이 규격.
     이 경우에도 **필요한 축만** 고정하고 나머지 축은 열어둘 것. 고정을 남기면 **왜 고정인지 1줄** 남긴다.
   - ⚠ 전환할 때 `clear_*()` 만 호출하면 **플래그만 꺼지고 값 줄이 죽은 값으로 남아** 디테일 패널에 계속 숫자로 보인다
     (다음 사람이 "고정돼 있네" 로 오진). **`set_*(0)` → `clear_*()`** 로 T3D 줄 자체를 지울 것 (아래 §T3D 「`bOverride_*` 짝 줄」과 같은 뿌리).
   - 실사고 2026-07-29: 오피스 스트립 `Stage{n}WellBox.WidthOverride=170` 탓에 값 문자열이 길어졌을 때
     바가 못 늘어나고 **글자만 바 밖으로 나갔다** → `MinDesiredWidth=170` 으로 전환. 4번(겹침)과 한 뿌리다.

---

## 4.6 물리 크기 / 세이프에어리어 — §4.5 가 못 잡는 축 (2026-08-06 실측)

⚠⚠ **§4.5 의 검증(2304×1440 / 3120×1440)은 가로 오버플로만 잡는다.** 둘 다 세로가 1440 이라
**터치타겟·폰트의 물리 크기 문제는 구조적으로 안 보인다.** 기준 기기인 갤탭은 "가로 최악"이면서
동시에 **"물리 크기 최선"** 이다. 이 축은 mm 로 따로 검산해야 한다.

### 왜 폰이 최악인가

`UIScaleRule` 미지정 = `ShortestSide`, 커브 `1080→0.75 / 1440→1.0`. 가로 게임에서 짧은 변 = 화면 높이
⇒ **가상 세로는 어떤 기기든 1440 고정.** 11인치 태블릿과 6.7인치 폰이 같은 양의 UI 를 같은 가상
좌표로 그리므로, **폰에서는 전부 물리적으로 축소된다** (같은 위젯이 태블릿 9.1mm / S24+ 4.4mm).

| 환산 상수 | 값 |
|---|---|
| **폰(S24+ 6.7")** | **1 virtual px ≈ 0.0495 mm** (가상 1440 ↔ 71.3mm) |
| 태블릿(11") | 1 virtual px ≈ 0.103 mm (가상 1440 ↔ 148mm) |
| 폰트 실제 em | **`Size × 1.333`** (Slate 는 96 DPI 래스터 — `SlateFontInfo.h` `RenderDPI=96`) |
| 폰 sp 환산 | **`Size × 0.355 sp`** ⇒ 12sp 하한 = **`FontSize ≥ 34`** |

### 규칙

1. **터치타겟 하한 — 가이드라인 실측 환산 (S24+ 기준)**

   | 기준 | 최소 규격 | 물리 | virtual |
   |---|---|---|---|
   | Apple HIG | 44×44 pt | ≈7.3mm | **147** |
   | Material 3 | 48×48 dp | ≈7.6mm | **154** |
   | 손끝 인체공학(참고) | — | 9~10mm | 182~202 |

   ⇒ **실무 하한 = 144~154.** 상시 노출 버튼(뒤로/탭/CTA)은 이 대역을 지킬 것.
   88 같은 값은 **폰에서 24dp** 로 권장의 절반이다.
   ⚠ 화면 **모서리**에 놓이는 버튼(가로모드 상단 좌우)은 그립상 가장 닿기 어려운 구역이므로 대역 상단을 쓸 것.
   ※ 이 프로젝트의 기존 `MinHeight` 분포(트리 101개)는 `80(15) / 150(12) / 88(11) / 72(11) / 60(8) / 130(7)` —
   **130~150 클러스터가 이미 존재**하므로 144 는 outlier 가 아니다. 손봐야 할 집단은 60~88 쪽.
2. **텍스트 하한 = `FontSize 34` (≈12sp).** 부제/캡션은 28(10sp)까지 허용, 그 이하는 금지.
3. **밴드/스트립 높이는 안에 들어갈 최대 터치타겟 + 여백**으로 역산할 것. 먼저 높이를 정하고
   버튼을 우겨넣으면 반드시 작아진다 (오피스 밴드 140 안에 88 버튼이 그렇게 생겼다).

### ⚠⚠ SafeZone 은 둥근 모서리를 안 막는다

`FDisplayMetrics::RebuildDisplayMetrics`(`AndroidApplication.cpp:155`) → `FAndroidWindow::GetSafezone()`
→ `GAndroidLandscapeSafezone`(초기값 `(-1,-1,-1,-1)`). Java 쪽은 **`DisplayCutout` 만 처리하고
`RoundedCorner` API(Android 12+) 를 안 부른다.** ⇒ **모서리 라운딩 인셋 = 엔진이 0 으로 본다.**

이 프로젝트는 `bUseDisplayCutout=True`(엣지-투-엣지) + `Orientation=SensorLandscape`(펀치홀이
좌/우 어느 변에나) 라 증상이 양쪽 코너에 다 나온다.

⇒ **화면 가장자리에 붙는 UI 는 SafeZone + 명시적 코너 패딩 72 를 둘 다 건다.**
`SSafeZone::GetSafeMargin` = `Padding + (SafeMargin × SafeAreaScale)`(`SSafeZone.cpp:173`) 이라
**슬롯 패딩은 기기 인셋에 합산**된다(대체 아님). 72 는 반경 추정(r≈130px) 기반 설계값 —
정확한 반경은 UE 로 조회 불가이므로 **실기기 확인 후 조정**할 것.

### ⚠ SafeZone 을 어디에 거는가 — 캔버스 통째로 감싸지 말 것

`UI_InGameLayer` 는 `Overlay(Padding 50) → SafeZone → InGameCanvas` 로 캔버스를 통째로 감싸지만,
**월드 추적 위젯의 부모 캔버스에는 이 패턴을 쓰지 말 것.** 캔버스가 줄면 화면 밖 판정
(`OfficeLayerWidget.cpp:899` `CanvasSize ± OffscreenMargin`)이 같이 줄어 **가장자리 버블/링이
일찍 사라진다.** 배경 이미지도 인셋되면 둥근 모서리와 틈이 생겨 더 깨져 보인다.

⇒ **배경은 풀블리드 유지, SafeZone 은 인터랙티브 행(Row)만 감쌀 것.** 실적용 예 =
`UI_OfficeLayer.OfficeTopBand` (배경 3장 Fill + `BandSafeZone → BandRow`).
상세 = `docs/superpowers/specs/2026-08-06-office-band-mobile-safearea-design.md`.

⚠ Python 으로 삽입할 때 **재정렬 API 가 없다**(`shift_child`/`insert_child_at` 부재).
감쌀 대상이 **마지막 슬롯**이면 `remove_child` → `SafeZone.add_child` → `parent.add_child(SafeZone)`
(append) 로 z-order 가 보존된다. 마지막이 아니면 T3D 붙여넣기로 갈 것.

---

## 5. 텍스처 / 에셋

- `T_` 접두사 + 분류 폴더(`UI/Textures/<Cat>/`). AI 이미지 임포트는 `reference_image_to_ue_import_workflow`.
- **UI 텍스처 설정**: `lod_group=TEXTUREGROUP_UI`, `mip_gen_settings=TMGS_NO_MIPMAPS`, **`compression_settings=TC_EDITOR_ICON`(=UserInterface2D, alpha 보존)**. UI 표시 144~200px면 Lanczos 사전 리사이즈(`feedback_ui_image_resize`).
- **⚠ UE5.4 PNG/텍스처 임포트 = Interchange 우회 필수**: 이 프로젝트는 `/Interchange/Pipelines/DefaultTexturePipeline` 부재라 기본 `AssetImportTask` 임포트가 **"There was no data to import"로 전멸**(0/N). 해결 = task 에 **`factory=unreal.TextureFactory()` 강제**(legacy 임포트). 심플 UI 아이콘은 손SVG→`@resvg/resvg-js`→PNG — 도구/상세 = 메모리 `reference_svg_icon_pipeline`(`svg_icon_render.js`,`svg_icon_import.py`).
- 하드참조(`UPROPERTY UTexture2D*`/`ConstructorHelpers`)는 자동 쿠킹. 소프트참조(`TSoftObjectPtr`/문자열경로)는 Project Settings → Additional Asset Directories to Cook 등록 필수.
- DT CSV의 에셋 참조 칸 빈칸 금지(None으로 덮임).

---

## 6. Python 자동화 범위 (UE5.4 stock, 검증됨)

> 실행: Output Log 하단 Cmd 드롭다운=`Python` → `exec(open(r"C:\tmp\xxx.py", encoding="utf-8").read())`. 임시 스크립트는 `C:\tmp`, 실행 후 삭제. Tools 메뉴 안내 금지.

> **★ 자동화 우선 원칙 (2026-07-01, 사용자 지시):** stock Python으로 **가능한 셋업은 사용자에게 클릭시키지 말고 Claude가 스크립트로 직접 처리**한다. 자동화 대상 = WBP **에셋 생성(부모 C++ 지정)** · CDO 기본값 · **DataTable 행 등록**(export→append→fill, **백업+행수검증** 필수) · 텍스처/에셋 임포트+설정 · 에셋 이동/리네임. 사용자에게 남기는 건 **stock Python이 못 만드는 위젯 트리(BindWidget 계층) 붙여넣기**뿐 — 그것도 필요하면 **에디터-C++ 트리빌더(ConstructWidget) 또는 `FWidgetBlueprintEditorUtils::ImportWidgetsFromText` 래퍼**로 자동화 제안. "생성만 해두면 되는 것"을 수동 절차로 넘기지 말 것. (에디터 열려 있으면 콘솔 1줄 exec, 닫혀 있으면 `-run=pythonscript` 헤드리스.)

| 작업 | 가능? | 방법 |
|---|---|---|
| 텍스처 임포트+UI설정 | ✅ | `AssetImportTask`(destination_path=가상경로 `/Game/...`, save=False) → 프로퍼티 set → `EditorAssetLibrary.save_loaded_asset` |
| WBP **에셋 생성** + 부모 지정 + CDO 기본값 | ✅ | `WidgetBlueprintFactory.parent_class` → `create_asset(..., WidgetBlueprint, factory)` → `compile_blueprint` → `get_default_object` set props |
| WBP **위젯 트리** — **신규 트리 작성** (2026-08-12 검증: `UIE_GuideTooltip` 을 사용자 클릭 0 으로 저작) | ✅ | 「루트를 못 만든다」가 유일한 벽이었고 **팩토리에 루트를 만들게 시키면 뚫린다**. `WidgetTree.RootWidget` 은 여전히 protected(직접 set/get 둘 다 거부) — 대신 **생성 시점에 프로젝트 설정을 잠깐 바꾼다**: `s = unreal.get_default_object(unreal.load_object(None, "/Script/UMGEditor.UMGEditorProjectSettings"))` → `s.set_editor_property("DefaultRootWidget", unreal.CanvasPanel)` → `create_asset(...)`(팩토리가 `CanvasPanel_0` 을 루트로 생성) → **원복**(`SaveConfig` 안 부르면 ini 무변경). ⚠ `unreal.UMGEditorProjectSettings` **파이썬 심볼은 없다** — 반드시 `load_object` 로 클래스 경로를 로드할 것. 이후는 다음 행(기존 트리 수정)과 동일: `new_object` → `add_child_to_canvas/overlay/vertical_box` → 슬롯·브러시·폰트 세팅 → `compile_blueprint` → `_C` 리드백 → `save`. ⚠ **같은 세션에서 `delete_asset` → `create_asset` 은 실패한다**(패키지가 메모리에 남아 create 가 `None` 반환 → 루트 없는 껍데기로 오진하기 딱 좋다) — 다시 만들 거면 **프로세스 밖에서 .uasset 파일을 지우고** 새 프로세스로 생성할 것. 디자이너 손이 꼭 필요할 때만 T3D paste |
| WBP 위젯 트리 — **기존 트리에 위젯 추가/재배열** (2026-07-03 원격Python 검증, 2026-07-06 로딩화면 트리 통째 스왑으로 재검증) | ✅ | `widget_tree` 프로퍼티 미노출을 **서브오브젝트 경로 load_object 로 우회**: `unreal.load_object(None, "/Game/...WBP명.WBP명:WidgetTree.위젯이름")` 이 트리/개별 위젯 전부 로드됨. **⚠⚠ 경로가 두 개인데 하나만 쓸 수 있다 (2026-08-07 실측, 한 사이클 날림)**: `Asset.Asset:WidgetTree` = **저작 트리(쓰기 대상)** / `Asset.Asset_C:WidgetTree` = 컴파일 산출물(읽기 전용 복제). `_C` 쪽에 쓰면 **`set_editor_property`·`new_object`·`add_child` 전부 성공을 반환하지만 다음 `compile_blueprint` 이 통째로 되돌린다**(에러 0, 리드백에서 갑자기 사라져 "paste 가 드롭했다"로 오진하기 쉽다). **편집=접미사 없는 경로, 검증=`_C` 경로** 로 고정할 것. ⚠ `SlateBrush.ImageSize`는 `DeprecateSlateVector2D`라 `unreal.Vector2D` 직접 대입 불가. **`set_desired_size_override` 우회는 저작 아키타입에 직렬화 안 됨 (2026-07-20 HQCeleb 실측 — 저장 후 32×32 복귀, 구 2026-07-06 처방 폐기)** → 정답 = `unreal.DeprecateSlateVector2D()` 빈 생성 → `x`/`y` set_editor_property → 브러시에 `image_size` set → **`img.set_editor_property('brush', b)` 재대입**(수정 마킹) → 컴파일/저장 후 T3D 익스포트로 ImageSize 직렬화 리드백 검증. CanvasPanelSlot 은 `layout_data`(unreal.AnchorData(offsets=Margin, anchors=Anchors, alignment=Vector2D))+`auto_size` 로 앵커/오프셋 세팅 가능. 새 위젯 = `unreal.new_object(변형WBP_C, tree, "정확한BindWidget이름")` → `set_editor_property`(**EditAnywhere 프로퍼티는 protected 여도 읽기/쓰기 됨** — `RootWidget`/`bIsVariable`/`Status` 같은 비-Edit 프로퍼티만 차단) → 부모 패널 `add_child`/`remove_child`(중간 삽입 API 없음 → 뒤 형제 remove 후 재-add 로 순서 제어) + 반환 슬롯 `set_padding`/`set_horizontal_alignment` → `unreal.BlueprintEditorLibrary.compile_blueprint(bp)` → **검증 = `load_object("..._C:WidgetTree.새이름")`** (BPGC 트리 복제본 존재 = 컴파일 통과 증거, Status 못 읽는 것 대체) → `save_asset`. `bIsVariable` 세팅 불가여도 C++ BindWidget(Optional) 이름 매칭은 컴파일러가 자동 바인딩(UnseatButton 실측). 실행 전 .uasset 파일 백업, 검증 실패 시 저장 금지. 실전 예: `UIE_WorkstationOccupantSummary` UnseatButton 추가 |
| WBP 위젯 트리 — **에디터 전용 C++ 트리 빌더** (2026-06-05 검증) | ✅ | `#if WITH_EDITOR` + `UBlueprintFunctionLibrary`의 BlueprintCallable static 함수 → Python에서 `unreal.XxxTreeBuilder.build_all()` 호출. 본체: `WidgetTree->ConstructWidget<T>(cls, FName정확한이름)` 슬롯 배치 → `MarkBlueprintAsStructurallyModified` → `FKismetEditorUtilities::CompileBlueprint` → `UEditorLoadingAndSavingUtils::SavePackages`. Build.cs `if (Target.bBuildEditor) { UnrealEd, UMGEditor }`. **실전 예: `Private/Editor/BuildModalTreeBuilder.cpp`**. 검증 사실 2건: ① 필수 BindWidget 검증은 컴파일 중인 WBP가 **상속한 C++ 베이스**에만 적용 — 자식으로 배치한 인스턴스 클래스 내부의 필수 BindWidget(예: UButtonWidget.ButtonText) 미충족은 부모 컴파일을 안 깸 ② BindWidget 타입이 CommonButtonBase 파생이면 UUserWidget 합성 변형(UTabButtonWidget 계열)은 타입 불일치 — 베이스를 직접 ConstructWidget. protected 프로퍼티(ListView EntryWidgetClass/Orientation)는 `FindFProperty`+`SetPropertyValue_InContainer`. 재실행=트리 갈아엎음(수동 수정 덮어씀) 주의. **재실행 함정(실측)**: `RootWidget=nullptr`+`GetAllWidgets`만으론 outered 잔존 위젯이 안 잡혀 같은 이름 ConstructWidget이 충돌 → 새 트리가 컴파일러에 안 보임(BindWidget 전원 not-found, uasset엔 이름 존재). `GetObjectsWithOuter(Tree,...,true)`로 고아 포함 수집 후 `Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors\|REN_NonTransactional)` 추방 필수. 컴파일은 `FCompilerResultsLog` 오버로드로 `NumErrors`/`Status==BS_Error` 검사(안 하면 실패 모른 채 "완료" 출력) + 트리 구성 직후 `FindWidget` 전수 자가검증. CommonUI 스타일(`CUI_Style_*_C`)은 LoadClass 후 **`IsChildOf(스타일 베이스)` 가드 + `SetStyle(TSubclassOf<...>(cls))`** — 가드 없으면 타입 불일치 시 빈 배경. 텍스트 버튼은 C++ 클래스 직접 인스턴스 금지(내부 트리 없음→글자 안 보임), WBP 변형(`UI_Element_Button_C`)으로 ConstructWidget. **UCommonTextBlock에 Style 지정 시 SynchronizeProperties→UpdateFromStyle이 SetFont/SetColor를 스타일 값으로 무조건 되덮음**(CommonTextBlock.cpp:419·486) — 코드로 명시 폰트/색을 박을 땐 Style 지정 금지. **T3D는 diff-from-CDO 직렬화** — 인스턴스 프로퍼티가 부모 BP CDO 기본값과 같으면 줄 자체가 안 생김(세팅 실패로 오인 말 것, ReadBack 검증으로 판별) |
| DataTable 행 추가/수정 | ⚠️ | 직접 add-row 없음. `export_data_table_to_json_string` → 병합 → `fill_data_table_from_json_string`(**파괴적: 전체 비우고 채움**). 반드시 백업 + 행수 검증. enum은 **식별자**로 직렬화(UMETA DisplayName 아님), TSubclassOf는 `...WBP_Name_C` 풀패스 |
| 에셋 이동/리네임 | ✅ | `EditorAssetLibrary.rename_asset(old,new)`(참조 갱신). **raw 파일 이동 금지** |

리비전 컨트롤 켜져 있으면 "Unable to Check Out" 팝업 떠도 `Moving .tmp -> .uasset` 로그 있으면 저장은 됨.

⚠ **헤드리스 커맨드릿의 저장은 "성공"을 반환해도 디스크에 안 써질 수 있다 (2026-08-12 실측)** — 작업 도중 **다른 세션이 GUI 에디터를 띄우면** 그 에디터가 uasset 을 잡아 커맨드릿 저장이 `MoveFile ... (Error Code 32)` 로 무한 재시도하다 포기한다. `save_loaded_asset` 은 결과를 안 알려주므로 **파이썬 로그만 보면 저장된 줄 안다.** ⇒ ① 판정은 **디스크 mtime** 으로(스크립트 밖에서 `ls`), ② 에디터가 떠 있으면 커맨드릿 말고 **원격 실행**(`Tools/RemoteExec/remote_run.py`)으로 그 에디터 안에서 저장할 것 — 저장 주체가 잠금 소유자면 충돌이 없다. 시작 전 `Get-Process UnrealEditor` 는 필수지만 **작업 중에 켜질 수도 있다**는 게 이번 교훈.

### 6.1 Python API 이름/시그니처 함정 (2026-08-05 실측)

전부 **에러 메시지가 원인을 안 알려주는** 부류라 한 번씩 시간을 잡아먹는다.

| 함정 | 실측 |
|---|---|
| **`unreal.KismetRenderingLibrary` 는 없다** | UE Python 이 `Kismet` 접두사를 벗긴다 → **`unreal.RenderingLibrary`** (`draw_material_to_render_target` / `clear_render_target2d` / `export_render_target` 전부 여기). 이름을 모를 땐 `[n for n in dir(unreal) if ...]` 로 먼저 찾을 것 |
| **프로젝트 C++ 클래스는 snake_case 별칭이 없다** | `da.get_editor_property("shell_map")` → `Failed to find property`. **`"ShellMap"`(C++ 원래 이름)은 성공.** 엔진 클래스(`lod_group`, `blend_mode`)는 snake 가 먹어서 더 헷갈린다 ⇒ **`get/set_editor_property` 에는 항상 C++ 프로퍼티 이름 그대로** |
| **`export_render_target` 은 확장자를 무시한다** | 렌더 타깃이 float 포맷이면 `.png` 로 저장해도 내용은 **EXR**(매직 `v/1\x01`) → PIL 이 못 연다. 먼저 `rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)` |
| **`unreal.CustomInput()` 는 키워드 생성자를 안 받는다** | `CustomInput(input_name=...)` → `call() takes at most 0 arguments`. 빈 생성 후 `set_editor_property("input_name", n)` |
| **에디터 기동 직후 `load_asset` 이 None 을 준다** | 레지스트리 스캔 중이면 새 에셋에 None. `AssetRegistry.is_loading_assets()` 로 대기하거나 **`unreal.load_object(None, "패키지.에셋")` 로 폴백**. 이걸 모르면 "에셋이 없다"로 오진한다 |

⚠ **`try/except` 로 감싼 프로브가 이 오진을 증폭한다** — `None.get_editor_property()` 의 `AttributeError` 까지 같이 삼켜서 "프로퍼티가 없다"로 보고한다. 프로브는 **객체가 None 인지 먼저 찍고** 나서 프로퍼티를 볼 것.

⚠ **원격 실행은 출력이 길면 응답 자체가 깨진다 (2026-08-12 실측)** — 로그 줄이 많으면 `remote_execution` 의 JSON 이 중간에서 잘려
`Failed to deserialize JSON ... Unterminated string` → `RuntimeError: Remote party failed to send a valid response!` 로 죽는다(스크립트는 이미 다 돌아간 뒤라 **작업은 됐는데 결과만 유실**).
⇒ **원격 스크립트는 결과를 `C:\tmp\out.txt` 에 쓰고**(로컬에서 파일을 읽는다) 로그로는 "done" 한 줄만 남길 것. 프로브류처럼 출력이 많은 스크립트일수록 필수.

⚠ **원격 실행(`remote_run.py`)은 예외를 안 보여준다** — `SUCCESS: False` 만 뜬다. 스크립트 본문을 `try/except: traceback.format_exc()` + `unreal.log_error` 로 감싸지 않으면 원인을 영영 못 본다.

---

## 7. 빌드 & 검증

- 빌드: `UnrealBuildTool.exe CompanyGrowthRenewalEditor Win64 Development -Project=...` (CLAUDE.md Build Commands). **에디터 켜져 있으면 Live Coding 잠금으로 실패(exit 6) → 에디터 닫고 풀 빌드.**
- 새 UCLASS / 헤더 멤버 레이아웃 변경(UPROPERTY 추가·제거)은 Ctrl+Alt+F11(Live Coding) 불안정 → **풀 빌드**.
- 빌드 후 에디터 자동 실행 금지(사용자가 켬).
- PIE: BindWidget 경고 사라졌는지, 기능 동작, 빈 화면 터치가 맵 입력 안 막는지 확인.

**⚠ 「빌드 실패」와 「내 코드 실패」는 다르다 (2026-08-05 실측)**: 동시 세션 환경에서는 남의 미완성 파일 하나가 전체를 exit 6 으로 끝낸다. 로그의 빨간 줄만 보고 내 작업을 의심하지 말 것.
- **판정 = obj 타임스탬프**: `Intermediate/Build/Win64/x64/UnrealEditor/Development/<Module>/<파일>.cpp.obj` 가 소스보다 **새로우면 컴파일 통과**. obj 자체가 없으면 그 파일이 실패한 것.
- **UPROPERTY 변경 반영 판정 = UHT 생성물**: `Intermediate/Build/.../UHT/<Class>.gen.cpp` 에 **새 프로퍼티 이름이 있고 제거한 이름이 없는지**. 리플렉션 코드는 헤더에서 기계 생성이라 거짓말을 못 한다.
- **`Target is up to date` 는 내 코드 검증이 아니다** — 다른 세션이 이미 빌드를 끝냈다는 뜻일 뿐. 위 두 지표로 확인할 것.
- 남의 미완성 파일 때문에 막혔으면 **고치지 말고**, 그 세션의 `docs/superpowers/plans/` 를 먼저 찾아볼 것 — 대개 구현이 그대로 적혀 있어 추측이 필요 없고, 이미 끝나 있는 경우도 있다(빌드가 중간 스냅샷을 잡았을 뿐).

---

## 8. 함정 모음 (이번에 실제로 밟은 것들)

1. **카테고리 폴더 루트에 에셋 던짐** → 분류 하위폴더에. (`feedback_asset_categorized_folder`)
2. **죽은 시스템에 붙임** → push/호출 사이트 grep으로 라이브 확인.
3. **`ProjectWorldLocationToScreen` 직접 + ÷ViewportScale** → AbsoluteToLocal.
4. **델리게이트 누적**(NativeConstruct만 Add) → Destruct에서 RemoveAll.
5. **C4458 셰도잉**(지역변수 `Slot`) → `-WarningsAsErrors`로 빌드 실패. 구체 이름.
6. **버튼+이미지 과분할** → `UButton` 스타일 브러시 자체를 그래픽으로(`SetStyle`/`GetStyle`), 스왑·트랜스폼은 버튼에 직접.
7. **Python으로 위젯트리 만들려다 헛수고** → stock Python은 `UWidgetBlueprint`의 `widget_tree`/`tick_frequency`/`generated_class` 미노출(검증 2026-06-03 헤드리스 로그). 디자이너 수동, **또는 무인 자동화가 목표면 `BindWidget` 대신 C++ `RebuildWidget` 자가 트리**(설계 노트 참조).
8. **DT JSON enum을 DisplayName으로** → None fallback. 식별자 사용.
9. **고정 px 레이아웃**(건물마다 크기 다름) → 바운드/월드투영 기반 동적 배치.
10. **월드 앵커 좌표를 루트 NativePaint 로 직접 드로잉** → 레이아웃↔페인트 공간 불일치로 줌 의존처럼 보이는 어긋남 (§3 "월드 앵커 UI 배치"). 슬롯 배치로. **기존 검증 시스템(버블) 참고 시 경로 전체를 복제할 것 — 앵커만 베끼고 배치 방식을 바꾸면 검증 무효.**

---

## 설계 노트 — 패턴 선택 기준

- **동종 세트(방향만 다른 버튼 N개 등)**: WBP에 N개 까는 것도 표준이지만, 위치/스타일을 코드가 제어하면 **C++ 런타임 생성**이 더 정석(DRY). WBP엔 컨테이너(`RootCanvas`) 1개만 BindWidget, 버튼은 `WidgetTree->ConstructWidget<UButton>` + `AddChildToCanvas` 루프로. 핸들러도 1세트. 디자이너에서 개별 버튼을 손봐야 하면 반대로 WBP 배치 또는 변형 컴포넌트. (유일한 실례였던 PlacementArrows가 2026-07-21 폐기되어 현재 이 하이브리드 방식을 쓰는 위젯은 없다 — 런타임 생성이 필요하면 아래 자가 트리 쪽이 실례가 더 많다.)
- **루트 컨테이너는 본질적으로 1개 필요**: 절대좌표 배치엔 `CanvasPanel`(+`CanvasPanelSlot.SetPosition/SetSize`) 필수. Overlay/Box는 자유좌표 불가. 루트를 C++ `RebuildWidget`에서 만들 수도 있으나 fragile → WBP에 CanvasPanel 1개 두는 게 안정적.
- **완전 무인(헤드리스) WBP 생성이 목표면 → `BindWidget` 대신 C++ `RebuildWidget` 자가 트리.** stock Python이 위젯 트리를 못 만들므로(§6, 함정 #7), 디자이너 0단계로 끝내려면 위젯이 자기 트리를 코드로 구성해야 한다. 패턴: `RebuildWidget()`에서 `WidgetTree->RootWidget == nullptr`일 때 `WidgetTree->ConstructWidget<T>(cls, FName)`로 만들고 루트 지정(디자이너가 직접 넣었으면 `FindWidget`으로 그걸 사용 — 양립). 그러면 WBP는 빈 채로 `WidgetBlueprintFactory` 헤드리스 생성+부모지정, DT 등록까지 `UnrealEditor-Cmd -ExecutePythonScript` 한 번에 끝남(디자이너 작업 0). 패턴 자체는 빌드/헤드리스로 동작 확인(2026-06-03). 폰트 등 에셋은 `TSoftObjectPtr+LoadSynchronous`. 복잡한 다중 트리는 여전히 디자이너 권장. **판단 기준: "디자이너가 이 위젯 내부를 손볼 일이 있나?" 없으면 C++ 자가 트리(무인), 있으면 BindWidget(수동).** (실례 — `UFloatingNumberWidget`(자원 +N 플로팅 숫자)는 자가 트리로 만들 수 있었으나, "+N" 의 **타이포를 아트가 소유**하도록 최종적으로 **BindWidget(필수)+디자이너 배치**로 결정. 판단 기준이 BindWidget 을 가리킨 케이스.)
- **단발 오버레이 vs 공유 레이어**: 수명 명확한 단발 기능(배치 화살표 등)은 **기능별 독립 오버레이 위젯**(AddToViewport, 캡슐화)이 정석. 동시에 **여러** 월드추적 마커(이름표/알림/말풍선/데미지)가 생기면 **공유 오버레이 레이어 1개**(캔버스 1개에 자식 N개 add/remove, 좌표변환 전담)로 모으는 게 정석 — 선례: `ScoreOrbContainerWidget`/`CoinFlyoutContainerWidget`, `UIManagerSubsystem::GetInGameLayer()`. 다수 필요해질 때 도입(YAGNI).

## WBP 위젯트리 T3D 텍스트 저작 + 시각 루프 (2026-06-06 확립 — UI_HQManagePanel 리디자인으로 검증)

**용도**: stock Python이 WBP 위젯트리를 못 만드는 한계의 우회. Claude가 트리 전체를 UMG 클립보드(T3D) 포맷 텍스트 파일로 작성 → 사용자가 디자이너에서 **붙여넣기 1회**. 수정 루프도 "txt 수정 → 재붙여넣기"라 ~10초.

### 절차 (순서 엄수)
1. **디자인 확정 먼저**: §0.5 브리프로 요구사항 합의 → **Artifact 목업**(§0.5 「목업 매체」 규칙 — `artifact-design` 스킬 로드, 반복 튜닝은 인터랙티브 슬라이더; 로컬 `HTML→Chrome`은 실사 배경 필요 시만)으로 비율/느낌을 사용자와 합의. **UMG에서 산수로 더듬지 말 것** — 목업 수치를 이식.
2. **참조 에셋 선행 생성**: 트리가 참조할 스타일/폰트 에셋이 없으면 paste 시 그 프로퍼티만 **silent None** (에러 없음 → Roboto 폴백/스타일 누락). 텍스트 스타일은 make_* 스크립트(Python)로 먼저 생성.
3. 트리 txt 작성 → `C:\tmp\*.txt` (공백 없는 경로). 기존 WBP의 디자이너 복사(Ctrl+C) 출력을 문법 레퍼런스로 받아둘 것.
4. 에디터: 루트 위젯 선택→Delete(트리 비움) → **메모장에서** txt 전체 복사(채팅창 복사 금지 — wrap 변형) → Hierarchy 루트 항목 선택→Ctrl+V → **Compile(BindWidget 경고 0 확인)** → Save.
5. **시각 루프**: PowerShell `CopyFromScreen` 캡처(+크롭) → Read로 진단 → txt 수정 → 재붙여넣기. 사용자 스크린샷이 더 정확하면 그걸 받기.
6. **보관**: 확정 트리는 `docs/05_UI/WidgetTrees/<WBP명>.tree.txt` 로 커밋 (목업은 `04_ArtDirection/UIChromePrompts/output/MOCKUP_*`). **실물은 WBP** — 에디터에서 수정했으면 재디자인 전에 디자이너 복사(Ctrl+C)로 txt부터 현행화 (WidgetTrees/README.md 규칙).

### T3D 포맷 규칙 (검증됨)
- 위젯 = top-level `Begin Object Class=... Name=...` (이름 **트리 전역 유일**). 슬롯 = 컨테이너 블록 안 서브오브젝트(빈 선언 → `Begin Object Name=` 재오픈에 Padding/Align/`Parent=`/`Content=`) + `Slots(i)=` 배열(자식 순서).
- **⚠⚠ 「빈 선언 → 재오픈」은 슬롯에만 해당한다. 위젯 본체는 블록 하나에 다 넣어야 한다 (2026-08-05 실측 — 붙여넣기 전면 실패)**:
  위젯은 `Begin Object Class=... Name="X"` **단일 블록** 안에 자기 슬롯 선언 · 슬롯 재오픈 · `Slots(i)=` · 프로퍼티 · `DisplayLabel` 을 **전부** 담는다.
  위젯을 빈 껍데기로 먼저 늘어놓고 나중에 `Begin Object Name="X"` 로 다시 열어 프로퍼티를 넣으면 파서가 그 프로퍼티를 어디에도 못 붙여 **paste 가 통째로 깨진다**(에러 메시지 없음).
  ⇒ **작성 전 반드시 라이브 트리 파일 하나를 문법 레퍼런스로 열어 볼 것** — `WidgetTrees/UIE_TraitSetChip.tree.txt` 가 가장 작고 완전한 예다. `DisplayLabel` 도 라이브 트리엔 예외 없이 있다.
- **기계 검증 (작성 후 필수)**: `Content="…'이름'"` 참조 ↔ `Begin Object Class=… Name="이름"` 선언, `Slots(i)=` ↔ 슬롯 선언, `Begin`/`End` 균형, 위젯 중첩 0, 그리고 **루트 후보가 정확히 1개**인지. 스크립트로 훑으면 20줄이고 붙여넣기 실패를 사전에 잡는다.
- `ExportPath` 불필요. BP 위젯 참조 = `/Game/...Asset.Asset_C'이름'`. 스타일 = `Style="/Script/Engine.BlueprintGeneratedClass'/Game/....x_C'"` + `bStyleNoLongerNeedsConversion=True`.
- **⚠ T3D 리드백 시 `bOverride_*` 짝 줄을 반드시 같이 볼 것 (2026-07-27 실측)**: T3D는 diff-from-CDO라 `MinDesiredWidth=450` 같은 값 줄이 **플래그 없이 홀로 남아 있을 수 있고, 그건 적용 안 되는 죽은 값**이다(`USizeBox::SynchronizeProperties`가 `bOverride_*`가 false면 값을 무시). 값 줄만 보고 "크기 권위 충돌"로 오진하면 멀쩡한 레이아웃을 건드리게 된다 — **판정 공식: 값 줄 + `bOverride_<이름>=True` 둘 다 있어야 살아 있는 값.** 죽은 값은 디테일 패널에도 숫자로 보여 오인을 부르니, 발견하면 `set_*(0)` → `clear_*()` 로 줄 자체를 지워 둘 것.
- FText는 평문 `Text="승급"` OK. SizeBox 치수는 `bOverride_WidthOverride=True` 플래그 동반 필수. **색은 linear** (sRGB hex 직접 금지 — 변환식 ((c+0.055)/1.055)^2.4).
- **브러시 프로퍼티명 함정 (2026-06-06 실측)**: `UBorder` = `Background=`, `UImage` = `Brush=` — Border에 Brush= 쓰면 silent 무시. Border 자식은 반드시 `BorderSlot` 서브오브젝트 경유(`Content=` 직접 지정 금지). `Slots(i)=` 값은 **따옴표 필수**. 위젯을 루트 블록 안에 중첩하면 paste 시 일부 위젯 소실 → BindWidget not-found 컴파일 실패 — 반드시 톱레벨 형제 평탄화. ⚠ **UBorder 콘텐츠 패딩은 위젯 `Padding`에도 `BorderSlot.Padding`과 같은 값**을 넣을 것 — 슬롯에만 넣으면 컴파일 시 위젯 `Padding`(기본 0)이 슬롯을 덮어 패딩 무효(디테일엔 슬롯 숫자 보여 오인). 상세 = 아래 「레이아웃 함정」.
- **⚠⚠ UMG 디자이너 프리뷰는 `NativeTick` 을 돌리지 않는다 (2026-08-07 실측)**: 그래서 **"틱에서 머티리얼/브러시에 값을 주입하는" 설계는 디자이너에서 통째로 거짓말을 한다** — 저작자가 실물을 못 보므로 수정 루프 자체가 성립하지 않는다(같은 트리를 3번 다시 붙여넣게 만든 원인). 게다가 주입 소스로 흔히 쓰는 `NativeTick(MyGeometry)` 는 **UUserWidget 전체 크기**라, 실제 그릴 대상이 그보다 작은 내부 패널이면 런타임에도 틀린다(디자이너에선 "Desired on Screen" 이라 우연히 맞기도 해서 더 헷갈린다). ⇒ **크기 의존 머티리얼은 자가측정(fwidth) 마스터로**(`UI_STYLE_CATALOG.md` SDF 3계열 표), 굳이 주입해야 하면 `대상위젯->GetCachedGeometry().GetLocalSize()` 를 쓸 것.
- **T3D paste 대신 원격 Python 직접 저작 (2026-08-07 Task 7c 로 전환)**: paste 는 ① `Style=` 을 silent drop 하고 ② 위젯을 통째로 드롭해도 에러가 없어 **"붙였는데 왜 다르지"** 루프를 만든다. 기존 트리를 손보는 작업이면 위 §6 표의 저작 트리 경로로 `new_object`/`add_child`/`set_editor_property` → `compile_blueprint` → **`_C` 트리 리드백 전수 검증** → 검증 통과할 때만 `save_asset` 이 훨씬 빠르고 확실하다. **끝에 붙일 위젯은 순서 문제도 없다**(마지막 자식이면 `remove_child` → 새 부모에 `add_child` → 부모 패널에 `add_child` 로 순서 보존).
- **BP 위젯 인스턴스의 `Style=`(TSubclassOf) 오버라이드는 paste가 silent drop (2026-07-04 UI_BuildOpen 실측)**: 같은 인스턴스 블록의 Text=/IconTexture=는 적용되는데 Style= 클래스 참조만 조용히 버려져 CDO 기본 스타일로 남음. 스타일 오버라이드는 **paste 후 원격 Python으로 적용**: `load_object(None, "...:WidgetTree.버튼명").set_editor_property("style", 스타일_C)` → `compile_blueprint` → `save_asset` → 리드백 검증.
- 새 변형 WBP 생성도 가능: 에셋 우클릭 생성(부모 C++ 클래스 지정) → 빈 루트에 트리 paste (BindWidget/Optional 이름 = C++ 헤더와 대조).
- **⚠ `connect_material_expressions` 는 실패해도 예외를 안 던진다 — 반환값을 반드시 확인할 것 (2026-07-29, 반나절 날림)**:
  핀 이름이 틀리면 조용히 `False` 만 돌려주고 입력이 빈 채 남는다 → **머티리얼 컴파일 실패 → 런타임엔 '기본 머티리얼'이 대신 쓰여 아무것도 안 그려진다.**
  로그에도 `LogMaterial: Warning: ... Failed to compile Material ..., Default Material will be used in game.` 한 줄만 뜨고 어느 노드가 문제인지는 안 나온다.
  **입력 핀 이름은 노드마다 다르고, 단일 입력 노드는 이름이 없다(`""`)** — `ComponentMask`/`OneMinus` 에 `"Input"` 을 주면 실패한다.
  실측 핀 이름: `ComponentMask`/`OneMinus`=`""` · `Multiply`/`Add`/`Subtract`/`AppendVector`=`"A"`,`"B"` ·
  `Panner`=`"Coordinate"`,`"Time"`,`"Speed"` · `TextureSample`=`"UVs"`,`"Tex"`.
  모르면 `unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)` 로 먼저 조회.
  ```python
  def link(a, a_out, b, b_in):
      if not unreal.MaterialEditingLibrary.connect_material_expressions(a, a_out, b, b_in):
          raise RuntimeError(f"연결 실패: {a.get_name()}[{a_out}] -> {b.get_name()}[{b_in}]")
  ```
  진단은 **복잡도를 한 단계씩 올린 임시 머티리얼을 여러 개 만들어 한 번에 recompile → 로그에서 어느 이름이 실패했는지 보는 이분 탐색**이 가장 빠르다.
- **⚠ `VectorParameter`/`Constant4Vector` 의 기본 출력은 float3(RGB) — `.ba` ComponentMask 는 컴파일 실패 (2026-08-12 실측)**:
  `Not enough components in (Material.PreshaderBuffer[0].xyz: float3) for component mask 0011` → **머티리얼이 통째로 default 로 폴백**(연결 API 는 전부 True 를 돌려줘서 코드만 보면 정상).
  4채널을 파라미터 하나로 쓰려면 **채널 핀(`"R"`/`"G"`/`"B"`/`"A"`)을 `AppendVector`(입력 `"A"`,`"B"`)로 조립**할 것. 예: UV 크롭 창 `UVWindow`(R=OffU,G=OffV,B=ScaleU,A=ScaleV) → `Append(B,A)`=스케일, `Append(R,G)`=오프셋.
  **진단 = 만들자마자 그리기**: `draw_material_to_render_target` 로 원본 머티리얼과 신규를 같은 텍스처로 각각 그려 **픽셀 동일성**을 보면(기본값이 항등이어야 하는 파생 머티리얼은 반드시 동일) 폴백을 즉시 잡는다. 로그의 `LogMaterial: Warning: ... Failed to compile` 한 줄이 유일한 다른 단서다.
- **머티리얼 노드 열거는 `Material.expressions` 금지** — 5.4 에서 protected(EditorOnlyData 이동). `get_num_material_expressions()` / `delete_all_material_expressions()` 를 쓸 것. **그래프 역추적은 가능** — `MaterialEditingLibrary.get_material_property_input_node(mat, MP_*)` + `get_material_property_input_node_output_name` + `get_inputs_for_material_expression(mat, node)` 로 Emissive/Opacity 부터 거슬러 올라가면 원본 계약(어느 핀이 물려 있나)을 손 안 대고 확인할 수 있다.
- **⚠ `delete_all_material_expressions` 는 한 번에 다 안 지운다 (2026-07-29 실측: 38 → 18 → 8 → 3)**:
  한 번만 부르고 다시 지으면 **고아 노드가 남아 재실행마다 그래프가 불어난다**(파라미터 중복은 안 생기고 컴파일/동작엔 무해하지만, 사람이 열면 스파게티).
  → **개수가 안 줄어들 때까지 반복 호출**할 것. `while n and n != prev: prev=n; delete_all(...)`.
- **UI 모션은 "픽셀이 몇 % 변했나"로 검증하지 말 것 (2026-07-29 사고)**: 안쪽 무늬만 흔들려도 수치는 잘 나오지만 눈에는 **정지로 보인다**.
  판정 지표는 **실루엣(가장자리)이 초당 몇 px 움직이나**. 그리고 로딩/전환 화면처럼 **노출이 몇 초뿐인 UI 는 "은은한" 속도가 곧 정지**다 —
  실측 기준선: 2560 캔버스에서 구름 22px/s = 안 보임 / 95px/s = 보임.
- **UI 머티리얼 헤드리스 검증**: `draw_material_to_render_target`(MD_UI 지원) → **다음 원격 호출에서** `export_render_target`.
  같은 호출 안에서 draw→export 하면 렌더 커맨드가 아직 안 돌아 **빈 이미지**가 나온다. 또 결과 RT 는 프리멀티라 **알파 채널은 항상 0** — 판정은 RGB 로 할 것.
- **브러시 `ImageSize` 는 Python 으로 못 바꾼다 — T3D paste 전용 (2026-07-29 실측)**: 5.4 의 `FSlateBrush::ImageSize` 는 `FDeprecateSlateVector2D` 라
  ① `brush.set_editor_property("image_size", unreal.Vector2D(w,h))` → `NativizeStructInstance` **TypeError** (`unreal.DeprecateSlateVector2D(w,h)` 생성자도 인자 0개라 불가),
  ② `image.set_brush_size(unreal.Vector2D(w,h))` 는 **예외는 없지만 위젯트리 아키타입에 persist 안 됨** (리드백 여전히 기본 32x32).
  → 값이 꼭 필요하면 트리 txt 의 `Brush=(ImageSize=(X=..,Y=..))` 로 paste 할 것.
  ※ 단, **캔버스 슬롯에 크기가 명시된 이미지면 ImageSize 는 렌더에 영향 없음**(desired size 자동사이징 전용). 실제로 `UI_LoadingWidget` 의 CloudA/B 는
    트리 txt 에 460x116/300x76 이 적혀 있었는데 라이브 에셋은 32x32 였고 화면은 정상이었다 — 슬롯 크기가 유일 권위라는 증거. 불일치를 버그로 오진하지 말 것.

### 레이아웃 함정 (이번 검증에서 발견)
- **⚠ `ScaleBox` 는 혼자서는 아무것도 안 묶는다 — 폭을 묶는 건 슬롯이다 (2026-08-06 엔진 소스 확인)**:
  `SScaleBox::ComputeContentScale`(SScaleBox.cpp:178) 은 **자기 할당 크기 ÷ 자식 desired 크기**로 배율을 내고,
  `ComputeDesiredSize`(:378) 는 ScaleToFit 일 때 **자식의 원본 desired 크기를 그대로 반환**한다.
  ⇒ ScaleBox 를 감싼 **부모 슬롯이 `SizeRule=Fill` + `HAlign_Fill`** 이 아니면 할당 = desired 가 되어 **배율이 영원히 1**이다.
  `HAlign_Center/Left` 는 `AlignChild` 가 자식에게 desired 크기를 그대로 주므로(클램프 없음) 똑같이 안 먹는다.
  ⇒ 순서는 **① 폭을 묶고(Fill 슬롯) ② 그 안에 ScaleBox(`ScaleToFit`+`DownOnly`)**. ScaleBox 안쪽 `ScaleBoxSlot` 정렬(기본 Center)은 배율과 무관 — 배치만 정한다.
  실사고: `UIE_UpgradeSlotHorizon` 에서 `HorizontalBox_101` 3슬롯이 전부 `Automatic` 이라 부제가 길어지면 강화 버튼이 카드 밖으로 밀려남 → 가운데 열을 `Fill` 로.
- **⚠ 값이 두 경로(액터 vs 세이브)에서 오면 강화 직후 한쪽만 늦는다 (2026-08-06 빌드업 인원 표기 실측)**:
  `UpgradeEnhancement` 는 액터 상태만 즉시 바꾸고 저장은 `RequestDeferredSave()` 다. 그래서 **세이브를 읽는 조회 함수**
  (`UEmployeeManager::GetBuildingEmployeeCapacity` = `AtFloors(INDEX_NONE)` → `BuildingSave.BuildingData.Body_Module_Copies`)를
  갱신 직후에 부르면 **현재값만 옛 값**이고 미리보기(다음값)만 맞는다 — "왼쪽 숫자가 안 바뀐다" 증상.
  ⇒ 강화 직후 갱신 경로에서는 **현재/다음을 같은 권위(액터 레벨)로** 뽑을 것: `AtFloors(CurrentLevel)` / `AtFloors(NextLevel)`.
- **UBorder 콘텐츠 패딩은 위젯 `Padding`에도 넣어라 (2026-07-04 실측)**: `UBorder`(패널·배지·셀·플레이트)는 패딩이 **이중** — 자식 `BorderSlot.Padding` + 위젯 자체 `UBorder.Padding`. `UBorder::SynchronizeProperties()`가 **컴파일마다 위젯 `Padding`을 Slate에 밀어넣어 슬롯 값을 덮음**. 그래서 값을 `BorderSlot`에만 넣고 위젯 `Padding=(0,0,0,0)`으로 두면 **화면 패딩 0**인데 디테일 패널엔 슬롯 숫자(예:28)가 보여 "숫자는 있는데 안 먹힘"으로 오인. **위젯 `Padding`과 `BorderSlot.Padding`을 같은 값으로** 둘 것(어느 쪽이 이겨도 정확). ⚠ HBox/VBox/Overlay/SizeBox는 위젯 레벨 패딩 오버라이드가 없어 **슬롯 패딩이 유일 권위 → 무관**(Border만 이 함정).
- **Float 보더 ShadowMargin**: `MI_Border_*_Float`은 ShadowMargin(짧은 변 비율, **실측 0.02** — 구 문서의 0.06은 오기, 2026-07-20 리드백 확인)만큼 시각 박스를 위젯 안쪽에 그림 → **위젯크기 = 시각목표 ÷ (1-2×margin)** 역산 + 콘텐츠 패딩에 마진 가산. 모서리 장식(리본/X) 위치도 시각 라인 기준. 이거 모르면 "탭이 프레임 관통/요소가 떠 보임" 증상. (룩 자체는 2026-07-20 v2 다크 플랫 — 카탈로그 §1 참조)
- **모달 3단 깊이 필수**: 다크 셸(프레임) → 밝은 플레이트(콘텐츠 바닥, Panel_Cream) → well(음각)/칩(밝음). 플레이트 없이 카드를 셸에 직접 올리면 어수선함.
- **리본과 가로 탭을 같은 밴드에 두지 말 것** (실게임 조사: 층 분리 또는 좌측 세로 탭). 리본은 전용 실루엣(꼬리) 없으면 "탭 1개 추가"로 읽힘 — 다크 셸에선 내부 타이틀이 정석.
- 디자이너 프리뷰는 WBP 기본값 표시(런타임 값 아님) — 인스턴스 `DefaultStatValue` 등으로 프리뷰 정리.
- 흰 글리프 공용 부품을 라이트 면에 쓸 때: 인스턴스 `ColorAndOpacity` 틴트(콘텐츠 곱셈).
- **plain `UTextBlock` BindWidget엔 `CUI_Style_Text_*` 지정 불가**(Style 슬롯은 CommonTextBlock 전용) — T3D 작성 전 BindWidget의 C++ 헤더 타입을 반드시 대조. CommonTextBlock으로 바꾸려면 헤더 변경=풀 빌드. plain이면 폰트 직접 지정으로 스케일 사양(폰트/사이즈)을 수동 일치 (2026-06-06 빌드모달 타일 실측).
- **C++가 텍스트만 토글하는 배지를 T3D에서 장식 Border(알약)로 감싸면 비표시 상태에 빈 알약이 남는다** (2026-08-02 리뷰 페이지 검수 발견) — 감싸기 전 C++ 토글 대상을 확인하고, 감쌌으면 C++ 토글/리빌 대상을 플레이트로 승격(BindWidgetOptional 또는 `GetParent()+IsA<UBorder>` 폴백 — 구 `LaunchConfirmWidget::FillReviewSection` 의 `BadgePlateOrSelf` 패턴 — 2026-08-22 리뷰 페이지 폐기로 그 람다는 소멸, 원리만 유지).
- **셀레브레이션 위젯(스파클류)은 연출 종료 후에도 opacity 0 으로 남아 desired size 가 레이아웃을 영구 팽창시킨다** (2026-08-02) — 고정 높이 SizeBox 캡 오버레이 안에 넣어 흡수(Slate 무클리핑이라 렌더는 밖으로 정상 표출). 또한 산개 오프셋이 baked 된 위젯에 C++가 `SetRenderTransform` 을 걸면 Translation 이 0으로 스톰프됨 — 스케일 연출은 `SetRenderScale` 로.

## 참고 (CLAUDE.md / 메모리)
- CLAUDE.md: "자주 실수하는 코드 패턴"(위젯 로드/좌표/델리게이트/탭/입력모드/이모지), "레거시 제거 규칙", "파일 삭제 시 참조 동반 수정".
- 메모리: `feedback_asset_categorized_folder`, `feedback_ui_component_pattern`, `feedback_no_special_chars`, `feedback_tab_spelling`, `feedback_ue_python_exec`, `feedback_temp_script_cleanup`, `project_ui_design_tooling`, `project_alertmark_system`, `project_number_abbreviation`.
