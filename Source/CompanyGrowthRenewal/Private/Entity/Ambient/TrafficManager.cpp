// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/TrafficManager.h"

#include "Entity/Ambient/TrafficRoute.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/EntityManager.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Enum/CompanyType.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "TimeCycle/TimeCycleManager.h"

// 저사양 디바이스 프로파일용 트래픽 차량 수 상한(0=무제한). 예: Android_Low 에서 +CVars=cg.Traffic.MaxCars=20.
static TAutoConsoleVariable<int32> CVarTrafficMaxCars(
	TEXT("cg.Traffic.MaxCars"), 0,
	TEXT("트래픽 차량 수 상한. 0=무제한(TotalCars 그대로), >0=상한."),
	ECVF_Default);

ATrafficManager::ATrafficManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// 차 모델 7종 기본 로드 (ConstructorHelpers 는 static 이어야 하므로 개별 선언).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M0(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_Sport.SM_Vehicle_Sport"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M1(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_Classic.SM_Vehicle_Classic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M2(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_Hatchback.SM_Vehicle_Hatchback"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M3(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_Van.SM_Vehicle_Van"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M4(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_Muscle.SM_Vehicle_Muscle"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M5(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_Pickup.SM_Vehicle_Pickup"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M6(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Meshes/SM_Vehicle_MonsterTruck.SM_Vehicle_MonsterTruck"));
	UStaticMesh* Models[] = { M0.Object, M1.Object, M2.Object, M3.Object, M4.Object, M5.Object, M6.Object };
	for (UStaticMesh* M : Models)
	{
		if (M) { CarModels.Add(M); }
	}

	// 라이트카드 메시 = 엔진 Plane, 머티리얼 = MI_CarLight_*
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) { LightCardMesh = Plane.Object; }
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Head(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Materials/MI_CarLight_Head.MI_CarLight_Head"));
	if (Head.Succeeded()) { HeadLightMaterial = Head.Object; }
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Tail(TEXT("/Game/CompanyGrowth/Environment/AmbientVehicles/Materials/MI_CarLight_Tail.MI_CarLight_Tail"));
	if (Tail.Succeeded()) { TailLightMaterial = Tail.Object; }
}

void ATrafficManager::BeginPlay()
{
	Super::BeginPlay();
	RefreshBuildingLocations();

	// 낮엔 헤드/테일 라이트 카드(additive)를 숨겨 fill 절감 — day/night 전환 구독. 못 찾으면 항상 표시.
	if (ATimeCycleManager* Clock = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass())))
	{
		CachedTimeCycle = Clock;
		Clock->OnSunRise.AddDynamic(this, &ATrafficManager::HandleSunRise);
		Clock->OnSunSet.AddDynamic(this, &ATrafficManager::HandleSunSet);
		const int32 Cur = Clock->GetCurrentTime().ToSeconds();
		const int32 Rise = Clock->GetSunRiseTime().ToSeconds();
		const int32 Set = Clock->GetSunSetTime().ToSeconds();
		bGlowVisible = (Cur >= Set) || (Cur < Rise);
	}

	if (GraphNodes.Num() >= 2 && GraphEdges.Num() >= 1)
	{
		bGraphMode = true;
		BuildGraphTraffic();
	}
	else
	{
		BuildTraffic();
	}

	// 라이트 ISM 생성(CreateInstanceComponents) 후 현재 day/night 상태 반영(낮이면 숨김).
	ApplyNightVisibility(bGlowVisible);
}

void ATrafficManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		Clock->OnSunRise.RemoveDynamic(this, &ATrafficManager::HandleSunRise);
		Clock->OnSunSet.RemoveDynamic(this, &ATrafficManager::HandleSunSet);
	}
	Super::EndPlay(EndPlayReason);
}

void ATrafficManager::ApplyNightVisibility(bool bNight)
{
	// 차체는 낮에도 다니므로 건드리지 않고, 발광 라이트 카드 ISM만 토글. 빌딩반경 인스턴스 게이팅과 독립.
	bGlowVisible = bNight;
	if (FrontLightISM) { FrontLightISM->SetVisibility(bNight, true); }
	if (RearLightISM)  { RearLightISM->SetVisibility(bNight, true); }
}

void ATrafficManager::HandleSunRise() { ApplyNightVisibility(false); }
void ATrafficManager::HandleSunSet()  { ApplyNightVisibility(true); }

