# 60 FPS 최적화 — 종합 문서

CompanyGrowthRenewal 프로젝트의 성능 최적화 단일 진실 원천 (Single Source of Truth).

**대상 플랫폼**: 모바일 (Android / iOS) + PC 에디터
**목표**: PIE 60 FPS 안정 + 모바일 60 FPS 달성

---

## Phase 1-A — Config/DefaultEngine.ini (적용 완료, 2026-05-02)

시각 손해 0%, 시각 영향 없는 설정만:

| 키 | 변경 전 | 변경 후 | 이유 |
|---|---|---|---|
| `r.Shadow.Virtual.Enable` | `1` | ~~`0`~~ **`1` (롤백)** | ~~VSM은 Lumen/Nanite와 묶음 기능~~ **잘못된 판단**. VSM은 독립 작동하며, CSM 폴백 시 shadow swimming + staircase artifacts 발생 확인 (2026-05-27 롤백) |
| `r.Mobile.PlanarReflectionMode` | `2` | `0` | 평면 반사(거울/물) 사용 안 함 |
| `t.MaxFPS` | `120` | `0` | vsync에 위임 — 디바이스 refresh rate 자동 매칭 (60Hz / 120Hz 모두 대응) |
| `FrameRateLock` (iOS) | `PUFRL_60` | `PUFRL_None` | iPhone Pro ProMotion 120Hz 활성화 |

---

## Phase 2~5 — 코드측 최적화 (적용 완료, 2026-05-02)

### #1 TimeCycleSky 변경 감지 + 0.1s 스로틀

**파일**:
- `Source/CompanyGrowthRenewal/Public/TimeCycle/TimeCycleSky.h`
- `Source/CompanyGrowthRenewal/Private/TimeCycle/TimeCycleSky.cpp:142-260`

**문제**: 매 프레임 `SetIntensity` / `Temperature` / `SetRelativeRotation` 호출 → DirectionalLight를 dirty 표시 → RHI 셰이더 constant 매 프레임 재전송. 시간은 게임 분 단위로 변하는데 60Hz 갱신은 100% 낭비.

**해결**:
- 헤더에 `UpdateAccumulator` + `LastSunIntensity/MoonIntensity/Temperature/Pitch` 추가
- Tick 시작에 `0.1s` 스로틀 (10Hz 갱신)
- 각 Setter 호출 전 `FMath::IsNearlyEqual` 변경 감지
- SunPitch 비교는 한 번만 하고 Sun/Moon 둘 다 적용 (회전 동기화 유지)

**효과**: RHI dirty 제거 약 0.5~1ms

### #2 BubbleContainerWidget Slot 캐싱

**파일**:
- `Source/CompanyGrowthRenewal/Public/UI/Element/BubbleContainerWidget.h`
- `Source/CompanyGrowthRenewal/Private/UI/Element/BubbleContainerWidget.cpp`

**문제**: `NativeTick`이 매 프레임 `Cast<UCanvasPanelSlot>(Data.BubbleWidget->Slot)` 호출. 활성 버블 N개 × 매 프레임 RTTI 비교.

**해결**:
- 헤더 `class UCanvasPanelSlot;` 전방 선언 + `FBubbleAnimData::CachedSlot` 추가
- `CreateBubble`의 `AddChildToCanvas` 반환값을 즉시 `CachedSlot`에 저장
- `NativeTick`에서 Cast 대신 캐시된 포인터 사용

**효과**: 0.2~0.5ms (활성 버블 개수에 비례)

**미적용 후보**: 카메라 정지 시 좌표 갱신 스킵 — `MainMapPlayerController` 카메라 트랜스폼 변경 추적 필요해서 이번엔 보류

### #3 InGameLayerWidget 디버그 로그 정리

**파일**: `Source/CompanyGrowthRenewal/Private/UI/Panel/InGameLayerWidget.cpp:47-68`

**문제**: `static bool bLogged` 분기가 매 프레임 도는 1회용 디버그 로그.

**해결**: 블록 전체 제거 (1회 로그면 NativeConstruct로 옮길 가치 없음).

**효과**: 미미 (분기 cost ~0)

### #4 Officeworker 13개 SkeletalMesh VisibilityTick

**파일**: `Source/CompanyGrowthRenewal/Private/Entity/Officeworker/Officeworker.cpp:121-137`

**문제**: 직원 1명당 13개 SkeletalMeshComponent (Face + 12개 LeaderPose follower). 화면 밖 직원도 매 프레임 본 트랜스폼 갱신.

**해결**: 13개 컴포넌트 모두에 `VisibilityBasedAnimTickOption = OnlyTickPoseWhenRendered` 일괄 적용 (생성자에서 lambda로).

**효과**: 화면 밖 직원당 0.1~0.3ms 절감

**LeaderPose follower 동시 적용 이유**: root(Face)가 OnlyTickPoseWhenRendered여도 follower들이 다른 옵션이면 Pose 동기화 시 일관성 깨질 위험. 모두 동일 옵션이 안전.

### #5 On-demand Tick (Wobble Timeline 진행 중에만 Tick 활성화)

**핵심 패턴 — 이 프로젝트의 모든 Interactable에 적용됨**.

