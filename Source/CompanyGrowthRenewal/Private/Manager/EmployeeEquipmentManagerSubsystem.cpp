#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Utils/EmployeeEquipmentTransactionRules.h"
#include "Misc/PackageName.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace
{
    int32 FindOwner(const FEmployeeEquipmentState& InState, const FGuid& InstanceId)
    {
        if (!InstanceId.IsValid()) return INDEX_NONE;
        for (const auto& EmployeePair : InState.Loadouts)
            for (const auto& SlotPair : EmployeePair.Value.Slots)
                if (SlotPair.Value == InstanceId) return EmployeePair.Key;
        return INDEX_NONE;
    }

    bool Fail(FEmployeeEquipmentResult& Result, FName Error)
    {
        Result.ErrorCode = Error;
        return false;
    }

    bool AddItems(FGameSaveData& Candidate, EItemType Item, int32 Quantity)
    {
        if (Item == EItemType::None || Item >= EItemType::Count || Quantity <= 0) return false;
        int32& Balance = Candidate.ItemInventory.FindOrAdd(Item);
        if (Balance < 0 || Balance > MAX_int32 - Quantity) return false;
        Balance += Quantity;
        return true;
    }

    bool IsBannerActive(const FEmployeeEquipmentBannerRow& Banner, const FDateTime& Now)
    {
        return (Banner.StartsAtUtc.GetTicks() <= 0 || Now >= Banner.StartsAtUtc) &&
            (Banner.EndsAtUtc.GetTicks() <= 0 || Now < Banner.EndsAtUtc);
    }

    bool BuildGradePools(const UTableManagerSubsystem& Tables, const FEmployeeEquipmentBannerRow& Banner, TArray<FName> (&GradePools)[6])
    {
        if (!EmployeeEquipmentRules::ValidateBannerSettings(Banner) || Banner.DefinitionIds.IsEmpty()) return false;
        TSet<FName> Seen;
        for (FName DefinitionId : Banner.DefinitionIds)
        {
            const FEmployeeEquipmentDefinitionRow* Definition = Tables.FindEmployeeEquipmentDefinition(DefinitionId);
            if (!Definition || Definition->Grade >= EEmployeeEquipmentGrade::Count || Seen.Contains(DefinitionId)) return false;
            Seen.Add(DefinitionId);
            GradePools[uint8(Definition->Grade)].Add(DefinitionId);
        }
        for (int32 Index = 0; Index < 6; ++Index)
            if (EmployeeEquipmentRules::IsGradeReachable(Banner, EEmployeeEquipmentGrade(Index)) && GradePools[Index].IsEmpty()) return false;
        return true;
    }
}

void UEmployeeEquipmentManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UTableManagerSubsystem>();
    Collection.InitializeDependency<USaveLoadManager>();
    if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        SaveMgr->OnGameDataLoaded.AddUObject(this, &UEmployeeEquipmentManagerSubsystem::HandleLoaded);
}

void UEmployeeEquipmentManagerSubsystem::Deinitialize()
{
    if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>()) SaveMgr->OnGameDataLoaded.RemoveAll(this);
    if (UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>()) Missions->OnMissionCompleted.Remove(MissionCompletedHandle);
    MissionCompletedHandle.Reset();
    Super::Deinitialize();
}

