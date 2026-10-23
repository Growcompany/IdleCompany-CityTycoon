#include "Entity/Ambient/AmbientMeshBatchSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Entity/Ambient/AmbientAircraftFlyby.h"
#include "Entity/Ambient/AmbientSplineMover.h"
#include "Entity/Ambient/AmbientWaterFloat.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "SceneView.h"
#include "StaticMeshResources.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmbientBatch, Log, All);

namespace
{
bool IsAmbientBatchCandidate(const AActor* Candidate)
{
	// 항공기는 BeginPlay 이후 거리에 따라 메시를 숨겼다 켜는데, 묶음 인스턴스는 그 숨김을 따르지 못한다.
	return IsValid(Candidate) && !Candidate->IsA<AAmbientAircraftFlyby>()
		&& (Candidate->IsA<AAmbientWaterFloat>() || Candidate->IsA<AAmbientSplineMover>());
}
}

bool UAmbientMeshBatchSubsystem::ShouldCreateSubsystem(UObject* InOuter) const
{
	if (!Super::ShouldCreateSubsystem(InOuter))
	{
		return false;
	}
	const UWorld* OuterWorld = Cast<UWorld>(InOuter);
	return OuterWorld && OuterWorld->IsGameWorld();
}

void UAmbientMeshBatchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	TArray<AActor*> Candidates;
	for (TActorIterator<AAmbientWaterFloat> It(&InWorld); It; ++It) { Candidates.Add(*It); }
	for (TActorIterator<AAmbientSplineMover> It(&InWorld); It; ++It) { Candidates.Add(*It); }
	AddActors(Candidates);

	// WP 공간 로딩 셀은 이 시점 뒤(NotifyMatchStarted 블로킹 로드·카메라 이동)에 붙으므로 레벨이 추가될 때마다 다시 모은다.
	FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UAmbientMeshBatchSubsystem::HandleLevelAdded);
}

void UAmbientMeshBatchSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	Entries.Reset();
	Groups.Reset();
	Holder = nullptr;
	Super::Deinitialize();
}

void UAmbientMeshBatchSubsystem::HandleLevelAdded(ULevel* AddedLevel, UWorld* LevelWorld)
{
	if (LevelWorld != GetWorld() || !IsValid(AddedLevel))
	{
		return;
	}
	TArray<AActor*> Candidates;
	for (AActor* LevelActor : AddedLevel->Actors)
	{
		if (IsAmbientBatchCandidate(LevelActor)) { Candidates.Add(LevelActor); }
	}
	AddActors(Candidates);
}

void UAmbientMeshBatchSubsystem::AddActors(const TArray<AActor*>& Candidates)
{
	TArray<int32, TInlineAllocator<8>> TouchedGroups;
	int32 Added = 0;
	for (AActor* Candidate : Candidates)
	{
		const int32 GroupIndex = TryAdd(Candidate);
		if (GroupIndex != INDEX_NONE)
		{
			TouchedGroups.AddUnique(GroupIndex);
			++Added;
		}
	}
	if (Added == 0)
	{
		return;
	}
	// AddInstance 는 렌더 갱신을 걸지 않으므로(TrafficManager 와 동일) 묶음 끝에 그룹당 한 번 프록시를 다시 만든다.
	for (const int32 GroupIndex : TouchedGroups)
	{
		if (UInstancedStaticMeshComponent* GroupISM = Groups[GroupIndex].ISM) { GroupISM->MarkRenderStateDirty(); }
	}
	UE_LOG(LogAmbientBatch, Log, TEXT("[AmbientBatch] 액터 %d → ISM 그룹 %d (이번 +%d)"), Entries.Num(), Groups.Num(), Added);
}

int32 UAmbientMeshBatchSubsystem::TryAdd(AActor* SourceActor)
{
	UStaticMeshComponent* SourceMesh = IsAmbientBatchCandidate(SourceActor) ? SourceActor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!SourceMesh || !SourceMesh->GetStaticMesh() || !SourceMesh->IsRegistered()
		|| !SourceMesh->IsVisible() || SourceMesh->bHiddenInGame || SourceMesh->bVisibleInSceneCaptureOnly)
	{
		return INDEX_NONE;
	}
	if (Entries.ContainsByPredicate([SourceMesh](const FBatchEntry& Other) { return Other.Source.Get() == SourceMesh; }))
	{
		return INDEX_NONE;
	}

	const int32 GroupIndex = FindOrCreateGroup(SourceMesh);
	if (GroupIndex == INDEX_NONE || !Groups[GroupIndex].ISM)
	{
		return INDEX_NONE;
	}
	FAmbientBatchGroup& Group = Groups[GroupIndex];
	const FTransform& SourceTransform = SourceMesh->GetComponentTransform();
	int32 InstanceIndex = INDEX_NONE;
	if (Group.FreeInstances.Num() > 0)
	{
		InstanceIndex = Group.FreeInstances.Pop();
		Group.ISM->UpdateInstanceTransform(InstanceIndex, SourceTransform, true, false, true);
	}
	else
	{
		InstanceIndex = Group.ISM->AddInstance(SourceTransform, true);
	}

	FBatchEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Source = SourceMesh;
	Entry.GroupIndex = GroupIndex;
	Entry.InstanceIndex = InstanceIndex;
	Entry.LastSent = SourceTransform;

	// SetVisibility(false) 면 AmbientVisibilityComponent 가 IsVisible() 로 보고 움직임 틱을 꺼버린다 → 가시 플래그는 두고 메인 뷰·그림자에서만 뺀다(씬캡처엔 원본이 남아 오피스 배경 캡처가 그대로).
	SourceMesh->SetVisibleInSceneCaptureOnly(true);
	SourceMesh->SetCastShadow(false);
	return GroupIndex;
}

