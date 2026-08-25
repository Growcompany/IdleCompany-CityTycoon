// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/AviationBeaconManager.h"

#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h" // TActorIterator
#include "Kismet/GameplayStatics.h"
#include "TimeCycle/TimeCycleManager.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/CityCompanyDirector.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAviationBeaconManager::AAviationBeaconManager()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 엔진 기본 구체(항상 쿠킹). 머티리얼은 런타임 로드(BuildSkylineBeacons).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (MeshFinder.Succeeded())
	{
		BeaconMesh = MeshFinder.Object;
	}
}

void AAviationBeaconManager::BeginPlay()
{
	Super::BeginPlay();

	// 낮엔 항공장애등(additive)을 숨겨 fill 절감 — day/night 전환 구독. 못 찾으면 항상 표시.
	if (ATimeCycleManager* Clock = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass())))
	{
		CachedTimeCycle = Clock;
		Clock->OnSunRise.AddDynamic(this, &AAviationBeaconManager::HandleSunRise);
		Clock->OnSunSet.AddDynamic(this, &AAviationBeaconManager::HandleSunSet);
		const int32 Cur = Clock->GetCurrentTime().ToSeconds();
		const int32 Rise = Clock->GetSunRiseTime().ToSeconds();
		const int32 Set = Clock->GetSunSetTime().ToSeconds();
		bGlowVisible = (Cur >= Set) || (Cur < Rise);
	}

	// 회사 철거 완료 시 해당 건물 비콘 제거 — 런타임 철거 대응(비콘은 건물 자식이 아니라 매니저 소유 ISM).
	if (UCityAcquisitionManager* Acq = GetWorld()->GetSubsystem<UCityAcquisitionManager>())
	{
		Acq->OnCompanyCleared.AddUObject(this, &AAviationBeaconManager::HandleCompanyCleared);
	}

	// 비콘 빌드는 한 박자 뒤로 — Director 의 스카이라인 매핑 + 인수매니저 LoadFromGame 이 끝난 뒤
	// Cleared 게이트를 신뢰할 수 있게(서브시스템 초기화 순서 의존 제거, SpawnClickProxies 의 0.5s 지연 미러).
	// ApplyNightVisibility 는 빌드 직후 BuildSkylineBeacons 말미에서 호출한다.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			BuildTimerHandle, this, &AAviationBeaconManager::BuildSkylineBeacons, 0.5f, false);
	}
}

void AAviationBeaconManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(BuildTimerHandle);
		if (UCityAcquisitionManager* Acq = W->GetSubsystem<UCityAcquisitionManager>())
		{
			Acq->OnCompanyCleared.RemoveAll(this);
		}
	}
	if (ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		Clock->OnSunRise.RemoveDynamic(this, &AAviationBeaconManager::HandleSunRise);
		Clock->OnSunSet.RemoveDynamic(this, &AAviationBeaconManager::HandleSunSet);
	}
	Super::EndPlay(EndPlayReason);
}

void AAviationBeaconManager::ApplyNightVisibility(bool bNight)
{
	bGlowVisible = bNight;
	for (UInstancedStaticMeshComponent* ISM : BeaconISMs)
	{
		if (ISM)
		{
			ISM->SetVisibility(bNight, true);
		}
	}
}

void AAviationBeaconManager::HandleSunRise() { ApplyNightVisibility(false); }
void AAviationBeaconManager::HandleSunSet()  { ApplyNightVisibility(true); }

