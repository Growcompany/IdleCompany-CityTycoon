// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/StreetLampSpotPool.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimeCycle/TimeCycleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RenderUtils.h"
#include "SceneInterface.h"

AStreetLampSpotPool::AStreetLampSpotPool()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 새 자산 없이 재사용: 엔진 Plane + MI_StreetLampCard(부모 M_CarLight = Additive/Unlit).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) { PoolCardMesh = Plane.Object; }
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Card(TEXT("/Game/CompanyGrowth/Environment/Beacon/MI_StreetLampCard.MI_StreetLampCard"));
	if (Card.Succeeded()) { PoolCardMaterial = Card.Object; }
}

void AStreetLampSpotPool::BeginPlay()
{
	Super::BeginPlay();

	for (const TSoftObjectPtr<UStaticMesh>& SoftMesh : LampMeshes)
	{
		if (UStaticMesh* Mesh = SoftMesh.LoadSynchronous()) { ResolvedLampMeshes.AddUnique(Mesh); }
	}
	FWorldDelegates::LevelAddedToWorld.AddUObject(this, &AStreetLampSpotPool::OnLevelAdded);
	FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &AStreetLampSpotPool::OnLevelRemoved);
	for (ULevel* LoadedLevel : GetWorld()->GetLevels())
	{
		if (LoadedLevel && LoadedLevel->bIsVisible && !LoadedLevel->bIsBeingRemoved)
		{
			CollectLampHeads(LoadedLevel);
		}
	}
	RebuildLampData();
}

void AStreetLampSpotPool::EnsureLightPool()
{
	if (!Pool.IsEmpty() || LampHeads.IsEmpty() || PoolSize <= 0) { return; }
	if (const FSceneInterface* Scene = GetWorld()->Scene)
	{
		const FStaticShaderPlatform ShaderPlatform = Scene->GetShaderPlatform();
		if (IsMobilePlatform(ShaderPlatform) && !IsMobileDeferredShadingEnabled(ShaderPlatform)
			&& !MobileForwardEnableLocalLights(ShaderPlatform)) { return; }
	}

	Pool.Reserve(PoolSize);
	for (int32 i = 0; i < PoolSize; ++i)
	{
		USpotLightComponent* L = NewObject<USpotLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(Intensity);
		L->SetLightColor(LightColor);
		L->SetOuterConeAngle(OuterConeAngle);
		L->SetInnerConeAngle(InnerConeAngle);
		L->SetAttenuationRadius(AttenuationRadius);
		L->SetCastShadows(false);
		L->SetVolumetricScatteringIntensity(0.f);
		L->RegisterComponent();
		L->SetVisibility(false);
		Pool.Add(L);
	}
	UE_LOG(LogTemp, Warning, TEXT("[LampSpot] 램프 %d개 / 광원 %d개 생성"), LampHeads.Num(), Pool.Num());
}

void AStreetLampSpotPool::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
	bLampDataDirty = false;
	LevelLampData.Reset();
	ResolvedLampMeshes.Reset();
	CachedTimeCycle.Reset();
	SetPoolEnabled(false);
	if (PoolISM) { PoolISM->ClearInstances(); }
	Pool.Reset();
	LampHeads.Reset();
	LampCardAnchors.Reset();
	Super::EndPlay(EndPlayReason);
}

void AStreetLampSpotPool::OnLevelAdded(ULevel* AddedLevel, UWorld* LevelWorld)
{
	if (LevelWorld != GetWorld() || !IsValid(AddedLevel)) { return; }
	CollectLampHeads(AddedLevel);
}

void AStreetLampSpotPool::OnLevelRemoved(ULevel* RemovedLevel, UWorld* LevelWorld)
{
	if (LevelWorld != GetWorld()) { return; }
	if (!RemovedLevel)
	{
		bLampDataDirty |= !LevelLampData.IsEmpty();
		LevelLampData.Reset();
	}
	else
	{
		bLampDataDirty |= LevelLampData.Remove(RemovedLevel) > 0;
	}
}

void AStreetLampSpotPool::RefreshLampSources(TConstArrayView<ULevel*> ChangedLevels)
{
	if (!HasActorBegunPlay()) { return; }
	for (ULevel* LampLevel : ChangedLevels)
	{
		if (!IsValid(LampLevel) || LampLevel->OwningWorld != GetWorld()) { continue; }
		bLampDataDirty |= LevelLampData.Remove(LampLevel) > 0;
		if (LampLevel->bIsVisible && !LampLevel->bIsBeingRemoved)
		{
			CollectLampHeads(LampLevel);
		}
	}
}

