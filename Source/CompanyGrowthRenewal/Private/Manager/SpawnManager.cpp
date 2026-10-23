// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/SpawnManager.h"

#include "NavigationSystem.h"
#include "EngineUtils.h" // TActorIterator (레벨 배치 부지 수집)
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
#include "Entity/Plot/CityPlotWalkwayManager.h"
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

	// 레벨 배치 부지는 인접 게이트를 면제한다 — 되돌릴 때 여기 한 곳만 false 로.
	// 리버워크 39부지는 블록이 촘촘해(최대 대각 간극 4,040) 간극 판정이 성립하지만, 손으로 놓는 부지는
	// 도로를 사이에 두고 5,000 을 넘기 쉽고 그러면 시작 부지 다음부터 전부 도달 불가가 된다.
	// 부지가 소수일 때 확장 병목은 가격 사다리가 맡는다(회사 인수 게이트도 같은 이유로 2026-06-28 제거).
	constexpr bool bAuthoredPlotsBypassAdjacency = true;

	bool IsAdjacencyExempt(const ACityPlotActor* Plot)
	{
		return bAuthoredPlotsBypassAdjacency && Plot && Plot->IsAuthoredInLevel();
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
		TEXT("MainMap_City"),
		TEXT("MainMap_Manhattan"),
		TEXT("MainMap_TheRiverwalkCity"),
		TEXT("Demonstration_WP_Office")
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

	// 도시 부지 확보 — 두 경로가 상호 배타다. DT 좌표 스폰(리버워크)을 먼저 하고,
	// 그다음 레벨에 손으로 배치된 부지를 등록한다(City_Pack 계열 맵).
	// 세이브 로드보다 먼저 실행되므로 여기선 확보 + bOwnedAtStart 만 반영하고,
	// 로드된 OwnedPlotIds 소유 복원은 SaveLoadManager 가 RestorePlotOwnership 으로 처리한다.
	SpawnCityPlots();
	RegisterLevelPlacedPlots();
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
	// DT_CityPlot 의 Center 는 TheRiverwalkCity 좌표 전용이다. 다른 도시 맵에서 스폰하면 부지와 가격 배지가
	// 허공에 뜬다 — 그 맵들은 RegisterLevelPlacedPlots 로 레벨에 배치된 부지를 쓴다.
	if (GetWorld() && !GetWorld()->GetMapName().Contains(TEXT("TheRiverwalkCity")))
	{
		UE_LOG(LogTemp, Log, TEXT("[SpawnManager] SpawnCityPlots: DT 좌표 스폰은 리버워크 전용 — 레벨 배치 부지로 진행"));
		return;
	}

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

		// 레벨 배치용 행은 Center 가 비어 있다 — 스폰하면 원점 허공에 부지가 뜬다.
		// 그 행의 액터는 RegisterLevelPlacedPlots 가 레벨에서 찾아 등록한다.
		if (PlotData.bPlacedInLevel)
		{
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

	EnsureVacantPlotDressingManager();

	UE_LOG(LogTemp, Log, TEXT("[SpawnManager] SpawnCityPlots: spawned %d plots"), SpawnedPlots.Num());
}

bool USpawnManager::RegisterPlacedPlot(ACityPlotActor* Plot)
{
	// ⚠ 이 함수는 **부지가 스스로** 부른다(ACityPlotActor::BeginPlay). 한 번 훑는 방식은 쓰지 말 것 —
	// World Partition 맵에서는 OnWorldBeginPlay 시점에 부지 액터 셀이 아직 스트리밍되지 않아
	// TActorIterator 가 0 개를 반환한다. 그러면 등록이 비고 bOwnedAtStart 가 적용되지 않아
	// "먼저 부지를 인수하세요" 로 건설이 전부 막힌다(실행마다 결과가 달라지는 타이밍 버그였다).
	if (!IsValid(Plot))
	{
		return false;
	}

	if (!TableManager)
	{
		if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
		{
			TableManager = GI->GetSubsystem<UTableManagerSubsystem>();
		}
	}

	const FName PlacedId = Plot->GetPlotId();
	if (PlacedId.IsNone())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[SpawnManager] 배치된 부지 '%s' 에 PlotId 가 비어 있다 — Details 에서 DT_CityPlot 행 이름을 지정할 것"),
			*Plot->GetName());
		return false;
	}
	if (!Plot->IsAuthoredInLevel())
	{
		// OnWorldBeginPlay 선등록은 부지 BeginPlay보다 빠를 수 있으므로 저작 경계를 먼저 확정한다.
		// DT 스폰 부지는 InitFromAuthoredBounds 내부에서 제외된다.
		Plot->InitFromAuthoredBounds();
	}

	if (ACityPlotActor** Existing = SpawnedPlots.Find(PlacedId))
	{
		if (*Existing == Plot)
		{
			return false; // 이미 등록됨 — 재호출은 무해(멱등)
		}
		// 같은 PlotId 가 둘 다 살아 있으면 세이브의 OwnedPlotIds 가 어느 쪽인지 못 가려 소유가 엉킨다.
		if (IsValid(*Existing))
		{
			UE_LOG(LogTemp, Error,
				TEXT("[SpawnManager] PlotId '%s' 중복 — '%s' 는 등록하지 않는다. 배치마다 고유 PlotId 를 줄 것"),
				*PlacedId.ToString(), *Plot->GetName());
			return false;
		}
		// 셀이 언로드되며 죽은 엔트리다. UPROPERTY TMap 은 **키를 지우지 않고 값만 null 로** 만들므로
		// Find 는 계속 성공한다 — 이걸 중복으로 읽으면 다시 스트리밍 인 된 부지가 영구히 미등록이 되고,
		// 수용량 조회·표면 Z·원장 소유 복원이 전부 죽은 포인터/널을 보게 된다. 돌아온 액터로 덮는다.
		SpawnedPlots.Remove(PlacedId);
	}

	if (TableManager)
	{
		bool bOk = false;
		const FCityPlotData PlotData = TableManager->GetCityPlotData(PlacedId, bOk);
		if (!bOk)
		{
			// 가격·시작소유는 DT 가 권위다. 행이 없으면 가격 0 으로 아무나 가져가므로 반드시 보이게 할 것.
			UE_LOG(LogTemp, Error,
				TEXT("[SpawnManager] 배치된 부지 PlotId '%s' 에 해당하는 DT_CityPlot 행이 없다 — 가격/시작소유 미설정"),
				*PlacedId.ToString());
		}
		else if (PlotData.bOwnedAtStart)
		{
			Plot->SetOwnedState(true);
		}
	}

	// 세이브 로드가 이미 지나간 뒤에 스트리밍된 부지 — 원장에서 소유를 되살린다.
	if (OwnedPlotIdLedger.Contains(PlacedId))
	{
		Plot->SetOwnedState(true);
	}

	SpawnedPlots.Add(PlacedId, Plot);
	UE_LOG(LogTemp, Log, TEXT("[SpawnManager] 부지 등록 %s (총 %d)"), *PlacedId.ToString(), SpawnedPlots.Num());

	if (Plot->IsAuthoredInLevel())
	{
		EnsurePlotWalkwayManager();
		if (IsValid(PlotWalkwayManager))
		{
			PlotWalkwayManager->RefreshPlot(PlacedId);
		}
	}
	EnsureVacantPlotDressingManager();
	if (IsValid(VacantPlotDressingManager))
	{
		VacantPlotDressingManager->RefreshPlot(PlacedId);
	}
	return true;
}

