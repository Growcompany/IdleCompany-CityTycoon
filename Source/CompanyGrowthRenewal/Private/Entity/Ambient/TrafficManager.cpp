// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Ambient/TrafficManager.h"

#include "Entity/Ambient/TrafficRoute.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "StaticMeshResources.h"
#include "HAL/IConsoleManager.h"
#include "TimeCycle/TimeCycleManager.h"

// 저사양 디바이스 프로파일용 트래픽 차량 수 상한(0=무제한). 예: Android_Low 에서 +CVars=cg.Traffic.MaxCars=20.
static TAutoConsoleVariable<int32> CVarTrafficMaxCars(
	TEXT("cg.Traffic.MaxCars"), 0,
	TEXT("트래픽 차량 수 상한. 0=무제한(TotalCars 그대로), >0=상한."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarTrafficMinUpdateInterval(
	TEXT("cg.Traffic.MinUpdateInterval"), 0.f,
	TEXT("트래픽 시뮬레이션 최소 갱신 간격(초). 0=액터 설정 유지, >0=SimUpdateInterval과 큰 값 사용. 다음 Tick부터 적용."),
	ECVF_Default);

// 기기 A/B 용(-ForceDPCVars=cg.Traffic.CameraPool=0). 끄면 재활용만 멈추고 카메라 반경 게이팅은 유지.
static TAutoConsoleVariable<int32> CVarTrafficCameraPool(
	TEXT("cg.Traffic.CameraPool"), 1,
	TEXT("트래픽 카메라 풀 재활용. 1=카메라에서 멀어진 숨김 차를 화면 밖 스폰 밴드로 재배치, 0=끄기(게이팅은 유지)."),
	ECVF_Default);

// 기기 티어별 차량 수 배율 — 줌 규칙의 Min/MaxActiveCars 와 풀 크기에 곱한다(디바이스 프로파일: Low 1.0 / Mid 1.5 / High 2.0).
static TAutoConsoleVariable<float> CVarTrafficActiveCarsScale(
	TEXT("cg.Traffic.ActiveCarsScale"), 1.f,
	TEXT("트래픽 화면 표시 대수 배율. MinActiveCars~MaxActiveCars 와 풀 크기에 곱한다. 1=액터 설정 그대로."),
	ECVF_Default);

static float GetActiveCarsScale()
{
	return FMath::Clamp(CVarTrafficActiveCarsScale.GetValueOnGameThread(), 0.1f, 10.f);
}

// 기기 A/B 용 — 줌 규칙 대신 목표 대수를 고정한다.
static TAutoConsoleVariable<int32> CVarTrafficActiveCarsOverride(
	TEXT("cg.Traffic.ActiveCarsOverride"), 0,
	TEXT("트래픽 목표 대수 고정(그래프=화면 표시 대수, 루트=활성 대수). 0=줌 규칙(MinActiveCars~MaxActiveCars), >0=이 대수(풀 크기로 잘림)."),
	ECVF_Default);

namespace
{
// 발광 카드 회전: Z = 카메라 방향, Y 를 월드업 쪽으로 잡아 X 가 수평이 되게 고정한다.
// MakeFromZ 는 X/Y 를 임의로 고르므로 비균일 스케일(타원)이 카메라 이동에 따라 돌아간다.
// 시선이 수직에 가까우면 Y 기준이 퇴화하니 그때만 폴백.
FRotator CardRotation(const FVector& ToCamDir)
{
	if (FMath::Abs(ToCamDir.Z) > 0.995f)
	{
		return FRotationMatrix::MakeFromZ(ToCamDir).Rotator();
	}
	return FRotationMatrix::MakeFromZY(ToCamDir, FVector::UpVector).Rotator();
}

template <typename TCar>
int32 CountActiveTrafficCars(const TArray<TCar>& InCars)
{
	int32 Count = 0;
	for (const TCar& Car : InCars) { Count += Car.bActive ? 1 : 0; }
	return Count;
}

// 숨김(0스케일) 활성 차만, 포커스에서 먼 차부터 — 보이는 차를 끄면 눈앞에서 사라진다.
template <typename TCar>
int32 PickHiddenTrafficCarToDeactivate(const TArray<TCar>& InCars, const FVector& Focus)
{
	int32 Pick = INDEX_NONE;
	double PickDistSq = -1.0;
	for (int32 Idx = 0; Idx < InCars.Num(); ++Idx)
	{
		const TCar& Car = InCars[Idx];
		if (!Car.bActive || Car.bVisible)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(Car.LastLoc, Focus);
		if (DistSq > PickDistSq)
		{
			Pick = Idx;
			PickDistSq = DistSq;
		}
	}
	return Pick;
}

}


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
	// 첫 배치(0초 UpdateGraphCar/UpdateCar)부터 카메라 반경·줌 활성 대수로 판정. 카메라를 아직 못 찾으면 줌인 대수로 시작.
	DesiredActiveCars = static_cast<float>(MinActiveCars) * GetActiveCarsScale();
	UpdateFocus();

	// 낮엔 헤드/테일 라이트 카드(additive)를 숨겨 fill 절감 — day/night 전환 구독. 못 찾으면 항상 표시.
	if (ATimeCycleManager* Clock = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass())))
	{
		CachedTimeCycle = Clock;
		Clock->OnVisualTimeChanged.AddDynamic(this, &ATrafficManager::HandleVisualTimeChanged);
		const int32 Cur = Clock->GetVisualTime().ToSeconds();
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
		Clock->OnVisualTimeChanged.RemoveDynamic(this, &ATrafficManager::HandleVisualTimeChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ATrafficManager::ApplyNightVisibility(bool bNight)
{
	// 차체는 낮에도 다니므로 건드리지 않고, 발광 라이트 카드 ISM만 토글. 카메라 반경 인스턴스 게이팅과 독립.
	if (bNight && !bGlowVisible)
	{
		bGlowVisible = true;
		LastLightCamLoc = FVector(TNumericLimits<float>::Max());
		RefreshLightBillboards(true);
	}
	bGlowVisible = bNight;
	if (FrontLightISM) { FrontLightISM->SetVisibility(bNight, true); }
	if (RearLightISM)  { RearLightISM->SetVisibility(bNight, true); }
}

void ATrafficManager::HandleVisualTimeChanged()
{
	if (const ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		ApplyNightVisibility(Clock->IsVisualNight());
	}
}

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
	const int32 PoolNeed = FMath::RoundToInt(MaxActiveCars * GetActiveCarsScale()) + TransitAllowance;   // 풀은 화면 목표 최대 + 대기 여유분이면 충분 — 그 이상은 숨긴 채 정점만 소모한다
	const int32 EffectiveCars = FMath::Min((MaxCars > 0) ? FMath::Min(TotalCars, MaxCars) : TotalCars, PoolNeed);
	Cars.Reset();
	Cars.Reserve(EffectiveCars);
	const int32 InitialActive = FMath::Clamp(FMath::RoundToInt(DesiredActiveCars), 0, EffectiveCars);
	for (int32 i = 0; i < EffectiveCars; ++i)
	{
		FTrafficCar Car;
		Car.bActive = i < InitialActive;
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
		if (Cars.Last().bActive) { UpdateCar(Cars.Last(), 0.f); }
	}

	for (UInstancedStaticMeshComponent* I : BodyISM) { if (I) { I->MarkRenderStateDirty(); } }
	if (FrontLightISM) { FrontLightISM->MarkRenderStateDirty(); }
	if (RearLightISM) { RearLightISM->MarkRenderStateDirty(); }

	UE_LOG(LogTemp, Log, TEXT("[TrafficManager] %d 대 분배 (경로 %d / 모델 %d)"),
		Cars.Num(), Routes.Num(), CarModels.Num());
}

void ATrafficManager::UpdateFocus()
{
	// 활성 대수 제어는 표시 반경(원)이 아니라 실제 화면 안의 차를 센다 — 근접 줌에서 원은 화면 자취의 약 4배 넓이.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	FSceneViewProjectionData ProjData;
	bHasViewProj = LP && LP->ViewportClient && LP->GetProjectionData(LP->ViewportClient->Viewport, ProjData);
	if (bHasViewProj)
	{
		ViewProj = ProjData.ComputeViewProjectionMatrix();
	}

	const APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!PCM)
	{
		bHasFocus = false;
		return;
	}
	const FVector CamLoc = PCM->GetCameraLocation();
	// 이 게임의 폰은 카메라 리그의 주시점(=화면 중심)이다. 폰이 없으면 카메라 바로 아래, 높이는 직전 포커스를 유지.
	const APawn* FocusPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	FocusLoc = FocusPawn ? FocusPawn->GetActorLocation() : FVector(CamLoc.X, CamLoc.Y, FocusLoc.Z);
	const float CamFocusDist = static_cast<float>(FVector::Dist(CamLoc, FocusLoc));
	ViewRadius = CamFocusDist * ViewRadiusScale;
	const int32 OverrideCount = CVarTrafficActiveCarsOverride.GetValueOnGameThread();
	const float ZoomAlpha = FMath::Clamp((CamFocusDist - ZoomNearDistance) / FMath::Max(ZoomFarDistance - ZoomNearDistance, 1.f), 0.f, 1.f);
	DesiredActiveCars = (OverrideCount > 0) ? static_cast<float>(OverrideCount)
		: FMath::Lerp(static_cast<float>(MinActiveCars), static_cast<float>(MaxActiveCars), ZoomAlpha) * GetActiveCarsScale();
	bHasFocus = true;
}