void UEmployeeEquipmentManagerSubsystem::HandleLoaded()
{
    State = FEmployeeEquipmentState();
    if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        if (const USaveGame_GameData* Save = SaveMgr->GetCurrentSaveData()) RestoreState(Save->GameData.EmployeeEquipmentState);
    // 미션 매니저가 직원 매니저를 거쳐 이 매니저에 의존한다 ― InitializeDependency 대신 로드 뒤 한 번 구독한다
    if (!MissionCompletedHandle.IsValid())
        if (UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
            MissionCompletedHandle = Missions->OnMissionCompleted.AddUObject(this, &UEmployeeEquipmentManagerSubsystem::HandleMissionCompleted);
    OnEquipmentChanged.Broadcast();
}

void UEmployeeEquipmentManagerSubsystem::HandleMissionCompleted(FName MissionId, const FMissionTable& Mission)
{
    const UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (Missions && Missions->IsTutorialCompleted()) TryGrantStarterTickets();
}

void UEmployeeEquipmentManagerSubsystem::RestoreState(const FEmployeeEquipmentState& InState) { State = InState; }
const FEmployeeEquipmentInstance* UEmployeeEquipmentManagerSubsystem::FindInstance(const FGuid& InstanceId) const { return State.Instances.Find(InstanceId); }
int32 UEmployeeEquipmentManagerSubsystem::GetInstanceEmployee(const FGuid& InstanceId) const { return FindOwner(State, InstanceId); }

const FEmployeeEquipmentInstance* UEmployeeEquipmentManagerSubsystem::GetEquippedInstance(int32 EmployeeId, EEmployeeEquipmentSlot EquipmentSlot) const
{
    const FEmployeeEquipmentLoadout* Loadout = State.Loadouts.Find(EmployeeId);
    return Loadout ? FindInstance(Loadout->Slots.FindRef(EquipmentSlot)) : nullptr;
}

FEquipmentAppearanceSelection UEmployeeEquipmentManagerSubsystem::GetAppearanceSelection(int32 EmployeeId, EEmployeeEquipmentSlot EquipmentSlot) const
{
    const FEmployeeEquipmentAppearanceSelections* Choices = State.AppearanceSelections.Find(EmployeeId);
    return Choices ? Choices->Slots.FindRef(EquipmentSlot) : FEquipmentAppearanceSelection();
}

FEmployeeEquipmentRequest UEmployeeEquipmentManagerSubsystem::MakeRequest(EEquipmentCommand Command) const
{
    FEmployeeEquipmentRequest Request;
    Request.Command = Command;
    Request.OperationId = FGuid::NewGuid();
    Request.ExpectedRevision = State.Revision;
    if (const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
        if (const FEmployeeEquipmentEconomyRow* Economy = Tables->GetEmployeeEquipmentEconomy()) Request.CatalogVersion = Economy->CatalogVersion;
    return Request;
}

bool UEmployeeEquipmentManagerSubsystem::TryGrantRecruitmentGift(int32 EmployeeId)
{
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentEconomyRow* Economy = Tables ? Tables->GetEmployeeEquipmentEconomy() : nullptr;
    if (!Economy || Economy->RecruitGiftChancePercent <= 0) return false;
    // 굴림이 빗나가면 트랜잭션 자체를 열지 않는다 — 85% 는 세이브 비용이 0 이다.
    if (FMath::RandRange(1, 100) > Economy->RecruitGiftChancePercent) return false;
    FEmployeeEquipmentRequest Request = MakeRequest(EEquipmentCommand::RecruitGift);
    Request.EmployeeId = EmployeeId;
    return Execute(Request).bCommitted;
}

bool UEmployeeEquipmentManagerSubsystem::TryGrantStarterTickets()
{
    if (State.bStarterClaimed) return false;
    if (!Execute(MakeRequest(EEquipmentCommand::ClaimStarter)).bCommitted) return false;
    // 튜토리얼 마지막 수령의 보상 창은 이 지급 뒤 같은 호출 흐름에서 뜬다 ― 한 틱 뒤 창이 닫혔는지 보고 알린다
    GetGameInstance()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UEmployeeEquipmentManagerSubsystem::ShowStarterNoticeWhenFree));
    return true;
}

void UEmployeeEquipmentManagerSubsystem::ShowStarterNoticeWhenFree()
{
    UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    if (Missions && Missions->IsRewardSplashShown())
    {
        if (!StarterNoticeHandle.IsValid())
            StarterNoticeHandle = Missions->OnRewardSplashClosed.AddUObject(this, &UEmployeeEquipmentManagerSubsystem::ShowStarterNoticeWhenFree);
        return;
    }
    if (Missions && StarterNoticeHandle.IsValid()) Missions->OnRewardSplashClosed.Remove(StarterNoticeHandle);
    StarterNoticeHandle.Reset();
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentEconomyRow* Economy = Tables ? Tables->GetEmployeeEquipmentEconomy() : nullptr;
    UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    if (UIMgr && Economy)
        UIMgr->ShowNotification(FText::Format(NSLOCTEXT("EmployeeEquipment", "StarterGranted", "첫 장비 상자 {0}개를 받았습니다"),
            FText::AsNumber(Economy->StarterTickets)), 3.0f, ENotificationType::Success);
}

FEmployeeEquipmentResult UEmployeeEquipmentManagerSubsystem::GetOperation(const FGuid& OperationId) const
{
    if (const FEmployeeEquipmentOperationReceipt* Receipt = State.CompletedOperations.Find(OperationId)) return Receipt->Result;
    FEmployeeEquipmentResult Result; Result.OperationId = OperationId; Result.ErrorCode = TEXT("OperationNotFound"); return Result;
}