void ATrafficManager::BuildTraffic()
{
	// 레벨의 모든 ATrafficRoute 수집
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATrafficRoute::StaticClass(), Found);

	Routes.Reset();
	RouteReverse.Reset();
	for (AActor* A : Found)
	{
		ATrafficRoute* R = Cast<ATrafficRoute>(A);
		if (R && R->GetSpline() && R->GetSpline()->GetSplineLength() > KINDA_SMALL_NUMBER)
		{
			Routes.Add(R->GetSpline());
			RouteReverse.Add(R->bReverse);
		}
	}

	if (Routes.Num() == 0 || CarModels.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrafficManager] 경로(ATrafficRoute %d) 또는 차 모델(%d) 없음 → 비활성화."),
			Routes.Num(), CarModels.Num());
		SetActorTickEnabled(false);
		return;
	}

	CreateInstanceComponents();

	// 차량 분배 (경로 라운드로빈 + 모델/속도/시작거리 결정적 분산). 저사양 상한(cg.Traffic.MaxCars) 적용.
	const int32 MaxCars = CVarTrafficMaxCars.GetValueOnGameThread();
	const int32 EffectiveCars = (MaxCars > 0) ? FMath::Min(TotalCars, MaxCars) : TotalCars;
	Cars.Reset();
	Cars.Reserve(EffectiveCars);
	for (int32 i = 0; i < EffectiveCars; ++i)
	{
		FTrafficCar Car;
		Car.RouteIndex = i % Routes.Num();
		Car.bReverse = RouteReverse[Car.RouteIndex];
		Car.RouteLength = Routes[Car.RouteIndex]->GetSplineLength();
		Car.ModelIndex = i % CarModels.Num();
		Car.Speed = FMath::Lerp(MinSpeed, MaxSpeed, FMath::Frac(i * 0.6180339887f));   // 황금비 저불일치 분산
		Car.Distance = Car.RouteLength * FMath::Frac(i * 0.3819660113f);
		Car.Yaw = 0.f;
		Car.BodyInstance = BodyISM[Car.ModelIndex]->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
		if (ModelHeadlights.IsValidIndex(Car.ModelIndex))
		{
			for (int32 h = 0; h < ModelHeadlights[Car.ModelIndex].Num(); ++h)
			{
				Car.HeadInst.Add(FrontLightISM->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector)));
			}
		}
		if (ModelTaillights.IsValidIndex(Car.ModelIndex))
		{
			for (int32 t = 0; t < ModelTaillights[Car.ModelIndex].Num(); ++t)
			{
				Car.TailInst.Add(RearLightISM->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector)));
			}
		}
		Cars.Add(Car);
		UpdateCar(Cars.Last(), 0.f);
	}

	for (UInstancedStaticMeshComponent* I : BodyISM) { if (I) { I->MarkRenderStateDirty(); } }
	if (FrontLightISM) { FrontLightISM->MarkRenderStateDirty(); }
	if (RearLightISM) { RearLightISM->MarkRenderStateDirty(); }

	UE_LOG(LogTemp, Log, TEXT("[TrafficManager] %d 대 분배 (경로 %d / 모델 %d / 빌딩 %d)"),
		Cars.Num(), Routes.Num(), CarModels.Num(), BuildingLocations.Num());
}

void ATrafficManager::RefreshBuildingLocations()
{
	BuildingLocations.Reset();

	// 1) 플레이어가 지은 산업 빌딩(업종 배정됨)
	if (UEntityManager* EM = GetWorld() ? GetWorld()->GetSubsystem<UEntityManager>() : nullptr)
	{
		const TArray<ABuildingBaseActor*>& Bs = EM->GetBuildings();
		for (ABuildingBaseActor* B : Bs)
		{
			if (B && B->GetCompanyType() != ECompanyType::None)
			{
				BuildingLocations.Add(B->GetActorLocation());
			}
		}
	}

	// 2) 장식 스카이라인 빌딩(BP_MB*) — 정적(레벨 배치, 안 움직임). [Perf] 매 3s 전체 액터 스캔 대신 1회만 수집해 캐시.
	//    BP_MB 파생은 AActor 라 타입캐스팅 불가 → 클래스명 접두사 매칭.
	if (bIncludeCityBuildings && !CityBuildingClassPrefix.IsEmpty() && GetWorld())
	{
		if (!bCityCacheBuilt)
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				AActor* A = *It;
				if (A && A->GetClass() && A->GetClass()->GetName().StartsWith(CityBuildingClassPrefix))
				{
					StaticCityBuildingLocations.Add(A->GetActorLocation());
				}
			}
			bCityCacheBuilt = true;
		}
		BuildingLocations.Append(StaticCityBuildingLocations);
	}
}

bool ATrafficManager::IsNearBuilding(const FVector& WorldLoc, float Radius) const
{
	const float R2 = Radius * Radius;
	for (const FVector& B : BuildingLocations)
	{
		// 평면(XY) 거리 — 높이 차이 무시
		const float dx = B.X - WorldLoc.X;
		const float dy = B.Y - WorldLoc.Y;
		if (dx * dx + dy * dy <= R2)
		{
			return true;
		}
	}
	return false;
}

