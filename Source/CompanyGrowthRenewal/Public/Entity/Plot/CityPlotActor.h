// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/InteractableBaseActor.h"
#include "Interfaces/BubbleAnchorProvider.h"
#include "CityPlotActor.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UPlotPurchasePanelWidget;
class UWidget;

// World XY run of a plot perimeter; the normal points into usable land.
struct FCityPlotBoundarySegment
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
	FVector2D InwardNormal = FVector2D::ZeroVector;
};

/**
 * 도시 블록(부지) 1개 = 인스턴스 1개. 레벨에 블록마다 배치하고 PlotId로 DT_CityPlot 행과 연결한다.
 * 클릭 인터랙션은 부모 AInteractableBaseActor 의 BoxComponent(channel1 Block, QueryOnly) +
 * "Interactable" 태그 + IInputHandler 를 그대로 재사용한다.
 *   - 미소유: 탭 → 구매 확인 모달 (Task 3)
 *   - 소유:   탭 입력 없음
 * 미소유 부지만 TryPurchase 경로로 연결하고, 소유 부지 탭은 처리하지 않는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ACityPlotActor : public AInteractableBaseActor, public IBubbleAnchorProvider
{
	GENERATED_BODY()

public:
	ACityPlotActor();

	// PlotId 로 DT 조회 → Extent 캐시 + BoxComponent 크기를 Extent 에 맞춤 (DT 좌표 스폰 경로)
	void Init(FName InPlotId);

	// 레벨 배치 경로 — 손으로 그린 BoxComponent 가 Extent/수용량의 권위. DT 는 가격/시작소유만 제공한다.
	// 기하를 DT 가 덮지 않으므로 맵마다 다른 부지 배치가 DT 행 공유 없이 성립한다.
	void InitFromAuthoredBounds();

	// 이 부지에 지을 수 있는 건물 수. 배치형은 박스 크기에서 산출(칸수 곱), DT 스폰형은 DT 값.
	// DT BuildingCapacity 를 직접 읽던 3개 지점(BuildModal/Placement/VacantDressing)의 단일 진입점.
	int32 GetBuildingCapacity() const;

	// 부지 격자 칸 수(가로/세로). 배치형은 박스에서 산출, DT 스폰형은 DT 값.
	FIntPoint GetGridCells() const;

	bool IsAuthoredInLevel() const { return bAuthoredBounds; }

	// 빌드 모드 진입/종료 시 호출 — 소유 부지 외곽에 기존 건설 테두리를 켠다.
	void SetBuildModeOutlineVisible(bool bVisible);

	FName GetPlotId() const { return PlotId; }
	bool IsOwned() const { return bOwned; }
	void SetOwnedState(bool bInOwned);

	// 부지 footprint 반경(X,Y) — 건물 배치 제약 bounds (Task 4)
	FVector2D GetExtent() const { return CachedExtent; }
	FVector GetCenter() const { return GetActorLocation(); }

	// 이 부지 블록 메시 위에 footprint 사각형이 통째로 얹혀 있는지(깎인 모서리/블록 밖이면 false).
	// 사각형 4코너 + 중심에서 아래로 StreetSurface(ECC_GameTraceChannel3) 트레이스(bTraceComplex)해 전부
	// 이 부지 블록(또는 /TheRiverwalkCity/ 메시)에 맞아야 true.
	// 블록 미해결이어도 트레이스는 한다 — 그때는 "도시 지면 메시에 맞았는가"만으로 판정한다.
	// "전부 가능" 폴백은 채널이 5점 전부 무응답일 때(= 콜리전/채널 미설정)로 한정된다.
	bool IsFootprintOnGround(const FVector& FootprintCenter, float HalfX, float HalfY) const;

	// 조경의 월드 XY 사각형이 배치형 부지의 유효 칸 안에만 들어오는지 판정한다.
	bool IsDressingFootprintAllowed(const FVector2D& Center, const FVector2D& HalfExtent) const;

	// Merged exposed edges of the authored logical cells. Construction outline, dressing and unowned marker share them.
	void GetBoundarySegments(TArray<FCityPlotBoundarySegment>& OutSegments) const;

	// Optional decoration-only polygon takes precedence. A malformed nonempty polygon returns false and no edges.
	bool GetDressingBoundarySegments(TArray<FCityPlotBoundarySegment>& OutSegments) const;

	// StreetSurface 복합 트레이스로 실제 지면 Z를 구한다. 지면 권위가 없으면 false.
	bool TryTraceGroundZ(const FVector2D& WorldXY, float& OutGroundZ) const;

	// 건축/조경이 공유하는 지면 쿼리 준비. 로드된 보도의 ISM/HISM 인스턴스 필터까지 활성화한다.
	static void PrepareCityGroundQueries(UWorld* CityWorld);

	// footprint 가 블록 밖(인도/깎인 모서리)으로 삐져나갔을 때, MaxPush 안에서 땅 안쪽으로 최소 이동시킨다.
	// 밀 방향은 "실패한 코너들의 부호 합"의 반대로 역산하고, 그 방향으로 이분 탐색해 접지되는 최근접 위치를 찾는다.
	// (블록 메시가 깎인 모서리를 가진 임의 형상이라 공식으로 경계를 못 구한다 — 트레이스가 유일한 권위.)
	// 반환 = 최종 위치가 접지인지. OutAdjustedXY = 보정된 XY(보정 불필요/불가면 입력 XY 그대로).
	// 이미 접지면 평가 1회로 즉시 반환(드래그 상시 경로의 트레이스 비용을 현행 유지).
	bool ResolveFootprintOntoGround(const FVector& FootprintCenter, float HalfX, float HalfY,
		float MaxPush, FVector2D& OutAdjustedXY) const;

	// IBubbleAnchorProvider — 가격 배지가 추적할 부지 상단 월드 위치(부지 중심 위). 가격 배지 앵커로 재사용한다.
	virtual FVector GetBubbleAnchorPosition() const override;

	// 부지를 선택하고 인수 조건/가격을 확인한다. 조건 충족 시 같은 창에서 구매를 확정한다.
	// 진입점 2개: 미소유 부지 3D 탭(OnEndInteract) + 가격 배지(UPlotPriceBadgeWidget) 클릭 — 둘 다 이 함수 호출.
	void TryPurchase();

	// 부수효과 없이 회사 정리·인접·자금 조건을 판정한다. 가이드 타겟팅과 구매 확정 직전 검증이 공유한다.
	bool CanBePurchasedNow() const;

	// 튜토리얼 부지 설명 — 탭으로 연 하단 인수 카드가 떠 있는지와 그 [닫기]
	bool IsPurchaseDialogOpen() const;
	UWidget* GetPurchaseDialogCloseButtonWidget() const;
	UWidget* GetPurchaseDialogConfirmButtonWidget() const;
	// 튜토리얼 부지 설명 동안 선택 바닥을 켜 둔다 ― 매 틱 불러도 사라지기 전에만 시계를 되감는다
	void HoldSelectionHighlight();

	// IInputHandler — 미소유=TryPurchase(), 소유=no-op.
	virtual void OnEndInteract_Implementation(APlayerController* InstigatingPC) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// PlotSizeCells → BoxComponent 크기를 매번 다시 그린다(저장 권위는 프로퍼티, 박스는 산출물).
	virtual void OnConstruction(const FTransform& Transform) override;

	// PlotSizeCells 를 footprint 반경(cm)으로. 칸 단위라 셀 배수 스냅이 애초에 필요 없다.
	FVector2D GetAuthoredHalfExtent() const;

	// 도시 조작을 유지한 채 하단에 인수 조건과 가격을 보여준다.
	void OpenPurchasePanel();
	void RefreshPurchasePanel();
	void ClosePurchasePanel();

	// [인수] 콜백 — 조건 재검사 → Money 결제 → 소유 전환 → 세이브
	UFUNCTION()
	void HandlePurchaseConfirmed();

	void HandlePurchasePanelClosed();
	void SetPurchaseHighlight(bool bVisible);
	TWeakObjectPtr<UPlotPurchasePanelWidget> PurchasePanel;
	FDelegateHandle PurchasePanelClosedHandle;
	FDelegateHandle PurchaseResourceChangedHandle;
	FDelegateHandle PurchaseCompanyClearedHandle;
	bool bPurchaseHighlighted = false;
	float PurchaseHighlightStartedAt = 0.f;
	bool bTutorialHoldEmphasis = false;

	// AInteractableBaseActor 의 순수 가상함수 — 부지는 EntityManager 건물 목록에 등록하지 않으므로 no-op
	virtual void RegisterWithEntityManager() override {}
	virtual void UnregisterWithEntityManager() override {}

	// 선택 시에만 보이는 지면 색면과 얇은 외곽선. 최초 선택 때 만들고 재사용한다.
	UPROPERTY(Transient)
	UProceduralMeshComponent* UnownedBoundaryMesh = nullptr;
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* UnownedBoundarySelectedMID = nullptr;
	bool bUnownedBoundaryBuilt = false;

	void BuildUnownedBoundaryMesh();

	// 런타임 스폰 리버워크 부지의 실제 블록을 접지 판정 권위로 캐시한다.
	// LevelInstance 스트리밍이 늦으면 제한된 시간 동안 재시도한다.
	void ResolveGroundBlockAuthority();

	// 부지 외곽 테두리(변 4개 얇은 평면). 빌드 모드 중 "여기까지 내 땅"을 알려주는 용도라
	// 전면 오버레이와 달리 화면을 가리지 않는다(전면 오버레이는 2026-08 에 그 이유로 폐기됨).
	UPROPERTY()
	TArray<UStaticMeshComponent*> OutlineEdgeSMCs;

	// 변마다 흐름 축과 단면 원점이 달라 MID 도 조각마다 하나다 — 한 장을 공유하면 전부 같은 방향으로 흐른다.
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> OutlineEdgeMIDs;

	// 소유 부지의 건설 가능한 바닥을 은은하게 밝히는 평면. 울타리와 같이 켜지고 꺼진다.
	// Additive 라 바닥 질감(보도 이음선)이 그대로 비친다 — 덮개로 읽히면 구 오버레이와 같은 실패가 된다.
	// 울타리 조각은 모양에서 유도되므로 개수가 고정이 아니다 — 풀에서 필요한 만큼 꺼내 쓴다.
	// 레일(두께 있는 Cube)과 코너 기둥(Cylinder)은 메시가 달라 풀을 나눈다.
	UPROPERTY()
	TArray<UStaticMeshComponent*> OutlinePostSMCs;

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> OutlinePostMIDs;

	// 소유 칸의 실제 지면을 따라가는 발광 표면. 빌드 모드 진입 때만 다시 만든다.
	UPROPERTY(Transient)
	UProceduralMeshComponent* PlotFloorGlowMesh = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* PlotFloorGlowMID = nullptr;

	// 풀에서 Index 번째 조각을 꺼낸다(없으면 만든다). Mesh/Material 은 최초 생성 때만 쓰인다.
	UStaticMeshComponent* AcquireOutlinePiece(TArray<UStaticMeshComponent*>& Pool,
		TArray<UMaterialInstanceDynamic*>& MIDPool, int32 Index,
		UStaticMesh* Mesh, UMaterialInterface* Mat);

	void EnsureOutlineBuilt();

	// 부지 중심의 실제 바닥 Z. 무응답이면 액터 Z.
	float TraceGroundZ() const;

	void BuildFloorGlowMesh(UMaterialInterface* GlowMat, double Cell, int32 CellsX, int32 CellsY,
		const FVector& Center, double OriginX, double OriginY);

	// footprint 5점(중심 + 4코너)의 개별 접지 결과. IsFootprintOnGround(전부 통과 판정)와
	// ResolveFootprintOntoGround(실패 코너로 밀 방향 역산)가 이 단일 트레이스 구현을 공유한다.
	// 인덱스: 0=중심, 1=(-X,-Y), 2=(+X,-Y), 3=(-X,+Y), 4=(+X,+Y).
	// 반환 = 트레이스 채널이 응답했는지. false 면 채널/콜리전 미설정 → 호출자는 "전부 가능" 폴백.
	bool EvaluateFootprintPoints(const FVector& FootprintCenter, float HalfX, float HalfY,
		bool (&OutPointsOnBlock)[5]) const;

	// 블록 탐색 재시도 횟수(무한 루프 방지)
	int32 GroundBlockRetryCount = 0;

	// 블록 탐색 재시도 타이머(간격 재시도 — 핸들을 들고 있어야 중복 예약이 안 쌓인다)
	FTimerHandle GroundBlockRetryTimer;

	// 지면 쿼리가 WP 스트리밍/콜리전 준비보다 먼저 실행됐을 때의 경계 생성 재시도.
	int32 BoundaryRetryCount = 0;
	FTimerHandle BoundaryRetryTimer;
	FTimerHandle PurchaseHighlightTimer;

	// "접지 판정 권위 없음" 경고를 부지당 1회만 내기 위한 플래그. 판정은 const 경로라 mutable.
	mutable bool bLoggedNoTraceAuthority = false;

	// ResolveGroundBlockAuthority 가 찾은 이 부지의 블록 액터(IsFootprintOnGround 트레이스의 권위 기준). 약참조.
	TWeakObjectPtr<AActor> CachedBlockActor;

	// 블록을 한 번이라도 찾았는지. false 면 접지 판정이 "이 블록인가"를 못 보고 "도시 지면인가"까지만 본다
	// (판정 자체는 계속 동작 — 트레이스를 건너뛰지 않는다). 진단 로그 컨텍스트로도 쓴다.
	bool bBlockResolved = false;

	// DT_CityPlot 의 RowName 과 일치시킬 것 (레벨 배치 시 에디터에서 지정)
	UPROPERTY(EditAnywhere, Category = "Plot")
	FName PlotId = NAME_None;

	// 부지 크기 = 논리 격자 칸 수. 월드 크기는 도시 지면 기본 간격과 Plot 외곽 여유로 계산한다.
	// BoxComponent 가 아니라 이 값이 저장 권위다 — 에디터가 액터를 옮길 때 RerunConstructionScripts 가
	// 네이티브 컴포넌트를 아키타입으로 리셋하므로, 박스에 직접 넣은 크기는 드래그 한 번에 날아간다.
	// 박스는 OnConstruction 이 이 값으로 매번 다시 그리는 산출물이다.
	UPROPERTY(EditAnywhere, Category = "Plot", meta = (DisplayName = "Plot Size (cells)", ClampMin = "1", UIMin = "1"))
	FIntPoint PlotSizeCells = FIntPoint(3, 3);

	// 직사각형에서 **빼낼 칸**(좌하단 0,0 기준 칸 좌표). 비어 있으면 순수 직사각형 = 기존 동작 그대로다.
	// 블록 한 귀퉁이가 파인 부지를 표현하려고 둔다 — 부지를 둘로 쪼개면 가격·배지·수용량·인접 판정이
	// 전부 두 벌이 되므로(PlotId 가 게임 규칙의 키다) 모양만 빼는 쪽이 맞다.
	UPROPERTY(EditAnywhere, Category = "Plot", meta = (DisplayName = "Excluded Cells"))
	TArray<FIntPoint> ExcludedCells;

	// Axis-aligned simple polygon in plot-local XY, ordered around the actual usable sidewalk.
	// Empty uses logical plot cells; this does not change construction or ownership geometry.
	UPROPERTY(EditAnywhere, Category = "Plot|Dressing")
	TArray<FVector2D> DressingBoundaryLocalVertices;

	bool IsDressingBoundaryValid() const;
	mutable bool bLoggedInvalidDressingBoundary = false;

	// 저작 칸수를 1 이상으로 보정한 값. 기하(박스/반경/울타리)는 이미 전부 이걸 쓰므로
	// 칸 판정도 같은 값을 봐야 한다 — 한쪽만 0 을 그대로 쓰면 보이는데 못 짓는 부지가 된다.
	FIntPoint GetAuthoredCells() const;

	// 그 칸이 실제 부지에 속하는가. 범위 밖이거나 제외 목록에 있으면 false.
	bool IsCellOccupied(int32 CellX, int32 CellY) const;

	// 저작 모양의 실제 칸 수(= 수용량 상한). 직사각형 칸수에서 제외 칸을 뺀 값.
	int32 GetOccupiedCellCount() const;

	// 소유 상태 (런타임). 세이브 복원/구매 확정은 Task 3 에서 SetOwnedState 호출.
	bool bOwned = false;

	// Init 에서 DT 로부터 캐시한 Extent
	FVector2D CachedExtent = FVector2D::ZeroVector;

	// true = 레벨 배치형(박스가 기하 권위). DT 스폰형과 달리 Init 이 박스를 덮지 않는다.
	bool bAuthoredBounds = false;

	// 지금 테두리가 조립돼 보이는 상태인가. SetBuildModeOutlineVisible 이 드래그 중 매 프레임
	// 불리므로(UpdatePlotFootprintPreview 경로) 상태가 안 바뀌면 재조립을 건너뛴다.
	bool bOutlineShown = false;

	// true = DT 좌표 스폰 경로로 초기화됨. BeginPlay 가 Init 뒤에 돌아도 배치형으로 오인하지 않게 하는 가드
	// (지금은 SpawnActor 안에서 BeginPlay 가 먼저 돌아 PlotId 가 비어 있지만, 그 순서에 의존하지 않는다).
	bool bInitializedFromTable = false;
};