void USpawnManager::RegisterLevelPlacedPlots()
{
	// begin play 시점에 이미 스트리밍된 부지를 먼저 받는다. 늦게 들어오는 부지는
	// 각자 BeginPlay 에서 RegisterPlacedPlot 을 불러 스스로 등록한다(스트리밍 순서 무관).
	UWorld* PlotWorld = GetWorld();
	if (!PlotWorld)
	{
		return;
	}

	int32 RegisteredCount = 0;
	for (TActorIterator<ACityPlotActor> It(PlotWorld); It; ++It)
	{
		if (RegisterPlacedPlot(*It))
		{
			++RegisteredCount;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[SpawnManager] RegisterLevelPlacedPlots: 선등록 %d (부지 총 %d) — 나머지는 스트리밍 시 자가 등록"),
		RegisteredCount, SpawnedPlots.Num());
}

void USpawnManager::EnsureVacantPlotDressingManager()
{
	UWorld* PlotWorld = GetWorld();
	if (PlotWorld && !IsValid(VacantPlotDressingManager))
	{
		FActorSpawnParameters DressingSpawnParams;
		DressingSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		VacantPlotDressingManager = PlotWorld->SpawnActor<AVacantPlotDressingManager>(
			AVacantPlotDressingManager::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			DressingSpawnParams);
		if (!VacantPlotDressingManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] VacantPlotDressingManager spawn failed"));
		}
	}
}