bool UEmployeeEquipmentManagerSubsystem::IsCatalogValid(FString& OutError) const
{
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentEconomyRow* Economy = Tables ? Tables->GetEmployeeEquipmentEconomy() : nullptr;
    if (!Economy || Economy->CatalogVersion.IsNone() || Economy->InventoryCapacity <= 0 || Economy->MaxEnhanceLevel <= 0 ||
        Economy->PrimaryEnhancePerLevel <= 0 || Economy->SecondaryScale <= 0 || Economy->StarterTickets <= 0 ||
        Economy->RecruitGiftChancePercent < 0 || Economy->RecruitGiftChancePercent > 100 ||
        Economy->RecruitGiftGrade >= EEmployeeEquipmentGrade::Count)
    { OutError = TEXT("Missing or invalid equipment economy"); return false; }
    if (Tables->GetEmployeeEquipmentDefinitions().IsEmpty() || Tables->GetEmployeeEquipmentEffects().Num() != 59)
    { OutError = TEXT("Equipment catalog requires definitions and 59 effects"); return false; }
    for (uint8 GradeIndex = 0; GradeIndex < uint8(EEmployeeEquipmentGrade::Count); ++GradeIndex)
    {
        const FEmployeeEquipmentGradeRow* Grade = Tables->FindEmployeeEquipmentGrade(EEmployeeEquipmentGrade(GradeIndex));
        if (!Grade || Grade->DismantleCores <= 0 || Grade->EnhanceCoreCost <= 0)
        { OutError = TEXT("Missing or invalid equipment grade"); return false; }
    }
    for (const auto& Pair : Tables->GetEmployeeEquipmentEffects())
    {
        const FEmployeeEquipmentEffectRow& Effect = Pair.Value;
        if (Effect.EffectId != Pair.Key || Effect.DisplayName.IsEmpty() || Effect.Description.IsEmpty() || Effect.Hook.IsNone())
        { OutError = Pair.Key.ToString() + TEXT(": invalid effect metadata"); return false; }
        if (Effect.bBasic && (Effect.GradeValues.Num() != 6 || Effect.AggregateCap <= 0))
        { OutError = Pair.Key.ToString() + TEXT(": invalid basic effect scales"); return false; }
        for (float Value : Effect.GradeValues)
            if (!FMath::IsFinite(Value) || Value <= 0) { OutError = Pair.Key.ToString() + TEXT(": invalid effect value"); return false; }
        for (const auto& Parameter : Effect.ScalarParameters)
            if (Parameter.Key.IsNone() || !FMath::IsFinite(Parameter.Value)) { OutError = Pair.Key.ToString() + TEXT(": invalid effect parameter"); return false; }
    }
    int32 SlotCounts[6] = {};
    int32 GradeCounts[6] = {};
    for (const auto& Pair : Tables->GetEmployeeEquipmentDefinitions())
    {
        const FEmployeeEquipmentDefinitionRow& Def = Pair.Value;
        if (Def.DefinitionId != Pair.Key || Def.DisplayName.IsEmpty() || Def.EquipmentSlot >= EEmployeeEquipmentSlot::Count || Def.Grade >= EEmployeeEquipmentGrade::Count)
        { OutError = Pair.Key.ToString() + TEXT(": invalid definition"); return false; }
        ++SlotCounts[uint8(Def.EquipmentSlot)]; ++GradeCounts[uint8(Def.Grade)];
        const int32 ExpectedUnique = Def.Grade >= EEmployeeEquipmentGrade::Legendary ? 2 : (Def.Grade >= EEmployeeEquipmentGrade::Rare ? 1 : 0);
        if (Def.UniqueEffectIds.Num() != ExpectedUnique || (Def.SecondaryEffectId.IsNone() != (Def.Grade == EEmployeeEquipmentGrade::Common)))
        { OutError = Pair.Key.ToString() + TEXT(": effect composition mismatch"); return false; }
        const FEmployeeEquipmentEffectRow* Primary = Tables->FindEmployeeEquipmentEffect(Def.PrimaryEffectId);
        const FEmployeeEquipmentEffectRow* Secondary = Tables->FindEmployeeEquipmentEffect(Def.SecondaryEffectId);
        if (!Primary || !Primary->bBasic || Primary->GradeValues.Num() != 6 || (!Def.SecondaryEffectId.IsNone() && (!Secondary || !Secondary->bBasic || Secondary->GradeValues.Num() != 6)) || Def.PrimaryEffectId == Def.SecondaryEffectId)
        { OutError = Pair.Key.ToString() + TEXT(": invalid basic effects"); return false; }
        for (FName EffectId : Def.UniqueEffectIds)
        {
            const FEmployeeEquipmentEffectRow* Effect = Tables->FindEmployeeEquipmentEffect(EffectId);
            if (!Effect || Effect->bBasic) { OutError = Pair.Key.ToString() + TEXT(": missing unique effect"); return false; }
        }
        const FEmployeeEquipmentAppearanceRow* Appearance = Tables->FindEmployeeEquipmentAppearance(Def.AppearanceId);
        if (!Appearance || Appearance->EquipmentSlot != Def.EquipmentSlot || Appearance->PartOverrides.IsEmpty())
        { OutError = Pair.Key.ToString() + TEXT(": missing appearance"); return false; }
        if (Def.Icon.IsNull() || !FPackageName::DoesPackageExist(Def.Icon.ToSoftObjectPath().GetLongPackageName()))
        { OutError = Pair.Key.ToString() + TEXT(": missing icon package"); return false; }
        for (const FEquipmentPartOverride& Override : Appearance->PartOverrides)
        {
            const FFunnyPartPoolTable* Part = Tables->FindFunnyPartRow(Override.PartRowId);
            if (!Part || Part->PartSlot != Override.PartSlot || Part->Mesh.IsNull() || !FPackageName::DoesPackageExist(Part->Mesh.ToSoftObjectPath().GetLongPackageName()))
            { OutError = Pair.Key.ToString() + TEXT(": missing appearance mesh package"); return false; }
        }
    }
    for (int32 Index = 0; Index < 6; ++Index)
        if (SlotCounts[Index] == 0 || GradeCounts[Index] == 0 || !Tables->FindEmployeeEquipmentSlot(EEmployeeEquipmentSlot(Index)))
        { OutError = TEXT("Equipment catalog has an empty slot or grade"); return false; }
    if (Tables->GetEmployeeEquipmentBanners().Num() != 3) { OutError = TEXT("Expected one equipment banner per ticket tier"); return false; }
    TSet<FName> PityGroups;
    TSet<EItemType> TicketTypes;
    for (const auto& Pair : Tables->GetEmployeeEquipmentBanners())
    {
        const FEmployeeEquipmentBannerRow& Banner = Pair.Value;
        TArray<FName> GradePools[6];
        if (Banner.BannerId != Pair.Key || Banner.DisplayName.IsEmpty() || !BuildGradePools(*Tables, Banner, GradePools))
        { OutError = Pair.Key.ToString() + TEXT(": invalid ticket, weights, guarantee or pool"); return false; }
        TSet<FName> BannerDefinitions;
        for (FName DefinitionId : Banner.DefinitionIds) BannerDefinitions.Add(DefinitionId);
        for (const auto& DefinitionPair : Tables->GetEmployeeEquipmentDefinitions())
        {
            if (EmployeeEquipmentRules::IsGradeReachable(Banner, DefinitionPair.Value.Grade) && !BannerDefinitions.Contains(DefinitionPair.Key))
            { OutError = Pair.Key.ToString() + TEXT(": eligible definition missing from banner: ") + DefinitionPair.Key.ToString(); return false; }
        }
        if (TicketTypes.Contains(Banner.TicketType) || PityGroups.Contains(Banner.PityGroupId))
        { OutError = Pair.Key.ToString() + TEXT(": duplicate ticket tier or pity group"); return false; }
        TicketTypes.Add(Banner.TicketType);
        PityGroups.Add(Banner.PityGroupId);
    }
    OutError.Reset(); return true;
}

