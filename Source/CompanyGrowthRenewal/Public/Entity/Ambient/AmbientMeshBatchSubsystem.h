#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AmbientMeshBatchSubsystem.generated.h"

class AActor;
class ULevel;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT()
struct FAmbientBatchGroup
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(Transient)
	bool bCastShadow = false;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ISM = nullptr;

	// RemoveInstance 는 뒤 인덱스를 당겨 엔트리 매핑이 깨지므로 빈 슬롯은 스케일 0 으로 두고 재사용한다.
	TArray<int32> FreeInstances;
};

// 움직여서 에디터 HISM 병합에서 빠진 앰비언트 보트/부유물을 메시·머티리얼 그룹당 ISM 1개로 그린다(트랜스폼·연출은 원본 액터가 계속 소유).
UCLASS()
class COMPANYGROWTHRENEWAL_API UAmbientMeshBatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* InOuter) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	struct FBatchEntry
	{
		TWeakObjectPtr<UStaticMeshComponent> Source;
		int32 GroupIndex = INDEX_NONE;
		int32 InstanceIndex = INDEX_NONE;
		FTransform LastSent;
	};

	void HandleLevelAdded(ULevel* AddedLevel, UWorld* LevelWorld);
	void AddActors(const TArray<AActor*>& Candidates);
	int32 TryAdd(AActor* SourceActor);
	int32 FindOrCreateGroup(UStaticMeshComponent* SourceMesh);
	void RetireEntry(int32 EntryIndex);
	void UpdateForcedLods();

	UPROPERTY(Transient)
	TObjectPtr<AActor> Holder = nullptr;

	UPROPERTY(Transient)
	TArray<FAmbientBatchGroup> Groups;

	TArray<FBatchEntry> Entries;
};
