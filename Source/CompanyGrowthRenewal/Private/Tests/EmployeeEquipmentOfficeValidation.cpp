#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "String/LexFromString.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "GameMode/OfficeGameMode.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Manager/EmployeeManager.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Utils/EmployeeEquipmentAppearanceResolver.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"

namespace
{
    bool bEquipmentOfficeReportPending = false;

    TSharedRef<FJsonObject> EquipmentTotals(const UEmployeeEquipmentManagerSubsystem& Equipment,
        const UTableManagerSubsystem& Tables, int32 EmployeeId)
    {
        auto Totals = MakeShared<FJsonObject>();
        if (const auto* Economy = Tables.GetEmployeeEquipmentEconomy())
        {
            const auto Loadout = FEmployeeEquipmentEffectEvaluator::BuildLoadout(Equipment.GetState(), EmployeeId,
                Tables.GetEmployeeEquipmentDefinitions(), Tables.GetEmployeeEquipmentEffects(), *Economy);
            Totals->SetBoolField(TEXT("valid"), Loadout.bValid);
            auto Basic = MakeShared<FJsonObject>();
            for (const auto& Pair : Loadout.BasicValues) Basic->SetNumberField(Pair.Key.ToString(), Pair.Value);
            Totals->SetObjectField(TEXT("basic_values"), Basic);
            TArray<TSharedPtr<FJsonValue>> Unique;
            for (const auto& Pair : Loadout.UniqueEffects) Unique.Add(MakeShared<FJsonValueString>(Pair.Key.ToString()));
            Totals->SetArrayField(TEXT("unique_effects"), Unique);
        }
        return Totals;
    }

