// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/SpawnManager.h"

#include "NavigationSystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/CGGameInstance.h"
#include "Table/InteractableInfo.h"
#include "Manager/TableManagerSubsystem.h"
//#include "Data/SpawnerInfo.h"
#include "Engine/StreamableManager.h"
#include "Manager/EntityManager.h"
#include "Entity/InteractableBaseActor.h"
#include "Entity/Building/BuildingBaseActor.h"
//#include "Entity/Interactable/Crop/CropBaseActor.h"
#include "Util/CoordinateUtils.h"
#include "Entity/Plot/CityPlotActor.h"
#include "Entity/Ambient/VacantPlotDressingManager.h"
#include "Table/CityPlotData.h"

namespace
{
	// 인접 판정 = 두 부지 격자 사각형의 경계 간 간극(cm). 중심간 거리로 재면 부지 크기가
	// 제각각(3x2 ~ 6x10)일 때 큰 부지가 이웃에서 떨어져 나가 도달 불가가 된다.
	// 5,000 = 실제 최대 대각 이웃 간극 4,040(Plot_09↔Plot_10) 위, 2블록 밖 15,000대 아래.
	// 값을 바꾸면 Tools/Balance/plot_adjacency_check.py 로 고아/도달불가 부지를 재검산할 것.
	constexpr float PlotAdjacencyGap = 5000.f;

	// 두 부지 사각형(중심 + XY extent)의 경계 간 간극. 겹치면 0.
	float PlotRectGap(const FVector& CenterA, const FVector2D& ExtentA,
		const FVector& CenterB, const FVector2D& ExtentB)
	{
		const float GapX = FMath::Max(0.f, FMath::Abs(CenterA.X - CenterB.X) - (ExtentA.X + ExtentB.X));
		const float GapY = FMath::Max(0.f, FMath::Abs(CenterA.Y - CenterB.Y) - (ExtentA.Y + ExtentB.Y));
		return FMath::Sqrt(GapX * GapX + GapY * GapY);
	}
}

USpawnManager::USpawnManager()
{
}

bool USpawnManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// ���� �θ� Ŭ������ ������ ����ϴ��� Ȯ��
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// Outer�� UWorld�� ĳ�����Ͽ� ���� ���� ��������
	UWorld* World = Cast<UWorld>(Outer);

	// �� ����ý����� ���� �÷��� ���忡���� �ʿ�
	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return false;
	}

	TArray<FString> AllowedLevelNames = {
		TEXT("MainMap"),
		TEXT("MainMap_TheRiverwalkCity")
	};

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	if (AllowedLevelNames.Contains(CurrentLevelName))
	{
		// ���ԵǾ� �ִٸ� ����ý��� ����
		UE_LOG(LogTemp, Log, TEXT("USpawnManager will be created for level: %s"), 
			*CurrentLevelName);
		return true;
	}

	// �ƴϸ� ��������
	UE_LOG(LogTemp, Log, TEXT("USpawnManager will NOT be created for level: %s"), 
		*CurrentLevelName);
	return false;
}

void USpawnManager::Initialize(FSubsystemCollectionBase& Collection)
{
}

void USpawnManager::Deinitialize()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(CheckNavMeshAndAsyncLoadTimerHandle);
	}
}

void USpawnManager::OnWorldBeginPlay(UWorld& InWorld)
{
	GameInstance = UCGGameInstance::GetInstance();
	TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();

	// 도시 부지 스폰 — MainMap 전용(USpawnManager 자체가 MainMap 에서만 생성됨).
	// 세이브 로드보다 먼저 실행되므로 여기선 스폰 + bOwnedAtStart 만 반영하고,
	// 로드된 OwnedPlotIds 소유 복원은 SaveLoadManager 가 RestorePlotOwnership 으로 처리한다.
	SpawnCityPlots();
}


AInteractableBaseActor* USpawnManager::SpawnBuilding(const FInteractableInfo& InteractableInfo,
	const FBuildingEntitySaveData* LoadData)
{
	UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] SpawnBuilding called - Name: %s"),
		*InteractableInfo.Name.ToString());

	ABuildingBaseActor* SpawnedBuilding = Cast<ABuildingBaseActor>(
		GetWorld()->SpawnActor(ABuildingBaseActor::StaticClass()));

	UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] Building spawned: %p"), SpawnedBuilding);

	if (SpawnedBuilding != nullptr && LoadData)
	{
		SpawnedBuilding->InitializeFromSaveData(InteractableInfo, *LoadData);
	}

	return SpawnedBuilding;
}

