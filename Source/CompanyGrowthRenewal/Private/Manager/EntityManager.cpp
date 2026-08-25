// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/EntityManager.h"

#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/CGGameInstance.h"
#include "Entity/InteractableBaseActor.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/SpawnManager.h"
#include "Table/InteractableInfo.h"
#include "Global/GlobalUtilFunctions.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/TableManagerSubsystem.h"


// 이 서브시스템이 처음 생성될 때(월드가 로드될 때) 호출됩니다.
void UEntityManager::Initialize(FSubsystemCollectionBase & Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogTemp, Warning, TEXT("[UEntityManager::Initialize()]"));

	// 다른 서브시스템에 대한 참조를 얻거나,
	// 필요한 데이터를 로드하고,
	// 델리게이트를 바인딩하는 등의 초기 설정 작업을 수행
}

// 이 서브시스템이 소멸될 때(월드가 언로드될 때) 호출됩니다.
void UEntityManager::Deinitialize()
{
	UE_LOG(LogTemp, Warning, TEXT("[UEntityManager::Deinitialize()]"));

	// TArray, TMap 등의 컨테이너를 비우고,
	// 바인딩했던 델리게이트를 해제하고,
	// 동적으로 할당했던 메모리를 해제하는 등의 정리 작업을 수행

	Super::Deinitialize();
}

void UEntityManager::CollectEntityDataForSave()
{
	// 저장 전에 건물 데이터 수집
	BuildingDataList.Empty();

	UE_LOG(LogTemp, Warning, TEXT("[EntityManager] CollectEntityDataForSave - this: %p, Buildings: %d"),
		this, Buildings.Num());

	// 중복 Index 체크용 Set
	TSet<int32> UsedIndices;

	// 건물만 저장 (Building 전용 구조체 사용)
	for (ABuildingBaseActor* Building : Buildings)
	{
		if (IsValid(Building))
		{
			int32 BuildingIndex = Building->GetBuildingIndex();

			// 중복 체크
			if (UsedIndices.Contains(BuildingIndex))
			{
				UE_LOG(LogTemp, Error, TEXT("[EntityManager] DUPLICATE INDEX DETECTED! Building: %s, Index: %d"),
					*Building->GetName(), BuildingIndex);
			}
			else
			{
				UsedIndices.Add(BuildingIndex);
			}

			UE_LOG(LogTemp, Log, TEXT("[EntityManager] Saving building: %s (Index: %d)"),
				*Building->GetName(), BuildingIndex);

			BuildingDataList.Add(Building->GetBuildingSaveData());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[EntityManager] CollectEntityDataForSave - Buildings: %d"),
		BuildingDataList.Num());
}

void UEntityManager::RestoreEntityDataFromLoad()
{
	// 로드 후 건물 데이터 복원 처리
	UE_LOG(LogTemp, Log, TEXT("[EntityManager] RestoreEntityDataFromLoad - Buildings: %d"),
		BuildingDataList.Num());

	// 복원할 데이터가 없으면 early return
	if (BuildingDataList.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[EntityManager] No buildings to restore"));
		return;
	}

	// SpawnManager와 TableManager 가져오기
	USpawnManager* SpawnManager = GetWorld()->GetSubsystem<USpawnManager>();
	if (!SpawnManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityManager] SpawnManager not found!"));
		return;
	}

	// World 유효성 검사
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityManager] World is null!"));
		return;
	}

	// GameInstance 유효성 검사
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityManager] GameInstance is null!"));
		return;
	}

	// TableManager 가져오기
	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (!TableManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityManager] TableManager not found!"));
		return;
	}

	// TableManager 초기화 상태 확인
	if (!TableManager->IsFullyInitialized())
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityManager] TableManager is not fully initialized yet!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EntityManager] Starting building restore - Building count: %d"), BuildingDataList.Num());

	// 저장된 건물들을 월드에 스폰
	for (const FBuildingEntitySaveData& BuildingData : BuildingDataList)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Attempting to load building: %s (Index: %d)"),
			*BuildingData.InteractableName.ToString(),
			BuildingData.BuildingIndex);

		// TableManager에서 InteractableInfo 가져오기
		bool bFound = false;
		FInteractableInfo InteractableInfo = TableManager->GetInteractableInfo(BuildingData.InteractableName, bFound);

		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] GetInteractableInfo result for %s: %s"),
			*BuildingData.InteractableName.ToString(),
			bFound ? TEXT("FOUND") : TEXT("NOT FOUND"));

		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EntityManager] InteractableInfo not found for: %s"),
				*BuildingData.InteractableName.ToString());
			continue;
		}

		// SpawnManager를 통해 건물 스폰
		ABuildingBaseActor* SpawnedBuilding = Cast<ABuildingBaseActor>(
			SpawnManager->SpawnBuilding(InteractableInfo, &BuildingData));

		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] SpawnBuilding returned: %s"),
			SpawnedBuilding ? TEXT("SUCCESS") : TEXT("NULL"));

		if (SpawnedBuilding && IsValid(SpawnedBuilding))
		{
			// 트랜스폼 설정
			FTransform SpawnTransform(BuildingData.Rotation, BuildingData.Location, BuildingData.Scale);
			SpawnedBuilding->SetActorTransform(SpawnTransform);

			// Tags 설정 (클릭 가능하게)
			SpawnedBuilding->Tags.Add(FName("Interactable"));
			SpawnedBuilding->Tags.Add(FName("Building"));

			// Collision 박스 재계산
			SpawnedBuilding->ReCalcBoxExtent();

			// NavMesh 업데이트
			SpawnedBuilding->UpdateNavBlockerState();

			UE_LOG(LogTemp, Log, TEXT("[EntityManager] Spawned building: %s at %s"),
				*BuildingData.InteractableName.ToString(),
				*BuildingData.Location.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Failed to spawn building: %s"),
				*BuildingData.InteractableName.ToString());
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Restore completed - %d buildings processed"),
		BuildingDataList.Num());
}