void ATrafficManager::UpdateCar(FTrafficCar& Car, float DeltaTime)
{
	USplineComponent* S = Routes.IsValidIndex(Car.RouteIndex) ? Routes[Car.RouteIndex] : nullptr;
	const float Len = Car.RouteLength;
	if (!S || Len <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (DeltaTime > 0.f)
	{
		Car.Distance += Car.Speed * DeltaTime;
	}
	Car.Distance = FMath::Fmod(Car.Distance, Len);
	if (Car.Distance < 0.f) { Car.Distance += Len; }

	const float Sign = Car.bReverse ? -1.f : 1.f;
	const float SampleDist = Car.bReverse ? (Len - Car.Distance) : Car.Distance;
	const FVector Loc = S->GetLocationAtDistanceAlongSpline(SampleDist, ESplineCoordinateSpace::World);

	// 룩어헤드: 폐쇄 루프면 wrap, 비폐쇄(직선)면 끝점으로 clamp.
	// 비폐쇄에서 fmod 로 감으면 끝점 부근 AheadLoc 이 시작점으로 점프 → Dir 뒤집힘(~180) → 차가 축 회전.
	float AheadD = SampleDist + Sign * LookAheadDistance;
	if (S->IsClosedLoop())
	{
		AheadD = FMath::Fmod(AheadD, Len);
		if (AheadD < 0.f) { AheadD += Len; }
	}
	else
	{
		AheadD = FMath::Clamp(AheadD, 0.f, Len);
	}
	const FVector AheadLoc = S->GetLocationAtDistanceAlongSpline(AheadD, ESplineCoordinateSpace::World);

	FVector Dir = AheadLoc - Loc;
	Dir.Z = 0.f;
	float TargetYaw;
	if (Dir.SizeSquared() > 10.f)
	{
		TargetYaw = Dir.Rotation().Yaw;
	}
	else
	{
		// 룩어헤드가 끝점에 수렴(끝 도달)하면 스플라인 탄젠트로 폴백. 역방향은 +180 보정.
		TargetYaw = S->GetRotationAtDistanceAlongSpline(SampleDist, ESplineCoordinateSpace::World).Yaw;
		if (Car.bReverse) { TargetYaw = FRotator::NormalizeAxis(TargetYaw + 180.f); }
	}

	if (DeltaTime <= 0.f)
	{
		Car.Yaw = TargetYaw;
	}
	else
	{
		const float d = FMath::FindDeltaAngleDegrees(Car.Yaw, TargetYaw);
		const float MaxStep = MaxYawRate * DeltaTime;
		Car.Yaw += FMath::Clamp(d, -MaxStep, MaxStep);
	}
	Car.Yaw = FRotator::NormalizeAxis(Car.Yaw);

	// 빌딩 근처 아니면 숨김(디버그 시 무시). 보이는 차는 1.1배 반경까지 유지 — 경계를 스치는 차의 점멸 방지(히스테리시스).
	const bool bNear = bDebugIgnoreBuildingGating || IsNearBuilding(Loc, BuildingActivationRadius * (Car.bVisible ? 1.1f : 1.f));
	const bool bWas = Car.bVisible;
	ApplyCarInstances(Car.ModelIndex, Car.BodyInstance, Car.HeadInst, Car.TailInst, Loc, Car.Yaw, bNear, bWas);
	Car.bVisible = bNear;
}

void ATrafficManager::CreateInstanceComponents()
{
	// 모델별 바닥 보정값(-BoundsMin.Z): 차 바닥이 경로 Z 에 오도록 들어올림.
	ModelGroundLift.Reset();
	for (UStaticMesh* M : CarModels)
	{
		const FBox BB = M ? M->GetBoundingBox() : FBox(ForceInit);
		ModelGroundLift.Add(M ? -BB.Min.Z : 0.f);
	}

	// 모델별 라이트 위치: 차 메시 "Color Bloom" 발광면에서 추출한 로컬좌표(헤드=앞+Y 흰색, 테일=뒤-Y 빨강, 좌우 ±X 대칭).
	//   Blender 로 7개 차 FBX 의 발광면 클러스터를 각 차의 UE bbox 에 매핑해 베이크(2026-06-23). 자세한 건 메모리 참조.
	auto MakeLR = [](float X, float Y, float Z) { return TArray<FVector>{ FVector(-X, Y, Z), FVector(X, Y, Z) }; };
	struct FLightBake { const TCHAR* Name; float HX, HY, HZ, TX, TY, TZ; };
	static const FLightBake Bakes[] = {
		{ TEXT("SM_Vehicle_Sport"),        69.f, 184.f,  58.f,  60.f, -190.f,  78.f },
		{ TEXT("SM_Vehicle_Classic"),      73.f, 189.f,  76.f,  72.f, -222.f,  74.f },
		{ TEXT("SM_Vehicle_Hatchback"),    64.f, 176.f,  71.f,  65.f, -175.f,  87.f },
		{ TEXT("SM_Vehicle_Van"),          63.f, 206.f,  74.f,  79.f, -225.f,  82.f },
		{ TEXT("SM_Vehicle_Muscle"),       60.f, 194.f,  66.f,  54.f, -197.f,  71.f },
		{ TEXT("SM_Vehicle_Pickup"),       80.f, 236.f,  72.f,  92.f, -262.f,  87.f },
		{ TEXT("SM_Vehicle_MonsterTruck"), 81.f, 243.f, 156.f,  94.f, -239.f, 161.f },
	};
	ModelHeadlights.Reset();
	ModelTaillights.Reset();
	for (UStaticMesh* M : CarModels)
	{
		TArray<FVector> Heads, Tails;
		bool bBaked = false;
		if (M)
		{
			const FString Nm = M->GetName();
			for (const FLightBake& Bk : Bakes)
			{
				if (Nm == Bk.Name)
				{
					Heads = MakeLR(Bk.HX, Bk.HY, Bk.HZ);
					Tails = MakeLR(Bk.TX, Bk.TY, Bk.TZ);
					bBaked = true;
					break;
				}
			}
			if (!bBaked)
			{
				// 폴백(베이크 없는 새 모델): bbox 추정 — 앞/뒤 범퍼 안쪽, ±35% 폭, 높이 40%(앞)/55%(뒤).
				const FBox BB = M->GetBoundingBox();
				const float HalfW = (BB.Max.X - BB.Min.X) * 0.35f;
				const float Hgt = BB.Max.Z - BB.Min.Z;
				const float LenY = BB.Max.Y - BB.Min.Y;
				Heads = MakeLR(HalfW, BB.Max.Y - LenY * 0.08f, BB.Min.Z + Hgt * 0.40f);
				Tails = MakeLR(HalfW, BB.Min.Y + LenY * 0.05f, BB.Min.Z + Hgt * 0.55f);
			}
		}
		ModelHeadlights.Add(Heads);
		ModelTaillights.Add(Tails);
	}

	// 모델당 ISM 생성 (회색 방지: SetStaticMesh→Register→SetMaterial 명시)
	BodyISM.Reset();
	for (UStaticMesh* M : CarModels)
	{
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetupAttachment(Root);
		if (M) { ISM->SetStaticMesh(M); }
		ISM->SetMobility(EComponentMobility::Movable);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCastShadow(false);
		ISM->RegisterComponent();
		if (M)
		{
			for (int32 SlotIdx = 0; SlotIdx < M->GetStaticMaterials().Num(); ++SlotIdx)
			{
				if (UMaterialInterface* SlotMat = M->GetMaterial(SlotIdx))
				{
					ISM->SetMaterial(SlotIdx, SlotMat);
				}
			}
		}
		BodyISM.Add(ISM);
	}

	auto MakeLightISM = [&](UMaterialInterface* Mat) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetupAttachment(Root);
		if (LightCardMesh) { ISM->SetStaticMesh(LightCardMesh); }
		ISM->SetMobility(EComponentMobility::Movable);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCastShadow(false);
		ISM->RegisterComponent();
		if (Mat) { ISM->SetMaterial(0, Mat); }
		return ISM;
	};
	FrontLightISM = MakeLightISM(HeadLightMaterial);
	RearLightISM = MakeLightISM(TailLightMaterial);
}

