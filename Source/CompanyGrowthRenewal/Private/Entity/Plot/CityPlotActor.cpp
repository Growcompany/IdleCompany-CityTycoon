// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Plot/CityPlotActor.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h" // LineTraceMulti / ETraceTypeQuery / EDrawDebugTrace (footprint 바닥 트레이스)
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SpawnManager.h"
#include "Table/CityPlotData.h"
#include "Table/ConstructionCost.h"
#include "Table/BuildingData.h"
#include "Player/Components/PlotPlacementRules.h" // 배치형 부지 박스 → 격자/수용량 산정(단위 테스트 대상)
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "Enum/NotificationType.h"
#include "UI/UIBase.h"
#include "UI/Panel/PlotPurchasePanelWidget.h"
#include "UI/Panel/BuildOpenWidget.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/MissionManagerSubsystem.h"

namespace
{
	// 건설/인수 표시가 같은 접지 규칙을 쓴다. 단차와 구멍은 작은 패치로 나눠 건너뛰며,
	// 평평한 구간은 네 꼭짓점만 사용한다. 표시를 만들 때만 트레이스한다.
	struct FCityPlotGroundSurface
	{
		struct FSample { float Z = 0.f; bool bHit = false; };
		TMap<FVector2D, FSample> Samples;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		int32 MissingPatches = 0;

		void AppendPatch(const FVector2D& Min, const FVector2D& Max, const FVector& Origin,
			double MinSize, float Rim, TFunctionRef<bool(const FVector2D&, float&)> Trace)
		{
			const FVector2D Mid = (Min + Max) * 0.5;
			const FVector2D Points[9] = {
				Min, {Max.X, Min.Y}, Max, {Min.X, Max.Y},
				{Mid.X, Min.Y}, {Max.X, Mid.Y}, {Mid.X, Max.Y}, {Min.X, Mid.Y}, Mid
			};
			FSample PatchSamples[9];
			bool bAllHit = true;
			bool bAnyHit = false;
			for (int32 Index = 0; Index < 9; ++Index)
			{
				const FSample* Cached = Samples.Find(Points[Index]);
				if (Cached)
				{
					PatchSamples[Index] = *Cached;
				}
				else
				{
					PatchSamples[Index].bHit = Trace(Points[Index], PatchSamples[Index].Z);
					Samples.Add(Points[Index], PatchSamples[Index]);
				}
				bAllHit &= PatchSamples[Index].bHit;
				bAnyHit |= PatchSamples[Index].bHit;
			}
			if (!bAnyHit) { ++MissingPatches; return; }

			bool bPlanar = bAllHit;
			if (bAllHit)
			{
				const double U[9] = {0., 1., 1., 0., 0.5, 1., 0.5, 0., 0.5};
				const double V[9] = {0., 0., 1., 1., 0., 0.5, 1., 0.5, 0.5};
				for (int32 Index = 0; Index < 9; ++Index)
				{
					const double PlaneZ = PatchSamples[0].Z
						+ (PatchSamples[1].Z - PatchSamples[0].Z) * U[Index]
						+ (PatchSamples[3].Z - PatchSamples[0].Z) * V[Index];
					if (FMath::Abs(PatchSamples[Index].Z - PlaneZ) > 2.0) { bPlanar = false; break; }
				}
			}
			if (bPlanar)
			{
				const int32 First = Vertices.Num();
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Vertices.Add(FVector(Points[Index].X - Origin.X, Points[Index].Y - Origin.Y,
						PatchSamples[Index].Z + 10.0 - Origin.Z));
					Normals.Add(FVector::UpVector);
					UVs.Add(FVector2D(Rim, 0.0));
				}
				Triangles.Append({First, First + 2, First + 1, First, First + 3, First + 2});
				return;
			}

			const bool bSplitX = Max.X - Min.X > MinSize;
			const bool bSplitY = Max.Y - Min.Y > MinSize;
			if (!bSplitX && !bSplitY) { ++MissingPatches; return; }
			// 얇은 외곽선도 긴 축을 따라 분할한다. 건설 면은 두 축을 함께 분할한다.
			if (bSplitX)
			{
				if (bSplitY)
				{
					AppendPatch(Min, Mid, Origin, MinSize, Rim, Trace);
					AppendPatch({Mid.X, Min.Y}, {Max.X, Mid.Y}, Origin, MinSize, Rim, Trace);
					AppendPatch({Min.X, Mid.Y}, {Mid.X, Max.Y}, Origin, MinSize, Rim, Trace);
					AppendPatch(Mid, Max, Origin, MinSize, Rim, Trace);
				}
				else
				{
					AppendPatch(Min, {Mid.X, Max.Y}, Origin, MinSize, Rim, Trace);
					AppendPatch({Mid.X, Min.Y}, Max, Origin, MinSize, Rim, Trace);
				}
			}
			else
			{
				AppendPatch(Min, {Max.X, Mid.Y}, Origin, MinSize, Rim, Trace);
				AppendPatch({Min.X, Mid.Y}, Max, Origin, MinSize, Rim, Trace);
			}
		}
	};
}

// 로드된 City_Pack 보도만 StreetSurface 쿼리에 참여시킨다. 스트리밍 이후 호출해 지면 감지를 준비한다.
void ACityPlotActor::PrepareCityGroundQueries(UWorld* CityWorld)
{
	if (!CityWorld)
	{
		return;
	}

	// AStaticMeshActor 만 보면 HISM 으로 묶인 인도를 놓친다(HISM 액터는 플레인 AActor).
	// UHierarchicalInstancedStaticMeshComponent 는 UStaticMeshComponent 파생이라 전 액터를 훑어 컴포넌트로 잡는다.
	int32 OptedInCount = 0;
	int32 InstancedCount = 0;
	int32 QueryForcedCount = 0;
	int32 NoSimpleGeomCount = 0;
	for (TActorIterator<AActor> It(CityWorld); It; ++It)
	{
		for (UStaticMeshComponent* SMC : TInlineComponentArray<UStaticMeshComponent*>(*It))
		{
			const UStaticMesh* Mesh = SMC ? SMC->GetStaticMesh() : nullptr;
			if (!Mesh || !Mesh->GetPathName().Contains(TEXT("/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Sidewalk")))
			{
				continue;
			}

			// ⚠ SetCollisionResponseToChannel 만으로는 모자라다 — 쿼리 콜리전이 꺼진 컴포넌트는 채널 응답과
			// 무관하게 트레이스에서 통째로 제외돼 "117개 켰는데 무응답" 이 된다(City_Pack 최적화 때 꺼진 상태).
			// 새로 켜는 것은 다른 채널을 전부 Ignore 로 중립화해 ch3 트레이스에만 참여시킨다 — 폰 이동·네비·
			// 건물 클릭(ch1)에 없던 차단을 만들지 않기 위해서다. 이미 쿼리가 켜져 있던 것은 응답을 건드리지 않는다.
			const ECollisionEnabled::Type Current = SMC->GetCollisionEnabled();
			if (Current == ECollisionEnabled::NoCollision || Current == ECollisionEnabled::PhysicsOnly)
			{
				SMC->SetCollisionResponseToAllChannels(ECR_Ignore);
				SMC->SetCollisionEnabled(Current == ECollisionEnabled::PhysicsOnly
					? ECollisionEnabled::QueryAndPhysics
					: ECollisionEnabled::QueryOnly);
				++QueryForcedCount;
			}

			SMC->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
			++OptedInCount;
			if (UInstancedStaticMeshComponent* InstancedSMC = Cast<UInstancedStaticMeshComponent>(SMC))
			{
				++InstancedCount;
				// ISM/HISM의 기존 물리 바디는 컴포넌트 BodyInstance와 충돌 필터를 별도로 보관한다.
				for (FBodyInstance* InstanceBody : InstancedSMC->InstanceBodies)
				{
					if (InstanceBody)
					{
						if (Current == ECollisionEnabled::NoCollision || Current == ECollisionEnabled::PhysicsOnly)
						{
							InstanceBody->SetResponseToAllChannels(ECR_Ignore);
							InstanceBody->SetCollisionEnabled(SMC->GetCollisionEnabled());
						}
						InstanceBody->SetResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
					}
				}
			}

			// 응답을 켜도 충돌 지오메트리가 없으면 트레이스가 아무것도 맞히지 못한다(최적화 임포트에서 흔함).
			if (const UBodySetup* Body = SMC->GetBodySetup())
			{
				if (Body->AggGeom.GetElementCount() == 0 && Body->CollisionTraceFlag == CTF_UseSimpleAsComplex)
				{
					++NoSimpleGeomCount;
				}
			}
			else
			{
				++NoSimpleGeomCount;
			}
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[PROBE] City_Pack sidewalk StreetSurface ON: %d (인스턴스드 %d, 쿼리강제 %d, 충돌지오메트리없음 %d)"),
		OptedInCount, InstancedCount, QueryForcedCount, NoSimpleGeomCount);
}

ACityPlotActor::ACityPlotActor()
{
	// 부지는 Wobble Timeline 을 쓰지 않으므로 Tick 불필요
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 클릭 트레이스(InteractableInputHandler)가 잡으려면 "Interactable" 태그 필요.
	// BoxComponent(channel1 Block)·IInputHandler 는 부모 생성자에서 이미 셋업됨 — 재생성 금지.
	Tags.Add(FName("Interactable"));

}

void ACityPlotActor::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 PlotId 를 지정한 채 배치된 부지 — 손으로 그린 박스가 기하 권위다.
	// SpawnManager 의 DT 스폰 경로는 Init(PlotId) 를 직접 불러 이 분기를 타지 않는다.
	if (!PlotId.IsNone() && !bInitializedFromTable)
	{
		InitFromAuthoredBounds();

		// 스스로 등록한다 — World Partition 은 월드 BeginPlay 이후에도 셀을 스트리밍 인 하므로
		// 서브시스템이 한 번 훑는 방식이면 그때 아직 로드 안 된 부지를 영구히 놓쳐
		// bOwnedAtStart 가 적용되지 않고 "먼저 부지를 인수하세요" 로 건설이 전부 막힌다.
		if (USpawnManager* SpawnMgr = GetWorld() ? GetWorld()->GetSubsystem<USpawnManager>() : nullptr)
		{
			SpawnMgr->RegisterPlacedPlot(this);
		}
	}
}

void ACityPlotActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClosePurchasePanel();
	SetPurchaseHighlight(false);
	if (UWorld* CityWorld = GetWorld())
	{
		CityWorld->GetTimerManager().ClearTimer(GroundBlockRetryTimer);
	}
	Super::EndPlay(EndPlayReason);
}

