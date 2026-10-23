#include "Manager/ManufacturingDevelopmentManager.h"

#include "Core/CGGameInstance.h"
#include "Data/BuildingSaveData.h"
#include "Data/EmployeePotentialData.h"
#include "Data/EmployeeStatsData.h"
#include "Data/ManufacturingDevelopmentRules.h"
#include "Data/ManufacturingResearchRules.h"
#include "Table/ProductRecipeTable.h"
#include "Data/FatigueConfig.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Enum/BuildingTraitTarget.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Office/WorkstationUpgradeRules.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "Data/GameSaveData.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "TimerManager.h"

namespace
{
    bool FindManufacturingOffice(const FGameSaveData& GameData, int32 BuildingID, const FOfficeSaveData*& OutOffice,
        const FBuildingSaveData*& OutBuilding)
    {
        OutOffice = GameData.OfficeDataMap.Find(BuildingID);
        OutBuilding = nullptr;
        if (!OutOffice) return false;
        for (const FBuildingEntitySaveData& Entry : GameData.Buildings)
        {
            if (Entry.BuildingIndex == BuildingID)
            {
                OutBuilding = &Entry.BuildingData;
                return IsManufacturingType(Entry.BuildingData.CompanyType);
            }
        }
        return false;
    }

    TArray<FWorkstationSaveData> GetCurrentWorkstations(const UGameInstance* GameInstance,
        const FOfficeSaveData& OfficeData, int32 BuildingID)
    {
        TArray<FWorkstationSaveData> Workstations = OfficeData.OfficeWorkstations;
        const UCGGameInstance* CGInstance = Cast<UCGGameInstance>(GameInstance);
        UWorld* CurrentWorld = GameInstance ? GameInstance->GetWorld() : nullptr;
        if (!CGInstance || !CurrentWorld || CGInstance->GetCurrentManagedBuildingIndex() != BuildingID)
        {
            return Workstations;
        }
        UOfficeManager* OfficeManager = CurrentWorld->GetSubsystem<UOfficeManager>();
        if (!OfficeManager || !OfficeManager->IsInteriorRestored()) return Workstations;
        Workstations.Reset();
        for (const AWorkstationActorBase* Workstation : OfficeManager->GetPlacedWorkstations())
        {
            if (IsValid(Workstation)) Workstations.Add(Workstation->GetSaveData());
        }
        return Workstations;
    }
}

void UManufacturingDevelopmentManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorkConfig = LoadObject<UFatigueConfig>(nullptr, TEXT("/Game/CompanyGrowth/Data/DA_FatigueConfig.DA_FatigueConfig"));
    if (!WorkConfig) UE_LOG(LogTemp, Warning, TEXT("[Manufacturing] Missing DA_FatigueConfig; work accrual is disabled."));
    LastOnlineSeconds = FPlatformTime::Seconds();
    LastOnlineUtc = FDateTime::UtcNow();
    EnterBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
        this, &UManufacturingDevelopmentManager::HandleEnterBackground);
    EnterForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
        this, &UManufacturingDevelopmentManager::HandleEnterForeground);
    GetGameInstance()->GetTimerManager().SetTimer(OnlineTimerHandle, this,
        &UManufacturingDevelopmentManager::AccrueOnline, 1.0f, true);
}

void UManufacturingDevelopmentManager::Deinitialize()
{
    if (UGameInstance* Instance = GetGameInstance()) Instance->GetTimerManager().ClearTimer(OnlineTimerHandle);
    FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(EnterBackgroundHandle);
    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(EnterForegroundHandle);
    Super::Deinitialize();
}

double UManufacturingDevelopmentManager::GetDevelopmentPoints(int32 BuildingID) const
{
    return ManufacturingDevelopmentRules::SumRates(GetDisciplinePoints(BuildingID));
}

double UManufacturingDevelopmentManager::GetPointsPerSecond(int32 BuildingID) const
{
    return ManufacturingDevelopmentRules::SumRates(GetDisciplinePointRates(BuildingID));
}