void ATrafficManager::ApplyCarInstances(int32 ModelIdx, int32 BodyInst, const TArray<int32>& HeadInsts, const TArray<int32>& TailInsts, const FVector& WorldLoc, float WorldYaw, bool bVisible, bool bWasVisible)
{
	// 숨김→숨김: 인스턴스가 이미 scale 0 → 트랜스폼 재기록 스킵(인스턴스 버퍼 churn 제거). 안 보이므로 위치 변화 무관.
	if (!bVisible && !bWasVisible)
	{
		return;
	}
	const float BodyScale = bVisible ? 1.f : 0.f;
	const float LightScale = bVisible ? LightCardScale : 0.f;

	FVector BodyLoc = WorldLoc;
	BodyLoc.Z += (ModelGroundLift.IsValidIndex(ModelIdx) ? ModelGroundLift[ModelIdx] : 0.f) + VehicleZOffset;

	const FRotator BodyRot(0.f, WorldYaw + MeshYawOffset.Yaw, 0.f);
	const FTransform BodyFull(BodyRot, BodyLoc, FVector::OneVector);
	const FTransform BodyRender(BodyRot, BodyLoc, FVector(BodyScale));

	if (BodyISM.IsValidIndex(ModelIdx) && BodyISM[ModelIdx])
	{
		BodyISM[ModelIdx]->UpdateInstanceTransform(BodyInst, BodyRender, true, false, true);
	}

	// 발광면 위치를 월드로 변환 후 '카메라 향함' 빌보드 → 라디얼 글로우가 어느 각도서든 동그랗게 퍼져 보임.
	// 위치/기본스케일은 매프레임 재조준 패스(RefreshLightBillboards)용 캐시에도 기록.
	auto WriteCard = [&](UInstancedStaticMeshComponent* ISM, int32 Inst, const FVector& WP,
	                     TArray<FVector>& PosCache, TArray<float>& BaseCache)
	{
		if (PosCache.Num() <= Inst)
		{
			PosCache.SetNumZeroed(Inst + 1);
			BaseCache.SetNumZeroed(Inst + 1);
		}
		PosCache[Inst] = WP;
		BaseCache[Inst] = LightScale;
		const FVector ToCamV = CachedCameraLoc - WP;
		const FVector Dir = ToCamV.GetSafeNormal();
		const FRotator Rot = ToCamV.IsNearlyZero() ? FRotator::ZeroRotator : FRotationMatrix::MakeFromZ(Dir).Rotator();
		const float WS = LightScale * FMath::Clamp(ToCamV.Size() / FMath::Max(LightRefDistance, 1.f), LightMinFactor, LightMaxGrow);
		// CamPush: 카드가 차체 표면에 있어 스침각에서 절반이 차체에 파묻혀 z-테스트로 잘리는 깜빡임 방지.
		ISM->UpdateInstanceTransform(Inst, FTransform(Rot, WP + Dir * LightCardCamPush, FVector(WS)), true, false, true);
	};

	if (FrontLightISM && ModelHeadlights.IsValidIndex(ModelIdx))
	{
		const TArray<FVector>& Heads = ModelHeadlights[ModelIdx];
		for (int32 i = 0; i < HeadInsts.Num() && i < Heads.Num(); ++i)
		{
			WriteCard(FrontLightISM, HeadInsts[i], BodyFull.TransformPosition(Heads[i]), FrontCardPos, FrontCardBase);
		}
	}
	if (RearLightISM && ModelTaillights.IsValidIndex(ModelIdx))
	{
		const TArray<FVector>& Tails = ModelTaillights[ModelIdx];
		for (int32 i = 0; i < TailInsts.Num() && i < Tails.Num(); ++i)
		{
			WriteCard(RearLightISM, TailInsts[i], BodyFull.TransformPosition(Tails[i]), RearCardPos, RearCardBase);
		}
	}
}

