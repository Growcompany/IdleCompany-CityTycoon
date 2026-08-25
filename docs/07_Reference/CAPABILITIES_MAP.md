# CAPABILITIES_MAP - 재사용 가능한 공개 API 카탈로그

> 새 기능 구현 전 여기서 **이미 있는 함수**를 먼저 찾을 것 (reinvent 방지).
> CLAUDE.md 구현 전 기존 기능 탐색 규칙의 참조 색인. 헤더 스캔 자동 생성(2026-06-03).
> 코드 변경 시 stale 가능 - 의심되면 실제 헤더 확인. 새 재사용 API 추가 시 이 문서에도 한 줄 추가.
> **UI 연출/이펙트/가챠 효과 소스(텍스처/코드패턴/머티리얼/Niagara)는 별도 카탈로그: `docs/05_UI/UI_VFX_SOURCEBOOK.md`**

## CAMERA & PLAYER-INPUT (UE5.4 C++)
총 71 클래스 / 910 API 카탈로그됨.


### APlayerCamera
Base camera pawn with movement (WASD/drag), zoom (wheel), pan, and building focus automation. Cross-map foundation.
파일: `Private/Player/PlayerCamera.h`

| API | 용도 |
|---|---|
| `void FocusOnBuilding(ABuildingBaseActor* Building, float TopPadding = 50.f, float BottomPadding = 50.f, float ScreenCenterRatioX = 0.5f)` | Frame building onscreen with configurable padding and horizontal center ratio for skin panel display |
| `void FocusOnBuildingForPlacement(ABuildingBaseActor* Building, float TopPadding = 50.f, float BottomPadding = 500.f)` | Frame building + restore input state when exiting placement widget (camera focuses, then re-enables normal input) |
| `virtual void FocusOnLocation(const FVector& TargetLocation, float DesiredDistance = 0.f)` | Pan camera to world location with optional distance override; overridable for subclass customization |
| `void FocusOnActor(AActor* Actor, float ScreenCenterRatioX = 0.5f, bool bTrackOcclusion = true)` | 바운딩박스 기반 범용 포커스. `bTrackOcclusion=false` 면 가림 고스트를 켜지 않음 (로딩 뒤 초기 포커스·배치 모드용) |
| `void StopCameraTransition()` | Cancel in-progress camera pan/zoom smoothing (useful before applying new focus) |
| `bool IsTransitioningCamera() const` | 포커스 글라이드가 진행 중인가. 도착 델리게이트가 없어 폴링으로 읽는다 ― 플레이어 입력이 `StopCameraTransition()` 으로 글라이드를 취소할 수 있어 "완료" 통지가 안 오는 경로가 있다. **"도착했는가" 를 물을 때는 쓰지 말 것**(아래) |
| `bool IsFocusVisuallySettled(float LocationTolerance = 50.f, float ZoomTolerance = 0.01f) const` | **눈으로 도착했는가**. ⚠ 위 완료 플래그의 임계(거리 0.1uu · 줌차 0.001)는 지수 보간(`CameraTransitionSpeed 3.0`)과 만나 **시각적 도착보다 2~3초 늦게** 내려간다(정착 시간 = `ln(초기차/임계)/speed`) ― 그 플래그를 기다리면 튜토리얼 설명 같은 후속 연출이 타임아웃으로 날아간다. 카메라가 없거나(`MovementInputHandler` 미부착) 전환 중이 아니면 **true**(기다리지 않는 쪽 = 갇히지 않는 쪽) |
| `void ClearFocusTarget()` | 포커스 종료 + 가림 고스트 해제. 패널 `NativeOnDeactivated` 에서 `GoToNormalMode()` 와 쌍으로 호출 (빠뜨리면 건물이 숨겨진 채 남음) |
| `void RestoreFocusTarget(AActor* Target)` | 카메라를 움직이지 않고 가림 판정만 재등록 — 자식 위젯 push 로 비활성화됐던 패널이 `NativeOnActivated` 에서 고스트 복귀시킬 때. `ClearFocusTarget()` 의 짝 |
| `void BeginBuild(const FBuildableCardTable& buildableInfo)` | Activate building placement mode (delegates to PlacementHandler) |
| `void EndBuild()` | Deactivate building placement mode |
| `void StartBuildingRelocation(AInteractableBaseActor* ExistingBuilding)` | Prepare to move an already-placed building (saves original position for undo) |
| `void EndBuildingRelocation()` | Cancel building move and restore original position |

### AOfficeCameraPawn
Office-specific camera pawn (inherits APlayerCamera). Restricts yaw rotation, disables Spin/PlacementHandler/InteractableInputHandler. Adds wall edit mode and employee follow targeting.
파일: `Public/Player/OfficeCameraPawn.h`

| API | 용도 |
|---|---|
| `void EnterWallEditMode()` | Enable camera yaw rotation (unlock from default 135째 restriction) for wall decoration editing |
| `void ExitWallEditMode()` | Restore yaw rotation to default (135째) after wall editing complete |
| `void FocusOnWall(EWallSide WallSide, const FVector& FocusLocation = FVector::ZeroVector)` | Rotate camera to face Left/Right wall with smooth interpolation (optional custom look point) |
| `bool IsInWallEditMode() const` | Query whether wall edit mode is active |
| `void FocusOnEmployee(AOfficeworker* Employee, float DesiredDistance = 500.f, bool bEnableFollow = true)` | Pan+zoom to employee; optionally follow them as they move (useful for training/observation modes) |
| `bool ApplyInitialOfficeFraming(bool bForce=false)` | 세이브 로드 완료 뒤 현재 Office footprint의 개방 코너+첫 파사드 깊이를 safe frame에 맞춰 최초 1회 포커스. 성공적으로 포커스를 요청했거나 이미 적용됐으면 true. `bForce=true`는 동일 Pawn 검증 재적용 전용이며 확장에는 호출하지 않는다 |
| `void StopFollowingEmployee()` | End employee tracking and return camera to manual control |
| `bool IsFollowingEmployee() const` | Check if currently tracking an employee |
| `void BeginWorkstationPlacement(const FWorkstationCardTable& WorkstationInfo)` | Activate workstation placement mode on office floor |
| `void EndWorkstationPlacement()` | Deactivate workstation placement mode |
| `bool IsInWorkstationPlacementMode() const` | Check if workstation placement UI is visible |
| `const FWorkstationCardTable& GetCurrentWorkstationInfo() const` | Retrieve the workstation being placed (read-only snapshot) |
| `void BeginDecorationPlacement(const FDecorationCardTable& DecorationInfo)` | Start floor decoration placement (grid-based, not wall) |
| `void BeginWallDecorationPlacement(const FDecorationCardTable& DecorationInfo, EWallSide WallSide)` | Start wall decoration placement (wall-constrained, with camera rotation) |
| `void EndDecorationPlacement()` | End decoration placement mode |
| `bool IsInDecorationPlacementMode() const` | Check if any decoration placement is active |
| `const FDecorationCardTable& GetCurrentDecorationInfo() const` | Get the decoration being placed (read-only snapshot) |

### AWorldMapCameraPawn
Overworld/tiled map camera (inherits APlayerCamera). Top-down fixed angle, no rotation. Y-axis bounds enforcement for tile streaming.
파일: `Public/Player/WorldMapCameraPawn.h`

| API | 용도 |
|---|---|
| `FIntPoint GetCurrentTileCoord() const` | Determine which tile grid cell the camera is currently centered on (useful for lazy-loading regions) |

### ARecruitmentCameraPawn
Fixed camera for recruitment map. No input handling, positioned at PlayerStart, low door-facing angle.
파일: `Public/Player/RecruitmentCameraPawn.h`

### ALootBoxOrbitPawn
Orbiting camera around loot box actor. Left-right rotation via mouse/touch drag, fixed distance/height, no translation.
파일: `Public/Player/LootBoxOrbitPawn.h`

| API | 용도 |
|---|---|
| `void SetLootBox(ALootBoxActor* NewLootBox)` | Retarget orbit center to a new loot box actor |
| `void ChangeLootBoxAppearance(ELootBoxRarity Rarity, ELootBoxType Type)` | Update 3D model skin based on rarity/type enum (affects viewer perception) |

### AMainMapPlayerController
Main-map input mode state machine. Routes touch/mouse to Normal/BuildPlace/UI/Factory modes. Manages LootBox map transitions.
파일: `Private/Player/MainMapPlayerController.h`

| API | 용도 |
|---|---|
| `void SetGameInputMode(EInputMode NewMode)` | Directly set input mode (use mode-specific functions below instead for clarity) |
| `void GoToNormalMode()` | Resume normal gameplay input (building click, movement, etc.) |
| `void GoToUIMode()` | Block game input and enable UI mouse/touch cursor |
| `void GoToBuildPlaceMode()` | Enter building placement input state (enable placement drag/rotate, disable normal clicks) |
| `void GoToFactoryMode()` | Switch to factory-specific input (used for factory/production screen) |
| `EInputMode GetCurrentInputMode() const` | Query active input mode (check before mode transition to avoid duplicate calls) |
| `EInputType GetCurrentInputType() const` | Get platform input type (KeyMouse/Touch/GamePad) for platform-specific UI branching |
| `void OpenLootBoxMap(ELootBoxCategory Category)` | Transition to loot box screen with category filter |

### AOfficePlayerController
Office-map controller. Inherits MainMapPlayerController. Adds explicit UI/Game mode toggle (separate from MainMap state machine).
파일: `Private/Player/OfficePlayerController.h`

| API | 용도 |
|---|---|
| `void SetUIMode()` | Show cursor and disable camera movement (for UI interaction phases) |
| `void SetGameMode()` | Hide cursor and enable camera movement (for exploration phases) |
| `bool IsUIMode() const` | Check if cursor is visible and camera input disabled |

### AWorldMapPlayerController
World map controller. Inherits MainMapPlayerController for touch/pan/zoom compatibility. Adds return-to-main navigation.
파일: `Public/Player/WorldMapPlayerController.h`

| API | 용도 |
|---|---|
| `void ReturnToMainMap()` | Transition from world map back to main building screen |

### AUpgradeMapPlayerController
Upgrade/progression screen controller. Placeholder (no public APIs yet).
파일: `Public/Player/UpgradeMapPlayerController.h`

### ALootBoxPlayerController
Loot box screen input handler. PC-side input (keyboard/Enhanced Input) + mobile touch (legacy). Connects UI widget to 3D loot box actor.
파일: `Public/Player/LootBoxPlayerController.h`

### UMovementInputHandler
Handles camera movement (WASD/drag), zoom (wheel), rotation (right-stick/spin). Applies boundary clamping and edge-pan acceleration.
파일: `Private/Player/Components/MovementInputHandler.h`

| API | 용도 |
|---|---|
| `void Initialize(UFloatingPawnMovement* Movement)` | Attach to movement component for physics-driven panning |
| `void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)` | Bind Enhanced Input actions (Move, Zoom, Spin, DragMove) |
| `void InitDragMoveIMC() const` | Enable drag-to-pan input mapping context (call when drag mode activates) |
| `void ReleaseDragMoveIMC() const` | Disable drag-to-pan input mapping context |
| `float GetZoomValue() const` | Read normalized zoom state [0.0, 1.0] for camera transition interpolation |
| `void SetZoomValue(float NewValue)` | Override zoom value when snapping to a focus target (clamped to [0.0, 1.0]) |
| `void UpdateZoom()` | Process zoom curve and apply to spring arm (called each frame) |
| `void ApplyZoomSettings()` | Push ZoomValue to SpringArm length without modifying the value itself |
| `void UpdateDof() const` | Apply depth-of-field post-process based on current zoom |
| `void MoveTracking()` | Continuous drag-pan update (interpolates toward target position) |
| `void PositionCheck()` | Enforce boundary radius clamping on camera location |

### UInteractableInputHandler
Building click detection and long-press relocation. Raycast from screen to world, debounce touch, trigger long-press timer, dispatch OnInteract events.
파일: `Private/Player/Components/InteractableInputHandler.h`

| API | 용도 |
|---|---|
| `void Initialize()` | Prepare input subsystem references and timer state |
| `void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)` | Bind Enhanced Input actions for hold/tap interactions |

### UPlacementHandler
Unified placement system for buildings, workstations, floor/wall decorations. Drag tracking, edge-pan, arrow nudge, visualization preview, overlap validation.
파일: `Public/Player/Components/PlacementHandler.h`

`FWorkstationCapacityRules::Evaluate(CurrentSeats, CandidateSeats, EmployeeCapacity)` is the pure fail-closed rule used by workstation placement to reject invalid or over-capacity candidates before spawning.

| API | 용도 |
|---|---|
| `void Initialize()` | Cache asset references and initialize internal state |
| `void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)` | Bind placement-specific input actions |
| `void SetPlacementTargetEntity(const FInteractableInfo& interactableInfo, TSubclassOf<AActor> InOverlapCheckClass = nullptr)` | Prepare a building for placement with collision class filter |
| `void PostPlacementTargetRecalcBoxExtent()` | Re-validate placement target collision after size/extent change |
| `void ReleasePlacementTargetEntity()` | Clear placement target and destroy preview actor |
| `bool PlacingEntity()` | Process frame-by-frame placement updates (drag tracking, rotation, edge-pan) |
| `void InitPlacementIMC()` | Enable placement-specific input mapping context |
| `void ReleasePlacementIMC()` | Disable placement-specific input mapping context |
| `FVector2D GetPlacementTargetBottomScreenPosition() const` | Compute screen projection of placement actor base (for UI anchor placement) |
| `void TrackMovePlacement()` | Update placement position based on current mouse/touch ray |
| `void TrackMovePlacement(const FVector& WorldPosition)` | Snap placement to world position (grid/floor placement mode) |
| `void TrackMovePlacement(const FVector2D& ScreenPosition)` | Move placement by screen coordinate (wall placement mode, projects to wall plane) |
| `void UpdateTrackMovePlacement()` | Apply frame smoothing to placement drag tracking |
| `void RotatePlacementEntity()` | Advance rotation state by 90째 (cyclically) |
| `AInteractableBaseActor* GetPlacementTargetEntity() const` | Read-only access to current placement actor |
| `void SetWorkstationPlacementTarget(TSubclassOf<AWorkstationActorBase> WorkstationClass, FName WorkstationTypeID)` | Prepare workstation for office placement |
| `void ReleaseWorkstationPlacementTarget()` | Clear workstation placement state |
| `AWorkstationActorBase* GetPlacementWorkstation() const` | Read current workstation being placed |
| `bool PlacingWorkstation(bool* bOutCapacityReached = nullptr)` | 실제 좌석 합계와 현재 건물 직원 정원으로 스폰 전 상한을 검증하고, 성공 배치가 정원을 정확히 채웠는지 반환 |
| `bool IsWorkstationMode() const` | Check if current mode is workstation placement |
| `void SetDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo)` | Start floor decoration (grid) placement |
| `void ReleaseDecorationPlacementTarget()` | Clear floor decoration placement state |
| `ADecorationActor* GetPlacementDecoration() const` | Read current decoration being placed |
| `bool PlacingDecoration()` | Update decoration placement frame-by-frame |
| `bool IsDecorationMode() const` | Check if current mode is floor decoration placement |
| `const FDecorationCardTable& GetPlacementDecorationInfo() const` | Read decoration info snapshot |
| `void SetWallDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo, EWallSide WallSide)` | Start wall decoration (constrained to wall surface) placement |
| `void ReleaseWallDecorationPlacementTarget()` | Clear wall decoration placement state |
| `bool PlacingWallDecoration()` | Update wall decoration placement frame-by-frame |
| `bool IsWallDecorationMode() const` | Check if current mode is wall decoration placement |
| `EWallSide GetCurrentWallSide() const` | Read which wall (Left/Right) is being placed on |
| `void SwitchToWall(EWallSide NewWallSide, const FVector& HitLocation = FVector::ZeroVector)` | Transition wall decoration to opposite wall during drag (preserves rotation offset) |
| `EPlacementMode GetCurrentPlacementMode() const` | Query current placement mode (None, Building, Workstation, Decoration, WallDecoration) |
| `void StartArrowNudge(EPlacementNudgeDir Dir)` | Begin accelerated glide in direction (Up/Down/Left/Right, screen-relative) while arrow held |
| `void StopArrowNudge()` | End arrow nudge and snap to nearest grid cell |
| `bool CanDropHere() const` | Check if placement is valid at current position (for arrow UI green/red) |
| `bool HasActivePlacement() const` | Check if any placement target is active (for arrow UI visibility) |
| `bool GetPlacementArrowLayout(FVector& OutCenter, FVector& OutRightEdge) const` | Compute two world points for arrow UI widget positioning (center + edge for scale) |
| `void BeginPlacementZoom()` | Focus+zoom camera on target building at start of placement mode |
| `void SetOriginalPosition(const FVector& Position)` | Store original position for building move undo |
| `void SetExistingBuildingAsTarget(AInteractableBaseActor* ExistingBuilding)` | Prepare already-placed building for relocation |
| `bool ConfirmBuildingMove()` | Finalize building move and clear undo position |
| `void CancelBuildingMove()` | Abort building move and restore original position |
| `void CompleteBuildingPlacement(AInteractableBaseActor* Building)` | Wrap up placement (destroy preview, clear state) |
| `void StartDragging()` | Set internal drag flag (for edge-pan acceleration context) |
| `void StopDragging()` | Clear drag flag |
| `bool IsDraggingPlacement() const` | Check if currently in drag-move phase |

### UWallPlacementHandler
Dedicated wall surface placement handler. Converts touch screen coords to wall plane, validates wall bounds, manages preview actor, no building/grid support (DecorActor only).
파일: `Public/Player/Components/WallPlacementHandler.h`

| API | 용도 |
|---|---|
| `void StartPlacement(TSubclassOf<ADecorationActor> DecorationClass, EWallSide WallSide)` | Activate wall placement mode for a decoration class on a specific wall |
| `void EndPlacement()` | Deactivate wall placement mode and destroy preview |
| `bool IsPlacing() const` | Check if wall placement is currently active |
| `void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)` | Bind touch/drag input actions for wall placement |

### UInputTypeManager
Platform input type detection (KeyMouse/Touch/GamePad). Static utility for branching UI/input logic by platform.
파일: `Private/Player/InputTypeManager.h`

| API | 용도 |
|---|---|
| `static EInputType GetPlatformInputType()` | Detect active input device and return enum (KeyMouse/Touch/GamePad) for platform-specific behavior |
| `EInputType GetValue() const` | Read current cached input type |
| `void SetValue(EInputType NewInputType)` | Override input type (rarely needed; auto-detect via GetPlatformInputType() preferred) |

### UCGCheatManager
Debug console commands. Resource grants, employee leveling, stage completion, building ops, HQ upgrades, mine/trait/product seeding.
파일: `Public/Player/CGCheatManager.h`

| API | 용도 |
|---|---|
| `void CheatPing()` | Simple connectivity test console command |
| `void Exp(int32 Amount, int32 EmployeeID = -1)` | Add exp to employee (or all with -1) |
| `void DistExp(int32 Amount, int32 QualityGradeValue = 3)` | Distribute exp across building employees by quality tier |
| `void ExpStatus()` | Print employee exp/level summary to log |
| `void Money(int64 Amount)` | Add currency |
| `void ShowResources()` | Display all resource inventory |
| `void GrantRes(FString ScenarioRowName)` | Load test resource scenario (Rich/Default/Poor/Empty) |
| `void ListEmp()` | List all employees with IDs |
| `void SetRank(int32 EnhancementLevel)` | Set selected employee enhancement level (0-12) |
| `void CompleteStep()` | Finish current building step immediately |
| `void StageStatus()` | Show current stage state and timeline |
| `void SetOpTime(float Seconds)` | Override building operation countdown (0 = next tick complete) |
| `void OpStatus()` | Display operation state and remaining time |
| `void Ticket(int32 Amount = 10, int32 TierValue = 0)` | Grant recruitment tickets (Normal/Advanced/Premium) |
| `void SetBldLv(int32 Level, int32 BuildingIndex = -1)` | Set building level directly (current managed building if index=-1) |
| `void UnlockSkins(int32 Count = 5)` | Unlock N random unreleased skins |
| `void HQLevelUp()` | Force HQ level up (bypass requirements) |
| `void SetHQLv(int32 Level)` | Set HQ level directly |
| `void MineDebug()` | Print all country mines (level, rate, storage, saturation) |
| `void MineFill(const FString& CountryName)` | Fill mine storage to capacity (saturation/claim UI test) |
| `void MineSetLv(const FString& CountryName, const FString& UpgradeName, int32 Level)` | Set mine upgrade level (Rate or Storage) |
| `void SeedProducts(int32 NumSlots = 24, int64 MinQty = 100, int64 MaxQty = 8000)` | Populate world-map sell modal with random products for testing |
| `void SeedTraits(int32 NumKinds = 30, int32 MinQty = 1, int32 MaxQty = 5)` | Seed trait inventory with random trait cards (testing trade/collection) |
| `void TraitStatus()` | Print trait inventory summary |

## PLACEMENT/BUILDER System (UE5.4 C++)