FVector2D ACityPlotActor::GetAuthoredHalfExtent() const
{
	const FIntPoint Cells = GetAuthoredCells();
	const double Cell = FPlotPlacementRules::GetPlotCellSize(ResolvePlotBaseCellSize(GetWorld()));
	return FVector2D(Cells.X * Cell * 0.5, Cells.Y * Cell * 0.5);
}

void ACityPlotActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 박스는 PlotSizeCells 로부터 매번 다시 그린다 — 여기서 박스를 "읽어서 되쓰면" 아키타입으로
	// 리셋된 값을 그대로 확정해 사용자가 Details 에 넣은 크기를 덮어쓴다(에디터가 액터를 옮길 때마다).
	if (!BoxComponent)
	{
		return;
	}

	const FVector2D Half = GetAuthoredHalfExtent();
	constexpr float PlotBoxHeight = 500.0f;
	BoxComponent->SetBoxExtent(FVector(Half.X, Half.Y, PlotBoxHeight), /*bUpdateOverlaps=*/false);
}

void ACityPlotActor::InitFromAuthoredBounds()
{
	if (bInitializedFromTable)
	{
		return;
	}

	bAuthoredBounds = true;

	// 기하 권위 = PlotSizeCells(액터 UPROPERTY). 박스는 거기서 파생되는 산출물이라 여기서도 다시 쓴다.
	CachedExtent = GetAuthoredHalfExtent();

	if (BoxComponent)
	{
		constexpr float PlotBoxHeight = 500.0f;
		BoxComponent->SetBoxExtent(FVector(CachedExtent.X, CachedExtent.Y, PlotBoxHeight));
	}

	const FIntPoint Cells = GetGridCells();
	UE_LOG(LogTemp, Log, TEXT("[CityPlot] %s 배치형 — 격자 %dx%d, 수용량 %d, 반경 %.0f x %.0f"),
		*PlotId.ToString(), Cells.X, Cells.Y,
		FPlotPlacementRules::DeriveCapacityFromGrid(Cells), CachedExtent.X, CachedExtent.Y);

}

FIntPoint ACityPlotActor::GetGridCells() const
{
	if (bAuthoredBounds)
	{
		return GetAuthoredCells();
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return FIntPoint(0, 0);
	}
	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	return bOk ? FIntPoint(PlotData.GridCols, PlotData.GridRows) : FIntPoint(0, 0);
}

int32 ACityPlotActor::GetBuildingCapacity() const
{
	if (bAuthoredBounds)
	{
		// 파인 칸이 있으면 그만큼 수용량이 준다 — 울타리 모양과 수용량이 같은 저작값에서 나와야
		// "보이는 땅보다 많이 지어지는" 어긋남이 안 생긴다. 제외 칸이 없으면 칸수 곱과 같다.
		return GetOccupiedCellCount();
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return 0;
	}
	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	return bOk ? PlotData.BuildingCapacity : 0;
}

void ACityPlotActor::Init(FName InPlotId)
{
	PlotId = InPlotId;
	bInitializedFromTable = true;
	bAuthoredBounds = false;

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityPlot] Init: TableManager null (PlotId=%s)"), *PlotId.ToString());
		return;
	}

	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityPlot] Init: no DT row for PlotId=%s"), *PlotId.ToString());
		return;
	}

	// Extent(footprint 반경)는 격자(GridCols/Rows × 셀크기)에서 산출한다.
	// 반경 = 칸수 × 셀크기 / 2. DT 의 Extent 필드는 보조/derive 용도(직접대입하지 않음).
	const int32 GridCols = FMath::Max(1, PlotData.GridCols);
	const int32 GridRows = FMath::Max(1, PlotData.GridRows);
	// 기하는 전부 월드 판정 단일 규칙 — 전역은 배치/오라처럼 월드가 손에 없는 경로용이다.
	const double TableCell = FPlotPlacementRules::GetPlotCellSize(ResolvePlotBaseCellSize(GetWorld()));
	CachedExtent.X = GridCols * TableCell * 0.5;
	CachedExtent.Y = GridRows * TableCell * 0.5;

	// 인터랙션 볼륨을 부지 격자 footprint 에 맞춤 (2D extent → Box extent X/Y, Z 는 기존 볼륨 두께 유지)
	if (BoxComponent)
	{
		constexpr float PlotBoxHeight = 500.0f;
		BoxComponent->SetBoxExtent(FVector(CachedExtent.X, CachedExtent.Y, PlotBoxHeight));
	}

	// 소유 상태는 Init 이 건드리지 않는다 — SpawnCityPlots(bOwnedAtStart) + RestorePlotOwnership/구매(Task3) 가 단일 권위.

	ResolveGroundBlockAuthority();
}

namespace
{
	// 도시 지면 메시 판정 — 원본 도시 타일과, 여러 타일을 하나로 합친 머지 메시 둘 다 인정한다.
	// 머지 메시는 에디터 Merge Actors 산출물이라 /TheRiverwalkCity/ 가 아닌 _GENERATED 아래로 떨어진다.
	bool IsCityGroundMesh(const UStaticMesh* Mesh)
	{
		if (!Mesh) { return false; }
		const FString Path = Mesh->GetPathName();
		return Path.Contains(TEXT("/TheRiverwalkCity/")) || Path.Contains(TEXT("/_GENERATED/"));
	}

	// footprint 접지 판정용 "건축 가능 지면" 판별. 리버워크 블록 탐색과 분리한다 —
	// City_Pack 블록은 단일 타일이 아니라 보도 모듈 조립체다.
	// 도로는 일부러 제외한다(도로 위 건설 금지). 보도 모듈을 따라가므로 L자·돌출부 같은 비정형 블록도
    // 사각형 박스와 무관하게 실형상대로 걸러진다.
	bool IsBuildableCityGroundMesh(const UStaticMesh* Mesh)
	{
		if (!Mesh) { return false; }
		if (IsCityGroundMesh(Mesh)) { return true; }
		// PrepareCityGroundQueries 가 ch3 를 켜는 대상과 같은 집합이어야 한다 —
		// 한쪽만 바뀌면 "채널은 응답하는데 전부 무효"(전면 빨강) 또는 "무응답 → 제약 전면 해제"로 조용히 갈린다.
		return Mesh->GetPathName().Contains(TEXT("/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Sidewalk"));
	}

	// [Perf] 접지 권위용 스트리트 타일 후보 수집은 프레임당 1회만 한다.
	struct FStreetTileCandidate { FVector BoundsOrigin; AStaticMeshActor* Actor; };
	TWeakObjectPtr<UWorld> GGroundBlockCacheWorld;
	uint64 GGroundBlockCacheFrame = static_cast<uint64>(-1);
	TArray<FStreetTileCandidate> GGroundBlockCandidates;

	const TArray<FStreetTileCandidate>& GetStreetTileCandidates(UWorld* Wld)
	{
		// 같은 월드·같은 프레임이면 재사용. 프레임 단위로만 재스캔 → 스트리밍 중 재시도(다음 프레임)엔 자동 최신화.
		if (GGroundBlockCacheWorld.Get() == Wld && GGroundBlockCacheFrame == GFrameCounter)
		{
			return GGroundBlockCandidates;
		}
		GGroundBlockCacheWorld = Wld;
		GGroundBlockCacheFrame = GFrameCounter;
		GGroundBlockCandidates.Reset();

		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(Wld, AStaticMeshActor::StaticClass(), Found);
		for (AActor* Candidate : Found)
		{
			AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Candidate);
			if (!MeshActor) continue;
			UStaticMeshComponent* CandidateSMC = MeshActor->GetStaticMeshComponent();
			UStaticMesh* CandidateMesh = CandidateSMC ? CandidateSMC->GetStaticMesh() : nullptr;
			if (!IsCityGroundMesh(CandidateMesh)) continue;

			FVector BoundsOrigin, BoundsExtent;
			MeshActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
			GGroundBlockCandidates.Add({ BoundsOrigin, MeshActor });
		}
		return GGroundBlockCandidates;
	}
}

float ACityPlotActor::TraceGroundZ() const
{
	const FVector Center = GetActorLocation();
	float GroundZ = Center.Z;
	TryTraceGroundZ(FVector2D(Center.X, Center.Y), GroundZ);
	return GroundZ;
}

