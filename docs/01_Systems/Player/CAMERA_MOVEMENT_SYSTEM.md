# 카메라 / 플레이어 이동 시스템 (CAMERA & MOVEMENT) — SOT

> MainMap(도시뷰)의 플레이어 카메라 이동·줌·회전·드래그·포커싱 단일 진실 소스.
> 이동/회전 "둔함·반대" 같은 **체감(feel)** 작업, 카메라 포커싱 추가, 입력 모드 작업 전 우선 참조.
> 최초 정리: 2026-06-30 (체감 튜닝 패스) · 갱신: 2026-08-23(OfficeMap 최초 고층 외관 프레이밍).
> 값은 **커밋 완료**(`Private/Player/PlayerCamera.cpp` = `3474295d`, 2026-07-04) · 여전히 **튜닝 가능** 상태.

---

## 1. 구성 (액터 / 컴포넌트)

플레이어 폰 = **`APlayerCamera`** (`Private/Player/PlayerCamera.h/.cpp`), `APawn` 상속.

```
APlayerCamera (Root SceneComponent)
├─ USpringArmComponent  "SpringArm"     ← 카메라 팔(줌=팔길이, 피치, 랙)
│   └─ UCameraComponent "MainCamera"    ← 실제 뷰 (FOV)
├─ UFloatingPawnMovement "MovementComponent"  ← PC 이동 물리(가속/감속/최고속)
├─ USphereComponent     "Collision"     ← 지면 위치 체크용(이동 X)
├─ UMovementInputHandler  ← 이동/줌/회전/드래그 입력 전부 담당 (ActorComponent)
├─ UInteractableInputHandler ← 건물/책상 클릭·홀드
└─ UPlacementHandler         ← 배치(건설) 모드
```

- **상속 폰**: `AOfficeCameraPawn`(오피스), `AWorldMapCameraPawn`(월드맵)이 `APlayerCamera`를 상속해 **같은 `UMovementInputHandler` 코드를 공유**한다 → 한 곳을 고치면 세 맵에 전파됨(§7 오버라이드 표 참조).
- 컨트롤러: `AMainMapPlayerController` (`Private/Player/MainMapPlayerController.h/.cpp`). 입력 모드 + 모바일 레거시 터치 이벤트 바인딩 담당.

---

## 2. ⚠️ 가장 중요한 비자명 사실 — 이동 입력 경로가 플랫폼별로 다름

| 플랫폼 | 이동 입력 | 코드 경로 | 물리 |
|---|---|---|---|
| **PC (키/패드)** | `IA_Move` | `MovementInputHandler::Move()` → `AddMovementInput` | **`UFloatingPawnMovement`** (Acceleration/Deceleration/MaxSpeed) |
| **모바일 (터치)** | `IA_DragMove` | `OnDragMove()` → `TrackMove()` → `AddActorWorldOffset` | **무브먼트 컴포넌트 미사용** (그랩-월드 직접 이동) |

> **함정**: `UFloatingPawnMovement`의 `Acceleration/Deceleration` 튜닝은 **모바일에 영향 0**이다(PC 전용). 모바일 드래그 체감은 `TrackMove`의 **스무딩 Lerp + 데드존**이 지배한다.
>
> **회전(Spin)과 카메라 랙(SpringArm)은 플랫폼 공유** → PC·모바일 양쪽에 적용된다.

---

## 3. 입력 액션 / 매핑 컨텍스트

`UMovementInputHandler` 생성자에서 `ConstructorHelpers`로 하드 로드 (`/Game/CompanyGrowth/Input/`):

| 에셋 | 바인딩 (`SetupPlayerInputComponent`) | 콜백 |
|---|---|---|
| `IMC_BaseInput` | priority 0 (상시) | — |
| `IMC_DragMove` | `InitDragMoveIMC()`에서 추가 (priority `MOVEMENT`) | — |
| `IA_Move` | `Triggered` | `Move()` |
| `IA_Zoom` | `Triggered` | `Zoom()` |
| `IA_Spin` | `Triggered` | `Spin()` |
| `IA_DragMove` | `Started`/`Triggered` | `OnDragStarted()` / `OnDragMove()` |