**파일**:
- `Source/CompanyGrowthRenewal/Private/Entity/InteractableBaseActor.cpp` (생성자, PlayWobble, Wooble_TimelineFinished, EndWooble)
- `Source/CompanyGrowthRenewal/Private/Entity/Building/BuildingBaseActor.cpp` (PlayWobble, Wooble_TimelineFinished override)
- `Source/CompanyGrowthRenewal/Private/Entity/Factory/BrickFactory.cpp` (PlayBounce, Wooble_TimelineFinished override)

**문제**: `AInteractableBaseActor::Tick`이 단순히 `Wooble_Timeline.IsPlaying() → TickTimeline()` 만 호출하는데, 빌딩/공장 N개 × 매 프레임 빈 분기 도는 비용. 인터랙션 안 할 때는 100% 낭비.

**해결 (정식 UE 패턴)**:
1. 부모 생성자: `bCanEverTick=true` + `bStartWithTickEnabled=false` (자식이 안 덮어씀)
2. `PlayWobble()` / `PlayBounce()` 끝 (`PlayFromStart()` 직후): `SetActorTickEnabled(true)`
3. `Wooble_TimelineFinished()` 끝: `SetActorTickEnabled(false)`
4. `EndWooble()` (수동 종료) 끝: `SetActorTickEnabled(false)`

**중요한 함정**: 자식 클래스에서 `PlayWobble` / `Wooble_TimelineFinished` override 시 `Super` 호출하지 않으면 부모의 `SetActorTickEnabled` 호출이 누락됨. **자식 override에도 동일 패턴 직접 추가 필요** — BuildingBaseActor + BrickFactory 둘 다 Super 호출 안 해서 자식에도 적용함.

**효과**: 빌딩/공장 N개 × Tick 분기 비용 → 인터랙션 시점 ~0.3초만 Tick. 평소 0.

---

## Phase 8 — 전수 조사 감사 1차 적용 (2026-06-02)

멀티에이전트 전수 조사(9차원 finder + 발견 건별 적대적 검증)에서 나온 위험 0~低 "DO FIRST" 6건 적용. **컴파일·링크 검증 완료**(UBT `CompanyGrowthRenewalEditor Win64 Development` exit 0, 변경 9개 .cpp 개별 컴파일 + lib/dll 링크). PIE 회귀 체크 통과.

### #1 EmployeeAnimInstance 매 프레임 디버그 메시지 차단
**파일**: `Private/Entity/Officeworker/EmployeeAnimInstance.cpp`
**문제**: `NativeUpdateAnimation`이 워커마다 매 프레임 state FString 빌드 + `GEngine->AddOnScreenDebugMessage`(시핑에도 실행). N명 전부 화면 → N × (2 alloc + Printf + debug-map insert)/frame.
**해결**: 디버그 블록을 `#if !UE_BUILD_SHIPPING` + `cg.DebugWorkerState` cvar(기본 0)로 차단. 콘솔에서 켤 때만 동작.

### #2 ProjectOperationManager 진단 루프 삭제
**파일**: `Private/Manager/ProjectOperationManager.cpp:TickCallback`
**문제**: `UpdateOperations` 전에 active operation마다 `GetOfficeSaveDataByBuildingID` + `CalculateWarehouseCapacity`를 호출해 UE_LOG 한 줄만 출력(1Hz × N, 서브시스템 조회 + Buildings 선형 스캔). 게다가 `GetOfficeSaveDataByBuildingID`가 `OfficeDataMap.FindOrAdd` → 읽기마다 빈 엔트리 삽입(세이브 비대화).
**해결**: 진단 for-loop 통째 삭제 → `UpdateOperations(TimerTickInterval)`만 남김. (FindOrAdd 자체 수정은 Phase 9 후보)

### #3 Wobble Timeline per-frame 비용 제거 (빌딩 + 베이스)
**파일**: `Private/Entity/InteractableBaseActor.cpp`, `Private/Entity/Building/BuildingBaseActor.cpp`
**문제**:
- 베이스/빌딩 모두 `Wooble_TimelineUpdate`에 매 프레임 `UE_LOG(Error)`(시핑 포함 전 빌드 컴파일).
- 빌딩 override는 `scaleValue`를 무시하고 리터럴 `1.f`를 3컴포넌트 머티리얼에 매 프레임 재푸시 → 같은 값으로 render proxy 매 프레임 dirty.
**해결**:
- 베이스: per-frame Error 로그 제거(머티리얼 push는 `scaleValue`를 실제 사용하므로 **유지**). PlayWobble/EndWooble의 per-click Error 로그도 제거.
- 빌딩: `Wooble_TimelineUpdate` 비움 + Wobble=1.f 게이트를 `PlayWobble`에서 1회 ON(Finished의 OFF는 기존 유지). per-click Error 로그 제거.
**함정 회피**: FTimeline은 계속 tick(빈 update 콜백이어도 `Wooble_TimelineFinished` 정상 발화) → 동결 회귀 없음. 빌딩은 원래 0/1 스냅이라 시각 동일.

### #4 Officeworker 버블 누산기 공유 static 버그 수정
**파일**: `Private/Entity/Officeworker/Officeworker.cpp:Tick`, `Public/Entity/Officeworker/Officeworker.h`
**문제**: `static thread_local float BubbleCheckAccum` — 전 워커 공유. N명이 매 프레임 `+=` → 1초 게이트가 ~N배 빨리·비결정적으로 발화(버블 cadence desync + 낭비).
**해결**: per-instance `float BubbleCheckAccum` 멤버로 교체 → 워커별 독립 1Hz.

