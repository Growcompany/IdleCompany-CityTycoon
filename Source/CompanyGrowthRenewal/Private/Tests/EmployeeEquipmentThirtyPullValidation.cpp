#if WITH_EDITOR

#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Utils/EmployeeEquipmentTransactionRules.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

// Calculates candidates with the real catalog, but persists only into local variables.
// The live manager is never restored or executed; receipt replay uses a transient manager.
struct FEquipmentThirtyPullValidation
{
    TArray<TSharedPtr<FJsonValue>> Checks;
    bool bPassed = true;
    int32 CatalogDefinitionCount = 0;
    int32 ObservedDefinitionCount = 0;
    int32 TotalSampledDraws = 0;
    bool bHat056Seen = false;

    void Check(const FString& Name, bool bValue)
    {
        auto Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Name); Item->SetBoolField(TEXT("passed"), bValue);
        Checks.Add(MakeShared<FJsonValueObject>(Item)); bPassed &= bValue;
    }

    bool Same(const FGameSaveData& A, const FGameSaveData& B) const
    {
        return FGameSaveData::StaticStruct()->CompareScriptStruct(&A, &B, 0);
    }

    void Run(UWorld* PlayWorld)
    {
        auto* Instance = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
        auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
        auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        const auto* Economy = Tables ? Tables->GetEmployeeEquipmentEconomy() : nullptr;
        Check(TEXT("PIE_catalog_and_manager_available"), Equipment && Tables && Economy);
        if (!Equipment || !Tables || !Economy) { Save(); return; }
        FString CatalogError;
        const bool bCatalogValid = Equipment->IsCatalogValid(CatalogError);
        Check(TEXT("actual_catalog_valid"), bCatalogValid);
        if (!bCatalogValid)
        {
            UE_LOG(LogTemp, Error, TEXT("Equipment catalog invalid: %s"), *CatalogError);
            Save();
            return;
        }
        const auto& Definitions = Tables->GetEmployeeEquipmentDefinitions();
        CatalogDefinitionCount = Definitions.Num();
        const FEmployeeEquipmentState LiveBefore = Equipment->GetState();
        for (const EItemType Tier : {EItemType::EmployeeEquipmentTicketNormal, EItemType::EmployeeEquipmentTicketAdvanced, EItemType::EmployeeEquipmentTicketPremium})
        {
            const FEmployeeEquipmentBannerRow* Banner = nullptr;
            for (const auto& Pair : Tables->GetEmployeeEquipmentBanners())
                if (Pair.Value.TicketType == Tier) { Banner = &Pair.Value; break; }
            const FString Prefix = FString::Printf(TEXT("tier_%d_"), static_cast<int32>(Tier));
            Check(Prefix + TEXT("standard_banner_available"), Banner != nullptr);
            if (!Banner) continue;
            FGameSaveData Seed;
            Seed.ItemInventory.Add(Tier, 100);
            Seed.ResourceBank.Add(EResourceType::Diamond, 77);
            auto& SeedPity = Seed.EmployeeEquipmentState.BannerStates.FindOrAdd(Banner->PityGroupId);
            SeedPity.SinceEpic = Banner->EpicPity - 1;
            SeedPity.SinceLegendary = Banner->LegendaryPity - 2;
            SeedPity.SinceAscendant = Banner->AscendantPity - 3;
            FEmployeeEquipmentRequest Request;
            Request.OperationId = FGuid::NewGuid(); Request.Command = EEquipmentCommand::Pull;
            Request.Count = 30; Request.BannerId = Banner->BannerId; Request.CatalogVersion = Economy->CatalogVersion;
            FGameSaveData Candidate = Seed;
            FEmployeeEquipmentResult Result;
            Result.OperationId = Request.OperationId; Result.CatalogVersion = Request.CatalogVersion;
            const bool bBuilt = Equipment->BuildCandidate(Request, Candidate, Result);
            Check(Prefix + TEXT("actual_thirty_candidate_succeeds"), bBuilt);
            if (!bBuilt) continue;
            Check(Prefix + TEXT("thirty_grants_and_exactly_thirty_matching_tickets"), Result.GrantedInstances.Num() == 30
                && Candidate.EmployeeEquipmentState.Instances.Num() == 30 && Result.SpentTickets == 30
                && Result.TicketType == Tier && Candidate.ItemInventory.FindRef(Tier) == 70
                && Candidate.ResourceBank.FindRef(EResourceType::Diamond) == 77);
            if (Result.GrantedInstances.Num() != 30) continue;
            FEmployeeEquipmentPityState Expected = SeedPity;
            TSet<FGuid> Seen;
            bool bIndividualGuarantees = true;
            for (const auto& Granted : Result.GrantedInstances)
            {
                const auto* Definition = Tables->FindEmployeeEquipmentDefinition(Granted.DefinitionId);
                bIndividualGuarantees &= Definition != nullptr && !Seen.Contains(Granted.InstanceId) && Granted.AcquisitionOperationId == Request.OperationId;
                Seen.Add(Granted.InstanceId);
                if (!Definition) continue;
                const auto Grade = Definition->Grade;
                if (Expected.SinceEpic + 1 >= Banner->EpicPity) bIndividualGuarantees &= Grade >= EEmployeeEquipmentGrade::Epic;
                if (Expected.SinceLegendary + 1 >= Banner->LegendaryPity) bIndividualGuarantees &= Grade >= EEmployeeEquipmentGrade::Legendary;
                if (Expected.SinceAscendant + 1 >= Banner->AscendantPity) bIndividualGuarantees &= Grade == EEmployeeEquipmentGrade::Ascendant;
                Expected.SinceEpic = Grade >= EEmployeeEquipmentGrade::Epic ? 0 : Expected.SinceEpic + 1;
                Expected.SinceLegendary = Grade >= EEmployeeEquipmentGrade::Legendary ? 0 : Expected.SinceLegendary + 1;
                Expected.SinceAscendant = Grade >= EEmployeeEquipmentGrade::Ascendant ? 0 : Expected.SinceAscendant + 1;
            }
            const auto Actual = Candidate.EmployeeEquipmentState.BannerStates.FindRef(Banner->PityGroupId);
            Check(Prefix + TEXT("all_thirty_draws_apply_individual_pity_in_order"), bIndividualGuarantees && Seen.Num() == 30
                && Actual.SinceEpic == Expected.SinceEpic && Actual.SinceLegendary == Expected.SinceLegendary && Actual.SinceAscendant == Expected.SinceAscendant);

            auto Reject = [&](FEmployeeEquipmentRequest Invalid, const FGameSaveData& Before, FName Error, const FString& Name)
            {
                FGameSaveData After = Before; FEmployeeEquipmentResult Rejected;
                const bool bAccepted = Equipment->BuildCandidate(Invalid, After, Rejected);
                Check(Prefix + Name, !bAccepted && Rejected.ErrorCode == Error && Rejected.GrantedInstances.IsEmpty() && Same(After, Before));
            };
            for (int32 Count : {-1, 0, 31})
            {
                auto Invalid = Request; Invalid.Count = Count;
                Reject(Invalid, Seed, TEXT("InvalidPullRequest"), FString::Printf(TEXT("count_%d_rejected_without_changes"), Count));
            }
            FGameSaveData ShortTickets = Seed; ShortTickets.ItemInventory[Tier] = 29;
            Reject(Request, ShortTickets, TEXT("InsufficientTickets"), TEXT("twenty_nine_tickets_reject_whole_batch"));
            FGameSaveData ShortCapacity = Seed;
            for (int32 Index = 0; Index < Economy->InventoryCapacity - 29; ++Index)
            {
                FEmployeeEquipmentInstance Existing; Existing.InstanceId = FGuid::NewGuid(); Existing.DefinitionId = Result.GrantedInstances[0].DefinitionId;
                ShortCapacity.EmployeeEquipmentState.Instances.Add(Existing.InstanceId, Existing);
            }
            Reject(Request, ShortCapacity, TEXT("InventoryFull"), TEXT("twenty_nine_spaces_reject_whole_batch"));

            FGameSaveData Published = Seed;
            bool bCommitting = false; int32 PersistCount = 0, PublishCount = 0;
            Check(Prefix + TEXT("failed_memory_commit_keeps_all_thirty_rewards_unpublished"), !EmployeeEquipmentRules::CommitCandidate(bCommitting, Candidate,
                [&PersistCount](const FGameSaveData&) { ++PersistCount; return false; },
                [&Published](const FGameSaveData& Value) { Published = Value; },
                [&PublishCount](const FGameSaveData&) { ++PublishCount; }) && Same(Published, Seed) && PublishCount == 0);
            Result.bCommitted = true; Result.Revision = 1;
            Candidate.EmployeeEquipmentState.Revision = 1;
            FEmployeeEquipmentOperationReceipt Receipt;
            Receipt.RequestHash = EmployeeEquipmentRules::FingerprintRequest(Request); Receipt.Result = Result;
            Candidate.EmployeeEquipmentState.CompletedOperations.Add(Request.OperationId, Receipt);
            Check(Prefix + TEXT("thirty_batch_publishes_once_after_success"), EmployeeEquipmentRules::CommitCandidate(bCommitting, Candidate,
                [&PersistCount](const FGameSaveData&) { ++PersistCount; return true; },
                [&Published](const FGameSaveData& Value) { Published = Value; },
                [&PublishCount](const FGameSaveData&) { ++PublishCount; }) && Same(Published, Candidate) && PersistCount == 2 && PublishCount == 1);
            TStrongObjectPtr<UGameInstance> TransientInstance(NewObject<UGameInstance>());
            TStrongObjectPtr<UEmployeeEquipmentManagerSubsystem> ReplayManager(NewObject<UEmployeeEquipmentManagerSubsystem>(TransientInstance.Get()));
            ReplayManager->RestoreState(Published.EmployeeEquipmentState);
            const auto Replayed = ReplayManager->Execute(Request);
            Check(Prefix + TEXT("same_operation_replays_thirty_without_another_charge_or_grant"), Replayed.bCommitted && Replayed.GrantedInstances.Num() == 30
                && Replayed.GrantedInstances[0].InstanceId == Result.GrantedInstances[0].InstanceId
                && Replayed.GrantedInstances.Last().InstanceId == Result.GrantedInstances.Last().InstanceId
                && FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&ReplayManager->GetState(), &Published.EmployeeEquipmentState, 0)
                && Published.ItemInventory.FindRef(Tier) == 70 && PersistCount == 2 && PublishCount == 1);
        }
        const FEmployeeEquipmentBannerRow* BestBannerByGrade[6] = {};
        for (const auto& Pair : Tables->GetEmployeeEquipmentBanners())
        {
            const FEmployeeEquipmentBannerRow& Banner = Pair.Value;
            for (int32 GradeIndex = 0; GradeIndex < 6; ++GradeIndex)
            {
                const EEmployeeEquipmentGrade Grade = EEmployeeEquipmentGrade(GradeIndex);
                if (!EmployeeEquipmentRules::IsGradeReachable(Banner, Grade)) continue;
                const FEmployeeEquipmentBannerRow* Current = BestBannerByGrade[GradeIndex];
                if (!Current || Banner.GradeWeights[GradeIndex] > Current->GradeWeights[GradeIndex])
                    BestBannerByGrade[GradeIndex] = &Banner;
            }
        }
        bool bEveryGradeHasBanner = true;
        for (const auto& Pair : Definitions)
            bEveryGradeHasBanner &= BestBannerByGrade[uint8(Pair.Value.Grade)] != nullptr;
        Check(TEXT("every_catalog_grade_has_a_reachable_banner"), bEveryGradeHasBanner);
        TSet<FName> ObservedDefinitions;
        for (const auto& Pair : Tables->GetEmployeeEquipmentBanners())
        {
            const FEmployeeEquipmentBannerRow& Banner = Pair.Value;
            TSet<FName> Missing;
            for (const auto& DefinitionPair : Definitions)
                if (BestBannerByGrade[uint8(DefinitionPair.Value.Grade)] == &Banner)
                    Missing.Add(DefinitionPair.Key);
            if (Missing.IsEmpty()) continue;
            bool bCandidateSucceeded = true;
            constexpr int32 MaxCoverageBatches = 1000;
            for (int32 Batch = 0; Batch < MaxCoverageBatches && !Missing.IsEmpty(); ++Batch)
            {
                FGameSaveData Candidate;
                Candidate.ItemInventory.Add(Banner.TicketType, 30);
                FEmployeeEquipmentRequest Request;
                Request.OperationId = FGuid::NewGuid();
                Request.Command = EEquipmentCommand::Pull;
                Request.Count = 30;
                Request.BannerId = Banner.BannerId;
                Request.CatalogVersion = Economy->CatalogVersion;
                FEmployeeEquipmentResult Result;
                Result.OperationId = Request.OperationId;
                Result.CatalogVersion = Request.CatalogVersion;
                if (!Equipment->BuildCandidate(Request, Candidate, Result) || Result.GrantedInstances.Num() != 30)
                {
                    bCandidateSucceeded = false;
                    UE_LOG(LogTemp, Error, TEXT("Equipment coverage pull failed on %s batch %d: %s"), *Banner.BannerId.ToString(), Batch, *Result.ErrorCode.ToString());
                    break;
                }
                TotalSampledDraws += Result.GrantedInstances.Num();
                for (const FEmployeeEquipmentInstance& Granted : Result.GrantedInstances)
                {
                    ObservedDefinitions.Add(Granted.DefinitionId);
                    Missing.Remove(Granted.DefinitionId);
                }
            }
            if (!Missing.IsEmpty())
            {
                TArray<FString> MissingIds;
                for (FName DefinitionId : Missing) MissingIds.Add(DefinitionId.ToString());
                MissingIds.Sort();
                UE_LOG(LogTemp, Error, TEXT("Equipment coverage missing from %s: %s"), *Banner.BannerId.ToString(), *FString::Join(MissingIds, TEXT(", ")));
            }
            Check(Banner.BannerId.ToString() + TEXT("_all_assigned_definitions_observed_from_real_pull"), bCandidateSucceeded && Missing.IsEmpty());
        }
        ObservedDefinitionCount = ObservedDefinitions.Num();
        Check(TEXT("every_actual_definition_observed_from_real_pull"), ObservedDefinitionCount == CatalogDefinitionCount);
        const FName Hat056(TEXT("EQ_HEAD_HAT_056"));
        bHat056Seen = ObservedDefinitions.Contains(Hat056);
        if (Definitions.Contains(Hat056)) Check(TEXT("head_hat_056_observed_from_real_pull"), bHat056Seen);
        Check(TEXT("actual_live_equipment_state_never_changed"), FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Equipment->GetState(), &LiveBefore, 0));
        Save();
    }

    void Save()
    {
        auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("finished"), true); Report->SetBoolField(TEXT("passed"), bPassed); Report->SetArrayField(TEXT("checks"), Checks);
        Report->SetNumberField(TEXT("catalog_definition_count"), CatalogDefinitionCount);
        Report->SetNumberField(TEXT("observed_definition_count"), ObservedDefinitionCount);
        Report->SetNumberField(TEXT("total_sampled_draws"), TotalSampledDraws);
        Report->SetBoolField(TEXT("head_hat_056_seen"), bHat056Seen);
        Report->SetStringField(TEXT("scope"), TEXT("Actual production BuildCandidate with live readonly catalog; all candidate, persistence and replay state isolated in memory. No SaveLoad commit or live Execute."));
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentThirtyPull"));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("transactions.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("Equipment thirty pull validation %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    }
};

static FAutoConsoleCommandWithWorld GValidateEquipmentThirtyPull(TEXT("EquipmentUI.ValidateThirtyPull"), TEXT("Validate production thirty-pull candidates and receipt replay entirely in memory."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld)
    {
        UWorld* PlayWorld = CommandWorld && CommandWorld->WorldType == EWorldType::PIE ? CommandWorld : nullptr;
        if (!PlayWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); break; }
        FEquipmentThirtyPullValidation Validation; Validation.Run(PlayWorld);
    }));

#endif