bool ACityPlotActor::TryTraceGroundZ(const FVector2D& WorldXY, float& OutGroundZ) const
{
	// 부지 액터 Z 는 사람이 눈으로 놓은 값이라 실제 바닥과 수 m 어긋날 수 있다(실측 5m).
	// StreetSurface(건축 가능 지면 채널)로 지정한 XY의 높이를 잰다.
	const UWorld* Wld = GetWorld();
	const FVector Center = GetActorLocation();
	if (!Wld)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlotBoundaryGroundProbe), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(this);
	if (Wld->LineTraceSingleByChannel(Hit,
			FVector(WorldXY.X, WorldXY.Y, Center.Z + 2000.f),
			FVector(WorldXY.X, WorldXY.Y, Center.Z - 2000.f),
			ECC_GameTraceChannel3, Params))
	{
		OutGroundZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

FIntPoint ACityPlotActor::GetAuthoredCells() const
{
	return FIntPoint(FMath::Max(1, PlotSizeCells.X), FMath::Max(1, PlotSizeCells.Y));
}

bool ACityPlotActor::IsCellOccupied(int32 CellX, int32 CellY) const
{
	const FIntPoint Cells = GetAuthoredCells();
	if (CellX < 0 || CellY < 0 || CellX >= Cells.X || CellY >= Cells.Y)
	{
		return false;
	}
	return !ExcludedCells.Contains(FIntPoint(CellX, CellY));
}

void ACityPlotActor::GetBoundarySegments(TArray<FCityPlotBoundarySegment>& OutSegments) const
{
	OutSegments.Reset();
	if (!bAuthoredBounds)
	{
		const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
		const FVector2D Min = Center - CachedExtent;
		const FVector2D Max = Center + CachedExtent;
		if (CachedExtent.X > 0.0 && CachedExtent.Y > 0.0)
		{
			OutSegments.Add({ FVector2D(Min.X, Min.Y), FVector2D(Max.X, Min.Y), FVector2D(0, 1) });
			OutSegments.Add({ FVector2D(Max.X, Min.Y), FVector2D(Max.X, Max.Y), FVector2D(-1, 0) });
			OutSegments.Add({ FVector2D(Max.X, Max.Y), FVector2D(Min.X, Max.Y), FVector2D(0, -1) });
			OutSegments.Add({ FVector2D(Min.X, Max.Y), FVector2D(Min.X, Min.Y), FVector2D(1, 0) });
		}
		return;
	}

	const FIntPoint Cells = GetAuthoredCells();
	const double Cell = FPlotPlacementRules::GetPlotCellSize(ResolvePlotBaseCellSize(GetWorld()));
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	const FVector2D Origin = Center - FVector2D(Cells.X * Cell * 0.5, Cells.Y * Cell * 0.5);
	for (int32 CellY = 0; CellY <= Cells.Y; ++CellY)
	{
		int32 RunStart = -1;
		int32 RunDir = 0;
		for (int32 CellX = 0; CellX <= Cells.X; ++CellX)
		{
			const bool bBelow = IsCellOccupied(CellX, CellY - 1);
			const bool bAbove = IsCellOccupied(CellX, CellY);
			const int32 Dir = (CellX < Cells.X && bBelow != bAbove) ? (bBelow ? -1 : 1) : 0;
			if (Dir != RunDir)
			{
				if (RunDir != 0 && RunStart >= 0)
				{
					const FVector2D Left(Origin.X + RunStart * Cell, Origin.Y + CellY * Cell);
					const FVector2D Right(Origin.X + CellX * Cell, Origin.Y + CellY * Cell);
					OutSegments.Add(RunDir > 0
						? FCityPlotBoundarySegment{ Left, Right, FVector2D(0, 1) }
						: FCityPlotBoundarySegment{ Right, Left, FVector2D(0, -1) });
				}
				RunStart = CellX;
				RunDir = Dir;
			}
		}
	}
	for (int32 CellX = 0; CellX <= Cells.X; ++CellX)
	{
		int32 RunStart = -1;
		int32 RunDir = 0;
		for (int32 CellY = 0; CellY <= Cells.Y; ++CellY)
		{
			const bool bLeft = IsCellOccupied(CellX - 1, CellY);
			const bool bRight = IsCellOccupied(CellX, CellY);
			const int32 Dir = (CellY < Cells.Y && bLeft != bRight) ? (bLeft ? 1 : -1) : 0;
			if (Dir != RunDir)
			{
				if (RunDir != 0 && RunStart >= 0)
				{
					const FVector2D Bottom(Origin.X + CellX * Cell, Origin.Y + RunStart * Cell);
					const FVector2D Top(Origin.X + CellX * Cell, Origin.Y + CellY * Cell);
					OutSegments.Add(RunDir > 0
						? FCityPlotBoundarySegment{ Bottom, Top, FVector2D(-1, 0) }
						: FCityPlotBoundarySegment{ Top, Bottom, FVector2D(1, 0) });
				}
				RunStart = CellY;
				RunDir = Dir;
			}
		}
	}
}

bool ACityPlotActor::IsDressingBoundaryValid() const
{
	const int32 Count = DressingBoundaryLocalVertices.Num();
	if (Count < 4 || (Count & 1) != 0 || CachedExtent.X <= 0.0 || CachedExtent.Y <= 0.0)
	{
		return false;
	}
	constexpr double Tolerance = 0.01;
	double TwiceArea = 0.0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2D& A = DressingBoundaryLocalVertices[Index];
		const FVector2D& B = DressingBoundaryLocalVertices[(Index + 1) % Count];
		if (!FMath::IsFinite(A.X) || !FMath::IsFinite(A.Y)
			|| FMath::Abs(A.X) > CachedExtent.X + Tolerance || FMath::Abs(A.Y) > CachedExtent.Y + Tolerance
			|| (FMath::Abs(A.X - B.X) <= Tolerance) == (FMath::Abs(A.Y - B.Y) <= Tolerance))
		{
			return false;
		}
		TwiceArea += A.X * B.Y - B.X * A.Y;
	}
	if (FMath::Abs(TwiceArea) <= Tolerance)
	{
		return false;
	}
	// Non-neighboring edges may not intersect or touch; this also rejects self-crossing outlines.
	for (int32 First = 0; First < Count; ++First)
	{
		const FVector2D& A = DressingBoundaryLocalVertices[First];
		const FVector2D& B = DressingBoundaryLocalVertices[(First + 1) % Count];
		for (int32 Second = First + 1; Second < Count; ++Second)
		{
			if (Second == First + 1 || (First == 0 && Second == Count - 1)) { continue; }
			const FVector2D& C = DressingBoundaryLocalVertices[Second];
			const FVector2D& D = DressingBoundaryLocalVertices[(Second + 1) % Count];
			const bool bABHorizontal = FMath::Abs(A.Y - B.Y) <= Tolerance;
			const bool bCDHorizontal = FMath::Abs(C.Y - D.Y) <= Tolerance;
			bool bIntersects = false;
			if (bABHorizontal && bCDHorizontal)
			{
				bIntersects = FMath::Abs(A.Y - C.Y) <= Tolerance
					&& FMath::Max(FMath::Min(A.X, B.X), FMath::Min(C.X, D.X))
						<= FMath::Min(FMath::Max(A.X, B.X), FMath::Max(C.X, D.X)) + Tolerance;
			}
			else if (!bABHorizontal && !bCDHorizontal)
			{
				bIntersects = FMath::Abs(A.X - C.X) <= Tolerance
					&& FMath::Max(FMath::Min(A.Y, B.Y), FMath::Min(C.Y, D.Y))
						<= FMath::Min(FMath::Max(A.Y, B.Y), FMath::Max(C.Y, D.Y)) + Tolerance;
			}
			else
			{
				const FVector2D& H0 = bABHorizontal ? A : C;
				const FVector2D& H1 = bABHorizontal ? B : D;
				const FVector2D& V0 = bABHorizontal ? C : A;
				const FVector2D& V1 = bABHorizontal ? D : B;
				bIntersects = V0.X >= FMath::Min(H0.X, H1.X) - Tolerance
					&& V0.X <= FMath::Max(H0.X, H1.X) + Tolerance
					&& H0.Y >= FMath::Min(V0.Y, V1.Y) - Tolerance
					&& H0.Y <= FMath::Max(V0.Y, V1.Y) + Tolerance;
			}
			if (bIntersects) { return false; }
		}
	}
	return true;
}

bool ACityPlotActor::GetDressingBoundarySegments(TArray<FCityPlotBoundarySegment>& OutSegments) const
{
	OutSegments.Reset();
	if (DressingBoundaryLocalVertices.IsEmpty())
	{
		GetBoundarySegments(OutSegments);
		return true;
	}
	if (!bAuthoredBounds || !IsDressingBoundaryValid())
	{
		if (!bLoggedInvalidDressingBoundary)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CityPlot] %s: invalid decoration boundary; suppressing dressing"), *PlotId.ToString());
			bLoggedInvalidDressingBoundary = true;
		}
		return false;
	}
	const int32 Count = DressingBoundaryLocalVertices.Num();
	double TwiceArea = 0.0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2D& A = DressingBoundaryLocalVertices[Index];
		const FVector2D& B = DressingBoundaryLocalVertices[(Index + 1) % Count];
		TwiceArea += A.X * B.Y - B.X * A.Y;
	}
	const double Winding = TwiceArea > 0.0 ? 1.0 : -1.0;
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2D Start = Center + DressingBoundaryLocalVertices[Index];
		const FVector2D End = Center + DressingBoundaryLocalVertices[(Index + 1) % Count];
		const FVector2D Direction = (End - Start).GetSafeNormal();
		OutSegments.Add({ Start, End, FVector2D(-Direction.Y, Direction.X) * Winding });
	}
	return true;
}

bool ACityPlotActor::IsDressingFootprintAllowed(const FVector2D& Center, const FVector2D& HalfExtent) const
{
	if (!FMath::IsFinite(Center.X) || !FMath::IsFinite(Center.Y)
		|| !FMath::IsFinite(HalfExtent.X) || !FMath::IsFinite(HalfExtent.Y)
		|| HalfExtent.X <= 0.0 || HalfExtent.Y <= 0.0)
	{
		return false;
	}
	if (!DressingBoundaryLocalVertices.IsEmpty())
	{
		TArray<FCityPlotBoundarySegment> Boundary;
		if (!GetDressingBoundarySegments(Boundary)) { return false; }
		const FVector2D RectMin = Center - HalfExtent;
		const FVector2D RectMax = Center + HalfExtent;
		const FVector2D Corners[] = {
			RectMin, FVector2D(RectMax.X, RectMin.Y), RectMax, FVector2D(RectMin.X, RectMax.Y)
		};
		for (const FVector2D& Corner : Corners)
		{
			bool bOnBoundary = false;
			bool bInside = false;
			for (const FCityPlotBoundarySegment& Edge : Boundary)
			{
				if ((FMath::IsNearlyEqual(Edge.Start.X, Edge.End.X, 0.01)
					&& FMath::IsNearlyEqual(Corner.X, Edge.Start.X, 0.01)
					&& Corner.Y >= FMath::Min(Edge.Start.Y, Edge.End.Y) - 0.01
					&& Corner.Y <= FMath::Max(Edge.Start.Y, Edge.End.Y) + 0.01)
					|| (FMath::IsNearlyEqual(Edge.Start.Y, Edge.End.Y, 0.01)
						&& FMath::IsNearlyEqual(Corner.Y, Edge.Start.Y, 0.01)
						&& Corner.X >= FMath::Min(Edge.Start.X, Edge.End.X) - 0.01
						&& Corner.X <= FMath::Max(Edge.Start.X, Edge.End.X) + 0.01))
				{
					bOnBoundary = true;
					break;
				}
				if ((Edge.Start.Y > Corner.Y) != (Edge.End.Y > Corner.Y))
				{
					const double CrossX = Edge.Start.X + (Corner.Y - Edge.Start.Y)
						* (Edge.End.X - Edge.Start.X) / (Edge.End.Y - Edge.Start.Y);
					if (CrossX > Corner.X) { bInside = !bInside; }
				}
			}
			if (!bOnBoundary && !bInside) { return false; }
		}
		// A concave notch can pass through the rectangle while all four corners remain inside.
		for (const FCityPlotBoundarySegment& Edge : Boundary)
		{
			if (FMath::IsNearlyEqual(Edge.Start.X, Edge.End.X, 0.01))
			{
				if (Edge.Start.X > RectMin.X && Edge.Start.X < RectMax.X
					&& FMath::Max(FMath::Min(Edge.Start.Y, Edge.End.Y), RectMin.Y)
						< FMath::Min(FMath::Max(Edge.Start.Y, Edge.End.Y), RectMax.Y))
				{
					return false;
				}
			}
			else if (Edge.Start.Y > RectMin.Y && Edge.Start.Y < RectMax.Y
				&& FMath::Max(FMath::Min(Edge.Start.X, Edge.End.X), RectMin.X)
					< FMath::Min(FMath::Max(Edge.Start.X, Edge.End.X), RectMax.X))
			{
				return false;
			}
		}
		return true;
	}
	if (!bAuthoredBounds)
	{
		return true;
	}
	if (CachedExtent.X <= 0.0 || CachedExtent.Y <= 0.0)
	{
		return false;
	}

	const FVector PlotLocation = GetActorLocation();
	const FVector2D PlotMin(PlotLocation.X - CachedExtent.X, PlotLocation.Y - CachedExtent.Y);
	const FVector2D PlotMax(PlotLocation.X + CachedExtent.X, PlotLocation.Y + CachedExtent.Y);
	const FVector2D RectMin = Center - HalfExtent;
	const FVector2D RectMax = Center + HalfExtent;
	if (RectMin.X < PlotMin.X || RectMin.Y < PlotMin.Y || RectMax.X > PlotMax.X || RectMax.Y > PlotMax.Y)
	{
		return false;
	}

	const FIntPoint Cells = GetAuthoredCells();
	const FVector2D CellSize(CachedExtent.X * 2.0 / Cells.X, CachedExtent.Y * 2.0 / Cells.Y);
	for (int32 CellY = 0; CellY < Cells.Y; ++CellY)
	{
		for (int32 CellX = 0; CellX < Cells.X; ++CellX)
		{
			if (IsCellOccupied(CellX, CellY))
			{
				continue;
			}
			const FVector2D CellMin = PlotMin + FVector2D(CellX * CellSize.X, CellY * CellSize.Y);
			const FVector2D CellMax = CellMin + CellSize;
			// 경계를 맞대는 것은 허용하고, 제외 칸의 내부를 덮으면 거부한다.
			if (RectMin.X < CellMax.X && RectMax.X > CellMin.X
				&& RectMin.Y < CellMax.Y && RectMax.Y > CellMin.Y)
			{
				return false;
			}
		}
	}
	return true;
}