TArray<double> UManufacturingDevelopmentManager::GetDisciplinePoints(int32 BuildingID) const
{
    TArray<double> Result = {0., 0., 0.};
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    const auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
    const FOfficeSaveData* Office = nullptr;
    const FBuildingSaveData* Building = nullptr;
    if (Save && FindManufacturingOffice(Save->GameData, BuildingID, Office, Building))
        for (int32 Field = 0; Field < 3; ++Field)
            if (Office->ManufacturingDevelopmentPoints.IsValidIndex(Field)
                && FMath::IsFinite(Office->ManufacturingDevelopmentPoints[Field]))
                Result[Field] = FMath::Max(0., Office->ManufacturingDevelopmentPoints[Field]);
    return Result;
}

TArray<double> UManufacturingDevelopmentManager::GetDisciplinePointRates(int32 BuildingID) const
{
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    const auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
    const auto* Office = Save ? Save->GameData.OfficeDataMap.Find(BuildingID) : nullptr;
    if (Office && Office->ManufacturingResearch.ProjectIndex > 0) return {0., 0., 0.};
    float Yield = 1;
    const auto Workers = BuildWorkers(BuildingID, Yield);
    return Office && WorkConfig ? FManufacturingWorkSimulator::EstimateRates(Office->ManufacturingWork, Workers, *WorkConfig, Yield)
        : TArray<double>({0., 0., 0.});
}

FManufacturingResearchStatus UManufacturingDevelopmentManager::GetActiveResearchStatus(int32 BuildingID) const
{
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    const auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
    const auto* Office = Save ? Save->GameData.OfficeDataMap.Find(BuildingID) : nullptr;
    return Office && Office->ManufacturingResearch.ProjectIndex > 0
        ? GetResearchStatus(BuildingID, Office->ManufacturingResearch.ProjectIndex) : FManufacturingResearchStatus();
}

int32 UManufacturingDevelopmentManager::GetPendingResearchResult(int32 BuildingID) const
{
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    const auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
    const auto* Office = Save ? Save->GameData.OfficeDataMap.Find(BuildingID) : nullptr;
    return Office && !Office->PendingManufacturingResearchResults.IsEmpty()
        ? Office->PendingManufacturingResearchResults[0] : INDEX_NONE;
}

bool UManufacturingDevelopmentManager::AcknowledgeResearchResult(int32 BuildingID, int32 ProjectIndex)
{
    const UCGGameInstance* Game = Cast<UCGGameInstance>(GetGameInstance());
    if (Game && Game->IsVisitMode()) return false;
    AccrueOnline();
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
    auto* Office = Save ? Save->GameData.OfficeDataMap.Find(BuildingID) : nullptr;
    if (!Office || ProjectIndex <= 0 || !Office->PendingManufacturingResearchResults.Contains(ProjectIndex)) return false;
    const TArray<int32> Previous = Office->PendingManufacturingResearchResults;
    Office->PendingManufacturingResearchResults.RemoveSingle(ProjectIndex);
    TGuardValue<bool> AccrualGuard(bAccruing, true);
    if (!Saves->SaveGameData())
    {
        Save->GameData.OfficeDataMap.FindChecked(BuildingID).PendingManufacturingResearchResults = Previous;
        return false;
    }
    OnPointsChanged.Broadcast(BuildingID);
    return true;
}

bool UManufacturingDevelopmentManager::GetEmployeeWorkState(int32 BuildingID, int32 EmployeeID, float& OutFatigue, bool& bOutResting) const
{
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    const auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
    const auto* Office = Save ? Save->GameData.OfficeDataMap.Find(BuildingID) : nullptr;
    const auto* Work = Office ? Office->ManufacturingWork.Employees.Find(EmployeeID) : nullptr;
    OutFatigue = Work ? Work->Fatigue : 0;
    bOutResting = Work && Work->Rest != EManufacturingRest::None;
    return Work != nullptr;
}

