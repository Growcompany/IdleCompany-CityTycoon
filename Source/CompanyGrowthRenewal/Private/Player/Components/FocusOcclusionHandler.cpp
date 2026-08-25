#include "Player/Components/FocusOcclusionHandler.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/EntityManager.h"
#include "Player/Components/FocusOcclusionMath.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"

UFocusOcclusionHandler::UFocusOcclusionHandler()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UFocusOcclusionHandler::SetFocusTarget(AActor* InTarget)
{
	if (!InTarget)
	{
		ClearFocusTarget();
		return;
	}

	FocusTarget = InTarget;

	if (UWorld* CurWorld = GetWorld())
	{
		// 0 이하 간격은 SetTimer 가 조용히 타이머를 지워버려 고스트가 켜진 채 굳는다
		const float SafeInterval = FMath::Max(UpdateInterval, 0.01f);
		CurWorld->GetTimerManager().SetTimer(
			UpdateTimerHandle, this, &UFocusOcclusionHandler::UpdateOcclusion, SafeInterval, true);
	}

	// 타이머 첫 발화를 기다리지 않고 즉시 1회 — 카메라 전환 시작과 동시에 비켜주게.
	// 안전망을 건너뛴 RefreshOccluders 직행: 호출자들이 GoToUIMode 를 등록 "뒤"에 부르므로
	// 이 시점 모드는 아직 Normal 이고, 안전망을 태우면 등록이 그 자리에서 취소된다.
	RefreshOccluders();
}

void UFocusOcclusionHandler::ClearFocusTarget()
{
	FocusTarget.Reset();

	if (UWorld* CurWorld = GetWorld())
	{
		CurWorld->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	// 즉시 복원하지 않고 목표만 0 으로 — Tick 이 페이드아웃을 마친 뒤 복원한다
	for (TPair<TWeakObjectPtr<ABuildingBaseActor>, FGhostState>& Pair : GhostStates)
	{
		Pair.Value.Goal = 0.f;
	}
}

void UFocusOcclusionHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* CurWorld = GetWorld())
	{
		CurWorld->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	FocusTarget.Reset();
	ForceRestoreAllBuildings();
	GhostStates.Empty();

	Super::EndPlay(EndPlayReason);
}

void UFocusOcclusionHandler::ForceRestoreAllBuildings()
{
	// 우리가 켠 것부터 되돌린다 — 월드 정리 중이라 EntityManager 를 못 얻어도 이건 남는다
	for (const TPair<TWeakObjectPtr<ABuildingBaseActor>, FGhostState>& Pair : GhostStates)
	{
		if (ABuildingBaseActor* Building = Pair.Key.Get())
		{
			Building->SetOccluderGhost(false);
		}
	}

	UWorld* CurWorld = GetWorld();
	if (!CurWorld)
	{
		return;
	}

	// 추적을 놓친 고스트까지 쓸어담는 안전망
	if (UEntityManager* EntityMgr = CurWorld->GetSubsystem<UEntityManager>())
	{
		for (ABuildingBaseActor* Building : EntityMgr->GetBuildings())
		{
			if (Building && Building->IsOccluderGhosted())
			{
				Building->SetOccluderGhost(false);
			}
		}
	}
}

void UFocusOcclusionHandler::UpdateOcclusion()
{
	// 입력 모드가 Normal 로 돌아왔으면 패널이 닫힌 것 — 해제 호출을 빠뜨린 경로 대비 안전망.
	// 주기 진입점에만 둔다: 등록 호출에 적용하면 "놓친 해제"가 아니라 등록 자체를 죽인다.
	if (APlayerCamera* Cam = Cast<APlayerCamera>(GetOwner()))
	{
		if (AMainMapPlayerController* PC = Cam->GetPlayerController())
		{
			if (PC->GetCurrentInputMode() == EInputMode::Normal)
			{
				ClearFocusTarget();
				return;
			}
		}
	}

	RefreshOccluders();
}