int32 ACityPlotActor::GetOccupiedCellCount() const
{
	const FIntPoint Cells = GetAuthoredCells();
	int32 Count = 0;
	for (int32 CellY = 0; CellY < Cells.Y; ++CellY)
	{
		for (int32 CellX = 0; CellX < Cells.X; ++CellX)
		{
			Count += IsCellOccupied(CellX, CellY) ? 1 : 0;
		}
	}
	return Count;
}

UStaticMeshComponent* ACityPlotActor::AcquireOutlinePiece(TArray<UStaticMeshComponent*>& Pool,
	TArray<UMaterialInstanceDynamic*>& MIDPool, int32 Index,
	UStaticMesh* Mesh, UMaterialInterface* Mat)
{
	while (Pool.Num() <= Index)
	{
		Pool.Add(nullptr);
		MIDPool.Add(nullptr);
	}
	if (Pool[Index])
	{
		return Pool[Index];
	}
	if (!Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
	Piece->SetupAttachment(RootComponent);
	Piece->RegisterComponent();
	Piece->SetStaticMesh(Mesh);
	Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetCanEverAffectNavigation(false);
	Piece->SetCastShadow(false);
	Piece->SetVisibility(false);
	// 색은 머티리얼 기본값이 소유한다 — 생김새 노브는 머티리얼, 기하 노브만 C++.
	MIDPool[Index] = Mat ? Piece->CreateDynamicMaterialInstance(0, Mat) : nullptr;
	Pool[Index] = Piece;
	return Piece;
}

void ACityPlotActor::EnsureOutlineBuilt()
{
	// 조각 수가 모양에서 결정되므로 여기서는 아무것도 미리 만들지 않는다.
	// 실제 생성은 SetBuildModeOutlineVisible 이 필요한 개수만큼 AcquireOutlinePiece 로 한다.
}

void ACityPlotActor::SetBuildModeOutlineVisible(bool bVisible)
{
	// 건설 테두리는 소유 부지에만 사용한다. 미소유 선택선은 인수 다이얼로그 수명을 따른다.
	const bool bShow = bVisible && bOwned && bAuthoredBounds;

	// 이 함수는 드래그 중 매 프레임 불린다(UpdatePlotFootprintPreview -> SetOwnedPlotOutlines).
	// 아래 조립은 복합 라인트레이스 + 머티리얼 로드 + 조각 전원 트랜스폼/MID 재기록이라 프레임당
	// 반복할 비용이 아니고, 세션 중에는 입력(칸수/제외칸/위치/바닥)이 하나도 안 바뀐다.
	// bShow 가 bOwned 를 포함하므로 세션 중 인수한 부지는 다음 프레임에 전환으로 잡힌다.
	if (bShow == bOutlineShown)
	{
		return;
	}
	bOutlineShown = bShow;

	auto HideAll = [this]()
	{
		for (UStaticMeshComponent* P : OutlineEdgeSMCs) { if (P) { P->SetVisibility(false); } }
		for (UStaticMeshComponent* P : OutlinePostSMCs) { if (P) { P->SetVisibility(false); } }
		if (PlotFloorGlowMesh) { PlotFloorGlowMesh->SetVisibility(false); }
	};

	if (!bShow)
	{
		HideAll();
		return;
	}

	UCGGameInstance* GI = UCGGameInstance::GetInstance();
	UGlobalAssetCache* AssetCache = GI ? GI->GetGlobalAssetCache() : nullptr;
	UStaticMesh* RailMesh = AssetCache ? AssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Cube) : nullptr;
	UStaticMesh* PostMesh = AssetCache ? AssetCache->GetPrimitiveShapeMesh(EPrimitiveShapeType::Cylinder) : nullptr;
	if (!RailMesh || !PostMesh)
	{
		return;
	}

	TSoftObjectPtr<UMaterialInterface> EdgePath(
		FSoftObjectPath(TEXT("/Game/CompanyGrowth/Resources/Materials/M_PlotEdgeFlow.M_PlotEdgeFlow")));
	UMaterialInterface* EdgeMat = EdgePath.LoadSynchronous();
	TSoftObjectPtr<UMaterialInterface> GlowPath(
		FSoftObjectPath(TEXT("/Game/CompanyGrowth/Resources/Materials/M_PlotFloorGlow.M_PlotFloorGlow")));
	UMaterialInterface* GlowMat = GlowPath.LoadSynchronous();

	const FVector RailBound = RailMesh->GetBounds().BoxExtent * 2.0;
	const FVector PostBound = PostMesh->GetBounds().BoxExtent * 2.0;
	const double RailBaseXY = FMath::Max<double>(KINDA_SMALL_NUMBER, FMath::Max(RailBound.X, RailBound.Y));
	const double RailBaseZ = FMath::Max<double>(KINDA_SMALL_NUMBER, RailBound.Z);
	const double PostBaseXY = FMath::Max<double>(KINDA_SMALL_NUMBER, FMath::Max(PostBound.X, PostBound.Y));
	const double PostBaseZ = FMath::Max<double>(KINDA_SMALL_NUMBER, PostBound.Z);

	// 시각 치수는 도시 지면 규격에 비례하며, 건물 점유 간격과 독립적이다.
	const double PlotBaseCell = ResolvePlotBaseCellSize(GetWorld());
	const double Cell = FPlotPlacementRules::GetPlotCellSize(PlotBaseCell);
	const double FenceHeight = FMath::Max(60.0, PlotBaseCell * 0.045);
	const double RailThickness = FMath::Max(12.0, PlotBaseCell * 0.0075);
	const double PostDiameter = RailThickness * 2.0;
	const double DashPeriod = PlotBaseCell * 0.31;
	const double DashLength = PlotBaseCell * 0.11;
	const double FlowSpeed = PlotBaseCell * 0.25;

	constexpr float OutlineLiftCm = 35.f; // 지면과 겹치지 않도록 밑동을 살짝 띄운다.
	const FVector Center = GetActorLocation();
	const double GroundZ = TraceGroundZ() + OutlineLiftCm;
	const double FenceZ = GroundZ + FenceHeight * 0.5;
	const int32 CellsX = GetAuthoredCells().X;
	const int32 CellsY = GetAuthoredCells().Y;
	// 칸 격자의 (0,0) 모서리가 놓이는 월드 좌표. 액터는 부지 중심에 있다.
	const double OriginX = Center.X - CellsX * Cell * 0.5;
	const double OriginY = Center.Y - CellsY * Cell * 0.5;

	// The same logical perimeter drives both construction rails and dressing's default boundary.
	TArray<FCityPlotBoundarySegment> Rails;
	GetBoundarySegments(Rails);
	TSet<FIntPoint> PostVerts;
	for (const FCityPlotBoundarySegment& Run : Rails)
	{
		PostVerts.Add(FIntPoint(FMath::RoundToInt((Run.Start.X - OriginX) / Cell),
			FMath::RoundToInt((Run.Start.Y - OriginY) / Cell)));
		PostVerts.Add(FIntPoint(FMath::RoundToInt((Run.End.X - OriginX) / Cell),
			FMath::RoundToInt((Run.End.Y - OriginY) / Cell)));
	}

	// === 레일 배치 ===
	int32 RailIndex = 0;
	for (const FCityPlotBoundarySegment& Run : Rails)
	{
		UStaticMeshComponent* Piece = AcquireOutlinePiece(OutlineEdgeSMCs, OutlineEdgeMIDs, RailIndex, RailMesh, EdgeMat);
		if (!Piece) { break; }

		Piece->SetWorldLocation(FVector((Run.Start.X + Run.End.X) * 0.5,
			(Run.Start.Y + Run.End.Y) * 0.5, FenceZ));
		Piece->SetWorldRotation(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Run.End.Y - Run.Start.Y, Run.End.X - Run.Start.X)), 0.0));
		// 길이에서 기둥 지름을 빼 레일이 기둥 안쪽에서 끝나게 한다 — 겹치면 Additive 라 코너만 밝아진다.
		const double RailLen = FMath::Max(1.0, (Run.End - Run.Start).Size() - PostDiameter);
		Piece->SetWorldScale3D(FVector(RailLen / RailBaseXY, RailThickness / RailBaseXY, FenceHeight / RailBaseZ));
		Piece->SetVisibility(true);

		if (UMaterialInstanceDynamic* MID = OutlineEdgeMIDs.IsValidIndex(RailIndex) ? OutlineEdgeMIDs[RailIndex] : nullptr)
		{
			const FVector Flow = Piece->GetForwardVector();
			MID->SetVectorParameterValue(TEXT("FlowAxis"),
				FLinearColor(static_cast<float>(Flow.X), static_cast<float>(Flow.Y), 0.f, 0.f));
			MID->SetVectorParameterValue(TEXT("CrossAxis"), FLinearColor(0.f, 0.f, 1.f, 0.f));
			MID->SetScalarParameterValue(TEXT("EdgeOrigin"), static_cast<float>(GroundZ));
			MID->SetScalarParameterValue(TEXT("FenceHeight"), static_cast<float>(FenceHeight));
			MID->SetScalarParameterValue(TEXT("DashPeriod"), static_cast<float>(DashPeriod));
			MID->SetScalarParameterValue(TEXT("DashLength"), static_cast<float>(DashLength));
			MID->SetScalarParameterValue(TEXT("FlowSpeed"), static_cast<float>(FlowSpeed));
		}
		++RailIndex;
	}
	for (int32 i = RailIndex; i < OutlineEdgeSMCs.Num(); ++i)
	{
		if (OutlineEdgeSMCs[i]) { OutlineEdgeSMCs[i]->SetVisibility(false); }
	}

	// === 코너 기둥 ===
	int32 PostIndex = 0;
	for (const FIntPoint& Vert : PostVerts)
	{
		UStaticMeshComponent* Piece = AcquireOutlinePiece(OutlinePostSMCs, OutlinePostMIDs, PostIndex, PostMesh, EdgeMat);
		if (!Piece) { break; }

		Piece->SetWorldLocation(FVector(OriginX + Vert.X * Cell, OriginY + Vert.Y * Cell, FenceZ));
		Piece->SetWorldRotation(FRotator::ZeroRotator);
		Piece->SetWorldScale3D(FVector(PostDiameter / PostBaseXY, PostDiameter / PostBaseXY, FenceHeight / PostBaseZ));
		Piece->SetVisibility(true);

		if (UMaterialInstanceDynamic* MID = OutlinePostMIDs.IsValidIndex(PostIndex) ? OutlinePostMIDs[PostIndex] : nullptr)
		{
			// 기둥은 끊기지 않는다 — 점 길이를 주기와 같게 두면 SDF 가 항상 안쪽이라 꽉 찬 기둥이 된다.
			MID->SetVectorParameterValue(TEXT("FlowAxis"), FLinearColor(1.f, 0.f, 0.f, 0.f));
			MID->SetVectorParameterValue(TEXT("CrossAxis"), FLinearColor(0.f, 0.f, 1.f, 0.f));
			MID->SetScalarParameterValue(TEXT("EdgeOrigin"), static_cast<float>(GroundZ));
			MID->SetScalarParameterValue(TEXT("FenceHeight"), static_cast<float>(FenceHeight));
			MID->SetScalarParameterValue(TEXT("DashPeriod"), static_cast<float>(DashPeriod));
			MID->SetScalarParameterValue(TEXT("DashLength"), static_cast<float>(DashPeriod));
			MID->SetScalarParameterValue(TEXT("FlowSpeed"), 0.f);
		}
		++PostIndex;
	}
	for (int32 i = PostIndex; i < OutlinePostSMCs.Num(); ++i)
	{
		if (OutlinePostSMCs[i]) { OutlinePostSMCs[i]->SetVisibility(false); }
	}

	BuildFloorGlowMesh(GlowMat, Cell, CellsX, CellsY, Center, OriginX, OriginY);
}

