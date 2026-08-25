#include "Entity/Plot/CityDemolitionActor.h"
#include "Components/StaticMeshComponent.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Player/BuildingLandCameraShake.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/SoftObjectPtr.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

ACityDemolitionActor::ACityDemolitionActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false; // 초기화(BeginDemolition) 전 틱 방지
}

void ACityDemolitionActor::BeginDemolition(AActor* InBuilding, int32 InFloors, int32 InCompanyKey)
{
	if (!InBuilding) { Destroy(); return; }

	Building = InBuilding;
	CompanyKey = InCompanyKey;
    OrigScale = InBuilding->GetActorScale3D();
    BaseLoc = InBuilding->GetActorLocation();

    FVector BOrigin, BExtent;
    InBuilding->GetActorBounds(false, BOrigin, BExtent);
    FullHeight = BExtent.Z * 2.f;             // GetActorBounds Extent = 반(half) → 2배가 전체 높이
    CenterXY = FVector2D(BOrigin.X, BOrigin.Y);

    Floors = FMath::Max(1, InFloors);

    // BP_MB 는 Static mobility — Static 에서 런타임 스케일은 경고/무시되므로 메시들을 Movable 로 승격
    TArray<UStaticMeshComponent*> Meshes;
    InBuilding->GetComponents<UStaticMeshComponent>(Meshes);
    for (UStaticMeshComponent* MeshComp : Meshes)
    {
        if (MeshComp) { MeshComp->SetMobility(EComponentMobility::Movable); }
    }
    // SetActorScale3D 는 루트를 스케일하므로, 루트가 비-메시 Static SceneComponent 인 경우까지 커버
    if (USceneComponent* RootComp = InBuilding->GetRootComponent())
    {
        RootComp->SetMobility(EComponentMobility::Movable);
    }

    // 붕괴 중 클릭/충돌 방지를 위해 즉시 콜리전 차단
    InBuilding->SetActorEnableCollision(false);

    SetActorTickEnabled(true);
}

void ACityDemolitionActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) { return; }
	if (!Building.IsValid())
	{
		bFinished = true;
		OnVisualCleared.Broadcast(CompanyKey);
		Destroy();
		return;
	}

    StepTimer += DeltaSeconds;
    if (StepTimer < StepInterval) { return; }
    StepTimer = 0.f;
    StepInterval = FMath::Max(0.045f, StepInterval * 0.93f); // 스텝마다 가속(살짝 완화)

    ++StepsDone;
    // 마지막 스텝은 zero-scale(디제너릿 트랜스폼) 대신 곧장 쿵 연출로 — 바닥 먼지 중복도 방지
    if (StepsDone >= Floors) { Finish(); return; }

    const float Fraction = (float)(Floors - StepsDone) / (float)Floors;

    // 고층일수록 퍼프를 솎아 총량을 ~MaxPuffs로 제한 — 붕괴 Z스케일은 매 층 그대로(부드러운 하강) 두고 VFX만 게이트.
    // Floors<=12 stride1(매 층), 24층 stride2(=2층마다), 64층 stride6 → 퍼프 ~10개.
    const int32 MaxPuffs = 12;
    const int32 VfxStride = FMath::Max(1, FMath::CeilToInt((float)Floors / (float)MaxPuffs));
    const bool bSpawnPuff = (StepsDone % VfxStride) == 0;

    float HalfX = 150.f, HalfY = 150.f; // footprint 폴백
    if (AActor* B = Building.Get())
    {
        B->SetActorScale3D(FVector(OrigScale.X, OrigScale.Y, OrigScale.Z * Fraction));
        if (bSpawnPuff)
        {
            // Z 만 축소 → XY footprint 불변. 스폰하는 스텝에만 바운드에서 가로폭을 읽어 VFX 를 footprint 에 맞춤
            FVector BoOrigin, BoExtent;
            B->GetActorBounds(false, BoOrigin, BoExtent);
            HalfX = BoExtent.X; HalfY = BoExtent.Y;
        }
    }

    if (bSpawnPuff)
    {
        // 무너지는 층 높이에 "건물 한 층 올라갈 때"와 동일한 VFX(NS_Ground_Jump_Fx) — 단일 중앙 버스트
        const FVector FloorTop(CenterXY.X, CenterXY.Y, BaseLoc.Z + FullHeight * Fraction);
        const float AvgM = ((HalfX + HalfY) * 0.5f) / 100.f;       // footprint 평균(cm→m)
        // footprint 비례 — 하한 2.5(층 VFX 규칙 미러), 상한 6.0(넓은 타워가 화면 덮는 반투명 overdraw 방지)
        const float BaseScale = FMath::Clamp(AvgM * 0.5f, 2.5f, 6.0f);
        SpawnFloorVFX(FloorTop, BaseScale * 1.15f);
    }
}