FManufacturingResearchStatus UManufacturingDevelopmentManager::GetResearchStatus(int32 BuildingID, int32 ProjectIndex) const
{
    FManufacturingResearchStatus Status;
    Status.TeamCapability.Init(0, 3);
    Status.BlockReason = NSLOCTEXT("ManufacturingResearch", "Unavailable", "개발 정보를 확인할 수 없습니다");
    const UGameInstance* Instance = GetGameInstance();
    auto* Saves = Instance ? Instance->GetSubsystem<USaveLoadManager>() : nullptr;
    auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    const auto* Save = Saves ? Saves->GetCurrentSaveData() : nullptr;
    const FOfficeSaveData* Office = nullptr;
    const FBuildingSaveData* Building = nullptr;
    if (!Save || !Tables || !FindManufacturingOffice(Save->GameData, BuildingID, Office, Building)) return Status;
    bool bFound = false;
    const auto Recipe = Tables->GetProductRecipe(Building->CompanyType, ProjectIndex, bFound);
    if (!bFound || Recipe.CompanyType != Building->CompanyType || Recipe.ProjectIndex != ProjectIndex
        || !ManufacturingResearchRules::HasValidDefinition(Recipe.RequiredTeamCapability, Recipe.ResearchWorkSeconds)) return Status;
    Status.bValid = true;
    Status.RequiredCapability = Recipe.RequiredTeamCapability;
    Status.RequiredWorkSeconds = Recipe.ResearchWorkSeconds;
    Status.ActiveProjectIndex = Office->ManufacturingResearch.ProjectIndex;
    Status.bResearched = Office->TierProgress.ClearedProjects.Contains(ProjectIndex);
    Status.bInProgress = !Status.bResearched && Status.ActiveProjectIndex == ProjectIndex;
    Status.CompletedWorkSeconds = Status.bResearched ? Status.RequiredWorkSeconds
        : Status.bInProgress ? static_cast<float>(Office->ManufacturingResearch.CompletedWorkSeconds) : 0.f;
    float Yield = 1.f;
    const auto Workers = BuildWorkers(BuildingID, Yield);
    for (const auto& Worker : Workers)
        for (int32 Field = 0; Field < 3; ++Field)
            Status.TeamCapability[Field] = static_cast<int32>(FMath::Min<int64>(MAX_int32,
                static_cast<int64>(Status.TeamCapability[Field]) + FMath::Max(0, Worker.Capability[Field])));
    Status.BlockReason = FText::GetEmpty();
    if (Status.bResearched) return Status;
    if (const auto* Game = Cast<UCGGameInstance>(Instance); Game && Game->IsVisitMode())
        Status.BlockReason = NSLOCTEXT("ManufacturingResearch", "Visit", "방문 중에는 개발할 수 없습니다");
    else if (FProjectTierProgress::GetTierForProject(ProjectIndex) > Office->TierProgress.CurrentTier)
        Status.BlockReason = NSLOCTEXT("ManufacturingResearch", "TierLocked", "이전 단계의 제품 개발을 완료하세요");
    else if (Status.ActiveProjectIndex > 0 && !Status.bInProgress)
        Status.BlockReason = NSLOCTEXT("ManufacturingResearch", "Busy", "다른 제품을 개발 중입니다");
    else if (Workers.IsEmpty())
        Status.BlockReason = NSLOCTEXT("ManufacturingResearch", "NoTeam", "직원을 책상에 배치하세요");
    else if (!ManufacturingResearchRules::HasCapability(Status.TeamCapability, Status.RequiredCapability))
        Status.BlockReason = NSLOCTEXT("ManufacturingResearch", "Capability", "현재 팀의 분야별 능력이 부족합니다");
    Status.bCanStart = !Status.bInProgress && Status.BlockReason.IsEmpty();
    return Status;
}

bool UManufacturingDevelopmentManager::StartResearch(int32 BuildingID, int32 ProjectIndex, FText& OutFailure)
{
    AccrueOnline();
    const auto Status = GetResearchStatus(BuildingID, ProjectIndex);
    OutFailure = Status.BlockReason;
    if (!Status.bCanStart)
    {
        if (OutFailure.IsEmpty()) OutFailure = NSLOCTEXT("ManufacturingResearch", "AlreadyStarted", "이미 개발 중이거나 개발을 완료한 제품입니다");
        return false;
    }
    auto* Saves = GetGameInstance()->GetSubsystem<USaveLoadManager>();
    auto* Save = Saves->GetCurrentSaveData();
    auto& Office = Save->GameData.OfficeDataMap.FindChecked(BuildingID);
    const auto Previous = Office.ManufacturingResearch;
    Office.ManufacturingResearch.ProjectIndex = ProjectIndex;
    Office.ManufacturingResearch.CompletedWorkSeconds = 0.0;
    // 저장 과정의 온라인 정산이 시작 전 잔여 소수 시간을 개발에 더하지 않게 한다.
    TGuardValue<bool> AccrualGuard(bAccruing, true);
    if (!Saves->SaveGameData())
    {
        Save->GameData.OfficeDataMap.FindChecked(BuildingID).ManufacturingResearch = Previous;
        OutFailure = NSLOCTEXT("ManufacturingResearch", "SaveFailed", "개발 시작을 저장하지 못했습니다");
        return false;
    }
    OnPointsChanged.Broadcast(BuildingID);
    return true;
}