void ACityPlotActor::BuildFloorGlowMesh(UMaterialInterface* GlowMat, double Cell, int32 CellsX, int32 CellsY,
	const FVector& Center, double OriginX, double OriginY)
{
	if (!GlowMat)
	{
		if (PlotFloorGlowMesh) { PlotFloorGlowMesh->SetVisibility(false); }
		return;
	}

	if (!PlotFloorGlowMesh)
	{
		PlotFloorGlowMesh = NewObject<UProceduralMeshComponent>(this, TEXT("PlotFloorGlowSurface"), RF_Transient);
		PlotFloorGlowMesh->SetupAttachment(RootComponent);
		PlotFloorGlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PlotFloorGlowMesh->SetGenerateOverlapEvents(false);
		PlotFloorGlowMesh->SetCanEverAffectNavigation(false);
		PlotFloorGlowMesh->SetCastShadow(false);
		PlotFloorGlowMesh->RegisterComponent();
	}
	PlotFloorGlowMesh->SetWorldLocationAndRotation(Center, FRotator::ZeroRotator);
	PlotFloorGlowMesh->SetWorldScale3D(FVector::OneVector);
	PlotFloorGlowMesh->ClearAllMeshSections();
	if (!PlotFloorGlowMID)
	{
		PlotFloorGlowMID = UMaterialInstanceDynamic::Create(GlowMat, this);
		PlotFloorGlowMesh->SetMaterial(0, PlotFloorGlowMID);
	}
	PlotFloorGlowMID->SetVectorParameterValue(TEXT("Center"),
		FLinearColor(static_cast<float>(Center.X), static_cast<float>(Center.Y), 0.f, 0.f));
	PlotFloorGlowMID->SetScalarParameterValue(TEXT("HalfX"), static_cast<float>(CellsX * Cell * 0.5));
	PlotFloorGlowMID->SetScalarParameterValue(TEXT("HalfY"), static_cast<float>(CellsY * Cell * 0.5));

	FCityPlotGroundSurface Surface;
	auto Trace = [this](const FVector2D& XY, float& OutZ) { return TryTraceGroundZ(XY, OutZ); };
	for (int32 CellY = 0; CellY < CellsY; ++CellY)
	{
		for (int32 CellX = 0; CellX < CellsX; ++CellX)
		{
			if (!IsCellOccupied(CellX, CellY)) { continue; }
			Surface.AppendPatch({OriginX + CellX * Cell, OriginY + CellY * Cell},
				{OriginX + (CellX + 1) * Cell, OriginY + (CellY + 1) * Cell},
				Center, Cell / 32.0, 0.f, Trace);
		}
	}
	if (Surface.Vertices.IsEmpty())
	{
		PlotFloorGlowMesh->SetVisibility(false);
		return;
	}
	PlotFloorGlowMesh->CreateMeshSection_LinearColor(0, Surface.Vertices, Surface.Triangles,
		Surface.Normals, {}, {}, {}, false);
	PlotFloorGlowMesh->SetVisibility(true);
}