- 모바일은 `MainMapPlayerController::SetupInputComponent`에서 **레거시 `BindTouch`**(Pressed/Released/Repeat)도 추가로 바인딩 → 탭/홀드/드래그 판정(`InteractableInputHandler`로 전달).
- IMC/IA는 **바이너리 `.uasset`** 이라 모디파이어(Negate/Scalar/Smooth/DeadZone)는 코드에서 안 보임 → 에디터에서 직접 확인해야 함(§6 참조).

---

## 4. 서브시스템별 동작

### 4.1 PC 이동 — `Move()`
- `AddMovementInput(ActorForward, Value[1])` + `AddMovementInput(ActorRight, Value[0])` (카메라 기준 상대 이동).
- 경계(`BoundaryRadius`) 근처에서 중앙 반대 방향이면 `resistanceFactor`로 점진 저항(soft zone = 0.9~1.1 R).
- 입력 시 `StopCameraTransition()`로 진행 중인 포커스 글라이드 취소.

### 4.2 모바일 드래그 팬 — `OnDragStarted` / `OnDragMove` / `TrackMove`
- `OnDragStarted` → `PositionCheck()`: 손가락 밑 지면점을 `TargetHandle`에 저장(그랩 기준점).
- `OnDragMove`: 싱글터치일 때만 `TrackMove()`. 멀티→싱글 전환 시 `TargetHandle` 재설정.
- `TrackMove()`: `newMove = TargetHandle - IntersectionPos - offset` (그랩한 지면점이 손가락에 오도록 폰 이동). 데드존·스무딩·경계 저항 후 `AddActorWorldOffset`.

### 4.3 줌 — `Zoom()` / `UpdateZoom()` / `ApplyZoomSettings()`
- `Zoom()`: `ZoomDirection = Value[0]`, `UpdateZoom()`이 `ZoomValue += dir*0.01` (clamp 0~1).
- `ApplyZoomSettings()` = **줌 한 값으로 팔길이·피치·최고속·가속/감속·FOV 동시 적용**(아래 식). `C_Zoom` 커브로 `lerpKey` 산출.

```
lerpKey = C_Zoom(ZoomValue)              // 0=가까이, 1=멀리
TargetArmLength = Lerp(MinZoom, MaxZoom, lerpKey)
SpringArm.Pitch = Lerp(ZoomInPitch, ZoomOutPitch, lerpKey)
NewMaxSpeed     = Lerp(8000, 48000, lerpKey)
MaxSpeed        = NewMaxSpeed
Acceleration    = NewMaxSpeed * 6        // ← feel 튜닝 (§5)
Deceleration    = NewMaxSpeed * 6        // ← feel 튜닝 (§5)
FOV             = Lerp(ZoomInFOV, ZoomOutFOV, lerpKey)
```
- 리그 평가는 순수 함수 `UMovementInputHandler::EvaluateZoomRig(ZoomValue)`(= `CameraFramingMath::EvaluateRig`)로 분리. `ApplyZoomSettings`는 그 결과를 적용만 한다. 프레이밍 탐색은 컴포넌트를 건드리지 않는다.

### 4.4 회전 — `Spin()`
- `Owner->AddActorLocalRotation(FRotator(0, -Value[0], 0))` — **폰(=리그) yaw 회전**.
- 부호 `-` 는 "리그가 돌면 화면(월드)이 반대로 보이는" 반전을 보정한 것(§5/§6).
- 카메라가 긴 팔 끝에 있어 회전이 큰 호를 그림 → **위치 랙에도 걸림**(§5 카메라 랙).

### 4.5 경계 유지 — `MoveTracking()` (타이머 0.0166s ≈ 60Hz)
- 경계 밖이면 중앙으로 당김(안쪽이면 스케일 0 = 무동작).
- KeyMouse일 때만 `Collision`을 지면 교차점으로 이동(클릭 체크용).

### 4.6 카메라 포커싱 API (재사용 — 새로 짜지 말 것)
`APlayerCamera` 공개 메서드. `Tick`에서 `VInterpTo`/`FInterpTo`(`CameraTransitionSpeed=3.0`)로 부드럽게 전환. 입력 들어오면 `StopCameraTransition()`로 취소.

