// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Components/ActorComponent.h"
#include "Table/InteractableInfo.h"
#include "Table/DecorationCardTable.h"
#include "Office/DecorationTypes.h"
#include "Enum/CompanyType.h"
#include "PlacementHandler.generated.h"

class UWidgetComponent;
class UGlobalAssetCache;
class AInteractableBaseActor;
class APlayerCamera;
class AStaticMeshActor;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;
class UInputComponent;
class UNavigationSystemV1;
class AWorkstationActorBase;
class ADecorationActor;
class AOfficeInterior;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;


struct FInputActionValue;

UENUM()
enum class EPlacementMode : uint8
{
	None,
	Building,
	Workstation,
	Decoration,      // Grid 배치 (바닥)
	WallDecoration   // Wall 배치 (벽)
};

// 접촉 자석의 후보 대상 — 월드 축정렬 점유 사각형.
// MainMap 은 DT 격자 칸수로, 오피스는 액터 실측 bounds 로 채운다(둘 다 월드 축정렬이라 규약이 같다).
struct FNeighborFootprint
{
	FVector2D Center = FVector2D::ZeroVector;
	float HalfX = 0.f;
	float HalfY = 0.f;
};

// 축별 접촉 보정량. 스냅 결과가 무효일 때 축을 하나씩 빼고 재평가하는 폴백이 있어,
// 보정량과 "그 축에 후보가 있었는지"를 분리해 돌려준다.
struct FContactSnapResult
{
	float Delta[2] = { 0.f, 0.f };
	bool bHasDelta[2] = { false, false };
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class COMPANYGROWTHRENEWAL_API UPlacementHandler : public UActorComponent
{
GENERATED_BODY()

private:
	// 연속 배치 시 미리보기 액터 이동 여유 공간
	static constexpr float PlacementPadding = 5.f;

	UPROPERTY()
	TObjectPtr<APlayerCamera> Owner = nullptr; // Added UPROPERTY and changed type

	UPROPERTY()
	FInteractableInfo PlacementTargetInfo;

	bool bHasPlacementTarget = false;

	// 배치 확정 시 새 건물에 베이크할 산업 (빌드 모달이 SetPendingCompanyType 으로 주입). 배치 종료 시 None 리셋
	ECompanyType PendingCompanyType = ECompanyType::None;

	// 현재 프리뷰가 올라가 있는 "호버 부지" PlotId — 매 프레임 동적 판정으로 갱신(ComputePlotPlacement 경유).
	// 소유 부지 위가 아니면 None. 배치 확정 시 이 값으로 건물 ↔ 부지 링크를 건다. 배치 종료 시 None 리셋.
	FName PendingPlotId = NAME_None;

	// 부지 건설 세션 여부 — MainMap 건물 배치는 항상 부지 제약(전역 [건설] 버튼·부지 탭 모두).
	// true 면 프리뷰 밑 소유 부지에만 배치 가능(부지 밖 = 빨강). BeginBuild(SetPendingPlotId)에서 켜고 종료 시 false.
	bool bPlotBuildSession = false;

	// 부지 탭 진입 시 초기 프리뷰를 그 부지 위에서 시작하기 위한 시드 PlotId(선택적 nicety). 첫 스폰 후 의미 없음.
	FName InitialSeedPlotId = NAME_None;

	// 배치 중인 건물의 footprint 칸수(W×D). 부지 footprint 월드 반경/점유 계산에 사용 (BeginBuild 에서 캐시). 기본 1×1.
	int32 PendingFootprintWidthCells = 1;
	int32 PendingFootprintDepthCells = 1;

	// 자유 이동(양자화 X) + 부지밖 금지(clamp) + 겹침방지. 부지 건설 세션(bPlotBuildSession)일 때만 동작.
	// 입력 위치(XY) 위의 소유 부지를 GetOwnedPlotAt 으로 동적 판정해, footprint 사각형이 부지 안에 있도록 위치만 clamp 한다.
	// (격자 양자화 안 함 — 건물은 연속 자유 이동. footprint 사각형은 연속 AABB 로 겹침/바닥을 판정한다.)
	//   - 부지 위 + 빈 자리: OutSnappedPos = clamp된 자유 위치, bOutValidCell = true, OutResolvedPlotId = 그 부지
	//   - 부지 위 + 겹침/바닥밖: clamp 하되 bOutValidCell = false
	//   - 부지 밖: clamp 안 함(입력 유지), bOutValidCell = false, OutResolvedPlotId = None (빨강)
	// 반환값: 부지 건설 세션이면 true(부지 밖이어도 true — 자유 배치로 폴백하지 않음), 아니면 false.
	// (Z/스케일은 절대 건드리지 않는다 — 호출자가 X/Y 만 사용.)
	bool ComputePlotPlacement(const FVector& InWorldPos, FVector& OutSnappedPos, bool& bOutValidCell, FName& OutResolvedPlotId) const;

	// 신규 프리뷰 생성 직후 1회만 실행하는 bounded 탐색. 화면 중앙에 가까운 소유 부지부터 검사하고,
	// 각 부지 안의 제한된 footprint-aware 후보를 ComputePlotPlacement 권위로 검증한다.
	bool TryFindInitialPlotPlacement(const FVector& DesiredWorldPos, FVector& OutPlacementPos, FName& OutPlotId) const;

public:
	// 접촉 자석 임계 거리(cm). 기존 건물 변/부지 경계/지면 경계에서 이 거리 안이면 딱 붙는다.
	// 틈이면 끌어당기고 겹침이면 밀어내는 양쪽에 같은 값이 쓰인다. 이 거리를 넘어서면 자석이 풀려
	// 자유 이동 + 기존 빨강 판정 그대로. 0 = 자석 완전 비활성(회귀 시 즉시 끄는 탈출구).
	// 기본 430 = FootprintCellSize(4300) 의 10%.
	UPROPERTY(EditAnywhere, Category = "Placement|Snap", meta = (ClampMin = "0.0"))
	float PlacementSnapDistance = 430.f;

	// footprint 가 블록 밖(인도/깎인 모서리)으로 삐져나갔을 때 땅 안쪽으로 되밀 수 있는 최대 거리(cm).
	// 자석 거리와 분리해 둔다 — 되밀기가 넘어야 하는 건 "부지 AABB ↔ 실제 블록 경계"의 간격(= 인도 폭)이고,
	// 이건 자석 임계거리와 무관한 축이다. 자석값(430)을 빌려 쓰면 인도가 그보다 넓은 변에서 구제가 실패해
	// 부지 끝줄이 통째로 빨강이 된다. 코너 하나만 실패하면 밀기 방향이 대각선이라 축당 실효 거리는 1/√2 배.
	// 기본 2150 = FootprintCellSize(4300) 의 절반. 0 = 되밀기 비활성.
	UPROPERTY(EditAnywhere, Category = "Placement|Snap", meta = (ClampMin = "0.0"))
	float PlotGroundPushMax = 2150.f;

	// 오피스 접촉 자석의 임계 거리 = 배치물 짧은 축 폭 × 이 비율.
	// 1인/2인/4인 책상 폭이 제각각이라 고정 cm 는 그중 하나에만 맞는다(MainMap 은 셀 크기가 일정해 고정값을 쓴다).
	// 0 = 오피스 자석 비활성.
	UPROPERTY(EditAnywhere, Category = "Placement|Snap", meta = (ClampMin = "0.0"))
	float OfficeSnapDistanceRatio = 0.1f;

	// 이웃 사각형 변에 접촉시키는 축별 XY 보정량. MainMap(Gap=0)/오피스(Gap=PlacementPadding) 공용.
	// bAlreadyOverlapping 은 호출자가 자기 겹침 규약(MainMap 은 ContactEpsilon 여유)으로 판정해 넘긴다 —
	// 여기서 재판정하면 두 곳이 다른 사각형을 보게 되어 "빨간데 붙거나 검은데 안 붙는" 불일치가 난다.
	static FContactSnapResult ComputeContactSnap(
		const FVector2D& RawXY, float HalfX, float HalfY,
		TArrayView<const FNeighborFootprint> Neighbors,
		float SnapDistance, float Gap, bool bAlreadyOverlapping);

private:
	// 오피스 바닥 배치(책상/장식) 좌표 = 5유닛 격자 + 접촉 자석.
	// ⚠ TrackMovePlacement 무인자/월드좌표 두 경로가 **반드시** 이걸 함께 써야 한다.
	// 한쪽에만 자석을 넣으면 다음 틱에 다른 쪽이 커서에서 다시 계산해 덮어써서 자석이 통째로 사라진다.
	FVector ComputeOfficeFloorPosition(const FVector& InWorldPos) const;

	// 오피스 바닥 배치(책상/장식)를 이웃 가구·벽에 접촉시킨 XY. 자석 꺼짐/대상 없음이면 입력 그대로.
	FVector2D ApplyOfficeContactSnap(const FVector2D& InXY) const;

	// 벽 스냅용 — 오피스 배치 시작 시 1회 캐시. 배치 종료 시 null.
	UPROPERTY()
	TObjectPtr<AOfficeInterior> CachedOfficeInterior = nullptr;


	UPROPERTY()
	TObjectPtr<AInteractableBaseActor> PlacementTargetEntity = nullptr; // Added UPROPERTY

	UPROPERTY()
	TObjectPtr<UGlobalAssetCache> GlobalAssetCache = nullptr; // Added UPROPERTY

	UPROPERTY(EditAnywhere)
	TObjectPtr<UStaticMeshComponent> PlacementSMC = nullptr;

	// 배치 중 건물이 차지할 footprint 사각형 1개(초록=가능/빨강=불가). 칸 격자 대신 이거 하나만.
	// PlacementSMC(건물 데칼)와 별개 — 건물 메시/스케일은 절대 안 건드림. 첫 사용 시 lazy 생성.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlotFootprintSMC = nullptr;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PlotFootprintMID = nullptr;

	// 배치 중 "이미 차지된 공간" 표시 — 호버 부지의 기존 건물들 footprint 사각형(검은). 건물 1채당 1개(셀 아님).
	// 풀로 재사용: 필요 수만큼 생성/표시하고 나머진 숨김. PlotFootprintSMC(내 프리뷰)와 별개.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PlotOccupiedSMCs;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PlotOccupiedMIDs;

	// 점유 마커 풀 상한(부지당 건물 수 = BuildingCapacity 수준, 여유). 초과분은 표시 생략.
	static constexpr int32 MaxPlotOccupiedMarkers = 64;

	// 점유 마커 풀에서 Index 슬롯을 가져오거나(없으면 생성). 머티리얼/월드 미비면 nullptr.
	UStaticMeshComponent* GetOrCreatePlotOccupiedSMC(int32 Index);

	// footprint 프리뷰 DMI 생성용 베이스 머티리얼(M_PlotHighlight, "Color" 벡터 파라미터). BeginPlay 에서 1회 소프트 로드.
	// 로드 실패면 null → footprint 프리뷰 생성 자체를 막아 graceful 숨김.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlotHighlightBaseMaterial = nullptr;

	// MainMap 부지 건설 세션의 footprint 사각형 1개를 갱신·표시. 유효한 부지가 없으면 현재 위치에 빨강으로 표시.
	// (스냅 위치/유효성은 ComputePlotPlacement 산출값(ActorLocation 스냅 + bPlotPlacementValid) 소비만, 재계산 안 함.)
	void UpdatePlotFootprintPreview();

	// footprint 사각형 숨김(배치 종료/비부지모드).
	void HidePlotFootprintPreview();

	// 건물 위 배치 마커의 표시/원복은 모든 신규·이동 종료 경로가 공유한다.
	void BeginPlacementFeedback();
	void EndPlacementFeedback();

	// 빈 부지 소품은 기존 footprint 권위(PendingPlotId + 회전 반영 칸수)를 그대로 소비한다.
	void UpdateVacantPlotDressingPreview();
	void ClearVacantPlotDressingPreview();
	void RefreshVacantPlotDressingPlots(FName FirstPlotId, FName SecondPlotId) const;

	// DT BuildingCapacity 판정의 단일 런타임 진입점. 원래 부지로 이동 중이면 자기 자신을 count에서 제외한다.
	bool HasCapacityForCurrentPlacement(FName PlotId) const;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> PlacementVisualizationActor = nullptr;

	UPROPERTY()
	TObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem = nullptr; // Added UPROPERTY

	// Input
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> IMC_BuildMode;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_BuildMove;

	bool HasMappingContext = false;

	FDelegateHandle PlacementTargetRecalcBoxExtentDelegateHandle;

	// DragMove
	FVector StoredMove;
	FVector TargetHandle;
	void UpdateBuildAsset();

	bool IsPlacementGrounded();           // [Perf] 위치/모드 변화 없으면 캐시 반환하는 얇은 래퍼
	bool ComputeIsPlacementGrounded();    // 실제 9 LineTrace 본체(래퍼에서만 호출)
	// 그라운드 체크 결과 캐시 — 같은 snapped 위치+모드+벽면이면 트레이스 스킵(드래그 시작 시 무효화).
	bool bGroundCacheValid = false;
	bool bLastGroundResult = false;
	FVector LastGroundKey = FVector::ZeroVector;
	EPlacementMode LastGroundMode = EPlacementMode::None;
	EWallSide LastGroundWallSide = EWallSide::Left;
	bool CanDrop = false;

	// 부지 배치 시 마지막 footprint 위치의 유효성(부지 안 + 미겹침 + 바닥 위). 부지 모드에서 CanDrop 의 추가 게이트.
	// 부지 모드가 아니면 항상 true (기존 자유 배치 영향 없음).
	bool bPlotPlacementValid = true;

	// Edge Panning (화면 가장자리에서 카메라 자동 이동)
	void UpdateEdgePanning();
	float EdgePanThreshold = 0.2f;  // 화면 가장자리 20%
	float EdgePanSpeedMain = 15000.f;  // MainMap 건물 배치 속도
	float EdgePanSpeedOffice = 750.f;  // OfficeMap 업무공간 배치 속도
	bool bIsDraggingPlacement = false;  // 배치 드래그 중인지

public:
	void StartDragging() { bIsDraggingPlacement = true; bGroundCacheValid = false; } // 새 드래그 = 그라운드 캐시 무효
	void StopDragging() { bIsDraggingPlacement = false; }
	bool IsDraggingPlacement() const { return bIsDraggingPlacement; }

private:

	// 겹침 체크 대상 클래스 (MainMap: Building, OfficeMap: Workstation/Decoration)
	TSubclassOf<AActor> OverlapCheckClass = nullptr;

	// 현재 배치 모드
	EPlacementMode CurrentPlacementMode = EPlacementMode::None;

	// 업무공간 배치용
	UPROPERTY()
	TObjectPtr<AWorkstationActorBase> PlacementWorkstation = nullptr;

	// 배치 중인 업무공간 TypeID (저장용)
	FName PlacementWorkstationTypeID;

	// 장식품 배치용
	UPROPERTY()
	TObjectPtr<ADecorationActor> PlacementDecoration = nullptr;

	// 현재 배치 중인 장식품 정보
	FDecorationCardTable PlacementDecorationInfo;

	// 벽 장식품 배치용
	EWallSide CurrentWallSide = EWallSide::Left;
	FPlane CurrentWallPlane;  // 현재 벽의 평면 (터치 좌표 투영용)

	// 벽 장식품 사용자 회전 오프셋 (0, 90, 180, 270 중 하나)
	// 벽 전환 시에도 이 값을 유지하여 사용자가 Rotate한 상태 보존
	float WallDecorationRotationOffset = 0.f;

private:
	// NavMesh ������Ʈ ����
	TSet<AActor*> PendingNavMeshUpdates;
	FTimerHandle NavMeshUpdateTimer;

private:
	FVector OriginalBuildingPosition; // Cancel �� ������ ��ġ
	FName OriginalBuildingPlotId = NAME_None;

public:	
	// Sets default values for this component's properties
	UPlacementHandler();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION() // Added UFUNCTION
	void OnBuildMovePressed(const FInputActionValue& Value);

	UFUNCTION()
	void OnBuildMoveReleased(const FInputActionValue& Value);

public:	
	void Initialize();
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);
	bool CheckEntityAgainstPlacementTarget(const FInteractableInfo& info);