AInteractableBaseActor* USpawnManager::SpawnNewBuilding(const FInteractableInfo& InteractableInfo, bool bIsNewlyPlaced)
{
	UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] SpawnNewBuilding called - Name: %s, NewlyPlaced: %d"),
		*InteractableInfo.Name.ToString(), bIsNewlyPlaced);

	ABuildingBaseActor* SpawnedBuilding = Cast<ABuildingBaseActor>(
		GetWorld()->SpawnActor(ABuildingBaseActor::StaticClass()));

	UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] New Building spawned: %p"), SpawnedBuilding);

	if (SpawnedBuilding != nullptr)
	{
		// 새 건물 생성
		SpawnedBuilding->SetInteractableInfo(InteractableInfo);

		// bIsNewlyPlaced 플래그를 태그로 저장 (PlacementHandler에서 VFX 호출 위해)
		if (bIsNewlyPlaced)
		{
			SpawnedBuilding->Tags.Add(FName("NewlyPlaced"));
		}
	}

	return SpawnedBuilding;
}

void USpawnManager::ClearSpawnerTimer()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(CheckNavMeshAndAsyncLoadTimerHandle);
	}
}

void USpawnManager::SpawnCityPlots()
{
	// 중복 스폰 방지 — OnWorldBeginPlay 는 월드당 1회지만 가드로 안전성 확보
	if (bPlotsSpawned)
	{
		return;
	}

	UWorld* SpawnWorld = GetWorld();
	if (!SpawnWorld)
	{
		return;
	}

	if (!TableManager)
	{
		// OnWorldBeginPlay 에서 캐시되지만 방어적으로 재조회
		if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
		{
			TableManager = GI->GetSubsystem<UTableManagerSubsystem>();
		}
	}
	if (!TableManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] SpawnCityPlots: TableManager null"));
		return;
	}

	TArray<FName> PlotIds;
	TableManager->GetAllCityPlotRows(PlotIds);
	if (PlotIds.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SpawnManager] SpawnCityPlots: no DT_CityPlot rows"));
		bPlotsSpawned = true;
		return;
	}

	for (const FName& PlotId : PlotIds)
	{
		bool bOk = false;
		const FCityPlotData PlotData = TableManager->GetCityPlotData(PlotId, bOk);
		if (!bOk)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] SpawnCityPlots: missing DT row for %s"), *PlotId.ToString());
			continue;
		}

		// 회전: 도시 블록 방향 정렬은 Task 6(에디터 실측)에서 확정. 지금은 식별 회전 사용.
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ACityPlotActor* Plot = SpawnWorld->SpawnActor<ACityPlotActor>(
			ACityPlotActor::StaticClass(), PlotData.Center, FRotator::ZeroRotator, SpawnParams);

		if (!Plot)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] SpawnCityPlots: SpawnActor failed for %s"), *PlotId.ToString());
			continue;
		}

		// Init 이 DT 조회 → 격자 기반 Extent/Box 산출 + bOwnedAtStart 초기 소유 반영
		Plot->Init(PlotId);

		// bOwnedAtStart 부지는 즉시 소유 상태로 (신규 게임의 시작 부지). 로드 게임은 RestorePlotOwnership 가 갱신.
		if (PlotData.bOwnedAtStart)
		{
			Plot->SetOwnedState(true);
		}

		SpawnedPlots.Add(PlotId, Plot);
	}

	bPlotsSpawned = true;

	if (!IsValid(VacantPlotDressingManager))
	{
		FActorSpawnParameters DressingSpawnParams;
		DressingSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		VacantPlotDressingManager = SpawnWorld->SpawnActor<AVacantPlotDressingManager>(
			AVacantPlotDressingManager::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			DressingSpawnParams);
		if (!VacantPlotDressingManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] VacantPlotDressingManager spawn failed"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SpawnManager] SpawnCityPlots: spawned %d plots"), SpawnedPlots.Num());
}

void USpawnManager::GatherOwnedPlotIds(TArray<FName>& OutOwnedPlotIds) const
{
	OutOwnedPlotIds.Reset();
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		if (Pair.Value && Pair.Value->IsOwned())
		{
			OutOwnedPlotIds.Add(Pair.Key);
		}
	}
}

int32 USpawnManager::DebugOwnAllPlots()
{
	int32 Count = 0;
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		if (IsValid(Pair.Value) && !Pair.Value->IsOwned())
		{
			Pair.Value->SetOwnedState(true);
			++Count;
		}
	}
	return Count;
}

ACityPlotActor* USpawnManager::GetOwnedPlotAt(const FVector& WorldPos) const
{
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		ACityPlotActor* Plot = Pair.Value;
		if (!IsValid(Plot) || !Plot->IsOwned())
		{
			continue;
		}

		// 부지는 ZeroRotator 축정렬(설계 §11.3) → 월드 XY AABB 검사. Extent = 격자 반경.
		const FVector Center = Plot->GetCenter();
		const FVector2D Extent = Plot->GetExtent();
		if (FMath::Abs(WorldPos.X - Center.X) <= Extent.X &&
			FMath::Abs(WorldPos.Y - Center.Y) <= Extent.Y)
		{
			return Plot;
		}
	}
	return nullptr;
}

