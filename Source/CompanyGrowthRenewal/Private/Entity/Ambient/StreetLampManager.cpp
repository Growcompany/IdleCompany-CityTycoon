// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/StreetLampManager.h"

#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "EngineUtils.h" // TActorIterator
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "TimeCycle/TimeCycleManager.h"

// 저사양 디바이스 프로파일에서 cg.StreetLamp.Enable 0 으로 가로등 글로우 전부 끔(과부하 완화).
static TAutoConsoleVariable<int32> CVarStreetLampEnable(
	TEXT("cg.StreetLamp.Enable"), 1,
	TEXT("0=street lamp glow off (low-end)."),
	ECVF_Scalability);

AStreetLampManager::AStreetLampManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 엔진 기본 평면(빌보드 카드 — 차 라이트와 동일 패턴). soft radial 카드. 머티리얼은 런타임 로드.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (MeshFinder.Succeeded())
	{
		GlowMesh = MeshFinder.Object;
	}
}

void AStreetLampManager::BeginPlay()
{
	Super::BeginPlay();
	RetriesLeft = MaxRetries;

	// 낮엔 가로등 글로우(additive)를 숨겨 fill 절감 — day/night 전환 구독. 못 찾으면 항상 표시.
	if (ATimeCycleManager* Clock = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass())))
	{
		CachedTimeCycle = Clock;
		Clock->OnSunRise.AddDynamic(this, &AStreetLampManager::HandleSunRise);
		Clock->OnSunSet.AddDynamic(this, &AStreetLampManager::HandleSunSet);
		const int32 Cur = Clock->GetCurrentTime().ToSeconds();
		const int32 Rise = Clock->GetSunRiseTime().ToSeconds();
		const int32 Set = Clock->GetSunSetTime().ToSeconds();
		bGlowVisible = (Cur >= Set) || (Cur < Rise);
	}

	BuildStreetLamps();
}

void AStreetLampManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		Clock->OnSunRise.RemoveDynamic(this, &AStreetLampManager::HandleSunRise);
		Clock->OnSunSet.RemoveDynamic(this, &AStreetLampManager::HandleSunSet);
	}
	Super::EndPlay(EndPlayReason);
}

void AStreetLampManager::ApplyNightVisibility(bool bNight)
{
	bGlowVisible = bNight;
	if (DetailISM)
	{
		DetailISM->SetVisibility(bNight, true);
	}
	if (LODISM)
	{
		LODISM->SetVisibility(bNight, true);
	}
	// 밤으로 켜질 땐 정지 게이트를 우회해 다음 틱에서 즉시 한 번 재페이싱/LOD 판정.
	if (bNight)
	{
		bBakedOnce = false;
	}
}

void AStreetLampManager::HandleSunRise() { ApplyNightVisibility(false); }
void AStreetLampManager::HandleSunSet()  { ApplyNightVisibility(true); }

UInstancedStaticMeshComponent* AStreetLampManager::MakeGlowISM()
{
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetupAttachment(RootComponent);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->RegisterComponent();
	ISM->SetStaticMesh(GlowMesh);
	if (GlowMaterial)
	{
		ISM->SetMaterial(0, GlowMaterial);
	}
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCanEverAffectNavigation(false);
	ISM->SetCastShadow(false);
	ISM->bUseAsOccluder = false;
	// 전경 맵 = 먼 가로등도 보여야 하므로 기본은 컬링 끔. End>0(저사양 프로파일)일 때만 컬.
	if (LampCullEndDistance > 0.f)
	{
		ISM->bNeverDistanceCull = false;
		ISM->SetCullDistances((int32)FMath::Max(0.f, LampCullStartDistance), (int32)LampCullEndDistance);
	}
	else
	{
		ISM->bNeverDistanceCull = true;
	}
	ISM->bAllowCullDistanceVolume = false; // 레벨 Cull Distance Volume 영향 차단
	return ISM;
}