// 메시 이름이 아니라 에셋 포인터로 비교한다 - 이름 비교는 _Large 접미사 같은 변종에 조용히 걸린다.
void AStreetLampSpotPool::CollectLampHeads(ULevel* LampLevel)
{
	if (ResolvedLampMeshes.IsEmpty() || LevelLampData.Contains(LampLevel)) { return; }
	FLevelLampData Collected;
	for (AActor* LampActor : LampLevel->Actors)
	{
		if (!IsValid(LampActor) || LampActor == this) { continue; }
		for (UInstancedStaticMeshComponent* ISM : TInlineComponentArray<UInstancedStaticMeshComponent*>(LampActor))
		{
			const UStaticMesh* M = ISM ? ISM->GetStaticMesh() : nullptr;
			if (!M || !ResolvedLampMeshes.Contains(M)) { continue; }

			// 1순위 = 소켓("Lamp" 로 시작). 램프 머리는 기둥 위가 아니라 팔 끝에 있어 바운드 중심 추정은
			// XY 에서도 빗나간다(SM_Street_Lamp_Large 는 Y 폭 255 vs X 폭 65 = 팔이 Y 로 뻗음).
			// 세트 메시라 소켓 1개가 그 메시의 인스턴스 전부를 덮는다.
			TArray<FVector> HeadLocals;
			for (const UStaticMeshSocket* Sock : M->Sockets)
			{
				if (Sock && Sock->SocketName.ToString().StartsWith(TEXT("Lamp"), ESearchCase::IgnoreCase))
				{
					HeadLocals.Add(Sock->RelativeLocation);
				}
			}
			const bool bFromSocket = !HeadLocals.IsEmpty();
			if (!bFromSocket)
			{
				const FBox BB = M->GetBoundingBox();
				HeadLocals.Add(FVector(BB.GetCenter().X, BB.GetCenter().Y,
					BB.Min.Z + (BB.Max.Z - BB.Min.Z) * HeadHeightRatio));
			}
			UE_LOG(LogTemp, Verbose, TEXT("[LampSpot] %s: %s %d곳 x 인스턴스 %d"),
				*M->GetName(), bFromSocket ? TEXT("소켓") : TEXT("바운드추정"), HeadLocals.Num(), ISM->GetInstanceCount());

			const int32 Count = ISM->GetInstanceCount();
			for (int32 i = 0; i < Count; ++i)
			{
				FTransform InstW;
				if (!ISM->GetInstanceTransform(i, InstW, /*bWorldSpace=*/true)) { continue; }
				if (InstW.GetScale3D().IsNearlyZero()) { continue; }
				// 밑동 -> 머리의 수평 벡터가 팔 방향이다. 카드를 그 방향으로 밀고 그 축으로 늘린다.
				{
					const FVector Base = InstW.GetLocation();
					FVector Arm = InstW.TransformPosition(HeadLocals[0]) - Base;
					Arm.Z = 0.f;
					const FRotator ArmRot = Arm.IsNearlyZero() ? FRotator::ZeroRotator : FRotator(0.f, Arm.Rotation().Yaw, 0.f);
					Collected.CardAnchors.Add(FTransform(ArmRot, Base));
				}
				for (const FVector& Local : HeadLocals)
				{
					Collected.Heads.Add(InstW.TransformPosition(Local));
				}
			}
		}
	}
	if (!Collected.Heads.IsEmpty())
	{
		LevelLampData.Add(LampLevel, MoveTemp(Collected));
		bLampDataDirty = true;
	}
}

void AStreetLampSpotPool::RebuildLampData()
{
	bLampDataDirty = false;
	bAssignmentsDirty = true;
	LampHeads.Reset();
	LampCardAnchors.Reset();
	for (auto It = LevelLampData.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		LampHeads.Append(It.Value().Heads);
		LampCardAnchors.Append(It.Value().CardAnchors);
	}
	double SumZ = 0.0;
	for (const FVector& Head : LampHeads) { SumZ += Head.Z; }
	AverageLampHeadZ = LampHeads.IsEmpty() ? 0.0 : SumZ / LampHeads.Num();
	BuildPoolCards();
	EnsureLightPool();
	// 언로드된 가로등의 광원은 카메라를 아직 찾지 못했더라도 즉시 끈다.
	for (USpotLightComponent* Light : Pool)
	{
		if (Light) { Light->SetVisibility(false); }
	}
	if (bPoolEnabled && !Pool.IsEmpty()) { RefreshAssignments(); }
}