ACityPlotActor* USpawnManager::GetSpawnedPlotById(FName InPlotId) const
{
	if (ACityPlotActor* const* Found = SpawnedPlots.Find(InPlotId))
	{
		return *Found;
	}
	return nullptr;
}

void USpawnManager::GetAdjacentUnownedPlots(TArray<ACityPlotActor*>& OutAdjacentPlots) const
{
	OutAdjacentPlots.Reset();

	// 소유 부지의 (중심, extent) 를 먼저 수집(매 미소유 부지 × 매 소유 부지 검사 — 부지 ~40개라 충분).
	TArray<TPair<FVector, FVector2D>> OwnedRects;
	OwnedRects.Reserve(SpawnedPlots.Num());
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		ACityPlotActor* Plot = Pair.Value;
		if (IsValid(Plot) && Plot->IsOwned())
		{
			OwnedRects.Emplace(Plot->GetCenter(), Plot->GetExtent());
		}
	}

	if (OwnedRects.Num() == 0)
	{
		return;
	}

	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		ACityPlotActor* Plot = Pair.Value;
		if (!IsValid(Plot) || Plot->IsOwned())
		{
			continue;
		}

		const FVector PlotCenter = Plot->GetCenter();
		const FVector2D PlotExtent = Plot->GetExtent();
		for (const TPair<FVector, FVector2D>& OwnedRect : OwnedRects)
		{
			// 수직(Z) 편차는 무시하고 평면(XY) 간극으로 판정(부지는 같은 지면 평면에 정렬).
			if (PlotRectGap(PlotCenter, PlotExtent, OwnedRect.Key, OwnedRect.Value) <= PlotAdjacencyGap)
			{
				OutAdjacentPlots.Add(Plot);
				break;
			}
		}
	}
}

bool USpawnManager::IsPlotAdjacentBuyable(const ACityPlotActor* Plot) const
{
	if (!IsValid(Plot) || Plot->IsOwned())
	{
		return false;
	}

	const FVector PlotCenter = Plot->GetCenter();
	const FVector2D PlotExtent = Plot->GetExtent();

	// 소유 부지 중 하나라도 간극 안이면 인수 가능(GetAdjacentUnownedPlots 와 동일 정의 — 단일 출처).
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		const ACityPlotActor* Other = Pair.Value;
		if (IsValid(Other) && Other->IsOwned())
		{
			if (PlotRectGap(PlotCenter, PlotExtent, Other->GetCenter(), Other->GetExtent()) <= PlotAdjacencyGap)
			{
				return true;
			}
		}
	}
	return false;
}

void USpawnManager::RestorePlotOwnership(const TArray<FName>& OwnedPlotIds)
{
	if (OwnedPlotIds.Num() == 0)
	{
		return;
	}

	for (const FName& PlotId : OwnedPlotIds)
	{
		if (ACityPlotActor* const* FoundPlot = SpawnedPlots.Find(PlotId))
		{
			if (ACityPlotActor* Plot = *FoundPlot)
			{
				Plot->SetOwnedState(true);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] RestorePlotOwnership: owned plot %s not spawned"), *PlotId.ToString());
		}
	}
}