void ATrafficManager::RefreshLightBillboards()
{
	// 시뮬(30Hz)과 독립 — 카메라가 움직인 프레임에만 라이트 카드의 회전·거리스케일을 재계산.
	// 위치는 시뮬 스텝이 캐시에 써둔 값 그대로(차량은 30Hz 스텝 이동 유지). 정지 시 비용 0.
	if (!bGlowVisible || (FrontCardPos.Num() == 0 && RearCardPos.Num() == 0))
	{
		return;
	}
	APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (!PCM)
	{
		return;
	}
	const FVector CamLoc = PCM->GetCameraLocation();
	if (FVector::DistSquared(CamLoc, LastLightCamLoc) < FMath::Square(LightMoveThreshold))
	{
		return;
	}
	LastLightCamLoc = CamLoc;
	CachedCameraLoc = CamLoc;

	auto RefreshISM = [&](UInstancedStaticMeshComponent* ISM, const TArray<FVector>& PosCache, const TArray<float>& BaseCache)
	{
		if (!ISM)
		{
			return;
		}
		bool bAny = false;
		for (int32 i = 0; i < PosCache.Num(); ++i)
		{
			if (BaseCache[i] <= 0.f)
			{
				continue; // 게이팅으로 숨겨진 카드 — 시뮬 스텝이 다시 보이게 할 때까지 스킵
			}
			const FVector ToCamV = CamLoc - PosCache[i];
			const FVector Dir = ToCamV.GetSafeNormal();
			const FRotator Rot = ToCamV.IsNearlyZero() ? FRotator::ZeroRotator : FRotationMatrix::MakeFromZ(Dir).Rotator();
			const float WS = BaseCache[i] * FMath::Clamp(ToCamV.Size() / FMath::Max(LightRefDistance, 1.f), LightMinFactor, LightMaxGrow);
			ISM->UpdateInstanceTransform(i, FTransform(Rot, PosCache[i] + Dir * LightCardCamPush, FVector(WS)), true, false, true);
			bAny = true;
		}
		if (bAny)
		{
			ISM->MarkRenderInstancesDirty();
		}
	};
	RefreshISM(FrontLightISM, FrontCardPos, FrontCardBase);
	RefreshISM(RearLightISM, RearCardPos, RearCardBase);
}