bool UEmployeeEquipmentManagerSubsystem::QuotePull(FName BannerId, int32 Count, FEmployeeEquipmentPullQuote& OutQuote) const
{
    OutQuote = {};
    OutQuote.BannerId = BannerId;
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentBannerRow* Banner = Tables ? Tables->FindEmployeeEquipmentBanner(BannerId) : nullptr;
    const UItemInventoryManager* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
    if (!Banner || !IsBannerActive(*Banner, FDateTime::UtcNow()))
    { OutQuote.ErrorCode = TEXT("BannerUnavailable"); return false; }
    if (!Items) { OutQuote.ErrorCode = TEXT("StateNotReady"); return false; }
    TArray<FName> GradePools[6];
    if (!BuildGradePools(*Tables, *Banner, GradePools)) { OutQuote.ErrorCode = TEXT("CatalogInvalid"); return false; }
    if (!EmployeeEquipmentRules::QuoteTicketPull(BannerId, Banner->TicketType, Count, Items->GetItemCount(Banner->TicketType), OutQuote)) return false;
    const FEmployeeEquipmentEconomyRow* Economy = Tables->GetEmployeeEquipmentEconomy();
    if (!Economy) { OutQuote.bCanPull = false; OutQuote.ErrorCode = TEXT("CatalogInvalid"); return false; }
    if (Count > Economy->InventoryCapacity - State.Instances.Num())
    { OutQuote.bCanPull = false; OutQuote.ErrorCode = TEXT("InventoryFull"); }
    return true;
}

TArray<int32> UEmployeeEquipmentManagerSubsystem::GetNextGradeWeights(FName BannerId) const
{
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentBannerRow* Banner = Tables ? Tables->FindEmployeeEquipmentBanner(BannerId) : nullptr;
    return Banner ? EmployeeEquipmentRules::NextGradeWeights(*Banner, State.BannerStates.FindRef(Banner->PityGroupId)) : TArray<int32>();
}

TMap<FName, float> UEmployeeEquipmentManagerSubsystem::GetNextDefinitionProbabilities(FName BannerId) const
{
    TMap<FName, float> Probabilities;
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentBannerRow* Banner = Tables ? Tables->FindEmployeeEquipmentBanner(BannerId) : nullptr;
    const TArray<int32> Weights = GetNextGradeWeights(BannerId);
    if (!Banner || Weights.Num() != 6) return Probabilities;
    TArray<FName> GradePools[6];
    if (!BuildGradePools(*Tables, *Banner, GradePools)) return Probabilities;
    int32 Counts[6] = {};
    for (int32 Index = 0; Index < 6; ++Index) Counts[Index] = GradePools[Index].Num();
    for (FName Id : Banner->DefinitionIds)
    {
        const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Id);
        if (!Definition || Counts[uint8(Definition->Grade)] <= 0) return {};
        const uint8 GradeIndex = uint8(Definition->Grade);
        const float Conditional = 1.0f / Counts[GradeIndex];
        Probabilities.Add(Id, Weights[GradeIndex] / 10000.0f * Conditional);
    }
    return Probabilities;
}