void AAviationBeaconManager::BuildSkylineBeacons()
{
	if (!BeaconMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkyBeacon] BeaconMesh 없음 — 중단"));
		return;
	}

	// 비콘 머티리얼은 런타임 로드 — 에셋이 C++ 이후 생성되어도 재시작 없이 반영
	// (CLAUDE.md 런타임 로드 패턴, BuildingBaseActor::UpdateRooftopBeacons 와 동일).
	if (!BeaconMaterial)
	{
		BeaconMaterial = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Environment/Beacon/MI_AviationBeacon.MI_AviationBeacon"))).LoadSynchronous();
	}

	// 인수 가능한 스카이라인 건물의 Key 정본(Director 가 태그/DT 검증까지 끝낸 매핑) — 철거 게이트 + 키잉용.
	// 회사 데이터가 없는 순수 장식 BP_MB 는 이 맵에 없음 → Key=INDEX_NONE 로 게이트 없이 항상 비콘 생성(회귀 방지).
	TMap<AActor*, int32> KeyByBuilding;
	if (UCityCompanyDirector* Dir = GetWorld()->GetSubsystem<UCityCompanyDirector>())
	{
		for (const FCitySkylineEntry& E : Dir->GetSkylineEntries())
		{
			if (AActor* B = E.Building.Get())
			{
				KeyByBuilding.Add(B, E.BuildingKey);
			}
		}
	}
	UCityAcquisitionManager* Acq = GetWorld()->GetSubsystem<UCityAcquisitionManager>();

	int32 TotalBeacons = 0;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Building = *It;
		if (!Building || !Building->GetClass())
			continue;
		if (!Building->GetClass()->GetName().StartsWith(CityBuildingClassPrefix))
			continue;

		// 철거(Cleared)된 회사 건물은 비콘 생략 — 숨겨진 건물 옥상 자리에 허공 비콘이 남는 버그 방지(재시작 포함).
		const int32* FoundKey = KeyByBuilding.Find(Building);
		const int32 CompanyKey = FoundKey ? *FoundKey : INDEX_NONE;
		if (Acq && CompanyKey != INDEX_NONE && Acq->GetState(CompanyKey) == EAcqState::Cleared)
			continue;

		// 이 건물의 메시 컴포넌트에서 비콘 소켓 수집.
		// ★ 스카이라인 BP는 ABuildingBaseActor 와 동일 모듈 구조 — 지붕이 Top_Module(ISM)의
		//   "인스턴스"(예: Z=20400)로 얹혀 있다. ISM 컴포넌트 자체는 바닥(Z=0)이라 GetSocketTransform 은
		//   지면을 반환한다. → 각 인스턴스 월드변환 ∘ 소켓 로컬 로 실제 지붕 소켓 위치를 계산.
		TArray<FTransform> WorldXforms;
		TArray<FVector> FinalScales;
		TArray<bool> LargeFlags; // 인스턴스별 CustomData0 = 대형만 깜빡(소형은 상시등) 게이트

		TArray<UStaticMeshComponent*> MeshComps;
		Building->GetComponents<UStaticMeshComponent>(MeshComps);
		for (UStaticMeshComponent* MeshComp : MeshComps)
		{
			if (!MeshComp)
				continue;
			UStaticMesh* SM = MeshComp->GetStaticMesh();
			if (!SM)
				continue;

			UInstancedStaticMeshComponent* SrcISM = Cast<UInstancedStaticMeshComponent>(MeshComp);

			for (UStaticMeshSocket* Socket : SM->Sockets)
			{
				if (!Socket)
					continue;

				const FString SockName = Socket->SocketName.ToString();
				const bool bLarge = SockName.StartsWith(TEXT("L_Beacon"));
				const bool bBeacon = bLarge || SockName.StartsWith(TEXT("Beacon"));
				if (!bBeacon)
					continue;

				const FTransform SocketLocal(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale);
				const float Base = bLarge ? BeaconScaleLarge : BeaconScaleNormal;

				auto AddSocketAt = [&](const FTransform& SocketWorld)
				{
					FVector P = SocketWorld.GetLocation();
					P.Z += BeaconZLift;
					WorldXforms.Add(FTransform(SocketWorld.GetRotation(), P, SocketWorld.GetScale3D()));
					FinalScales.Add(SocketWorld.GetScale3D() * Base); // 건물 스케일 따라가며 Large/Normal
					LargeFlags.Add(bLarge);
				};

				if (SrcISM && SrcISM->GetInstanceCount() > 0)
				{
					// ISM: 지붕이 얹힌 각 인스턴스 기준으로 소켓 월드 계산 (소켓로컬 ∘ 인스턴스월드)
					for (int32 Inst = 0; Inst < SrcISM->GetInstanceCount(); ++Inst)
					{
						FTransform InstW;
						SrcISM->GetInstanceTransform(Inst, InstW, /*bWorldSpace=*/true);
						AddSocketAt(SocketLocal * InstW);
					}
				}
				else
				{
					// 일반 StaticMeshComponent: 컴포넌트 변환 그대로
					AddSocketAt(MeshComp->GetSocketTransform(Socket->SocketName, RTS_World));
				}
			}
		}

		if (WorldXforms.Num() == 0)
			continue;

		// 건물별 ISM 1개 — 인스턴스가 한 건물에 모여 있어 ObjectPositionWS ≈ 그 건물 → 건물 간 비동기 유지
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetupAttachment(RootComponent);
		ISM->SetMobility(EComponentMobility::Movable);
		ISM->RegisterComponent();
		ISM->SetStaticMesh(BeaconMesh);
		if (BeaconMaterial)
		{
			ISM->SetMaterial(0, BeaconMaterial);
		}
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCastShadow(false);
		ISM->bUseAsOccluder = false;
		ISM->NumCustomDataFloats = 1; // [0]=BlinkGate(1=대형 깜빡/0=소형 상시) — 머티리얼 PerInstanceCustomData
		BeaconISMs.Add(ISM);
		// 인수 가능한 건물이면 Key 로 인덱싱 — 나중에 철거(OnCompanyCleared) 시 이 건물 비콘만 제거.
		if (CompanyKey != INDEX_NONE)
		{
			BeaconISMByKey.Add(CompanyKey, ISM);
		}

		for (int32 i = 0; i < WorldXforms.Num(); ++i)
		{
			FTransform T = WorldXforms[i];
			T.SetScale3D(FinalScales[i]);
			const int32 InstIdx = ISM->AddInstance(T, /*bWorldSpace=*/true);
			ISM->SetCustomDataValue(InstIdx, 0, LargeFlags[i] ? 1.f : 0.f, /*bMarkRenderStateDirty=*/i == WorldXforms.Num() - 1);
		}

		TotalBeacons += WorldXforms.Num();
		UE_LOG(LogTemp, Warning, TEXT("[SkyBeacon] %s beacons=%d"), *Building->GetName(), WorldXforms.Num());
	}

	UE_LOG(LogTemp, Warning, TEXT("[SkyBeacon] 완료 — 건물 ISM %d개 / 비콘 %d개"), BeaconISMs.Num(), TotalBeacons);

	// 지연 빌드라 BeginPlay 의 초기 ApplyNightVisibility 가 빈 ISM 에 걸렸음 → 빌드 직후 현재 낮/밤 상태 반영.
	ApplyNightVisibility(bGlowVisible);
}

void AAviationBeaconManager::HandleCompanyCleared(int32 Key)
{
	// 철거된 회사 건물의 비콘 ISM 을 찾아 제거 — 건물은 숨김(Destroy 아님)이라 비콘은 매니저가 직접 정리해야 한다.
	UInstancedStaticMeshComponent* ISM = nullptr;
	if (BeaconISMByKey.RemoveAndCopyValue(Key, ISM) && ISM)
	{
		BeaconISMs.Remove(ISM);
		ISM->DestroyComponent();
	}
}
