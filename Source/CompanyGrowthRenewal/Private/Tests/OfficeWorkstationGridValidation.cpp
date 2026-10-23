#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/Components/PlacementHandler.h"
#include "Components/PrimitiveComponent.h"
#include "ProceduralMeshComponent.h"
#include "String/LexFromString.h"

namespace
{
bool ReportOfficeGridFootprint(UPlacementHandler* Handler, AOfficeInterior* Interior, const TCHAR* Label)
{
    FBox2D Footprint;
    if (!Handler->GetOfficeWorkstationFootprint(Handler->GetPlacementWorkstation(), Footprint)) return false;
    const FBox2D Floor = Interior->GetCurrentFloorBoundsLocal();
    const FVector2D MinCells = (Footprint.Min - Floor.Min) / UPlacementHandler::OfficeGridCellSize;
    const FVector2D MaxCells = (Footprint.Max - Floor.Min) / UPlacementHandler::OfficeGridCellSize;
    const auto Aligned = [](double Value) { return FMath::Abs(Value - FMath::RoundToDouble(Value)) < 0.001; };
    const bool bAligned = Aligned(MinCells.X) && Aligned(MinCells.Y) && Aligned(MaxCells.X) && Aligned(MaxCells.Y);
    const bool bInside = Footprint.Min.X >= Floor.Min.X - 0.01 && Footprint.Min.Y >= Floor.Min.Y - 0.01 &&
        Footprint.Max.X <= Floor.Max.X + 0.01 && Footprint.Max.Y <= Floor.Max.Y + 0.01;
    UE_LOG(LogTemp, Display, TEXT("[OfficeGridQA] %s footprint_min=%s footprint_max=%s min_cell=%s max_cell=%s size_cells=%s aligned=%d inside=%d can_drop=%d"),
        Label, *Footprint.Min.ToString(), *Footprint.Max.ToString(), *MinCells.ToString(), *MaxCells.ToString(),
        *(MaxCells - MinCells).ToString(), bAligned, bInside, Handler->CanDropHere());
    return bAligned;
}

// 실제 배치 프리뷰만 조작하며 배치 확정이나 세이브는 호출하지 않는다.
void RunOfficeGridQA(const TArray<FString>& Arguments)
{
    if (Arguments.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] Usage: OfficeGrid.QA begin|move <localX> <localY>|rotate|end|status"));
        return;
    }
    UWorld* PlayWorld = nullptr;
    if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); break; }
    }
    if (!PlayWorld) { UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] No PIE world")); return; }
    AOfficeCameraPawn* CameraPawn = nullptr;
    AOfficeInterior* Interior = nullptr;
    for (TActorIterator<AOfficeCameraPawn> It(PlayWorld); It; ++It) { CameraPawn = *It; break; }
    for (TActorIterator<AOfficeInterior> It(PlayWorld); It; ++It) { Interior = *It; break; }
    UPlacementHandler* Handler = CameraPawn ? CameraPawn->FindComponentByClass<UPlacementHandler>() : nullptr;
    if (!Handler || !Interior) { UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] Office pawn/interior missing")); return; }

    const FString& Command = Arguments[0];
    if (Command == TEXT("begin"))
    {
        if (CameraPawn->IsInWorkstationPlacementMode() || Handler->GetPlacementWorkstation())
        { UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] Placement already active")); return; }
        UTableManagerSubsystem* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        bool bStarted = false;
        if (Tables) for (const FWorkstationCardTable& Card : Tables->GetWorkstationCardInfos())
        {
            if (Arguments.Num() > 1 && Card.RowName != FName(*Arguments[1])) continue;
            if (!Card.BlueprintClass.IsNull() && Card.BlueprintClass.LoadSynchronous())
            {
                CameraPawn->BeginWorkstationPlacement(Card);
                bStarted = Handler->GetPlacementWorkstation() != nullptr;
                break;
            }
        }
        if (!bStarted) { UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] No loadable workstation card")); return; }
        Handler->StopDragging();
    }
    else if (Command == TEXT("move"))
    {
        double LocalX = 0.0, LocalY = 0.0;
        if (Arguments.Num() != 3 || !LexTryParseString(LocalX, *Arguments[1]) ||
            !LexTryParseString(LocalY, *Arguments[2]) || !FMath::IsFinite(LocalX) || !FMath::IsFinite(LocalY) ||
            !Handler->GetPlacementWorkstation())
        { UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] move requires active preview and finite local X Y")); return; }
        Handler->StopDragging();
        Handler->TrackMovePlacement(Interior->GetActorTransform().TransformPosition(FVector(LocalX, LocalY, 0.0)));
    }
    else if (Command == TEXT("rotate"))
    {
        if (Handler->GetPlacementWorkstation()) Handler->RotatePlacementEntity();
    }
    else if (Command == TEXT("check"))
    {
        AWorkstationActorBase* Target = Handler->GetPlacementWorkstation();
        if (!Target) { UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] check requires active preview")); return; }
        Handler->StopDragging();
        const FTransform BeforeTurns = Target->GetActorTransform();
        FBox2D BeforeFootprint;
        Handler->GetOfficeWorkstationFootprint(Target, BeforeFootprint);
        bool bAllAligned = ReportOfficeGridFootprint(Handler, Interior, TEXT("initial"));
        for (int32 Turn = 0; Turn < 4; ++Turn)
        {
            Handler->RotatePlacementEntity();
            bAllAligned &= ReportOfficeGridFootprint(Handler, Interior, *FString::Printf(TEXT("turn_%d"), Turn + 1));
        }
        FBox2D Footprint;
        if (!Handler->GetOfficeWorkstationFootprint(Target, Footprint)) return;
        const bool bFourTurnsRestored = Target->GetActorTransform().Equals(BeforeTurns, 0.01) &&
            Footprint.GetCenter().Equals(BeforeFootprint.GetCenter(), 0.01);
        UOfficeManager* OfficeManager = PlayWorld->GetSubsystem<UOfficeManager>();
        if (!OfficeManager) return;
        const FTransform OriginalTransform = Target->GetActorTransform();
        const auto RefreshAtOriginalPosition = [&]()
        {
            // 같은 위치 이동의 조기 반환을 피하고 실제 스냅/배치 검사를 다시 실행한다.
            Target->SetActorLocation(OriginalTransform.GetLocation() + FVector(1.0, 0.0, 0.0));
            Handler->TrackMovePlacement(OriginalTransform.GetLocation());
        };
        const bool bBaseline = Handler->CanDropHere();
        FActorSpawnParameters SpawnParams;
        SpawnParams.ObjectFlags |= RF_Transient;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.bDeferConstruction = true;
        AWorkstationActorBase* Neighbor = PlayWorld->SpawnActor<AWorkstationActorBase>(Target->GetClass(), OriginalTransform, SpawnParams);
        if (Neighbor)
        {
            Neighbor->WorkstationTypeID = Target->WorkstationTypeID;
            Neighbor->FinishSpawning(OriginalTransform);
            OfficeManager->RegisterPlacedWorkstation(Neighbor);
            RefreshAtOriginalPosition();
            const bool bSameCellRejected = !Handler->CanDropHere();
            const FVector AdjacentOffset = Interior->GetActorTransform().TransformVector(FVector(Footprint.GetSize().X, 0.0, 0.0));
            Neighbor->SetActorLocation(OriginalTransform.GetLocation() + AdjacentOffset);
            RefreshAtOriginalPosition();
            const bool bAdjacentAccepted = Handler->CanDropHere();
            OfficeManager->UnregisterPlacedWorkstation(Neighbor);
            Neighbor->Destroy();
            RefreshAtOriginalPosition();
            UE_LOG(LogTemp, Display, TEXT("[OfficeGridQA] check rotation_aligned=%d four_turns_restored=%d baseline_valid=%d same_cell_rejected=%d adjacent_accepted=%d restored_valid=%d"),
                bAllAligned, bFourTurnsRestored, bBaseline, bSameCellRejected, bAdjacentAccepted, Handler->CanDropHere());
        }
    }
    else if (Command == TEXT("end"))
    {
        CameraPawn->EndWorkstationPlacement();
    }
    else if (Command != TEXT("status"))
    { UE_LOG(LogTemp, Warning, TEXT("[OfficeGridQA] Unknown command")); return; }

    const AWorkstationActorBase* Preview = Handler->GetPlacementWorkstation();
    UE_LOG(LogTemp, Display, TEXT("[OfficeGridQA] edit=%d selected=%s dragging=%d"), CameraPawn->IsOfficePlacementEditing(),
        *GetNameSafe(CameraPawn->GetOfficeEditSelection()), Handler->IsDraggingPlacement());
    const FVector LocalPosition = Preview ? Interior->GetActorTransform().InverseTransformPosition(Preview->GetActorLocation()) : FVector::ZeroVector;
    UE_LOG(LogTemp, Display, TEXT("[OfficeGridQA] command=%s active=%d can_drop=%d local=(%.3f,%.3f,%.3f) yaw=%.3f"),
        *Command, Preview != nullptr, Handler->CanDropHere(), LocalPosition.X, LocalPosition.Y, LocalPosition.Z,
        Preview ? Preview->GetActorRotation().Yaw : 0.0);
    if (Preview) ReportOfficeGridFootprint(Handler, Interior, TEXT("status"));
    TInlineComponentArray<UPrimitiveComponent*> Components(CameraPawn);
    for (UPrimitiveComponent* Component : Components)
    {
        if (!Component->GetName().Contains(TEXT("Office"))) continue;
        int32 Vertices = 0, Triangles = 0;
        if (UProceduralMeshComponent* Mesh = Cast<UProceduralMeshComponent>(Component))
        {
            for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
            {
                if (const FProcMeshSection* Section = Mesh->GetProcMeshSection(SectionIndex))
                { Vertices += Section->ProcVertexBuffer.Num(); Triangles += Section->ProcIndexBuffer.Num() / 3; }
            }
        }
        UE_LOG(LogTemp, Display, TEXT("[OfficeGridQA] component=%s visible=%d hidden=%d vertices=%d triangles=%d bounds=%s"),
            *Component->GetName(), Component->IsVisible(), Component->bHiddenInGame, Vertices, Triangles,
            *Component->Bounds.BoxExtent.ToString());
    }
}

FAutoConsoleCommand OfficeGridQACommand(TEXT("OfficeGrid.QA"),
    TEXT("Preview-only Office grid validation: begin [DT row]|move <localX> <localY>|rotate|check|end|status"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&RunOfficeGridQA));
}
#endif