int32 ATrafficManager::PickNextNode(int32 FromNode, int32 AtNode, uint32& Rng) const
{
	if (!Adjacency.IsValidIndex(AtNode) || Adjacency[AtNode].Num() == 0)
	{
		return FromNode;
	}
	const TArray<int32>& Nb = Adjacency[AtNode];
	Rng = Rng * 1664525u + 1013904223u;
	// U턴(되돌아가기) 제외 이웃 중 랜덤 선택
	int32 Count = 0;
	for (int32 N : Nb) { if (N != FromNode) { ++Count; } }
	if (Count > 0)
	{
		int32 Pick = (int32)(Rng % (uint32)Count);
		for (int32 N : Nb)
		{
			if (N != FromNode)
			{
				if (Pick == 0) { return N; }
				--Pick;
			}
		}
	}
	return FromNode; // 막다른 길 → U턴
}

void ATrafficManager::BuildGraphTraffic()
{
	if (CarModels.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrafficManager] GRAPH: 차 모델 0 → 비활성화."));
		SetActorTickEnabled(false);
		return;
	}

	// 인접 리스트 구성(양방향)
	Adjacency.Reset();
	Adjacency.SetNum(GraphNodes.Num());
	for (const FIntPoint& E : GraphEdges)
	{
		if (GraphNodes.IsValidIndex(E.X) && GraphNodes.IsValidIndex(E.Y) && E.X != E.Y)
		{
			Adjacency[E.X].AddUnique(E.Y);
			Adjacency[E.Y].AddUnique(E.X);
		}
	}
	TArray<int32> StartNodes;
	for (int32 N = 0; N < Adjacency.Num(); ++N)
	{
		if (Adjacency[N].Num() > 0) { StartNodes.Add(N); }
	}
	if (StartNodes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrafficManager] GRAPH: 유효 노드 0 → 비활성화."));
		SetActorTickEnabled(false);
		return;
	}

	CreateInstanceComponents();

	// 시작 엣지 목록: 일반=TotalCars 랜덤 분배 / 디버그=모든 엣지 양방향(전 도로 커버 테스트)
	TArray<TPair<int32, int32>> Starts;
	if (bDebugFillAllEdges)
	{
		for (const FIntPoint& E : GraphEdges)
		{
			if (GraphNodes.IsValidIndex(E.X) && GraphNodes.IsValidIndex(E.Y))
			{
				Starts.Emplace(E.X, E.Y);
				Starts.Emplace(E.Y, E.X);   // 반대방향도 1대
			}
		}
	}
	else
	{
		const int32 MaxCars = CVarTrafficMaxCars.GetValueOnGameThread();
		const int32 EffectiveCars = (MaxCars > 0) ? FMath::Min(TotalCars, MaxCars) : TotalCars;
		for (int32 i = 0; i < EffectiveCars; ++i)
		{
			const int32 From = StartNodes[i % StartNodes.Num()];
			uint32 R = (uint32)(i * 2654435761u) + 12345u;
			R = R * 1664525u + 1013904223u;
			const int32 To = Adjacency[From][R % (uint32)Adjacency[From].Num()];
			Starts.Emplace(From, To);
		}
	}

	GCars.Reset();
	GCars.Reserve(Starts.Num());
	for (int32 i = 0; i < Starts.Num(); ++i)
	{
		FGraphCar Car;
		Car.Rng = (uint32)(i * 2654435761u) + 6791u;
		Car.FromNode = Starts[i].Key;
		Car.ToNode = Starts[i].Value;
		Car.NextNode = PickNextNode(Car.FromNode, Car.ToNode, Car.Rng);   // 회전 예측용 다음다음 노드
		Car.EdgeLen = FMath::Max(FVector::Dist2D(GraphNodes[Car.FromNode], GraphNodes[Car.ToNode]), 1.f);
		Car.Speed = FMath::Lerp(MinSpeed, MaxSpeed, FMath::Frac(i * 0.6180339887f));
		Car.Dist = Car.EdgeLen * (bDebugFillAllEdges ? 0.5f : FMath::Frac(i * 0.3819660113f));
		Car.LaneOff = LaneOffset + (((i % 2) == 0) ? 0.f : LaneWidth);   // 안쪽/바깥쪽 차선 교대
		Car.ModelIndex = i % CarModels.Num();
		Car.BodyInstance = BodyISM[Car.ModelIndex]->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
		if (ModelHeadlights.IsValidIndex(Car.ModelIndex))
		{
			for (int32 h = 0; h < ModelHeadlights[Car.ModelIndex].Num(); ++h)
			{
				Car.HeadInst.Add(FrontLightISM->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector)));
			}
		}
		if (ModelTaillights.IsValidIndex(Car.ModelIndex))
		{
			for (int32 t = 0; t < ModelTaillights[Car.ModelIndex].Num(); ++t)
			{
				Car.TailInst.Add(RearLightISM->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector)));
			}
		}
		GCars.Add(Car);
		UpdateGraphCar(GCars.Last(), 0.f);
	}

	for (UInstancedStaticMeshComponent* I : BodyISM) { if (I) { I->MarkRenderStateDirty(); } }
	if (FrontLightISM) { FrontLightISM->MarkRenderStateDirty(); }
	if (RearLightISM) { RearLightISM->MarkRenderStateDirty(); }

	UE_LOG(LogTemp, Log, TEXT("[TrafficManager] GRAPH %d 대 (노드 %d / 엣지 %d / 빌딩 %d)"),
		GCars.Num(), GraphNodes.Num(), GraphEdges.Num(), BuildingLocations.Num());
}

