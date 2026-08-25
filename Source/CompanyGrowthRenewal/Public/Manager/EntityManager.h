// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Enum/ResourceType.h"
#include "Data/EntitySaveData.h"
#include "EntityManager.generated.h"

class AInteractableBaseActor;
class ABuildingBaseActor;
class AResourceBaseActor;
class ACropBaseActor;
//class AVillager;
class ASpawner;
class FDelegateHandle;

//template <typename T>
//class SimpleRxReactiveProperty;

/**
 * 월드에 존재하는 모든 엔티티(상호작용 가능 액터, 건물, 자원, 주민 등)를 관리하는 서브시스템입니다.
 * 월드가 생성될 때 함께 생성되고, 월드가 소멸될 때 함께 소멸됩니다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEntityManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem의 생명주기 함수
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

    //  월드의 모든 액터, 컴포넌트	BeginPlay()를 호출한 후, 호출

private:
	UPROPERTY()
	TArray<FBuildingEntitySaveData> BuildingDataList;

	// Interactables들 저장 및 관리용 Array
	UPROPERTY()
	TArray<AInteractableBaseActor*> Interactables;
	UPROPERTY()
	TArray<ABuildingBaseActor*> Buildings;
	UPROPERTY()
	TArray<ABuildingBaseActor*> BuildingsRequireBuild;
	int LastSortedBuildingFrameCount = 0;
	FVector LastSortedBuildingLocation;

	TMap<FName, UInstancedStaticMeshComponent*> DecorationMeshComponents;

	void AddInteractable(AInteractableBaseActor* Interactable);
	void RemoveInteractable(AInteractableBaseActor* Interactable);

	// 건물 해금 시스템 - 현재 해금된 건설 레벨 (런타임 캐시)
	int32 UnlockedConstructionLevel = 1;

public:
	// ===== Save/Load Getter/Setter =====
	// SaveLoadManager에서 호출하여 데이터 가져오기/설정하기

	// 건물 데이터 Get/Set
	const TArray<FBuildingEntitySaveData>& GetBuildingsData() const { return BuildingDataList; }
	void SetBuildingsData(const TArray<FBuildingEntitySaveData>& InBuildings) { BuildingDataList = InBuildings; }

	// 건물 해금 레벨 Get/Set
	int32 GetUnlockedConstructionLevel() const { return UnlockedConstructionLevel; }
	void SetUnlockedConstructionLevel(int32 InLevel) { UnlockedConstructionLevel = FMath::Clamp(InLevel, 1, 40); }

	// 특정 건물이 해금되었는지 체크 (건설 레벨로 확인)
	bool IsBuildingUnlocked(int32 BuildingConstructionLevel) const { return BuildingConstructionLevel <= UnlockedConstructionLevel; }

	// 다음 건물 해금 (건물 배치 완료 시 호출)
	void UnlockNextBuilding();

	// ===== Legacy Save/Load (SaveLoadManager로 통합 예정) =====
	// 현재 EntityManager 내부에서 Interactables를 순회하며 데이터를 수집
	void CollectEntityDataForSave();
	void RestoreEntityDataFromLoad();

	void AddBuilding(ABuildingBaseActor* Building, bool bRequireBuild = true);
	void RemoveBuilding(ABuildingBaseActor* Building);

	// 건물 수가 변동되었을 때 브로드캐스트 (UI 카운터 갱신용)
	FSimpleMulticastDelegate OnBuildingCountChanged;
	void ChangeBuildState(ABuildingBaseActor* Building, bool bRequireBuild);
	int GetRequireBuildBuildingCount();
	ABuildingBaseActor* FindNearestRequireBuildBuilding(FVector Location, int selectionIndex);
	const TArray<ABuildingBaseActor*>& GetBuildings() const { return Buildings; }
	ABuildingBaseActor* GetBuildingByIndex(int32 InBuildingIndex);

	// 특정 도시 블록(부지)에 소속된 건물 목록 — 수용량 카운트(Task 4)에서 사용
	TArray<ABuildingBaseActor*> GetBuildingsOnPlot(FName PlotId) const;

	// 부지에 건물을 더 지을 여지가 있는지 = 현재 건물 수 < 수용량(BuildingCapacity). (BuildModal 게이트)
	// 격자 제거로 물리적 빈칸 스캔 폐기 — 카운트 캡이 한도, 물리적 배치 가능 여부는 PlacementHandler 가 판정.
	bool HasFreeCapacityOnPlot(FName PlotId, int32 Capacity) const;

	// BuildingIndex 중복 체크
	bool IsBuildingIndexInUse(int32 InBuildingIndex) const;
};