| API | 용도 |
|---|---|
| `FocusOnBuilding(Building, ...)` | 건물 높이 비례 프레이밍. 순수 리그 모델 위 1-D 이분법(`CameraFramingMath::SolveZoomForHeight`, 잔차 ≤1cm)으로 줌값 역산. 반환=최종 팔길이 |
| `FocusOnActor(Actor, ScreenCenterRatioX, bTrackOcclusion=true)` | 바운딩박스 기반 범용 포커스. **`bTrackOcclusion=false`면 가림 고스트를 안 켠다**(로딩 뒤 초기 포커스·배치 모드용) |
| `FocusOnLocation(Loc, Dist)` | 특정 위치로 이동(+선택 줌) |
| `FocusOnAreaRadius(Center, R)` | 반경 R 원을 화면에 담기(MaxZoom 임시 확장) |
| `FocusOnBuildingOrAura(...)` | 건물 vs 영향권 반경 중 더 줌아웃 |
| `FocusOnBuildingForPlacement(...)` | 포커스 + 배치 IMC 복원 |
| `StopCameraTransition()` | 전환 중단 |
| `ClearFocusTarget()` | 포커스 종료 — 가림 고스트 해제. **패널 `NativeOnDeactivated`에서 `GoToNormalMode()`와 쌍으로 호출 필수** |
| `RestoreFocusTarget(Actor)` | **카메라를 안 움직이고** 가림 판정만 재등록. 자식 위젯 push로 비활성화됐던 패널이 `NativeOnActivated`에서 고스트를 되살릴 때 (`FocusOn*` 재호출은 카메라를 또 글라이드시킴). `ClearFocusTarget()`의 짝 |

**가림 고스트(2026-08-08)**: 포커스 중 카메라와 타깃 사이를 막는 건물은 `UFocusOcclusionHandler`가 0.1초마다
`ECC_GameTraceChannel1`(건물 클릭 박스와 같은 채널) 라인트레이스 5발로 검출해, 본체 메시를 숨기고 반투명 골조 박스
(`M_BuildingGhost`)로 대체한다. 트레이스는 `LineTraceMultiByChannel` + 조회자 응답 `ECR_Overlap`이라 최근접 1채에서
끊기지 않고 뒷줄까지 모으며, 카메라 거리순 정렬 후 `MaxGhostCount`(5)로 자른다.
해제는 3중 — ① `ClearFocusTarget()` 명시 호출 ② 타깃 `TWeakObjectPtr` 무효화 ③ 입력 모드가 `Normal`로 돌아오면 자동.
③ 때문에 **패널을 열지 않는 포커스(예: 철거 관전)에는 고스트가 사실상 안 걸린다**.
복원은 스냅샷이 아니라 상태 출처(`bGlowVisible`/`bAuraHighlightOn`/`bKeystoneRingOn`)에서 재계산하므로, 고스트 중
낮/밤이 바뀌어도 옥상 비콘·아우라가 맞는 상태로 돌아온다.
설계 = `docs/superpowers/specs/2026-08-08-focus-occlusion-ghost-design.md`,
PIE 검증 체크리스트 = `docs/superpowers/specs/2026-08-08-focus-occlusion-ghost-pie-checklist.md`.
⚠ 해제를 빠뜨리면 건물이 세션 내내 숨겨진 채 남는다(안전망 ③이 마지막 방어선).

### 4.7 OfficeMap 최초 고층 외관 프레이밍

`AOfficeCameraPawn`은 `USaveLoadManager::OnGameDataLoaded` 뒤 최초 1회 `ApplyInitialOfficeFraming(false)`를 실행한다. 로드 전 기본 `1×2`가 아니라 실제 복원된 footprint를 기준으로 잡기 위해 BeginPlay 즉시 실행하지 않는다. `bForce=true`는 실제 PIE 캡처처럼 같은 Pawn으로 여러 검증 상태를 순회할 때만 사용하며, 제품 확장 경로에서는 호출하지 않는다.