### #5 WorldMapLayerWidget 프레임 불변값 hoist
**파일**: `Private/UI/Panel/WorldMapLayerWidget.cpp`, `Public/UI/Panel/WorldMapLayerWidget.h`
**문제**: `UpdateCountryNamePositions`가 국가 루프 안에서 `GetFirstPlayerController` + `GetViewportWidgetGeometry` + `GetCachedGeometry`를 11회 반복(전부 프레임 불변, 동일 결과).
**해결**: 셋을 루프 밖 1회 계산으로 hoist + 투영 인라인. 죽은 `ProjectWorldToCanvas` 함수 + 헤더 선언 제거(레거시 규칙). 가시성(-500 threshold) 로직 + `AbsoluteToLocal` 규칙 보존.

### #6 위젯 슬롯 캐싱 (Phase 2~5 #2 BubbleContainer 패턴 미러)
**파일**: `Private/UI/Element/Effects/ScoreOrbContainerWidget.cpp`+`.h`, `Private/UI/Element/Common/CoinFlyoutContainerWidget.cpp`+`.h`
**문제**: NativeTick이 매 프레임 `Cast<UCanvasPanelSlot>(Image->Slot)`(orb 2개/coin 1개 × 활성 개수).
**해결**: anim 데이터 구조체에 슬롯 포인터(`OrbSlot`/`GlowSlot`/`CachedSlot`) 추가 → 생성 시 `AddChildToCanvas` 반환값 저장, Tick에서 캐시 사용. (효과 trivial~low, 패턴 일관성 목적)

**동시 수정 잠복 버그 3건**: #4 공유 static 누산기 / #3 무시되던 `scaleValue` / #2 FindOrAdd 유령삽입 압력.

---

## Phase 10 — MainMap 앰비언트 트래픽 최적화 (적용 완료, 2026-06-24)

7차원 멀티에이전트 모바일 감사(config / lighting / rendering / tick / drawcall / UI / VFX) + 발견 건별 적대적 검증(43건 confirmed). 가장 높은 **신규·저위험·고효과 1건** 적용. **빌드 검증 완료**(UBT `CompanyGrowthRenewalEditor Win64 Development` exit 0, UHT 재생성 + `TrafficManager.cpp` 컴파일 + DLL 링크, `-WarningsAsErrors` 통과). **PIE 동작 검증 대기**.

> 배경: Phase 8(2026-06-02) 이후 추가된 MainMap 앰비언트 시스템(트래픽/부지/보트 등)은 이전 최적화 패스 범위 밖이었음. 이번 감사는 그 신규 시스템 위주.

### #1 TrafficManager 매 프레임 시뮬 → 30Hz 스로틀 + 숨김 차량 인스턴스 쓰기 스킵

**파일**: `Public/Entity/Ambient/TrafficManager.h`, `Private/Entity/Ambient/TrafficManager.cpp`

**문제**: `ATrafficManager::Tick`이 매 프레임 ① `GCars.Sort`(O(n log n)) ② 50대 spacing 계산 ③ 차량별 spline/lerp/yaw 갱신 ④ 차량별 ISM `UpdateInstanceTransform`(body + 라이트 4장) ⑤ ISM 3개 `MarkRenderInstancesDirty`를 전부 수행. 차가 화면 밖이든 빌딩에서 멀든 무관하게 풀 비용. 빌딩 게이팅 밖(scale 0) 숨은 차도 매 프레임 트랜스폼 재기록(인스턴스 버퍼 churn).