    void EquipOwnedForOfficeValidation(const TArray<FString>& Arguments)
    {
        int32 EmployeeId = INDEX_NONE;
        if (Arguments.Num() != 2 || !LexTryParseString(EmployeeId, *Arguments[0]) || EmployeeId < 0 || bEquipmentOfficeReportPending)
        { UE_LOG(LogTemp, Error, TEXT("[EquipmentOfficeQA] Usage: CGR.EquipmentQA.EquipOwned <employeeid> <definitionid>; wait for any pending report.")); return; }
        UWorld* PlayWorld = nullptr;
        if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::PIE) continue;
            if (PlayWorld) { UE_LOG(LogTemp, Error, TEXT("[EquipmentOfficeQA] Exactly one PIE world is required.")); return; }
            PlayWorld = Context.World();
        }
        auto* Instance = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
        auto* OfficeMode = PlayWorld ? PlayWorld->GetAuthGameMode<AOfficeGameMode>() : nullptr;
        auto* EmployeeManager = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
        auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
        auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        auto* Worker = OfficeMode ? Cast<AFunnyOfficeworker>(OfficeMode->FindEmployeeActorByID(EmployeeId)) : nullptr;
        const FName DefinitionId(*Arguments[1]);
        const auto* Definition = Tables ? Tables->FindEmployeeEquipmentDefinition(DefinitionId) : nullptr;
        if (!EmployeeManager || !EmployeeManager->FindEmployee(EmployeeId) || !Equipment || !Definition || !Worker || Worker->bIsPortraitMode)
        { UE_LOG(LogTemp, Error, TEXT("[EquipmentOfficeQA] Existing employee, owned catalog entry and registered live Funny worker in OfficeMap are required.")); return; }

        const auto* Current = Equipment->GetEquippedInstance(EmployeeId, Definition->EquipmentSlot);
        const bool bNoOp = Current && Current->DefinitionId == DefinitionId;
        FGuid ChosenId = bNoOp ? Current->InstanceId : FGuid();
        if (!bNoOp)
        {
            TArray<FGuid> Available;
            for (const auto& Pair : Equipment->GetState().Instances)
                if (Pair.Value.DefinitionId == DefinitionId && Equipment->GetInstanceEmployee(Pair.Key) == INDEX_NONE) Available.Add(Pair.Key);
            Available.Sort([](const FGuid& Left, const FGuid& Right) { return Left.ToString() < Right.ToString(); });
            if (Available.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("[EquipmentOfficeQA] No unassigned owned %s; nothing changed."), *DefinitionId.ToString()); return; }
            ChosenId = Available[0];
        }
        auto Report = MakeShared<FJsonObject>();
        Report->SetNumberField(TEXT("employee_id"), EmployeeId);
        Report->SetStringField(TEXT("definition_id"), DefinitionId.ToString());
        Report->SetStringField(TEXT("instance_id"), ChosenId.ToString());
        Report->SetBoolField(TEXT("no_op"), bNoOp);
        Report->SetBoolField(TEXT("committed"), false);
        Report->SetNumberField(TEXT("revision_before"), Equipment->GetState().Revision);
        Report->SetObjectField(TEXT("equipment_totals_before"), EquipmentTotals(*Equipment, *Tables, EmployeeId));
        const FString PortraitPath = AOfficeworker::GetPortraitFilePathForEmployee(EmployeeId);
        const FDateTime BeforeTimestamp = IFileManager::Get().GetTimeStamp(*PortraitPath);
        Report->SetStringField(TEXT("portrait_timestamp_before"), BeforeTimestamp.ToIso8601());
        FString PreviousKey;
        FFileHelper::LoadFileToString(PreviousKey, *(PortraitPath + TEXT(".appearance")));
        Report->SetStringField(TEXT("portrait_key_before"), PreviousKey);
        if (!bNoOp)
        {
            auto Request = Equipment->MakeRequest(EEquipmentCommand::SetSlot);
            Request.EmployeeId = EmployeeId; Request.EquipmentSlot = Definition->EquipmentSlot;
            Request.InstanceId = ChosenId; Request.ExpectedSourceEmployeeId = INDEX_NONE;
            const auto Result = Equipment->Execute(Request);
            Report->SetStringField(TEXT("operation_id"), Result.OperationId.ToString());
            Report->SetBoolField(TEXT("committed"), Result.bCommitted);
            Report->SetStringField(TEXT("error_code"), Result.ErrorCode.ToString());
            Report->SetNumberField(TEXT("spent_tickets"), Result.SpentTickets);
            UE_LOG(LogTemp, Display, TEXT("[EquipmentOfficeQA] employee=%d definition=%s committed=%d error=%s revision=%lld"),
                EmployeeId, *DefinitionId.ToString(), Result.bCommitted, *Result.ErrorCode.ToString(), Result.Revision);
            if (!Result.bCommitted) return;
        }
        bEquipmentOfficeReportPending = true;
        const TWeakObjectPtr<UWorld> WeakWorld(PlayWorld);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [WeakWorld, EmployeeId, ChosenId, Report, PortraitPath, BeforeTimestamp, Elapsed = 0.f](float DeltaSeconds) mutable
        {
            Elapsed += DeltaSeconds;
            UWorld* CurrentWorld = WeakWorld.Get();
            auto* CurrentInstance = CurrentWorld ? CurrentWorld->GetGameInstance() : nullptr;
            auto* Employees = CurrentInstance ? CurrentInstance->GetSubsystem<UEmployeeManager>() : nullptr;
            auto* CurrentEquipment = CurrentInstance ? CurrentInstance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
            auto* CurrentTables = CurrentInstance ? CurrentInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
            if (!Employees || !CurrentEquipment || !CurrentTables)
            { bEquipmentOfficeReportPending = false; UE_LOG(LogTemp, Error, TEXT("[EquipmentOfficeQA] World ended before report.")); return false; }
            const bool bPortraitCurrent = Employees->IsPortraitCurrent(EmployeeId);
            if (Elapsed < .25f || (!bPortraitCurrent && Elapsed < 10.f)) return true;
            auto* CurrentMode = CurrentWorld->GetAuthGameMode<AOfficeGameMode>();
            auto* ActualWorker = CurrentMode ? Cast<AFunnyOfficeworker>(CurrentMode->FindEmployeeActorByID(EmployeeId)) : nullptr;
            Report->SetBoolField(TEXT("live_funny_worker"), ActualWorker && !ActualWorker->bIsPortraitMode);
            Report->SetStringField(TEXT("live_actor"), GetPathNameSafe(ActualWorker));
            Report->SetStringField(TEXT("live_class"), ActualWorker ? ActualWorker->GetClass()->GetPathName() : FString());
            Report->SetBoolField(TEXT("instance_equipped"), CurrentEquipment->GetInstanceEmployee(ChosenId) == EmployeeId);
            Report->SetNumberField(TEXT("revision_after"), CurrentEquipment->GetState().Revision);
            Report->SetObjectField(TEXT("equipment_totals_after"), EquipmentTotals(*CurrentEquipment, *CurrentTables, EmployeeId));
            Report->SetStringField(TEXT("equipment_appearance_key"), FEmployeeEquipmentAppearanceResolver::BuildKey(CurrentEquipment->GetState(), CurrentTables->GetEmployeeEquipmentDefinitions(), CurrentTables->GetEmployeeEquipmentAppearances(), EmployeeId));
            Report->SetBoolField(TEXT("portrait_current"), bPortraitCurrent);
            Report->SetStringField(TEXT("portrait_file"), PortraitPath);
            Report->SetNumberField(TEXT("portrait_bytes"), IFileManager::Get().FileSize(*PortraitPath));
            const FDateTime AfterTimestamp = IFileManager::Get().GetTimeStamp(*PortraitPath);
            Report->SetStringField(TEXT("portrait_timestamp_after"), AfterTimestamp.ToIso8601());
            Report->SetBoolField(TEXT("portrait_rewritten"), BeforeTimestamp != AfterTimestamp);
            FString SavedKey; FFileHelper::LoadFileToString(SavedKey, *(PortraitPath + TEXT(".appearance")));
            Report->SetStringField(TEXT("portrait_key_after"), SavedKey);
            TArray<TSharedPtr<FJsonValue>> Meshes;
            if (ActualWorker)
            {
                TInlineComponentArray<USkeletalMeshComponent*> Components; ActualWorker->GetComponents(Components);
                for (const auto* Component : Components)
                {
                    auto Entry = MakeShared<FJsonObject>();
                    Entry->SetStringField(TEXT("component"), Component->GetName());
                    Entry->SetStringField(TEXT("mesh"), GetPathNameSafe(Component->GetSkeletalMeshAsset()));
                    Entry->SetBoolField(TEXT("visible"), Component->IsVisible());
                    Meshes.Add(MakeShared<FJsonValueObject>(Entry));
                }
            }
            Report->SetArrayField(TEXT("live_meshes"), Meshes);
            const FString Directory = FPaths::ProjectSavedDir() / TEXT("EmployeeEquipment/OfficeValidation");
            IFileManager::Get().MakeDirectory(*Directory, true);
            const FString OutputPath = Directory / FString::Printf(TEXT("employee-%d-%s.json"), EmployeeId, *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));
            FString Json; TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
            const bool bSaved = FJsonSerializer::Serialize(Report, Writer) && FFileHelper::SaveStringToFile(Json, *OutputPath);
            UE_LOG(LogTemp, Display, TEXT("[EquipmentOfficeQA] reportSaved=%d portraitCurrent=%d elapsed=%.2f path=%s"), bSaved, bPortraitCurrent, Elapsed, *OutputPath);
            bEquipmentOfficeReportPending = false;
            return false;
        }));
    }

    FAutoConsoleCommand EquipOwnedOfficeValidationCommand(TEXT("CGR.EquipmentQA.EquipOwned"),
        TEXT("Equip one existing unassigned item on a registered OfficeMap employee, then report live meshes, effect totals and saved portrait. Already equipped is read-only."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&EquipOwnedForOfficeValidation));
}
#endif