int32 UManufacturingDevelopmentManager::GetRestingEmployeeCount(int32 BuildingID) const
{
    float Yield = 1;
    int32 Count = 0;
    for (const auto& Worker : BuildWorkers(BuildingID, Yield))
    {
        float Fatigue = 0;
        bool bResting = false;
        if (GetEmployeeWorkState(BuildingID, Worker.EmployeeId, Fatigue, bResting) && bResting) ++Count;
    }
    return Count;
}

TArray<FManufacturingWorkerInput> UManufacturingDevelopmentManager::BuildWorkers(int32 BuildingID, float& OutYield) const
{
    OutYield = 1;
    const UGameInstance* Instance = GetGameInstance();
    const UCGGameInstance* CGInstance = Cast<UCGGameInstance>(Instance);
    if (!Instance || (CGInstance && CGInstance->IsVisitMode())) return {};
    USaveLoadManager* SaveManager = Instance->GetSubsystem<USaveLoadManager>();
    USaveGame_GameData* SaveData = SaveManager ? SaveManager->GetCurrentSaveData() : nullptr;
    const FOfficeSaveData* Office = nullptr;
    const FBuildingSaveData* Building = nullptr;
    if (!SaveData || !FindManufacturingOffice(SaveData->GameData, BuildingID, Office, Building)) return {};

    const UEmployeeManager* Employees = Instance->GetSubsystem<UEmployeeManager>();
    const UTableManagerSubsystem* Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
    if (!Employees || !Tables) return {};
    const UEmployeeEquipmentManagerSubsystem* Equipment = Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
    const UBuildingTraitManagerSubsystem* Traits = Instance->GetSubsystem<UBuildingTraitManagerSubsystem>();
    const FEmployeeEquipmentEconomyRow* EquipmentEconomy = Tables->GetEmployeeEquipmentEconomy();
    const TArray<FEmployeeInstance> ActiveEmployees = Employees->GetActiveEmployeesInBuilding(BuildingID);
    if (ActiveEmployees.IsEmpty()) return {};
    TMap<int32, const FEmployeeInstance*> ActiveById;
    for (const FEmployeeInstance& Employee : ActiveEmployees) ActiveById.Add(Employee.EmployeeID, &Employee);

    const TArray<FWorkstationSaveData> Workstations = GetCurrentWorkstations(Instance, *Office, BuildingID);
    TSet<int32> CountedEmployeeIds;
    TArray<FManufacturingWorkerInput> Workers;
    for (const FWorkstationSaveData& Workstation : Workstations)
    {
        const FComputerSetupLevelData* Setup = Tables->FindWorkstationSetupLevelData(Workstation.ComputerSetupLevel);
        if (!Setup) continue;
        const FWorkstationUpgradeDefinition* ProcessingDefinition = Tables->FindWorkstationUpgradeDefinition(EWorkstationUpgradeType::Processing);
        const FWorkstationUpgradeDefinition* ToolsDefinition = Tables->FindWorkstationUpgradeDefinition(EWorkstationUpgradeType::Tools);
        const float ProcessingBonus = ProcessingDefinition ? WorkstationUpgradeRules::GetCumulativeBonus(
            *ProcessingDefinition, Workstation.ComputerSetupLevel, Workstation.UpgradeLevels.FindRef(EWorkstationUpgradeType::Processing)) : 0.f;
        const float ToolsBonus = ToolsDefinition ? WorkstationUpgradeRules::GetCumulativeBonus(
            *ToolsDefinition, Workstation.ComputerSetupLevel, Workstation.UpgradeLevels.FindRef(EWorkstationUpgradeType::Tools)) : 0.f;
        const FWorkstationUpgradeDefinition* EnvironmentDefinition = Tables->FindWorkstationUpgradeDefinition(EWorkstationUpgradeType::Environment);
        const float EnvironmentBonus = EnvironmentDefinition ? WorkstationUpgradeRules::GetCumulativeBonus(
            *EnvironmentDefinition, Workstation.ComputerSetupLevel, Workstation.UpgradeLevels.FindRef(EWorkstationUpgradeType::Environment)) : 0.f;
        const float DeskSpeedFactor = 1.f + FMath::Max(0.f, Setup->WorkSpeedBonusRate + ProcessingBonus);
        for (const FChairSlotState& Chair : Workstation.ChairStates)
        {
            const FEmployeeInstance* const* EmployeePtr = ActiveById.Find(Chair.OccupantEmployeeID);
            if (!EmployeePtr || CountedEmployeeIds.Contains(Chair.OccupantEmployeeID)) continue;
            CountedEmployeeIds.Add(Chair.OccupantEmployeeID);
            const FEmployeeInstance& Employee = **EmployeePtr;
            if (Employee.DisciplinePoints.Num() != 3) continue;
            FManufacturingWorkerInput Input;
            Input.EmployeeId = Employee.EmployeeID;
            Input.Level = Employee.Level;
            Input.PrimarySlot = GetPrimaryDisciplineSlot(Employee);
            Input.Capability = Employee.DisciplinePoints;
            for (int32 Field = 0; Field < 3; ++Field)
                Input.Affinities[Field] = UOfficeStageProgressManager::DisciplineAffinity(
                    Employee.DisciplinePoints.IsValidIndex(Field) ? Employee.DisciplinePoints[Field] : 0);
            const int32 EnhanceBonus = UEmployeeTypeHelper::GetEnhanceStatBonus(Employee.EnhancementLevel);
            const FPotentialModifiers Potential = UEmployeePotentialHelper::AggregateModifiers(Employee.PotentialAbility);
            Input.BaseOutput = UEmployeeTypeHelper::CalculateBaseOutput(Employee.Level);
            Input.WorkRate = (1.f + (Employee.Stats.WorkSpeed + EnhanceBonus) * EmployeeStatTuning::WorkSpeedFactorPerPoint) * DeskSpeedFactor;
            Input.ScoreMultiplier = Potential.ScoreMult * (1.f + ToolsBonus);
            Input.CritChance = EmployeeStatTuning::BaseCritChance + Potential.CritChanceAdd
                + (Employee.Stats.CritChance + EnhanceBonus) * EmployeeStatTuning::CritChancePerPoint
                + (Traits ? Traits->GetAggregatedTraitPercent(BuildingID, EBuildingTraitTarget::CritChance) / 100.f : 0.f);
            Input.CritPower = EmployeeStatTuning::CriticalMultiplier(Employee.Stats.CritDamage + EnhanceBonus, Potential.CritDamageAdd);
            Input.Stamina = Employee.Stats.Stamina + EnhanceBonus;
            Input.Composure = Employee.Stats.Composure + EnhanceBonus;
            Input.Environment = EnvironmentBonus;
            Input.RecoveryMultiplier = Traits ? Traits->GetTraitFactor(BuildingID, EBuildingTraitTarget::FatigueRecovery) : 1.f;
            Input.ExperienceMultiplier = Employees->GetExpGainMultiplier(Employee) * Potential.ExpMult
                * UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType::EmployeeGrowth,
                    Building->EnhancementLevels.FindRef(EBuildingEnhancementType::EmployeeGrowth));
            if (Equipment && EquipmentEconomy)
            {
                if (CachedEquipmentRevision != Equipment->GetState().Revision)
                {
                    EquipmentLoadouts.Reset();
                    CachedEquipmentRevision = Equipment->GetState().Revision;
                }
                if (!EquipmentLoadouts.Contains(Employee.EmployeeID))
                    EquipmentLoadouts.Add(Employee.EmployeeID, FEmployeeEquipmentEffectEvaluator::BuildLoadout(Equipment->GetState(),
                        Employee.EmployeeID, Tables->GetEmployeeEquipmentDefinitions(), Tables->GetEmployeeEquipmentEffects(), *EquipmentEconomy));
                Input.Loadout = EquipmentLoadouts.FindChecked(Employee.EmployeeID);
                if (const auto* Drowsy = Tables->FindEmployeeEquipmentEffect(TEXT("EQF_DROWSY_RECOVERY")))
                    Input.Loadout.Rules.Add(TEXT("DrowsyMinimumDuration"), Drowsy->ScalarParameters.FindRef(TEXT("MinimumDuration")));
                if (const auto* Bolt = Tables->FindEmployeeEquipmentEffect(TEXT("EQF_BOLT_RETURN")))
                    Input.Loadout.Rules.Add(TEXT("BoltMinimumDuration"), Bolt->ScalarParameters.FindRef(TEXT("MinimumDuration")));
            }
            Workers.Add(MoveTemp(Input));
        }
    }
    const int32 YieldLevel = Building->EnhancementLevels.FindRef(EBuildingEnhancementType::ProjectYield);
    const float Yield = UBuildingEnhancementHelper::CalculateEffectMultiplier(EBuildingEnhancementType::ProjectYield, YieldLevel);
    const float Trait = Traits ? Traits->GetTraitFactor(BuildingID, EBuildingTraitTarget::DevScore) : 1.0;
    OutYield = Yield * Trait;
    return Workers;
}