**해결**:
1. **30Hz 스로틀**(`SimUpdateInterval=0.0333f`, EditAnywhere): `SimAccum` 누적자로 시뮬을 ~30Hz 게이팅. 누적 델타(`SimDelta`)를 `UpdateGraphCar`/`UpdateCar`에 전달 → 스로틀해도 차 속도·car-following 동일(spacing은 SimDelta로 재계산). 스로틀된 프레임은 정렬/갱신/인스턴스 업로드/RHI 플러시를 전부 early-return 스킵. (Phase 2~5 #1 TimeCycleSky 0.1s 스로틀 패턴 미러)
2. **숨김→숨김 스킵**: `FGraphCar`에 `bVisible` 추가(`FTrafficCar`엔 이미 있음). `ApplyCarInstances`에 `bWasVisible` 인자 추가 → `!bVisible && !bWasVisible`이면 인스턴스 쓰기 통째 스킵. 가시 전환(visible→hidden)에서만 scale-0을 1회 기록.
   - **함정 회피**: `AddInstance(FTransform::Identity)`는 origin·scale **1** → 숨은 차가 첫 쓰기를 스킵하면 원점에 차가 보임. `AddInstance(scale 0)` 초기화로 일관성 확보(숨은 차 = scale 0 유지).
3. **저사양 차량 수 상한**: `cg.Traffic.MaxCars` 콘솔변수(0=무제한) 추가 → 디바이스 프로파일에서 `+CVars=cg.Traffic.MaxCars=20`로 저tier 차량 수 축소.
4. **부수 정리**: `bDebugLog` 기본값 `true→false`(3초마다 찍던 시핑 로그). ※ 배치된 레벨 인스턴스가 override(true)로 저장돼 있을 수 있으니 MainMap의 placed `ATrafficManager`도 확인.

**효과**: 30Hz 스로틀 = 높은 fps에서 매니저 비용 ~절반. 숨김 스킵 = **프레임레이트 무관**, 게이트 밖 차량(대부분)의 인스턴스 버퍼 churn 제거 → 낮은 폰 핵심 절감. 두 레버가 상보적(스로틀=고fps에서, 스킵=항상). **거리 무관 균일 최적화**라 본 문서 "카메라 시점 제약" 패러다임에도 부합.

**위험/검증 포인트(PIE)**: ① 차 주행 부드러움(30Hz 스텝 가시성 — 도시 줌에선 33~53cm/스텝이라 무해 예상) ② 빌딩 근처 차 등장/사라짐 정상 ③ 깜빡임·원점(0,0) 차 없음 ④ `cg.Traffic.MaxCars 20` 후 PIE 재시작 시 차량 수 축소.

---

## 회귀 사례 (영구 보존 — 다시 같은 실수 안 하도록)

### 사례 1: `bCanEverTick=false` + 부모 `FTimeline` 함정

**증상**: 빌딩 클릭 안 했는데 클릭된 모션(Wobble) 시작 시점 스케일 그대로 영구 정지.

**원인**:
- `ABuildingBaseActor::ABuildingBaseActor()`에서 `PrimaryActorTick.bCanEverTick = false` 설정
- 부모 `AInteractableBaseActor::Tick(DeltaSeconds)`이 `Wooble_Timeline.TickTimeline(DeltaSeconds)` 호출하던 구조
- `bCanEverTick=false`는 **타입 트리 전체**를 끄는 스위치 → 부모 Tick도 안 돔
- 클릭으로 `PlayWobble()` 호출 → Timeline `Playing` 상태 진입 (Tick 무관) → 다음 프레임부터 `TickTimeline` 호출 안 됨 → Timeline 진행 시간 0 그대로 동결

**진단 실수**: cpp 파일에서 `void ABuildingBaseActor::Tick`만 grep해서 "본문 없음 → 안전하게 끌 수 있음"으로 잘못 결론. **Timeline은 별도 시스템이라 Tick 함수 본문에 안 보임**.

**교훈**:
- `bCanEverTick=false` 적용 전 반드시 부모 클래스 Tick override + Timeline / FlipBook / Curve 사용 여부 확인
- `FTimeline` (구조체)은 액터 Tick에 의존, `UTimelineComponent`도 마찬가지
- 빈 Tick이라도 부모가 무엇을 하는지 봐야 안전 판단 가능
- "필요한 동안만 Tick 켜기" 패턴(#5)이 정답 — Tick을 무조건 끄는 게 아니라 의존 시스템에 맞춰 on-demand로 운용

### 사례 2: 자식 override + Super 미호출 + 부모 변경

자식이 부모 가상 함수 override하면서 `Super::` 호출 안 하면, 부모에 추가한 로직이 자식 인스턴스에서 실행 안 됨. 부모 클래스 수정 시 항상:
- `Grep "void {ChildClass}::{VirtualFn}"`으로 자식 override 본문 검사
- 자식이 Super 호출 안 하면 자식에도 동일 변경 적용

### 사례 3: `r.Shadow.Virtual.Enable=0` → CSM 폴백 → 시각 품질 붕괴

**증상**: 메인맵 원거리 시점에서 그림자가 울렁울렁 흔들리고, 각진 블록 형태가 나타남. 카메라 이동 시 증상 심화.

**원인**:
- VSM을 "Lumen/Nanite 없으면 의미 없음"으로 판단하고 OFF
- 실제로는 VSM은 독립 작동하며, CSM 폴백 시 두 가지 고질적 문제 발생:
  1. **Shadow swimming**: CSM 캐스케이드 경계가 카메라 이동마다 재계산 → 텍셀이 표면 위에서 미끄러짐
  2. **Staircase artifacts**: CSM 텍셀 해상도 한계 → 원거리 그림자 경계 픽셀화
- Isometric 광역 시야 + MaxZoomDistance 320000 조합이라 CSM 품질 저하가 극대화됨

**교훈**:
- VSM은 Lumen/Nanite 전용이 아님. Shadow quality만으로도 VSM 독립 가치 있음
- "시각 손해 0%" 라고 분류한 설정이라도 Shadow 시스템 교체는 반드시 PIE에서 카메라 이동하며 시각 검증 필수
- 특히 Isometric 넓은 시야에서 CSM은 구조적으로 부적합 (텍셀 밀도가 시야 면적에 반비례)

**조치**: 2026-05-27 `r.Shadow.Virtual.Enable=1` 롤백

---

## 절대 되돌리면 안 되는 결정

### `r.AllowOcclusionQueries=0` 절대 ON 금지

**현재 값**: `Config/DefaultEngine.ini:73` `r.AllowOcclusionQueries=0`

**이유**: 사용자가 과거 ON으로 사용 중 **깜빡임 현상**(안 가려진 액터가 한 프레임 안 보이고 다시 나타남) 발생해서 OFF로 변경. Occlusion Query는 비동기 처리(1~3 프레임 결과 지연)라 카메라 빠른 회전 시 잘못된 컬링 결과가 적용됨.

**대안 (필요 시)**:
- `r.Mobile.AllowSoftwareOcclusion=1` (소프트웨어 오클루전, 깜빡임 없음)
- Cull Distance Volume
- Per-Object Cull Distance

### `MaxZoomDistance` 축소 거부됨

`MaxZoomDistance 320000 → 50000` 제안은 사용자가 명시적으로 거부. 게임플레이상 광범위 줌 필요. **이 항목 다시 제안 금지**.

---

## 카메라 시점 제약 — 거리 기반 최적화 무용

**카메라 특성**: Isometric 줌 시점 + 한눈에 모든 빌딩이 시야에 진입. 화면 안의 빌딩들끼리 거리 차이가 거의 없음.

→ 거리 기반(멀리 있는 것 단순화/제거) 최적화는 효과 없거나 미미. 이 패러다임에서는 **균일 최적화 + DrawCall 통합**이 정답.

### 다시 제안 금지 (효과 없음)

| 항목 | 무용 이유 |
|---|---|
| **HLOD (Hierarchical LOD)** | 거리에 따른 자동 결합 메시 전환 → 줌 변동 적어 효과 없음 |
| **Skeletal Mesh / Static Mesh 자동 거리 LOD** | 거리별 자동 LOD 전환 → 모두 비슷한 거리라 항상 LOD0 |
| **Cull Distance Volume / Per-Object Cull Distance** | 모든 액터가 화면 안 → 컬링될 게 없음 |
| **URO (Update Rate Optimization)** | 거리별 애니 빈도 차등 → 사용자가 명시적으로 거부함 (효과 없음 + 시각 일관성 깨짐) |

### 대안 — 효과 있는 균일 최적화

거리 무관, 모든 액터에 동일하게 적용:
- `SetForcedLOD(N)` 강제 LOD 고정 (자동 전환 X, 코드로 강제)
- **Static Mesh Merge** (에디터 Window → Developer Tools → Merge Actors) — 같은 머티리얼 액터들을 영구 1개 메시로
- **HISM (Hierarchical Instanced Static Mesh)** 인스턴싱 확장
- 메시 LOD0 폴리곤 직접 단순화 (모바일 권장: 빌딩당 5,000~10,000 폴리곤)
- Material Instance 통합으로 DrawCall 배칭 활용

---

## 모바일 출시 전 필수 — PSO Cache (Pipeline State Object)

**왜 필수**: 모바일 GPU가 새 셰이더를 처음 본 순간 런타임 컴파일 → **300~1000ms hitch**. 사용자는 "버그/끊김"으로 인식. FPS 평균보다 게임 평가에 더 영향. 이 게임처럼 머티리얼 변형이 다수(빌딩 스킨 / 업그레이드 단계 / 가챠 연출)면 hitch 발생 빈도 매우 높음.

**해결**: Bundled PSO Cache — 디바이스에서 PSO 한 번 수집 → 빌드에 사전 컴파일된 PSO 포함 → 런타임 컴파일 0.

**작업 흐름**:
1. Project Settings → Rendering → "Shader Pipeline Cache" 활성화
2. Development 빌드를 디바이스에서 실행, 모든 화면/빌딩/스킨/가챠 연출 한 번씩 표시 (PSO 수집)
3. `Saved/CollectedPSOs/` 에 생성된 `.upipelinecache` 파일을 프로젝트의 `Build/<Platform>/PipelineCaches/` 로 복사
4. 다음 Shipping 빌드 시 자동 통합

**측정**: PIE 콘솔 `r.PSOWarmup.Verbose=1` 로 hitch 위치 확인.

**문서**: UE5 Bundled PSO Cache (Unreal Engine 공식)

**적용 시점**: 모바일 알파/베타 빌드 직전 (개발 중에는 불필요, 배포 빌드에만 의미).

---

## 미적용 후보 — 측정 결과에 따라 적용 결정

### Phase 1-B: 시각 영향 있는 ini (GPU bound 시 우선)

| 키 | 변경 안 | 효과 추정 | 시각 영향 |
|---|---|---|---|
| `r.MobileHDR` | `1 → 0` | GPU 25~40% 회복 | 색감 평탄화 가능 |
| `r.MSAACount` | `4 → 2` | GPU 절반 회복 | 거의 없음 |

**적용 기준**: `stat unit`에서 GPU 시간이 16ms 초과 + Game/Draw은 양호한 경우.

### Phase 6: 1순위 EmployeeBehaviorComponent (Game bound 시 우선)

**파일**: `Source/CompanyGrowthRenewal/Private/Entity/Officeworker/EmployeeBehaviorComponent.cpp:108-194`

**문제**:
- L113: `TickBuffs` 매 프레임 (10Hz로 충분)
- L171-188: `bMovingToWorkstation` 동안 매 프레임 `Cast<AOfficeworker>` + `Cast<AAIController>` + `PathFollowingComponent::GetStatus()` 폴링

**해결 방향**:
1. PathFollowing 폴링 → `AAIController::ReceiveMoveCompleted` 델리게이트 이벤트화 (UE 표준)
2. `BuffTickAccumulator` 도입, 0.1s 주기로 `TickBuffs(0.1f)` 호출

**효과 추정**: 직원 200명 기준 5~10ms 회복

**위험**: 이벤트 누락 시 직원이 워크스테이션에 도착해도 다음 행동 진입 안 함. 적용 후 5분 PIE 관찰 필요.

### Phase 7+: 이전 세션에서 진단된 추가 후보 (적용 보류 중)

#### 직원 13개 부분 메시 AnimBP 비활성화

**파일**: `Officeworker.cpp:161-173 PostInitializeComponents`

```cpp
HairBase->SetLeaderPoseComponent(Face);
HairBase->SetAnimationMode(EAnimationMode::AnimationCustomMode);  // ← 추가
```

**효과**: 50명 시 600+ AnimInstance Tick 제거, 10ms+ Game Thread 절감

**주의**: 이번 세션의 #4(VisibilityBasedAnimTickOption)와 효과 일부 겹침. VisibilityTick은 화면 밖에서만 효과, AnimationCustomMode는 항상 효과. 둘 다 적용 가능.

#### 빌딩 ISM 평소 Static + Material WPO 흔들림

**파일**: `BuildingBaseActor.cpp:41-60`

**현재**: Body/Top/Top_Empty Module 셋 다 Movable.

**제약 (사용자 확인됨)**:
- 건설 시: 위치 이동 필요 (Movable)
- 이동 모드: 빌딩 옮길 때 (Movable)
- 클릭 흔들림: 시각만 흔들림 (Material WPO로 Static 유지)
- 사용자 강조: "본인이 관리 안 하는 건물은 Movable일 필요 없음"

**해결 방향**:
1. 생성자에서 ISM 3개 기본 Static
2. 건설/이동 모드 진입 시 `SetMobility(Movable)`, 완료 시 `SetMobility(Static)`
3. 클릭 흔들림은 Material WPO로 처리 (`ShakeIntensity` 스칼라 파라미터 + `WPO = ShakeIntensity × sin(Time × Frequency) × VertexNormal`)

**효과**: GPU 15~25% 회복

**주의**: Material 수정 필요 — 디자이너 협업 또는 마스터 머티리얼 직접 편집

#### EmployeeAnimInstance Velocity 캐싱 (10Hz)

**파일**: `EmployeeAnimInstance.cpp:44-46`

매 프레임 `GetOwner()->GetVelocity()` 호출 → 0.1초마다 캐시 갱신으로 변경.

**효과**: 50명 × 0.06ms = 3ms 절감

#### Officeworker MID 캐싱

**파일**: `Officeworker.cpp:554-562 SetMaterialParameter`

색상 변경마다 `CreateAndSetMaterialInstanceDynamic` 새로 호출 → 스폰 시 한 번만 생성, 이후 `SetVectorParameterValue` 호출.

**효과**: 색상 변경 시 프레임 스파이크 제거

### Phase 9 후보 — 전수 조사 감사 잔여 (2026-06-02, 우선순위 순)

**효과 큰 워커 핫패스 (invalidation 훅 설계 필요 → 신중히)**:
- **Officeworker::Tick `bIsRoaming` 폴링** (`Officeworker.cpp:306-318`): 매 프레임 `Cast<AAIController>` + `GetPathFollowingComponent()->GetStatus()` + `IsTimerActive()` → UE `PathFollowingComponent::OnRequestFinished` 델리게이트 패턴으로 전환. **성공/실패 결과 양쪽**에서 `OnMoveCompleted` 호출 필수(NavMesh 경로 실패 시 워커 영구정지 방지). Phase 6의 EmployeeBehaviorComponent 폴링 항목과 같은 fix 계열, 별개 위치.
- **EmployeeBehaviorComponent::GenerateIncome** (`L453-494`): non-Operation 분기가 매 프레임 `GetInstance` + `GetSubsystem<UEmployeeManager>` + `FindEmployee`로 거의 불변인 Level/EnhancementLevel만 읽음 → 컴포넌트에 캐시. **주의**: `EnhanceEmployee`(EmployeeManager.cpp:639)가 broadcast 안 함 → enhance/spawn/manage-apply 경로에 캐시 invalidation 훅 추가 필수(`OnExperienceGained`만으론 EnhancementLevel 변경 못 잡음).
- **EmployeeManager::FindEmployee O(n)** (`L363-373`): income/score 틱마다 워커별 선형 스캔, 집계 O(workers × EmployeeList.Num())/frame → `TMap<int32,int32> EmployeeIDToIndex`로 O(1). mutation 6곳(Add L77/L137/L1145, RemoveAt L180, RemoveAll L1119/L1189) 동기화 필요(RemoveAt/RemoveAll은 인덱스 시프트 → stable keying 또는 rebuild).

**버그/정리**:
- **GetOfficeSaveDataByBuildingID `FindOrAdd` → `Find`** (`ProjectOperationManager.cpp:79`): read-only 호출 경로의 유령삽입 제거. 8개 호출처(L43 삭제됨, L249/305/339/378/419/834/857)가 nullptr 처리 가능한지 + operation 엔트리 lazy 생성에 의존하는지 추적 후 적용.

**저영향 (측정/판단 후 결정)**:
- 워커 버블/얼굴 이모지 `LoadSynchronous`를 office 로드 시 1회 pre-resolve (`Officeworker.cpp:1164` / `StickOfficeworker.cpp:114`) — first-touch hitch만 제거(steady-state는 이미 cheap resolve).
- WorldMap 국가 이름표를 카메라 이동 시에만 reposition (현재 Phase 8 #5는 hoist까지만 적용).
- Config: `r.Mobile.AntiAliasing` 변경은 Phase 1-B(`r.MobileHDR=0`/`r.MSAACount=2`)와 **묶어서만** 의미(MobileHDR=1이면 on-tile MSAA 불가). `s.AsyncLoadingThreadEnabled`(IoStore라 EDL inert, ALT만 소폭 game-thread offload). GC 튜닝은 UE5.4 기본값과 대부분 겹침 → `stat gc` 측정 후.

**감사 기각 항목 (재제안 금지)**: EmploymentPanelWidget NativeTick(아무도 push 안 하는 고아 dead code), iOS PoolSize/Mobile AO/PixelProjectedReflection(디바이스 프로필에서 이미 inert), 120Hz soft cap(perf 아닌 UX 변경).

### Phase 10 후보 — MainMap 모바일 감사 잔여 (2026-06-24, 우선순위 순)

7차원 감사 43건 confirmed 중 TrafficManager(Phase 10 #1 적용) 외 잔여.

**#0 진단 — 코드 손대기 전 먼저**:
- 낮은 폰이 실제 매칭되는 디바이스 프로파일 확인(`dp.Override Android_Low` 강제 테스트, 또는 logcat 프로파일 로그). 스톡 매칭 규칙이 후해서 **약한 폰이 `Android_High`에 매칭되면** 아래 `Android_Low` override가 적용조차 안 됨 → 매칭 자체가 렉 원인일 수 있음.

**DO FIRST (신규·저위험·고효과)**:
- **CityPlotActor 반투명 틴트 오버드로** (`CityPlotActor.cpp` SetupPlotTint): 미소유 부지마다 블록 메시를 통째 복제해 0.5알파 반투명 MID로 덮음 → 블록당 메시 1장 추가 + 대면적 반투명 overdraw. 수정=풋프린트 크기 **평면 쿼드**로 교체(멀티섹션 메시 복제 제거). **시각 트레이드오프**: 깎인 블록 모서리 안 따라감(쿼드=bbox 사각형) → 사인 필요. ⚠️ `M_PlotHighlight`는 PlacementHandler 배치 프리뷰와 **공유** → 머티리얼 blend mode 건들지 말고 plot 컴포넌트만 교체.
- **스카이라인 ~112 낱개 StaticMeshActor → HISM 머지** (`Content/TheRiverwalkCity/LevelInstance`): 반복 타일 미인스턴싱. 본 문서 "대안 — 효과 있는 균일 최적화(Static Mesh Merge / HISM)" 전략의 구체적 대상. ⚠️ `Street_D_01/G_01`은 CityPlotActor가 낱개 actor로 의존(line trace + 경로매칭)→**제외**. RoadMain/SideWalk/WhitePaint는 이미 ISM. 에디터/Python 작업.
- **저사양 디바이스 프로파일 신설** (`DefaultDeviceProfiles.ini` / `DefaultEngine.ini`): 현재 스톡 엔진 템플릿뿐(프로젝트 [Android] override 없음). 제안:
  - `DefaultEngine.ini [/Script/Engine.RendererSettings]`: `r.Mobile.EnableMovableLightCSMShaderCulling=True`(현재 False — CSM 범위 밖 draw가 싼 셰이더. **read-only cvar이라 디바이스 프로파일 +CVars 불가, 반드시 ini RendererSettings에**. 그림자 켜진 Mid/High에서만 의미).
  - `[Android_Low]`: `+CVars=r.MSAACount=2`(Phase 1-B 승인, "거의 없음" 시각 — cvar은 `r.MobileMSAA`가 아니라 `r.MSAACount`가 실재) / `+CVars=cg.Traffic.MaxCars=20` / `+CVars=r.Streaming.PoolSize=400`(600→).
  - `[Android_Mid]`: `+CVars=cg.Traffic.MaxCars=35`.

**저위험 정리**:
- **VolumetricCloud actor가 MainMap에 1개** → 삭제 또는 `r.VolumetricCloud=0`(하늘은 `SM_TimeCycleSkySphere` 돔 + `MI_Complex_CloudyCool`이 그림). 모바일 forward에선 거의 렌더 안 될 수 있어 데드 씬 정리 성격(폰 GPU 절감은 기대 낮음, 에디터/PIE는 절감).
- **Boat `AmbientSplineMover` `TickInterval=0.05~0.1f`**(150cm/s라 100ms 스텝=15cm, 무해). **단 TrafficManager Tick은 건들지 말 것**(1200cm/s, 20배).
- **디버그 로그 시핑 잔존**: `ResourceItemManager.cpp:130/167`(store/spend마다 `UE_LOG(Warning)`, 브로드캐스트 전 fire). Verbose 강등 또는 제거.
- **`GachaCaptureStage.cpp` OnConstruction `bCaptureEveryFrame=true` 가드**: `if (GetWorld() && !GetWorld()->IsGameWorld())` (※ `GIsEditor` 게이트는 쿡 빌드에서 무용 — 검증자 정정). 현재 MainMap 0개라 잠복.
- **MoneyVFX Niagara `ENCPoolMethod::AutoRelease`**(`EmployeeBehaviorComponent.cpp:1395`) — **단 OfficeMap 코드**(MainMap 무관, 풀링만 적용·frustum cull 불필요).

**WIP 충돌로 보류** (2026-06-24 기준 커밋 안 된 작업 존재): `InGameLayerWidget`(5s 버블 재평가 — subsystem/`FDateTime::Now()` 루프 밖 hoist + HUD tick 게이팅), `BubbleContainerWidget`(`160-168` 죽은 zoom 보정 블록 `Lerp(0,0)=0` 삭제). plot-price-chip/버블 리팩토링 진행 중이라 위에 덮지 않음 — WIP 정리 후 재평가.

**감사 기각 / 기존 결정과 충돌(재제안 금지)**:
- **빌딩 per-object cull distance**(`BuildingBaseActor` `bNeverDistanceCull=true`): 감사는 원거리 컬링 제안했으나 본 문서 "카메라 시점 제약 — 거리 기반 최적화 무용"과 충돌 → **보류**(광역 시야라 모두 화면 안).
- **VSM off / 그림자 거리 축소 / MaxZoom 축소**: 기존 롤백·거부 결정 유지.
- **VSM 동작 정정(중요)**: `r.Shadow.Virtual.Enable=1`은 **데스크톱 에디터(SM6 deferred)에서만** 동작. 실제 안드로이드(ES3.1 forward, `r.Mobile.ShadingPath=0`)는 Nanite 게이트(`r.Nanite.ProjectEnabled=False`)로 **VSM 미동작 → Mobile CSM** 사용. 즉 회귀 사례 3의 "shadow swimming"은 **에디터 시각 기준**(롤백 유효), 폰 그림자 비용은 별개(Mid/High tier; `Android_Low`는 `sg.ShadowQuality=0`이라 이미 그림자 거의 없음). 폰 그림자 최적화는 Mobile CSM / `SetCastShadows` 기준으로 판단할 것.
- **감사 오탐(census 아티팩트 등)**: fog/PPV "각 2개"(실제 각 1개 — `rg --text -c`가 바이너리 umap 가짜 라인 카운트), 빌딩스킨 35텍스처 NeverStream(메커니즘 오류, UI 그룹 27MB), 워커 얼굴 텍스처 TC_EditorIcon(미사용 파일 오인 — 실제 사용 56개 중 0개).

---

## 검증 절차

### 빌드

```powershell
powershell -Command "& '${UE_ROOT}\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' CompanyGrowthRenewalEditor Win64 Development '-Project=${PROJECT_ROOT}\CompanyGrowthRenewal.uproject'"
```

**주의**:
- 빌드 전 에디터 종료 (Live Coding 충돌 방지) 또는 에디터에서 `Ctrl+Alt+F11`로 Live Coding 사용
- 헤더 변경 시 Live Coding 불가 → 풀 빌드 필요
- 생성자 변경은 Live Coding으로 적용 가능하지만 **이미 spawn된 인스턴스에는 적용 안 됨** → PIE 재시작 필수

### 측정

PIE에서 콘솔(`)에 입력:
```
stat unit
stat fps
```

30초 안정화 후 4가지 시간 확인:

| 분류 | 의미 | 16.67ms 초과 시 다음 단계 |
|---|---|---|
| Frame | 전체 프레임 시간 | 16.67ms 이하면 60FPS 달성 |
| Game | 게임스레드 (Tick, AI, 이벤트) | Phase 6 (EmployeeBehavior) 적용 |
| Draw | 렌더 스레드 (DrawCall 큐잉) | DrawCall 절감 (ISM 머지 등) |
| GPU | GPU 시간 | Phase 1-B (Mobile HDR / MSAA) 적용 |

### 회귀 체크리스트

각 변경 적용 후 5분 PIE에서:
1. 빌딩 클릭 → Wobble 정상 동작 → 끝나면 원래 스케일로 복귀
2. 클릭 안 했을 때 빌딩 모션 멈춤 없음 (`bCanEverTick=false` 회귀 재발 방지)
3. 공장 인터랙션 → bouncing → 종료 시 스케일 복원
4. 일출/일몰 (5~7시, 18~20시) 색상 끊김 없는지
5. 직원이 화면 밖 → 안으로 들어올 때 자세 자연스러운지 (VisibilityTick 부작용)
6. 빌딩 위 버블 위치가 카메라 따라 정상 추적

---

## 다음 세션 시작 시 우선 참조

1. 이 문서 (적용 완료/미적용/금지/회귀 모두)
2. 메모리 `project_perf_settings.md` (요약 인덱스)
3. CLAUDE.md "구현 전문가 / 코드 리뷰어 에이전트 사용" 규칙
4. `Config/DefaultEngine.ini` 현재 ini 값 확인 (수정 시 반드시 이 문서 갱신)

## 변경 이력

- 2026-05-02 (이전 세션): Phase 1-A ini 4개 + Phase A~E 코드 진단
- 2026-05-02 (현재 세션): Phase 2~5 코드 5개 적용 + On-demand Tick 패턴 + 회귀 사례 영구 보존
- 2026-05-02 (추가): "카메라 시점 제약 — 거리 기반 최적화 무용" 섹션 추가 (HLOD/거리 LOD/Cull Distance/URO 다시 제안 금지) + "PSO Cache 모바일 출시 전 필수" 섹션 추가
- 2026-06-02: Phase 8 전수 조사 감사 1차 적용 (DO FIRST 6건 + 잠복 버그 3건, UBT exit 0 검증). Phase 9 후보(워커 핫패스 A-1/A-3/A-4 + FindOrAdd 정리 + 저영향 config) 등재. 감사 기각 항목 재제안 금지 목록 추가.
- 2026-06-24: 7차원 MainMap 모바일 감사(43건 confirmed + 적대적 검증). **Phase 10 #1 TrafficManager** 30Hz 스로틀 + 숨김 차량 인스턴스 쓰기 스킵 + `cg.Traffic.MaxCars` cvar + `bDebugLog` off 적용(UBT exit 0, PIE 검증 대기). Phase 10 후보 등재(CityPlot 반투명 오버드로 / 스카이라인 HISM 머지 / 저사양 디바이스 프로파일 / VolumetricCloud / boat TickInterval 등). **VSM 폰 미동작 정정**(에디터=VSM, 폰=Mobile CSM) + 디바이스 프로파일 매칭 진단(#0) 추가.