- `AOfficeInterior::GetCurrentFloorBoundsLocal()`에서 개방 코너 절반과 파사드 첫 `400cm` 깊이를 hero box로 만든다.
- 실제 SpringArm 피치·팔길이, Camera FOV, viewport aspect를 각 줌 샘플에 적용해 8개 모서리가 정규화 화면 안에 드는 가장 가까운 줌을 찾는다. 구현 탐색은 `0.06~0.94`를 사용해 외부 검증 계약 `0.05~0.95`에 1% 수치 완충을 둔다. 전역 단조성을 가정하지 않고 32구간 순차 탐색 뒤 첫 통과 구간만 8회 정제한다.
- 선택 결과는 기존 `FocusOnLocation()`으로 한 번만 부드럽게 적용하며, 탐색 중 임시 변경한 줌/컴포넌트 상태는 항상 복원한다. 어떤 샘플도 맞지 않으면 Office 최대 팔길이 `5000cm`를 안전 폴백으로 쓴다.
- **확장 델리게이트에는 바인딩하지 않는다.** 사무실 확장 시 외관만 즉시 갱신하고 현재 플레이어 카메라는 강제로 이동하지 않는다.

---

## 5. 체감(feel) 튜닝 레퍼런스 ★

> "이동/회전이 둔함·칼각·반대" 문제의 노브. 전부 코드 상수(런타임 비례 계산) — 값 바꾸면 **풀빌드 필요**(§6 Live Coding 함정).

| 증상 | 노브 | 위치 | 현재값 | 방향 |
|---|---|---|---|---|
| PC 이동이 둔함/칼각 | `Acceleration`·`Deceleration` 배수 | `MovementInputHandler.cpp` `ApplyZoomSettings()` | `MaxSpeed × 6` (≈0.17s 이즈) | ↑=칼각·즉각, ↓=부드럽·관성 |
| 모바일 드래그가 둔함 | 스무딩 `Lerp` alpha | `MovementInputHandler.cpp` `TrackMove()` | `0.8` | ↑(→1.0)=손가락 밀착, ↓=떨림억제 |
| 느린 드래그 끈적임 | 데드존 | `TrackMove()` | `8.0` | ↓=더 민감 |
| 팬·회전 잔상(전 플랫폼) | `CameraLagSpeed`·`CameraRotationLagSpeed` | `PlayerCamera.cpp` 생성자 | 둘 다 `22` (tau≈0.045s) | ↑=즉각, ↓=잔상↑. `false`=랙 끔 |
| 회전 방향 반대 | Spin yaw 부호 | `MovementInputHandler.cpp` `Spin()` | `-Value[0]` | 반대로 느껴지면 `Value[0]`로 원복 |

**원리**:
- `Acceleration` = 출발 가속률, `Deceleration` = 정지 감속률. `MaxSpeed`는 **상한 클램프일 뿐 반응성 못 만듦**. 둘 다 `MaxSpeed` 비례라 줌 단계와 무관하게 체감 일정.
- (배경) 엔진 기본값 `Accel 4000/Decel 8000`을 방치하면 `MaxSpeed` 최대 48000에서 **램프 ~12s·정지 드리프트 ~250m** = 둔함의 주범이었음.
- 카메라 랙은 SpringArm이 목표를 `QInterpTo(speed)`로 쫓는 시간상수(≈1/speed). 10이면 0.1s라 둔함 → 22로 상향.

---

## 6. 알려진 함정 / 미해결