bool UManufacturingDevelopmentManager::AccrueAll(double Seconds, const FDateTime& OnlineStart,
    const FDateTime& OnlineEnd, const FDateTime& OfflineSettledEnd)
{
    if (bAccruing || !FMath::IsFinite(Seconds) || Seconds <= 0.0) return false;
    UGameInstance* Instance = GetGameInstance();
    const UCGGameInstance* CGInstance = Cast<UCGGameInstance>(Instance);
    if (!Instance || (CGInstance && CGInstance->IsVisitMode())) return false;
    USaveLoadManager* SaveManager = Instance->GetSubsystem<USaveLoadManager>();
    USaveGame_GameData* SaveData = SaveManager ? SaveManager->GetCurrentSaveData() : nullptr;
    if (!SaveData) return false;
    bAccruing = true;
    TArray<int32> ChangedBuildings;
    TMap<int32, double> EmployeeExperience;
    TArray<FIntPoint> CompletedProducts;
    for (TPair<int32, FOfficeSaveData>& Pair : SaveData->GameData.OfficeDataMap)
    {
        if (!WorkConfig) continue;
        float Yield = 1;
        const auto Workers = BuildWorkers(Pair.Key, Yield);
        if (Workers.IsEmpty()) continue;
        FManufacturingResearchSimulation Research;
        if (Pair.Value.ManufacturingResearch.ProjectIndex > 0)
        {
            const auto Status = GetResearchStatus(Pair.Key, Pair.Value.ManufacturingResearch.ProjectIndex);
            Research.State = &Pair.Value.ManufacturingResearch;
            Research.TeamCapability = Status.TeamCapability;
            Research.RequiredCapability = Status.RequiredCapability;
            Research.RequiredWorkSeconds = Status.RequiredWorkSeconds;
        }
        const auto Result = FManufacturingWorkSimulator::Advance(Pair.Value.ManufacturingWork,
            Pair.Key, Workers, *WorkConfig, Yield, Seconds, Research.State ? &Research : nullptr);
        Pair.Value.ManufacturingDevelopmentPoints.SetNumZeroed(3);
        for (int32 Field = 0; Field < 3; ++Field)
            Pair.Value.ManufacturingDevelopmentPoints[Field] = ManufacturingDevelopmentRules::Accrue(
                Pair.Value.ManufacturingDevelopmentPoints[Field], Result.Points[Field], 1.);
        for (const auto& Grant : Result.Experience)
            EmployeeExperience.FindOrAdd(Grant.Key) += Grant.Value;
        if (Result.CompletedResearchProjectIndex > 0)
        {
            const int32 CompletedProject = Result.CompletedResearchProjectIndex;
            Pair.Value.TierProgress.RecordProjectClear(CompletedProject);
            Pair.Value.PendingManufacturingResearchResults.AddUnique(CompletedProject);
            CompletedProducts.Add(FIntPoint(Pair.Key, CompletedProject));
        }
        ChangedBuildings.Add(Pair.Key);
    }
    if (!ChangedBuildings.IsEmpty() && OnlineEnd > OnlineStart)
    {
        RecordOnlineInterval(SaveData->GameData, OnlineStart, OnlineEnd);
    }
    if (OfflineSettledEnd.GetTicks() > 0)
    {
        SaveData->GameData.ManufacturingLastOfflineSettledEndUtc = FMath::Max(
            SaveData->GameData.ManufacturingLastOfflineSettledEndUtc, OfflineSettledEnd);
    }
    if (auto* Employees = Instance->GetSubsystem<UEmployeeManager>())
    {
        for (const auto& Grant : EmployeeExperience)
        {
            Employees->CommitResolvedExperience(Grant.Key, Grant.Value);
        }
    }
    for (const auto& Completed : CompletedProducts)
    {
        if (CGInstance && CGInstance->GetCurrentManagedBuildingIndex() == Completed.X && CGInstance->GetWorld()
            && CGInstance->GetWorld()->GetMapName().Contains(TEXT("OfficeMap")))
        {
            if (auto* Stage = CGInstance->GetWorld()->GetSubsystem<UOfficeStageProgressManager>())
            {
                const int32 PreviousTier = Stage->GetTierProgress().CurrentTier;
                const auto& Progress = SaveData->GameData.OfficeDataMap.FindChecked(Completed.X).TierProgress;
                Stage->SetTierProgress(Progress);
                Stage->NotifyManufacturingProductDeveloped(Completed.Y);
                if (Progress.CurrentTier > PreviousTier) Stage->OnTierUnlocked.Broadcast(Progress.CurrentTier);
            }
        }
    }
    if (!CompletedProducts.IsEmpty()) SaveManager->RequestDeferredSave();
    for (const int32 BuildingID : ChangedBuildings) OnPointsChanged.Broadcast(BuildingID);
    bAccruing = false;
    return !ChangedBuildings.IsEmpty();
}