### UPlacementManagerComponent
Core placement orchestrator; manages placement preview/selected/hovered states and validates grid rules
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Builder\Components\PlacementManagerComponent.h`

| API | 용도 |
|---|---|
| `void Placement_Start(struct FPlacementData Data)` | Begin placement process with specified actor class and grid/alignment rules |
| `void Placement_Cancel()` | Abort active placement and destroy preview actor without placing |
| `void Placement_Accept()` | Finalize placement if grid/validation conditions pass; spawn permanent actor |
| `void ApplyYawRotationToPlacement(float Yaw)` | Apply Z-axis rotation to preview during active placement |
| `void SelectUnderCursor()` | Locate and select APlacementActor at cursor; triggers OnSelected event |
| `bool Replacement_Start(class APlacementActor* building)` | Begin relocating an already-placed building; saves original transform for cancel |
| `void Replacement_Accept()` | Confirm building relocation to new position |
| `void Replacement_Cancel()` | Restore building to pre-replacement transform |
| `void RemoveBuilding(class APlacementActor* toRemove)` | Despawn building and release its reserved grid cells |
| `bool GetUnderCursorTrans(FTransform& UnderCursorLoc)` | Query transform at cursor; applies grid snapping rules per GridSettings |

### AGridManager
Grid-based placement validation and cell reservation tracker; handles grid cell state and visualization
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Builder\Actors\GridManager.h`

| API | 용도 |
|---|---|
| `void ApplyGridSettings(FTransform& transform)` | Snap location to grid cells per GridSize and bSnapToGridCellCenter settings |
| `bool CanAddBuildingToGrid(APlacementActor* actorToPlace) const` | Check if building fits at current location (all cells free and within bounds) |
| `TArray<struct FGridCell> GetCellsNeededForBuilding(APlacementActor* actorToPlace) const` | Compute which grid cells are required by a building's width_Cell/depth_Cell footprint |
| `bool AddBuildingToGrid(class APlacementActor* ToPlace)` | Reserve cells for new building and register it; first-time placement only |
| `void ReplaceBuilding(class APlacementActor* ToPlace)` | Release old cells and reserve new ones when building moves during replacement |
| `void OnBuildingRemoved(class APlacementActor* ToRemove)` | Free reserved cells when building is deleted |
| `FGridCell LocationToCell(FVector Location)` | Convert world location to grid cell X,Y indices |
| `void DrawCells(TArray<FGridCell> cells, int meshIndex = 1, float Padding = 10.0f, UMaterialInterface* CustomCellDrawMaterial = nullptr, FVector offset = FVector(0,0,60))` | Render preview mesh visualization for cell list (for debug or UI feedback) |
| `void ClearCellDrawing(int meshIndex, bool bAllSections = false)` | Remove procedural mesh visualization for specified section |
| `void SetGridSettingsData(FGridSettingsData data)` | Update grid size and snap-to-center preference at runtime |

### APlacementActor
Placeable building/entity base; grid-aware with hover/select animation and placement lifecycle callbacks
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Builder\Actors\PlacementActor.h`

| API | 용도 |
|---|---|
| `bool RegisterForReplacement()` | Request movement; returns true if selected and accepted by PlacementManager |
| `void RemoveBuilding()` | Self-destruct and free reserved grid cells |
| `FGuid GetBuildingId() const` | Unique ID for grid cell reservation tracking |
| `TArray<struct FGridCell> GetReservedCells()` | Query which grid cells this building occupies |
| `void SetReserverCells(TArray<struct FGridCell> newCells)` | Update reserved cells (used during grid sync) |

### UPlacementHandler
Office/building placement controller; tracks placement mode (Building/Workstation/Decoration/WallDecoration) and handles drag/nudge input
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Player\Components\PlacementHandler.h`

| API | 용도 |
|---|---|
| `void SetPlacementTargetEntity(const FInteractableInfo& interactableInfo, TSubclassOf<AActor> InOverlapCheckClass = nullptr)` | Set entity (Building/Workstation/Decoration) as current placement target |
| `void ReleasePlacementTargetEntity()` | Clear placement target and destroy preview |
| `bool PlacingEntity()` | Execute placement pipeline for current entity (grid validation, finalize) |
| `void TrackMovePlacement()` | Update preview position to follow cursor (for drag operations) |
| `void TrackMovePlacement(const FVector& WorldPosition)` | Snap preview to world position (grid-floor placement) |
| `void TrackMovePlacement(const FVector2D& ScreenPosition)` | Project screen position to wall plane (wall decoration placement) |
| `void UpdateTrackMovePlacement()` | Recalculate preview after movement callbacks |
| `void RotatePlacementEntity()` | Rotate preview 90 degrees around Z-axis |
| `void SetWorkstationPlacementTarget(TSubclassOf<AWorkstationActorBase> WorkstationClass, FName WorkstationTypeID)` | Begin workstation placement mode |
| `void ReleaseWorkstationPlacementTarget()` | End workstation placement |
| `AWorkstationActorBase* GetPlacementWorkstation() const` | Query active workstation preview actor |
| `bool PlacingWorkstation(bool* bOutCapacityReached = nullptr)` | Finalize workstation placement after enforcing the current building's employee-capacity seat limit; reports exact capacity reach |
| `void SetDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo)` | Begin grid-based decoration (floor) placement |
| `void ReleaseDecorationPlacementTarget()` | End decoration placement |
| `ADecorationActor* GetPlacementDecoration() const` | Query active decoration preview actor |
| `bool PlacingDecoration()` | Finalize decoration placement |
| `void SetWallDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo, EWallSide WallSide)` | Begin wall decoration placement on specified wall |
| `void ReleaseWallDecorationPlacementTarget()` | End wall decoration placement |
| `bool PlacingWallDecoration()` | Finalize wall decoration placement |
| `void SwitchToWall(EWallSide NewWallSide, const FVector& HitLocation = FVector::ZeroVector)` | Move preview to different wall; preserves rotation offset across walls |
| `void StartArrowNudge(EPlacementNudgeDir Dir)` | Begin accelerating preview in cardinal direction (UI arrow glide movement) |
| `void StopArrowNudge()` | Stop glide and snap to nearest grid cell |
| `bool CanDropHere() const` | Check if current position is valid for placement (UI arrow color feedback) |
| `bool HasActivePlacement() const` | Query whether placement mode is active (UI visibility) |
| `bool GetPlacementArrowLayout(FVector& OutCenter, FVector& OutRightEdge) const` | Get 3D basis points for arrow UI overlay (building center + radius reference) |
| `void BeginPlacementZoom()` | Zoom camera onto building during placement start |
| `void SetOriginalPosition(const FVector& Position)` | Store position for placement cancel/undo |
| `void SetExistingBuildingAsTarget(AInteractableBaseActor* ExistingBuilding)` | Register existing building for movement/replacement workflow |
| `bool ConfirmBuildingMove()` | Validate and commit building relocation |
| `void CancelBuildingMove()` | Restore building to original position on cancel |
| `void CompleteBuildingPlacement(AInteractableBaseActor* Building)` | Finalize placement lifecycle (cleanup, callbacks) |

### UWallPlacementHandler
Dedicated wall decoration placement; manages wall surface bounds, touch-to-wall mapping, and overlap validation
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Player\Components\WallPlacementHandler.h`

| API | 용도 |
|---|---|
| `void StartPlacement(TSubclassOf<ADecorationActor> DecorationClass, EWallSide WallSide)` | Initialize wall placement mode for given decoration and wall side |
| `void EndPlacement()` | Exit wall placement mode |
| `bool IsPlacing() const` | Query if wall placement is active |

### UInteractableInputHandler
Input dispatcher for building/interactable interactions; routes tap, hold, and drag events
파일: `${PROJECT_ROOT}\Private\Player\Components\InteractableInputHandler.h`

| API | 용도 |
|---|---|
| `void Initialize()` | Setup input subsystem and mapping contexts |
| `void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)` | Register Enhanced Input action callbacks |

## ENTITY (Buildings, Employees, Interactables, Decorations, Workstations)

### AInteractableBaseActor
Base class for all interactable entities (buildings, facilities, factories). Manages mesh rendering, collision, navigation blocking, and wobble animations.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Entity/InteractableBaseActor.h`

| API | 용도 |
|---|---|
| `void ReCalcBoxExtent() const` | Recalculate BoxComponent extent to fit mesh bounds; broadcasts ReCalcBoxExtentDelegate. |
| `void UpdateNavBlockerState()` | Update navigation blocker component state to match entity placement. |
| `void DetachNavBlockerForMove()` | Detach navigation blocker before moving entity to new location. |
| `void AttachNavBlockerToNewLocation()` | Reattach navigation blocker after entity has been moved. |
| `bool IsWoobleAnimationPlaying() const` | Check if wobble (shake) animation is currently playing. |
| `void FinalizeEntityRegistration()` | Finalize registration with EntityManager after placement confirmation. |
| `float Interact()` | Handle interaction callback; returns work time for animations. |
| `void SetInteractableInfo(const FInteractableInfo& InInfo)` | Initialize entity with interactable info (name, type, row data). |
| `void PlayWobble()` | Play wiggle/shake animation when entity is clicked. |
| `void EndWooble()` | Stop wobble animation immediately. |
| `UMeshComponent* GetMainMeshComponent() const` | Get primary mesh component for rendering and collision. |
| `void CalcTemporaryDistanceFromLocation(FVector InLocation)` | Cache squared distance for sorting (temp use in placement/selection). |
| `float GetDistSquared() const` | Retrieve cached squared distance for sorting entities. |

### ABuildingBaseActor
Main building entity with floors, skins, lighting, employees, and enhancement system. Handles building placement, upgrade, and management UI.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Entity/Building/BuildingBaseActor.h`

| API | 용도 |
|---|---|
| `void BuildSuccess()` | Mark building as complete (post-placement); triggers VFX and saves. |
| `void SaveGameData()` | Immediately persist building state to save file (floors, type, employees, skins). |
| `void InitializeFromSaveData(const FInteractableInfo& InInfo, const FBuildingEntitySaveData& LoadData)` | Reconstruct building from save data on load (floors, type, employees, enhancements). |
| `FBuildingEntitySaveData GetBuildingSaveData() const` | Export building state for saving (all floors, type, employees, skins, lights, enhancements). |
| `void PlayPlacementLandSequence()` | Play landing animation with whoosh + thud + rumble when building is placed. |
| `void PlayBuildCompleteVFX(float SpawnHeight = 0.0f)` | Spawn dust/completion VFX at height after build/upgrade finishes. |
| `void AddFloor(bool bShouldSave = true)` | Add one floor; if bShouldSave=false, caller must save once after bulk calls (N+1 write prevention). |
| `bool IsEnhancementPurchaseAllowed(EBuildingEnhancementType EnhancementType) const` | 강화 구매 권위 판정. 튜토리얼 미완료에는 BuildingFloor만 허용하고, 완료 후에는 건물 티어·키스톤 규칙을 적용. `UpgradeEnhancement()`가 비용 차감 전에 재검사 |
| `void ApplySkin(int32 SkinID, bool bShouldSave = true)` | Apply mesh skin and save (optional). |
| `int32 GetAppliedSkinID() const` | Get current applied mesh skin ID (default 100=Common). |
| `void ApplyLight(int32 LightID, bool bShouldSave = true)` | Apply window emissive color and save (optional). |
| `int32 GetAppliedLightID() const` | Get current applied lighting ID (default 100=warm yellow). |
| `FName GetBuildingID() const` | Get building type ID (DataTable RowName, e.g. 'Building_1'). |
| `int32 GetBuildingIndex() const` | Get unique building index for save/map keys. |
| `void SetBuildingIndex(int32 InIndex)` | Set unique building index (used for save/load and Map<Index>). |
| `int32 GetBuildingLevel() const` | Get current building level (affects capacity, bonuses). |
| `ECompanyType GetCompanyType() const` | Get assigned industry type (Office, Game, Hotel, etc.; None if unset). |
| `void SetCompanyType(ECompanyType NewType)` | Set industry type and broadcast OnBubbleRefreshRequested delegate. |
| `void OpenManagePanel(AMainMapPlayerController* MainPC)` | Open management panel for existing buildings (handles UI layer transitions). |
| `void OpenBuildingUI(AMainMapPlayerController* MainPC)` | Open building UI: IndustrySelectPanel if CompanyType=None, else OpenManagePanel. |
| `void SetWindowLightActive(bool bActive)` | Toggle window light on/off based on project status (project exists = on). |
| `void StartFloating(float Height = 50.0f, bool bImmediate = false)` | Begin floating (levitating) animation during placement; bImmediate skips animation. |
| `void StopFloating()` | Stop floating animation and return to original Z. |
| `void LandBuilding()` | Play landing animation and trigger completion VFX. |
| `bool IsFloating() const` | Check if building is currently in floating state. |
| `float CalculateTopModuleHeight() const` | Compute roof module height (roof element only). |
| `float GetTotalBuildingHeight() const` | Get total building height including roof module. |
| `FVector GetBubbleAnchorPosition() const` | Get world position for bubble/UI anchoring (mesh horizontal center + top). |
| `bool UpgradeEnhancement(EBuildingEnhancementType EnhancementType, int32 Count = 1)` | Upgrade building stat (Durability, Prestige, Efficiency, etc.); broadcast; save. |
| `int32 GetEnhancementLevel(EBuildingEnhancementType EnhancementType) const` | Get current level of specific enhancement stat. |
| `float GetEnhancementMultiplier(EBuildingEnhancementType EnhancementType) const` | Get effect multiplier for enhancement (e.g. 1.5 = 150% income boost). |
| `int64 GetUpgradeCost(EBuildingEnhancementType EnhancementType) const` | Get cost to upgrade next level of enhancement stat. |
| `int64 GetBulkUpgradeCost(EBuildingEnhancementType EnhancementType, int32 Count) const` | Get total cost for bulk upgrade (Count levels). |
| `float GetVaultCapacity() const` | Get Money vault capacity from `UProjectOperationManager`'s single source: tier `BandStart` reference rate × `VaultSeconds(Level)`, with 60,000 fallback. |
| `FBox GetBuildingWorldBounds() const` | 본체 메시(MainMesh + Body/Top/TopEmpty ISM) 월드 바운드 합산. 박스 컴포넌트 제외 — 아우라/고스트 박스가 자기 바운드로 부풀지 않게. |
| `void SetOccluderGhost(bool bOn)` | 가림 고스트 토글 (본체 메시 + 비콘/아우라/돔 숨김 → 반투명 골조 박스). OFF 복원은 스냅샷이 아니라 상태 출처에서 재계산. |
| `void SetGhostOpacity(float Opacity)` | 고스트 박스 불투명도만 갱신 (페이드 전용). 고스트 상태가 아니면 무시. |
| `bool IsOccluderGhosted() const` | 현재 가림 고스트 상태인지 (추적을 놓친 고스트 일괄 복원용). |

### AOfficeworker
Employee/worker character with customizable appearance (face, hair, clothing), state animations, bubble indicators, and workstation assignment.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Entity/Officeworker/Officeworker.h`

| API | 용도 |
|---|---|
| `void SetCharacterAppearance(const FCharacterAppearance& Appearance, EEmployeeRank Rank, EEmployeeGender Gender, int32 EmployeeID = -1)` | Apply appearance data (hair, clothing, colors, morph targets) by rank/gender. |
| `void SetEmployeeID(int32 InEmployeeID)` | Set employee ID for deterministic clothing/accessories (e.g. shoe type). |
| `int32 GetEmployeeID() const` | Get assigned employee ID. |
| `void SetWorkerBubbleType(EWorkerBubbleType NewType)` | Update status bubble above head (Working, Resting, Buffed, etc.; None hides it). |
| `EWorkerBubbleType GetCurrentBubbleType() const` | Get current bubble type (state indicator). |
| `void SetGlowOverlay(bool bEnabled, FLinearColor Color = FLinearColor(1.f, 0.85f, 0.2f, 1.f))` | Toggle glow overlay on mesh (event buff visual; default yellow). |
| `EEmployeeState GetEmployeeState() const` | Get current employee behavior state (Working, Resting, Wandering, etc.). |
| `bool IsSeated() const` | Check if employee is sitting at workstation. |
| `bool IsWalking() const` | Check if employee is moving (animation state). |
| `float GetSpeed() const` | Get current movement speed (1-5 = walk, 5+ = run). |
| `void StartRandomRoaming(float InRoamRadius = 500.f, float InMinWaitTime = 2.f, float InMaxWaitTime = 5.f)` | Begin idle wandering in radius; stops on command or state change. |
| `void StopRandomRoaming()` | Stop idle wandering. |
| `bool IsRoaming() const` | Check if employee is currently wandering. |
| `void SetSelected(bool bSelected)` | Apply selection overlay material (UI feedback). |
| `bool IsSelected() const` | Check if employee has selection overlay. |
| `void SetAssignedWorkstation(AWorkstationActorBase* Workstation, int32 SeatIndex)` | Assign employee to workstation seat (stores weak reference). |
| `bool TeleportToWorkstationAndSit()` | Instant teleport to assigned workstation and sit (Stage mode); returns success. |
| `void MoveToWorkstationArea()` | Begin navigation to assigned workstation (Operation mode). |
| `void StandUpFromWorkstation()` | Stand up and leave assigned workstation. |
| `AWorkstationActorBase* GetAssignedWorkstation() const` | Get assigned workstation actor (nullptr if none). |
| `int32 GetAssignedSeatIndex() const` | Get assigned seat index (-1 if unassigned). |
| `FVector GetCenterLocation() const` | Get center position of character for camera focusing. |

### UEmployeeBehaviorComponent
Component managing employee state machine (Work/Rest/Wander), income generation, buffs, and animation transitions for Idle/Stage/Operation modes.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Entity/Officeworker/EmployeeBehaviorComponent.h`

| API | 용도 |
|---|---|
| `void SetState(EEmployeeState NewState)` | Transition to new behavior state (Working, Resting, Wandering); broadcasts OnStateChanged. |
| `EEmployeeState GetState() const` | Get current behavior state. |
| `void SetBehaviorMode(EEmployeeBehaviorMode NewMode)` | Set behavior pattern mode (Idle, Stage, Operation); affects animation/income logic. |
| `EEmployeeBehaviorMode GetBehaviorMode() const` | Get current behavior pattern mode. |
| `void StartWander()` | Begin wandering (Idle mode). |
| `void ReturnToWorkstation()` | End wandering and return to workstation. |
| `void StartMoveToWorkstation()` | Begin pathfinding to assigned workstation (Operation mode). |
| `void StartOperationWander()` | Begin wandering with Operation mode timings (3-8s). |
| `void StartCheerSitting()` | Play seated cheer animation (Step success). |
| `void StartCheerStandUp()` | Play standing cheer animation (launch success). |
| `void OnCheerAnimationComplete()` | Callback from AnimNotify when cheer finishes; return to default state. |
| `void OnTypeToSitComplete()` | Callback from AnimNotify when type?뭩it transition completes. |
| `void OnSitToStandComplete()` | Callback from AnimNotify when sit?뭩tand transition completes. |
| `void OnStandToSitComplete()` | Callback from AnimNotify when stand?뭩it transition completes. |
| `void StartGreeting()` | Begin greeting animation (RecruitmentGameMode); broadcasts OnGreetingFinished on complete. |
| `void OnGreetingComplete()` | Callback from AnimNotify when greeting animation finishes. |
| `void AddBuff(EBuffType Type, float Value, float Duration)` | Apply temporary buff (score, crit chance, work speed); merges if already active. |
| `float GetTotalScoreMultiplier() const` | Get largest ScoreMultiplier from active buffs (default 1.0 if none). |
| `float GetTotalCritChanceBonus() const` | Sum all CritChance bonuses from active buffs. |
| `float GetWorkSpeedMultiplier() const` | Get minimum WorkSpeed multiplier from active buffs (default 1.0 if none). |
| `bool HasActiveBuff() const` | Check if any buffs are currently active (UI indication). |
| `void SetOperationIncome(float IncomePerSecond)` | Set per-employee income rate for Operation mode (project revenue / employee count). |
| `float GetFatigueOutputFactor() const` | 피로→출력 커플링 팩터 (Tired 초과 선형 감산, 1.0~1−MaxFatiguePenalty). 점수/수익 경로 공용. |
| `void ApplyTapRelief()` | 캐치 탭 — 피로 부분 차감 (DA TapRelief, 하드 0 리셋 아님). |
| `EFatigueSlackPhase GetFatigueSlackPhase() const` | 현재 슬랙 단계 (None/Telegraph/Slumping/Bolting). 캐치 라이브 힌트가 꾸벅(깨우기)과 이탈(불러오기)로 문구·졸업 키를 가르는 축 ― `IsAwaitingCatch()` 의 단계 해상도 버전. |
| `float GetSpeedFactor() const` *(private)* | 유효업무속도(스탯+★)와 WorkSpeed 버프가 모이는 단일 지점 — `Interval`/`Overflow` 양쪽이 이 값을 공유(스탯 재설계 §3.1, 2026-07-26 신설). 외부 공개 아님, 내부 참조용으로만 카탈로그. |
| `float GetEffectiveBoltChance() const` *(private)* | 침착성 반영 실효 폭주(자리 이탈) 확률 — `BoltChance × (1−min(유효침착성×ComposureBoltScale, MaxBoltReduction))`(스탯 재설계 §3.3, 2026-07-26 신설). 외부 공개 아님, 내부 참조용으로만 카탈로그. |

### UEmployeeTypeHelper
직원 직급/출력 정적 헬퍼 (BlueprintFunctionLibrary).
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Data/EmployeeTypes.h`