bool UEmployeeEquipmentManagerSubsystem::QuoteEnhance(const FGuid& InstanceId, EEquipmentMaterialMode Mode, int32& OutCoreCost, int32& OutDuplicateCount) const
{
    OutCoreCost = 0; OutDuplicateCount = 0;
    const FEmployeeEquipmentInstance* Instance = FindInstance(InstanceId);
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentDefinitionRow* Definition = Instance && Tables ? Tables->FindEmployeeEquipmentDefinition(Instance->DefinitionId) : nullptr;
    const FEmployeeEquipmentGradeRow* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
    const FEmployeeEquipmentEconomyRow* Economy = Tables ? Tables->GetEmployeeEquipmentEconomy() : nullptr;
    if (!Instance || !Grade || !Economy || Instance->EnhanceLevel >= Economy->MaxEnhanceLevel) return false;
    if (Mode == EEquipmentMaterialMode::Core) OutCoreCost = Grade->EnhanceCoreCost;
    else if (Mode == EEquipmentMaterialMode::Duplicate) OutDuplicateCount = 1;
    else return false;
    return true;
}

bool UEmployeeEquipmentManagerSubsystem::QuoteDismantle(const TArray<FGuid>& InstanceIds, int32& OutCores) const
{
    OutCores = 0;
    if (InstanceIds.IsEmpty()) return false;
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!Tables) return false;
    TSet<FGuid> Seen;
    int64 Total = 0;
    for (const FGuid& Id : InstanceIds)
    {
        const FEmployeeEquipmentInstance* Instance = FindInstance(Id);
        if (!Instance || Seen.Contains(Id) || Instance->bLocked || GetInstanceEmployee(Id) != INDEX_NONE || (InstanceIds.Num() > 1 && Instance->EnhanceLevel > 0)) return false;
        Seen.Add(Id);
        const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Instance->DefinitionId);
        const FEmployeeEquipmentGradeRow* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
        if (!Grade || Grade->DismantleCores <= 0) return false;
        Total += Grade->DismantleCores;
    }
    if (Total > MAX_int32) return false;
    OutCores = int32(Total); return true;
}

bool UEmployeeEquipmentManagerSubsystem::EmployeeExists(int32 EmployeeId, const FGameSaveData& Snapshot) const
{
    if (EmployeeId <= 0) return false;
    if (UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>())
        if (Employees->FindEmployee(EmployeeId)) return true;
    for (const auto& Office : Snapshot.OfficeDataMap)
        for (const FEmployeeInstance& Employee : Office.Value.EmployeeList)
            if (Employee.EmployeeID == EmployeeId) return true;
    return false;
}

void UEmployeeEquipmentManagerSubsystem::RemoveEmployeeFromCandidate(int32 EmployeeId, FGameSaveData& Candidate) const
{
    Candidate.EmployeeEquipmentState.Loadouts.Remove(EmployeeId);
    Candidate.EmployeeEquipmentState.AppearanceSelections.Remove(EmployeeId);
    ++Candidate.EmployeeEquipmentState.Revision;
}

bool UEmployeeEquipmentManagerSubsystem::GrantDefinition(FName DefinitionId, const FGuid& OperationId, FEmployeeEquipmentState& Candidate, FEmployeeEquipmentResult& OutResult) const
{
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(DefinitionId);
    const FEmployeeEquipmentEconomyRow* Economy = Tables->GetEmployeeEquipmentEconomy();
    if (!Definition || !Economy || Candidate.Instances.Num() >= Economy->InventoryCapacity) return Fail(OutResult, TEXT("InventoryFullOrInvalidReward"));
    FEmployeeEquipmentInstance Instance;
    Instance.InstanceId = FGuid::NewGuid();
    Instance.DefinitionId = DefinitionId;
    Instance.AcquisitionOperationId = OperationId;
    Instance.bLocked = !Candidate.UnlockedDefinitionIds.Contains(DefinitionId) && Definition->Grade >= Economy->AutoLockFirstGrade;
    Candidate.Instances.Add(Instance.InstanceId, Instance);
    Candidate.UnlockedDefinitionIds.Add(DefinitionId);
    if (!Candidate.UnlockedAppearanceIds.Contains(Definition->AppearanceId))
    {
        Candidate.UnlockedAppearanceIds.Add(Definition->AppearanceId);
        OutResult.NewAppearanceIds.Add(Definition->AppearanceId);
    }
    OutResult.GrantedInstances.Add(Instance);
    return true;
}