// Plane 은 XY 평면에 법선 +Z 라 회전 없이 이미 위를 본다. 반half 50 이므로 스케일 = 반경/50.
void AStreetLampSpotPool::BuildPoolCards()
{
	if (PoolISM) { PoolISM->ClearInstances(); }
	if (!bUsePoolCards || !PoolCardMesh || LampCardAnchors.IsEmpty()) { return; }
	if (!PoolISM)
	{
		PoolISM = NewObject<UInstancedStaticMeshComponent>(this);
		PoolISM->SetupAttachment(RootComponent);
		PoolISM->SetStaticMesh(PoolCardMesh);
		PoolISM->SetMobility(EComponentMobility::Movable);
		PoolISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PoolISM->SetCanEverAffectNavigation(false);
		PoolISM->SetCastShadow(false);
		PoolISM->SetVisibility(bPoolEnabled);
		PoolISM->RegisterComponent();
		// 공유 MI 를 고치지 않고 MID 로 색에만 배율을 곱한다(MainMap 무영향).
		if (PoolCardMaterial)
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PoolCardMaterial, this))
			{
				const FLinearColor Base = MID->K2_GetVectorParameterValue(TEXT("LightColor"));
				MID->SetVectorParameterValue(TEXT("LightColor"), Base * PoolCardBrightness);
				PoolISM->SetMaterial(0, MID);
			}
			else { PoolISM->SetMaterial(0, PoolCardMaterial); }
		}
	}

	// Plane 은 반half 50 이라 스케일 = 반경/50. 회전은 팔 방향 yaw 이므로 로컬 X 가 팔 축이다.
	const float S = FMath::Max(PoolCardRadius, 1.f) / 50.f;
	TArray<FTransform> CardTransforms;
	CardTransforms.Reserve(LampCardAnchors.Num());
	for (const FTransform& Anchor : LampCardAnchors)
	{
		const FVector Fwd = Anchor.GetRotation().GetForwardVector();
		FVector Loc = Anchor.GetLocation() + Fwd * PoolCardForwardOffset;
		Loc.Z = Anchor.GetLocation().Z + PoolCardZOffset;
		CardTransforms.Emplace(Anchor.GetRotation(), Loc, FVector(S * PoolCardAspect, S, 1.f));
	}
	PoolISM->AddInstances(CardTransforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true, /*bUpdateNavigation=*/false);
	PoolISM->SetVisibility(bPoolEnabled);
	UE_LOG(LogTemp, Log, TEXT("[LampSpot] 바닥 카드 %d장 (반경 %.0f / 종횡 %.2f / 전방 %.0f / 밝기 x%.2f)"),
		PoolISM->GetInstanceCount(), PoolCardRadius, PoolCardAspect, PoolCardForwardOffset, PoolCardBrightness);
}

void AStreetLampSpotPool::SetPoolEnabled(bool bEnabled)
{
	bPoolEnabled = bEnabled;
	bAssignmentsDirty = true;
	for (USpotLightComponent* L : Pool)
	{
		if (L && !bEnabled) { L->SetVisibility(false); }
	}
	if (PoolISM) { PoolISM->SetVisibility(bEnabled); }
	if (bEnabled && !Pool.IsEmpty()) { RefreshAssignments(); }
}