void ACityPlotActor::BuildUnownedBoundaryMesh()
{
	UWorld* CityWorld = GetWorld();
	if (!CityWorld || bOwned || !bPurchaseHighlighted || CachedExtent.X <= 0.0 || CachedExtent.Y <= 0.0)
	{
		return;
	}

	TArray<FCityPlotBoundarySegment> Boundary;
	if (DressingBoundaryLocalVertices.IsEmpty() || !GetDressingBoundarySegments(Boundary))
	{
		GetBoundarySegments(Boundary);
	}
	if (Boundary.IsEmpty()) { return; }

	const float BaseCell = ResolvePlotBaseCellSize(CityWorld);
	const double Inset = BaseCell * 0.10;
	const double RimWidth = BaseCell * 0.012;
	const FVector MeshOrigin = GetActorLocation();
	auto TraceBoundaryGroundZ = [&](const FVector2D& XY, float& OutGroundZ) -> bool
	{
		const bool bHasStreetGround = TryTraceGroundZ(XY, OutGroundZ);

		// 도로 메시가 보도 채널보다 위에 있으면 시각 경계를 실제 위쪽 표면에 놓는다.
		FHitResult RoadHit;
		FCollisionQueryParams RoadQuery(SCENE_QUERY_STAT(UnownedPlotRoadGroundProbe), true);
		RoadQuery.AddIgnoredActor(this);
		for (int32 ProbeIndex = 0; ProbeIndex < 8; ++ProbeIndex)
		{
			if (!CityWorld->LineTraceSingleByChannel(RoadHit,
				FVector(XY.X, XY.Y, MeshOrigin.Z + 2000.f),
				FVector(XY.X, XY.Y, MeshOrigin.Z - 2000.f),
				ECC_Visibility, RoadQuery))
			{
				return bHasStreetGround;
			}

			const UPrimitiveComponent* HitComponent = RoadHit.GetComponent();
			const UStaticMeshComponent* RoadComponent = Cast<UStaticMeshComponent>(HitComponent);
			const UStaticMesh* RoadMesh = RoadComponent ? RoadComponent->GetStaticMesh() : nullptr;
			if (RoadMesh
				&& RoadMesh->GetPathName().StartsWith(TEXT("/Game/City_Pack/Meshes/Road-Sidewalk_Pieces/SM_Road"))
				&& RoadHit.ImpactNormal.Z >= 0.9f)
			{
				if (!bHasStreetGround || RoadHit.ImpactPoint.Z > OutGroundZ)
				{
					OutGroundZ = RoadHit.ImpactPoint.Z;
				}
				return true;
			}

			// 벤치 등 지면을 가리는 컴포넌트를 제외하고 실제 바닥을 찾는다.
			if (!HitComponent)
			{
				return bHasStreetGround;
			}
			RoadQuery.AddIgnoredComponent(HitComponent);
		}
		return bHasStreetGround;
	};

	TArray<int32> PreviousEdges;
	TArray<int32> NextEdges;
	PreviousEdges.Init(INDEX_NONE, Boundary.Num());
	NextEdges.Init(INDEX_NONE, Boundary.Num());
	for (int32 First = 0; First < Boundary.Num(); ++First)
	{
		for (int32 Second = 0; Second < Boundary.Num(); ++Second)
		{
			if (First != Second && FVector2D::Distance(Boundary[First].End, Boundary[Second].Start) <= 1.0)
			{
				NextEdges[First] = Second;
				PreviousEdges[Second] = First;
				break;
			}
		}
	}
	auto MiterPoint = [](const FVector2D& Corner, const FVector2D& FirstNormal,
		const FVector2D& SecondNormal, double Offset) -> FVector2D
	{
		const double Denominator = 1.0 + FVector2D::DotProduct(FirstNormal, SecondNormal);
		return Corner + (Denominator <= KINDA_SMALL_NUMBER ? SecondNormal * Offset
			: (FirstNormal + SecondNormal) * (Offset / Denominator));
	};

	TArray<FCityPlotBoundarySegment> InsetEdges[2];
	TArray<double> XBreaks;
	TArray<double> YBreaks;
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		for (int32 Index = 0; Index < Boundary.Num(); ++Index)
		{
			const FCityPlotBoundarySegment& Edge = Boundary[Index];
			const FVector2D PreviousNormal = PreviousEdges[Index] == INDEX_NONE
				? Edge.InwardNormal : Boundary[PreviousEdges[Index]].InwardNormal;
			const FVector2D NextNormal = NextEdges[Index] == INDEX_NONE
				? Edge.InwardNormal : Boundary[NextEdges[Index]].InwardNormal;
			FCityPlotBoundarySegment InsetEdge = Edge;
			InsetEdge.Start = MiterPoint(Edge.Start, PreviousNormal, Edge.InwardNormal, Inset + Layer * RimWidth);
			InsetEdge.End = MiterPoint(Edge.End, Edge.InwardNormal, NextNormal, Inset + Layer * RimWidth);
			InsetEdges[Layer].Add(InsetEdge);
			XBreaks.AddUnique(InsetEdge.Start.X);
			XBreaks.AddUnique(InsetEdge.End.X);
			YBreaks.AddUnique(InsetEdge.Start.Y);
			YBreaks.AddUnique(InsetEdge.End.Y);
		}
	}
	XBreaks.Sort();
	YBreaks.Sort();
	auto IsInside = [](const FVector2D& Point, const TArray<FCityPlotBoundarySegment>& Edges)
	{
		bool bInside = false;
		for (const FCityPlotBoundarySegment& Edge : Edges)
		{
			if ((Edge.Start.Y > Point.Y) != (Edge.End.Y > Point.Y))
			{
				const double CrossX = Edge.Start.X + (Point.Y - Edge.Start.Y)
					* (Edge.End.X - Edge.Start.X) / (Edge.End.Y - Edge.Start.Y);
				if (Point.X < CrossX) { bInside = !bInside; }
			}
		}
		return bInside;
	};

	// 직교 폴리곤의 모든 모서리 좌표에서 나누면 오목한 모양과 제외 칸도 사각 패치로 정확히 채워진다.
	// 색면과 외곽선은 서로 겹치지 않고 한 섹션/재질을 공유한다.
	FCityPlotGroundSurface Surface;
	for (int32 XIndex = 0; XIndex + 1 < XBreaks.Num(); ++XIndex)
	{
		for (int32 YIndex = 0; YIndex + 1 < YBreaks.Num(); ++YIndex)
		{
			const FVector2D Min(XBreaks[XIndex], YBreaks[YIndex]);
			const FVector2D Max(XBreaks[XIndex + 1], YBreaks[YIndex + 1]);
			if (Max.X - Min.X < 0.1 || Max.Y - Min.Y < 0.1) { continue; }
			const FVector2D Mid = (Min + Max) * 0.5;
			if (!IsInside(Mid, InsetEdges[0])) { continue; }
			const float Rim = IsInside(Mid, InsetEdges[1]) ? 0.f : 1.f;
			Surface.AppendPatch(Min, Max, MeshOrigin, BaseCell / 32.0, Rim, TraceBoundaryGroundZ);
		}
	}

	CityWorld->GetTimerManager().ClearTimer(BoundaryRetryTimer);
	if (Surface.MissingPatches > 0 && BoundaryRetryCount < 8)
	{
		++BoundaryRetryCount;
		CityWorld->GetTimerManager().SetTimer(BoundaryRetryTimer, this,
			&ACityPlotActor::BuildUnownedBoundaryMesh, 0.5f, false);
	}
	if (Surface.Vertices.IsEmpty())
	{
		bUnownedBoundaryBuilt = false;
		if (UnownedBoundaryMesh) { UnownedBoundaryMesh->SetVisibility(false); }
		return;
	}

	TSoftObjectPtr<UMaterialInterface> MaterialPath(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Resources/Materials/M_UnownedPlotBoundary.M_UnownedPlotBoundary")));
	UMaterialInterface* GroundMaterial = MaterialPath.LoadSynchronous();
	if (!GroundMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityPlot] M_UnownedPlotBoundary missing (PlotId=%s)"), *PlotId.ToString());
		return;
	}
	if (!UnownedBoundaryMesh)
	{
		UnownedBoundaryMesh = NewObject<UProceduralMeshComponent>(this, TEXT("UnownedPlotBoundary"), RF_Transient);
		UnownedBoundaryMesh->SetupAttachment(RootComponent);
		UnownedBoundaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UnownedBoundaryMesh->SetGenerateOverlapEvents(false);
		UnownedBoundaryMesh->SetCanEverAffectNavigation(false);
		UnownedBoundaryMesh->SetCastShadow(false);
		UnownedBoundaryMesh->RegisterComponent();
	}
	UnownedBoundaryMesh->SetWorldLocationAndRotation(MeshOrigin, FRotator::ZeroRotator);
	UnownedBoundaryMesh->SetWorldScale3D(FVector::OneVector);
	UnownedBoundaryMesh->ClearAllMeshSections();
	UnownedBoundaryMesh->CreateMeshSection_LinearColor(0, Surface.Vertices, Surface.Triangles,
		Surface.Normals, Surface.UVs, {}, {}, false);
	if (!UnownedBoundarySelectedMID)
	{
		UnownedBoundarySelectedMID = UMaterialInstanceDynamic::Create(GroundMaterial, this);
	}
	UnownedBoundarySelectedMID->SetScalarParameterValue(TEXT("SelectionStartedAt"), PurchaseHighlightStartedAt);
	UnownedBoundaryMesh->SetMaterial(0, UnownedBoundarySelectedMID);
	bUnownedBoundaryBuilt = Surface.MissingPatches == 0 || BoundaryRetryCount >= 8;
	UnownedBoundaryMesh->SetVisibility(bPurchaseHighlighted && !bOwned);
}

void ACityPlotActor::ResolveGroundBlockAuthority()
{
	UWorld* CityWorld = GetWorld();
	if (!CityWorld || bAuthoredBounds || bBlockResolved)
	{
		return;
	}

	// LevelInstance 타일의 피벗 대신 실제 바운드 중심을 사용해 접지 판정 블록을 찾는다.
	const FVector PlotLocation = GetActorLocation();
	constexpr float SearchRadiusCm = 2000.f;
	float ClosestDistSq = SearchRadiusCm * SearchRadiusCm;
	AStaticMeshActor* BlockActor = nullptr;
	for (const FStreetTileCandidate& Candidate : GetStreetTileCandidates(CityWorld))
	{
		const float DistSq = FVector::DistSquared(Candidate.BoundsOrigin, PlotLocation);
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			BlockActor = Candidate.Actor;
		}
	}

	if (!BlockActor)
	{
		constexpr int32 MaxGroundBlockRetries = 40;
		constexpr float RetryInterval = 0.5f;
		if (GroundBlockRetryCount < MaxGroundBlockRetries)
		{
			++GroundBlockRetryCount;
			CityWorld->GetTimerManager().SetTimer(GroundBlockRetryTimer, this,
				&ACityPlotActor::ResolveGroundBlockAuthority, RetryInterval, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CityPlot] no city block tile found near plot (PlotId=%s)"),
				*PlotId.ToString());
		}
		return;
	}

	// IsFootprintOnGround는 이 블록과 도시 지면 메시를 최종 접지 권위로 사용한다.
	CachedBlockActor = BlockActor;
	bBlockResolved = true;
}

bool ACityPlotActor::IsFootprintOnGround(const FVector& FootprintCenter, float HalfX, float HalfY) const
{
	bool PointsOnBlock[5];
	if (!EvaluateFootprintPoints(FootprintCenter, HalfX, HalfY, PointsOnBlock))
	{
		return true; // 트레이스 권위 없음(블록 미해결/채널 미설정) → 막지 않음
	}

	for (const bool bPointOnBlock : PointsOnBlock)
	{
		if (!bPointOnBlock)
		{
			return false; // 한 점이라도 블록 밖 = 깎인 모서리/인도
		}
	}
	return true;
}