| API | 용도 |
|---|---|
| `static EEmployeeRank GetRankFromEnhancementLevel(int32)` | 강화 레벨 → 직급 (1:1, +12=회장). |
| `static float CalculateBaseOutput(int32 Level)` | 직원 기본 출력 단일 정의 — 점수/Idle수익 공용 밸런스 SOT (`0.65×(1+(Lv−1)×0.1)`). **정정**: 구 서술의 `EnhancementLevel` 인자와 `×1.15^강화` 항은 2026-07-14 스타포스 전환 이후로 존재하지 않는다 — 강화 이득은 이 함수가 아니라 스탯 파생(`GetEnhanceStatBonus`) 경로로 들어간다. |
| `static int32 GetEnhanceStatBonus(int32 EnhancementLevel)` | ★당 전 스탯(6종) 파생 보너스 (`max(0,E) × StatPerStar(2)`) — 유효스탯 = 저장값 + 이 값, 저장 안 함(하락 시 자동 정합). |
| `static TArray<int32> MakeStatProfile(int32 Total = 30, int32 MinPerStat = 3)` | 스폰 시 초기 스탯 분배 — 6칸에 `Total`을 랜덤 분배하되 각 칸 `MinPerStat` 보장(스탯 재설계 §4.1, 2026-07-26 신설). `MakeDisciplineProfile`과 동일 패턴. |
| `static void ApplyStatProfile(FEmployeeStats&, const TArray<int32>&)` | `MakeStatProfile` 결과(길이 6)를 `FEmployeeStats` 필드에 매핑 — 스폰 경로 2곳(`EmployeeManager`/`RecruitmentManagerSubsystem`)이 공유하는 단일 지점. |

### UEmployeePotentialHelper
잠재큐브(메이플식 % 축) 정적 헬퍼.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Data/EmployeePotentialData.h`

| API | 용도 |
|---|---|
| `static FPotentialModifiers AggregateModifiers(const FPotentialAbility&)` | 큐브 줄 가산 집계 (점수/수익/XP/크리 배선의 SOT). |
| `static bool ResetPotentialAbility(FPotentialAbility&, int32 Slots)` | 큐브 리롤 (티어 래칫 적용). 리롤 UI는 미구현 — 호출처 없음. |

### AFacilityBaseActor
Base class for world facilities (offices, shops) with unlock system, costs, and locked visual state management.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Entity/Facility/FacilityBaseActor.h`

| API | 용도 |
|---|---|
| `bool TryUnlock()` | Attempt to unlock facility; deducts cost; applies unlocked visual; returns success. |
| `bool IsUnlocked() const` | Check if facility is unlocked (accessible). |
| `EFacilityType GetFacilityType() const` | Get facility type (shop, office, etc.). |
| `ECountryType GetOwnerCountry() const` | Get country this facility belongs to. |
| `void SetLockedVisual(bool bLocked)` | Toggle locked material (darkened) on all meshes. |

### FactoryUpgradeUnlockPolicy
벽돌공장 자동 강화 그룹의 HQ 해금 계약을 해석하는 public inline 정책 API.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Data/FactoryUpgradeData.h`

| API | 용도 |
|---|---|
| `bool FactoryUpgradeUnlockPolicy::IsAutoUnlockType(EFactoryUpgradeType)` | `AutoCollection`, `AutoCollectionCapacity`, `BrickStock`의 자동 그룹 3타입인지 판정한다. |
| `bool FactoryUpgradeUnlockPolicy::TryResolveRequiredHQLevel(const TArray<FFactoryUpgradeDefinition>&, int32&)` | 자동 그룹 3행이 모두 존재하고 각 `RequiredHQLevel`이 양수이면서 동일한지 검증한 뒤 공통 HQ 요구 레벨을 반환한다. |

### ABrickFactory
Brick/resource production facility with timed spawning, upgrades, smoke/steam audio-visual effects, and auto-collection system.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Entity/Factory/BrickFactory.h`

| API | 용도 |
|---|---|
| `bool UpgradeFactory(EFactoryUpgradeType UpgradeType)` | Upgrade factory stat (Production, Efficiency, etc.); broadcasts OnFactoryUpgraded; checks cost/level. |
| `int32 GetUpgradeLevel(EFactoryUpgradeType UpgradeType) const` | Get current upgrade level for stat type. |
| `float GetUpgradeValue(EFactoryUpgradeType UpgradeType) const` | Get calculated value/bonus for upgrade level. |
| `bool CanUpgradeAny() const` | Check if any upgrade is available (funds + level cap + unlock conditions met). |
| `void UnlockAutoCollection()` | Unlock automatic resource collection (costs money; permanent). |
| `bool IsAutoCollectionUnlocked() const` | Check if auto-collection is active. |
| `int64 GetAutoCollectedAmount() const` | Get accumulated auto-collected resources pending harvest. |
| `void CollectAutoResources()` | Trigger auto-collection cycle (accumulates pending harvest). |
| `FFactorySaveData GetFactoryData() const` | Export factory state for saving (upgrades, auto-collection, timers). |
| `void SetFactoryData(const FFactorySaveData& InData)` | Restore factory state from save data. |

### AWorkstationActorBase
Base workstation (desk, chair, computer) with seats, upgradeable setup levels, skins, and employee assignment. The Double subclass is absent from live `DT_WorkstationCard` and retained for experiments; only the editor utility `BP_WorkStationIcongenerator` references its BPs, and that utility is outside the current cook roots (`DECISION_RECORDS.md` §8.20).
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Office/WorkstationActorBase.h`

| API | 용도 |
|---|---|
| `void InitializeWorkstation()` | Load workstation type from DataTable and apply setup level; called on BeginPlay. |
| `void SetHighlight(bool bEnabled)` | Toggle highlight mesh (selection feedback). |
| `void RecalcBoxExtent()` | Recalculate collision box to fit all visible meshes. |
| `void RestoreFromSaveData(const FWorkstationSaveData& SaveData)` | Restore workstation state from save (setup level, skins, assignments). |
| `FWorkstationSaveData GetSaveData() const` | Export workstation state for saving. |
| `bool TryUpgradeSetupLevel()` | Validate the next `DT_WorkstationSetupLevel` row and all required meshes, withdraw that row's Diamond cost, advance one linear level, apply equipment/speed, and save once. |
| `EComputerSetupLevel GetCurrentSetupLevel() const` | Get current setup level (Lv1-Lv6). |
| `bool GetCurrentSetupLevelData(FComputerSetupLevelData& OutData) const` | Read the complete current DataTable row for UI/gameplay; returns false when data is missing. |
| `bool GetNextSetupLevelData(FComputerSetupLevelData& OutData) const` | Read the next linear level DataTable row; returns false at Lv6 or when data is invalid/missing. |
| `bool IsMaxLevel() const` | Check if workstation is at max setup level (Level6 또는 Level6Twin). |
| `bool GetMaxVariantData(FComputerSetupLevelData& OutData) const` | 최대 레벨의 반대 외형 행 조회. 최대가 아니거나 행이 없으면 false. |
| `bool CanSwapMaxVariant() const` | 최대 레벨에서 반대 외형으로 바꿀 수 있는지. |
| `bool TrySwapMaxVariant()` | 메시 검증 → Diamond `VariantSwapCost` 정확 인출 → 성공 시에만 외형 전환. **레벨·속도는 불변**(동일 성능 교체). |
| `bool SetSlotSkin(EWorkstationSlot Slot, FName SkinID)` | Apply skin to slot (Laptop, Monitor, Keyboard, etc.); updates mesh/materials; saves. |
| `FName GetSlotSkinID(EWorkstationSlot Slot) const` | Get applied skin ID for slot. |
| `bool IsSlotActive(EWorkstationSlot Slot) const` | Check if slot is visible at current setup level. |
| `void SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh)` | Set mesh for workstation component slot (admin only). |
| `void SetSlotVisibility(EWorkstationSlot Slot, bool bVisible)` | Toggle visibility of slot mesh (admin only). |
| `float GetWorkSpeedBonusRate() const` | Get the current DataTable-driven cumulative workstation speed rate used by assigned employee Score Orb cadence. |
| `EWorkstationType GetWorkstationType() const` | Get workstation type (Single or Double); pure virtual?봫ust override. |
| `int32 GetChairCount() const` | Get chair count (1 for Single, 2 for Double); pure virtual?봫ust override. |
| `bool HasEmptySeat() const` | Check if any seat is unoccupied; pure virtual?봫ust override. |
| `bool CanSitAt(int32 SeatIndex) const` | Check if seat exists and is empty; pure virtual?봫ust override. |
| `int32 SitDown(AOfficeworker* Worker)` | Seat employee in first available seat; return seat index (-1 if full); pure virtual?봫ust override. |
| `bool SitDownAt(int32 SeatIndex, AOfficeworker* Worker)` | Seat employee in specific seat (if empty); pure virtual?봫ust override. |
| `void StandUp(AOfficeworker* Worker)` | Remove employee from seat (any seat); pure virtual?봫ust override. |
| `FVector GetSeatLocation(int32 SeatIndex) const` | Get world position of seat; pure virtual?봫ust override. |
| `FRotator GetSeatRotation(int32 SeatIndex) const` | Get rotation for seated pose; pure virtual?봫ust override. |
| `int32 FindWorkerSeatIndex(AOfficeworker* Worker) const` | Get seat index of employee (or -1 if not seated here); pure virtual?봫ust override. |
| `int32 GetOccupantCount() const` | Get number of seated employees; pure virtual?봫ust override. |
| `AOfficeworker* GetOccupantAt(int32 SeatIndex) const` | Get employee seated at index (nullptr if empty); pure virtual?봫ust override. |
| `bool AssignEmployeeToSeat(int32 SeatIndex, int32 EmployeeID)` | Persist employee ID to seat (offline assignment); pure virtual?봫ust override. |
| `void UnassignSeat(int32 SeatIndex)` | Clear seat assignment (employee ID); pure virtual?봫ust override. |
| `int32 GetAssignedEmployeeID(int32 SeatIndex) const` | Get assigned employee ID for seat (-1 if unassigned); pure virtual?봫ust override. |
| `int32 FindEmptyAssignmentSlot() const` | Find first unassigned seat index (-1 if all full); pure virtual?봫ust override. |
| `bool SetChairSkin(int32 ChairIndex, FName SkinID)` | Apply skin to chair; pure virtual?봫ust override. |
| `bool SetAllChairsSkin(FName SkinID)` | Apply skin to all chairs; pure virtual?봫ust override. |
| `FName GetChairSkinID(int32 ChairIndex) const` | Get applied chair skin ID; pure virtual?봫ust override. |
| `void SetChairMesh(int32 ChairIndex, UStaticMesh* NewMesh)` | Set chair mesh (admin); pure virtual?봫ust override. |
| `void AttachEquipmentToSockets()` | Parent all equipment meshes to desk sockets (BeginPlay); pure virtual?봫ust override. |

### ASingleWorkstationActor
Single-seat workstation (1 or 2 chairs); applies the Lv1-Lv6 DataTable equipment configuration to its center/side laptop, monitor, keyboard, mouse, and computer components.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Office/SingleWorkstationActor.h`

| API | 용도 |
|---|---|
| `void SetWorkstationType(EWorkstationType NewType)` | Set to Single or Double chair mode (affects GetChairCount and seat availability). |
| `void InitializeWorkstation() override` | Load workstation data from table and initialize all chair/equipment components. |
| `void RestoreFromSaveData(const FWorkstationSaveData& SaveData) override` | Restore saved setup level, skins, and chair assignments. |
| `FWorkstationSaveData GetSaveData() const override` | Export all state for saving. |
| `EWorkstationType GetWorkstationType() const override` | Return configured workstation type (Single/Double). |
| `int32 GetChairCount() const override` | Return 1 if Single, 2 if Double. |
| `bool HasEmptySeat() const override` | Check if any chair is free. |
| `bool CanSitAt(int32 SeatIndex) const override` | Check if seat 0-1 exists and is unoccupied. |
| `int32 SitDown(AOfficeworker* Worker) override` | Sit employee in first available chair; return seat index. |
| `bool SitDownAt(int32 SeatIndex, AOfficeworker* Worker) override` | Sit employee in specific chair (0 or 1 if double); return success. |
| `void StandUp(AOfficeworker* Worker) override` | Remove employee from any chair. |
| `FVector GetSeatLocation(int32 SeatIndex) const override` | Get world position for chair seat point. |
| `FRotator GetSeatRotation(int32 SeatIndex) const override` | Get rotation for seated animation. |
| `int32 FindWorkerSeatIndex(AOfficeworker* Worker) const override` | Find which chair employee is in. |
| `int32 GetOccupantCount() const override` | Return number of seated employees (0-2). |
| `AOfficeworker* GetOccupantAt(int32 SeatIndex) const override` | Get employee seated at index. |
| `bool AssignEmployeeToSeat(int32 SeatIndex, int32 EmployeeID) override` | Save employee ID to seat for offline assignment. |
| `void UnassignSeat(int32 SeatIndex) override` | Clear employee ID assignment. |
| `int32 GetAssignedEmployeeID(int32 SeatIndex) const override` | Get offline-assigned employee ID. |
| `int32 FindEmptyAssignmentSlot() const override` | Find first unassigned chair. |
| `bool SetChairSkin(int32 ChairIndex, FName SkinID) override` | Apply skin to chair (0 or 1); updates mesh/materials; saves. |
| `bool SetAllChairsSkin(FName SkinID) override` | Apply skin to both chairs. |
| `FName GetChairSkinID(int32 ChairIndex) const override` | Get applied chair skin. |
| `void SetChairMesh(int32 ChairIndex, UStaticMesh* NewMesh) override` | Set chair mesh (admin). |
| `bool SetSlotSkin(EWorkstationSlot Slot, FName SkinID) override` | Apply skin to monitor/laptop/keyboard (handles multi-location slots like Laptop_Center+Laptop_Side). |
| `bool IsSlotActive(EWorkstationSlot Slot) const override` | Check if slot is visible at current setup level (Lv1 only laptop, Lv2+keyboard, Lv3+ monitors). |
| `void SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh) override` | Set mesh for slot (admin). |
| `void SetSlotVisibility(EWorkstationSlot Slot, bool bVisible) override` | Toggle slot visibility (admin). |
| `void AttachEquipmentToSockets() override` | Parent laptop/monitor/keyboard/mouse/computer to desk sockets. |

