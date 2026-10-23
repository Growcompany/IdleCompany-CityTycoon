#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Components/StaticMeshComponent.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Office/OfficeInterior.h"
#include "Office/DecorationActor.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/Components/PlacementHandler.h"

namespace
{
void ValidateCarpetPlacement()
{
	UWorld* PlayWorld = nullptr;
	if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); break; }
	AOfficeCameraPawn* Camera = nullptr;
	AOfficeInterior* Interior = nullptr;
	if (PlayWorld)
	{
		for (TActorIterator<AOfficeCameraPawn> It(PlayWorld); It; ++It) { Camera = *It; break; }
		for (TActorIterator<AOfficeInterior> It(PlayWorld); It; ++It) { Interior = *It; break; }
	}
	UPlacementHandler* Handler = Camera ? Camera->FindComponentByClass<UPlacementHandler>() : nullptr;
	UOfficeManager* Manager = PlayWorld ? PlayWorld->GetSubsystem<UOfficeManager>() : nullptr;
	UTableManagerSubsystem* Tables = PlayWorld && PlayWorld->GetGameInstance() ? PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Handler || !Manager || !Tables || !Interior || Handler->HasActivePlacement() || Camera->IsOfficePlacementEditing())
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeCarpetQA] Requires idle Office PIE"));
		return;
	}
	FDecorationCardTable CarpetCard;
	for (const FDecorationCardTable& Card : Tables->GetDecorationCardInfos())
		if (Card.Category == EDecorationCategory::Carpet) { CarpetCard = Card; break; }
	if (CarpetCard.RowName.IsNone()) { UE_LOG(LogTemp, Error, TEXT("[OfficeCarpetQA] Missing Carpet DT row")); return; }
	int32 Failures = 0;
	const auto Check = [&](const TCHAR* Name, bool bPassed)
	{
		if (!bPassed) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("[OfficeCarpetQA] %s %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Name);
	};
	const float OriginalSnap = Handler->OfficeSnapDistanceRatio;
	Handler->OfficeSnapDistanceRatio = 0.f;
	Handler->SetDecorationPlacementTarget(CarpetCard);
	ADecorationActor* Preview = Handler->GetPlacementDecoration();
	if (!Preview || !Preview->MeshComponent->GetStaticMesh())
	{
		Check(TEXT("preview_mesh_loaded"), false);
		Handler->ReleaseDecorationPlacementTarget();
		Handler->OfficeSnapDistanceRatio = OriginalSnap;
		return;
	}
	const FBox2D Floor = Interior->GetCurrentFloorBoundsLocal();
	FVector ValidPosition;
	bool bFoundPosition = false;
	for (int32 Y = 1; Y < 10 && !bFoundPosition; ++Y) for (int32 X = 1; X < 10 && !bFoundPosition; ++X)
	{
		const FVector2D Local = Floor.Min + Floor.GetSize() * FVector2D(X * 0.1, Y * 0.1);
		Handler->TrackMovePlacement(Interior->GetActorTransform().TransformPosition(FVector(Local, 0)));
		if (Handler->CanDropHere()) { bFoundPosition = true; ValidPosition = Preview->GetActorLocation(); }
	}
	Check(TEXT("inside_floor_allows_carpet"), bFoundPosition);
	if (bFoundPosition)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADecorationActor* Fixture = PlayWorld->SpawnActor<ADecorationActor>(ADecorationActor::StaticClass(), Preview->GetActorTransform(), SpawnParams);
		Fixture->Category = EDecorationCategory::Furniture;
		Fixture->SetDecorationMesh(Preview->MeshComponent->GetStaticMesh());
		Manager->RegisterPlacedDecoration(Fixture);
		const auto Refresh = [&]()
		{
			Handler->TrackMovePlacement(ValidPosition + FVector(10, 0, 0));
			Handler->TrackMovePlacement(ValidPosition);
		};
		Refresh();
		Check(TEXT("carpet_under_furniture"), Handler->CanDropHere());
		Fixture->Category = EDecorationCategory::Carpet;
		Fixture->SetDecorationMesh(Preview->MeshComponent->GetStaticMesh());
		Refresh();
		Check(TEXT("carpet_overlap_rejected_without_physics"), !Handler->CanDropHere());
		Manager->UnregisterPlacedDecoration(Fixture);
		Fixture->Destroy();
		Refresh();
		Check(TEXT("removal_releases_carpet_area"), Handler->CanDropHere());
		Handler->RotatePlacementEntity();
		Handler->TrackMovePlacement(Interior->GetActorTransform().TransformPosition(FVector(Floor.Max + FVector2D(1000), 0)));
		Check(TEXT("rotated_carpet_outside_floor_rejected"), !Handler->CanDropHere());
		Check(TEXT("selection_trace_retained"), Preview->MeshComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block);
		Check(TEXT("drag_trace_retained"), Preview->MeshComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel2) == ECR_Block);
		Check(TEXT("navigation_disabled"), !Preview->MeshComponent->CanEverAffectNavigation());
	}
	Handler->ReleaseDecorationPlacementTarget();
	Handler->OfficeSnapDistanceRatio = OriginalSnap;
	UE_LOG(LogTemp, Display, TEXT("[OfficeCarpetQA] completed failures=%d"), Failures);
}
FAutoConsoleCommand OfficeCarpetQA(TEXT("OfficeCarpet.QA"), TEXT("Validate carpet placement with transient fixtures; no placement commit or save."),
	FConsoleCommandDelegate::CreateStatic(&ValidateCarpetPlacement));
}
#endif