void UFocusOcclusionHandler::RefreshOccluders()
{
	AActor* Target = FocusTarget.Get();
	if (!Target)
	{
		ClearFocusTarget();
		return;
	}

	APlayerCamera* Cam = Cast<APlayerCamera>(GetOwner());
	UWorld* CurWorld = GetWorld();
	if (!Cam || !Cam->CameraComponent || !CurWorld)
	{
		return;
	}

	FBox TargetBounds(ForceInit);
	if (ABuildingBaseActor* TargetBuilding = Cast<ABuildingBaseActor>(Target))
	{
		TargetBounds = TargetBuilding->GetBuildingWorldBounds();
	}
	else
	{
		FVector BoundsOrigin, BoundsExtent;
		Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);
		TargetBounds = FBox::BuildAABB(BoundsOrigin, BoundsExtent);
	}
	if (!TargetBounds.IsValid)
	{
		return;
	}

	const FVector CamLoc = Cam->CameraComponent->GetComponentLocation();

	// 스프링암은 피치만 돌고 요는 폰이 도는 구조라 폰의 오른쪽 = 카메라의 수평 오른쪽
	TArray<FVector> Samples;
	FocusOcclusionMath::ComputeSamplePoints(TargetBounds, Cam->GetActorRightVector(), SampleInset, Samples);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FocusOcclusion), false, Cam);
	Params.AddIgnoredActor(Target);

	// 건물 박스는 이 채널에 Block 이라 기본 응답(전 채널 Block)이면 최근접 1개에서 트레이스가 끊긴다.
	// 조회자 응답을 Overlap 으로 낮추면 합의 결과가 Touch 로 내려가(FMath::Min) 광선이 끝까지 가며 뒷줄 건물까지 모은다.
	const FCollisionResponseParams TouchAll(ECR_Overlap);

	TArray<ABuildingBaseActor*> Occluders;
	for (const FVector& SamplePoint : Samples)
	{
		TArray<FHitResult> Hits;
		CurWorld->LineTraceMultiByChannel(Hits, CamLoc, SamplePoint, ECC_GameTraceChannel1, Params, TouchAll);
		for (const FHitResult& Hit : Hits)
		{
			ABuildingBaseActor* Building = Cast<ABuildingBaseActor>(Hit.GetActor());
			if (Building && Building != Target)
			{
				Occluders.AddUnique(Building);
			}
		}
	}

	// 가까운 것부터 남긴다 — 밀집 지역에서 도시가 통째로 사라지지 않게 상한을 건다
	Occluders.Sort([CamLoc](const ABuildingBaseActor& A, const ABuildingBaseActor& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), CamLoc)
			 < FVector::DistSquared(B.GetActorLocation(), CamLoc);
	});
	const int32 GhostLimit = FMath::Max(MaxGhostCount, 0);
	if (Occluders.Num() > GhostLimit)
	{
		Occluders.SetNum(GhostLimit);
	}

	for (TPair<TWeakObjectPtr<ABuildingBaseActor>, FGhostState>& Pair : GhostStates)
	{
		Pair.Value.Goal = 0.f;
	}
	for (ABuildingBaseActor* Building : Occluders)
	{
		if (FGhostState* Existing = GhostStates.Find(Building))
		{
			Existing->Goal = 1.f;
		}
		else
		{
			Building->SetOccluderGhost(true);
			FGhostState NewState;
			NewState.Alpha = 0.f;
			NewState.Goal = 1.f;
			GhostStates.Add(Building, NewState);
		}
	}
}

void UFocusOcclusionHandler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GhostStates.Num() == 0)
	{
		return;
	}

	TArray<TWeakObjectPtr<ABuildingBaseActor>> Finished;
	for (TPair<TWeakObjectPtr<ABuildingBaseActor>, FGhostState>& Pair : GhostStates)
	{
		ABuildingBaseActor* Building = Pair.Key.Get();
		if (!Building)
		{
			Finished.Add(Pair.Key);
			continue;
		}

		Pair.Value.Alpha = FocusOcclusionMath::StepGhostAlpha(
			Pair.Value.Alpha, Pair.Value.Goal, DeltaTime, TransitionTime);
		Building->SetGhostOpacity(Pair.Value.Alpha * GhostOpacity);

		if (Pair.Value.Goal <= 0.f && Pair.Value.Alpha <= 0.f)
		{
			Building->SetOccluderGhost(false);
			Finished.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<ABuildingBaseActor>& Key : Finished)
	{
		GhostStates.Remove(Key);
	}
}