bool USpawnManager::FindFreeFootprintOnPlot(FName InPlotId, FVector2D Footprint, FTransform& OutTransform,
	const TArray<FVector>& AlreadyPlanned) const
{
#if !UE_BUILD_SHIPPING
	ACityPlotActor* Plot = GetSpawnedPlotById(InPlotId);
	if (!Plot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] FindFreeFootprint: 부지 미스폰 %s"), *InPlotId.ToString());
		return false;
	}

	const float HalfX = Footprint.X * 0.5f;
	const float HalfY = Footprint.Y * 0.5f;
	const FVector Center = Plot->GetCenter();
	const FVector2D Extent = Plot->GetExtent();

	TArray<ABuildingBaseActor*> Existing;
	if (const UWorld* W = GetWorld())
	{
		if (UEntityManager* EntityMgr = W->GetSubsystem<UEntityManager>())
		{
			Existing = EntityMgr->GetBuildingsOnPlot(InPlotId);
		}
	}

	// 격자 칸을 전부 열거한다. 이전엔 OffX 바깥/OffY 안쪽 루프의 first-fit 이라
	// 한 부지의 건물들이 X=0 에 고정된 채 Y 로만 늘어서 한 줄이 됐다.
	const float StepX = FMath::Max(Footprint.X, 100.0f);
	const float StepY = FMath::Max(Footprint.Y, 100.0f);
	const int32 CellsX = FMath::Max(1, FMath::FloorToInt((Extent.X * 2.0f) / StepX));
	const int32 CellsY = FMath::Max(1, FMath::FloorToInt((Extent.Y * 2.0f) / StepY));

	// 격자를 부지 중심 기준으로 대칭 배치하되 안쪽으로 오므린다.
	// 오므리지 않으면 가장자리 칸의 건물 끝이 부지 경계와 정확히 일치해(칸 4300 = 건물 4300)
	// ResolveFootprintOntoGround 의 접지 트레이스가 인도/깎인 모서리로 판정해 대량 탈락한다.
	constexpr float InsetFactor = 0.84f;

	TArray<FVector> Cells;
	Cells.Reserve(CellsX * CellsY);
	for (int32 Cx = 0; Cx < CellsX; ++Cx)
	{
		for (int32 Cy = 0; Cy < CellsY; ++Cy)
		{
			const float OffX = (Cx - (CellsX - 1) * 0.5f) * StepX * InsetFactor;
			const float OffY = (Cy - (CellsY - 1) * 0.5f) * StepY * InsetFactor;
			Cells.Add(FVector(Center.X + OffX, Center.Y + OffY, Center.Z));
		}
	}

	// 칸 개수와 서로소인 stride 로 훑어 흩뿌린다 — 순서대로 채우면 또 한 줄이 된다.
	// PlotId 해시를 시작점으로 써서 부지마다 패턴이 달라지되, 같은 부지는 항상 같은 결과(R2 재현성).
	const int32 N = Cells.Num();
	int32 Stride = 5;
	while (Stride > 1 && FMath::GreatestCommonDivisor(Stride, N) != 1)
	{
		--Stride;
	}
	// GetTypeHash 는 uint32 — FMath::Abs 를 씌우면 C4146(부호없는 타입에 단항 마이너스)로 빌드가 깨진다
	const int32 Start = static_cast<int32>(GetTypeHash(InPlotId) % static_cast<uint32>(N));

	for (int32 Step = 0; Step < N; ++Step)
	{
		const int32 CellIdx = (Start + Step * Stride) % N;
		FVector Candidate = Cells[CellIdx];

		// 겹침 판정 — PlacementHandler::ComputePlotPlacement(private)의 AABB 검사를 축소 재현.
		// 계수 1.0 이면 인접 칸 중심거리가 정확히 Footprint 라 건물이 모서리를 맞대 한 덩어리로 보이고,
		// 1.5 로 올리면 대각 칸까지 걸러져 3x3 부지가 네 모서리 4칸만 남는데 그 칸들은 부지 경계에 붙어
		// 접지 트레이스에서 자주 탈락한다(배치 실패 급증). 1.05 = 축이 맞닿는 인접만 막고 대각은 허용.
		constexpr float SpacingFactor = 1.5f;
		auto OverlapsAt = [&Candidate, &Footprint](const FVector& OtherLoc)
		{
			return FMath::Abs(OtherLoc.X - Candidate.X) < Footprint.X * SpacingFactor
				&& FMath::Abs(OtherLoc.Y - Candidate.Y) < Footprint.Y * SpacingFactor;
		};

		bool bOverlap = false;
		for (ABuildingBaseActor* Other : Existing)
		{
			if (Other && OverlapsAt(Other->GetActorLocation()))
			{
				bOverlap = true;
				break;
			}
		}
		// 아직 스폰 전인 예정 위치도 같이 피한다 (일괄 스폰 시더용)
		if (!bOverlap)
		{
			for (const FVector& Planned : AlreadyPlanned)
			{
				if (OverlapsAt(Planned))
				{
					bOverlap = true;
					break;
				}
			}
		}
		if (bOverlap)
		{
			continue;
		}

		FVector2D Adjusted(Candidate.X, Candidate.Y);
		if (!Plot->ResolveFootprintOntoGround(Candidate, HalfX, HalfY, StepX, Adjusted))
		{
			continue;
		}
		Candidate.X = Adjusted.X;
		Candidate.Y = Adjusted.Y;

		// 90도 단위 yaw — 정사각 footprint 면 AABB 가 그대로라 겹침 판정에 영향이 없고,
		// 전부 같은 방향으로 서 있는 '복사붙여넣기' 느낌만 걷어낸다. 비정방은 회전 금지.
		const bool bSquare = FMath::IsNearlyEqual(Footprint.X, Footprint.Y);
		const float Yaw = bSquare ? 90.0f * static_cast<float>((CellIdx + Start) % 4) : 0.0f;

		// Scale 은 손대지 않는다 — 오토핏은 '땅에 박힘' 회귀로 의도적 제거됨
		OutTransform = FTransform(FRotator(0.0f, Yaw, 0.0f), Candidate, FVector::OneVector);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] FindFreeFootprint: 빈 자리 없음 %s"), *InPlotId.ToString());
	return false;
#else
	return false;
#endif
}