void AStreetLampManager::BuildStreetLamps()
{
	// 저사양 킬스위치: 끄면 글로우 전부 비우고 중단.
	if (CVarStreetLampEnable.GetValueOnGameThread() == 0)
	{
		if (DetailISM) { DetailISM->ClearInstances(); }
		if (LODISM)    { LODISM->ClearInstances(); }
		return;
	}

	if (!GlowMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StreetLamp] GlowMesh 없음 — 중단"));
		return;
	}

	// 글로우 머티리얼 런타임 로드(에셋이 C++ 이후 생성되어도 재시작 불필요).
	if (!GlowMaterial)
	{
		GlowMaterial = TSoftObjectPtr<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Environment/Beacon/MI_StreetLampCard.MI_StreetLampCard"))).LoadSynchronous();
	}
	if (!GlowMaterial)
	{
		// loud-failure: 침묵하면 도시 전역에 불투명 회색 평면이 깔린다. 카드 MI 생성/쿠킹 등록 확인.
		UE_LOG(LogTemp, Warning, TEXT("[StreetLamp] MI_StreetLampCard 로드 실패 — 글로우 중단. 카드 MI 생성/쿠킹 등록 확인."));
		return;
	}

	// 인식할 가로등 메시 집합 해석.
	TSet<UStaticMesh*> LampSet;
	for (const TSoftObjectPtr<UStaticMesh>& Soft : LampMeshes)
	{
		if (Soft.IsNull())
			continue;
		if (UStaticMesh* SM = Soft.LoadSynchronous())
			LampSet.Add(SM);
	}
	if (LampSet.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StreetLamp] LampMeshes 미지정 — 중단"));
		return;
	}

	// 공유 ISM 2개 준비(없으면 생성). 재시도 대비 매번 ClearInstances.
	if (!DetailISM) { DetailISM = MakeGlowISM(); }
	if (!LODISM)    { LODISM = MakeGlowISM(); }
	DetailISM->ClearInstances();
	LODISM->ClearInstances();
	DetailPositions.Reset();
	DetailBaseScales.Reset();
	LODPositions.Reset();
	LODBaseScales.Reset();
	LampUnits.Reset();

	// 모든 액터의 ISM 컴포넌트 중 메시가 가로등인 것만(폴리지 HISM = ISM 손자라 그대로 잡힘).
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* LampActor = *It;
		if (!LampActor)
			continue;

		TArray<UInstancedStaticMeshComponent*> ISMComps;
		LampActor->GetComponents<UInstancedStaticMeshComponent>(ISMComps);
		for (UInstancedStaticMeshComponent* ISM : ISMComps)
		{
			if (!ISM)
				continue;
			UStaticMesh* SM = ISM->GetStaticMesh();
			if (!SM || !LampSet.Contains(SM))
				continue;

			// 소켓 분류: "LODLamp"(중앙 1장, 원거리) vs "Lamp*"(전구 5알, 근거리).
			// 주의: "LODLamp".StartsWith("Lamp")==false 라 LOD가 디테일로 오인되지 않음. LOD를 먼저 검사.
			TArray<FTransform> DetailSocketLocals;
			bool bHasLODSocket = false;
			FTransform LODSocketLocal;
			for (UStaticMeshSocket* Socket : SM->Sockets)
			{
				if (!Socket)
					continue;
				const FString SockName = Socket->SocketName.ToString();
				if (SockName.StartsWith(TEXT("LODLamp")))
				{
					LODSocketLocal = FTransform(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale);
					bHasLODSocket = true;
				}
				else if (SockName.StartsWith(TEXT("Lamp")))
				{
					DetailSocketLocals.Add(FTransform(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale));
				}
			}

			const int32 Count = ISM->GetInstanceCount();
			for (int32 Inst = 0; Inst < Count; ++Inst)
			{
				FTransform InstW;
				ISM->GetInstanceTransform(Inst, InstW, /*bWorldSpace=*/true);

				FLampUnit Unit;

				// 근거리 디테일 카드(소켓로컬 ∘ 인스턴스월드 — 항공등과 동일 합성 순서).
				for (const FTransform& SocketLocal : DetailSocketLocals)
				{
					FVector P = (SocketLocal * InstW).GetLocation();
					P.Z += LampZLift;
					const int32 Idx = DetailPositions.Add(P);
					DetailBaseScales.Add((SocketLocal * InstW).GetScale3D() * LampGlowScale);
					Unit.DetailInstances.Add(Idx);
				}

				// 원거리 중앙 LOD 카드(LODLamp 소켓). 없으면 디테일 중심을 거리 기준으로만 사용.
				if (bHasLODSocket)
				{
					const FTransform LODWorld = LODSocketLocal * InstW;
					FVector P = LODWorld.GetLocation();
					P.Z += LampZLift;
					const int32 Idx = LODPositions.Add(P);
					LODBaseScales.Add(LODWorld.GetScale3D() * LODGlowScale);
					Unit.LODInstance = Idx;
					Unit.Anchor = P;
				}
				else if (Unit.DetailInstances.Num() > 0)
				{
					FVector Sum = FVector::ZeroVector;
					for (int32 Idx : Unit.DetailInstances)
						Sum += DetailPositions[Idx];
					Unit.Anchor = Sum / Unit.DetailInstances.Num();
				}
				else
				{
					Unit.Anchor = InstW.GetLocation();
				}

				if (Unit.DetailInstances.Num() > 0 || Unit.LODInstance != INDEX_NONE)
				{
					LampUnits.Add(MoveTemp(Unit));
				}
			}
		}
	}

	if (DetailPositions.Num() == 0 && LODPositions.Num() == 0)
	{
		// LevelInstance 로드 전일 수 있음 — 재시도.
		if (RetriesLeft-- > 0 && RetryInterval > 0.f)
		{
			GetWorldTimerManager().SetTimer(RetryTimer, this, &AStreetLampManager::BuildStreetLamps, RetryInterval, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[StreetLamp] 가로등 인스턴스 0 — 폴리지/LampMeshes 확인"));
		}
		return;
	}

	// 초기 배치: 디테일은 기본 스케일로 보이게(첫 틱 전에도 보이게), LOD는 0스케일로 숨김 → 첫 틱이 거리 LOD 확정.
	for (int32 i = 0; i < DetailPositions.Num(); ++i)
	{
		DetailISM->AddInstance(FTransform(FQuat::Identity, DetailPositions[i], DetailBaseScales[i]), /*bWorldSpace=*/true);
	}
	for (int32 i = 0; i < LODPositions.Num(); ++i)
	{
		LODISM->AddInstance(FTransform(FQuat::Identity, LODPositions[i], FVector::ZeroVector), /*bWorldSpace=*/true);
	}

	LastCamLoc = FVector(TNumericLimits<float>::Max());
	bBakedOnce = false;
	AccumTime = 0.f;
	UE_LOG(LogTemp, Warning, TEXT("[StreetLamp] 완료 — 가로등 %d기 / 디테일 %d / LOD %d"),
		LampUnits.Num(), DetailPositions.Num(), LODPositions.Num());

	// 글로우가 재시도/지연 빌드되므로 빌드 완료 후 현재 day/night 상태를 다시 반영(낮이면 숨김).
	ApplyNightVisibility(bGlowVisible);
}