void AStreetLampSpotPool::RefreshAssignments()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StreetLamp_RefreshAssignments);
	const APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (!PCM) { return; }
	const FVector CamLoc = PCM->GetCameraLocation();
	const FRotator CamRotation = PCM->GetCameraRotation();
	if (!bAssignmentsDirty && CamLoc.Equals(LastAssignmentCameraLocation)
		&& CamRotation.Equals(LastAssignmentCameraRotation)
		&& MaxAssignDistance == LastAssignmentMaxDistance
		&& LightDropFromHead == LastAssignmentLightDrop)
	{
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(StreetLamp_SelectAssignments);
	const float MaxSq = FMath::Square(MaxAssignDistance);

	// 기준은 카메라 위치가 아니라 '화면 중앙이 보는 지점'이다. 이 카메라는 -47.5도 내려보므로
	// 위치상 가장 가까운 램프는 화면 밖(발밑)이고 플레이어가 보는 램프는 더 멀다. 위치 기준으로
	// 고르면 안 보이는 램프에 광원이 배정되고, 줌인해 둘이 수렴할 때만 켜진 것처럼 보인다.
	FVector Focus = CamLoc;
	{
		const double PlaneZ = LampHeads.IsEmpty() ? CamLoc.Z : AverageLampHeadZ;
		const FVector Fwd = CamRotation.Vector();
		if (FMath::Abs(Fwd.Z) > 1.e-3)
		{
			const double Hit = (PlaneZ - CamLoc.Z) / Fwd.Z;
			if (Hit > 0.0) { Focus = CamLoc + Fwd * Hit; }
		}
	}

	// 부분 선택: 559개에서 상위 N개만 뽑는다. 전체 정렬은 불필요하다.
	TArray<TPair<float, int32>> Near;
	Near.Reserve(Pool.Num() + 1);
	for (int32 i = 0; i < LampHeads.Num(); ++i)
	{
		const float D2 = FVector::DistSquared(Focus, LampHeads[i]);
		if (D2 > MaxSq) { continue; }
		if (Near.Num() < Pool.Num())
		{
			Near.Add(TPair<float, int32>(D2, i));
			if (Near.Num() == Pool.Num())
			{
				Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
			}
		}
		else if (D2 < Near.Last().Key)
		{
			Near.Last() = TPair<float, int32>(D2, i);
			Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		}
	}

	// [진단/임시] 어느 게이트가 광원을 막는지 런타임 값으로 가른다. 원인 확정되면 지운다.
	{
		static int32 DiagCount = 0;
		static double NextAt = 0.0;
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Now < 1.0 && DiagCount > 0) { DiagCount = 0; NextAt = 0.0; }
		if (DiagCount < 12 && Now >= NextAt)
		{
			++DiagCount; NextAt = Now + 2.0;
			double NearestSq = TNumericLimits<double>::Max();
			for (const FVector& H : LampHeads)
			{
				NearestSq = FMath::Min(NearestSq, FVector::DistSquared(Focus, H));
			}
			UE_LOG(LogTemp, Warning, TEXT("[LampSpotDiag] #%d t=%.1f cam=(%.0f,%.0f,%.0f) 시선=(%.0f,%.0f,%.0f) 캠-시선 %.0f / 램프 %d개 / 시선최근접 %.0f / 상한 %.0f / 후보 %d / 풀 %d / 밤=%d"),
				DiagCount, Now, CamLoc.X, CamLoc.Y, CamLoc.Z, Focus.X, Focus.Y, Focus.Z,
				FVector::Dist(CamLoc, Focus), LampHeads.Num(),
				FMath::Sqrt(NearestSq), MaxAssignDistance, Near.Num(), Pool.Num(), bPoolEnabled ? 1 : 0);
		}
	}

	for (int32 p = 0; p < Pool.Num(); ++p)
	{
		USpotLightComponent* L = Pool[p];
		if (!L) { continue; }
		if (!Near.IsValidIndex(p))
		{
			L->SetVisibility(false);
			continue;
		}
		FVector Loc = LampHeads[Near[p].Value];
		Loc.Z -= LightDropFromHead;
		L->SetWorldLocationAndRotation(Loc, FRotator(-90.f, 0.f, 0.f));
		L->SetVisibility(bPoolEnabled);
	}
	LastAssignmentCameraLocation = CamLoc;
	LastAssignmentCameraRotation = CamRotation;
	LastAssignmentMaxDistance = MaxAssignDistance;
	LastAssignmentLightDrop = LightDropFromHead;
	bAssignmentsDirty = false;
}

void AStreetLampSpotPool::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 같은 프레임의 여러 스트리밍 이벤트는 한 번의 인스턴스 업로드로 합친다.
	if (bLampDataDirty) { RebuildLampData(); }
	if (Pool.IsEmpty() && !PoolISM) { return; }

	// 시계는 WP 스트리밍으로 늦게 뜰 수 있다 - BeginPlay 1회 조회로는 영구히 null 로 남는다.
	if (!CachedTimeCycle.IsValid())
	{
		CachedTimeCycle = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass()));
	}

	bool bNight = true;   // 시계를 못 찾으면 표시 유지(밤 연출 회귀 방지)
	if (const ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		if (Clock->HasActorBegunPlay())
		{
			const int32 CurSec = Clock->GetVisualTime().ToSeconds();
			const int32 RiseSec = Clock->GetSunRiseTime().ToSeconds();
			const int32 SetSec = Clock->GetSunSetTime().ToSeconds();
			bNight = (CurSec >= SetSec) || (CurSec < RiseSec);
		}
	}
	if (bNight != bPoolEnabled) { SetPoolEnabled(bNight); }
	if (!bPoolEnabled) { return; }

	if (Pool.IsEmpty()) { return; }   // 카드만 쓰는 구성 - 광원 재배치 불필요
	Accum += DeltaSeconds;
	if (Accum < UpdateInterval) { return; }
	Accum = 0.f;
	RefreshAssignments();
}