bool ATrafficManager::IsInView(const FVector& WorldLoc, bool bWasVisible) const
{
	if (bDebugIgnoreBuildingGating)
	{
		return true;
	}
	if (!bHasFocus)
	{
		return false;
	}
	// 보이던 차는 1.1배 반경까지 유지 — 경계를 스치는 차의 점멸 방지(히스테리시스).
	const float GateRadius = ViewRadius * (bWasVisible ? 1.1f : 1.f);
	return FVector::DistSquared2D(WorldLoc, FocusLoc) < FMath::Square(GateRadius);
}

bool ATrafficManager::IsOnScreen(const FVector& WorldLoc) const
{
	// 뷰포트가 없으면(에디터 미리보기·테스트 월드) 표시 반경 판정으로 대신한다.
	if (!bHasViewProj)
	{
		return true;
	}
	const FVector4 Clip = ViewProj.TransformFVector4(FVector4(WorldLoc, 1.f));
	return Clip.W > 0.f && FMath::Abs(Clip.X) <= Clip.W && FMath::Abs(Clip.Y) <= Clip.W;
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

	// 경사 추종(그래프 모드와 동일 규약)
	{
		const FVector Rise = AheadLoc - Loc;
		const float Horiz = FMath::Max(FVector(Rise.X, Rise.Y, 0.f).Size(), 1.f);
		const float TargetPitch = FMath::RadiansToDegrees(FMath::Atan2(Rise.Z, Horiz));
		if (DeltaTime <= 0.f) { Car.Pitch = TargetPitch; }
		else
		{
			const float dp = FMath::FindDeltaAngleDegrees(Car.Pitch, TargetPitch);
			const float MaxPStep = MaxPitchRate * DeltaTime;
			Car.Pitch += FMath::Clamp(dp, -MaxPStep, MaxPStep);
		}
	}
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

	const bool bInView = IsInView(Loc, Car.bVisible);
	ApplyCarInstances(Car.ModelIndex, Car.BodyInstance, Car.HeadInst, Car.TailInst, Loc, Car.Yaw, bInView, Car.bVisible, Car.Pitch);
	Car.bVisible = bInView;
	Car.LastLoc = Loc;
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
	for (int32 Mi = 0; Mi < CarModels.Num(); ++Mi)
	{
		UStaticMesh* M = CarModels[Mi];
		TArray<FVector> Heads, Tails;
		bool bBaked = false;
		if (M)
		{
			const FString Nm = M->GetName();

			// 1순위 = 소켓. 램프 하우징에 직접 찍은 위치가 bbox 추정이나 베이크 표보다 정확하다.
			// 이름 앞글자로 판정(Head*/Tail*, 대소문자 무시, 개수 무관) - 좌우 2개가 표준이고 더 달아도 그대로 쓴다.
			for (const UStaticMeshSocket* Sock : M->Sockets)
			{
				if (!Sock) { continue; }
				const FString SockName = Sock->SocketName.ToString();
				if (SockName.StartsWith(TEXT("Head"), ESearchCase::IgnoreCase)) { Heads.Add(Sock->RelativeLocation); }
				else if (SockName.StartsWith(TEXT("Tail"), ESearchCase::IgnoreCase)) { Tails.Add(Sock->RelativeLocation); }
			}
			if (Heads.Num() > 0 || Tails.Num() > 0)
			{
				bBaked = true;
				UE_LOG(LogTemp, Warning, TEXT("[Traffic] 라이트 소켓 %s: 헤드 %d / 테일 %d"), *Nm, Heads.Num(), Tails.Num());
			}

			if (!bBaked)
			{
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
			}
			if (!bBaked)
			{
				// 폴백: 길이축을 bbox 로 판별한다 - 에셋 팩마다 차를 X 축으로 눕히기도 한다(City_Pack).
				// 코 방향은 ModelYawOffsets 로 역산: 180 보정을 받는 모델은 메시의 코가 길이축 음방향이다.
				// 라이트 로컬좌표는 BodyFull 로 변환되므로 메시의 실제 코에 놓으면 월드 앞쪽에 선다.
				const FBox BB = M->GetBoundingBox();
				const float dX = BB.Max.X - BB.Min.X;
				const float dY = BB.Max.Y - BB.Min.Y;
				const float Hgt = BB.Max.Z - BB.Min.Z;
				const bool bLenAlongX = dX >= dY;
				const float Len = bLenAlongX ? dX : dY;
				const float Wid = bLenAlongX ? dY : dX;
				const float HalfW = Wid * 0.35f;
				const float LenMin = bLenAlongX ? BB.Min.X : BB.Min.Y;
				const float LenMax = bLenAlongX ? BB.Max.X : BB.Max.Y;
				const float YawFix = ModelYawOffsets.IsValidIndex(Mi) ? ModelYawOffsets[Mi] : 0.f;
				const bool bNoseNeg = FMath::Abs(FRotator::NormalizeAxis(YawFix)) > 90.f;
				const float NoseAt = bNoseNeg ? (LenMin + Len * 0.08f) : (LenMax - Len * 0.08f);
				const float TailAt = bNoseNeg ? (LenMax - Len * 0.05f) : (LenMin + Len * 0.05f);
				const float HeadZ = BB.Min.Z + Hgt * 0.40f;
				const float TailZ = BB.Min.Z + Hgt * 0.55f;
				if (bLenAlongX)
				{
					Heads = { FVector(NoseAt, -HalfW, HeadZ), FVector(NoseAt, HalfW, HeadZ) };
					Tails = { FVector(TailAt, -HalfW, TailZ), FVector(TailAt, HalfW, TailZ) };
				}
				else
				{
					Heads = MakeLR(HalfW, NoseAt, HeadZ);
					Tails = MakeLR(HalfW, TailAt, TailZ);
				}
				UE_LOG(LogTemp, Warning,
					TEXT("[Traffic] 라이트 폴백 %s: 길이축=%s 코=%s Len=%.0f Wid=%.0f Head=(%.0f,%.0f,%.0f)"),
					*Nm, bLenAlongX ? TEXT("X") : TEXT("Y"), bNoseNeg ? TEXT("-") : TEXT("+"),
					Len, Wid, Heads[1].X, Heads[1].Y, Heads[1].Z);
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

void ATrafficManager::ApplyCarInstances(int32 ModelIdx, int32 BodyInst, const TArray<int32>& HeadInsts, const TArray<int32>& TailInsts, const FVector& WorldLoc, float WorldYaw, bool bVisible, bool bWasVisible, float WorldPitch, float WorldRoll)
{
	// 숨김→숨김: 이미 scale 0 → 트랜스폼 재기록 스킵 (버퍼 churn 제거)
	if (!bVisible && !bWasVisible)
	{
		return;
	}
	// 숨김 = 파괴 아닌 스케일 0 (인스턴스 인덱스 유지, 재등장 비용 0)
	const float BodyScale = bVisible ? 1.f : 0.f;
	const float LightScale = bVisible ? LightCardScale : 0.f;

	FVector BodyLoc = WorldLoc;
	BodyLoc.Z += (ModelGroundLift.IsValidIndex(ModelIdx) ? ModelGroundLift[ModelIdx] : 0.f) + VehicleZOffset;

	// MeshYawOffset 을 메시 로컬에서 먼저 적용한 뒤 진행방향(pitch+yaw)으로 돌린다.
	// FRotator(P,Y,0) 의 Z 는 Yaw 와 무관하게 sin(P) 라, 180 뒤집힌 메시에 pitch 를 그냥 넣으면 오르막에서 코를 박는다.
	// 롤/피치는 진행 방향 프레임에 건다. MeshQ 의 180도 요는 up 벡터를 안 바꾸므로 모델별 부호 보정이 필요 없다.
	const FQuat TravelQ = FRotator(WorldPitch, WorldYaw, WorldRoll).Quaternion();
	const float ModelYaw = ModelYawOffsets.IsValidIndex(ModelIdx) ? ModelYawOffsets[ModelIdx] : 0.f;
	const FQuat MeshQ = FRotator(0.f, MeshYawOffset.Yaw + ModelYaw, 0.f).Quaternion();
	const FRotator BodyRot = (TravelQ * MeshQ).Rotator();
	const FTransform BodyFull(BodyRot, BodyLoc, FVector::OneVector);
	const FTransform BodyRender(BodyRot, BodyLoc, FVector(BodyScale));

	if (BodyISM.IsValidIndex(ModelIdx) && BodyISM[ModelIdx])
	{
		BodyISM[ModelIdx]->UpdateInstanceTransform(BodyInst, BodyRender, true, false, true);
	}

	// 발광면 월드 위치 + 카메라 향한 빌보드 → 어느 각도서든 원형 글로우
	// 위치·기본 스케일은 재조준 패스(RefreshLightBillboards) 캐시에도 기록
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
		if (!bGlowVisible)
		{
			return;
		}
		const FVector ToCamV = CachedCameraLoc - WP;
		const FVector Dir = ToCamV.GetSafeNormal();
		// MakeFromZ 는 X/Y 를 엔진이 임의로 고른다 - 비균일 스케일을 걸면 타원이 카메라 이동에 따라 돌아간다.
		// MakeFromZY(월드업) 이면 Y 가 수평으로 고정된다(실측 - X 아님). 그 Y 를 늘려 가로 타원.
		const FRotator Rot = ToCamV.IsNearlyZero() ? FRotator::ZeroRotator : CardRotation(Dir);
		// 화면상 크기 일정 범위 유지 (멀어져 픽셀 줄면 글로우 점멸)
		const float WS = LightScale * FMath::Clamp(ToCamV.Size() / FMath::Max(LightRefDistance, 1.f), LightMinFactor, LightMaxGrow);
		// CamPush: 스침각에서 카드 절반이 차체에 묻혀 z-테스트로 잘리는 깜빡임 방지
		ISM->UpdateInstanceTransform(Inst, FTransform(Rot, WP + Dir * LightCardCamPush, FVector(WS, WS * LightCardAspect, WS)), true, false, true);
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

void ATrafficManager::RefreshLightBillboards(bool bRefreshHidden)
{
	// 시뮬(30Hz)과 독립 — 카메라가 움직인 프레임에만 라이트 카드의 회전·거리스케일을 재계산.
	// 위치는 시뮬 스텝이 캐시에 써둔 값 그대로(차량은 30Hz 스텝 이동 유지). 정지 시 비용 0.
	if (!bGlowVisible || (FrontCardPos.Num() == 0 && RearCardPos.Num() == 0))
	{
		return;
	}
	APlayerCameraManager* PCM = GetWorld() ? UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0) : nullptr;
	if (!PCM && !bRefreshHidden)
	{
		return;
	}
	const FVector CamLoc = PCM ? PCM->GetCameraLocation() : CachedCameraLoc;
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
				if (bRefreshHidden)
				{
					ISM->UpdateInstanceTransform(i, FTransform(FQuat::Identity, PosCache[i], FVector::ZeroVector), true, false, true);
					bAny = true;
				}
				continue; // 게이팅으로 숨겨진 카드는 카메라 이동만으로 다시 보이지 않는다.
			}
			const FVector ToCamV = CamLoc - PosCache[i];
			const FVector Dir = ToCamV.GetSafeNormal();
			const FRotator Rot = ToCamV.IsNearlyZero() ? FRotator::ZeroRotator : CardRotation(Dir);
			const float WS = BaseCache[i] * FMath::Clamp(ToCamV.Size() / FMath::Max(LightRefDistance, 1.f), LightMinFactor, LightMaxGrow);
			ISM->UpdateInstanceTransform(i, FTransform(Rot, PosCache[i] + Dir * LightCardCamPush, FVector(WS, WS * LightCardAspect, WS)), true, false, true);
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
		const int32 PoolNeed = FMath::RoundToInt(MaxActiveCars * GetActiveCarsScale()) + TransitAllowance;   // 풀은 화면 목표 최대 + 대기 여유분이면 충분 — 그 이상은 숨긴 채 정점만 소모한다
		const int32 EffectiveCars = FMath::Min((MaxCars > 0) ? FMath::Min(TotalCars, MaxCars) : TotalCars, PoolNeed);
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
	const int32 InitialActive = FMath::Clamp(FMath::RoundToInt(DesiredActiveCars), 0, Starts.Num());
	for (int32 i = 0; i < Starts.Num(); ++i)
	{
		FGraphCar Car;
		// 전 도로 커버 테스트는 전 차량 활성, 그 외엔 앞에서부터 줌 비례 대수만.
		Car.bActive = bDebugFillAllEdges || i < InitialActive;
		Car.Rng = (uint32)(i * 2654435761u) + 6791u;
		Car.FromNode = Starts[i].Key;
		Car.ToNode = Starts[i].Value;
		Car.NextNode = PickNextNode(Car.FromNode, Car.ToNode, Car.Rng);   // 회전 예측용 다음다음 노드
		Car.EdgeLen = FMath::Max(FVector::Dist2D(GraphNodes[Car.FromNode], GraphNodes[Car.ToNode]), 1.f);
		Car.CruiseSpeed = FMath::Lerp(MinSpeed, MaxSpeed, FMath::Frac(i * 0.6180339887f));
		Car.Speed = Car.CruiseSpeed;
		Car.PrevNode = Car.FromNode;
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
		if (GCars.Last().bActive) { UpdateGraphCar(GCars.Last(), 0.f); }
	}

	for (UInstancedStaticMeshComponent* I : BodyISM) { if (I) { I->MarkRenderStateDirty(); } }
	if (FrontLightISM) { FrontLightISM->MarkRenderStateDirty(); }
	if (RearLightISM) { RearLightISM->MarkRenderStateDirty(); }

	UE_LOG(LogTemp, Log, TEXT("[TrafficManager] GRAPH %d 대 (노드 %d / 엣지 %d)"),
		GCars.Num(), GraphNodes.Num(), GraphEdges.Num());
}

void ATrafficManager::UpdateGraphCar(FGraphCar& Car, float DeltaTime)
{
	if (!GraphNodes.IsValidIndex(Car.FromNode) || !GraphNodes.IsValidIndex(Car.ToNode) || !GraphNodes.IsValidIndex(Car.NextNode))
	{
		return;
	}

	// 유턴의 반원은 노드 앞뒤 동일한 거리 구간에 걸쳐 진행한다.
	const float UTurnTrim = FMath::Min(CornerRadius, Car.EdgeLen * 0.45f);
	const bool bApproachingUTurn = Car.NextNode == Car.FromNode;
	const bool bLeavingUTurn = Car.PrevNode == Car.ToNode && Car.PrevNode != Car.FromNode && Car.Dist < UTurnTrim;
	const bool bHasUTurn = UTurnTrim > 1.f && (bApproachingUTurn || bLeavingUTurn);
	const float UTurnAxisRatio = FMath::Min(1.f, UTurnTrim / FMath::Max(Car.LaneOff, 1.f));
	const float UTurnSpeed = 2.f * UTurnTrim * FMath::DegreesToRadians(MaxYawRate) * UTurnAxisRatio / PI;
	const float DistanceToUTurn = bLeavingUTurn ? 0.f : Car.EdgeLen - UTurnTrim - Car.Dist;

	// 목표 속도를 코너 선회각과 앞차 간격으로 깎고, 실제 속도는 가/감속 한계로 따라가게 한다.
	// 전진량만 클램프하면(아래 MaxAdvance) 속도가 아니라 위치가 잘려 멈칫거림으로 보인다.
	if (Car.CruiseSpeed <= 0.f) { Car.CruiseSpeed = Car.Speed; }
	{
		const FVector DirAB = (GraphNodes[Car.ToNode] - GraphNodes[Car.FromNode]).GetSafeNormal2D();
		const FVector DirBC = (GraphNodes[Car.NextNode] - GraphNodes[Car.ToNode]).GetSafeNormal2D();
		float TargetSpeed = Car.CruiseSpeed;

		if (Car.EdgeLen - Car.Dist < CornerBrakeDistance && !DirAB.IsNearlyZero() && !DirBC.IsNearlyZero())
		{
			const float TurnAmt = FMath::Clamp(1.f - FVector::DotProduct(DirAB, DirBC), 0.f, 1.f);
			TargetSpeed *= FMath::Lerp(1.f, CornerSpeedFactor, TurnAmt);
		}

		if (bHasUTurn)
		{
			const float BrakeDistance = FMath::Max(0.f, (Car.Speed * Car.Speed - UTurnSpeed * UTurnSpeed) / (2.f * MaxBrake));
			if (DistanceToUTurn <= FMath::Max(CornerBrakeDistance, BrakeDistance))
			{
				TargetSpeed = FMath::Min(TargetSpeed, UTurnSpeed);
			}
		}

		const float Gap = Car.SpacingGap - MinCarGap;
		if (Gap < FollowSlowDistance)
		{
			TargetSpeed = FMath::Min(TargetSpeed, Car.CruiseSpeed * FMath::Clamp(Gap / FollowSlowDistance, 0.f, 1.f));
		}

		if (DeltaTime <= 0.f) { Car.Speed = TargetSpeed; }
		else
		{
			const float dv = TargetSpeed - Car.Speed;
			const float MaxDv = ((dv > 0.f) ? MaxAccel : MaxBrake) * DeltaTime;
			Car.Speed = FMath::Max(0.f, Car.Speed + FMath::Clamp(dv, -MaxDv, MaxDv));
		}
	}

	if (DeltaTime > 0.f)
	{
		// 겹침 안전망: 속도 제어가 못 따라간 프레임에도 앞차를 파고들지 않게 전진량을 자른다.
		float MaxAdvance = FMath::Max(0.f, Car.SpacingGap - MinCarGap);
		if (bHasUTurn)
		{
			// 감속 중 유턴에 진입해도 경로의 각변화가 차체 선회 한계를 넘지 않게 한다.
			MaxAdvance = FMath::Min(MaxAdvance, FMath::Max(0.f, DistanceToUTurn) + UTurnSpeed * DeltaTime);
		}
		Car.Dist += FMath::Clamp(Car.Speed * DeltaTime, 0.f, MaxAdvance);
	}

	// 엣지 끝 도달 → 다음 엣지로. 다음다음(NextNode)은 미리 선택돼 있어 회전을 예측할 수 있음.
	int32 Guard = 0;
	while (Car.Dist >= Car.EdgeLen && Guard++ < 8)
	{
		Car.Dist -= Car.EdgeLen;
		Car.PrevNode = Car.FromNode;
		Car.FromNode = Car.ToNode;
		Car.ToNode = Car.NextNode;
		Car.NextNode = PickNextNode(Car.FromNode, Car.ToNode, Car.Rng);
		Car.EdgeLen = FMath::Max(FVector::Dist2D(GraphNodes[Car.FromNode], GraphNodes[Car.ToNode]), 1.f);
	}

	const FVector A = GraphNodes[Car.FromNode];
	const FVector B = GraphNodes[Car.ToNode];
	const FVector C = GraphNodes[Car.NextNode];
	const float T = FMath::Clamp(Car.Dist / Car.EdgeLen, 0.f, 1.f);
	FVector Base = FMath::Lerp(A, B, T);

	// 코너 둥글리기: 노드 앞뒤 R 구간을 제어점이 노드인 2차 베지어로 잇는다. 위치가 꼭짓점을 그대로
	// 통과하면 헤딩만 부드러워도 각지게 꺾여 보인다. 제어점은 세그먼트에서 뽑아 경사의 Z 를 유지한다.
	FVector CornerTangent = FVector::ZeroVector;
	bool bOnUTurn = false;
	if (CornerRadius > 1.f)
	{
		const FVector P = GraphNodes.IsValidIndex(Car.PrevNode) ? GraphNodes[Car.PrevNode] : A;
		const float LenPA = FVector::Dist2D(P, A);
		const float LenBC = FVector::Dist2D(B, C);
		const float Rout = FMath::Min3(CornerRadius, Car.EdgeLen * 0.45f, LenBC * 0.45f);
		const float Rin = FMath::Min3(CornerRadius, Car.EdgeLen * 0.45f, LenPA * 0.45f);
		float s = -1.f;
		FVector Q0 = A, Q1 = A, Q2 = A;
		if (Car.Dist > Car.EdgeLen - Rout && Rout > 1.f)
		{
			Q0 = FMath::Lerp(A, B, (Car.EdgeLen - Rout) / Car.EdgeLen);
			Q1 = B;
			Q2 = FMath::Lerp(B, C, Rout / LenBC);
			s = 0.5f * (Car.Dist - (Car.EdgeLen - Rout)) / Rout;
			bOnUTurn = Car.NextNode == Car.FromNode;
		}
		else if (Car.Dist < Rin && Rin > 1.f && Car.PrevNode != Car.FromNode)
		{
			Q0 = FMath::Lerp(P, A, (LenPA - Rin) / LenPA);
			Q1 = A;
			Q2 = FMath::Lerp(A, B, Rin / Car.EdgeLen);
			s = 0.5f + 0.5f * (Car.Dist / Rin);
			bOnUTurn = Car.PrevNode == Car.ToNode;
		}
		if (bOnUTurn)
		{
			// 왕복으로 겹치는 제어점에서는 접선이 사라지므로 양쪽 차선을 좌회전 반원으로 잇는다.
			const FVector Forward = (Q1 - Q0) / FVector::Dist2D(Q0, Q1);
			const FVector TurnRight(-Forward.Y, Forward.X, 0.f);
			const float Angle = PI * s;
			const float SinAngle = FMath::Sin(Angle);
			const float CosAngle = FMath::Cos(Angle);
			// 짧은 엣지는 앞뒤 반경만 줄여 끝 노드를 넘어가지 않게 한다.
			const float ForwardRadius = FMath::Min(Car.LaneOff, static_cast<float>(FVector::Dist2D(Q0, Q1)));
			Base = Q0 + Forward * (ForwardRadius * SinAngle) + TurnRight * (Car.LaneOff * CosAngle);
			CornerTangent = (Forward * (ForwardRadius * CosAngle) - TurnRight * (Car.LaneOff * SinAngle)).GetSafeNormal();
		}
		else if (s >= 0.f)
		{
			const float u = 1.f - s;
			Base = Q0 * (u * u) + Q1 * (2.f * u * s) + Q2 * (s * s);
			CornerTangent = ((Q1 - Q0) * (2.f * u) + (Q2 - Q1) * (2.f * s)).GetSafeNormal2D();
		}
	}

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

	// 경사 추종: 룩어헤드와의 Z 차이로 목표 pitch(오르막 dz>0 = 코 들림). 엣지 전환 시 튀지 않게 각속도 제한.
	{
		const FVector Rise = LookPt - Base;
		const float Horiz = FMath::Max(FVector(Rise.X, Rise.Y, 0.f).Size(), 1.f);
		const float TargetPitch = bOnUTurn ? CornerTangent.Rotation().Pitch : FMath::RadiansToDegrees(FMath::Atan2(Rise.Z, Horiz));
		if (DeltaTime <= 0.f) { Car.Pitch = TargetPitch; }
		else
		{
			const float dp = FMath::FindDeltaAngleDegrees(Car.Pitch, TargetPitch);
			const float MaxPStep = MaxPitchRate * DeltaTime;
			Car.Pitch += FMath::Clamp(dp, -MaxPStep, MaxPStep);
		}
	}
	if (Dir.SizeSquared() < 1.f) { Dir = B - A; Dir.Z = 0.f; }
	if (Dir.SizeSquared() < 1.f) { Dir = FVector(1.f, 0.f, 0.f); }
	Dir.Normalize();
	// 헤딩 소스는 하나여야 한다. 룩어헤드(LookAheadDistance)는 코너 구간(CornerRadius)보다 먼저
	// 선회를 시작하는데, 코너에 들어서면 목표가 베지어 접선(시작점 = 직선 방향)으로 바뀌어
	// 헤딩이 되돌아간다 = 꺾었다 돌아왔다 다시 꺾는 움직임. 필렛이 켜져 있으면 경로 접선만 쓴다.
	float TargetYaw;
	if (CornerRadius > 1.f)
	{
		const FVector PathDir = CornerTangent.IsNearlyZero() ? (B - A).GetSafeNormal2D() : CornerTangent;
		TargetYaw = PathDir.IsNearlyZero() ? Car.Yaw : PathDir.Rotation().Yaw;
	}
	else
	{
		TargetYaw = Dir.Rotation().Yaw;   // 필렛 꺼짐 = 구 동작(룩어헤드로 미리 선회)
	}
	const float PrevYaw = Car.Yaw;

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

	// 선회 기울기: 요 변화율을 MaxYawRate 로 정규화. UE 는 +Yaw 가 우회전이라 바깥쪽 기울기는 음수.
	{
		const float YawRate = (DeltaTime > 0.f) ? (FMath::FindDeltaAngleDegrees(PrevYaw, Car.Yaw) / DeltaTime) : 0.f;
		const float TargetRoll = -MaxBankAngle * FMath::Clamp(YawRate / MaxYawRate, -1.f, 1.f);
		if (DeltaTime <= 0.f) { Car.Roll = TargetRoll; }
		else
		{
			const float MaxRStep = MaxRollRate * DeltaTime;
			Car.Roll += FMath::Clamp(TargetRoll - Car.Roll, -MaxRStep, MaxRStep);
		}
	}

	// 차선 오프셋은 '부드럽게 도는 헤딩' 기준 → 교차로에서 위치 팝 없이 자연스럽게 휨(우측통행, 차별 안/바깥 차선).
	const float YawRad = FMath::DegreesToRadians(Car.Yaw);
	const FVector HeadingDir(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f);
	const FVector Right(-HeadingDir.Y, HeadingDir.X, 0.f);
	const FVector Loc = bOnUTurn ? Base : Base + Right * Car.LaneOff;

	const bool bInView = IsInView(Loc, Car.bVisible);
	ApplyCarInstances(Car.ModelIndex, Car.BodyInstance, Car.HeadInst, Car.TailInst, Loc, Car.Yaw, bInView, Car.bVisible, Car.Pitch, Car.Roll);
	Car.bVisible = bInView;
	Car.LastLoc = Loc;

	// 연속으로 머문 시간만 센다 — 한 번이라도 반경 안으로 돌아오면 재활용 대기를 처음부터.
	const bool bBeyondRecycle = bHasFocus && FVector::DistSquared2D(Loc, FocusLoc) > FMath::Square(ViewRadius + RecycleMargin);
	Car.OutOfRangeTime = bBeyondRecycle ? Car.OutOfRangeTime + DeltaTime : 0.f;
}

bool ATrafficManager::TryRespawnIntoBand(int32 CarIdx)
{
	if (!bSpawnCandidatesBuilt)
	{
		bSpawnCandidatesBuilt = true;
		SpawnCandidates.Reset();
		const float BandMinSq = FMath::Square(ViewRadius);
		const float BandMaxSq = FMath::Square(ViewRadius + SpawnBand);
		auto CollectCandidates = [&](bool bInwardOnly)
		{
			for (const FIntPoint& E : GraphEdges)
			{
				if (!GraphNodes.IsValidIndex(E.X) || !GraphNodes.IsValidIndex(E.Y) || E.X == E.Y)
				{
					continue;
				}
				// 밴드 판정은 차가 실제로 나타나는 출발 노드로 — 엣지 중점 기준이면 긴 엣지의 출발점이 화면 안일 수 있다.
				const double DistSqX = FVector::DistSquared2D(GraphNodes[E.X], FocusLoc);
				const double DistSqY = FVector::DistSquared2D(GraphNodes[E.Y], FocusLoc);
				if (DistSqX >= BandMinSq && DistSqX <= BandMaxSq && (!bInwardOnly || DistSqY < DistSqX)) { SpawnCandidates.Emplace(E.X, E.Y); }
				if (DistSqY >= BandMinSq && DistSqY <= BandMaxSq && (!bInwardOnly || DistSqX < DistSqY)) { SpawnCandidates.Emplace(E.Y, E.X); }
			}
		};
		// 포커스 쪽으로 달리는 엣지만 — 바깥으로 출발한 차는 화면에 한 번도 안 잡히고 재활용만 된다. 없으면 밴드 전체로.
		CollectCandidates(true);
		if (SpawnCandidates.Num() == 0)
		{
			CollectCandidates(false);
		}
	}
	if (SpawnCandidates.Num() == 0)
	{
		return false;
	}
	FGraphCar& Car = GCars[CarIdx];
	const float LaneSplit = LaneOffset + LaneWidth * 0.5f;
	const bool bOuterLane = Car.LaneOff > LaneSplit;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		Car.Rng = Car.Rng * 1664525u + 1013904223u;
		// LCG 하위 비트는 주기가 짧아 상위 비트로 고른다.
		const FIntPoint& Cand = SpawnCandidates[static_cast<int32>((Car.Rng >> 16) % static_cast<uint32>(SpawnCandidates.Num()))];
		bool bBlocked = false;
		for (int32 OtherIdx = 0; OtherIdx < GCars.Num() && !bBlocked; ++OtherIdx)
		{
			const FGraphCar& OtherCar = GCars[OtherIdx];
			bBlocked = OtherIdx != CarIdx && OtherCar.bActive && OtherCar.FromNode == Cand.X && OtherCar.ToNode == Cand.Y
				&& (OtherCar.LaneOff > LaneSplit) == bOuterLane && OtherCar.Dist < MinRespawnGap;
		}
		if (!bBlocked)
		{
			RespawnGraphCar(Car, Cand.X, Cand.Y);
			return true;
		}
	}
	return false;
}

void ATrafficManager::RecycleGraphCars()
{
	for (int32 CarIdx = 0; CarIdx < GCars.Num(); ++CarIdx)
	{
		const FGraphCar& Car = GCars[CarIdx];
		// 숨겨진 활성 차만 옮긴다 — 보이는 차를 옮기면 순간이동이 화면에 잡힌다.
		if (!Car.bActive || Car.bVisible || Car.OutOfRangeTime < RecycleDelay)
		{
			continue;
		}
		if (TryRespawnIntoBand(CarIdx))
		{
			if (bDebugLog) { ++DebugRecycleCount; }
		}
		else if (SpawnCandidates.Num() == 0)
		{
			return;
		}
	}
}

void ATrafficManager::RespawnGraphCar(FGraphCar& Car, int32 FromNode, int32 ToNode)
{
	// BuildGraphTraffic 과 같은 초기화. bVisible=false 라 다음 스텝 ApplyCarInstances 가 보간 없이 새 위치를 쓴다.
	Car.FromNode = FromNode;
	Car.ToNode = ToNode;
	Car.PrevNode = FromNode;
	Car.NextNode = PickNextNode(FromNode, ToNode, Car.Rng);
	Car.EdgeLen = FMath::Max(static_cast<float>(FVector::Dist2D(GraphNodes[FromNode], GraphNodes[ToNode])), 1.f);
	Car.Dist = 0.f;
	Car.Speed = Car.CruiseSpeed;
	Car.Yaw = static_cast<float>((GraphNodes[ToNode] - GraphNodes[FromNode]).GetSafeNormal2D().Rotation().Yaw);
	Car.Pitch = 0.f;
	Car.Roll = 0.f;
	Car.SpacingGap = 1e9f;
	Car.OutOfRangeTime = 0.f;
	Car.bVisible = false;
}

void ATrafficManager::AdjustActiveCars(float DeltaTime)
{
	// 전 도로 커버 테스트는 전 차량이 활성이어야 한다.
	if (!bHasFocus || (bGraphMode && bDebugFillAllEdges))
	{
		return;
	}
	const int32 PoolNum = bGraphMode ? GCars.Num() : Cars.Num();
	const float Desired = FMath::Min(DesiredActiveCars, static_cast<float>(PoolNum));
	// 끄는 차는 이미 숨김(0스케일)이라 인스턴스를 다시 쓸 필요가 없다. 숨김 차가 없으면 이번 스텝은 그대로.
	// 목표에서 0.75 이상 벗어날 때만 스텝당 1대 — 반올림 경계(x.5)에서 줌이 떨려도 대수가 왕복하지 않는다.
	if (!bGraphMode)
	{
		// 루트 모드는 고정 스플라인이라 화면 밖에서 들여올 수 없어 활성 대수로 제어한다.
		const int32 ActiveNow = CountActiveTrafficCars(Cars);
		if (Desired >= static_cast<float>(ActiveNow) + 0.75f)
		{
			const int32 Idx = Cars.IndexOfByPredicate([](const FTrafficCar& Car) { return !Car.bActive; });
			if (Idx != INDEX_NONE)
			{
				// 한 번도 켜진 적 없는 차는 Yaw=0 이라 다음 스텝의 회전 제한에 걸려 제자리에서 돈다 → 켜는 순간 자세를 맞춘다.
				Cars[Idx].bActive = true;
				UpdateCar(Cars[Idx], 0.f);
			}
		}
		else if (Desired <= static_cast<float>(ActiveNow) - 0.75f)
		{
			const int32 Idx = PickHiddenTrafficCarToDeactivate(Cars, FocusLoc);
			if (Idx != INDEX_NONE)
			{
				Cars[Idx].bActive = false;
			}
		}
		return;
	}

	int32 ActiveNow = 0;
	int32 OnScreenNow = 0;
	for (const FGraphCar& Car : GCars)
	{
		ActiveNow += Car.bActive ? 1 : 0;
		OnScreenNow += (Car.bActive && Car.bVisible && IsOnScreen(Car.LastLoc)) ? 1 : 0;
	}
	ActivateCooldown = FMath::Max(0.f, ActivateCooldown - DeltaTime);
	const int32 ActiveCap = FMath::Clamp(FMath::RoundToInt(Desired) + TransitAllowance, 0, PoolNum);
	if (ActiveNow > ActiveCap || static_cast<float>(OnScreenNow) > Desired + 0.75f)
	{
		const int32 Idx = PickHiddenTrafficCarToDeactivate(GCars, FocusLoc);
		if (Idx != INDEX_NONE)
		{
			GCars[Idx].bActive = false;
		}
	}
	else if (static_cast<float>(OnScreenNow) < Desired - 0.75f && ActiveNow < ActiveCap && ActivateCooldown <= 0.f)
	{
		const int32 Idx = GCars.IndexOfByPredicate([](const FGraphCar& Car) { return !Car.bActive; });
		// 스폰 밴드에서 출발시켜 화면 밖에서 들어오게 한다. 후보가 없으면 다음 스텝에 다시.
		if (Idx != INDEX_NONE && TryRespawnIntoBand(Idx))
		{
			GCars[Idx].bActive = true;
			ActivateCooldown = ActivateInterval;
		}
	}
}

void ATrafficManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 시뮬 스로틀과 독립적으로 매 프레임: 카메라가 움직였으면 라이트 카드 재조준(30Hz 빌보드 스냅 = 이동 시 차 라이트 깜빡임 제거).
	RefreshLightBillboards();

	// 액터 설정과 기기별 최소 간격 중 큰 값을 사용한다. 건너뛴 시간도 다음 주행 스텝에 합산한다.
	const float EffectiveUpdateInterval = FMath::Max(SimUpdateInterval,
		FMath::Max(0.f, CVarTrafficMinUpdateInterval.GetValueOnGameThread()));
	SimAccum += DeltaTime;
	if (SimAccum < EffectiveUpdateInterval)
	{
		return;
	}
	const float SimDelta = SimAccum;
	SimAccum = 0.f;

	// 시계는 WP 스트리밍으로 늦게 뜰 수 있다 - BeginPlay 의 1회 조회로는 영구히 null 로 남는다.
	// (WindowLightManagerSubsystem 은 Tick 에서 재탐색한다. 여기도 같게 맞춘다.)
	if (!CachedTimeCycle.IsValid())
	{
		CachedTimeCycle = Cast<ATimeCycleManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ATimeCycleManager::StaticClass()));
	}

	// 밤 게이트를 매 스텝 재평가한다. 시각 변경 이벤트만 믿으면 어긋나는 경우가 셋이다:
	// 시계를 늦게 찾는 경우, 시계의 BeginPlay 전에 시각을 읽는 경우, 치트가 시간을 점프시켜
	// 이벤트를 안 거치는 경우. 어느 쪽이든 라이트 ISM 이 숨겨진 채 복귀하지 못한다.
	int32 DiagCurSec = -1;
	bool bDiagNight = false;
	if (const ATimeCycleManager* Clock = CachedTimeCycle.Get())
	{
		if (Clock->HasActorBegunPlay())
		{
			const int32 CurSec = Clock->GetVisualTime().ToSeconds();
			const int32 RiseSec = Clock->GetSunRiseTime().ToSeconds();
			const int32 SetSec = Clock->GetSunSetTime().ToSeconds();
			const bool bNightNow = (CurSec >= SetSec) || (CurSec < RiseSec);
			DiagCurSec = CurSec;
			bDiagNight = bNightNow;
			if (bNightNow != bGlowVisible) { ApplyNightVisibility(bNightNow); }
		}
	}

	// [진단/임시] 시계 유무와 무관하게 1회 찍는다 - 시계 블록 안에 두면 시계가 없을 때 아무것도 안 나와
	// "로그가 안 보인다" 외에 정보가 없다. 원인 확정되면 이 블록째로 지운다.
	static int32 DiagCount = 0;
	static double NextDiagAt = 0.0;
	const double DiagNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (DiagNow < 1.0 && DiagCount > 0) { DiagCount = 0; NextDiagAt = 0.0; }   // 새 PIE 면 리셋
	if (DiagCount < 6 && DiagNow >= NextDiagAt)
	{
		++DiagCount;
		NextDiagAt = DiagNow + 3.0;
		float NightVal = -999.f;
		if (UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/MPC_WindowLight")))
		{
			if (UMaterialParameterCollectionInstance* MPCI = GetWorld()->GetParameterCollectionInstance(MPC))
			{
				MPCI->GetScalarParameterValue(TEXT("Night_Intensity"), NightVal);
			}
		}
		const int32 FrontN = FrontLightISM ? FrontLightISM->GetInstanceCount() : -1;
		const int32 RearN = RearLightISM ? RearLightISM->GetInstanceCount() : -1;
		FTransform T0;
		if (FrontLightISM && FrontN > 0) { FrontLightISM->GetInstanceTransform(0, T0, true); }
		const ATimeCycleManager* DiagClock = CachedTimeCycle.Get();
		UE_LOG(LogTemp, Warning, TEXT("[LightDiag] #%d t=%.1f 시계=%s BegunPlay=%s 시각=%d초 밤=%d 글로우표시=%d | Night_Intensity=%.3f | 헤드ISM %d개 보임=%d 메시=%s 머티=%s | 테일ISM %d개 | 샘플0 스케일=%.3f 위치=%s | 차량 %d대"),
			DiagCount, DiagNow,
			DiagClock ? TEXT("있음") : TEXT("없음"),
			DiagClock ? (DiagClock->HasActorBegunPlay() ? TEXT("예") : TEXT("아니오")) : TEXT("-"),
			DiagCurSec, bDiagNight ? 1 : 0, bGlowVisible ? 1 : 0, NightVal,
			FrontN, FrontLightISM ? (FrontLightISM->IsVisible() ? 1 : 0) : -1,
			(FrontLightISM && FrontLightISM->GetStaticMesh()) ? *FrontLightISM->GetStaticMesh()->GetName() : TEXT("NULL"),
			(FrontLightISM && FrontLightISM->GetMaterial(0)) ? *FrontLightISM->GetMaterial(0)->GetName() : TEXT("NULL"),
			RearN, T0.GetScale3D().X, *T0.GetLocation().ToCompactString(), GCars.Num());
	}

	// 라이트 빌보드용 카메라 위치 캐시(라이트마다 GetPlayerCameraManager 호출 방지).
	if (bGlowVisible)
	{
		if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			CachedCameraLoc = PCM->GetCameraLocation();
		}
	}

	UpdateFocus();

	if (bGraphMode)
	{
		// car-following: (엣지,차선,거리)로 정렬 후 인접한 앞차와의 간격만 계산(O(n log n)) → 겹침 방지.
		GCars.Sort([this](const FGraphCar& A, const FGraphCar& B)
		{
			// 비활성 차는 뒤로 몰아 간격 계산에서 뺀다.
			if (A.bActive != B.bActive) { return A.bActive; }
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
				if (A.bActive && Bc.bActive && Bc.FromNode == A.FromNode && Bc.ToNode == A.ToNode && La == Lb)
				{
					GCars[i].SpacingGap = Bc.Dist - A.Dist;
				}
			}
		}
		for (FGraphCar& Car : GCars) { if (Car.bActive) { UpdateGraphCar(Car, SimDelta); } }

		// 주행 뒤에 활성 대수 조정·재배치 — 숨김 차만 옮기므로 이번 프레임 화면은 그대로, 다음 스텝 정렬·간격에 바로 반영된다.
		// 조정을 먼저 해야 곧 재활용될 먼 숨김 차가 비활성화 대상으로 쓰인다.
		bSpawnCandidatesBuilt = false;
		AdjustActiveCars(SimDelta);
		// 전 도로 커버 테스트(bDebugFillAllEdges)는 차가 배치된 엣지에 남아야 한다.
		if (bHasFocus && !bDebugFillAllEdges && CVarTrafficCameraPool.GetValueOnGameThread() != 0)
		{
			RecycleGraphCars();
		}
	}
	else
	{
		for (FTrafficCar& Car : Cars) { if (Car.bActive) { UpdateCar(Car, SimDelta); } }
		AdjustActiveCars(SimDelta);
	}

	if (bDebugLog)
	{
		DebugLogAccum += SimDelta;
		if (DebugLogAccum >= 3.f)
		{
			DebugLogAccum = 0.f;
			int32 VisibleCount = 0;
			int32 OnScreenCount = 0;
			for (const FGraphCar& Car : GCars)
			{
				VisibleCount += Car.bVisible ? 1 : 0;
				OnScreenCount += (Car.bVisible && IsOnScreen(Car.LastLoc)) ? 1 : 0;
			}
			for (const FTrafficCar& Car : Cars)
			{
				VisibleCount += Car.bVisible ? 1 : 0;
				OnScreenCount += (Car.bVisible && IsOnScreen(Car.LastLoc)) ? 1 : 0;
			}
			const int32 Total = bGraphMode ? GCars.Num() : Cars.Num();
			const int32 ActiveCount = bGraphMode ? CountActiveTrafficCars(GCars) : CountActiveTrafficCars(Cars);
			UE_LOG(LogTemp, Warning, TEXT("[TrafficManager] %s 표시 %d/활성 %d/풀 %d (목표 %.1f, 반경내 %d), 표시반경 %.0f(포커스=%d), 재배치 %d, 노드%d/엣지%d/경로%d (게이팅무시=%d, 재활용=%d)"),
				bGraphMode ? TEXT("GRAPH") : TEXT("ROUTE"), OnScreenCount, ActiveCount, Total, DesiredActiveCars, VisibleCount, ViewRadius, bHasFocus ? 1 : 0, DebugRecycleCount,
				GraphNodes.Num(), GraphEdges.Num(), Routes.Num(), bDebugIgnoreBuildingGating ? 1 : 0,
				CVarTrafficCameraPool.GetValueOnGameThread());
			DebugRecycleCount = 0;
		}
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
	// [디버그] 카메라 풀: 초록=표시 반경, 빨강=재활용 반경.
	if (bDebugDrawGraph && bHasFocus && GetWorld())
	{
		const FVector Center(FocusLoc.X, FocusLoc.Y, FocusLoc.Z + VehicleZOffset + 30.f);
		DrawDebugCircle(GetWorld(), Center, ViewRadius, 96, FColor::Green, false, -1.f, 0, 40.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
		DrawDebugCircle(GetWorld(), Center, ViewRadius + RecycleMargin, 96, FColor::Red, false, -1.f, 0, 40.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	}

	// ISM 은 도시 크기 컴포넌트 바운드로 LOD 를 골라 늘 LOD0 → 차종별 최근접 가시 차 기준으로 엔진 정적메시 LOD 선택을 재현해 강제.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	FSceneViewProjectionData ProjData;
	if (LP && LP->ViewportClient && LP->GetProjectionData(LP->ViewportClient->Viewport, ProjData))
	{
		const double ScreenMultiple = 0.5 * FMath::Max(ProjData.ProjectionMatrix.M[0][0], ProjData.ProjectionMatrix.M[1][1]);
		static const IConsoleVariable* SMLODScaleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.StaticMeshLODDistanceScale"));
		// 렌더러(SceneVisibility)와 같은 배율 = CVar × View.LODDistanceFactor(FOV/DefaultFOV, 방금 GetProjectionData 가 갱신한 값).
		const float ScreenSizeScale = (SMLODScaleVar ? SMLODScaleVar->GetFloat() : 1.f) * PC->LocalPlayerCachedLODDistanceFactor;
		TArray<double> NearestDistSq;
		NearestDistSq.Init(TNumericLimits<double>::Max(), BodyISM.Num());
		auto AddNearest = [&](int32 ModelIdx, int32 BodyInst, bool bVis)
		{
			FTransform InstT;
			if (bVis && BodyISM.IsValidIndex(ModelIdx) && BodyISM[ModelIdx] && BodyISM[ModelIdx]->GetStaticMesh()
				&& BodyISM[ModelIdx]->GetInstanceTransform(BodyInst, InstT, true))
			{
				const FVector BoundsOrigin = InstT.TransformPosition(BodyISM[ModelIdx]->GetStaticMesh()->GetBounds().Origin);
				NearestDistSq[ModelIdx] = FMath::Min(NearestDistSq[ModelIdx], FVector::DistSquared(BoundsOrigin, ProjData.ViewOrigin));
			}
		};
		for (const FTrafficCar& Car : Cars) { AddNearest(Car.ModelIndex, Car.BodyInstance, Car.bActive && Car.bVisible); }
		for (const FGraphCar& Car : GCars) { AddNearest(Car.ModelIndex, Car.BodyInstance, Car.bActive && Car.bVisible); }
		for (int32 Mi = 0; Mi < BodyISM.Num(); ++Mi)
		{
			UStaticMesh* Mesh = BodyISM[Mi] ? BodyISM[Mi]->GetStaticMesh() : nullptr;
			const FStaticMeshRenderData* RD = Mesh ? Mesh->GetRenderData() : nullptr;
			if (!RD || NearestDistSq[Mi] == TNumericLimits<double>::Max()) { continue; }
			const double ScreenRadiusSq = FMath::Square(ScreenMultiple * Mesh->GetBounds().SphereRadius) / FMath::Max(1.0, NearestDistSq[Mi]);
			int32 Lod = RD->LODResources.Num() - 1;
			while (Lod > 0 && FMath::Square(RD->ScreenSize[Lod].GetValue() * ScreenSizeScale * 0.5f) <= ScreenRadiusSq) { --Lod; }
			// 0=자동이라 +1. 같은 값이면 엔진이 early-out → 프록시 재생성은 LOD 가 바뀔 때만.
			BodyISM[Mi]->SetForcedLodModel(FMath::Max(Lod, Mesh->GetMinLODIdx()) + 1);
		}
	}

	// 인스턴스 버퍼 갱신 반영(컴포넌트당 1회) — 프록시 재생성 없이 GPU-Scene 인스턴스 델타만 플러시(떨림 방지).
	for (UInstancedStaticMeshComponent* I : BodyISM) { if (I) { I->MarkRenderInstancesDirty(); } }
	if (bGlowVisible && FrontLightISM) { FrontLightISM->MarkRenderInstancesDirty(); }
	if (bGlowVisible && RearLightISM) { RearLightISM->MarkRenderInstancesDirty(); }
}
