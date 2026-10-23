#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Office/OfficeInterior.h"
#include "Office/WorkstationActorBase.h"
#include "Office/DecorationActor.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/OfficePlayerController.h"
#include "Player/Components/PlacementHandler.h"
#include "Player/Components/InteractableInputHandler.h"
#include "Components/StaticMeshComponent.h"

namespace
{
void RunOfficeEditQA()
{
    UWorld* PlayWorld = nullptr;
    if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); break; }
    }
    AOfficeCameraPawn* Camera = nullptr;
    AOfficeInterior* Interior = nullptr;
    if (PlayWorld)
    {
        for (TActorIterator<AOfficeCameraPawn> It(PlayWorld); It; ++It) { Camera = *It; break; }
        for (TActorIterator<AOfficeInterior> It(PlayWorld); It; ++It) { Interior = *It; break; }
    }
    UOfficeManager* Manager = PlayWorld ? PlayWorld->GetSubsystem<UOfficeManager>() : nullptr;
    UTableManagerSubsystem* Tables = PlayWorld && PlayWorld->GetGameInstance() ? PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    UPlacementHandler* Handler = Camera ? Camera->FindComponentByClass<UPlacementHandler>() : nullptr;
    AOfficePlayerController* Controller = Camera ? Cast<AOfficePlayerController>(Camera->GetController()) : nullptr;
    if (!Handler || !Interior || !Manager || !Tables || !Controller || Camera->IsOfficePlacementEditing())
    { UE_LOG(LogTemp, Warning, TEXT("[OfficeEditQA] Requires idle PIE Office")); return; }

    const EInputMode InitialMode = Controller->GetCurrentInputMode();
    const int32 InitialDeskCount = Manager->GetPlacedWorkstations().Num();
    const int32 InitialDecorationCount = Manager->GetPlacedDecorations().Num();
    TArray<AActor*> Fixtures;
    const FBox2D Floor = Interior->GetCurrentFloorBoundsLocal();
    const FVector2D FloorCenter = Floor.GetCenter();
    const auto FloorPosition = [&](double XOffset, double YOffset)
    { return Interior->GetActorTransform().TransformPosition(FVector(FloorCenter.X + XOffset, FloorCenter.Y + YOffset, 0.0)); };
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction = true;

    AWorkstationActorBase* Desk = nullptr;
    for (const FWorkstationCardTable& Card : Tables->GetWorkstationCardInfos())
    {
        UClass* DeskClass = Card.BlueprintClass.IsNull() ? nullptr : Card.BlueprintClass.LoadSynchronous();
        if (!DeskClass) continue;
        const FTransform SpawnTransform(FRotator::ZeroRotator, FloorPosition(-200.0, 0.0));
        Desk = PlayWorld->SpawnActor<AWorkstationActorBase>(DeskClass, SpawnTransform, Params);
        if (Desk)
        {
            Desk->WorkstationTypeID = Card.RowName;
            Desk->FinishSpawning(SpawnTransform);
            Desk->InstanceID = TEXT("OfficeEditQA_TransientDesk");
            Desk->CurrentSetupLevel = EComputerSetupLevel::Level2;
            Manager->RegisterPlacedWorkstation(Desk);
            Fixtures.Add(Desk);
        }
        break;
    }
    for (const EDecorationCategory Category : { EDecorationCategory::Plant, EDecorationCategory::Picture })
    {
        for (const FDecorationCardTable& Card : Tables->GetDecorationCardInfos())
        {
            if (Card.Category != Category) continue;
            bool bFound = false;
            const FDecorationData Data = Tables->GetDecorationData(Card.RowName, bFound);
            UStaticMesh* Mesh = bFound && !Data.DecorationMesh.IsNull() ? Data.DecorationMesh.LoadSynchronous() : nullptr;
            if (!Mesh) continue;
            const bool bWall = Card.AllowedSurface == EDecorationSurface::Wall;
            FVector SpawnPosition = FloorPosition(250.0, 0.0);
            if (bWall)
            {
                FVector WallMin, WallMax;
                Interior->GetWallPlacementBounds(EWallSide::Left, WallMin, WallMax);
                SpawnPosition = (WallMin + WallMax) * 0.5;
                SpawnPosition.Y = Interior->GetWallLocation(EWallSide::Left).Y + 2.0;
            }
            const FTransform SpawnTransform(FRotator(0.0, bWall ? 180.0 : 0.0, 0.0), SpawnPosition);
            ADecorationActor* Decoration = PlayWorld->SpawnActor<ADecorationActor>(ADecorationActor::StaticClass(), SpawnTransform, Params);
            if (Decoration)
            {
                Decoration->DecorationCardTableRowName = Card.RowName;
                Decoration->Category = Card.Category;
                Decoration->AllowedSurface = Card.AllowedSurface;
                Decoration->FinishSpawning(SpawnTransform);
                Decoration->SetDecorationMesh(Mesh);
                Manager->RegisterPlacedDecoration(Decoration);
                Fixtures.Add(Decoration);
            }
            break;
        }
    }

    int32 Failures = 0;
    const auto Check = [&](const TCHAR* Name, bool bPassed)
    {
        UE_LOG(LogTemp, Display, TEXT("[OfficeEditQA] %s=%d"), Name, bPassed);
        if (!bPassed) ++Failures;
    };
    Check(TEXT("fixtures_created"), Fixtures.Num() == 3);
    const bool bEntered = Camera->BeginOfficePlacementEdit();
    Check(TEXT("edit_entered"), bEntered);
    if (bEntered && Desk && Desk->DeskMesh)
    {
        UInteractableInputHandler* Interaction = Camera->FindComponentByClass<UInteractableInputHandler>();
        FVector2D DeskScreen;
        const bool bProjected = Controller->ProjectWorldLocationToScreen(Desk->DeskMesh->Bounds.Origin, DeskScreen);
        Check(TEXT("press_desk_projected"), bProjected && Interaction != nullptr);
        if (bProjected && Interaction)
        {
            double OriginalMouseX = 0.0, OriginalMouseY = 0.0;
            const bool bHadMouse = Controller->GetMousePosition(OriginalMouseX, OriginalMouseY);
            Controller->SetMouseLocation(FMath::RoundToInt(DeskScreen.X), FMath::RoundToInt(DeskScreen.Y));
            // release 전 실제 좌표 투영/히트 검사/Started 경로에서 바로 선택과 드래그가 성립해야 한다.
            Interaction->OnInteractionStarted(FInputActionValue(true));
            Check(TEXT("press_selects_before_release"), Camera->GetOfficeEditSelection() == Desk);
            Check(TEXT("same_press_starts_drag"), Handler->IsDraggingPlacement());
            Interaction->OnInteractionFinish(FInputActionValue(false));
            Check(TEXT("release_keeps_selection"), Camera->GetOfficeEditSelection() == Desk);
            Check(TEXT("release_stops_drag"), !Handler->IsDraggingPlacement());
            Camera->CancelOfficeEditSelection();
            if (bHadMouse) Controller->SetMouseLocation(FMath::RoundToInt(OriginalMouseX), FMath::RoundToInt(OriginalMouseY));
        }
    }
    if (bEntered) for (AActor* Fixture : Fixtures)
    {
        const FTransform Original = Fixture->GetActorTransform();
        const FString OriginalInstanceID = Desk ? Desk->InstanceID : FString();
        const FWorkstationSaveData OriginalDesk = Desk ? Desk->GetSaveData() : FWorkstationSaveData();
        const auto SnapshotMatches = [&](const FTransform& Expected)
        {
            FOfficeSaveData Snapshot;
            Manager->FillOfficeSaveData(Snapshot);
            if (Fixture == Desk)
            {
                for (const FWorkstationSaveData& Saved : Snapshot.OfficeWorkstations)
                {
                    if (Saved.WorkstationTypeID == Desk->WorkstationTypeID && Saved.Transform.Equals(Expected, 0.01)) return true;
                }
            }
            else if (const ADecorationActor* Decoration = Cast<ADecorationActor>(Fixture))
            {
                for (const FDecorationSaveData& Saved : Snapshot.PlacedDecorations)
                {
                    if (Saved.RowName == Decoration->DecorationCardTableRowName && Saved.Transform.Equals(Expected, 0.01)) return true;
                }
            }
            return false;
        };
        UE_LOG(LogTemp, Display, TEXT("[OfficeEditQA] fixture=%s"), *Fixture->GetName());
        const bool bSelected = Camera->SelectOfficeEditActor(Fixture);
        Check(TEXT("selected"), bSelected);
        if (!bSelected) continue;
        Handler->StopDragging();
        Camera->RotateOfficeEditSelection();
        Check(TEXT("preview_snapshot_preserves_original"), SnapshotMatches(Original));
        Camera->CancelOfficeEditSelection();
        Check(TEXT("cancel_restores_transform"), Fixture->GetActorTransform().Equals(Original, 0.01));
        Check(TEXT("cancel_keeps_actor"), IsValid(Fixture));

        if (!Camera->SelectOfficeEditActor(Fixture)) { Check(TEXT("reselect"), false); continue; }
        Handler->StopDragging();
        const ADecorationActor* Decoration = Cast<ADecorationActor>(Fixture);
        if (Decoration && Decoration->AllowedSurface == EDecorationSurface::Wall)
        {
            FVector2D ScreenPosition;
            if (Controller->ProjectWorldLocationToScreen(Fixture->GetActorLocation() + FVector(100.0, 0.0, 0.0), ScreenPosition))
                Handler->TrackMovePlacement(ScreenPosition);
            Camera->RotateOfficeEditSelection();
        }
        else
        {
            // 실 배치 검증기를 거치는 제한된 바닥 후보 탐색.
            for (int32 Candidate = 0; Candidate < 25; ++Candidate)
            {
                Handler->TrackMovePlacement(FloorPosition((Candidate % 5 - 2) * 150.0, (Candidate / 5 - 2) * 150.0));
                if (Handler->CanDropHere()) break;
            }
        }
        Check(TEXT("moving_snapshot_preserves_original"), SnapshotMatches(Original));
        const FTransform CandidateTransform = Fixture->GetActorTransform();
        const bool bCommitted = Camera->ConfirmOfficeEditSelection();
        Check(TEXT("confirmed"), bCommitted);
        if (bCommitted) Check(TEXT("committed_snapshot_matches"), SnapshotMatches(CandidateTransform));
        else Camera->CancelOfficeEditSelection();
        if (Desk)
        {
            FWorkstationSaveData AfterDesk = Desk->GetSaveData();
            AfterDesk.Transform = OriginalDesk.Transform;
            Check(TEXT("desk_id_chairs_upgrade_preserved"), Desk->InstanceID == OriginalInstanceID &&
                FWorkstationSaveData::StaticStruct()->CompareScriptStruct(&OriginalDesk, &AfterDesk, 0));
        }
    }
    if (bEntered && Desk)
    {
        Check(TEXT("remove_desk_selected"), Camera->SelectOfficeEditActor(Desk));
        Check(TEXT("remove_desk_rejected"), !Camera->CanRemoveOfficeEditSelection() && !Camera->RemoveOfficeEditSelection()
            && Camera->GetOfficeEditSelection() == Desk && Manager->GetPlacedWorkstations().Contains(Desk));
        Camera->CancelOfficeEditSelection();
    }
    if (bEntered) for (AActor* Fixture : Fixtures)
    {
        ADecorationActor* Decoration = Cast<ADecorationActor>(Fixture);
        if (!Decoration) continue;
        const bool bWall = Decoration->AllowedSurface == EDecorationSurface::Wall;
        const int32 CountBefore = Manager->GetPlacedDecorations().Num();
        Check(TEXT("remove_decoration_selected"), Camera->SelectOfficeEditActor(Decoration));
        Check(TEXT("remove_decoration_allowed"), Camera->CanRemoveOfficeEditSelection());
        const bool bRemoved = Camera->RemoveOfficeEditSelection();
        Check(bWall ? TEXT("wall_removed") : TEXT("floor_removed"), bRemoved && !IsValid(Decoration)
            && !Manager->GetPlacedDecorations().Contains(Decoration));
        FOfficeSaveData RemovedSnapshot;
        Manager->FillOfficeSaveData(RemovedSnapshot);
        Check(TEXT("removed_snapshot_count"), RemovedSnapshot.PlacedDecorations.Num() == CountBefore - 1);
        Check(TEXT("remove_cleans_selection"), Camera->IsOfficePlacementEditing() && !Camera->GetOfficeEditSelection()
            && !Camera->IsInWallEditMode() && !Handler->HasActivePlacement()
            && Controller->GetCurrentInputMode() == EInputMode::Normal);
        Check(TEXT("repeated_remove_safe"), !Camera->CanRemoveOfficeEditSelection() && !Camera->RemoveOfficeEditSelection());
    }
    FOfficeSaveData EmptyDecorationSnapshot;
    EmptyDecorationSnapshot.PendingStarterPreset = NAME_None;
    Check(TEXT("empty_decorations_do_not_reseed"), !Manager->ApplyStarterPresetToData(EmptyDecorationSnapshot, ECompanyType{})
        && EmptyDecorationSnapshot.PlacedDecorations.IsEmpty());
    Camera->EndOfficePlacementEdit();
    Check(TEXT("edit_exited"), !Camera->IsOfficePlacementEditing() && Camera->GetOfficeEditSelection() == nullptr);
    Check(TEXT("input_mode_restored"), Controller->GetCurrentInputMode() == InitialMode);
    for (AActor* Fixture : Fixtures)
    {
        if (!IsValid(Fixture)) continue;
        if (AWorkstationActorBase* Workstation = Cast<AWorkstationActorBase>(Fixture)) Manager->UnregisterPlacedWorkstation(Workstation);
        if (ADecorationActor* Decoration = Cast<ADecorationActor>(Fixture)) Manager->UnregisterPlacedDecoration(Decoration);
        Fixture->Destroy();
    }
    Check(TEXT("fixture_cleanup"), Manager->GetPlacedWorkstations().Num() == InitialDeskCount && Manager->GetPlacedDecorations().Num() == InitialDecorationCount);
    UE_LOG(LogTemp, Display, TEXT("[OfficeEditQA] completed failures=%d"), Failures);
}
FAutoConsoleCommand OfficeEditQACommand(TEXT("OfficeEdit.QA"),
    TEXT("Run bounded Office edit fixture validation. Uses real confirm/save path; back up game save first."),
    FConsoleCommandDelegate::CreateStatic(&RunOfficeEditQA));
}
#endif