void USpawnManager::EnsurePlotWalkwayManager()
{
	UWorld* PlotWorld = GetWorld();
	if (PlotWorld && !IsValid(PlotWalkwayManager))
	{
		FActorSpawnParameters WalkwaySpawnParams;
		WalkwaySpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlotWalkwayManager = PlotWorld->SpawnActor<ACityPlotWalkwayManager>(
			ACityPlotWalkwayManager::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			WalkwaySpawnParams);
		if (!PlotWalkwayManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SpawnManager] CityPlotWalkwayManager spawn failed"));
		}
	}
}

void USpawnManager::GatherOwnedPlotIds(TArray<FName>& OutOwnedPlotIds) const
{
	// 원장이 권위 — 스트리밍 아웃된 부지, 다른 도시 맵의 부지까지 보존한다.
	OutOwnedPlotIds = OwnedPlotIdLedger.Array();

	// 원장을 거치지 않고 소유로 바뀐 경로가 있어도 세이브에서 빠지지 않게 하는 안전망.
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		if (IsValid(Pair.Value) && Pair.Value->IsOwned() && !OwnedPlotIdLedger.Contains(Pair.Key))
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
	// 스트리밍 아웃된 부지는 키가 남은 채 값만 죽는다 — 소비자(수용량/표면 Z/테두리)에 넘기지 않는다.
	if (ACityPlotActor* const* Found = SpawnedPlots.Find(InPlotId))
	{
		return IsValid(*Found) ? *Found : nullptr;
	}
	return nullptr;
}

ACityPlotActor* USpawnManager::FindPlotByOccupantKey(int32 CompanyKey) const
{
	if (CompanyKey <= 0 || !TableManager)
	{
		return nullptr;
	}
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		if (!IsValid(Pair.Value))
		{
			continue;
		}
		bool bOk = false;
		const FCityPlotData PlotData = TableManager->GetCityPlotData(Pair.Key, bOk);
		if (bOk && PlotData.OccupantCompanyKeys.Contains(CompanyKey))
		{
			return Pair.Value;
		}
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

	// 면제 부지는 소유 부지 유무와 무관하게 목록에 든다 — bOwnedAtStart 를 빼먹으면 배지가 전멸해
	// 시작조차 못 하는 상태가 되므로, 소유 0개 조기 반환보다 앞에 둔다.
	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		ACityPlotActor* Plot = Pair.Value;
		if (IsValid(Plot) && !Plot->IsOwned() && IsAdjacencyExempt(Plot))
		{
			OutAdjacentPlots.Add(Plot);
		}
	}

	if (OwnedRects.Num() == 0)
	{
		return;
	}

	for (const TPair<FName, ACityPlotActor*>& Pair : SpawnedPlots)
	{
		ACityPlotActor* Plot = Pair.Value;
		if (!IsValid(Plot) || Plot->IsOwned() || IsAdjacencyExempt(Plot))
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

	if (IsAdjacencyExempt(Plot))
	{
		return true;
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

	// 아직 스트리밍 안 된 부지도 원장에는 올려 둔다 — RegisterPlacedPlot 이 늦게 들어올 때 집어간다.
	OwnedPlotIdLedger.Append(OwnedPlotIds);

	for (const FName& PlotId : OwnedPlotIds)
	{
		if (ACityPlotActor* const* FoundPlot = SpawnedPlots.Find(PlotId))
		{
			if (ACityPlotActor* Plot = IsValid(*FoundPlot) ? *FoundPlot : nullptr)
			{
				Plot->SetOwnedState(true);
			}
		}
	}
}

void USpawnManager::NoteOwnedPlot(FName InPlotId)
{
	if (!InPlotId.IsNone())
	{
		OwnedPlotIdLedger.Add(InPlotId);
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
			if (Other && OverlapsAt(Other->GetPlacementFootprintCenter()))
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