void UManufacturingDevelopmentManager::AccrueOnline()
{
    if (bAccruing || bApplyingOfflineProgress) return;
    const double Now = FPlatformTime::Seconds();
    const FDateTime NowUtc = FDateTime::UtcNow();
    const double Elapsed = Now - LastOnlineSeconds;
    LastOnlineSeconds = Now;
    const FDateTime StartUtc = LastOnlineUtc;
    LastOnlineUtc = NowUtc;
    if (bHasLoadedSave && !bInBackground) AccrueAll(Elapsed, StartUtc, NowUtc);
}

void UManufacturingDevelopmentManager::ResetAfterLoad()
{
    EquipmentLoadouts.Reset();
    CachedEquipmentRevision = -1;
    LastOnlineSeconds = FPlatformTime::Seconds();
    LastOnlineUtc = FDateTime::UtcNow();
    if (!bHasLoadedSave)
    {
        bHasLoadedSave = true;
        if (bHasPendingOfflineProgress)
        {
            const float Seconds = PendingOfflineSeconds;
            const FDateTime IntervalEndUtc = PendingOfflineEndUtc;
            bHasPendingOfflineProgress = false;
            PendingOfflineSeconds = 0.0f;
            PendingOfflineEndUtc = FDateTime();
            ApplyOfflineProgress(Seconds, IntervalEndUtc);
            LastOnlineSeconds = FPlatformTime::Seconds();
            LastOnlineUtc = FDateTime::UtcNow();
        }
    }
}

