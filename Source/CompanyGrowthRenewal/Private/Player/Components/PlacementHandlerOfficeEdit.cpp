#include "Player/Components/PlacementHandler.h"
#include "Office/OfficeManager.h"
#include "Office/OfficeInterior.h"
#include "Office/WorkstationActorBase.h"
#include "Office/DecorationActor.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Components/StaticMeshComponent.h"
#include "Entity/Officeworker/Officeworker.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void UPlacementHandler::SetOfficeEditSession(bool bEnabled)
{
	if (!bEnabled) FinishExistingOfficeMove(false);
	bOfficeEditSession = bEnabled;
	CachedOfficeInterior = bEnabled ? Cast<AOfficeInterior>(UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass())) : nullptr;
	UpdateOfficeGridPreview();
}

bool UPlacementHandler::BeginExistingOfficeMove(AActor* ActorToMove)
{
	if (!bOfficeEditSession || OfficeMoveActor.IsValid() || !CachedOfficeInterior || !IsValid(ActorToMove)) return false;
	UOfficeManager* Manager = GetWorld()->GetSubsystem<UOfficeManager>();
	AWorkstationActorBase* Desk = Cast<AWorkstationActorBase>(ActorToMove);
	ADecorationActor* Decoration = Cast<ADecorationActor>(ActorToMove);
	if (!Manager || (Desk ? !Manager->GetPlacedWorkstations().Contains(Desk)
		: !Decoration || !Manager->GetPlacedDecorations().Contains(Decoration))) return false;
	if (Decoration)
	{
		bool bFound = false;
		PlacementDecorationInfo = GetWorld()->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>()->GetDecorationCardInfo(Decoration->DecorationCardTableRowName, bFound);
		if (!bFound || PlacementDecorationInfo.AllowedSurface == EDecorationSurface::None) return false;
	}
	OfficeMoveActor = ActorToMove;
	bExistingOfficeMoveActive = true;
	OfficeMoveOriginalTransform = ActorToMove->GetActorTransform();
	OfficeMoveOriginalTags = ActorToMove->Tags;
	ActorToMove->Tags.AddUnique(TEXT("PlacementMode"));
	PlacementWorkstation = Desk;
	PlacementDecoration = Decoration;
	CurrentPlacementMode = Desk ? EPlacementMode::Workstation : EPlacementMode::Decoration;
	if (Desk) PlacementWorkstationTypeID = Desk->WorkstationTypeID;
	if (Decoration && PlacementDecorationInfo.AllowedSurface == EDecorationSurface::Wall)
	{
		CurrentPlacementMode = EPlacementMode::WallDecoration;
		const FVector Position = Decoration->GetActorLocation();
		CurrentWallSide = FMath::Abs(CachedOfficeInterior->GetWallPlane(EWallSide::Left).PlaneDot(Position))
			< FMath::Abs(CachedOfficeInterior->GetWallPlane(EWallSide::Right).PlaneDot(Position)) ? EWallSide::Left : EWallSide::Right;
		CurrentWallPlane = CachedOfficeInterior->GetWallPlane(CurrentWallSide);
		WallDecorationRotationOffset = Decoration->GetActorRotation().Pitch;
	}
	UStaticMeshComponent* TargetMesh = Desk ? Desk->DeskMesh : Decoration->MeshComponent;
	if (TargetMesh)
	{
		OfficeMoveOriginalTraceResponse = TargetMesh->GetCollisionResponseToChannel(ECC_GameTraceChannel2);
		TargetMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
	}
	bHasPlacementTarget = true;
	bGroundCacheValid = false;
	PlacementSMC->SetVisibility(false);
	if (Decoration && Decoration->MeshComponent)
	{
		const FBoxSphereBounds LocalBounds = Decoration->MeshComponent->CalcBounds(Decoration->MeshComponent->GetRelativeTransform());
		PlacementSMC->AttachToComponent(Decoration->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PlacementSMC->SetRelativeLocation(LocalBounds.Origin);
		PlacementSMC->SetRelativeScale3D(LocalBounds.BoxExtent / 50.f * 1.05f);
		PlacementSMC->SetVisibility(true);
	}
	UpdateBuildAsset();
	BeginPlacementFeedback();
	return true;
}

FTransform UPlacementHandler::GetCommittedOfficeTransform(const AActor* Actor) const
{
	return Actor && OfficeMoveActor.Get() == Actor ? OfficeMoveOriginalTransform : Actor ? Actor->GetActorTransform() : FTransform::Identity;
}

void UPlacementHandler::SyncOfficeMoveSeatedWorkers()
{
	AWorkstationActorBase* Desk = Cast<AWorkstationActorBase>(OfficeMoveActor.Get());
	if (!Desk) return;
	// 드래그 중 매 프레임 호출된다. 월드 전체 액터 순회 대신 책상이 쥔 좌석 점유자만 본다.
	const int32 ChairCount = Desk->GetChairCount();
	for (int32 Seat = 0; Seat < ChairCount; ++Seat)
	{
		AOfficeworker* Worker = Desk->GetOccupantAt(Seat);
		if (!IsValid(Worker) || !Worker->IsSeated()) continue;
		const int32 SeatIndex = Worker->GetAssignedSeatIndex();
		Worker->SetActorLocationAndRotation(Desk->GetSeatLocation(SeatIndex), Desk->GetSeatRotation(SeatIndex));
	}
}

bool UPlacementHandler::FinishExistingOfficeMove(bool bAccept)
{
	if (!bExistingOfficeMoveActive) return false;
	AActor* MovingActor = OfficeMoveActor.Get();
	if (bAccept && !MovingActor) return false;
	if (bAccept)
	{
		UpdateBuildAsset();
		if (!CanDrop) return false;
	}
	else if (MovingActor) MovingActor->SetActorTransform(OfficeMoveOriginalTransform);
	SyncOfficeMoveSeatedWorkers();
	if (AWorkstationActorBase* Desk = Cast<AWorkstationActorBase>(MovingActor)) Desk->RefreshNavObstacle();
	UStaticMeshComponent* TargetMesh = IsValid(PlacementWorkstation) ? PlacementWorkstation->DeskMesh
		: IsValid(PlacementDecoration) ? PlacementDecoration->MeshComponent : nullptr;
	if (TargetMesh) TargetMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel2, OfficeMoveOriginalTraceResponse);
	if (MovingActor) MovingActor->Tags = OfficeMoveOriginalTags;
	OfficeMoveActor.Reset();
	bExistingOfficeMoveActive = false;
	EndPlacementFeedback();
	PlacementSMC->SetVisibility(false);
	PlacementSMC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	StopDragging();
	ReleasePlacementIMC();
	PlacementWorkstation = nullptr;
	PlacementDecoration = nullptr;
	CurrentPlacementMode = EPlacementMode::None;
	bHasPlacementTarget = false;
	CanDrop = false;
	UpdateOfficeGridPreview();
	if (bAccept)
	{
		if (USaveLoadManager* Saves = GetWorld()->GetGameInstance()->GetSubsystem<USaveLoadManager>()) Saves->SaveGameData();
	}
	return true;
}