// 논리 footprint와 메시 AABB 답이 갈림 → 실메시 트레이스가 최종 판정
bool ACityPlotActor::EvaluateFootprintPoints(const FVector& FootprintCenter, float HalfX, float HalfY,
	bool (&OutPointsOnBlock)[5]) const
{
	// 권위가 없으면 호출자가 "전부 가능"으로 해석하도록 전부 true 로 두고 false 반환.
	for (bool& bPoint : OutPointsOnBlock)
	{
		bPoint = true;
	}

	// 블록 미해결이어도 트레이스 수행. 1차 판정 = StreetSurface 채널 응답(도시 지면 여부), 블록 동일성은 정밀 판정
	// 조기 반환 시 블록 탐색 실패 기기에서 트레이스 생략 → 전부 가능 폴백 → 인도 배치·되밀기 미동작
	UWorld* Wld = GetWorld();
	if (!Wld)
	{
		return false;
	}

	// 트레이스 채널 = StreetSurface(ECC_GameTraceChannel3). IsPlacementGrounded와 동일 출처
	const ETraceTypeQuery StreetTraceType = UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel3);

	// 깎인 모서리는 실형상 바깥 → bTraceComplex=true로 진짜 모양을 따라 판정
	constexpr bool bTraceComplex = true;

	AActor* BlockActor = CachedBlockActor.Get();

	// footprint 5점 = 중심 + 4코너(살짝 안쪽으로 0.98 곱해 경계 노이즈 회피).
	constexpr float CornerInset = 0.98f;
	const float InsetX = HalfX * CornerInset;
	const float InsetY = HalfY * CornerInset;
	const FVector2D Points[5] = {
		FVector2D(FootprintCenter.X, FootprintCenter.Y),
		FVector2D(FootprintCenter.X - InsetX, FootprintCenter.Y - InsetY),
		FVector2D(FootprintCenter.X + InsetX, FootprintCenter.Y - InsetY),
		FVector2D(FootprintCenter.X - InsetX, FootprintCenter.Y + InsetY),
		FVector2D(FootprintCenter.X + InsetX, FootprintCenter.Y + InsetY)
	};

	// 부지 자신의 클릭 볼륨과 경계 표시는 접지 권위가 아니므로 무시한다. 루프 밖 1회 구성.
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<ACityPlotActor*>(this));

	// 채널 미설정 안전장치: 5점 전부 무응답이면 마스크 불신 → 막지 않음 (콜리전 오설정으로 전면 배치 불가 회피)
	// 일부라도 응답하면 채널 정상 → 한 점이라도 블록 밖이면 무효 (깎인 모서리 걸러냄)
	bool bAnyTraceResponded = false;

	for (int32 PointIndex = 0; PointIndex < 5; ++PointIndex)
	{
		const FVector2D& Pt = Points[PointIndex];
		const FVector TraceStart(Pt.X, Pt.Y, FootprintCenter.Z + 1000.f);
		const FVector TraceEnd(Pt.X, Pt.Y, FootprintCenter.Z - 2000.f);

		TArray<FHitResult> Hits;
		const bool bAnyHit = UKismetSystemLibrary::LineTraceMulti(
			Wld, TraceStart, TraceEnd, StreetTraceType, bTraceComplex,
			ActorsToIgnore, EDrawDebugTrace::None, Hits, /*bIgnoreSelf=*/true);

		if (!bAnyHit)
		{
			// 이 점 빗나감 (깎인 모서리/바닥 없음). 채널 응답 여부는 다른 점이 결정
			OutPointsOnBlock[PointIndex] = false;
			continue;
		}

		bAnyTraceResponded = true; // 채널이 작동함(최소 한 점이 무언가에 맞음).

		// 부지 블록(BlockActor) 또는 /TheRiverwalkCity/ 메시에 맞으면 유효
		bool bPointValid = false;
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && BlockActor && HitActor == BlockActor)
			{
				bPointValid = true;
				break;
			}

			// 폴백 식별 — 히트 컴포넌트가 건축 가능 지면이면 허용(다른 블록 타일/머지 메시/City_Pack 보도 포함).
			if (const UStaticMeshComponent* HitSMC = Cast<UStaticMeshComponent>(Hit.GetComponent()))
			{
				if (IsBuildableCityGroundMesh(HitSMC->GetStaticMesh()))
				{
					bPointValid = true;
					break;
				}
			}
		}

		if (!bPointValid)
		{
			OutPointsOnBlock[PointIndex] = false; // 블록/도시 메시 밖에 맞음 → 이 점 무효.
		}
	}

	// 채널 무응답 = 미설정 → 판정 권위 없음 (호출자가 전부 가능 폴백)
	// 폴백은 배치 제약을 통째로 없애므로 반드시 로그 (모바일 제약 소실을 눈으로만 발견한 전례). 부지당 1회
	if (!bAnyTraceResponded)
	{
		if (!bLoggedNoTraceAuthority)
		{
			bLoggedNoTraceAuthority = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[CityPlot] footprint 접지 판정 권위 없음 — StreetSurface(GameTraceChannel3) 무응답. 배치 제약이 전부 해제된다. (PlotId=%s, bBlockResolved=%d)"),
				*PlotId.ToString(), bBlockResolved ? 1 : 0);
		}

		for (bool& bPoint : OutPointsOnBlock)
		{
			bPoint = true;
		}
		return false;
	}

	return true;
}

// 무효 처리 대신 유효해지는 최소 거리만 밀어 배치 유지
bool ACityPlotActor::ResolveFootprintOntoGround(const FVector& FootprintCenter, float HalfX, float HalfY,
	float MaxPush, FVector2D& OutAdjustedXY) const
{
	OutAdjustedXY = FVector2D(FootprintCenter.X, FootprintCenter.Y);

	bool PointsOnBlock[5];
	if (!EvaluateFootprintPoints(FootprintCenter, HalfX, HalfY, PointsOnBlock))
	{
		return true; // 권위 없음 → 접지로 간주(전부 가능 폴백)
	}

	bool bAllOnBlock = true;
	for (const bool bPointOnBlock : PointsOnBlock)
	{
		bAllOnBlock &= bPointOnBlock;
	}
	if (bAllOnBlock)
	{
		return true; // 이미 접지 — 상시 드래그 경로는 여기서 끝(평가 1회, 현행과 동일 비용)
	}

	if (MaxPush <= 0.f)
	{
		return false;
	}

	// 실패 코너 부호 합의 반대 = 밀 방향. 인덱스 1..4 = (-,-)(+,-)(-,+)(+,+)
	// (오른쪽 두 코너 실패 → 합 (+2,0) → 왼쪽. 한 코너면 대각선)
	static const FVector2D CornerSigns[4] = {
		FVector2D(-1.f, -1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, 1.f)
	};
	FVector2D FailSum = FVector2D::ZeroVector;
	for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
	{
		if (!PointsOnBlock[CornerIndex + 1])
		{
			FailSum += CornerSigns[CornerIndex];
		}
	}

	// 합 0 = 마주보는 코너 동시 실패 (footprint > 부지 등) → 방향 불명, 보정 포기
	if (FailSum.IsNearlyZero())
	{
		return false;
	}
	const FVector2D PushDir = FailSum.GetSafeNormal() * -1.f;

	// 최대 밀기에서도 안 되면 포기(경계에서 멀리 벗어난 의도적 이동으로 본다).
	auto IsGroundedAt = [&](float Dist) -> bool
	{
		const FVector2D Candidate = OutAdjustedXY + PushDir * Dist;
		return IsFootprintOnGround(FVector(Candidate.X, Candidate.Y, FootprintCenter.Z), HalfX, HalfY);
	};
	if (!IsGroundedAt(MaxPush))
	{
		return false;
	}

	// 이분 탐색: Lo(불가)~Hi(가능) 사이 최소 이동. 6회 = MaxPush/64 ≈ 34cm (2150 기준)
	// MaxPush를 키우면 반복수도 같이 올려야 정밀도 유지 (전엔 1075/5회로 동일 34cm)
	float Lo = 0.f;
	float Hi = MaxPush;
	for (int32 Step = 0; Step < 6; ++Step)
	{
		const float Mid = (Lo + Hi) * 0.5f;
		if (IsGroundedAt(Mid))
		{
			Hi = Mid;
		}
		else
		{
			Lo = Mid;
		}
	}

	OutAdjustedXY += PushDir * Hi;
	return true;
}

void ACityPlotActor::SetOwnedState(bool bInOwned)
{
	bOwned = bInOwned;

	SetPurchaseHighlight(false);
	if (bOwned) { ClosePurchasePanel(); }

	// 세이브 원장에 적는다 — 액터가 스트리밍 아웃돼도 소유가 남아야 한다(SpawnManager 주석 참조).
	if (bOwned)
	{
		if (USpawnManager* SpawnMgr = GetWorld() ? GetWorld()->GetSubsystem<USpawnManager>() : nullptr)
		{
			SpawnMgr->NoteOwnedPlot(PlotId);
		}
	}
}

FVector ACityPlotActor::GetBubbleAnchorPosition() const
{
	// 부지 중심 위로 살짝 띄워 인수 마커가 부지 위에 뜨도록 한다(WorkstationActorBase 미러).
	constexpr float PlotMarkerZOffset = 300.0f;
	return GetActorLocation() + FVector(0.0f, 0.0f, PlotMarkerZOffset);
}

void ACityPlotActor::OnEndInteract_Implementation(APlayerController* InstigatingPC)
{
	if (!bOwned)
	{
		TryPurchase();
	}
}

void ACityPlotActor::TryPurchase()
{
	if (!bOwned)
	{
		SetPurchaseHighlight(true);
		OpenPurchasePanel();
	}
}

bool ACityPlotActor::IsPurchaseDialogOpen() const
{
	const UPlotPurchasePanelWidget* Panel = PurchasePanel.Get();
	return Panel && Panel->IsActivated();
}

UWidget* ACityPlotActor::GetPurchaseDialogCloseButtonWidget() const
{
	return IsPurchaseDialogOpen() ? PurchasePanel->GetCloseButtonWidget() : nullptr;
}

UWidget* ACityPlotActor::GetPurchaseDialogConfirmButtonWidget() const
{
	return IsPurchaseDialogOpen() ? PurchasePanel->GetConfirmButtonWidget() : nullptr;
}

void ACityPlotActor::HoldSelectionHighlight()
{
	UWorld* CityWorld = GetWorld();
	if (!CityWorld || bOwned)
	{
		return;
	}
	if (!bPurchaseHighlighted)
	{
		SetPurchaseHighlight(true);
		return;
	}
	// 설명 중에는 「이 땅」이 한눈에 보이게 색면·선을 진하게 한다 ― 일반 선택의 연한 값(0.14/0.82)은 그대로
	if (!bTutorialHoldEmphasis && UnownedBoundarySelectedMID)
	{
		UnownedBoundarySelectedMID->SetScalarParameterValue(TEXT("FillOpacity"), 0.34f);
		UnownedBoundarySelectedMID->SetScalarParameterValue(TEXT("RimOpacity"), 1.f);
		bTutorialHoldEmphasis = true;
	}
	// 재질은 시작 뒤 9.4초부터 사라진다 ― 그 전에 이미 다 떠오른 시점(0.2초 전)으로 되감아 깜빡임 없이 이어 간다
	const float Now = CityWorld->GetTimeSeconds();
	if (Now - PurchaseHighlightStartedAt < 8.f)
	{
		return;
	}
	PurchaseHighlightStartedAt = Now - 0.2f;
	CityWorld->GetTimerManager().SetTimer(PurchaseHighlightTimer,
		FTimerDelegate::CreateUObject(this, &ACityPlotActor::SetPurchaseHighlight, false), 9.8f, false);
	if (UnownedBoundarySelectedMID)
	{
		UnownedBoundarySelectedMID->SetScalarParameterValue(TEXT("SelectionStartedAt"), PurchaseHighlightStartedAt);
	}
}

bool ACityPlotActor::CanBePurchasedNow() const
{
	if (bOwned)
	{
		return false;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return false;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	USpawnManager* SpawnMgr = GetWorld() ? GetWorld()->GetSubsystem<USpawnManager>() : nullptr;
	if (!TableMgr || !ResMgr || !SpawnMgr)
	{
		return false;
	}

	// 입주 회사가 전원 Cleared 여야 통과한다.
	if (UCityAcquisitionManager* Acq = GetWorld()->GetSubsystem<UCityAcquisitionManager>())
	{
		if (!Acq->ArePlotOccupantsCleared(PlotId))
		{
			return false;
		}
	}

	// 인접 판정은 배지 표시와 같은 SpawnManager API를 사용한다.
	if (!SpawnMgr->IsPlotAdjacentBuyable(this))
	{
		return false;
	}

	// 자금 충분 여부는 DT 가격으로 판정한다(MoneyPrice==0은 항상 통과).
	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		return false;
	}

	return PlotData.MoneyPrice <= 0 || ResMgr->GetResourceAmount(EResourceType::Money) >= PlotData.MoneyPrice;
}