void UManufacturingDevelopmentManager::ApplyOfflineProgress(float Seconds, const FDateTime& IntervalEndUtc)
{
    if (IntervalEndUtc.GetTicks() <= 0) return;
    if (!bHasLoadedSave)
    {
        const float ValidSeconds = FMath::IsFinite(Seconds) ? FMath::Max(0.f, Seconds) : 0.f;
        if (!bHasPendingOfflineProgress || ValidSeconds > PendingOfflineSeconds
            || (ValidSeconds == PendingOfflineSeconds && IntervalEndUtc > PendingOfflineEndUtc))
        {
            PendingOfflineSeconds = ValidSeconds;
            PendingOfflineEndUtc = IntervalEndUtc;
        }
        bHasPendingOfflineProgress = true;
        return;
    }
    if (bApplyingOfflineProgress || bAccruing || !bOfflineEligible) return;
    // 서버 응답을 기다리는 동안 적립된 온라인 시간을 먼저 같은 UTC ledger에 반영한다.
    AccrueOnline();
    bApplyingOfflineProgress = true;
    bOfflineEligible = false;
    const FDateTime NowUtc = FDateTime::UtcNow();
    if (!FMath::IsFinite(Seconds) || Seconds <= 0.f)
    {
        LastOnlineSeconds = FPlatformTime::Seconds();
        LastOnlineUtc = NowUtc;
        bApplyingOfflineProgress = false;
        return;
    }
    const double Capped = FMath::Min(static_cast<double>(Seconds),
        static_cast<double>(USaveLoadManager::GetOfflineCapSeconds()));
    const FDateTime StartUtc = IntervalEndUtc - FTimespan::FromSeconds(Capped);
    USaveLoadManager* SaveManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveLoadManager>() : nullptr;
    USaveGame_GameData* SaveData = SaveManager ? SaveManager->GetCurrentSaveData() : nullptr;
    if (!SaveData)
    {
        bOfflineEligible = true;
        bApplyingOfflineProgress = false;
        return;
    }
    const double UncoveredSeconds = ManufacturingDevelopmentRules::UncoveredSeconds(
        StartUtc, IntervalEndUtc, SaveData->GameData.ManufacturingOnlineIntervals,
        SaveData->GameData.ManufacturingLastOfflineSettledEndUtc);
    // 공장 및 기존 오프라인 운영 경과와 같은 20% 효율을 적용한다.
    constexpr double OfflineEfficiency = 0.2;
    if (!AccrueAll(UncoveredSeconds * OfflineEfficiency, FDateTime(), FDateTime(), IntervalEndUtc))
    {
        SaveData->GameData.ManufacturingLastOfflineSettledEndUtc = FMath::Max(
            SaveData->GameData.ManufacturingLastOfflineSettledEndUtc, IntervalEndUtc);
    }
    LastOnlineSeconds = FPlatformTime::Seconds();
    LastOnlineUtc = NowUtc;
    bApplyingOfflineProgress = false;
}