void ACityDemolitionActor::Finish()
{
    if (bFinished) { return; }
    bFinished = true;

    float HalfX = 200.f, HalfY = 200.f; // footprint 폴백
    if (AActor* B = Building.Get())
    {
        FVector BoOrigin, BoExtent;
        B->GetActorBounds(false, BoOrigin, BoExtent); // 숨기기 전에 footprint 확보
        HalfX = BoExtent.X; HalfY = BoExtent.Y;
		B->SetActorHiddenInGame(true);
		B->SetActorEnableCollision(false);
	}
	OnVisualCleared.Broadcast(CompanyKey);

	const FVector Base(CenterXY.X, CenterXY.Y, BaseLoc.Z);
    const float AvgM = ((HalfX + HalfY) * 0.5f) / 100.f;
    const float BaseScale = FMath::Clamp(AvgM * 0.5f, 2.5f, 6.0f);
    // 바닥 "쿵" — 단일 대형 폭발(둘레 링은 같은 자리 overdraw라 중앙 하나로 통합)
    SpawnFloorVFX(Base, BaseScale * 2.0f);

    if (UWorld* W = GetWorld())
    {
        if (!USettingsManagerSubsystem::IsReduceMotion(this))
        {
            if (APlayerController* PC = W->GetFirstPlayerController())
            {
                PC->ClientStartCameraShake(UBuildingLandCameraShake::StaticClass(), 0.5f);
            }
        }
        if (UGameInstance* GI = W->GetGameInstance())
        {
            if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
            {
                // 건물 배치 착지음(PlayPlacementLandSequence thud 피크)과 동일 — 붕괴 슬램을 "건물 지을 때"와 한 몸으로
                SoundMgr->PlaySound(FName("Building_FloorUp_Body"));
                SoundMgr->PlaySoundWithDuration(FName("Building_FloorUp_Rumble"), 1.5f, 0.8f);
            }
        }
    }

    Destroy();
}

void ACityDemolitionActor::SpawnFloorVFX(const FVector& Loc, float AbsScale)
{
    UWorld* W = GetWorld();
    if (!W) { return; }
    // 건물이 한 층 올라갈 때(완성/업그레이드)와 동일한 VFX 재사용 — NS_Ground_Jump_Fx.
    // ABuildingBaseActor 가 하드참조라 쿠킹 보장 → 소프트경로 LoadSynchronous 가 모바일에서도 안전.
    static TSoftObjectPtr<UNiagaraSystem> RiseVFXPtr(
        FSoftObjectPath(TEXT("/Game/F_ToonSmokeAndDust/Fx/NS_Ground_Jump_Fx.NS_Ground_Jump_Fx")));
    UNiagaraSystem* VFX = RiseVFXPtr.LoadSynchronous();
    if (!VFX) { return; }
    // 풀링(AutoRelease) — 철거 1회당 다발 스폰이라 매번 새 컴포넌트 할당/파괴 대신 풀 재사용으로 churn 제거
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        W, VFX, Loc, FRotator::ZeroRotator, FVector(AbsScale),
        /*bAutoDestroy*/ true, /*bAutoActivate*/ true, ENCPoolMethod::AutoRelease);
}