### ADecorationActor
Office decoration entity (wall/floor/furniture). Manages mesh placement, unlock level requirements, surface placement rules, and DataTable-driven properties.
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/Office/DecorationActor.h`

| API | 용도 |
|---|---|
| `void SetDecorationMesh(UStaticMesh* NewMesh)` | Set or replace decoration mesh; recalcs box extent. |
| `void RecalcBoxExtent()` | Recalculate collision box to fit mesh bounds. |
| `bool CanPlaceOnSurface(EDecorationSurface Surface) const` | Check if decoration can be placed on surface type (wall/floor). |
| `bool IsUnlocked(int32 CurrentLevel) const` | Check if decoration is unlocked at current building level. |
| `EDecorationSurface GetAllowedSurface() const` | Get allowed placement surface (wall/floor/both). |
| `EDecorationCategory GetCategory() const` | Get decoration category (wall/floor/furniture). |

### AGachaCaptureStage
가챠 리빌용 라이브 캡처 무대(워커 스폰 + SceneCapture → RT). **매프레임 캡처 무대는 월드 동시 1개** — `AcquireShared/ReleaseShared` 로 소비처가 공유.
파일: `Public/UI/Gacha/GachaCaptureStage.h`

| API | 용도 |
|---|---|
| `UTextureRenderTarget2D* BeginReveal(const FEmployeeInstance&, EStageAnimMode=Dance)` | 단발: 워커 1명 스폰 + 코스메틱 + 애니 + 캡처 ON, 표시용 RT 반환 |
| `UTextureRenderTarget2D* BeginRevealBatch(const TArray<FEmployeeInstance>& EmployeesInSlotOrder, EStageAnimMode=Dance)` | 멀티(2~5): 슬롯 순서(C,R1,L1,R2,L2) N명을 **한 무대**에 세우고 와이드 RT 1장 반환(폭 `512+(N−1)×320`, 카메라는 가로비만큼 후퇴 = FOV 불변). `N==1` 이면 `BeginReveal` 위임 |
| `static FVector4 GetSlotUVWindow(int32 SlotIndex, int32 NumSlots, float InGroundLineV=GroundLineV)` | 슬롯 k 의 RT UV 창 `(OffU,OffV,ScaleU,ScaleV)` — `M_GachaRevealRT_Window` 의 `UVWindow` 에 그대로 주입. `NumSlots==1` = 풀 윈도우 |
| `static AGachaCaptureStage* AcquireShared(UObject* Holder, UWorld*, TSubclassOf<AGachaCaptureStage>)` | 살아있는 공유 무대를 인계받거나 스폰 (중복 캡처 방지) |
| `void ReleaseShared(const UObject* Holder)` / `void EndReveal()` | Holder 가 현 소유자일 때만 정리(인계됐으면 no-op) / 캡처 OFF + 워커·무대 정리 |

## MANAGER/SUBSYSTEM (UE5.4 C++)

### UTableManagerSubsystem
Central DataTable repository and accessor for all game tables (UI widgets, buildables, employees, resources, etc.)
파일: `Private/Manager/TableManagerSubsystem.h`

| API | 용도 |
|---|---|
| `TSubclassOf<UUserWidget> GetWidgetClass(EWidgetType Type) const` | Get cached widget class by type (UI entrance point ??use this instead of direct class pointers) |
| `TArray<FBuildableCardTable> GetBuildableInfos() const` | Fetch all buildable construction cards from table cache |
| `FInteractableInfo GetInteractableInfo(FName RowName, bool& bOutSuccess) const` | Resolve interactable (building/actor) metadata by RowName (success flag pattern) |
| `FBuildingData GetBuildingData(FName RowName, bool& bOutSuccess) const` | Get visual/mesh metadata for a specific building type |
| `void GetAllCityDressingRows(TArray<FCityDressingData>& OutRows) const` | `DT_CityDressing`의 패치 슬롯을 `PresetId → SlotId` 순으로 반환. 누락 메시/행은 호출자가 fail-closed 처리 |
| `FResourceInfo GetResourceInfo(EResourceType ResourceType, bool& bOutSuccess) const` | Fetch resource icon, name, color metadata by type |
| `FProjectData GetGameProjectData(int32 ProjectIndex, bool& bOutSuccess) const` | Get project (task) metadata by stage index |
| `FProjectData GetProjectData(ECompanyType CompanyType, int32 ProjectIndex, bool& bOutSuccess) const` | Cross-industry project lookup (used by TradeOrder, WorldMap) |
| `TArray<FBuildingEnhancementDefinition> GetEnhancementsForCompanyType(ECompanyType CompanyType) const` | Get all enhancement slot definitions (icons, names, limits) for a company type, sorted by SortOrder |
| `bool GetBuildingTraitData(FName TraitID, FBuildingTraitTableRow& OutRow) const` | Fetch building trait metadata by ID (gacha pool, category, rarity) |
| `TArray<FBuildingTraitTableRow> GetBuildingTraitsByRarity(ELootBoxRarity Rarity) const` | Get gacha pool by rarity tier for pull randomization |
| `bool GetLaunchLootBand(FName Key, FLaunchLootBandRow& OutRow) const` | 출시 판정 밴드 행 조회 (DT_LaunchLootBand — Low/Mid/High 표시명 + 밴드색). 미스 시 경고 1줄이고 코드 폴백은 없다 |
| `const FComputerSetupLevelData* FindWorkstationSetupLevelData(EComputerSetupLevel Level) const` | 책상 세팅 Lv1~Lv6 행 조회 (DT_WorkstationSetupLevel). 캐시 시점에 행 유효성/이름-enum 정합을 검증하므로 실패 = 데이터 결손이고 코드 폴백은 없다 |

### ULaunchLootManagerSubsystem
출시 전리품 드랍(평점 등급별 DT_LaunchLoot 가중 롤 + 즉시 지급). 파일: `Public/Manager/LaunchLootManagerSubsystem.h`

| API | 용도 |
|---|---|
| `static FName ReviewScoreToTableKey(int32)` | 평점 → Low/Mid/High 테이블 키 (임계 SOT = `MidScoreThreshold 24` / `HighScoreThreshold 32` 상수) |
| `static FString GetScoreBandLabel(int32 BandIndex)` | 평점 구간 표시 라벨("23점 이하"/"24~31점"/"32점 이상") — 임계 상수 파생, 확률표/툴팁 공용 |
| `RollAndGrant(TableKey, Tier, ScaleBasis, ExtraRolls, bGrant, bSaveAfterGrant)` | 가중 롤 + 지급(시뮬 가능). ⚠ Low 는 티어 롤 보너스 제외(2026-08-12) |
| `TArray<FLaunchLootPreviewEntry> GetLootPreview(int32 Tier) const` | 티어 기준 등장 가능 보상 병합 목록 + 테이블별 슬롯당 확률(%), **메인 보상 우선 정렬** — 픽칭 피드 미리보기/확률표(`ULaunchLootOddsWidget`)용 |
| `GetLastLaunchLoot() / ClearLastLaunchLoot()` | 출시 확정 롤 결과 캐시 (보상 리빌 UI 동기 조회) |

### ULaunchReactionSubsystem
출시 후 운영 중 반응 드립(비평가 4 + SNS 3). 파일: `Public/Manager/LaunchReactionSubsystem.h`

| API | 용도 |
|---|---|
| `PrepareReactions(BuildingID, StageData)` / `BeginDrip(BuildingID)` / `CancelDrip(BuildingID)` + `OnReactionReady(BuildingID, FLaunchReaction)` | 출시 확정 시 7건 큐 적재 → 리빌 닫힐에 드립 시작(첫 건 +4s, 이후 8~12s) → 운영 종료에 취소. 레벨 전환 = `Deinitialize` 가 큐 리셋(저장·재생 없음 — 의도) |

### 건물 특성 효과축 v2 (`Public/Enum/BuildingTraitTarget.h` — 헤더 전용 inline)

| API | 용도 |
|---|---|
| `EBuildingTraitTarget GetTargetForCategory(EBuildingTraitCategory)` | 분야 → 주대상 단일 진실원천 (19:19 1:1) |
| `FText GetTraitTargetDisplayName(EBuildingTraitTarget)` | 대상 표시명. ⚠ `UEnum::GetDisplayNameTextByValue` 금지(패키징에서 식별자로 떨어짐) |
| `ETraitTargetPolarity GetTraitTargetPolarity(EBuildingTraitTarget)` | 증가형/감소형. 값은 항상 양수로 저장하고 방향은 대상이 소유 |
| `ETraitTargetUnit GetTraitTargetUnit(EBuildingTraitTarget)` | % / %p / 개 |
| **`FText FormatTraitEffectText(EBuildingTraitTarget, float)`** | **효과 1줄 문구 생성 — UI 는 CSV 텍스트 대신 이것만 쓴다** |

### 건물 특성 집계 (`UBuildingTraitManagerSubsystem`)

| API | 용도 |
|---|---|
| `float GetAggregatedTraitPercent(int32 BuildingIndex, EBuildingTraitTarget)` | 장착+세트+오라+메타 전체 집계 |
| `float GetTraitFactor(int32 BuildingIndex, EBuildingTraitTarget)` | `1 + Pct/100`. ⚠ FlatCount 대상(HRPower/ProductionCount)엔 쓰지 말 것 |
| `float GetRawTraitPercent(int32 BuildingIndex, EBuildingTraitTarget)` | 장착+세트만. AuraPower/MetaAmplify 구할 때 (재귀 차단용) |
| `float GetCompanyWideTraitPercent(EBuildingTraitTarget)` | 전 건물 최댓값. TradeValue 전용(항구가 건물 인자를 못 받음) |
| `void InvalidateTraitCache()` | 슬롯 구성 변경 시 raw 집계 캐시 폐기 |
| `bool IsFullyInitialized() const` | Check if all tables are loaded and cached (gate UI initialization) |
| `FMissionTable GetMissionData(FName MissionID, bool& bOutSuccess) const` | 미션 체인 행 조회 (DT_Mission, RowName = MissionID) |
| `bool GetShopItemRow(FName RowName, FShopItemTable& OutRow) const` | 상점 상품 행 조회 (DT_ShopItem) |
| `TArray<FName> GetShopItemRowsForTab(EShopTab Tab) const` | 탭별 상품 RowName 목록 (SortOrder 오름차순) |
| `bool GetGoalData(const FName& RowName, FGoalTable& OutData) const` | 목표판 목표 행 조회 (DT_Goal, RowName = GoalID) |

### UUIManagerSubsystem
Central UI state and widget lifecycle manager (layer stacks, dialogs, notifications)
파일: `Private/Manager/UIManagerSubsystem.h`

| API | 용도 |
|---|---|
| `void CreateInGameLayerWidget()` | Create and register MainMap HUD (called once per level) |
| `void ShowNotification(const FText& Message, float Duration=3.0f, ENotificationType Type=Normal)` | Display toast message with auto-color by type (Normal/Success/Error/Warning) |
| `void ShowFundsToast(int64 Delta, float Duration=2.0f)` | 수익 수령 전용 컴팩트 토스트. `DT_Resource[Money]` 아이콘·색 + `FormatFundsAmount(Delta, true)`를 사용하며 아이콘 실패 시 숫자만 유지, 별도 알림음 없음 |
| `void ShowRewardToast(const TArray<FMissionReward>& Rewards, const FText& Title, bool bShowTitle)` | `EWidgetType::RewardToast`로 비차단 보상 카드 토스트를 생성. `bShowTitle=true`면 `Title`과 아이콘+`xN`을 함께 표시하고, false면 제목을 접는다 |
| `UConfirmCancelWidget* ShowConfirmDialog(const FText& Title, const FText& Message, FSimpleDelegate OnConfirm, FSimpleDelegate OnCancel=FSimpleDelegate())` | Show modal dialog with C++ callback binding (dual-button, async) |
| `UConfirmCancelWidget* ShowAlertDialog(const FText& Title, const FText& Message, FSimpleDelegate OnConfirm=FSimpleDelegate())` | Show OK-only modal dialog (blocking, single callback) |
| `void ShowColoredNotification(const FText& Message, float Duration, FLinearColor TextColor)` | Display toast with custom RGB (rarity color, score rank, etc.) |
| `void ShowRejectNotification(const FText& Message, float CooldownSec=0.4f, ENotificationType Type=Failed)` | **거부 알림 — 조용한 실패를 막을 때 쓸 것.** 문구를 키로 쿨다운(홀드 10Hz 연타/이중 발화 억제). "이미 ~입니다" 류 상태 안내는 Type=Warning |
| `void NotifyInsufficientResource(EResourceType Type, int64 Need)` | **재화 부족 안내 — "{자원}이(가) {부족분} 부족합니다".** 부족분은 호출 시점 계산, 자원명은 DT_Resource(행 없으면 알림 생략). 뭉뚱그린 "자금이 부족합니다" 대신 이걸 쓸 것 |
| `void ClearAllNotifications()` | Dismiss all active toasts (level load, etc.) |
| `void CloseCurrentDialog()` | Force-dismiss active confirm/alert modal |
| `UInGameLayerWidget* GetInGameLayer() const` | Access MainMap HUD for widget binding or dynamic panel show/hide |
| `UOfficeLayerWidget* GetOfficeLayer() const` | Access OfficeMap HUD for panel toggle, UI updates during building management |
| `void ClearAllUI()` | Destroy all active UI widgets on level transition |
| `FOnUIInitialized OnInitializeSuccess` | Delegate: fired when Base UI (canvas, layers, containers) are fully created |
| `FOnUIResourceChanged OnUIResourceChanged` | Delegate: fired when ResourceItemManager changes (Type, NewValue) ??UI widgets subscribe |

### UInGameLayerWidget
MainMap HUD 레이어. 월드 위 동적 게이지·버블 풀과 튜토리얼 설명 타겟을 소유한다.
파일: `Public/UI/Panel/InGameLayerWidget.h`

| API | 용도 |
|---|---|
| `void ReserveTutorialVaultGauge(int32 BuildingIndex)` | M7 설명 대상 건물의 **기존** VaultGauge 풀 슬롯을 예약한다. 예약 대상은 `Hidden` LOD·중앙 근접 캡보다 우선하며 설명 동안 Bar 표현을 유지한다. 임시 중복 게이지는 만들지 않는다 |
| `void ReleaseTutorialVaultGauge()` | M7 설명 예약을 해제하고 다음 게이지 재평가에서 정상 LOD·거리 캡 정책으로 복귀한다. Explain 이탈·미션 교체·레이어 재준비·서브시스템 종료 경로에서 호출 |
| `void SetPlacementBuildingMarker(ABuildingBaseActor* Building, UPlacementHandler* PlacementHandler)` | 배치 프리뷰 지붕 위 셰브론을 설정/해제한다. 기존 선택 셰브론 슬롯을 공유하고 `배치 > 선택` 우선순위로 합성하며 `CanDropHere()`에 따라 초록/코랄 상태를 갱신한다 |

### UMissionManagerSubsystem
미션 체인 (오프닝 튜토리얼 = 첫 미션들). DT_Mission 단일 진실, 트래커 1개 + 가이드 펄스 물결 소프트 유도
파일: `Public/Manager/MissionManagerSubsystem.h`

| API | 용도 |
|---|---|
| `void TryStartMissionChain()` | MainMap StartPlay(정상 모드)에서 호출 — 진행 미션 있으면 트래커/가이드 구성 |
| `void NotifyBuildModalOpened(UCommonActivatableWidget* Modal)` | 빌드 모달 열림 신호 (가이드 페이즈 전진) |
| `void NotifyBuildingPlaced(ABuildingBaseActor* Building)` | 신규 건물 배치 확정 신호 (BuildFirstBuilding 완료) |
| `bool HasActiveMission() const` / `FName GetActiveMissionID() const` | 활성 미션 조회 (SaveGameData가 ID 수집) |
| `bool IsTutorialCompleted() const` | 저장된 명시적 튜토리얼 완료 상태 조회. GoalBoard unlock과 독립이며 M7 최종 클레임 원자 저장에서 true |
| `bool GetActiveMission(FMissionTable& OutMission) const` | 활성 미션 행 (트래커 렌더) |
| `bool GetMissionProgress(int64& OutCurrent, int64& OutTarget) const` | 누적형(CollectBricks) 진행도 |
| `FText GetCurrentMentorLine() const` | 현 가이드 페이즈 멘토 1줄 (DT MentorLines) |
| `UWidget* GetCurrentHighlightTarget() const` | 펄스 물결 타겟 위젯 (오버레이가 매 틱 조회) |
| `EGestureHintKind GetCurrentGesture() const` | 현 가이드 페이즈의 제스처 힌트 종류(None/Tap/Hold/Drag). 판정은 순수 함수 `GuideGestureRules::ResolveGesture`(`Public/Manager/GuideGestureRules.h`) ― 오버레이가 매 틱 조회해 손을 띄우고 **셰브론을 끈다** |
| `AActor* GetCurrentGestureAnchorActor() const` | 손끝을 둘 월드 액터 (M1 = `ABrickFactory`, M2/M4 배치 확정 = `UPlacementHandler::GetPlacementTargetActor()`). 투영·캔버스 배치는 오버레이 책임 |
| `float GetGuideDimScale() const` | 딤 알파에 곱하는 페이즈 계수 ― 배치 확정(M2·M4)과 M1 `Grind` 만 **0**(딤 없이 링·손만), 그 외 1. 오버레이가 `ResolveWidgetTarget(..., DimScale)` 로 그대로 넘긴다 |
| `void RegisterBuildPlacementPanel(UBuildPlacementPanelWidget*)` / `void UnregisterBuildPlacementPanel(UBuildPlacementPanelWidget*)` | 배치 바 핸들 등록/해제 (패널 `NativeConstruct`/`NativeDestruct`, 해제는 identity check ― 안 하면 스택에서 빠진 바를 몇 틱 더 링 타겟으로 돌려준다). 배치 확정 페이즈의 링 타겟 = `UBuildPlacementPanelWidget::GetPlaceButtonWidget()` |
| `OnMissionActivated / OnMissionCompleted / OnGuidePhaseChanged / OnMissionProgressChanged` | 델리게이트 (트래커/오버레이 구독) |
| `bool IsHardGateMission() const` | 현재 미션이 하드 게이트 구간인지 (DT `bHardGate`) — 소프트 존은 입력 차단 없이 지연 힌트로 유도 |
| `bool UsesDelayedHint() const` | 이 미션이 지연 힌트(무진전 5s 후 점등)를 쓰는지. 현행 자력 완주 7미션은 전부 하드 존이라 false이며, 이후 소프트 미션 추가 시 오버레이 점등 지연 판정의 단일 창구로 재사용 |
| `double GetPhaseElapsedSeconds() const` | 현 가이드 페이즈 진입 후 경과(초). 오버레이 지연 힌트(5s) 판정용 |
| `FTutorialMissionCompletionRules::Resolve(bool bHasNextMission)` | 튜토리얼 완료 정책 순수 함수. 다음 행이 있으면 progression supply 유무와 무관하게 `ImmediateAdvance`, 체인 끝이면 `FinalClaim` |
| `FTutorialMissionRestoreRules::ShouldRestoreClaimReady(...)` | 저장된 최종 클레임 대기를 복원할지 판정하는 순수 함수. 저장/복원 MissionID가 같고 활성 체인 마지막 행일 때만 true |
| `bool IsExplainCameraSettled() const` | M7 설명 페이즈(`CollectGuide::Explain`)의 카메라 글라이드가 끝났는지 ― 오버레이가 매 틱 조회해 딤만 깔지(대기) 구멍을 뚫을지(설명) 가른다. 내부는 `APlayerCamera::IsFocusVisuallySettled()`(완료 플래그가 아니다 ― 그 함정은 위 항목), **카메라를 못 찾으면 true** = 대기하지 않는다. M7 완료는 실제 `OnRevenueCollected(Amount > 0)`만 인정 |
| `FOnConditionSignal OnConditionSignal` | 델리게이트(멀티캐스트, `EMissionConditionType`) — 조건 훅의 **시그널 허브**. `UGoalBoardSubsystem` 이 구독해 목표를 판정 |

### UGoalBoardSubsystem
목표판 — 튜토리얼 체인 종료 후 동시 노출·자유 순서 목표(DT_Goal 11행). MissionManager 의 `OnConditionSignal` 을 구독해 판정하고, 수령은 **전건 수동** `[확정 2026-08-08]` — 달성 시점 무관하게 `Claimable` 래치만 하고, `ClaimGoal()` 은 트래커 [수령] 버튼에서만 호출한다(자동 지급 없음)
파일: `Public/Manager/GoalBoardSubsystem.h`

| API | 용도 |
|---|---|
| `bool IsUnlocked() const` | 보드 언락 여부 (체인 종료 시 true) — 트래커 목표 모드 진입 게이트 |
| `void UnlockBoard()` | 언락 prepare + 세이브 + UI publish를 묶은 기본 API. 호출처 = 치트 `UnlockGoalBoard` |
| `bool PrepareUnlockForAtomicSave()` / `void RollbackPreparedUnlock()` / `void PublishPreparedUnlock()` | 피날레 단일 저장 경계용 prepare/rollback/publish API. 상태·전수 재평가를 먼저 적용하되 저장/알림을 억제하고, 저장 실패면 준비 상태를 복원하며, 통합 저장 및 `OnMissionCompleted` 뒤에만 UI 변경을 발행 |
| `void GetBoardEntries(TArray<FGoalBoardEntry>& OutEntries) const` | 보드 리스트용 스냅샷 (SortOrder 오름차순, 상태·진행도 포함). `G1_RaiseFloor`은 저장된 GoalID별 누적값과 목표 5를 `ProgressCurrent/Target`으로 제공 |
| `bool GetRepresentativeGoal(FGoalBoardEntry& OutEntry) const` | 트래커 대표 목표 1건 — Claimable 최우선 → 진행률 최고 → SortOrder. 전부 수령이면 false. **트래커 가시성 판단은 이걸로** |
| `bool ClaimGoal(FName GoalID)` | [수령] — Claimable(충족 + 선행 수령)에서만 지급. 성공 시 세이브 + 밴드 토스트 + 브로드캐스트 |
| `void CollectSaveData(FGameSaveData& OutData) const` | 보드 unlock/completed/claimed와 `GoalEventProgressByID` 누적값을 세이브 구조체로 수집 (`SaveLoadManager::SaveGameData` 가 호출) |
| `FOnGoalBoardChanged OnGoalBoardChanged` | 델리게이트 — 목표 충족/수령/언락 시. 트래커·보드 패널이 구독해 재렌더 |
| `void DevCompleteGoal(FName GoalID)` / `void DevResetGoals()` | 치트 백엔드 (`CompleteGoal` / `ResetGoals`) |

### UPanelIntroSubsystem
패널 최초 진입 코치마크("본 적 있는가")와 제스처 힌트 졸업("몇 번 해냈는가")을 같이 들고 세이브하는 서브시스템. 둘 다 치트 `ResetPanelIntro` 하나가 리셋한다
파일: `Public/Manager/PanelIntroSubsystem.h`

| API | 용도 |
|---|---|
| `bool ShouldPlay(FName PanelKey) const` | 이 패널의 최초 진입 안내를 아직 안 봤는지 (딤+구멍+말풍선 재생 게이트) |
| `void MarkSeen(FName PanelKey)` | 봤다고 기록 + 세이브 (`FGameSaveData::SeenPanelIntros`) |
| `int32 GetHintCount(FName Key) const` | 제스처 힌트 성공 누적 횟수 (없으면 0) |
| `void IncrementHint(FName Key)` | 성공 1회 기록 + 세이브. **호출마다 풀세이브라 졸업 뒤에는 부르지 말 것** (`NAME_None` 은 무시) |
| `bool IsHintGraduated(FName Key, int32 Threshold) const` | `GetHintCount >= Threshold` ― 캐치 힌트는 키 `CatchDoze`/`CatchBolt` · 임계 3(`CatchHintRules`) |
| `void ResetAll()` | 코치마크 + 힌트 카운트 전부 초기화 + 세이브 (치트 `ResetPanelIntro` 백엔드) |
| `void CollectSaveData(FGameSaveData& OutData) const` | `SeenPanelIntros` + `GestureHintCounts` 를 세이브 구조체로 수집 |

### UEmployeeManager
Employee lifecycle: hire, fire, assign to buildings, track stats, enhance, portraits
파일: `Public/Manager/EmployeeManager.h`

| API | 용도 |
|---|---|
| `bool HireEmployee(EEmployeeDepartment Department, int32 EnhancementLevel=0)` | Create new intern-level employee (randomized appearance, name, stats by dept) |
| `bool HireEmployeeFromCard(const FEmployeeInstance& CardData, int32 BuildingIndex)` | Hire from gacha result (preserves appearance, potential, additional options) |
| `bool FireEmployee(int32 EmployeeInstanceID)` | Terminate and unassign employee (frees chair, removes from building) |
| `TArray<FEmployeeInstance> GetAllEmployees() const` | Iterate all hired employees across all buildings |
| `TArray<FEmployeeInstance> GetEmployeesInBuilding(int32 BuildingIndex) const` | Get staff roster for a single building |
| `TArray<AOfficeworker*> GetSpawnedWorkersInBuilding(int32 BuildingIndex) const` | Get runtime actor references for state inspection (position, animation, buffs) |
| `bool AssignEmployeeToBuilding(int32 EmployeeID, int32 BuildingIndex)` | Place unassigned employee in building workstation (인원이 실제로 +1 되는 호출은 정원 게이트를 통과해야 성공) |
| `bool UnassignEmployee(int32 EmployeeID)` | Remove from workstation and return to unassigned pool |
| `int32 GetEmployeeCountInBuilding(int32 BuildingIndex) const` | 해당 건물 소속 인원(벤치 포함). 고용 상한의 분자 — 별도 카운터 없이 EmployeeList 에서 파생 |
| `int32 GetBuildingEmployeeCapacity(int32 BuildingIndex, bool bLogIfZero = true) const` | **게임 내 유일한 인원 상한.** 세이브(증축 층수)+DT(`FBuildingData`) 파생. 0 이면 진단 로그 — **표시 경로는 `bLogIfZero=false`**(정상 0 이 로그를 덮음). 책상은 상한이 없다 |
| `int32 GetBuildingEmployeeCapacityAtFloors(int32 BuildingIndex, int32 AddedFloors, bool bLogIfZero = true) const` | 지정 증축 층수 기준 인원 상한(빌드업 다음 층 미리보기). 위 함수의 공통 구현 — AddedFloors < 0 = 세이브 현재 층수 |
| `bool CanHireIntoBuilding(int32 BuildingIndex) const` | 이 건물에 1명 더 넣을 수 있는지. 채용 진입 사전차단·고용 게이트의 단일 판정(**인원 상한** 기준) |
| `static bool IsUnderCapacity(int32 CurrentCount, int32 Capacity)` | 상한 판정 순수 함수 (월드 의존 없음 — 자동화 테스트 진입점) |
| `static FText GetCapacityFullMessage()` | 인원 초과 안내 문구. 사전차단과 고용 게이트가 같은 문구를 써야 원인이 하나로 읽힌다 |
| `FEmployeeInstance* GetEmployeeData(int32 EmployeeID)` | Get mutable employee data pointer (stats, experience, level, rank) |
| `bool AddExperience(int32 EmployeeID, float Amount)` | Grant experience (level-up cascade + OnExperienceGained broadcast) |
| `void DistributeExperienceToBuilding(int32 BuildingIndex, float BaseAmount, EQualityGrade QualityGrade)` | Split exp among building staff, scaled by product quality grade |
| `float GetQualityMultiplier(EQualityGrade Grade) const` | Return exp scale: S??.0x, A??.0x, etc. (DT-driven, not hardcoded) |
| `bool EnhanceEmployee(int32 TargetEmployeeID, TArray<int32> MaterialEmployeeIDs, float GaugePercentage)` | Merge materials into target (level cap by max, calc success chance, consume materials) |
| `void InvestEmployeeStat(int32 EmployeeID, EEmployeeStatIndex StatIndex)` | Invest 1 unused stat point + save + OnEmployeeStatsChanged broadcast |
| `void AutoDistributeStats(int32 EmployeeID)` | Dump all unused points into department primary stat + save + broadcast |
| `bool RerollEmployeePotential(int32 EmployeeID)` | Potential cube reroll: Money cost gate (GetPotentialResetCost) + ResetPotentialAbility + save + OnEmployeeStatsChanged (false = insufficient funds, no charge) |
| `FOnEmployeeStatsChanged OnEmployeeStatsChanged` | Delegate: (EmployeeID) fired after invest/auto-distribute/potential reroll (UI partial refresh) |
| `FOnEmployeeSelectionChanged OnEmployeeSelectionChanged` | Delegate: (OldID, NewID) fired when player clicks employee portrait or card |
| `FOnEmployeeHireCompleted OnEmployeeHireCompleted` | Delegate: (EmployeeID) fired after portrait capture finishes (gacha result ??hire flow) |
| `FOnExperienceGained OnExperienceGained` | Delegate: (EmployeeID, Amount) fired per exp gain (UI toast float-up animation) |

### UEntityManager
World-level entity registry (buildings, interactables) and unlocking system
파일: `Public/Manager/EntityManager.h`

| API | 용도 |
|---|---|
| `const TArray<FBuildingEntitySaveData>& GetBuildingsData() const` | Fetch all building save snapshots (position, rotation, level, staff, resources) |
| `void SetBuildingsData(const TArray<FBuildingEntitySaveData>& InBuildings)` | Restore building entities from save file (called by SaveLoadManager) |
| `int32 GetUnlockedConstructionLevel() const` | Get highest building tier unlocked (gates new construction templates) |
| `void SetUnlockedConstructionLevel(int32 InLevel)` | Advance unlock after building completion (SaveLoadManager calls on progression) |
| `bool IsBuildingUnlocked(int32 BuildingConstructionLevel) const` | Check if a given tier is available for purchase |
| `void UnlockNextBuilding()` | Increment unlock level by 1 (building placement completion hook) |
| `const TArray<ABuildingBaseActor*>& GetBuildings() const` | Iterate all placed buildings in world |
| `ABuildingBaseActor* GetBuildingByIndex(int32 InBuildingIndex)` | Find building actor by its unique BuildingIndex (null if not spawned) |
| `void AddBuilding(ABuildingBaseActor* Building, bool bRequireBuild=true)` | Register new building actor (construction state, OnBuildingCountChanged broadcast) |
| `void RemoveBuilding(ABuildingBaseActor* Building)` | Unregister building (demolish or sale) |
| `FSimpleMulticastDelegate OnBuildingCountChanged` | Delegate: fired when building list size changes (UI counter refresh) |
| `bool HasFreeCapacityOnPlot(FName PlotId, int32 Capacity) const` | 부지 로컬 여유 = 그 부지 건물 수 < `DT_CityPlot.BuildingCapacity` (몰아짓기 방지) |
| `bool HasFreeBuildingSlot(int32 MaxBuildingCount) const` | 전역 여유 = 전체 건물 수 < 본사 레벨 상한. 상한은 `USaveLoadManager::GetMaxBuildingCount()` 에서 받는다 |

### UResourceItemManager
Player resource currency management (Money, Brick, Cash, etc.) with auto-save and broadcasting
파일: `Public/Manager/ResourceItemManager.h`

| API | 용도 |
|---|---|
| `int64 GetResourceAmount(EResourceType Type) const` | Query current balance for a resource type |
| `bool HasResource(EResourceType Type, int64 Amount) const` | Check if player has at least Amount (used by purchase gates) |
| `bool CanAffordCosts(const TArray<FConstructionCost>& Costs, EResourceType& OutMissingType) const` | Multi-resource purchase gate: true if all costs affordable, else false + first missing type (toast via GetResourceInfo().DisplayName). Used by build/placement gates |
| `void StoreResource(EResourceType Type, int64 Amount, bool bShouldSave=true, bool bShouldBroadcast=true)` | Add resources. 피날레 원자 커밋은 save/broadcast를 모두 끄고 디스크 저장 성공 뒤 변경 이벤트를 공개한다 |
| `void SpendResource(EResourceType Type, int64 Amount, bool bShouldSave=true)` | Remove resources (cost, penalty) ??does not check balance, caller gates |
| `bool ExtractResource(EResourceType Type, int64 Requested, int64& Withdrawn, bool bShouldSave=true)` | Withdraw up to Requested (partial allowed, returns actual); false if Type unavailable |
| `const TMap<EResourceType, int64>& GetAllResources() const` | Bulk resource snapshot for save serialization |
| `void SetAllResources(const TMap<EResourceType, int64>& InResources, bool bShouldBroadcast=true)` | Restore all resources; 원자 롤백은 외부 업로드·UI 부수효과를 막기 위해 broadcast를 끌 수 있다 |
| `FOnResourceChanged OnResourceChanged` | Delegate: (Type, NewValue) fired on any Store/Spend/Extract ??UIManagerSubsystem relays to all UI |

### USaveLoadManager
Game persistence layer: serialize/deserialize all subsystem data, building/HQ/title progression
파일: `Public/Manager/SaveLoadManager.h`

| API | 용도 |
|---|---|
| `bool SaveGameData()` | Snapshot all manager data to save file (called by ResourceItemManager, WorldMapManager) |
| `bool LoadGameData()` | Restore all manager data from save file (called at game start) |
| `USaveGame_GameData* GetCurrentSaveData()` | Access cached save data struct (avoids reload each query) |
| `void InvalidateCache()` | Clear save cache after Save/Load (used by GI transition hooks) |
| `int32 GetBuildingTier(int32 BuildingIndex)` | 이 빌딩의 프로젝트 티어(1~10). **오피스 미진입 신축 빌딩은 1** — 티어는 `OfficeDataMap` 에 살아서 "엔트리 없음"이라는 제3 상태가 있고 여기서 흡수한다 |
| `int32 GetMaxBuildingTier()` | 보유 빌딩 중 최고 티어 (랭킹 `mt` 업로드) |
| `int32 CountBuildingsAtTier(int32 MinTier)` | MinTier 이상 빌딩 수 (HQ 조건 + 회사 등급 승격이 공유) |
| `ECompanyType GetBuildingCompanyType(int32 BuildingIndex)` | 빌딩 산업 (티어 로드맵 강화 노출 필터. `FindBuildingSaveData` 는 private) |
| `int32 GetHQLevel()` | Get HQ building current level (gates enhancement slot unlocks, feature progression) |
| `bool TryLevelUpHQ()` | Attempt HQ level up (checks resources, broadcasts OnHQLevelUp) |
| `ECompanyTitle GetCompanyTitle()` | Get current company rank (Star, Corp, MNC, etc.) for UI and progression gates |
| `bool TryPromoteTitle()` | Advance company rank (checks completion counts, broadcasts OnCompanyTitleChanged) |
| `bool IsIndustryUnlocked(ECompanyType CompanyType)` | Check if an industry type is unlocked based on HQ level (HQ spine gates industry availability) |
| `int32 GetIndustryRequiredHQLevel(ECompanyType CompanyType)` | Get minimum HQ level required to unlock an industry (from DT_CompanyInfo.RequiredHQLevel) |
| `int32 GetMaxBuildingCount()` | 현재 HQ 레벨의 건설 가능 건물 상한 (`DT_HQLevel.BuildingSlots`, 세이브 없는 파생 상태). 개수 비교는 `UEntityManager::HasFreeBuildingSlot` 이 한다 |
| `bool HasNextHQLevel()` | 다음 레벨 행 존재 = 만렙 아님. 상한 안내 문구가 만렙에서 "레벨을 올리세요"로 거짓말하지 않게 하는 분기 |
| `void CalculateOfflineGains(float OfflineSeconds)` | Apply server-calc idle income to all buildings (ProductionOrderManager integration) |
| `FOnBuildingLevelUp OnBuildingLevelUp` | Delegate: (BuildingIndex, NewLevel) fired on building level-up (UI unlock animation, unlock checks) |
| `FOnHQLevelUp OnHQLevelUp` | Delegate: (NewLevel) fired on HQ level-up (feature unlock toast, enhancement unlock) |
| `FOnGameDataLoaded OnGameDataLoaded` | Delegate: fired after LoadGameData completes (UI refresh, state machine advance) |
| `FOnOfflineGainsApplied OnOfflineGainsApplied` | Delegate: (TotalGained, OfflineSeconds) fired after CalculateOfflineGains (A6 modal trigger) |

### USoundManagerSubsystem
Audio playback and mixing: UI sounds, SFX, music, volume control, settings persistence
파일: `Public/Manager/SoundManagerSubsystem.h`

| API | 용도 |
|---|---|
| `void PlayUISound(FGameplayTag SoundTag)` | Play UI SFX by GameplayTag (button click, confirm, error beep) ??main UI entry point |
| `void PlayUISoundWithParams(FGameplayTag SoundTag, float VolumeScale, float PitchScale=1.0f)` | UI SFX + 콜별 볼륨/피치 스케일(DT 배율에 곱) — 고빈도 반복음의 연속 감쇠/피치 랜덤용. 선례: orb 착지 틱(OfficeMainWidget::StartStripStageJuice) |
| `void PlaySound(FName SoundID)` | Play 2D SFX (building construct, gacha pull, employee promotion) |
| `void PlaySoundAtLocation(FName SoundID, FVector Location)` | Play 3D SFX in world space (building activity, villager chatter) |
| `void PlaySoundWithVolume(FName SoundID, float VolumeScale)` | Play with envelope multiplier (fade envelope, time-based attenuation) |
| `void PlayMusic(EMusicType MusicType, bool bForceRestart=false)` | Play or resume background music (fade-in handled internally) |
| `void StopMusic(float FadeOutDuration=1.0f)` | Stop music with crossfade duration |
| `EMusicType GetCurrentMusicType() const` | Query active music track (avoid redundant playback) |
| `void SetMasterVolume(float Volume)` | Global volume slider (0.0 to 1.0, OnVolumeChanged broadcast) |
| `void SetCategoryVolume(ESoundCategory Category, float Volume)` | Control SFX/Music/Ambient/Voice volumes independently |
| `void SetMuted(bool bMute)` | Mute all audio (settings toggle, system interruption handling) |
| `float GetCategoryVolume(ESoundCategory Category) const` | Query current volume for a category |
| `FGameAudioSettings GetAudioSettings() const` | Get struct: Master, all Category volumes, mute state (UI binding) |
| `void ApplyAudioSettings(const FGameAudioSettings& NewSettings)` | Batch apply all volumes and mute state (settings modal confirm) |
| `void SaveAudioSettings()` | Persist audio config to local storage (settings panel exit) |
| `void LoadAudioSettings()` | Restore audio config from local storage (game start) |
| `FOnVolumeChanged OnVolumeChanged` | Delegate: (Category, NewVolume) fired on any volume change (UI slider update, icon state) |

### USpawnManager
Building and entity spawning at runtime (from save data or player placement)
파일: `Public/Manager/SpawnManager.h`

| API | 용도 |
|---|---|
| `AInteractableBaseActor* SpawnBuilding(const FInteractableInfo& InteractableInfo, const FBuildingEntitySaveData* LoadData=nullptr)` | Instantiate building actor from table metadata + optional save state (restore on load) |
| `AInteractableBaseActor* SpawnNewBuilding(const FInteractableInfo& InteractableInfo, bool bIsNewlyPlaced=false)` | Create fresh building with default state (player placement, first time construction) |
| `ACityPlotActor* GetSpawnedPlotById(FName PlotId) const` | 런타임 PlotId로 도시 부지 Actor 조회 |
| `AVacantPlotDressingManager* GetVacantPlotDressingManager() const` | 월드당 하나인 동적 빈 부지 HISM 매니저 조회 |

### AVacantPlotDressingManager

빈 부지의 벤치·자전거 패치를 메시별 HISM으로 관리하며, 저장 없이 현 도시 상태에서 가시성을 파생한다. Riverwalk 스트리밍 완료 뒤 기존 고정 ISM/HISM의 개별 바운드를 먼저 샘플링하므로 수목·가로시설과 겹치는 후보는 생성하지 않는다.
파일: `Public/Entity/Ambient/VacantPlotDressingManager.h`

| API | 용도 |
|---|---|
| `void UpdateBuildingPreview(FName PlotId, FVector2D Center, FVector2D HalfExtent)` | 회전 반영된 활성 건물 프리뷰 footprint를 갱신하고 이전·새 부지를 재평가 |
| `void ClearBuildingPreview()` | 프리뷰 점유를 지우고 이전 부지를 확정 건물 기준으로 복원 |
| `void RefreshPlot(FName PlotId)` | 특정 부지의 회사·건물·프리뷰 겹침만 다시 계산 |
| `void RefreshPlots(FName FirstPlotId, FName SecondPlotId)` | 건물 이동의 원본·목적지처럼 두 부지를 중복 없이 함께 갱신 |
| `void RefreshAllPlots()` | 건물 추가·삭제 안전망으로 전체 부지를 이벤트 기반 재평가 |
| `int32 GetPatchCount() / GetHISMComponentCount() / GetVisiblePatchCount()` | 런타임 드레싱 진단·성능 스모크용 읽기 전용 카운트 |

### UCityAcquisitionManager — visual clear boundary

| API | 용도 |
|---|---|
| `FOnCompanyVisualCleared OnCompanyVisualCleared` | 철거 상태 전환 시점이 아니라 회사 Actor가 실제 Hidden/소멸된 뒤 발생; 빈 부지 소품 등 시각 점유 해제에 사용 |

### ATimeCycleManager
게임 내 시간 진행과 일출·일몰 이벤트를 관리하는 액터
파일: `Public/TimeCycle/TimeCycleManager.h`

| API | 용도 |
|---|---|
| `FTimeCycleCode GetCycleDuration() const` | 배치된 하루 사이클 소요 시간을 읽기 전용으로 조회(Python 캡처 검증 포함) |
| `FTimeCycleCode GetSunSetTime() const` | 현재 일몰 시각을 읽기 전용으로 조회 |
| `bool ApplyTransientSunSetTimeForPreview(FTimeCycleCode NewTime)` | DevelopmentOnly: 에디터의 권위 game world에서만 유효한 일몰 시각을 transient 적용하고 내부 캐시를 동기화. packaged runtime은 항상 거부 |

### ATimeCycleSky
단일 방향광·SkyLight·노출을 낮/밤 위상으로 구동하는 하늘 액터
파일: `Public/TimeCycle/TimeCycleSky.h`

| API | 용도 |
|---|---|
| `FTimeCycleSkyDayLook GetDayLook() const` | 배치된 낮 강도·SkyLight·노출 값을 읽기 전용 snapshot으로 조회 |
| `FTimeCycleSkyNightLook GetNightLook() const` | 현재 야간 룩 5개 값을 설정 구조체로 조회 |
| `bool ApplyTransientNightLook(const FTimeCycleSkyNightLook& NewLook)` | DevelopmentOnly: 에디터의 권위 game world에서만 유효한 야간 룩을 transient 적용. packaged runtime은 항상 거부하며 에셋 기본값은 저장하지 않음 |

### UItemInventoryManager
Consumable item inventory (recruitment tickets, enhancement materials, etc.)
파일: `Public/Manager/ItemInventoryManager.h`

| API | 용도 |
|---|---|
| `int32 GetItemCount(EItemType Type) const` | Query item quantity by type (tickets remaining, materials) |
| `bool HasItem(EItemType Type, int32 Amount=1) const` | Check if player has sufficient quantity (used by purchase gates) |
| `void AddItem(EItemType Type, int32 Amount, bool bShouldSave=true, bool bShouldBroadcast=true)` | Grant items. 피날레 원자 커밋은 save/broadcast를 모두 끄고 저장 성공 뒤 이벤트를 공개한다 |
| `bool SpendItem(EItemType Type, int32 Amount, bool bShouldSave=true)` | Consume items (gacha pull, enhancement ritual); false if insufficient (no deduction) |
| `const TMap<EItemType, int32>& GetAllItems() const` | Bulk item snapshot for save serialization |
| `void SetAllItems(const TMap<EItemType, int32>& InItems, bool bShouldBroadcast=true)` | Restore all items; 원자 롤백에서는 transient UI 이벤트를 막기 위해 broadcast를 끌 수 있다 |
| `FOnItemChanged OnItemChanged` | Delegate: (Type, NewCount, Delta) fired on Add/Spend (UI badge, notification) |

### URecruitmentManagerSubsystem
Gacha/recruitment system: pull, pity, mileage, office seat tracking, gacha result lifecycle
파일: `Public/Manager/RecruitmentManagerSubsystem.h`

| API | 용도 |
|---|---|
| `bool ExecuteGachaPull(EGachaTier Tier, int32 BuildingIndex, FGachaResultData& OutResult)` | Execute gacha: deduct cost (ticket?묭iamond), roll rarity+potential, generate employee, apply pity |
| `bool ExecuteGachaPullBatch(EGachaTier, int32 RequestedCount, int32 BuildingIndex, TArray<FGachaResultData>& OutResults)` | 멀티 뽑기(전량 ×N ≤5) — 루프-오브-싱글 + 실행 시점 재클램프(티켓·정원·5), **티켓 전용**(다이아 폴백 off), 세이브 끝 1회 |
| `bool CanExecuteGachaPull(EGachaTier Tier) const` | Check cost availability (tickets or Diamond balance sufficient) |
| `static EItemType GetTicketTypeForTier(EGachaTier Tier)` | 티어별 전용 채용권 타입 — **CTA 라벨과 배치 API 가 같은 티켓을 보게 하는 단일 지점**(하드코딩 중복 방지) |
| `int32 GetTotalPermanentEmployeeCapacity(bool& bOutComplete) const` | 완공된 회사 건물의 `FBuildingData::GetEmployeeCapacity(증축층)` 합계. DT 누락/오버플로면 `bOutComplete=false`로 지급을 중단 |
| `int32 ReconcilePermanentCapacityTickets(bool bShouldSave=true)` | 계정 전역 영구 정원 high-water 증가분만 일반 채용권으로 지급. 모든 지급에 `일반 채용권 획득` 제목으로 `ShowRewardToast(..., true)`를 호출하며, 철거·재건·재접속으로는 재지급하지 않음 |
| `int32 GetFreeCapacity(int32 BuildingIndex) const` | 남은 정원(상한 − 현재 직원) = 멀티 수량 클램프의 분모. 패널 라벨과 배치 API 가 공유 |
| `bool ConfirmGachaHire(const FGachaResultData& ResultData)` | Commit gacha result: hire employee, assign chair, increment mileage, OnGachaPullCompleted broadcast |
| `bool ConfirmGachaHireBatch(const TArray<FGachaResultData>&, AWorkstationActorBase* PreferredWs, int32& OutSeated, int32& OutBenched)` | 일괄 채용 — **뽑기 순서 유지**(사번 표시=실제), 첫 명만 preferred 책상, 세이브 끝 1회. 착석/벤치 수를 집계 토스트용으로 반환 |
| `void DiscardGachaResult()` | Reject gacha result without hiring (mileage still granted, clear pending) |
| `int32 GetPityCount(EGachaTier Tier) const` | Get current pity counter (pulls since last 5??or 4?? |
| `int32 GetMileagePoints() const` | Get mileage score (accumulated from all gacha pulls, 200pt ??Legendary exchange) |
| `bool ExchangeMileage(int32 BuildingIndex, FGachaResultData& OutResult)` | Spend 200 mileage for guaranteed Legendary (same hire path as gacha) |
| `bool SpendMileage(int32 Points, bool bShouldSave=true)` | Deduct arbitrary mileage (shop mileage-tab purchases); false if insufficient |
| `int32 GetHRPower(int32 BuildingIndex) const` | Sum all HR staff enhancement levels in building (boosts gacha rarity rates) |
| `int32 GetOfficeTotalSeats(int32 BuildingIndex)` | Get max workstation capacity for building (from WorkstationManager) |
| `int32 GetOfficeOccupiedSeats(int32 BuildingIndex)` | Get current staff count in building |
| `bool HasAvailableSeat(int32 BuildingIndex)` | Check if building has free chair (gates hiring) |
| `void IncrementOccupiedSeats(int32 BuildingIndex)` | Increment occupancy (hire, EmployeeManager calls) |
| `void DecrementOccupiedSeats(int32 BuildingIndex)` | Decrement occupancy (fire) |
| `void SyncTotalSeats(int32 BuildingIndex, int32 ActualTotalSeats)` | Synchronize seat total after workstation restore from save (avoid drift) |
| `void InitializeOfficeRecruitment(int32 BuildingIndex)` | Create office recruitment data (chair array) on new building construction |
| `void RemoveOfficeRecruitment(int32 BuildingIndex)` | Delete office recruitment data on building demolish |
| `FOnGachaPullCompleted OnGachaPullCompleted` | Delegate: (Result) fired after ExecuteGachaPull (animation, reward float-up, transition) |
| `FOnMileageChanged OnMileageChanged` | Delegate: (NewPoints) fired on mileage increment (UI counter, milestone toast) |
| `FOnOfficeSeatChanged OnOfficeSeatChanged` | Delegate: (BuildingIndex, TotalSeats, OccupiedSeats) fired on seat change (roster UI update) |

### UShopManagerSubsystem
Shop purchase/limit/reset backend (DT_ShopItem driven; daily/weekly local-time resets)
파일: `Public/Manager/ShopManagerSubsystem.h`

| API | 용도 |
|---|---|
| `bool PurchaseItem(FName RowName)` | Buy a DT_ShopItem row: limit+currency validation, deduct (Money/Diamond/Mileage), grant via ItemInventoryManager, consolidated save |
| `bool CanAfford(FName RowName) const` | Check currency sufficiency for a shop row (UI affordability tint) |
| `int32 GetRemainingCount(FName RowName) const` | Remaining purchases this cycle (-1 = unlimited, 0 = sold out) |
| `bool ManualResetDaily()` | Diamond 50 instant daily-limit reset |
| `void CheckAndPerformResets()` | Apply midnight/Monday rollover (call on panel open & before purchase) |
| `FTimespan GetTimeUntilDailyReset() const` | Countdown to next local midnight (shop info line) |
| `FOnShopStockChanged OnShopStockChanged` | Delegate: fired on purchase/reset (card state refresh) |

### UWorldMapManager
World map production system: mines, factories, raw materials, refined products, orders, auto-assign
파일: `Public/Manager/WorldMapManager.h`

| API | 용도 |
|---|---|
| `void AddMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave=true)` | Grant raw materials (mining output, purchases, events) with auto-save and broadcast |
| `bool ConsumeMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave=true)` | Remove materials (factory recipe cost); does not check balance |
| `bool TryConsumeRecipe(const FProductRecipeTable& Recipe, int32 Units=1, bool bShouldSave=true)` | Atomically check + consume all recipe materials; false if any insufficient (no partial deduction) |
| `int64 GetMaterialAmount(ERawMaterialType Mat) const` | Query current material balance |
| `void AddEnergy(int64 Amount, bool bShouldSave=true)` | Grant energy (mining/factory passive output) with broadcast |
| `bool ConsumeEnergy(int64 Amount, bool bShouldSave=true)` | Remove energy (factory boost cost) |
| `int64 GetEnergy() const` | Query current energy balance |
| `void AddRefinedOil(int64 Amount, bool bShouldSave=true)` | Grant refined oil (factory special output) |
| `int64 GetRefinedOil() const` | Query current refined oil balance |
| `bool StartMiningBoost(ECountryType Country, ERawMaterialType Mat)` | Activate mine accelerator (cost Energy, speeds output) |
| `bool UpgradeMine(ECountryType Country, ERawMaterialType Mat)` | Increase mine level (cost Money, increases yield, unlocks new materials) |
| `FMiningFacility GetMine(ECountryType Country, ERawMaterialType Mat) const` | Get mine state (level, storage, production rate) |
| `TArray<FMiningFacility> GetMinesInCountry(ECountryType Country) const` | Get all mines for a country (UI list binding) |
| `int64 CollectMine(ECountryType Country, ERawMaterialType Mat)` | Harvest accumulated material from mine (returns collected qty, clears storage) |
| `bool ConstructMine(ECountryType Country, ERawMaterialType Mat)` | Build Lv.1 mine (cost Money, unlock material source for country) |
| `bool AssignLineAssignment(ECountryType Country, int32 LineIndex, ECompanyType CompanyType, int32 ProjectIndex, int32 OrderId, int32 Quantity, EQualityGrade Grade)` | Assign factory production (recipe lookup, ETA calc, queue order) |
| `bool ClearLine(ECountryType Country, int32 LineIndex)` | Cancel factory order (refund in-progress, reset line to idle) |
| `bool StartFactoryBoost(ECountryType Country, int32 LineIndex)` | Activate factory accelerator (cost Energy, 2x speed) |
| `FFactoryLine GetLine(ECountryType Country, int32 LineIndex) const` | Get factory line state (assignment, ETA, yield) |
| `TArray<FFactoryLine> GetFactoryLines(ECountryType Country) const` | Get all factory lines for a country |
| `void SetAutoAssign(ECountryType Country, bool bEnable)` | Enable/disable auto-assignment for country (uses first available line per order) |
| `bool IsAutoAssignEnabled(ECountryType Country) const` | Check if country has auto-assign active |
| `void EnqueueProductionOrder(const FProductionOrder& Order)` | Add order to production queue (ProductionOrderManager calls; WorldMapManager assigns to factories) |
| `TArray<FProductionOrder> GetPendingOrders() const` | Get current queue snapshot (UI pending list, debug) |
| **`int32 UProductionOrderManager::GetMaxProducible(const FProductionOrder&, EResourceType& OutBottleneck, bool& bOutMaterialBound) const`** | **How many units can actually be made** = `min(RemainingQuantity, min over materials of floor(Have / PerUnit))`. Ties broken by the tighter real ratio so a material that unblocks with one more unit is not named as the cap. Returns 0 when not even one is possible; recipe missing = fail-closed 0. **Line capacity is NOT included** — a line is a start gate, not a quantity cap |
| `bool TryAutoAssignForCountry(ECountryType Country)` | Auto-fill factory lines from queue if auto-assign enabled (called each Tick) |
| `void AddProduct(FIntPoint ProductKey, int64 Qty, EQualityGrade Grade, bool bShouldSave=true)` | Add finished product to warehouse (factory completion, purchases) with broadcast |
| `bool ConsumeProduct(FIntPoint ProductKey, int64 Qty, bool bShouldSave=true)` | Remove product (sale, market order consumption); no balance check |
| `int64 GetProductAmount(FIntPoint ProductKey) const` | Query quantity of specific product (CompanyType, ProjectIndex key) |
| `EQualityGrade GetProductGrade(FIntPoint ProductKey) const` | Get grade of final product (affects sale price, completion bonus) |
| `TArray<FIntPoint> GetAllProductKeys() const` | Get all (CompanyType, ProjectIndex) in warehouse (UI inventory list) |
| `int64 GetTotalWarehouseValue() const` | Sum all products by sale price (left panel networth display) |
| `void DebugSeedRandomProducts(int32 NumSlots=24, int64 MinQty=100, int64 MaxQty=8000)` | Populate warehouse with random products for testing (auto-call if empty on modal enter) |
| `ECountryType GetPrimaryCountryForMaterial(ERawMaterialType Mat) const` | Get major source country for material (resource panel ??country jump) |
| `FOnMaterialChanged OnMaterialChanged` | Delegate: (Mat, NewAmount) fired on material add/consume (UI counter, storage warning) |
| `FOnEnergyChanged OnEnergyChanged` | Delegate: (NewEnergy) fired on energy change (UI counter, boost unavailable state) |
| `FOnRefinedOilChanged OnRefinedOilChanged` | Delegate: (NewOil) fired on refined oil change |
| `FOnFactoryUnitCompleted OnFactoryUnitCompleted` | Delegate: (Country, LineIndex, ProductKey) fired on factory completion (anim, product add) |
| `FOnProductAdded OnProductAdded` | Delegate: (ProductKey, NewAmount) fired on product warehouse add (UI inventory update) |

### UCGGameInstance
Game root state holder: level transitions, office context, lootbox mode, recruitment flow, visit mode
파일: `Private/Core/CGGameInstance.h`

| API | 용도 |
|---|---|
| `void TransitionToLevel(const FString& LevelName, int32 BackgroundIndex=0)` | Load level by name (MainMap, OfficeMap, WorldMap, etc.) with optional background parallax index |
| `void SetCurrentManagedBuilding(ABuildingBaseActor* Building)` | Set context for OfficeMap entry (building data accessible during level) |
| `int32 GetCurrentManagedBuildingIndex() const` | Get active building index (OfficeMap context, employee management scope) |
| `void SetCurrentBuildingCompanyType(ECompanyType Type)` | Store company type of current building (UI industry-specific panel binding) |
| `ECompanyType GetCurrentBuildingCompanyType() const` | Query company type (used by TableManager::ResolveProjectData for auto-lookup) |
| `int32 AllocateNextBuildingIndex()` | Get unique BuildingIndex for new building (post-increment, save-restored) |
| `int32 GetNextBuildingIndex() const` | Peek next index without incrementing (save/load) |
| `void SetOfficeMode(EOfficeMode Mode)` | Set OfficeMap behavior (Normal/Training/PromotionTest/FreeView) ??controls UI visibility, input handling |
| `EOfficeMode GetOfficeMode() const` | Query office mode (UI panels enable/disable) |
| `void SetCurrentLootBoxCategory(ELootBoxCategory Category)` | Store current gacha shop category (LootBoxMap UI synchronization) |
| `ELootBoxCategory GetCurrentLootBoxCategory() const` | Get current shop category (used on LootBoxMap entry to restore tab) |
| `void TransitionToLootBoxMap(ELootBoxCategory Category, int32 BackgroundIndex=0)` | Jump to LootBoxMap and set initial gacha category (BuildingOpenWidget uses) |
| `void TransitionToRecruitmentMap(const FGachaResultData& Result, int32 BackgroundIndex=0)` | Jump to RecruitmentMap with gacha result (employee portrait scene) |
| `void ReturnFromRecruitmentMap()` | Return to OfficeMap after recruitment completes (auto-select recruited employee) |
| `const FGachaResultData& GetPendingGachaResult() const` | Access gacha result pending hire confirmation (RecruitmentMap reads) |
| `bool HasPendingGachaResult() const` | Check if gacha result awaiting confirmation (gates hire button availability) |
| `void SetLastRecruitedEmployeeID(int32 ID)` | Mark employee for auto-selection on OfficeMap return (recruited employee highlights) |
| `int32 GetLastRecruitedEmployeeID() const` | Get last recruited ID (OfficeMap auto-selects and scrolls to this employee) |
| `void ClearLastRecruitedEmployeeID()` | Clear selection after consuming (prevents stale selection on re-entry) |
| `void SetVisitMode(bool bInVisitMode)` | Enable/disable visit mode (viewing another player's city from ranking) |
| `bool IsVisitMode() const` | Check if viewing remote city (gates building operations, shows observer UI) |
| `void SetVisitData(const FCitySnapshot& InSnapshot, const FString& InPlayFabId, const FString& InDisplayName)` | Store visited city data + player info (MainMap deserialization, remote entity rendering) |
| `const FCitySnapshot& GetVisitCitySnapshot() const` | Get visited city state (buildings, employees, decorations for read-only rendering) |
| `const FString& GetVisitTargetDisplayName() const` | Get visited player display name (title bar, return-to-ranking button) |
| `void ClearVisitData()` | Clear visit state after return (prevent cross-visit data leakage) |
| `ECurrentMapType GetCurrentMapType() const` | Query active level type (guards input handling, UI visibility) |
| `bool IsInOfficeMap() const` | Fast check for OfficeMap (conditional office-only features) |
| `void SaveGameBeforeLevelTransition()` | Pre-emptive save on level unload (safeguard for level transition data loss) |
| `void LoadGameAfterLevelStart()` | Restore save data after level is fully initialized (called by level script OnBeginPlay) |
| `UGlobalAssetCache* GetGlobalAssetCache() const` | Access asset cache (preloaded meshes, materials, textures for runtime use) |
| `static UCGGameInstance* GetInstance()` | Access global game instance singleton (shortcut instead of GEngine::GetGameInstance) |

## UE5.4 CommonUI Framework (CompanyGrowthRenewal)

### UUIBase
Main UI container managing MainStack, PromptStack, and BottomStack layers for menu/modal/overlay UI management
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\UIBase.h`