void UManufacturingDevelopmentManager::RecordOnlineInterval(FGameSaveData& GameData,
    const FDateTime& Start, const FDateTime& End)
{
    if (End <= Start) return;
    const FDateTime Cutoff = End - FTimespan::FromSeconds(USaveLoadManager::GetOfflineCapSeconds());
    TArray<FManufacturingOnlineInterval>& OnlineIntervals = GameData.ManufacturingOnlineIntervals;
    OnlineIntervals.RemoveAll([&Cutoff](const FManufacturingOnlineInterval& Interval)
    {
        return Interval.EndUtc <= Cutoff;
    });
    if (!OnlineIntervals.IsEmpty() && Start <= OnlineIntervals.Last().EndUtc)
    {
        OnlineIntervals.Last().EndUtc = FMath::Max(End, OnlineIntervals.Last().EndUtc);
    }
    else
    {
        OnlineIntervals.Emplace(Start, End);
    }
    if (!OnlineIntervals.IsEmpty() && OnlineIntervals[0].StartUtc < Cutoff)
    {
        OnlineIntervals[0].StartUtc = Cutoff;
    }
}

void UManufacturingDevelopmentManager::HandleEnterBackground()
{
    AccrueOnline();
    bInBackground = true;
    bOfflineEligible = true;
    LastOnlineSeconds = FPlatformTime::Seconds();
    LastOnlineUtc = FDateTime::UtcNow();
}

void UManufacturingDevelopmentManager::HandleEnterForeground()
{
    bInBackground = false;
    LastOnlineSeconds = FPlatformTime::Seconds();
    LastOnlineUtc = FDateTime::UtcNow();
}