void AStreetLampManager::RefreshGlows(const FVector& CamLoc)
{
	const float RefD = FMath::Max(LampRefDistance, 1.f);
	const float SwitchSq = LODSwitchDistance * LODSwitchDistance;
	const float NearSq = FMath::Square(LODSwitchDistance * 0.9f); // 복귀 문턱 — 진입/복귀 10% 밴드 히스테리시스

	// 카드 위치 P 기준: 카메라를 바라보는 회전 + 거리 비례 스케일 팩터.
	auto FaceRot = [&](const FVector& P) -> FQuat
	{
		const FVector ToCam = CamLoc - P;
		return ToCam.IsNearlyZero() ? FQuat::Identity
			: FRotationMatrix::MakeFromZ(ToCam.GetSafeNormal()).ToQuat();
	};
	auto FactorAt = [&](const FVector& P) -> float
	{
		return FMath::Clamp((CamLoc - P).Size() / RefD, LampMinFactor, LampMaxGrow);
	};

	bool bDetailDirty = false;
	bool bLODDirty = false;

	for (FLampUnit& U : LampUnits)
	{
		const bool bHasDetail = U.DetailInstances.Num() > 0;
		// LOD 소켓이 있고 디테일도 있을 때만 거리로 스위치. (둘 중 하나만 있으면 그것만 항상 표시)
		if ((U.LODInstance != INDEX_NONE) && bHasDetail)
		{
			const float DistSq = (CamLoc - U.Anchor).SizeSquared();
			if (DistSq > SwitchSq)      { U.bFar = true; }
			else if (DistSq < NearSq)   { U.bFar = false; }
			// 밴드 안(0.9~1.0×Switch)에서는 이전 상태 유지
		}
		const bool bFar = (U.LODInstance != INDEX_NONE) && bHasDetail && U.bFar;

		const bool bDetailVisible = !bFar;
		for (int32 Idx : U.DetailInstances)
		{
			const FVector& P = DetailPositions[Idx];
			const FVector S = bDetailVisible ? DetailBaseScales[Idx] * FactorAt(P) : FVector::ZeroVector;
			DetailISM->UpdateInstanceTransform(Idx, FTransform(FaceRot(P), P, S), /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);
			bDetailDirty = true;
		}

		if (U.LODInstance != INDEX_NONE)
		{
			// LOD 카드: 원거리일 때, 또는 디테일이 아예 없는 가로등이면 항상 표시.
			const bool bLODVisible = bFar || !bHasDetail;
			const FVector& P = LODPositions[U.LODInstance];
			const FVector S = bLODVisible ? LODBaseScales[U.LODInstance] * FactorAt(P) : FVector::ZeroVector;
			LODISM->UpdateInstanceTransform(U.LODInstance, FTransform(FaceRot(P), P, S), /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);
			bLODDirty = true;
		}
	}

	if (bDetailDirty) { DetailISM->MarkRenderInstancesDirty(); }
	if (bLODDirty)    { LODISM->MarkRenderInstancesDirty(); }
}

void AStreetLampManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bGlowVisible || LampUnits.Num() == 0)
		return; // 낮(숨김) 또는 미빌드 → 갱신 불필요

	// [Perf] 스로틀: 매 프레임이 아니라 UpdateInterval 마다, 그리고 카메라가 충분히 움직였을 때만
	// 빌보드 페이싱+거리스케일+LOD를 재계산(인스턴스 버퍼 재업로드는 그때만 발생). 정지 시 비용 0.
	AccumTime += DeltaSeconds;
	if (AccumTime < LampUpdateInterval)
		return;
	AccumTime = 0.f;

	APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!PCM)
		return; // 카메라 아직 → 다음 틱 재시도

	const FVector CamLoc = PCM->GetCameraLocation();
	if (bBakedOnce && (CamLoc - LastCamLoc).SizeSquared() < LampMoveThreshold * LampMoveThreshold)
		return; // 카메라 거의 정지 → 스킵

	LastCamLoc = CamLoc;
	bBakedOnce = true;
	RefreshGlows(CamLoc);
}