| API | 용도 |
|---|---|
| `UCommonActivatableWidget* PushMenuClass(TSubclassOf<UCommonActivatableWidget> widgetClass)` | Push widget onto MainStack (primary UI layer) |
| `UCommonActivatableWidget* PushPromptClass(TSubclassOf<UCommonActivatableWidget> widgetClass)` | Push modal/prompt widget onto PromptStack (blocks UI underneath) |
| `UCommonActivatableWidget* PushBottomClass(TSubclassOf<UCommonActivatableWidget> widgetClass)` | Push overlay widget onto BottomStack (bottom-docked panel like info bar) |
| `void ClearAllLayers()` | Deactivate all widgets in all three stacks immediately |
| `int32 GetPromptStackCount() const` | Return count of active prompts; use to detect modal state |
| `int32 GetBottomStackCount() const` | Return count of active bottom overlays |
| `UCommonActivatableWidget* GetActiveBottomWidget() const` | 현재 BottomStack 최상단 활성 위젯을 읽기 전용으로 반환. 특정 패널 타입 확인 후 프로그래매틱 닫기 요청에 사용. |
| `bool PopBottomWidget()` | Deactivate topmost widget on BottomStack; returns success |
| `int32 GetMainStackCount() const` | Return count of active main widgets |
| `bool PopMainWidget()` | Deactivate topmost widget on MainStack; returns success |