1. **`Acceleration` 튜닝 ≠ 모바일** — §2. 모바일은 `TrackMove`를 만질 것.
2. **Live Coding 핫리로드가 `.voltbl`로 깨짐** — cpp만 바꿔도 `Cannot find image section .voltbl` + `pipe 0xEA`로 패치 실패(MSVC↔UE5.4 버그, 코드 무관·`.obj`는 정상). → **에디터 닫고 풀빌드**가 정답. 진짜 에러는 에디터 로그 아니라 `${UE_ROOT}\Engine\Programs\LiveCodingConsole\Saved\Logs\LiveCodingConsole.log`.
3. **숨은 `Smooth` 모디파이어** — IA_Move/IA_Spin은 코드측 모디파이어가 없으므로, `IMC_BaseInput`에 `Smooth`(저역통과)·`Scalar`·`DeadZone`이 붙으면 가속·랙과 무관한 floaty 원인이 됨. 에디터에서 확인.
4. **긴 SpringArm 팔 → 회전 "스윔"** — Spin이 폰 원점 기준 yaw라(팔 320000+socket -1200) 카메라가 큰 호를 그림. 더 깊은 개선은 화면 중앙 지면점을 피벗으로 회전.
5. **망원 FOV(20~30°)** 가 잔여 랙·모션을 증폭 → 같은 랙도 더 무겁게 보임. (`FocusOnBuilding`은 핸들러 Zoom Settings를 읽으므로 FOV/피치 밴드 변경은 그 값만 바꾸면 된다.)
6. `PlayerCamera.cpp` 생성자의 `SetFieldOfView(25)`는 `ApplyZoomSettings`가 즉시 덮어쓰는 **데드 코드**.
7. **모바일 실검증**: PC PIE 키보드로는 모바일 경로(드래그)가 안 돌아감. PIE 모바일 미리보기/터치 에뮬, 또는 **APK 재패키징·배포**(에디터 빌드만으론 폰 반영 X) 후 확인.
8. **수직 FOV는 FieldOfView가 아니다** — 엔진 기본 제약 MaintainYFOV에서 `tan(vFOV/2) = tan(FieldOfView/2) / CameraComponent.AspectRatio`(CameraStackTypes.cpp). FOV 30°의 수직 화각은 약 17°. 세로 점유율 계산에 `tan(FOV/2)`를 그대로 쓰면 1.78배 틀린다(2026-09-06 정정, 이전엔 분모의 2 누락과 상쇄돼 티가 안 났음).

---

## 7. 폰별 오버라이드 (상속 차이)

| 폰 | 차이 |
|---|---|
| `APlayerCamera` (MainMap) | 기준값. Min/Max 줌 1600/320000, Boundary 100000, 피치 -40/-55, FOV 30/20, 랙 22/22 |
| `AOfficeCameraPawn` | 줌 20/5000, Boundary 3000, 피치 -32/-48, FOV 40/30(코지), **Yaw 90~180° 클램프**, 로드 완료 후 최초 1회 외관 프레이밍, 벽편집 회전/직원 추적. Collision 72 |
| `AWorldMapCameraPawn` | 줌 5000/200000, **`CameraLagSpeed=8` 자체 오버라이드(더 큰 랙)**, **Spin·Placement·Interactable 비활성**(같은 코드지만 미바인딩) |

> WorldMap은 "좋은 레퍼런스"가 아님 — 같은 핸들러 공유 + 오히려 랙이 큼. 체감 개선은 공유 베이스(`APlayerCamera` + `UMovementInputHandler`)에서 할 것.

---

## 8. 입력 모드 (`MainMapPlayerController::EInputMode`)

`Normal` / `BuildPlace` / `BuildingClick` / `Factory` / `UI`. `UI` 모드에선 `OnTouchPressed`가 월드 입력을 무시 → 패널 닫을 때 `GoToNormalMode()` 쌍 필수(상세는 CLAUDE.md "UI/Normal 입력 모드 전환 규칙").

---

## 9. 파일 맵 (SOT)

| 파일 | 책임 |
|---|---|
| `Private/Player/PlayerCamera.cpp` | 폰·SpringArm·랙·카메라 포커싱 API·Tick 전환 |
| `Private/Player/Components/MovementInputHandler.cpp` | 이동/줌/회전/드래그 입력·`ApplyZoomSettings`·경계·feel 노브 |
| `Private/Player/Components/FocusOcclusionHandler.cpp` | 가림 고스트 판정(트레이스·상한·페이드 Tick)·해제 안전망. 표현은 `ABuildingBaseActor::SetOccluderGhost` |
| `Private/Player/MainMapPlayerController.cpp` | 입력 모드·모바일 터치 이벤트 |
| `Private/Player/OfficeCameraPawn.cpp` / `WorldMapCameraPawn.cpp` | 맵별 오버라이드 |
| `Private/Player/OfficeCameraFraming.cpp` | Office hero box·수평 FOV 투영·5% safe-frame 줌 탐색 순수 계산 |
| `Util/CoordinateUtils.*` | 화면→지면 투영(드래그·클릭 공용, O(1), 부호반전 없음) |
| `Public/Player/Components/CameraFramingMath.h` · `Private/.../CameraFramingMath.cpp` | 줌 리그 순수 평가·수직 FOV 변환·필요 거리·이분법 솔버 (테스트 `CGR.CameraFraming.*` 6종) |