void UEntityManager::AddInteractable(AInteractableBaseActor* Interactable)
{
	if (!IsValid(Interactable))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] AddInteractable called with invalid Interactable"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[EntityManager] AddInteractable called - this: %p, Actor: %s"),
		this, *Interactable->GetName());

	// 중복 체크
	if (Interactables.Contains(Interactable))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Interactable already exists, skipping: %s"), *Interactable->GetName());
		return;
	}

	Interactables.Add(Interactable);
	UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Added to Interactables array. Total count: %d"), Interactables.Num());
}

void UEntityManager::RemoveInteractable(AInteractableBaseActor* Interactable)
{
	if (!IsValid(Interactable))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] RemoveInteractable called with invalid Interactable"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[EntityManager] RemoveInteractable called - Actor: %s"),
		*Interactable->GetName());

	if (!Interactables.Contains(Interactable))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Interactable not found in array, skipping removal"));
		return;
	}

	Interactables.Remove(Interactable);
	UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Removed from Interactables. Remaining count: %d"), Interactables.Num());
}

void UEntityManager::AddBuilding(ABuildingBaseActor* Building, bool bRequireBuild)
{
	AddInteractable(Cast<AInteractableBaseActor>(Building));

	if (Buildings.Contains(Building) == false)
	{
		// 새 건물이고 BuildingIndex가 없으면 할당
		if (Building->GetBuildingIndex() == INDEX_NONE)
		{
			UCGGameInstance* GameInstance = GetWorld()->GetGameInstance<UCGGameInstance>();
			if (GameInstance)
			{
				int32 NewIndex = GameInstance->AllocateNextBuildingIndex();

				// 이미 사용 중인 Index면 사용하지 않는 Index를 찾을 때까지 반복
				while (IsBuildingIndexInUse(NewIndex))
				{
					UE_LOG(LogTemp, Warning, TEXT("[EntityManager] BuildingIndex %d already in use, allocating next"), NewIndex);
					NewIndex = GameInstance->AllocateNextBuildingIndex();
				}

				Building->SetBuildingIndex(NewIndex);
				UE_LOG(LogTemp, Log, TEXT("[EntityManager] Assigned BuildingIndex %d to building"), NewIndex);
			}
		}
		else
		{
			// 이미 Index가 있는 경우 중복 체크 (Add 전에 체크!)
			int32 ExistingIndex = Building->GetBuildingIndex();
			if (IsBuildingIndexInUse(ExistingIndex))
			{
				UE_LOG(LogTemp, Error, TEXT("[EntityManager] WARNING: BuildingIndex %d is already in use by another building!"), ExistingIndex);
			}
		}

		Buildings.Add(Building);
		OnBuildingCountChanged.Broadcast();
	}

	if (bRequireBuild)
	{
		if (BuildingsRequireBuild.Contains(Building) == false)
		{
			BuildingsRequireBuild.Add(Building);
		}
	}


	//if (Building->GetInteractableName() == "TownHall")
	//{
	//	TownHall = Building;
	//}
}