### UAnimatedActivatableWidget
Base class for CommonUI widgets with optional Show/Hide animations; binds animations by name in WBP
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\AnimatedActivatableWidget.h`

| API | 용도 |
|---|---|
| `void CloseWithAnimation()` | Play Hide animation (if exists) before deactivating; use instead of DeactivateWidget() |
| `bool IsCloseRequested() const` | Hide 애니메이션 완료 전에도 동기적인 닫기 요청 여부를 반환. 닫히는 패널을 활성 입력 대상으로 취급하지 않을 때 사용. |

### UGlobalUtilFunctions
Global utility function library for number formatting, coordinate helpers, and random string generation
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Global\GlobalUtilFunctions.h`

| API | 용도 |
|---|---|
| `FText AbbreviateNumber(int64 Value, ENumberAbbrevStyle Style = Auto, ENumberRoundMode RoundMode = Floor)` | Format large integers (1訝?) with unit suffix or comma grouping; style: Korean/Western, round: Floor/Round/Ceil |
| `FText FormatFundsAmount(int64 Value, bool bShowPositiveSign = false, ENumberRoundMode RoundMode = Floor)` | 자금 전용 한국식 축약 + 선택적 양수 부호 |
| `FText AbbreviateNumberFloat(double Value, ENumberAbbrevStyle Style = Auto, ENumberRoundMode RoundMode = Floor)` | Format large floats (revenue/sec, bonus rates) with unit suffix |
| `void SplitAbbreviatedNumber(int64 Value, FString& OutNumber, FString& OutUnit, Style = Auto, RoundMode = Floor)` | Split AbbreviateNumber output into number part + Korean unit ("24.5" / "억"). For money layouts that typeset the unit smaller than the digits (0.55x). OutUnit empty when there is no unit |
| `FText FormatExactNumber(int64 Value)` | Return full number with comma grouping (for tooltip detail view) |
| `FText FormatDurationKorean(double Seconds)` | Korean duration text ("52초" / "11분 40초" / "2시간 5분"), two largest units, ceil. **New standard for time display** — widget-local FormatDuration copies are legacy |
| `FText FormatEstimateDurationKorean(double Seconds)` | Estimate/forecast duration — rounds first (10s under 10min / 1min under 1hr / 10min above) then FormatDurationKorean. Use when the number is a projection, not a countdown |
| `FVector SteppedPosition(const FVector& position)` | Snap world position to 200-unit grid (building grid alignment) |
| `FString GenerateRandomStringWithSeed(int32 Length)` | Generate random alphanumeric string of specified length |

### FWidgetAnimationUtils
Static easing functions and animation state structures for UI motion and interpolation
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Utils\FWidgetAnimationUtils.h`

| API | 용도 |
|---|---|
| `static float EaseOutQuad(float T)` | Deceleration easing curve (slows down toward end) |
| `static float EaseOutBack(float T)` | Overshoot easing curve (target overshoots then bounces back; ideal for scale punch) |
| `static float EaseOutElastic(float T)` | Spring/bouncy easing curve (elastic damped oscillation) |
| `static float StampInScale(float T01)` / `static float StampInOpacity(float T01, float Max=0.94f)` / `static FVector2D ShakeOffset(float Elapsed, float Duration, float Amp)` | 고무도장 강타(스케일 2.8→0.94→1 + 잉크 불투명도) · 감쇠 셰이크 오프셋(px, 구간 밖은 0). 직원 가챠 채용 확정과 출시 판정 도장이 공유 |

### FNumberCountUpAnimation
Animated counter that interpolates a value from StartValue to EndValue over Duration
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Utils\FWidgetAnimationUtils.h`

| API | 용도 |
|---|---|
| `void Start(float From, float To, float InDuration = 0.3f)` | Begin count-up animation with optional custom duration |
| `float Tick(float DeltaTime)` | Update animation each frame; returns current interpolated value |
| `void Finish()` | Immediately jump to target value and end animation |
| `bool IsPlaying() const` | Check if animation is currently active |
| `float GetCurrentValue() const` | Get current interpolated value |

### FScalePunchAnimation
Scale punch animation that expands widget to PunchScale then shrinks back to 1.0 using EaseOutBack
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\Utils\FWidgetAnimationUtils.h`

| API | 용도 |
|---|---|
| `void Start(float InPunchScale = 1.2f, float InDuration = 0.2f)` | Begin scale punch with optional custom peak scale and duration |
| `float Tick(float DeltaTime)` | Update animation each frame; returns current scale value (1.0 ??PunchScale ??1.0) |
| `bool IsPlaying() const` | Check if animation is currently active |
| `float GetCurrentScale() const` | Get current scale value for SetRenderScale() |

### UButtonWidget
Reusable text button with optional outline, customizable text styling, and automatic button sound playback
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Buttons\ButtonWidget.h`

| API | 용도 |
|---|---|
| `void SetButtonText(const FText& NewText)` | Update button label text |
| `void SetTextSize(float NewSize)` | Change font size at runtime |
| `void SetTextColor(FSlateColor NewColor)` | Change text color |
| `void SetTextOutlineEnabled(bool bEnabled, float Size = 2.0f, FLinearColor Color = Black)` | Enable/disable text outline with optional custom thickness and color |
| `void UpdateTextOutline(float Size, FLinearColor Color)` | Modify outline properties (outline must be already enabled) |
| `void SetLetterSpacing(int32 NewSpacing)` | Adjust character spacing (negative: tight, positive: loose) |
| `FOnButtonIsSelectedChanged OnIsSelectedChanged` | Delegate fired when button selection state changes; broadcasts bool bIsSelected |
| `void SetRejectInsufficient(EResourceType Type, int64 Need)` | **`SetIsEnabled(false)` 대신 쓸 것 (재화 부족 한정).** 버튼을 살린 채 클릭만 삼키고 셰이크+부족분 토스트. 부족분은 클릭 시점 계산 |
| `void SetRejectReason(const FText& Reason)` | 재화 외 사유로 거부할 때. 빈 FText 는 무효 — 해제는 `ClearRejectReason()` |
| `void ClearRejectReason()` / `bool HasRejectReason() const` | 거부 해제 / 조회. 재사용 위젯은 매 Configure 마다 양방향 세팅 필수 |

> ⚠ 거부 모드는 **재화 부족처럼 곧 풀릴 조건에만**. MAX 레벨·미해금·미선택 같은 구조적 불가는
> 비활성이 맞다(눌리는데 매번 거절당하는 버튼은 비활성보다 나쁘다).
> **선택형(탭) 버튼 금지** — CommonUI 는 선택 토글을 `NativeOnClicked` 보다 먼저 처리한다.

### UIconButtonWidget
Icon + text button for items, loot, buttons with numeric count overlay
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Buttons\IconButtonWidget.h`

| API | 용도 |
|---|---|
| `void SetIcon(UTexture2D* NewIcon)` | Update button icon texture at runtime |
| `void SetIconFromSoft(TSoftObjectPtr<UTexture2D> SoftIcon)` | Asynchronously load and set icon from soft reference |
| `void SetButtonText(const FText& NewText)` | Update button label text |
| `void SetCount(int32 Count)` | Display item count (internally updates text) |
| `int32 GetCount() const` | Get currently displayed count |
| `void SetSelected(bool bInSelected)` | Toggle button selection state visually |

### UCloseButtonWidget
Reusable close/dismiss button; broadcasts click event for parent panel to handle deactivation
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Buttons\CloseButtonWidget.h`

| API | 용도 |
|---|---|
| `FOnCloseClicked OnCloseClicked` | Delegate fired when close button clicked; bind in parent panel NativeConstruct to trigger deactivation |

### UMaterialIconButtonWidget
Icon button with dynamic material instance allowing icon and parameter changes at runtime
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Buttons\MaterialIconButtonWidget.h`

| API | 용도 |
|---|---|
| `void SetIcon(UTexture2D* NewTexture)` | Update button icon via dynamic material texture parameter |
| `void SetIconFromSoft(TSoftObjectPtr<UTexture2D> SoftIcon)` | Asynchronously load and set icon from soft reference |
| `void SetImageSize(FVector2D NewSize)` | Change icon size at runtime |
| `void SetButtonEnabled(bool bEnabled)` | Enable/disable button interaction |
| `bool IsButtonEnabled() const` | Check if button is currently enabled |
| `UMaterialInstanceDynamic* GetDynamicMaterial() const` | Access dynamic material for advanced parameter tweaking |
| `void SetScalarParameter(FName ParameterName, float Value)` | Set material scalar parameter (opacity, etc.) |
| `void SetVectorParameter(FName ParameterName, FLinearColor Value)` | Set material vector parameter (color tint, etc.) |
| `FOnMaterialIconButtonClicked OnClicked` | Delegate fired on button click |
| `FOnMaterialIconButtonHovered OnHovered` | Delegate fired on mouse hover |
| `FOnMaterialIconButtonUnhovered OnUnhovered` | Delegate fired on mouse leave |

### UAlertMarkWidget
Reusable notification/alert dot (4 textures: Small/Large 횞 Red/Green); shows/hides dynamically
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Common\AlertMarkWidget.h`

| API | 용도 |
|---|---|
| `void SetMark(EAlertMarkSize InSize, EAlertMarkColor InColor)` | Update both size and color simultaneously; auto-selects texture |
| `void SetSize(EAlertMarkSize InSize)` | Change mark size (Small/Large) |
| `void SetColor(EAlertMarkColor InColor)` | Change mark color (Red/Green) |
| `void Show()` | Display mark (set Visibility to Visible) |
| `void Hide()` | Hide mark (set Visibility to Hidden) |
| `bool IsShown() const` | Check if mark is currently visible |