	void SetPlacementTargetEntity(const FInteractableInfo& interactableInfo, TSubclassOf<AActor> InOverlapCheckClass = nullptr);

	// 배치 확정 시 건물에 베이크할 산업 설정 (BeginBuild 경유)
	void SetPendingCompanyType(ECompanyType InType) { PendingCompanyType = InType; }

	// 부지 건설 세션 시작 + 건물 footprint 칸수 설정 (BeginBuild 경유, SetPendingCompanyType 미러).
	// InSeedPlotId 는 "탭한 부지"(초기 프리뷰 시작 위치 시드)일 뿐, 실제 배치 대상은 매 프레임 호버 부지로 동적 판정된다.
	// 전역 [건설] 버튼은 InSeedPlotId = None 으로 호출(초기 프리뷰는 뷰포트 중심 밑 소유 부지로 수렴).
	void SetPendingPlotId(FName InSeedPlotId, int32 InFootprintWidthCells = 1, int32 InFootprintDepthCells = 1)
	{
		bPlotBuildSession = true;
		InitialSeedPlotId = InSeedPlotId;
		PendingPlotId = NAME_None; // 호버 판정 전까지 미확정
		PendingFootprintWidthCells = FMath::Max(1, InFootprintWidthCells);
		PendingFootprintDepthCells = FMath::Max(1, InFootprintDepthCells);
	}
	FName GetPendingPlotId() const { return PendingPlotId; }
	void PostPlacementTargetRecalcBoxExtent();
	void ReleasePlacementTargetEntity();
	bool PlacingEntity();