void UEntityManager::RemoveBuilding(ABuildingBaseActor* Building)
{
	RemoveInteractable(Cast<AInteractableBaseActor>(Building));
	bool bRemoved = false;
	if (Buildings.Contains(Building))
	{
		Buildings.Remove(Building);
		bRemoved = true;
	}

	if (BuildingsRequireBuild.Contains(Building))
	{
		BuildingsRequireBuild.Remove(Building);
	}

	if (bRemoved)
	{
		OnBuildingCountChanged.Broadcast();
	}
}

void UEntityManager::ChangeBuildState(ABuildingBaseActor* Building, bool bRequireBuild)
{
	if (BuildingsRequireBuild.Contains(Building) && !bRequireBuild)
	{
		BuildingsRequireBuild.Remove(Building);
	}
	else if (!BuildingsRequireBuild.Contains(Building) && bRequireBuild)
	{
		BuildingsRequireBuild.Add(Building);
	}
}

int UEntityManager::GetRequireBuildBuildingCount()
{
	return BuildingsRequireBuild.Num();
}

ABuildingBaseActor* UEntityManager::FindNearestRequireBuildBuilding(FVector Location, int selectionIndex)
{
	if (BuildingsRequireBuild.Num() == 0)
	{
		return nullptr;
	}

	if (BuildingsRequireBuild.Num() <= selectionIndex)
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildingsRequireBuild.Num() <= selectionIndex"));
		return nullptr;
	}

	const int currentFrameCount = GFrameCounter;

	// 한프레임에 여러번 호출 될 수 있으나 한번만 정렬하도록
	if (LastSortedBuildingFrameCount != currentFrameCount ||
		FVector::Distance(LastSortedBuildingLocation, Location) > 0.1f)
	{
		for (ABuildingBaseActor* Building : BuildingsRequireBuild)
		{
			Building->CalcTemporaryDistanceFromLocation(Location);
		}

		BuildingsRequireBuild.Sort([Location](const ABuildingBaseActor& A, const ABuildingBaseActor& B)
			{
				return A.GetDistSquared() < B.GetDistSquared();
			});
		
		LastSortedBuildingFrameCount = currentFrameCount;
		LastSortedBuildingLocation = Location;
	}

	return BuildingsRequireBuild[selectionIndex];
}

ABuildingBaseActor* UEntityManager::GetBuildingByIndex(int32 InBuildingIndex)
{
	if (InBuildingIndex == INDEX_NONE)
	{
		return nullptr;
	}

	for (ABuildingBaseActor* Building : Buildings)
	{
		if (Building && Building->GetBuildingIndex() == InBuildingIndex)
		{
			return Building;
		}
	}
	return nullptr;
}

TArray<ABuildingBaseActor*> UEntityManager::GetBuildingsOnPlot(FName PlotId) const
{
	TArray<ABuildingBaseActor*> Result;
	if (PlotId.IsNone())
	{
		return Result;
	}

	for (ABuildingBaseActor* Building : Buildings)
	{
		if (Building && Building->GetOwningPlotId() == PlotId)
		{
			Result.Add(Building);
		}
	}
	return Result;
}

bool UEntityManager::HasFreeCapacityOnPlot(FName PlotId, int32 Capacity) const
{
	if (PlotId.IsNone()) return false;
	return GetBuildingsOnPlot(PlotId).Num() < FMath::Max(1, Capacity);
}

bool UEntityManager::IsBuildingIndexInUse(int32 InBuildingIndex) const
{
	if (InBuildingIndex == INDEX_NONE)
	{
		return false;
	}

	for (const ABuildingBaseActor* Building : Buildings)
	{
		if (Building && Building->GetBuildingIndex() == InBuildingIndex)
		{
			return true;
		}
	}
	return false;
}

// 건물 해금 시스템
void UEntityManager::UnlockNextBuilding()
{
	if (UnlockedConstructionLevel >= 40)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EntityManager] Already unlocked all buildings (Level 40)"));
		return;
	}

	UnlockedConstructionLevel++;
	UE_LOG(LogTemp, Log, TEXT("[EntityManager] Unlocked Construction Level %d"), UnlockedConstructionLevel);

	// SaveLoadManager를 통해 자동 저장되도록 트리거 (필요시 구현)
	// TODO: 자동 저장 로직 추가
}