### UStatRowWidget
Generic stat display row (icon, name, value, progress bar, separator); supports animated count-up and scale punch
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Common\StatRowWidget.h`

| API | 용도 |
|---|---|
| `void SetStatInfo(const FString& InStatName, const FString& InStatValue)` | Set both name and value text |
| `void SetStatName(const FString& InStatName)` | Update stat label only |
| `void SetStatValue(const FString& InStatValue)` | Update stat value text only |
| `void SetCurrentProgress(float CurrentValue, float TargetValue)` | Update progress bar (green if ??target, red if < target) |
| `void SetCurrentProgressAnimated(float CurrentValue, float TargetValue)` | Update with count-up animation + scale punch on value change |
| `void SetValueColor(const FSlateColor& InColor)` | Change value text color |
| `void SetNameColor(const FSlateColor& InColor)` | Change label text color |
| `void SetFontSize(int32 Size)` | Change font size for name and value |
| `UProgressBar* GetProgressBarWidget() const` | Access ProgressBar for coordinate queries (ScoreOrb targeting) |
| `void SetStatIcon(UTexture2D* InIcon)` | Set optional icon texture (nullptr collapses icon) |
| `void SetIconSize(FVector2D InSize)` | Resize icon display |
| `void SetIconPadding(FMargin InPadding)` | Adjust icon slot padding |

### UResourceWidget
Generic resource display (icon + abbreviate amount); supports fraction mode, affordability tint, and info tooltip
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Common\ResourceWidget.h`

| API | 용도 |
|---|---|
| `void SetResourceType(EResourceType type, bool bUseTypeColor = true)` | Change resource type and apply auto-coloring |
| `void SetValue(int64 amount)` | Update resource amount; auto-abbreviates per ENumberAbbrevStyle |
| `void SetValueWithMax(int64 current, int64 max)` | Set amount and max (shows as fraction if bShowAsFraction=true) |
| `void SetCanAfford(bool bInCanAfford)` | Tint red if cannot afford (false) or normal color (true) |
| `FVector2D GetCachedIconScreenPos() const` | Get screen position of icon for coin flyout animation origin |
| `bool CalculateIconScreenPos()` | Cache current screen position of icon; returns success |
| `void ForceRecalculateIconPos()` | Force recalculation on next tick (screen rotation, etc.) |

### UItemTooltipWidget
Inline popover tooltip for item/resource detail; auto-dismisses or stays until manual hide
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Common\ItemTooltipWidget.h`

| API | 용도 |
|---|---|
| `void ShowAt(FVector2D AbsoluteScreenPos, const FText& InName, UTexture2D* InIcon, const FText& InDesc, int32 InBasePrice = 0, float DurationSec = 2.5f, ETooltipAnchor AnchorMode = RightOfAnchor)` | Display tooltip at screen position with name, icon, description, price; auto-dismiss after duration (0 = no auto-dismiss) |
| `void Hide()` | Manually hide tooltip (play HideAnim if bound) |

### UGestureHintWidget
제스처 힌트 ― 손 글리프(탭/홀드/드래그) + 홀드 라디얼 링 + 라벨. 위치·수명은 소유자 책임인 순수 표시 위젯 (`EWidgetType::GestureHint`)
파일: `${PROJECT_ROOT}/Source/CompanyGrowthRenewal/Public/UI/Element/Common/GestureHintWidget.h`

| API | 용도 |
|---|---|
| `void SetGesture(EGestureHintKind Kind)` | None/Tap/Hold/Drag ― 손 모션과 링 가시성을 한 번에 정한다. **종류가 바뀔 때만 위상 리셋**(같은 제스처 재표시는 루프 유지) |
| `void SetLabel(const FText& Label)` | 손 아래 라벨 (빈 FText = Collapsed) |
| `EGestureHintKind GetGesture() const` | 현재 제스처 조회 |
| `void UGestureHintFxWidget::SetMotion(EGestureHintKind, float Elapsed)` | 탭 점 펄스·드래그 점선을 그리는 **자식** 위젯 ― 부모가 매 틱 주입. 자식으로 뺀 이유 = 자식 트리가 부모 `NativePaint` 보다 먼저 그려져야 이펙트가 손 **아래**로 깔린다 |
| `GestureHintMotion::TapOffsetY / TapPulse / HoldPercent / HoldScale / DragOffsetX` | 모션 순수 함수 (`Public/UI/Element/Common/GestureHintTypes.h`) ― 위젯 Tick 과 테스트 `CGR.UI.GestureHintMotion` 이 같은 식을 쓴다 |
| `CatchHintRules::ResolveKey / ResolveLabel` | 캐치 링 옆 라이브 힌트의 키(`CatchDoze`/`CatchBolt`)·문구·졸업(3회) 규칙 (`Public/UI/Element/Common/CatchHintRules.h`, 테스트 `CGR.Office.CatchHintRules`) |

### UCardWidgetBase
Base class for clickable card widgets (buildable entities, employees); supports async icon load and optional frame UI
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Cards\CardWidgetBase.h`

| API | 용도 |
|---|---|
| `UImage* GetEntityImage() const` | Get display image from CardFrame or legacy direct binding |