void ATrafficManager::UpdateGraphCar(FGraphCar& Car, float DeltaTime)
{
	if (!GraphNodes.IsValidIndex(Car.FromNode) || !GraphNodes.IsValidIndex(Car.ToNode) || !GraphNodes.IsValidIndex(Car.NextNode))
	{
		return;
	}

	if (DeltaTime > 0.f)
	{
		// car-following: 앞차와 MinCarGap 이내로 좁혀지면 전진 제한(겹침 방지). SpacingGap 은 Tick 에서 계산.
		const float MaxAdvance = FMath::Max(0.f, Car.SpacingGap - MinCarGap);
		Car.Dist += FMath::Clamp(Car.Speed * DeltaTime, 0.f, MaxAdvance);
	}

	// 엣지 끝 도달 → 다음 엣지로. 다음다음(NextNode)은 미리 선택돼 있어 회전을 예측할 수 있음.
	int32 Guard = 0;
	while (Car.Dist >= Car.EdgeLen && Guard++ < 8)
	{
		Car.Dist -= Car.EdgeLen;
		Car.FromNode = Car.ToNode;
		Car.ToNode = Car.NextNode;
		Car.NextNode = PickNextNode(Car.FromNode, Car.ToNode, Car.Rng);
		Car.EdgeLen = FMath::Max(FVector::Dist2D(GraphNodes[Car.FromNode], GraphNodes[Car.ToNode]), 1.f);
	}

	const FVector A = GraphNodes[Car.FromNode];
	const FVector B = GraphNodes[Car.ToNode];
	const FVector C = GraphNodes[Car.NextNode];
	const float T = FMath::Clamp(Car.Dist / Car.EdgeLen, 0.f, 1.f);
	const FVector Base = FMath::Lerp(A, B, T);

	// 룩어헤드: 현재 엣지를 넘으면 다음 엣지(B→C)로 이어 봐 교차로 전에 미리 선회 시작(휙 회전 방지).
	const float AheadDist = Car.Dist + LookAheadDistance;
	FVector LookPt;
	if (AheadDist <= Car.EdgeLen)
	{
		LookPt = FMath::Lerp(A, B, AheadDist / Car.EdgeLen);
	}
	else
	{
		const float NextLen = FMath::Max(FVector::Dist2D(B, C), 1.f);
		const float Over = FMath::Min(AheadDist - Car.EdgeLen, NextLen);
		LookPt = FMath::Lerp(B, C, Over / NextLen);
	}

	FVector Dir = LookPt - Base;
	Dir.Z = 0.f;
	if (Dir.SizeSquared() < 1.f) { Dir = B - A; Dir.Z = 0.f; }
	if (Dir.SizeSquared() < 1.f) { Dir = FVector(1.f, 0.f, 0.f); }
	Dir.Normalize();
	const float TargetYaw = Dir.Rotation().Yaw;

	if (DeltaTime <= 0.f)
	{
		Car.Yaw = TargetYaw;
	}
	else
	{
		const float d = FMath::FindDeltaAngleDegrees(Car.Yaw, TargetYaw);
		const float MaxStep = MaxYawRate * DeltaTime;
		Car.Yaw += FMath::Clamp(d, -MaxStep, MaxStep);
	}
	Car.Yaw = FRotator::NormalizeAxis(Car.Yaw);

	// 차선 오프셋은 '부드럽게 도는 헤딩' 기준 → 교차로에서 위치 팝 없이 자연스럽게 휨(우측통행, 차별 안/바깥 차선).
	const float YawRad = FMath::DegreesToRadians(Car.Yaw);
	const FVector HeadingDir(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f);
	const FVector Right(-HeadingDir.Y, HeadingDir.X, 0.f);
	const FVector Loc = Base + Right * Car.LaneOff;

	// 빌딩 근처 아니면 숨김(디버그 시 무시). 보이는 차는 1.1배 반경까지 유지 — 경계 점멸 방지(히스테리시스).
	const bool bNear = bDebugIgnoreBuildingGating || IsNearBuilding(Loc, BuildingActivationRadius * (Car.bVisible ? 1.1f : 1.f));
	ApplyCarInstances(Car.ModelIndex, Car.BodyInstance, Car.HeadInst, Car.TailInst, Loc, Car.Yaw, bNear, Car.bVisible);
	Car.bVisible = bNear;
}

void ATrafficManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 시뮬 스로틀과 독립적으로 매 프레임: 카메라가 움직였으면 라이트 카드 재조준(30Hz 빌보드 스냅 = 이동 시 차 라이트 깜빡임 제거).
	RefreshLightBillboards();

	// 시뮬을 SimUpdateInterval(기본 30Hz)로 스로틀 — 스로틀된 프레임은 정렬/차량 갱신/인스턴스 업로드/RHI 플러시를 전부 건너뜀. 누적 델타로 갱신해 차 속도는 동일.
	SimAccum += DeltaTime;
	if (SimAccum < SimUpdateInterval)
	{
		return;
	}
	const float SimDelta = SimAccum;
	SimAccum = 0.f;

	// 라이트 빌보드용 카메라 위치 캐시(라이트마다 GetPlayerCameraManager 호출 방지).
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
	{
		CachedCameraLoc = PCM->GetCameraLocation();
	}

	RefreshAccum += SimDelta;
	if (RefreshAccum >= BuildingRefreshInterval)
	{
		RefreshAccum = 0.f;
		RefreshBuildingLocations();
		if (bDebugLog)
		{
			const int32 Total = bGraphMode ? GCars.Num() : Cars.Num();
			UE_LOG(LogTemp, Warning, TEXT("[TrafficManager] %s %d대, 빌딩 %d, 노드%d/엣지%d/경로%d (게이팅무시=%d)"),
				bGraphMode ? TEXT("GRAPH") : TEXT("ROUTE"), Total, BuildingLocations.Num(),
				GraphNodes.Num(), GraphEdges.Num(), Routes.Num(), bDebugIgnoreBuildingGating ? 1 : 0);
		}
	}

	if (bGraphMode)
	{
		// car-following: (엣지,차선,거리)로 정렬 후 인접한 앞차와의 간격만 계산(O(n log n)) → 겹침 방지.
		GCars.Sort([this](const FGraphCar& A, const FGraphCar& B)
		{
			if (A.FromNode != B.FromNode) { return A.FromNode < B.FromNode; }
			if (A.ToNode != B.ToNode) { return A.ToNode < B.ToNode; }
			const int32 La = (A.LaneOff > LaneOffset + LaneWidth * 0.5f) ? 1 : 0;
			const int32 Lb = (B.LaneOff > LaneOffset + LaneWidth * 0.5f) ? 1 : 0;
			if (La != Lb) { return La < Lb; }
			return A.Dist < B.Dist;
		});
		for (int32 i = 0; i < GCars.Num(); ++i)
		{
			GCars[i].SpacingGap = 1e9f;
			if (i + 1 < GCars.Num())
			{
				const FGraphCar& A = GCars[i];
				const FGraphCar& Bc = GCars[i + 1];
				const int32 La = (A.LaneOff > LaneOffset + LaneWidth * 0.5f) ? 1 : 0;
				const int32 Lb = (Bc.LaneOff > LaneOffset + LaneWidth * 0.5f) ? 1 : 0;
				if (Bc.FromNode == A.FromNode && Bc.ToNode == A.ToNode && La == Lb)
				{
					GCars[i].SpacingGap = Bc.Dist - A.Dist;
				}
			}
		}
		for (FGraphCar& Car : GCars) { UpdateGraphCar(Car, SimDelta); }
	}
	else
	{
		for (FTrafficCar& Car : Cars) { UpdateCar(Car, SimDelta); }
	}

	// [디버그] 도로 그래프 오버레이: 엣지=시안 선, 노드=노란 점 (도로 위로 150 올려 가시성).
	if (bDebugDrawGraph && GraphNodes.Num() > 0 && GetWorld())
	{
		// 오버레이를 차와 같은 Z 보정(VehicleZOffset)으로 띄움 → 한 노브로 그래프+차가 함께 도로면에 올라감.
		const FVector Up(0.f, 0.f, VehicleZOffset + 30.f);
		for (const FIntPoint& E : GraphEdges)
		{
			if (GraphNodes.IsValidIndex(E.X) && GraphNodes.IsValidIndex(E.Y))
			{
				DrawDebugLine(GetWorld(), GraphNodes[E.X] + Up, GraphNodes[E.Y] + Up, FColor::Cyan, false, -1.f, 0, 40.f);
			}
		}
		for (const FVector& N : GraphNodes)
		{
			DrawDebugPoint(GetWorld(), N + Up, 35.f, FColor::Yellow, false, -1.f);
		}
	}

	// 인스턴스 버퍼 갱신 반영(컴포넌트당 1회) — 프록시 재생성 없이 GPU-Scene 인스턴스 델타만 플러시(떨림 방지).
	for (UInstancedStaticMeshComponent* I : BodyISM) { if (I) { I->MarkRenderInstancesDirty(); } }
	if (FrontLightISM) { FrontLightISM->MarkRenderInstancesDirty(); }
	if (RearLightISM) { RearLightISM->MarkRenderInstancesDirty(); }
}