void ACityPlotActor::OpenPurchasePanel()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	if (!TableMgr || !UIBase || bOwned || UIBase->GetPromptStackCount() > 0)
	{
		return;
	}
	if (PurchasePanel.IsValid())
	{
		RefreshPurchasePanel();
		return;
	}
	UCommonActivatableWidget* ActiveBottom = UIBase->GetActiveBottomWidget();
	if (ActiveBottom && !ActiveBottom->IsA<UBuildOpenWidget>() && !ActiveBottom->IsA<UPlotPurchasePanelWidget>())
	{
		return;
	}

	TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::PlotPurchasePanel);
	UPlotPurchasePanelWidget* Panel = WidgetClass
		? Cast<UPlotPurchasePanelWidget>(UIBase->PushBottomClass(WidgetClass.Get())) : nullptr;
	if (!Panel)
	{
		UE_LOG(LogTemp, Error, TEXT("[CityPlot] PlotPurchasePanel widget unavailable (PlotId=%s)"), *PlotId.ToString());
		return;
	}

	// 하단 스택에서 재사용될 때 구매 대상 콜백을 교체한다.
	Panel->OnConfirm.Clear();
	Panel->OnConfirm.AddUObject(this, &ACityPlotActor::HandlePurchaseConfirmed);
	PurchasePanel = Panel;
	PurchasePanelClosedHandle = Panel->OnDeactivated().AddUObject(this, &ACityPlotActor::HandlePurchasePanelClosed);
	if (UResourceItemManager* Resources = GI->GetSubsystem<UResourceItemManager>())
	{
		PurchaseResourceChangedHandle = Resources->OnResourceChanged.AddWeakLambda(this,
			[this](EResourceType Type, int64)
			{
				if (Type == EResourceType::Money) { RefreshPurchasePanel(); }
			});
	}
	if (UCityAcquisitionManager* Acquisition = GetWorld()->GetSubsystem<UCityAcquisitionManager>())
	{
		PurchaseCompanyClearedHandle = Acquisition->OnCompanyCleared.AddWeakLambda(this,
			[this](int32) { RefreshPurchasePanel(); });
	}
	RefreshPurchasePanel();
}

void ACityPlotActor::RefreshPurchasePanel()
{
	UPlotPurchasePanelWidget* Panel = PurchasePanel.Get();
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UResourceItemManager* Resources = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	USpawnManager* SpawnMgr = GetWorld() ? GetWorld()->GetSubsystem<USpawnManager>() : nullptr;
	if (!Panel || !TableMgr || !Resources || !SpawnMgr) { return; }
	if (bOwned) { ClosePurchasePanel(); return; }

	bool bFound = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bFound);
	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityPlot] purchase panel has no DT row for %s"), *PlotId.ToString());
		ClosePurchasePanel();
		return;
	}
	const UCityAcquisitionManager* Acquisition = GetWorld()->GetSubsystem<UCityAcquisitionManager>();
	const bool bCleared = !Acquisition || Acquisition->ArePlotOccupantsCleared(PlotId);
	const bool bAdjacent = SpawnMgr->IsPlotAdjacentBuyable(this);
	const bool bCanAfford = PlotData.MoneyPrice <= 0
		|| Resources->GetResourceAmount(EResourceType::Money) >= PlotData.MoneyPrice;
	const bool bCanPurchase = bCleared && bAdjacent && bCanAfford;
	FText Headline;
	FText Message;
	if (!bCleared)
	{
		Headline = NSLOCTEXT("CityPlot", "PanelOccupantsHeadline", "회사 정리가 필요해요");
		Message = NSLOCTEXT("CityPlot", "PanelOccupantsRequired", "건물을 눌러 인수·정리를 진행하세요");
	}
	else if (!bAdjacent)
	{
		Headline = NSLOCTEXT("CityPlot", "PanelAdjacentHeadline", "인접 부지부터 인수해요");
		Message = NSLOCTEXT("CityPlot", "PanelAdjacentRequired", "소유한 부지 옆으로 확장할 수 있어요");
	}
	else if (!bCanAfford)
	{
		Headline = NSLOCTEXT("CityPlot", "PanelFundsHeadline", "인수 자금이 부족해요");
		Message = NSLOCTEXT("CityPlot", "PanelFundsRequired", "필요한 자금을 모으면 인수할 수 있어요");
	}
	else
	{
		Headline = NSLOCTEXT("CityPlot", "PanelReadyHeadline", "새로운 부지를 인수해요");
		Message = NSLOCTEXT("CityPlot", "PanelReady", "인수 후 이곳에 건물을 지을 수 있어요");
	}
	Panel->SetPurchaseDetails(Headline, Message, PlotData.MoneyPrice, bCanAfford, bCanPurchase);
}

void ACityPlotActor::ClosePurchasePanel()
{
	if (UPlotPurchasePanelWidget* Panel = PurchasePanel.Get())
	{
		Panel->OnConfirm.RemoveAll(this);
		Panel->DeactivateWidget();
	}
	HandlePurchasePanelClosed();
}

void ACityPlotActor::HandlePurchasePanelClosed()
{
	if (UPlotPurchasePanelWidget* Panel = PurchasePanel.Get())
	{
		Panel->OnDeactivated().Remove(PurchasePanelClosedHandle);
	}
	PurchasePanel.Reset();
	PurchasePanelClosedHandle.Reset();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* Resources = GI->GetSubsystem<UResourceItemManager>())
		{
			Resources->OnResourceChanged.Remove(PurchaseResourceChangedHandle);
		}
	}
	if (UWorld* CityWorld = GetWorld())
	{
		if (UCityAcquisitionManager* Acquisition = CityWorld->GetSubsystem<UCityAcquisitionManager>())
		{
			Acquisition->OnCompanyCleared.Remove(PurchaseCompanyClearedHandle);
		}
	}
	PurchaseResourceChangedHandle.Reset();
	PurchaseCompanyClearedHandle.Reset();
	// 확인 버튼은 비활성화 후 OnConfirm을 보내므로 여기서는 구매 델리게이트를 지우지 않는다.
}

void ACityPlotActor::SetPurchaseHighlight(bool bVisible)
{
	// 튜토리얼 설명용으로 진하게 한 값은 일반 선택(확정된 연한 색면)으로 넘기지 않는다
	if (bTutorialHoldEmphasis && UnownedBoundarySelectedMID)
	{
		UnownedBoundarySelectedMID->ClearParameterValues();
	}
	bTutorialHoldEmphasis = false;
	UWorld* CityWorld = GetWorld();
	if (bVisible && !bOwned && CityWorld)
	{
		for (TActorIterator<ACityPlotActor> It(CityWorld); It; ++It)
		{
			ACityPlotActor* OtherPlot = *It;
			if (OtherPlot != this)
			{
				OtherPlot->SetPurchaseHighlight(false);
				OtherPlot->ClosePurchasePanel();
			}
		}
		bPurchaseHighlighted = true;
		PurchaseHighlightStartedAt = CityWorld->GetTimeSeconds();
		CityWorld->GetTimerManager().SetTimer(PurchaseHighlightTimer,
			FTimerDelegate::CreateUObject(this, &ACityPlotActor::SetPurchaseHighlight, false), 10.f, false);
		if (!bUnownedBoundaryBuilt)
		{
			BoundaryRetryCount = 0;
			BuildUnownedBoundaryMesh();
		}
		if (UnownedBoundarySelectedMID)
		{
			UnownedBoundarySelectedMID->SetScalarParameterValue(TEXT("SelectionStartedAt"), PurchaseHighlightStartedAt);
		}
	}
	else
	{
		bPurchaseHighlighted = false;
		if (CityWorld)
		{
			CityWorld->GetTimerManager().ClearTimer(PurchaseHighlightTimer);
			CityWorld->GetTimerManager().ClearTimer(BoundaryRetryTimer);
		}
	}
	if (UnownedBoundaryMesh)
	{
		UnownedBoundaryMesh->SetVisibility(bPurchaseHighlighted && !bOwned);
	}
}

void ACityPlotActor::HandlePurchaseConfirmed()
{
	// 모달이 이미 닫히는 중 다시 클릭 등으로 중복 호출돼도 안전하도록 소유 가드
	if (bOwned)
	{
		return;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !ResMgr)
	{
		return;
	}

	if (!CanBePurchasedNow())
	{
		if (UIManager)
		{
			UIManager->ShowNotification(NSLOCTEXT("CityPlot", "PurchaseConditionsChanged",
				"인수 조건이 변경되었습니다. 부지를 다시 선택해 주세요."), 3.0f, ENotificationType::Warning);
		}
		return;
	}

	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		return;
	}

	// MoneyPrice == 0 (시작 부지 등) 가드 — 0원 결제 토스트/소비 생략하고 바로 소유 전환
	if (PlotData.MoneyPrice > 0)
	{
		FConstructionCost Cost;
		Cost.ResourceType = EResourceType::Money;
		Cost.Cost = PlotData.MoneyPrice;

		TArray<FConstructionCost> Costs;
		Costs.Add(Cost);

		EResourceType MissingType = EResourceType::None;
		if (!ResMgr->CanAffordCosts(Costs, MissingType))
		{
			if (UIManager)
			{
				UIManager->ShowNotification(
					NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
			}
			return;
		}

		// bShouldSave=false: 소유 전환(SetOwnedState) 전에 세이브되면 새 부지가 OwnedPlotIds 에서 누락된다.
		// 차감→소유전환→단일 세이브 순서로 원자성 확보(중간 크래시 시 돈만 빠지는 창 제거).
		ResMgr->SpendResource(EResourceType::Money, PlotData.MoneyPrice, /*bShouldSave=*/false);
	}

	// 소유 전환 — 세이브 직렬화(OwnedPlotIds)는 SpawnedPlots.IsOwned() 순회를 단일 소스로 함
	SetOwnedState(true);

	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}

	// 미션판 G11 — 결제가 실제로 끝난 이 지점에서만 신호(모달 확인 시점은 취소 여지가 남는다)
	if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyPlotAcquired();
	}

	// 인수 성공 → 가격 배지 즉시 재평가(방금 산 부지 배지 제거 + 새로 인접해진 미소유 부지에 배지 — 바깥으로 번짐).
	if (UIManager)
	{
		if (UInGameLayerWidget* InGameLayer = UIManager->GetInGameLayer())
		{
			InGameLayer->RefreshPlotPriceBadges();
		}
	}

	if (UIManager)
	{
		UIManager->ShowNotification(
			FText::FromString(TEXT("부지를 인수했습니다")), 3.0f, ENotificationType::Success);
	}
}