### UEntityCardWidgetBase
Abstract base for entity cards (buildings, decorations, workstations); handles lock state, selection, async icon loading
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Element\Cards\EntityCardWidgetBase.h`

| API | 용도 |
|---|---|
| `virtual void HandleCardClicked() PURE_VIRTUAL` | Override in child class to implement click response (e.g., open detail panel) |
| `void SetSelected(bool bInSelected)` | Toggle visual selection state (used in FloorTile picker) |
| `bool IsSelected() const` | Check if card is currently selected |
| `bool IsLocked() const` | Check if card is locked (cannot be interacted) |
| `UImage* GetEntityImage() const` | Get card display image from frame |

### IButtonSoundInterface
Mixin interface for auto button-sound playback; inherit in button classes for automatic click/hover audio
파일: `${PROJECT_ROOT}\Source\CompanyGrowthRenewal\Public\UI\Interface\ButtonSoundInterface.h`

| API | 용도 |
|---|---|
| `virtual FGameplayTag GetClickSoundTag() const` | Override to specify custom click sound tag (default: UI.Sound.Button.Click) |
| `virtual FGameplayTag GetHoverSoundTag() const` | Override to specify custom hover sound tag (default: UI.Sound.Button.Hover) |
| `virtual bool ShouldPlayClickSound() const` | Override to conditionally suppress click sound |
| `virtual bool ShouldPlayHoverSound() const` | Override to conditionally suppress hover sound |
| `void PlayClickSound(const UObject* WorldContext)` | Manually trigger click sound playback via SoundManager |
| `void PlayHoverSound(const UObject* WorldContext)` | Manually trigger hover sound playback via SoundManager |

## OFFICE + PRODUCTION + WORLDMAP

### UOfficeManager
Central manager for office decoration mode, workstation placement, and revenue storage (WorldSubsystem)
파일: `Public/Office/OfficeManager.h`

| API | 용도 |
|---|---|
| `void EnterDecorationMode()` | Activate office decoration/customization mode |
| `void ExitDecorationMode()` | Deactivate office decoration mode |
| `bool IsInDecorationMode() const` | Check if currently in decoration mode |
| `void OnDecorationItemSelected(const FDecorationCardTable& CardData)` | Handle decoration item selection from UI |
| `void OnFloorTileSelected(FName TileRowName)` | Handle floor tile selection |
| `void RegisterPlacedDecoration(ADecorationActor* Decoration)` | Register placed decoration actor for tracking |
| `void UnregisterPlacedDecoration(ADecorationActor* Decoration)` | Unregister and remove tracked decoration |
| `void RegisterPlacedWorkstation(AWorkstationActorBase* Workstation)` | Register placed workstation for tracking |
| `void UnregisterPlacedWorkstation(AWorkstationActorBase* Workstation)` | Unregister and remove tracked workstation |
| `AWorkstationActorBase* FindWorkstationByEmployeeID(int32 EmployeeID)` | Find workstation assigned to specific employee |
| `const TArray<AWorkstationActorBase*>& GetPlacedWorkstations() const` | Get list of all placed workstations |
| `void RecalcAndSyncTotalSeats()` | 배치된 책상들의 `GetChairCount()` 합계를 `URecruitmentManagerSubsystem::SyncTotalSeats` 로 밀어넣는다. 라이브 카드는 전부 1인석이며 Double 계산 능력은 보존 코드에만 남는다. **고용 상한과 무관**(그건 `GetBuildingEmployeeCapacity`) — 현재 소비처 없음 |
| `bool HasAvailableWorkstationSlots() const` | Check if empty workstation slots exist |
| `void FillOfficeSaveData(FOfficeSaveData& OutOfficeData) const` | Serialize office decoration and workstation state for save |
| `void ApplyOfficeSaveData(const FOfficeSaveData& OfficeData)` | Restore office from saved decoration and workstation data |
| `float GetStoredRevenue() const` | Query accumulated revenue stored in office vault |
| `float GetStoredRevenueCapacity() const` | Query total storage capacity for revenue |
| `float AddToStoredRevenue(float Amount)` | Add revenue to office vault (respects capacity limits) |
| `float CollectStoredRevenue()` | Collect all stored revenue and reset vault |
| `bool IsStoredRevenueFull() const` | Check if revenue vault is at maximum capacity |
| `void SpawnDefaultDecorations()` | Spawn initial default decorations for new office (구 세이브 폴백 전용 — 신규 빌딩은 스타터 프리셋이 담당) |
| `bool ApplyStarterPresetToData(FOfficeSaveData&, ECompanyType)` | 스타터 프리셋 2단계: Pending 해석→데코/바닥타일 데이터 부풀리기 (ApplyOfficeSaveData 이전 호출) |
| `const TArray<ADecorationActor*>& GetPlacedDecorations() const` | Get list of all placed decorations |
| `FOnWorkstationCountChanged OnWorkstationCountChanged` | Delegate fired when workstation count changes (UI badge updates) |

### UOfficeEventRailWidget
오피스 이벤트 레일(상태 토스트 + 이벤트 카드 스택). 파일: `Public/UI/Element/Office/OfficeEventRailWidget.h`

| API | 용도 |
|---|---|
| `void AddTransientCard(UUserWidget* Card)` | 자동 만료 카드(반응 토스트)를 레일에 투입 — 트림 우선순위는 토스트급(결정 대기 `AddEventCard` 보다 먼저 밀려난다). 카드가 만료 시 스스로 `RemoveEventCard` 를 요청해야 한다(`OnRailRemoveRequested` 관용구) |

### FStarterPresetSeeder
스타터 프리셋 1단계 시드 (건설 확정 시점, static)
파일: `Public/Office/StarterPresetSeeder.h`

| API | 용도 |
|---|---|
| `static void SeedPendingForBuilding(ABuildingBaseActor*)` | footprint 칸수→프리셋 선택, FOfficeSaveData에 Pending+타일수 예약 (PlacingEntity에서 호출) |

관련 TableManager API: `GetStarterPresetRow(FName, bool&)` / `GetStarterPresetForBuilding(int32 FootprintCells, int32 BuildingIndex, bool&)` / `GetIndustryMoodPalette(ECompanyType, bool&)`
치트: `ApplyStarterPreset <PresetRow>` (OfficeMap, 타일+데코 즉시 적용) / `ExportStarterPreset` (현재 배치→프리셋 CSV 엔트리 형식, Saved/StarterPresetExport.txt)

### UOfficeStageProgressManager
Manage stage/project progression, simultaneous task scoring (Planning/Development/QA), launch confirmation, and board system (WorldSubsystem)
파일: `Public/Manager/OfficeStageProgressManager.h`

| API | 용도 |
|---|---|
| `void SelectProject(const FProjectData& ProjectData, int32 ProjectNumber, int32 StageNumber = 1)` | Select project for display without starting timer |
| `void StartNewStage(const FProjectData& ProjectData, int32 ProjectNumber, int32 StageNumber = 1)` | Begin new stage with timer and employee mode change |
| `void EndCurrentStage()` | Force terminate current stage |
| `void StartSimultaneousTimer(bool bShowEffect = false)` | Start 15-second simultaneous task timer for 3 categories |
| `void StopTimer()` | Pause/stop current timer |
| `float CalculateEmployeeContribution(const FEmployeeInstance& Employee, int32 StepNumber) const` | Calculate per-second score contribution from employee for step |
| `void AddEmployeeContribution(float PlanningScore, float DevScore, float QAScore)` | Add discrete employee score contribution to 3 categories |
| `void AddBonusScore(float BonusScore)` | Add external bonus points (active skills, events) |
| `const FStageProgressData& GetProgressData() const` | Query current stage progress state |
| `int32 GetCurrentStep() const` | Get current step number (1=Planning, 2=Dev, 3=QA) |
| `float GetRemainingTime() const` | Query remaining time in current timer |
| `float GetTotalDuration() const` | Get total step duration including trait/tier overrides |
| `bool IsTimerRunning() const` | Check if timer is currently active |
| `EQualityGrade GetCurrentQualityGrade() const` | Get current quality grade (D/C/B/A/S) |
| `float GetCurrentQualityScore() const` | Get normalized quality score percentage |
| `bool IsStageInProgress() const` | Check if any stage is currently in progress |
| `void RefreshBoard(EProjectMode Mode, int32 TierOverride = -1)` | Regenerate board with 3 random projects and apply traits |
| `void SelectBoardProject(EProjectMode Mode, int32 SlotIndex)` | Select project from board (commissioned/in-house) |
| `void HandleEventChoice(int32 ChoiceIndex)` | Process mid-project event choice selection |
| `const FProjectTierProgress& GetTierProgress() const` | Query tier progression data |
| `const TArray<FProjectBoardSlot>& GetBoardForMode(EProjectMode Mode) const` | Get current board slots for commissioned/in-house mode |
| `bool IsEventPaused() const` | Check if mid-project event is active/paused |
| `UProjectTraitEventHandler* GetTraitEventHandler() const` | Access trait/event system handler |
| `void RequestLaunchConfirm()` | Confirm launch (shift to Operating/Idle based on mode) |
| `void RequestLaunchCancel()` | Cancel launch and return to Idle state |
| `void NotifyOperationCompleted()` | Mark self-developed project operation as complete |
| `void NotifyReportClosed()` | Close result report and return to Idle |
| `void StartLaunch()` | Begin launch/manufacturing sequence |
| `void HandleLaunchDismissed()` | Dismiss launch results, reset score to Idle |
| `void RequestScoreOrb(FVector WorldPos, int32 StepNumber, float Score, bool bIsCritical)` | Request flying score orb visual from world position |
| `int64 CalculateProjectDevelopmentCost(const FProjectData&, EProjectDirection) const` | `[확정 2026-08-14]` 모든 프로젝트의 실제 착수 비용 0 반환. 방향성·건물 특성·구 DT 비용값과 무관 |
| `FOnTimeUpdated OnTimeUpdated` | Delegate: remaining time updated (float RemainingTime) |
| `FOnSimultaneousScoresUpdated OnSimultaneousScoresUpdated` | Delegate: 3 category scores updated (Plan/Dev/QA scores + targets) |
| `FOnStageResultEffect OnStageResultEffect` | Delegate: trigger result effect animation |
| `FOnSimultaneousTimerEnd OnSimultaneousTimerEnd` | Delegate: timer finished, ready for launch popup |
| `FOnStageComplete OnStageComplete` | Delegate: entire stage completed successfully |
| `FOnProjectSelected OnProjectSelected` | Delegate: project selected (data set) |
| `FOnBoardRefreshed OnBoardRefreshed` | Delegate: board regenerated with new slots |
| `FOnTierUnlocked OnTierUnlocked` | Delegate: new tier unlocked (int32 NewTier) |
| `FOnLifecycleChanged OnLifecycleChanged` | Delegate: project lifecycle state transition (OldState, NewState) |

### UProjectOperationManager
Manage launched project revenue generation, warehouse accumulation, fatigue/stamina bonuses (GameInstanceSubsystem)
파일: `Public/Manager/ProjectOperationManager.h`

| API | 용도 |
|---|---|
| `int32 StartOperation(const FStageProgressData& StageData, int32 BuildingID)` | Begin project operation with quality info, return operation index |
| `void EndOperation(int32 OperationIndex)` | Terminate single operation by index |
| `void EndAllOperationsByBuilding(int32 BuildingID)` | End all operations for specific building |
| `int64 CollectWarehouseRevenue(int32 OperationIndex)` | Claim accumulated revenue from single operation warehouse |
| `int64 CollectAllWarehouseByBuilding(int32 BuildingID)` | Claim all accumulated revenue for building's operations |
| `int64 CollectAllStoredRevenue()` | Global collect all building warehouses (MainMap collect-all button) |
| `int32 FillAllStoredRevenue(float Fraction)` | Fill every building warehouse to Fraction of capacity — inverse of CollectAll, raises all money bubbles (트레일러 "방치 수익" 컷 세팅) |
| `double GetTotalStoredRevenue() const` | 전 빌딩 저장 수익 합계(비파괴). `HasAnyStoredRevenue` 의 정량판 — 수거 버튼 금액 표시 |
| `float GetVaultReferenceRatePerSecond(int32 BuildingID) const` | 티어 첫 프로젝트(`BandStart`) 기본 수익에 MarketingPower·OperationRevenue 특성·직원 수익 배율을 적용한 금고 기준레이트. 운영 유무와 무관 |
| `float CalculateWarehouseCapacity(int32 BuildingID) const` | 현재 금고 레벨의 원 단위 용량. `BandStart` 기준레이트 × 5분→12시간 점근 보관시간 × 금고 특성, 최소 60,000 |
| `float CalculateWarehouseCapacityAtLevel(int32 BuildingID, int32 VaultLevel) const` | 임의 강화 레벨 기준 금고 용량. 관리 패널의 현재→다음 미리보기와 런타임이 공유하는 단일 계산 경로 |
| `float CalculateEmployeeStatBonus(int32 BuildingID) const` | Calculate revenue multiplier from avg employee Stamina + IncomeBonus stats and workplace stability (renamed 2026-07-26 from `CalculateEmployeeStaminaBonus` — IncomeBonus joined the same cached calc, stat redesign §3.5) |
| `void InvalidateStatBonusCache(int32 BuildingID)` | Invalidate the Stamina/IncomeBonus revenue bonus cache for building (renamed from `InvalidateStaminaCache`) — call whenever an assigned employee's effective stat changes (enhance/downgrade, SetStat cheat, hire/fire) |
| `void RefreshExpectedRevenue(int32 BuildingID)` | Recalculate expected revenue per second for building |
| `const TArray<FOperationData>& GetActiveOperations() const` | Get all currently running operations |
| `FOperationData* GetOperationByBuildingID(int32 BuildingID)` | Find operation pointer for building (returns nullptr if none) |
| `FOperationData* GetOperationByIndex(int32 OperationIndex)` | Find operation pointer by index (returns nullptr if invalid) |
| `bool GetOperationByBuildingID_BP(int32 BuildingID, FOperationData& OutData)` | Blueprint-safe query operation by building ID |
| `bool GetOperationByIndex_BP(int32 OperationIndex, FOperationData& OutData)` | Blueprint-safe query operation by index |
| `bool HasActiveOperation(int32 BuildingID) const` | Check if building has any running operation |
| `bool IsPlayerInOffice(int32 BuildingID) const` | Check if player is currently in specific office |
| `int32 GetActiveOperationCount() const` | Get total count of running operations |
| `FOnOperationStarted OnOperationStarted` | Delegate: operation started (int32 BuildingID) |
| `FOnOperationCompleted OnOperationCompleted` | Delegate: operation finished (int32 BuildingID, FOperationData) |
| `FOnOperationUpdated OnOperationUpdated` | Delegate: operation state updated every tick |
| `FOnRevenueCollected OnRevenueCollected` | Delegate: revenue collected (int64 Amount) |
| `FOnExpectedRevenueChanged OnExpectedRevenueChanged` | Delegate: expected revenue recalculated (int32 BuildingID, float NewRate) |

### UWorldFactoryManager
Manage batch production lines per country with upgrades (speed, automation, quality, storage, rush), wall-clock lazy model (GameInstanceSubsystem)
파일: `Public/Manager/WorldFactoryManager.h`

| API | 용도 |
|---|---|
| `int32 StartProduction(ECountryType Country, int32 ProductIndex, const FText& ProductName, const TSoftObjectPtr<UTexture2D>& IconPath, float BaseRatePerSecond, int64 TargetQty)` | Start new production line in country, return LineId |
| `bool InstantFinishLine(ECountryType Country, int32 LineId)` | Instantly complete production line (with discount) |
| `bool ClaimLine(ECountryType Country, int32 LineId)` | Claim completed production and add to inventory |
| `TArray<FWorldFactoryLineState> GetActiveLines(ECountryType Country) const` | Get all active production lines for country |
| `int32 GetActiveLineCount(ECountryType Country) const` | Get count of active production lines in country |
| `TArray<ECountryType> GetActiveCountries() const` | Get all countries with registered factory data |
| `bool GetLineState(ECountryType Country, int32 LineId, FWorldFactoryLineState& OutState) const` | Query production line state with lazy progress calculation |
| `int32 GetUpgradeLevel(ECountryType Country, EWorldFactoryUpgradeType UpgradeType) const` | Query upgrade level for specific type in country |
| `bool UpgradeLevelUp(ECountryType Country, EWorldFactoryUpgradeType UpgradeType)` | Increase upgrade level (checks cost, applies multipliers) |
| `int32 GetMaxLines(ECountryType Country) const` | Query max production lines (from LineExpansion upgrade) |
| `float GetSpeedMultiplier(ECountryType Country) const` | Query production speed multiplier from upgrades |
| `float GetRewardMultiplier(ECountryType Country) const` | Query output quantity multiplier from quality/automation upgrades |
| `int64 GetVaultCapacity(ECountryType Country) const` | Query vault storage capacity from StorageExpansion upgrade |
| `float GetInstantFinishDiscount(ECountryType Country) const` | Query cost discount for instant finish (RushProduction) |
| `int64 GetUpgradeCost(ECountryType Country, EWorldFactoryUpgradeType UpgradeType) const` | Calculate upgrade cost with scaling formula |
| `void SerializeForSave(TMap<ECountryType, FCountryFactoryData>& OutData) const` | Export factory data for save file |
| `void LoadFromSave(const TMap<ECountryType, FCountryFactoryData>& InData)` | Import factory data from save file |
| `void ApplyServerOfflineCatchup(float OfflineSeconds)` | Apply server-validated offline progress (20% efficiency) |
| `FOnWorldFactoryLineStarted OnLineStarted` | Delegate: production line started (Country, LineState) |
| `FOnWorldFactoryLineProgressed OnLineProgressed` | Delegate: production progress updated (Country, LineId, CurrentQty) |
| `FOnWorldFactoryLineCompleted OnLineCompleted` | Delegate: production finished (Country, LineId) |
| `FOnWorldFactoryLineClaimed OnLineClaimed` | Delegate: production claimed (Country, LineId, FinalQty) |
| `FOnWorldFactoryUpgradeChanged OnUpgradeChanged` | Delegate: upgrade level changed (Country, UpgradeType, NewLevel) |

### UWorldMapManager
Global hub for raw materials, energy, refined oil, mining, factory line assignment, production orders, finished goods inventory (GameInstanceSubsystem)
파일: `Public/Manager/WorldMapManager.h`

| API | 용도 |
|---|---|
| `void AddMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave = true)` | Add raw material to global inventory |
| `bool ConsumeMaterial(ERawMaterialType Mat, int64 Amount, bool bShouldSave = true)` | Consume raw material (checks availability) |
| `bool HasMaterials(const FProductRecipeTable& Recipe, int32 Units = 1) const` | Check if recipe materials available without consuming |
| `bool TryConsumeRecipe(const FProductRecipeTable& Recipe, int32 Units = 1, bool bShouldSave = true)` | Consume all recipe materials atomically |
| `int64 GetMaterialAmount(ERawMaterialType Mat) const` | Query raw material inventory amount |
| `void AddEnergy(int64 Amount, bool bShouldSave = true)` | Add energy to global pool |
| `bool ConsumeEnergy(int64 Amount, bool bShouldSave = true)` | Consume energy (checks availability) |
| `int64 GetEnergy() const` | Query current energy amount |
| `void AddRefinedOil(int64 Amount, bool bShouldSave = true)` | Add refined oil to inventory |
| `int64 GetRefinedOil() const` | Query refined oil inventory |
| `bool StartMiningBoost(ECountryType Country, ERawMaterialType Mat)` | Activate boost for mine (speed/reward) |
| `bool UpgradeMine(ECountryType Country, ERawMaterialType Mat)` | Upgrade mine facility (check cost, apply multipliers) |
| `FMiningFacility GetMine(ECountryType Country, ERawMaterialType Mat) const` | Query single mine state |
| `TArray<FMiningFacility> GetMinesInCountry(ECountryType Country) const` | Get all mines in country |
| `int64 CollectMine(ECountryType Country, ERawMaterialType Mat)` | Claim stored resources from mine |
| `bool ConstructMine(ECountryType Country, ERawMaterialType Mat)` | Build mine at Lv.1 (first time setup) |
| `bool AssignLineAssignment(ECountryType Country, int32 LineIndex, ECompanyType CompanyType, int32 ProjectIndex, int32 OrderId, int32 Quantity, EQualityGrade Grade)` | Assign production order to factory line |
| `bool ClearLine(ECountryType Country, int32 LineIndex)` | Unassign and clear production line |
| `bool StartFactoryBoost(ECountryType Country, int32 LineIndex)` | Activate boost for factory line |
| `FFactoryLine GetLine(ECountryType Country, int32 LineIndex) const` | Query single factory line state |
| `TArray<FFactoryLine> GetFactoryLines(ECountryType Country) const` | Get all factory lines in country |
| `void SetAutoAssign(ECountryType Country, bool bEnable)` | Enable/disable automatic order assignment |
| `bool IsAutoAssignEnabled(ECountryType Country) const` | Query auto-assign state |
| `void EnqueueProductionOrder(const FProductionOrder& Order)` | Queue production order for assignment |
| `TArray<FProductionOrder> GetPendingOrders() const` | Get queued orders awaiting assignment |
| `bool TryAutoAssignForCountry(ECountryType Country)` | Attempt automatic order-to-line assignment |
| `void AddProduct(FIntPoint ProductKey, int64 Qty, EQualityGrade Grade, bool bShouldSave = true)` | Add finished goods to global inventory |
| `bool ConsumeProduct(FIntPoint ProductKey, int64 Qty, bool bShouldSave = true)` | Consume finished goods (check availability) |
| `int64 GetProductAmount(FIntPoint ProductKey) const` | Query finished goods quantity |
| `EQualityGrade GetProductGrade(FIntPoint ProductKey) const` | Query quality grade of stored product |
| `TArray<FIntPoint> GetAllProductKeys() const` | Get all finished goods types in inventory |
| `int64 GetTotalWarehouseValue() const` | Calculate total market value of all stored goods |
| `ECountryType GetPrimaryCountryForMaterial(ERawMaterialType Mat) const` | Find country with highest mining output for material |
| `FOnMaterialChanged OnMaterialChanged` | Delegate: raw material inventory updated (Material, Amount) |
| `FOnEnergyChanged OnEnergyChanged` | Delegate: energy inventory updated (Amount) |
| `FOnRefinedOilChanged OnRefinedOilChanged` | Delegate: refined oil updated (Amount) |
| `FOnFactoryUnitCompleted OnFactoryUnitCompleted` | Delegate: factory line finished (Country, LineIndex, ProductKey) |
| `FOnProductAdded OnProductAdded` | Delegate: finished goods added (ProductKey, Amount) |
| `FOnProductConsumed OnProductConsumed` | Delegate: finished goods consumed (ProductKey, Amount) |

### UMineManager
Manage country-specific continuous mining with upgrades (rate, storage, line expansion, offline boost, auto-claim), wall-clock lazy model (GameInstanceSubsystem)
파일: `Public/Manager/MineManager.h`

| API | 용도 |
|---|---|
| `void EnsureCountryLines(ECountryType Country)` | Initialize country's mine data structure |
| `bool CreateMineLine(ECountryType Country, EResourceType Resource, int64 OverrideMaxStorage = 0)` | Activate new resource line in country (checks max lines, duplicates) |
| `TArray<EResourceType> GetAvailableResources(ECountryType Country) const` | Get unactivated resources available for pickup |
| `TArray<ECountryType> GetActiveCountries() const` | Get all countries with mining operations |
| `TArray<FMineLineState> GetActiveLines(ECountryType Country) const` | Get all active mining lines in country |
| `bool GetLineState(ECountryType Country, EResourceType Resource, FMineLineState& OutState) const` | Query line state with lazy progress calculation |
| `int32 GetUpgradeLevel(ECountryType Country, EMineUpgradeType UpgradeType) const` | Query upgrade level in country |
| `bool UpgradeLevelUp(ECountryType Country, EMineUpgradeType UpgradeType)` | Increase upgrade level (checks cost) |
| `int32 GetMaxLines(ECountryType Country) const` | Query max mining lines from LineExpansion |
| `float GetEffectiveRatePerSecond(ECountryType Country, EResourceType Resource) const` | Get mining rate with upgrades applied |
| `int64 GetStorageCapacity(ECountryType Country) const` | Query storage limit from upgrade |
| `int64 GetClaimAmount(ECountryType Country, EResourceType Resource)` | Get claimable amount and reset |
| `bool StartMiningBoost(ECountryType Country, EResourceType Resource)` | Activate speed/reward boost |
| `float GetOfflineCatchupMultiplier(ECountryType Country) const` | Query offline efficiency multiplier |
| `int64 GetUpgradeCost(ECountryType Country, EMineUpgradeType UpgradeType) const` | Calculate upgrade cost with scaling |
| `void SerializeForSave(TMap<ECountryType, FCountryMineData>& OutData) const` | Export mine data for save |
| `void LoadFromSave(const TMap<ECountryType, FCountryMineData>& InData)` | Import mine data from save |
| `void ApplyServerOfflineCatchup(float OfflineSeconds)` | Apply server-validated offline progress (20% efficiency) |
| `FOnMineLineCreated OnLineCreated` | Delegate: line activated (Country, LineState) |
| `FOnMineLineStorageChanged OnLineStorageChanged` | Delegate: line storage updated (Country, Resource, Qty) |
| `FOnMineLineSaturated OnLineSaturated` | Delegate: line hit storage limit (Country, Resource, bSaturated) |
| `FOnMineResourceClaimed OnResourceClaimed` | Delegate: resource claimed (Country, Resource, ClaimedQty) |
| `FOnMineLineRemoved OnLineRemoved` | Delegate: line removed from active (Country, Resource) |
| `FOnMineUpgradeChanged OnUpgradeChanged` | Delegate: upgrade level changed (Country, UpgradeType, NewLevel) |

### UTradeOrderManager
Manage trade orders by tier (Normal/Urgent/VIP) with spawning, expiration, combo tracking (GameInstanceSubsystem)
파일: `Public/Manager/TradeOrderManager.h`

| API | 용도 |
|---|---|
| `TArray<FTradeOrder> GetActiveOrders() const` | Get all current trade orders |
| `TArray<FTradeOrder> GetOrdersByTier(ETradeOrderTier Tier) const` | Filter orders by tier |
| `TArray<FTradeOrder> GetAcceptedOrders() const` | Get only accepted/committed orders |
| `TArray<FTradeOrder> GetUnacceptedOrders() const` | Get only un-accepted orders |
| `int32 GetAcceptedCount() const` | Get count of accepted orders |
| `int32 GetMaxAcceptedSlots() const` | Get max slots for accepted orders |
| `bool AcceptOrder(int32 OrderId)` | Accept trade order (up to 3 max) |
| `bool DismissOrder(int32 OrderId)` | Reject/abandon order (penalties if accepted) |
| `void MarkOrdersAsViewed(const TArray<int32>& OrderIds)` | Mark orders as seen by UI (no save if unchanged) |
| `int32 GetComboCount() const` | Get current consecutive fulfillment combo |
| `float GetComboMultiplier() const` | Get revenue multiplier from combo |
| `FTradeOrder FindMatchingOrder(ECompanyType Company, int32 ProjectIndex, bool& bOutFound) const` | Find best-match order for product (VIP > Urgent > Normal) |
| `bool ConsumeOrderQuantity(int32 OrderId, int64 FulfilledQty)` | Fulfill order quantity (partial or complete) |
| `FTradeOrder GenerateOrderByTier(ETradeOrderTier Tier, ECompanyType ForceCompany = ECompanyType::None)` | Instantly create order (testing/events) |
| `bool RegenerateIfExpired()` | Expire old orders and spawn new ones |
| `void ClearAllOrders()` | Remove all orders (reset/debug) |
| `void SerializeForSave(TArray<FTradeOrder>& OutOrders, int32& OutNextOrderId, int32& OutComboCount) const` | Export trade order state for save |
| `void LoadFromSave(const TArray<FTradeOrder>& InOrders, int32 InNextOrderId, int32 InComboCount)` | Import trade order state from save |
| `FOnOrderGenerated OnOrderGenerated` | Delegate: new order created (FTradeOrder) |
| `FOnOrderExpired OnOrderExpired` | Delegate: order timed out (int32 OrderId) |
| `FOnOrderCompleted OnOrderCompleted` | Delegate: order fully fulfilled (int32 OrderId) |
| `FOnOrderFulfilledPartial OnOrderFulfilledPartial` | Delegate: partial fulfillment (int32 OrderId, RemainingQty) |
| `FOnOrderAccepted OnOrderAccepted` | Delegate: order accepted (int32 OrderId) |
| `FOnOrderDismissed OnOrderDismissed` | Delegate: order rejected/abandoned (int32 OrderId, bWasAccepted) |
| `FOnComboChanged OnComboChanged` | Delegate: combo counter changed (int32 ComboCount, float Multiplier) |

### ACountryActor
World map country location marker and camera focus point
파일: `Public/Entity/Country/CountryActor.h`

| API | 용도 |
|---|---|
| `static ACountryActor* FindCountryActor(const UObject* WorldContextObject, ECountryType InCountry)` | Search world for country actor by type |
| `void RequestFocus(bool bAnimate = true)` | Request camera focus on country (with optional animation) |
| `void OnFocusRequested(bool bAnimate)` | Blueprint event: implement custom focus effects (fade, sound, etc) |
| `ECountryType CountryType` | Assigned country type enum |
| `float FocusDistance` | Zoom distance when focused (0=keep current, >0=zoom in) |

### UCountrySellRowWidget
국가×산업 수요 게이지 행/카드. `SetData()` 한 번으로 `UCountryMarketManager::OnDemandChanged` 자동 구독 — 수요 표시가 필요한 새 UI 는 이 클래스의 WBP 변형으로 만들 것 (구독 배선 재작성 불필요)
파일: `Public/UI/Element/Trade/CountrySellRowWidget.h`

| API | 용도 |
|---|---|
| `void SetData(ECountryType InCountry, ECompanyType InIndustry)` | 행 데이터 지정 + 매니저 구독 + 즉시 UI 갱신 |
| **`float GetSellMultiplier() const`** | **수량·주문과 무관한 판매 배율** = `MoneyBias × DemandMul × IndustryPriceMul × (허브면 PortPriceMul)`. `UTradePort::ComputeSell` 의 항 중 `BulkMul`(수량)·`MatchMul`(주문매칭)만 제외한 것 — 판매 전 "이 나라 이 산업이 지금 얼마나 유리한가"를 보여줄 때 사용 |
| `void SetSelected(bool bInSelected)` | 선택 상태 토글 |
| `void SetEfficiencyDelta(float DeltaPercent)` | 평균 대비 ±% 통보 (판매 모달 전용) |
| `void RefreshFromManager()` | 강제 갱신 (드물게 필요) |
| `static FLinearColor GetGaugeColor(float Ratio)` | 수요 비율 → 게이지 색 (0=붉은빛 포화 / 1=청록 만수요). 외부에서도 일관 색상용으로 재사용 |

변형: `UIE_CountrySellRow`(판매 모달 11국 라디오 행) / `UIE_CountryDemandCard`(국가 상세 정보 탭 카드)

### AWorkstationActorBase
Base class for office workstations (single/double seats) with upgrade levels and customization slots
파일: `Public/Office/WorkstationActorBase.h`

| API | 용도 |
|---|---|
| `void InitializeWorkstation()` | Initialize workstation from DataTable and setup level |
| `void SetHighlight(bool bEnabled)` | Toggle workstation selection highlight |
| `void RestoreFromSaveData(const FWorkstationSaveData& SaveData)` | Load workstation state from save |
| `FWorkstationSaveData GetSaveData() const` | Serialize workstation state for save |
| `bool TryUpgradeSetupLevel()` | Atomically advance one Lv1-Lv6 setup row after mesh validation and exact Diamond withdrawal |
| `EComputerSetupLevel GetCurrentSetupLevel() const` | Query current setup level |
| `bool GetCurrentSetupLevelData(FComputerSetupLevelData& OutData) const` | Query the complete current DataTable row without a code fallback |
| `bool GetNextSetupLevelData(FComputerSetupLevelData& OutData) const` | Query the complete next DataTable row; false at max/missing data |
| `bool IsMaxLevel() const` | Check if at max setup level |
| `bool SetSlotSkin(EWorkstationSlot Slot, FName SkinID)` | Apply cosmetic skin to slot |
| `FName GetSlotSkinID(EWorkstationSlot Slot) const` | Get current skin ID for slot |
| `bool IsSlotActive(EWorkstationSlot Slot) const` | Check if slot is unlocked for this level |
| `void SetSlotMesh(EWorkstationSlot Slot, UStaticMesh* NewMesh)` | Apply mesh to slot |
| `void SetSlotVisibility(EWorkstationSlot Slot, bool bVisible)` | Show/hide slot mesh |
| `float GetWorkSpeedBonusRate() const` | Get cumulative workstation work-speed rate applied to assigned employee Orb timing |
| `EWorkstationType GetWorkstationType() const` | Get workstation type (Single/Double, pure virtual) |
| `int32 GetChairCount() const` | Get number of seats (pure virtual) |

### ADecorationActor
Office decoration placement (walls, floors, furniture) with DataTable-driven gameplay and visuals
파일: `Public/Office/DecorationActor.h`

| API | 용도 |
|---|---|
| `void SetDecorationMesh(UStaticMesh* NewMesh)` | Apply mesh to decoration |
| `void RecalcBoxExtent()` | Update collision box to match mesh bounds |
| `bool CanPlaceOnSurface(EDecorationSurface Surface) const` | Check if decoration can be placed on surface type |
| `bool IsUnlocked(int32 CurrentLevel) const` | Check if unlocked at player's building level |
| `EDecorationSurface GetAllowedSurface() const` | Get allowed surface type (wall/floor/grid) |
| `EDecorationCategory GetCategory() const` | Get decoration category |
| `FName DecorationCardTableRowName` | DataTable row for gameplay data |
| `FText DecorationName` | Display name (cached from DataTable) |
| `FText Description` | Description (cached from DataTable) |
| `int32 Price` | Purchase price (cached from DataTable) |
| `int32 UnlockLevel` | Required building level (cached from DataTable) |

### AOfficeInterior
Office space geometry management with tile-based expansion, NavMesh bounds, floor tiling system
파일: `Public/Office/OfficeInterior.h`

| API | 용도 |
|---|---|
| `FBox GetNavMeshBoundsBox() const` | Get navigation bounds for employee AI |
| `FVector GetRandomFloorLocation() const` | Get random valid spawn location on floor |
| `void InitializeComponents()` | Initialize wall/floor components |
| `void UpdateNavMeshBounds()` | Recalculate NavMesh volume |
| `void GenerateFloorTiles()` | Create floor tiles based on TileCountX/Y |
| `void ClearFloorTiles()` | Remove all generated floor tiles |
| `void SetFloorTileMesh(UStaticMesh* NewMesh)` | Apply mesh to all floor tiles |
| `void LoadFloorTileFromRowName(FName RowName)` | Load and apply floor tile from DataTable |
| `FName GetCurrentFloorTileRowName() const` | Get current floor tile DataTable row |
| `FIntPoint GetTileCount() const` | 현재 유효 footprint `(TileCountX, TileCountY)` 반환 |
| `FIntPoint GetMaxTileCount() const` | 프로젝트 최대 footprint 반환(고층 외관 고정 하부 기준은 `5×6`) |
| `FBox2D GetCurrentFloorBoundsLocal() const` | 현재 footprint의 정확한 로컬 XY 바닥 경계 반환 |
| `FBox2D GetMaxFloorBoundsLocal() const` | 최대 footprint의 정확한 로컬 XY 바닥 경계 반환 |
| `float GetStructuralFloorZLocal() const` | 외관이 넘지 않아야 하는 구조 바닥의 로컬 Z 반환 |
| `FOnOfficeFootprintChanged OnFootprintChanged` | footprint가 유효하게 원자 적용된 뒤 한 번 방송하는 네이티브 멀티캐스트 `(FIntPoint)`; 외관 등 표현 계층의 재생성 신호 |
| `bool ExpandLeft()` | Expand left (TileCountX++) |
| `bool ExpandRight()` | Expand right (TileCountX++) |
| `bool ExpandForward()` | Expand forward (TileCountY++) |
| `bool ExpandBackward()` | Expand backward (TileCountY++) |
| `int32 GetExpansionCost(int32 Direction) const` | Calculate cost for expansion |
| `bool CanExpand(int32 Direction) const` | Check if expansion possible |
| `int32 TileCountX` | Current horizontal tile count |
| `int32 TileCountY` | Current vertical tile count |
| `float TileSize` | Single tile dimension (400x400 UE units) |
| `int32 MaxTileCountX` | Max horizontal expansion limit |
| `int32 MaxTileCountY` | Max vertical expansion limit |

### AOfficeExteriorShell
현재 Office footprint를 따라가는 4층 파사드 apron과 최대 `5×6` 기준 고정 전이층·하부 타워·스카이라인을 관리하는 비게임플레이 표현 Actor.
파일: `Public/Office/OfficeExteriorShell.h`

| API | 용도 |
|---|---|
| `bool SynchronizeToInterior()` | `TargetInterior`의 현재/최대 경계와 구조 Z에서 동일한 결정적 layout을 즉시 재생성. 런타임 검증·수동 동기화용 |
| `FIntPoint GetLastBuiltTileCount() const` | 마지막으로 성공 적용된 현재 footprint 반환 |
| `int32 GetDynamicInstanceCount() const` | 현재 파사드 bay·층 밴드·코너/trim ISM 인스턴스 합계 반환. 계약은 `4×(X+Y)+13`, 최대 `57` |

### FOfficeExteriorLayoutBuilder
UObject/월드 상태 없이 현재/최대 Office 경계에서 외관 transform을 계산하는 순수 레이아웃 빌더.
파일: `Public/Office/OfficeExteriorLayout.h`

| API | 용도 |
|---|---|
| `static FIntPoint GetCanonicalMaxTileCount()` | 고정 하부 구조의 정본 최대 footprint `5×6` 반환 |
| `static bool IsCanonicalMaxTileCount(FIntPoint MaxTileCount)` | 입력 최대 footprint가 프로젝트 계약과 일치하는지 확인 |
| `static bool Build(const FOfficeExteriorLayoutInput&, FOfficeExteriorLayoutResult&)` | 동적 apron과 고정 transfer/tower/skyline transform 계산. 잘못된 입력은 false이며 호출자 표현을 건드리지 않게 결과 적용 전 검증한다 |