FEmployeeEquipmentResult UEmployeeEquipmentManagerSubsystem::Execute(const FEmployeeEquipmentRequest& Request)
{
    FEmployeeEquipmentResult Result; Result.OperationId = Request.OperationId; Result.Revision = State.Revision; Result.CatalogVersion = Request.CatalogVersion;
    const FString Fingerprint = EmployeeEquipmentRules::FingerprintRequest(Request);
    if (const FEmployeeEquipmentOperationReceipt* Existing = State.CompletedOperations.Find(Request.OperationId))
    {
        if (Existing->RequestHash == Fingerprint) { const FEmployeeEquipmentResult CachedResult = Existing->Result; OnOperationCompleted.Broadcast(CachedResult); return CachedResult; }
        Result.ErrorCode = TEXT("IdempotencyConflict"); OnOperationFailed.Broadcast(Result); return Result;
    }
    if (bExecuting) Result.ErrorCode = TEXT("Busy");
    else if (!Request.OperationId.IsValid()) Result.ErrorCode = TEXT("InvalidOperationId");
    else if (Request.ExpectedRevision != State.Revision) Result.ErrorCode = TEXT("RevisionMismatch");
    else
    {
        TGuardValue<bool> Guard(bExecuting, true);
        USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
        FString CatalogError;
        const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        if (!SaveMgr || !SaveMgr->CanCommitEquipmentTransaction()) Result.ErrorCode = TEXT("StateNotReady");
        else if (!IsCatalogValid(CatalogError))
        {
            Result.ErrorCode = TEXT("CatalogInvalid");
            UE_LOG(LogTemp, Error, TEXT("[EmployeeEquipment] %s"), *CatalogError);
        }
        else if (Request.CatalogVersion != Tables->GetEmployeeEquipmentEconomy()->CatalogVersion) Result.ErrorCode = TEXT("CatalogChanged");
        else
        {
            FGameSaveData Candidate;
            if (!SaveMgr->BuildCurrentSnapshot(Candidate)) Result.ErrorCode = TEXT("StateNotReady");
            else if (BuildCandidate(Request, Candidate, Result))
            {
                Result.Revision = State.Revision + 1;
                Result.bCommitted = true;
                Candidate.EmployeeEquipmentState.Revision = Result.Revision;
                FEmployeeEquipmentOperationReceipt Receipt; Receipt.RequestHash = Fingerprint; Receipt.Result = Result;
                Candidate.EmployeeEquipmentState.CompletedOperations.Add(Request.OperationId, Receipt);
                if (!Result.GrantedInstances.IsEmpty()) Candidate.EmployeeEquipmentState.UnseenResultIds.Add(Request.OperationId);
                if (!SaveMgr->CommitEquipmentSnapshot(Candidate))
                {
                    Result = FEmployeeEquipmentResult(); Result.OperationId = Request.OperationId; Result.ErrorCode = TEXT("SaveFailed"); Result.Revision = State.Revision;
                }
            }
        }
    }
    if (Result.bCommitted) { OnEquipmentChanged.Broadcast(); OnOperationCompleted.Broadcast(Result); }
    else
    {
        const FName Failure = Result.ErrorCode;
        Result = FEmployeeEquipmentResult(); Result.OperationId = Request.OperationId; Result.ErrorCode = Failure; Result.Revision = State.Revision; Result.CatalogVersion = Request.CatalogVersion;
        UE_LOG(LogTemp, Warning, TEXT("[EmployeeEquipment] Rejected %s: %s"), *Request.OperationId.ToString(), *Result.ErrorCode.ToString()); OnOperationFailed.Broadcast(Result);
    }
    return Result;
}

