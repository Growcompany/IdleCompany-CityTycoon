// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
//#include "Data/SpawnData.h"
//#include "Data/SpawnInstance.h"
//#include "NavigationData.h"
#include "SpawnManager.generated.h"

class UCGGameInstance;
class UTableManagerSubsystem;
class AInteractableBaseActor;
class ACityPlotActor;
class AVacantPlotDressingManager;

//struct FSpawnerInfo;
struct FInteractableInfo;
struct FBuildingEntitySaveData;

UCLASS()
class COMPANYGROWTHRENEWAL_API USpawnManager : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	USpawnManager();
	
	// ���ϴ� Level������ �� Subsystem�� ����ǰ�
	virtual bool ShouldCreateSubsystem(UObject * Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase & Collection) override;
	virtual void Deinitialize() override;
	//  ������ ��� ����, ������Ʈ	BeginPlay()�� ȣ���� ��, ȣ��
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;


private:
	UCGGameInstance* GameInstance;
	UTableManagerSubsystem* TableManager;
	FTimerHandle CheckNavMeshAndAsyncLoadTimerHandle;

	int InteractableSpawnCount = 0;
	int DecorationSpawnCount = 0;

	// 스폰된 부지 액터 캐시 (PlotId → 액터). 소유 복원 시 빠른 조회용.
	UPROPERTY()
	TMap<FName, ACityPlotActor*> SpawnedPlots;

	// MainMap 동적 빈 부지 소품 — 월드당 정확히 1개, 메시별 HISM을 소유한다.
	UPROPERTY()
	AVacantPlotDressingManager* VacantPlotDressingManager = nullptr;

	// 부지 중복 스폰 방지 가드
	bool bPlotsSpawned = false;

public:
	// 건물 스폰 (저장 데이터로 복원 시 사용)
	AInteractableBaseActor* SpawnBuilding(const FInteractableInfo& InteractableInfo,
		const FBuildingEntitySaveData* LoadData = nullptr);

	// 새 건물 스폰 (배치 시 사용)
	AInteractableBaseActor* SpawnNewBuilding(const FInteractableInfo& InteractableInfo, bool bIsNewlyPlaced = false);

	// ===== City Plot (도시 부지) 런타임 스폰 =====
	// MainMap 시작 시(OnWorldBeginPlay) DT_CityPlot 전 행을 순회하여 ACityPlotActor 를 스폰한다.
	// 건물 복원(EntityManager::RestoreEntityDataFromLoad)과 동일한 SpawnManager 라이프사이클을 쓴다.
	// bPlotsSpawned 가드로 중복 스폰을 막는다.
	void SpawnCityPlots();

	// 로드된 세이브의 OwnedPlotIds 로 이미 스폰된 부지들의 소유 상태를 복원한다.
	// (스폰은 OnWorldBeginPlay, 세이브 로드는 그 뒤라 분리 — SaveLoadManager 가 호출.)
	void RestorePlotOwnership(const TArray<FName>& OwnedPlotIds);

	// 세이브(gather) 단일 소스 — 현재 IsOwned() 인 부지 PlotId 를 수집한다.
	// 구매 부지 + bOwnedAtStart 부지가 모두 SetOwnedState(true) 로 표시돼 있으므로 한 번에 직렬화된다.
	void GatherOwnedPlotIds(TArray<FName>& OutOwnedPlotIds) const;

	// [치트] 스폰된 모든 부지를 즉시 소유 상태로(구매 게이트/결제 우회). 반환 = 새로 소유된 부지 수.
	// 세이브 직렬화는 GatherOwnedPlotIds 단일 소스라 여기선 SetOwnedState 만 — 호출자가 SaveGameData 를 부른다.
	int32 DebugOwnAllPlots();

	// 주어진 월드 위치(XY) 위에 있는 "소유" 부지를 반환한다(없으면 nullptr).
	// 부지 bounds = Center ± GetExtent()(격자 반경). 부지는 월드축 정렬(회전 0)이라 단순 AABB 검사.
	// 건물 배치 프리뷰가 매 프레임 "현재 호버 부지"를 동적 판정하는 데 사용(PlacementHandler).
	ACityPlotActor* GetOwnedPlotAt(const FVector& WorldPos) const;

	// PlotId 로 스폰된 부지 액터 조회(없으면 nullptr). 칸별 건축가능 마스크 조회 등 PlotId→액터 경로 재사용.
	ACityPlotActor* GetSpawnedPlotById(FName InPlotId) const;

	AVacantPlotDressingManager* GetVacantPlotDressingManager() const
	{
		return VacantPlotDressingManager;
	}

	// [dev 프리셋] 부지 안에서 기존 건물과 겹치지 않는 배치 트랜스폼을 찾는다.
	// 실제 배치는 UPlacementHandler::ComputePlotPlacement(private)가 드래그 입력 기반으로 산출해 재사용 불가 —
	// 여기선 그 겹침/접지 판정만 축소 재현한다.
	// ⚠ AlreadyPlanned = 아직 스폰 전이라 GetBuildingsOnPlot 에 안 잡히는 예정 위치들.
	//    시더가 좌표를 먼저 다 계산하고 나중에 일괄 스폰하므로, 이걸 안 넘기면 같은 부지의 두 번째 건물이
	//    첫 번째와 같은 자리를 받아 조용히 겹친다.
	bool FindFreeFootprintOnPlot(FName InPlotId, FVector2D Footprint, FTransform& OutTransform,
		const TArray<FVector>& AlreadyPlanned = TArray<FVector>()) const;

	// 가격 배지 대상 — "소유 부지 사각형과의 경계 간극 ≤ PlotAdjacencyGap 인 미소유 부지"를 수집한다.
	// 간극 기반(격자 인덱스 아님)이라 off-grid 타일·크기가 제각각인 부지에도 견고.
	// 소유가 바뀌면 다음 호출에서 한 겹씩 바깥으로 번진다.
	// 항상 켜진 가격 배지(인접-미소유 위) + 인수 게이트(부지 탭)에서 단일 인접 정의로 재사용.
	void GetAdjacentUnownedPlots(TArray<ACityPlotActor*>& OutAdjacentPlots) const;

	// 특정 미소유 부지가 인수 가능(소유 부지에 인접)한지. ACityPlotActor::OnEndInteract 의 인수 게이트가 사용.
	bool IsPlotAdjacentBuyable(const ACityPlotActor* Plot) const;

	void ClearSpawnerTimer();

	int ClassRefIndex;
	int Counter;
	int IndexCounter;
	float TotalCount = 50.f;

	bool AutoSpawn = false;

	FRandomStream Seed;
};