int32 UAmbientMeshBatchSubsystem::FindOrCreateGroup(UStaticMeshComponent* SourceMesh)
{
	UStaticMesh* Mesh = SourceMesh->GetStaticMesh();
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	for (int32 SlotIdx = 0; SlotIdx < SourceMesh->GetNumMaterials(); ++SlotIdx)
	{
		Materials.Add(SourceMesh->GetMaterial(SlotIdx));
	}
	const bool bShadow = SourceMesh->CastShadow;

	const int32 Existing = Groups.IndexOfByPredicate([Mesh, &Materials, bShadow](const FAmbientBatchGroup& Other)
	{
		return Other.Mesh == Mesh && Other.Materials == Materials && Other.bCastShadow == bShadow;
	});
	if (Existing != INDEX_NONE)
	{
		return Existing;
	}

	UWorld* BatchWorld = GetWorld();
	if (!BatchWorld)
	{
		return INDEX_NONE;
	}
	if (!Holder)
	{
		FActorSpawnParameters Params;
		Params.Name = TEXT("AmbientBatchHolder");
		Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Holder = BatchWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!Holder)
		{
			return INDEX_NONE;
		}
		USceneComponent* HolderRoot = NewObject<USceneComponent>(Holder, TEXT("AmbientBatchRoot"));
		Holder->SetRootComponent(HolderRoot);
		HolderRoot->RegisterComponent();
	}

	// 회색 방지: SetStaticMesh→Register→슬롯별 SetMaterial 명시(TrafficManager CreateInstanceComponents 와 같은 순서).
	UInstancedStaticMeshComponent* GroupISM = NewObject<UInstancedStaticMeshComponent>(Holder,
		MakeUniqueObjectName(Holder, UInstancedStaticMeshComponent::StaticClass(), *FString::Printf(TEXT("AmbientBatch_%s"), *Mesh->GetName())));
	GroupISM->SetupAttachment(Holder->GetRootComponent());
	GroupISM->SetStaticMesh(Mesh);
	GroupISM->SetMobility(EComponentMobility::Movable);
	GroupISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroupISM->SetCanEverAffectNavigation(false);
	GroupISM->SetCastShadow(bShadow);
	GroupISM->SetHiddenInSceneCapture(true);   // 씬캡처는 원본이 담당 — 같은 자리에 두 번 그리지 않는다
	GroupISM->RegisterComponent();
	for (int32 SlotIdx = 0; SlotIdx < Materials.Num(); ++SlotIdx)
	{
		if (Materials[SlotIdx]) { GroupISM->SetMaterial(SlotIdx, Materials[SlotIdx]); }
	}

	FAmbientBatchGroup& Group = Groups.AddDefaulted_GetRef();
	Group.Mesh = Mesh;
	Group.Materials = MoveTemp(Materials);
	Group.bCastShadow = bShadow;
	Group.ISM = GroupISM;
	return Groups.Num() - 1;
}

void UAmbientMeshBatchSubsystem::RetireEntry(int32 EntryIndex)
{
	const FBatchEntry& Entry = Entries[EntryIndex];
	FAmbientBatchGroup& Group = Groups[Entry.GroupIndex];
	if (Group.ISM)
	{
		FTransform Hidden = Entry.LastSent;
		Hidden.SetScale3D(FVector::ZeroVector);
		Group.ISM->UpdateInstanceTransform(Entry.InstanceIndex, Hidden, true, false, true);
		Group.FreeInstances.Add(Entry.InstanceIndex);
	}
	// 같은 레벨 오브젝트가 GC 전에 다시 스트리밍 인 되면 원래 플래그로 재수집돼야 그룹 키(CastShadow)가 맞는다.
	if (UStaticMeshComponent* SourceMesh = Entry.Source.Get())
	{
		SourceMesh->SetVisibleInSceneCaptureOnly(false);
		SourceMesh->SetCastShadow(Group.bCastShadow);
	}
	Entries.RemoveAtSwap(EntryIndex);
}

void UAmbientMeshBatchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TArray<bool, TInlineAllocator<16>> DirtyGroups;
	DirtyGroups.Init(false, Groups.Num());
	for (int32 EntryIndex = Entries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		FBatchEntry& Entry = Entries[EntryIndex];
		const UStaticMeshComponent* SourceMesh = Entry.Source.Get();
		// WP 셀 언로드는 컴포넌트 등록 해제가 GC 보다 먼저라 IsRegistered 로 봐야 유령 인스턴스가 남지 않는다.
		if (!SourceMesh || !SourceMesh->IsRegistered())
		{
			DirtyGroups[Entry.GroupIndex] = true;
			RetireEntry(EntryIndex);
			continue;
		}
		UInstancedStaticMeshComponent* GroupISM = Groups[Entry.GroupIndex].ISM;
		const FTransform& Current = SourceMesh->GetComponentTransform();
		if (GroupISM && !Current.Equals(Entry.LastSent, 0.01))
		{
			GroupISM->UpdateInstanceTransform(Entry.InstanceIndex, Current, true, false, true);
			Entry.LastSent = Current;
			DirtyGroups[Entry.GroupIndex] = true;
		}
	}

	UpdateForcedLods();

	// 프록시 재생성 없이 인스턴스 델타만 플러시(TrafficManager 와 같은 이유: 떨림 방지).
	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		if (DirtyGroups[GroupIndex] && Groups[GroupIndex].ISM) { Groups[GroupIndex].ISM->MarkRenderInstancesDirty(); }
	}
}

void UAmbientMeshBatchSubsystem::UpdateForcedLods()
{
	// ISM 은 도시 크기 컴포넌트 바운드로 LOD 를 골라 늘 LOD0 → TrafficManager 와 같은 계산으로 그룹마다 가장 크게 보이는 인스턴스 기준 강제.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	FSceneViewProjectionData ProjData;
	if (!LP || !LP->ViewportClient || !LP->GetProjectionData(LP->ViewportClient->Viewport, ProjData))
	{
		return;
	}
	const double ScreenMultiple = 0.5 * FMath::Max(ProjData.ProjectionMatrix.M[0][0], ProjData.ProjectionMatrix.M[1][1]);
	static const IConsoleVariable* SMLODScaleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.StaticMeshLODDistanceScale"));
	const float ScreenSizeScale = (SMLODScaleVar ? SMLODScaleVar->GetFloat() : 1.f) * PC->LocalPlayerCachedLODDistanceFactor;

	TArray<double, TInlineAllocator<16>> MaxScreenRadiusSq;
	MaxScreenRadiusSq.Init(-1.0, Groups.Num());
	for (const FBatchEntry& Entry : Entries)
	{
		const UStaticMesh* Mesh = Groups[Entry.GroupIndex].Mesh;
		if (!Mesh) { continue; }
		const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
		const FVector BoundsOrigin = Entry.LastSent.TransformPosition(MeshBounds.Origin);
		// 엔진은 스케일이 들어간 컴포넌트 바운드로 고르므로 인스턴스 스케일을 반지름에 곱한다(scale 2 배 등).
		const double Radius = MeshBounds.SphereRadius * Entry.LastSent.GetMaximumAxisScale();
		const double ScreenRadiusSq = FMath::Square(ScreenMultiple * Radius) / FMath::Max(1.0, FVector::DistSquared(BoundsOrigin, ProjData.ViewOrigin));
		MaxScreenRadiusSq[Entry.GroupIndex] = FMath::Max(MaxScreenRadiusSq[Entry.GroupIndex], ScreenRadiusSq);
	}
	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		UInstancedStaticMeshComponent* GroupISM = Groups[GroupIndex].ISM;
		UStaticMesh* Mesh = Groups[GroupIndex].Mesh;
		const FStaticMeshRenderData* RD = Mesh ? Mesh->GetRenderData() : nullptr;
		if (!GroupISM || !RD || MaxScreenRadiusSq[GroupIndex] < 0.0) { continue; }
		int32 Lod = RD->LODResources.Num() - 1;
		while (Lod > 0 && FMath::Square(RD->ScreenSize[Lod].GetValue() * ScreenSizeScale * 0.5f) <= MaxScreenRadiusSq[GroupIndex]) { --Lod; }
		// 0=자동이라 +1. 같은 값이면 엔진이 early-out → 프록시 재생성은 LOD 가 바뀔 때만.
		GroupISM->SetForcedLodModel(FMath::Max(Lod, Mesh->GetMinLODIdx()) + 1);
	}
}

TStatId UAmbientMeshBatchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAmbientMeshBatchSubsystem, STATGROUP_Tickables);
}

bool UAmbientMeshBatchSubsystem::IsTickable() const
{
	return Entries.Num() > 0;
}