	void InitPlacementIMC();
	void ReleasePlacementIMC();

	FVector2D GetPlacementTargetBottomScreenPosition() const;
	void TrackMovePlacement();
	void TrackMovePlacement(const FVector& WorldPosition);  // 바닥 배치용 (월드 위치)
	void TrackMovePlacement(const FVector2D& ScreenPosition);  // 벽 배치용 (스크린 위치)
	void UpdateTrackMovePlacement();
	void RotatePlacementEntity();

	AInteractableBaseActor* GetPlacementTargetEntity() const { return PlacementTargetEntity; }

	// 업무공간 배치
	void SetWorkstationPlacementTarget(TSubclassOf<AWorkstationActorBase> WorkstationClass, FName WorkstationTypeID);
	void ReleaseWorkstationPlacementTarget();
	AWorkstationActorBase* GetPlacementWorkstation() const { return PlacementWorkstation; }
	bool PlacingWorkstation(bool* bOutCapacityReached = nullptr);
	bool IsWorkstationMode() const { return CurrentPlacementMode == EPlacementMode::Workstation; }

	// 장식품 배치 (Grid - 바닥)
	void SetDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo);
	void ReleaseDecorationPlacementTarget();
	ADecorationActor* GetPlacementDecoration() const { return PlacementDecoration; }
	bool PlacingDecoration();
	bool IsDecorationMode() const { return CurrentPlacementMode == EPlacementMode::Decoration; }
	const FDecorationCardTable& GetPlacementDecorationInfo() const { return PlacementDecorationInfo; }

	// 벽 장식품 배치 (Wall - 벽)
	void SetWallDecorationPlacementTarget(const FDecorationCardTable& DecorationInfo, EWallSide WallSide);
	void ReleaseWallDecorationPlacementTarget();
	bool PlacingWallDecoration();
	bool IsWallDecorationMode() const { return CurrentPlacementMode == EPlacementMode::WallDecoration; }
	EWallSide GetCurrentWallSide() const { return CurrentWallSide; }

	// 벽 전환 (드래그로 다른 벽으로 이동 시)
	// @param HitLocation 클릭/터치한 위치 (기본값: ZeroVector면 벽 중앙 사용)
	void SwitchToWall(EWallSide NewWallSide, const FVector& HitLocation = FVector::ZeroVector);

	// 배치 모드 관련
	EPlacementMode GetCurrentPlacementMode() const { return CurrentPlacementMode; }

	// 현재 위치에 놓을 수 있는지
	UFUNCTION(BlueprintPure, Category = "Placement")
	bool CanDropHere() const { return CanDrop; }

	// 배치 대상 Actor (모드별 반환) — 미션 제스처 힌트가 배치 프리뷰를 앵커로 쓴다
	AActor* GetPlacementTargetActor() const;

	// 배치 중인 대상이 있는지 (미션 조건 판정 등)
	UFUNCTION(BlueprintPure, Category = "Placement")
	bool HasActivePlacement() const { return GetPlacementTargetActor() != nullptr; }

	// 배치(건설) 시작 시 그 건물에 카메라 포커스+줌인 (기존 FocusOnBuildingForPlacement 활용)
	UFUNCTION(BlueprintCallable, Category = "Placement|Zoom")
	void BeginPlacementZoom();

	// 배치 줌인 강도 (1.0=기존 포커스와 동일, 작을수록 덜 당김 — 배치 중 주변 시야 확보)
	UPROPERTY(EditAnywhere, Category = "Placement|Zoom", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PlacementZoomRatioScale = 0.5f;

public:
	void SetOriginalPosition(const FVector& Position);
	void SetExistingBuildingAsTarget(AInteractableBaseActor* ExistingBuilding);
	bool ConfirmBuildingMove(); // ���� �ǹ� �̵� Ȯ��
	void CancelBuildingMove(); // Cancel �� ���� ��ġ�� ����
	void CompleteBuildingPlacement(AInteractableBaseActor* Building);
};