bool UEmployeeEquipmentManagerSubsystem::BuildCandidate(const FEmployeeEquipmentRequest& Request, FGameSaveData& Candidate, FEmployeeEquipmentResult& OutResult)
{
    const UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const FEmployeeEquipmentEconomyRow& Economy = *Tables->GetEmployeeEquipmentEconomy();
    FEmployeeEquipmentState& Equipment = Candidate.EmployeeEquipmentState;
    if (Request.Command == EEquipmentCommand::Pull)
    {
        const FEmployeeEquipmentBannerRow* Banner = Tables->FindEmployeeEquipmentBanner(Request.BannerId);
        if (!Banner || !IsBannerActive(*Banner, FDateTime::UtcNow())) return Fail(OutResult, TEXT("BannerUnavailable"));
        FEmployeeEquipmentPullQuote Quote;
        if (!EmployeeEquipmentRules::QuoteTicketPull(Request.BannerId, Banner->TicketType, Request.Count, Candidate.ItemInventory.FindRef(Banner->TicketType), Quote)) return Fail(OutResult, Quote.ErrorCode);
        if (!Quote.bCanPull) return Fail(OutResult, Quote.ErrorCode);
        if (Request.Count > Economy.InventoryCapacity - Equipment.Instances.Num()) return Fail(OutResult, TEXT("InventoryFull"));
        TArray<FName> GradePools[6];
        if (!BuildGradePools(*Tables, *Banner, GradePools)) return Fail(OutResult, TEXT("InvalidBannerPool"));
        OutResult.BannerId = Banner->BannerId;
        OutResult.TicketType = Banner->TicketType;
        OutResult.SpentTickets = Quote.RequiredTickets;
        Candidate.ItemInventory.FindOrAdd(Banner->TicketType) -= OutResult.SpentTickets;
        FEmployeeEquipmentPityState& Pity = Equipment.BannerStates.FindOrAdd(Banner->PityGroupId);
        for (int32 DrawIndex = 0; DrawIndex < Request.Count; ++DrawIndex)
        {
            int32 Roll = FMath::RandRange(0, 9999);
            EEmployeeEquipmentGrade Natural = EEmployeeEquipmentGrade::Ascendant;
            for (uint8 GradeIndex = 0; GradeIndex < 6; ++GradeIndex)
            {
                Roll -= Banner->GradeWeights[GradeIndex];
                if (Roll < 0) { Natural = EEmployeeEquipmentGrade(GradeIndex); break; }
            }
            const EEmployeeEquipmentGrade FinalGrade = EmployeeEquipmentRules::AdvancePity(Pity, Natural, Banner->EpicPity, Banner->LegendaryPity, Banner->AscendantPity);
            const TArray<FName>& Pool = GradePools[uint8(FinalGrade)];
            const FName Pick = Pool[FMath::RandRange(0, Pool.Num() - 1)];
            if (!GrantDefinition(Pick, Request.OperationId, Equipment, OutResult)) return false;
        }
        return true;
    }
    if (Request.Command == EEquipmentCommand::RecruitGift)
    {
        if (!EmployeeExists(Request.EmployeeId, Candidate)) return Fail(OutResult, TEXT("InvalidEmployee"));
        // 신입 1회 한정. 재시도나 재채용으로 두 벌째가 끼워지는 걸 막는다.
        if (const FEmployeeEquipmentLoadout* Worn = Equipment.Loadouts.Find(Request.EmployeeId))
            if (!Worn->Slots.IsEmpty()) return Fail(OutResult, TEXT("AlreadyOutfitted"));
        const TArray<FName> Pool = EmployeeEquipmentRules::BuildRecruitGiftPool(Tables->GetEmployeeEquipmentDefinitions(), Economy.RecruitGiftGrade);
        if (Pool.IsEmpty()) return Fail(OutResult, TEXT("NoRecruitGiftPool"));
        const FName Pick = Pool[FMath::RandRange(0, Pool.Num() - 1)];
        if (!GrantDefinition(Pick, Request.OperationId, Equipment, OutResult)) return false;
        const FEmployeeEquipmentDefinitionRow* Gift = Tables->FindEmployeeEquipmentDefinition(Pick);
        if (!Gift || Gift->EquipmentSlot >= EEmployeeEquipmentSlot::Count) return Fail(OutResult, TEXT("InvalidRecruitGift"));
        Equipment.Loadouts.FindOrAdd(Request.EmployeeId).Slots.Add(Gift->EquipmentSlot, OutResult.GrantedInstances.Last().InstanceId);
        return true;
    }
    if (Request.Command == EEquipmentCommand::ClaimStarter)
    {
        if (Equipment.bStarterClaimed || !Candidate.bTutorialCompleted) return Fail(OutResult, TEXT("StarterNotEligible"));
        if (Economy.StarterTickets <= 0) return Fail(OutResult, TEXT("InvalidStarter"));
        if (!AddItems(Candidate, EItemType::EmployeeEquipmentTicketNormal, Economy.StarterTickets)) return Fail(OutResult, TEXT("BalanceOverflow"));
        Equipment.bStarterClaimed = true; return true;
    }
    if (Request.Command == EEquipmentCommand::SetSlot || Request.Command == EEquipmentCommand::SetAppearance)
    {
        if (!EmployeeExists(Request.EmployeeId, Candidate) || Request.EquipmentSlot >= EEmployeeEquipmentSlot::Count) return Fail(OutResult, TEXT("InvalidEmployeeOrSlot"));
        if (Request.Command == EEquipmentCommand::SetAppearance)
        {
            const FEmployeeEquipmentSlotRow* SlotDefinition = Tables->FindEmployeeEquipmentSlot(Request.EquipmentSlot);
            const FEmployeeEquipmentAppearanceRow* Appearance = Tables->FindEmployeeEquipmentAppearance(Request.AppearanceId);
            if (Request.AppearanceMode == EEquipmentAppearanceMode::Override)
            {
                if (!Appearance || Appearance->EquipmentSlot != Request.EquipmentSlot || !Equipment.UnlockedAppearanceIds.Contains(Request.AppearanceId)) return Fail(OutResult, TEXT("AppearanceNotUnlocked"));
            }
            else if (Request.AppearanceMode != EEquipmentAppearanceMode::UseEquipment && Request.AppearanceMode != EEquipmentAppearanceMode::Hidden) return Fail(OutResult, TEXT("InvalidAppearanceMode"));
            else if (!Request.AppearanceId.IsNone() || (Request.AppearanceMode == EEquipmentAppearanceMode::Hidden && (!SlotDefinition || !SlotDefinition->bCanHide))) return Fail(OutResult, TEXT("AppearanceModeNotAllowed"));
            FEquipmentAppearanceSelection Selection; Selection.Mode = Request.AppearanceMode; Selection.AppearanceId = Request.AppearanceId;
            Equipment.AppearanceSelections.FindOrAdd(Request.EmployeeId).Slots.Add(Request.EquipmentSlot, Selection);
            return true;
        }
        if (!Request.InstanceId.IsValid()) { Equipment.Loadouts.FindOrAdd(Request.EmployeeId).Slots.Remove(Request.EquipmentSlot); return true; }
        const FEmployeeEquipmentInstance* Instance = Equipment.Instances.Find(Request.InstanceId);
        const FEmployeeEquipmentDefinitionRow* Definition = Instance ? Tables->FindEmployeeEquipmentDefinition(Instance->DefinitionId) : nullptr;
        if (!Definition || Definition->EquipmentSlot != Request.EquipmentSlot) return Fail(OutResult, TEXT("WrongEquipmentSlot"));
        const int32 SourceEmployee = FindOwner(Equipment, Request.InstanceId);
        if (SourceEmployee != Request.ExpectedSourceEmployeeId) return Fail(OutResult, TEXT("EquipmentOwnerChanged"));
        if (SourceEmployee != INDEX_NONE) Equipment.Loadouts.FindOrAdd(SourceEmployee).Slots.Remove(Request.EquipmentSlot);
        Equipment.Loadouts.FindOrAdd(Request.EmployeeId).Slots.Add(Request.EquipmentSlot, Request.InstanceId);
        return true;
    }
    if (Request.Command == EEquipmentCommand::Enhance)
    {
        FEmployeeEquipmentInstance* Target = Equipment.Instances.Find(Request.InstanceId);
        const FEmployeeEquipmentDefinitionRow* Definition = Target ? Tables->FindEmployeeEquipmentDefinition(Target->DefinitionId) : nullptr;
        const FEmployeeEquipmentGradeRow* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
        if (!Target || !Grade || Target->EnhanceLevel != Request.ExpectedLevel || Target->EnhanceLevel < 0 || Target->EnhanceLevel >= Economy.MaxEnhanceLevel) return Fail(OutResult, TEXT("InvalidEnhanceTarget"));
        if (Request.MaterialMode == EEquipmentMaterialMode::Core)
        {
            if (!Request.InstanceIds.IsEmpty()) return Fail(OutResult, TEXT("UnexpectedMaterial"));
            int32& Cores = Candidate.ItemInventory.FindOrAdd(EItemType::EmployeeEquipmentCore);
            if (Cores < Grade->EnhanceCoreCost) return Fail(OutResult, TEXT("InsufficientCores"));
            Cores -= Grade->EnhanceCoreCost; OutResult.CoreDelta = -Grade->EnhanceCoreCost;
        }
        else if (Request.MaterialMode == EEquipmentMaterialMode::Duplicate)
        {
            if (Request.InstanceIds.Num() != 1 || Request.InstanceIds[0] == Request.InstanceId) return Fail(OutResult, TEXT("InvalidEnhanceMaterial"));
            const FEmployeeEquipmentInstance* Material = Equipment.Instances.Find(Request.InstanceIds[0]);
            if (!Material || Material->DefinitionId != Target->DefinitionId || Material->EnhanceLevel != 0 || Material->bLocked || FindOwner(Equipment, Material->InstanceId) != INDEX_NONE) return Fail(OutResult, TEXT("ProtectedOrWrongMaterial"));
            Equipment.Instances.Remove(Request.InstanceIds[0]);
            Target = Equipment.Instances.Find(Request.InstanceId);
        }
        else return Fail(OutResult, TEXT("InvalidMaterialMode"));
        ++Target->EnhanceLevel;
        return true;
    }
    if (Request.Command == EEquipmentCommand::Dismantle || Request.Command == EEquipmentCommand::SetLock)
    {
        if (Request.InstanceIds.IsEmpty()) return Fail(OutResult, TEXT("NoEquipmentSelected"));
        TSet<FGuid> Seen;
        int64 CoreGrant = 0;
        for (const FGuid& Id : Request.InstanceIds)
        {
            FEmployeeEquipmentInstance* Instance = Equipment.Instances.Find(Id);
            if (!Instance || Seen.Contains(Id)) return Fail(OutResult, TEXT("DuplicateOrMissingInstance"));
            Seen.Add(Id);
            if (Request.Command == EEquipmentCommand::SetLock) { Instance->bLocked = Request.bLocked; continue; }
            if (Instance->bLocked || FindOwner(Equipment, Id) != INDEX_NONE) return Fail(OutResult, TEXT("ProtectedEquipment"));
            if (Instance->EnhanceLevel > 0 && (Request.InstanceIds.Num() != 1 || !Request.bConfirmEnhancedDismantle)) return Fail(OutResult, TEXT("EnhancedDismantleNeedsConfirmation"));
            const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Instance->DefinitionId);
            const FEmployeeEquipmentGradeRow* Grade = Definition ? Tables->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
            if (!Grade) return Fail(OutResult, TEXT("InvalidEquipmentDefinition"));
            CoreGrant += Grade->DismantleCores;
        }
        if (Request.Command == EEquipmentCommand::Dismantle)
        {
            if (CoreGrant > MAX_int32 || !AddItems(Candidate, EItemType::EmployeeEquipmentCore, int32(CoreGrant))) return Fail(OutResult, TEXT("BalanceOverflow"));
            for (const FGuid& Id : Request.InstanceIds) Equipment.Instances.Remove(Id);
            OutResult.CoreDelta = int32(CoreGrant);
        }
        return true;
    }
    return Fail(OutResult, TEXT("UnsupportedCommand"));
}
