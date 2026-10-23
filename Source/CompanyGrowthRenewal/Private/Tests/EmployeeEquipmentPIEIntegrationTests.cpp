#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Core/CGGameInstance.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/EntityManager.h"
#include "Player/MainMapPlayerController.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ShopManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"

namespace
{
    bool bLastIntegrationRestoreAttempted = false;
    bool bLastIntegrationRestoreSucceeded = false;
    // 같은 게임 스레드 호출 안에서 끝나므로 테스트 중 월드 타이머가 진행되지 않는다.
    // 원본 디스크 바이트와 아직 저장되지 않은 메모리 스냅샷을 별도로 보존한다.
    class FEquipmentPIERestoreScope
    {
    public:
        FEquipmentPIERestoreScope(FAutomationTestBase& InTest, USaveLoadManager& InSave,
            UEmployeeEquipmentManagerSubsystem& InEquipment, UResourceItemManager& InResources,
            UItemInventoryManager& InItems, UShopManagerSubsystem& InShop)
            : Test(InTest), Save(InSave), Equipment(InEquipment), Resources(InResources), Items(InItems), Shop(InShop)
        {
        }

        bool Capture()
        {
            USaveGame_GameData* Cached = Save.GetCurrentSaveData();
            if (!Cached || Cached->SaveSlotName != TEXT("GameSlot") || Cached->UserIndex != 0 ||
                !Save.BuildCurrentSnapshot(OriginalLive)) return false;
            OriginalCache = Cached->GameData;
            bOriginalSuppressed = Save.IsLeaderboardUploadSuppressed();
            bHadDiskSave = UGameplayStatics::DoesSaveGameExist(SlotName, 0);
            if (bHadDiskSave && !UGameplayStatics::LoadDataFromSlot(OriginalDisk, SlotName, 0)) return false;

            const FString BackupDirectory = FPaths::ProjectSavedDir() / TEXT("EmployeeEquipment/IntegrationBackups") /
                FGuid::NewGuid().ToString(EGuidFormats::Digits);
            if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true)) return false;
            USaveGame_GameData* LiveBackup = NewObject<USaveGame_GameData>();
            LiveBackup->GameData = OriginalLive;
            TArray<uint8> LiveBytes;
            if (!UGameplayStatics::SaveGameToMemory(LiveBackup, LiveBytes) ||
                !FFileHelper::SaveArrayToFile(LiveBytes, *(BackupDirectory / TEXT("LiveSnapshot.sav"))) ||
                (bHadDiskSave && !FFileHelper::SaveArrayToFile(OriginalDisk, *(BackupDirectory / TEXT("OriginalGameSlot.sav"))))) return false;
            Test.AddInfo(FString::Printf(TEXT("Recovery backup: %s (disk existed: %d)"), *BackupDirectory, bHadDiskSave));
            Save.SetLeaderboardUploadSuppressed(true);
            bCaptured = true;
            return true;
        }

        ~FEquipmentPIERestoreScope() { Restore(); }

        bool Restore()
        {
            if (!bCaptured || bRestored) return true;
            bRestored = true;
            bLastIntegrationRestoreAttempted = true;
            const bool bCommitted = Save.CommitEquipmentSnapshot(OriginalLive);
            // 저장 실패 시에도 테스트 재화가 다음 자동 저장에 흘러가지 않게 메모리를 원복한다.
            Resources.SetAllResources(OriginalLive.ResourceBank, false);
            Items.SetAllItems(OriginalLive.ItemInventory, false);
            Shop.SetShopData(OriginalLive.ShopSaveData);
            Equipment.RestoreState(OriginalLive.EmployeeEquipmentState);
            if (USaveGame_GameData* Cached = Save.GetCurrentSaveData()) Cached->GameData = OriginalCache;
            Equipment.OnEquipmentChanged.Broadcast();

            const bool bDiskRestored = bHadDiskSave
                ? UGameplayStatics::SaveDataToSlot(OriginalDisk, SlotName, 0)
                : (!UGameplayStatics::DoesSaveGameExist(SlotName, 0) || UGameplayStatics::DeleteGameInSlot(SlotName, 0));
            TArray<uint8> Readback;
            const bool bExactDisk = bHadDiskSave
                ? UGameplayStatics::LoadDataFromSlot(Readback, SlotName, 0) && Readback == OriginalDisk
                : !UGameplayStatics::DoesSaveGameExist(SlotName, 0);
            Save.SetLeaderboardUploadSuppressed(bOriginalSuppressed);
            Test.TestTrue(TEXT("Original economic snapshot restored through commit API"), bCommitted);
            Test.TestTrue(TEXT("Original slot restored byte for byte"), bDiskRestored && bExactDisk);
            Test.TestTrue(TEXT("Original equipment state restored"), FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(
                &Equipment.GetState(), &OriginalLive.EmployeeEquipmentState, 0));
            Test.TestEqual(TEXT("Original leaderboard suppression restored"), Save.IsLeaderboardUploadSuppressed(), bOriginalSuppressed);
            bLastIntegrationRestoreSucceeded = bCommitted && bDiskRestored && bExactDisk;
            Test.AddInfo(FString::Printf(TEXT("Restore completed: live commit=%d, original disk write=%d, byte-identical=%d, leaderboard suppression=%d"),
                bCommitted, bDiskRestored, bExactDisk, Save.IsLeaderboardUploadSuppressed() == bOriginalSuppressed));
            return bLastIntegrationRestoreSucceeded;
        }

        const FGameSaveData& GetOriginalLive() const { return OriginalLive; }

    private:
        FAutomationTestBase& Test;
        USaveLoadManager& Save;
        UEmployeeEquipmentManagerSubsystem& Equipment;
        UResourceItemManager& Resources;
        UItemInventoryManager& Items;
        UShopManagerSubsystem& Shop;
        FGameSaveData OriginalLive;
        FGameSaveData OriginalCache;
        TArray<uint8> OriginalDisk;
        const FString SlotName = TEXT("GameSlot");
        bool bOriginalSuppressed = false;
        bool bHadDiskSave = false;
        bool bCaptured = false;
        bool bRestored = false;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeEquipmentPIEIntegrationTest,
    "CGR.Tools.EmployeeEquipmentPIEIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEmployeeEquipmentPIEIntegrationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    bLastIntegrationRestoreAttempted = false;
    bLastIntegrationRestoreSucceeded = false;
    UWorld* PIEWorld = nullptr;
    int32 PIEWorldCount = 0;
    if (GEngine)
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE && Context.World()) { PIEWorld = Context.World(); ++PIEWorldCount; }
    if (!TestTrue(TEXT("Exactly one running PIE world is required"), PIEWorldCount == 1 && PIEWorld && PIEWorld->GetGameInstance())) return false;
    UGameInstance* GameInstance = PIEWorld->GetGameInstance();
    USaveLoadManager* Save = GameInstance->GetSubsystem<USaveLoadManager>();
    UEmployeeEquipmentManagerSubsystem* Equipment = GameInstance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
    UTableManagerSubsystem* Tables = GameInstance->GetSubsystem<UTableManagerSubsystem>();
    UEmployeeManager* Employees = GameInstance->GetSubsystem<UEmployeeManager>();
    UResourceItemManager* Resources = GameInstance->GetSubsystem<UResourceItemManager>();
    UItemInventoryManager* Items = GameInstance->GetSubsystem<UItemInventoryManager>();
    UShopManagerSubsystem* Shop = GameInstance->GetSubsystem<UShopManagerSubsystem>();
    if (!TestTrue(TEXT("Actual PIE subsystems are available"), Save && Equipment && Tables && Employees && Resources && Items && Shop)) return false;
    TestEqual(TEXT("Exactly three equipment ticket banners"), Tables->GetEmployeeEquipmentBanners().Num(), 3);
    for (const auto& BannerPair : Tables->GetEmployeeEquipmentBanners())
    {
        const TMap<FName, float> Probabilities = Equipment->GetNextDefinitionProbabilities(BannerPair.Key);
        TMap<EEmployeeEquipmentGrade, float> GradeProbability;
        float TotalProbability = 0.0f;
        TestEqual(TEXT("Every eligible item has an explicit probability"), Probabilities.Num(), BannerPair.Value.DefinitionIds.Num());
        for (const auto& ProbabilityPair : Probabilities)
        {
            const auto* Definition = Tables->FindEmployeeEquipmentDefinition(ProbabilityPair.Key);
            if (!TestNotNull(TEXT("Probability refers to a catalog item"), Definition)) return false;
            if (const float* Previous = GradeProbability.Find(Definition->Grade))
                TestTrue(TEXT("Items of the same grade have equal probability"), FMath::IsNearlyEqual(*Previous, ProbabilityPair.Value));
            else GradeProbability.Add(Definition->Grade, ProbabilityPair.Value);
            TotalProbability += ProbabilityPair.Value;
        }
        TestTrue(TEXT("Item probabilities sum to one"), FMath::IsNearlyEqual(TotalProbability, 1.0f, 0.0001f));
    }
    FString CatalogError;
    if (!TestTrue(*FString::Printf(TEXT("Saved catalog and icon packages are valid: %s"), *CatalogError), Equipment->IsCatalogValid(CatalogError)))
    { AddError(CatalogError); return false; }
    if (!TestTrue(TEXT("Initial load and office restoration have finished"), Save->CanCommitEquipmentTransaction())) return false;
    if (const UOfficeStageProgressManager* Stage = PIEWorld->GetSubsystem<UOfficeStageProgressManager>())
        if (!TestFalse(TEXT("Run outside active development/launch to preserve the attempt"), Stage->IsFocusLocked())) return false;
    if (!TestFalse(TEXT("Wait for any existing portrait queue before the test"), Employees->IsPortraitCapturing())) return false;
    const USaveGame_GameData* CurrentSave = Save->GetCurrentSaveData();
    if (!TestTrue(TEXT("Existing save must have completed the tutorial"), CurrentSave && CurrentSave->GameData.bTutorialCompleted)) return false;

    TArray<FEmployeeInstance> ExistingEmployees = Employees->GetAllEmployees();
    ExistingEmployees.RemoveAll([](const FEmployeeInstance& Employee) { return Employee.EmployeeID <= 0; });
    if (!TestTrue(TEXT("At least two existing employees are required; this test never creates/fires employees"), ExistingEmployees.Num() >= 2)) return false;
    const int32 FirstEmployeeId = ExistingEmployees[0].EmployeeID;
    const int32 SecondEmployeeId = ExistingEmployees[1].EmployeeID;
    if (!TestNotEqual(TEXT("Employee IDs are distinct"), FirstEmployeeId, SecondEmployeeId)) return false;
    const FEmployeeEquipmentEconomyRow* Economy = Tables->GetEmployeeEquipmentEconomy();
    const FEmployeeEquipmentBannerRow* Banner = Tables->FindEmployeeEquipmentBanner(TEXT("EquipmentNormal"));
    if (!TestTrue(TEXT("Production starter and normal banner exist"), Economy && Banner)) return false;

    FEquipmentPIERestoreScope RestoreScope(*this, *Save, *Equipment, *Resources, *Items, *Shop);
    if (!TestTrue(TEXT("Original live snapshot and disk bytes backed up before mutation"), RestoreScope.Capture())) return false;
    FGameSaveData Fixture = RestoreScope.GetOriginalLive();
    Fixture.EmployeeEquipmentState = FEmployeeEquipmentState();
    Fixture.ResourceBank.FindOrAdd(EResourceType::Diamond) = int64(100000);
    Fixture.ItemInventory.FindOrAdd(EItemType::EmployeeEquipmentTicketNormal) = 0;
    Fixture.ItemInventory.FindOrAdd(EItemType::EmployeeEquipmentTicketAdvanced) = 0;
    Fixture.ItemInventory.FindOrAdd(EItemType::EmployeeEquipmentTicketPremium) = 0;
    Fixture.ShopSaveData.EquipmentTicketPurchases.Reset();
    Fixture.ShopSaveData.EquipmentTicketResetPeriods.Reset();
    Fixture.ShopSaveData.PurchaseCounts.Remove(TEXT("Weekly_EquipmentNormal_14"));
    Fixture.ItemInventory.FindOrAdd(EItemType::EmployeeEquipmentCore) = 10000;
    if (!TestTrue(TEXT("Fixture uses the real save/manager commit path"), Save->CommitEquipmentSnapshot(Fixture))) return false;

    auto VerifyDisk = [&](const TCHAR* Step)
    {
        const USaveGame_GameData* Loaded = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromSlot(TEXT("GameSlot"), 0));
        const USaveGame_GameData* Cached = Save->GetCurrentSaveData();
        const bool bMatches = Loaded && Cached && FGameSaveData::StaticStruct()->CompareScriptStruct(&Loaded->GameData, &Cached->GameData, 0);
        TestTrue(*FString::Printf(TEXT("%s: full disk SaveGame equals committed cache"), Step), bMatches);
        if (Loaded)
        {
            TestTrue(*FString::Printf(TEXT("%s: disk equipment equals live manager"), Step),
                FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Loaded->GameData.EmployeeEquipmentState, &Equipment->GetState(), 0));
            TestEqual(*FString::Printf(TEXT("%s: disk diamonds equal live balance"), Step),
                Loaded->GameData.ResourceBank.FindRef(EResourceType::Diamond), Resources->GetResourceAmount(EResourceType::Diamond));
            TestEqual(*FString::Printf(TEXT("%s: disk tickets equal live balance"), Step),
                Loaded->GameData.ItemInventory.FindRef(EItemType::EmployeeEquipmentTicketNormal), Items->GetItemCount(EItemType::EmployeeEquipmentTicketNormal));
        }
        return bMatches;
    };
    auto Execute = [&](const TCHAR* Step, const FEmployeeEquipmentRequest& Request, FEmployeeEquipmentResult& OutResult)
    {
        const int64 BeforeRevision = Equipment->GetState().Revision;
        OutResult = Equipment->Execute(Request);
        if (!TestTrue(*FString::Printf(TEXT("%s commits (error: %s)"), Step, *OutResult.ErrorCode.ToString()), OutResult.bCommitted)) return false;
        TestEqual(*FString::Printf(TEXT("%s increments revision exactly once"), Step), Equipment->GetState().Revision, BeforeRevision + 1);
        return VerifyDisk(Step);
    };
    auto Reject = [&](const TCHAR* Step, const FEmployeeEquipmentRequest& Request, FName Error)
    {
        FGameSaveData BeforeCache = Save->GetCurrentSaveData()->GameData;
        FEmployeeEquipmentState BeforeEquipment = Equipment->GetState();
        TArray<uint8> BeforeDisk, AfterDisk;
        if (!UGameplayStatics::LoadDataFromSlot(BeforeDisk, TEXT("GameSlot"), 0)) return false;
        AddExpectedError(Error.ToString(), EAutomationExpectedErrorFlags::Contains, 1);
        const FEmployeeEquipmentResult Rejected = Equipment->Execute(Request);
        TestFalse(*FString::Printf(TEXT("%s rejects"), Step), Rejected.bCommitted);
        TestEqual(*FString::Printf(TEXT("%s returns specific reason"), Step), Rejected.ErrorCode, Error);
        TestTrue(*FString::Printf(TEXT("%s leaves manager unchanged"), Step), FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Equipment->GetState(), &BeforeEquipment, 0));
        TestTrue(*FString::Printf(TEXT("%s leaves full cache unchanged"), Step), FGameSaveData::StaticStruct()->CompareScriptStruct(&Save->GetCurrentSaveData()->GameData, &BeforeCache, 0));
        return TestTrue(*FString::Printf(TEXT("%s never writes disk"), Step), UGameplayStatics::LoadDataFromSlot(AfterDisk, TEXT("GameSlot"), 0) && BeforeDisk == AfterDisk);
    };

    FEmployeeEquipmentResult Result;
    // 받기 버튼이 없다 ― 튜토리얼 완료 알림이 부르는 지급 경로를 그대로 탄다
    const int64 BeforeStarterRevision = Equipment->GetState().Revision;
    if (!TestTrue(TEXT("Starter grant commits"), Equipment->TryGrantStarterTickets())) return false;
    TestEqual(TEXT("Starter grant increments revision exactly once"), Equipment->GetState().Revision, BeforeStarterRevision + 1);
    if (!VerifyDisk(TEXT("Starter"))) return false;
    TestEqual(TEXT("Starter creates no equipment instance"), Equipment->GetState().Instances.Num(), 0);
    TestEqual(TEXT("Starter grants catalog ticket count"), Items->GetItemCount(EItemType::EmployeeEquipmentTicketNormal), Economy->StarterTickets);
    TestFalse(TEXT("Starter grant is skipped once claimed"), Equipment->TryGrantStarterTickets());
    if (!Reject(TEXT("Starter cannot be claimed twice"), Equipment->MakeRequest(EEquipmentCommand::ClaimStarter), TEXT("StarterNotEligible"))) return false;
    FEmployeeEquipmentRequest StarterPull = Equipment->MakeRequest(EEquipmentCommand::Pull);
    StarterPull.BannerId = Banner->BannerId;
    if (!Execute(TEXT("Starter ticket opens one random box"), StarterPull, Result)) return false;
    if (!TestEqual(TEXT("One ticket grants one equipment"), Result.GrantedInstances.Num(), 1)) return false;
    const FEmployeeEquipmentInstance Target = Result.GrantedInstances[0];
    const FEmployeeEquipmentDefinitionRow* Definition = Tables->FindEmployeeEquipmentDefinition(Target.DefinitionId);
    if (!TestNotNull(TEXT("Random definition resolves through actual table manager"), Definition)) return false;

    FEmployeeEquipmentRequest Request = Equipment->MakeRequest(EEquipmentCommand::SetSlot);
    Request.EmployeeId = FirstEmployeeId; Request.EquipmentSlot = Definition->EquipmentSlot; Request.InstanceId = Target.InstanceId;
    if (!Execute(TEXT("Equip"), Request, Result)) return false;
    TestEqual(TEXT("First employee owns equipped instance"), Equipment->GetInstanceEmployee(Target.InstanceId), FirstEmployeeId);
    Request = Equipment->MakeRequest(EEquipmentCommand::SetSlot);
    Request.EmployeeId = SecondEmployeeId; Request.ExpectedSourceEmployeeId = FirstEmployeeId;
    Request.EquipmentSlot = Definition->EquipmentSlot; Request.InstanceId = Target.InstanceId;
    if (!Execute(TEXT("Transfer"), Request, Result)) return false;
    TestNull(TEXT("Transfer clears first employee slot"), Equipment->GetEquippedInstance(FirstEmployeeId, Definition->EquipmentSlot));
    TestEqual(TEXT("Transfer has exactly one owner"), Equipment->GetInstanceEmployee(Target.InstanceId), SecondEmployeeId);

    int32 CoreCost = 0, DuplicateCount = 0;
    if (!TestTrue(TEXT("Actual enhance quote resolves"), Equipment->QuoteEnhance(Target.InstanceId, EEquipmentMaterialMode::Core, CoreCost, DuplicateCount))) return false;
    const int32 CoresBeforeEnhance = Items->GetItemCount(EItemType::EmployeeEquipmentCore);
    Request = Equipment->MakeRequest(EEquipmentCommand::Enhance); Request.InstanceId = Target.InstanceId;
    if (!Execute(TEXT("Enhance"), Request, Result)) return false;
    TestEqual(TEXT("Enhance advances to +1"), Equipment->FindInstance(Target.InstanceId)->EnhanceLevel, 1);
    TestEqual(TEXT("Enhance spends exact quote"), Items->GetItemCount(EItemType::EmployeeEquipmentCore), CoresBeforeEnhance - CoreCost);

    Request = Equipment->MakeRequest(EEquipmentCommand::SetSlot);
    Request.EmployeeId = SecondEmployeeId; Request.EquipmentSlot = Definition->EquipmentSlot;
    if (!Execute(TEXT("Unequip before dismantle"), Request, Result)) return false;
    Request = Equipment->MakeRequest(EEquipmentCommand::SetLock); Request.InstanceIds = {Target.InstanceId}; Request.bLocked = true;
    if (!Execute(TEXT("Lock"), Request, Result)) return false;
    Request = Equipment->MakeRequest(EEquipmentCommand::Dismantle); Request.InstanceIds = {Target.InstanceId}; Request.bConfirmEnhancedDismantle = true;
    if (!Reject(TEXT("Locked dismantle"), Request, TEXT("ProtectedEquipment"))) return false;
    Request = Equipment->MakeRequest(EEquipmentCommand::SetLock); Request.InstanceIds = {Target.InstanceId};
    if (!Execute(TEXT("Unlock"), Request, Result)) return false;
    Request = Equipment->MakeRequest(EEquipmentCommand::Dismantle); Request.InstanceIds = {Target.InstanceId};
    if (!Reject(TEXT("Enhanced dismantle needs explicit confirmation"), Request, TEXT("EnhancedDismantleNeedsConfirmation"))) return false;
    Request = Equipment->MakeRequest(EEquipmentCommand::Dismantle); Request.InstanceIds = {Target.InstanceId}; Request.bConfirmEnhancedDismantle = true;
    if (!Execute(TEXT("Confirmed dismantle"), Request, Result)) return false;
    TestNull(TEXT("Dismantled instance removed"), Equipment->FindInstance(Target.InstanceId));
    TestTrue(TEXT("Dismantle preserves collection unlock"), Equipment->GetState().UnlockedDefinitionIds.Contains(Target.DefinitionId));
    TestTrue(TEXT("Dismantle preserves permanent appearance"), Equipment->GetState().UnlockedAppearanceIds.Contains(Definition->AppearanceId));
    Request = Equipment->MakeRequest(EEquipmentCommand::SetAppearance); Request.EmployeeId = SecondEmployeeId;
    Request.EquipmentSlot = Definition->EquipmentSlot; Request.AppearanceMode = EEquipmentAppearanceMode::Override; Request.AppearanceId = Definition->AppearanceId;
    if (!Execute(TEXT("Apply appearance after dismantle"), Request, Result)) return false;
    TestEqual(TEXT("Selected appearance survives missing equipment"), Equipment->GetAppearanceSelection(SecondEmployeeId, Definition->EquipmentSlot).AppearanceId, Definition->AppearanceId);

    FGameSaveData ShortageFixture;
    if (!Save->BuildCurrentSnapshot(ShortageFixture)) return false;
    ShortageFixture.ItemInventory.FindOrAdd(EItemType::EmployeeEquipmentTicketNormal) = 3;
    if (!TestTrue(TEXT("Three-ticket shortage fixture commits"), Save->CommitEquipmentSnapshot(ShortageFixture))) return false;
    FEmployeeEquipmentPullQuote PullQuote;
    if (!TestTrue(TEXT("Shortage quote remains valid"), Equipment->QuotePull(Banner->BannerId, 10, PullQuote))) return false;
    TestFalse(TEXT("Diamonds never cover missing tickets automatically"), PullQuote.bCanPull);
    TestEqual(TEXT("Quote identifies seven missing tickets"), PullQuote.MissingTickets, 7);
    Request = Equipment->MakeRequest(EEquipmentCommand::Pull); Request.BannerId = Banner->BannerId; Request.Count = 10;
    if (!Reject(TEXT("Ten-pull refuses three tickets even with diamonds"), Request, TEXT("InsufficientTickets"))) return false;
    ShortageFixture.ItemInventory.FindOrAdd(EItemType::EmployeeEquipmentTicketNormal) = 10;
    if (!Save->CommitEquipmentSnapshot(ShortageFixture)) return false;
    const int64 DiamondsBeforePull = Resources->GetResourceAmount(EResourceType::Diamond);
    const int32 InstancesBeforePull = Equipment->GetState().Instances.Num();
    Request = Equipment->MakeRequest(EEquipmentCommand::Pull); Request.BannerId = Banner->BannerId; Request.Count = 10;
    const FEmployeeEquipmentRequest PullRequest = Request;
    if (!Execute(TEXT("Ten-pull with matching tickets only"), Request, Result)) return false;
    const FEmployeeEquipmentResult PullResult = Result;
    TestEqual(TEXT("Ten-pull grants exactly ten instances"), Result.GrantedInstances.Num(), 10);
    TestEqual(TEXT("Ten-pull live inventory adds ten"), Equipment->GetState().Instances.Num(), InstancesBeforePull + 10);
    TestEqual(TEXT("Ten-pull ticket debit"), Items->GetItemCount(EItemType::EmployeeEquipmentTicketNormal), 0);
    TestEqual(TEXT("Ten-pull never debits diamonds"), Resources->GetResourceAmount(EResourceType::Diamond), DiamondsBeforePull);
    TestEqual(TEXT("Receipt records normal ticket tier"), Result.TicketType, EItemType::EmployeeEquipmentTicketNormal);
    TestEqual(TEXT("Receipt records requested banner"), Result.BannerId, Banner->BannerId);
    TSet<FGuid> GrantIds; bool bEpicOrHigher = false;
    for (const FEmployeeEquipmentInstance& Grant : PullResult.GrantedInstances)
    {
        GrantIds.Add(Grant.InstanceId);
        const FEmployeeEquipmentDefinitionRow* GrantedDefinition = Tables->FindEmployeeEquipmentDefinition(Grant.DefinitionId);
        TestTrue(TEXT("Granted definition belongs to active banner"), GrantedDefinition && Banner->DefinitionIds.Contains(Grant.DefinitionId));
        if (GrantedDefinition && GrantedDefinition->Grade >= EEmployeeEquipmentGrade::Epic) bEpicOrHigher = true;
    }
    TestEqual(TEXT("Ten-pull instance GUIDs are unique"), GrantIds.Num(), 10);
    TestTrue(TEXT("Fresh ten-pull reaches at least epic guarantee"), bEpicOrHigher);
    TArray<uint8> BeforeReplayDisk, AfterReplayDisk;
    if (!UGameplayStatics::LoadDataFromSlot(BeforeReplayDisk, TEXT("GameSlot"), 0)) return false;
    const FEmployeeEquipmentState BeforeReplayState = Equipment->GetState();
    Result = Equipment->Execute(PullRequest);
    TestTrue(TEXT("Replay returns original immutable result"), FEmployeeEquipmentResult::StaticStruct()->CompareScriptStruct(&Result, &PullResult, 0));
    TestTrue(TEXT("Replay does not mutate inventory, pity, or revision"), FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Equipment->GetState(), &BeforeReplayState, 0));
    TestTrue(TEXT("Replay does not write disk"), UGameplayStatics::LoadDataFromSlot(AfterReplayDisk, TEXT("GameSlot"), 0) && BeforeReplayDisk == AfterReplayDisk);
    if (!VerifyDisk(TEXT("Replay"))) return false;

    const EItemType TicketTypes[] = {EItemType::EmployeeEquipmentTicketNormal, EItemType::EmployeeEquipmentTicketAdvanced, EItemType::EmployeeEquipmentTicketPremium};
    const TCHAR* TierNames[] = {TEXT("Normal"), TEXT("Advanced"), TEXT("Premium")};
    const EEmployeeEquipmentGrade Floors[] = {EEmployeeEquipmentGrade::Common, EEmployeeEquipmentGrade::Rare, EEmployeeEquipmentGrade::Epic};
    for (int32 TierIndex = 0; TierIndex < 3; ++TierIndex)
    {
        const FName Product(*FString::Printf(TEXT("Dia_Equipment%s_10"), TierNames[TierIndex]));
        const FName TierBannerId(*FString::Printf(TEXT("Equipment%s"), TierNames[TierIndex]));
        const FEmployeeEquipmentBannerRow* TierBanner = Tables->FindEmployeeEquipmentBanner(TierBannerId);
        if (!TestNotNull(TEXT("Tier banner exists"), TierBanner)) return false;
        FEquipmentTicketShopRequest ShopRequest;
        FEquipmentTicketShopQuote ShopQuote;
        FName ShopError;
        if (!TestTrue(TEXT("Prepare real shop purchase"), Shop->PrepareEquipmentTicketPurchase(Product, ShopRequest, ShopQuote, ShopError))) return false;
        const int32 BeforeTickets = Items->GetItemCount(TicketTypes[TierIndex]);
        const int64 BeforeDiamonds = Resources->GetResourceAmount(EResourceType::Diamond);
        const int32 BeforeInstances = Equipment->GetState().Instances.Num();
        const FEquipmentTicketShopResult ShopResult = Shop->PurchaseEquipmentTickets(ShopRequest);
        if (!TestTrue(TEXT("Real ticket SKU commits"), ShopResult.bCommitted)) return false;
        TestEqual(TEXT("Ten-ticket SKU grants ten matching tickets"), Items->GetItemCount(TicketTypes[TierIndex]), BeforeTickets + 10);
        TestEqual(TEXT("Shop spends exactly quoted price"), Resources->GetResourceAmount(EResourceType::Diamond), BeforeDiamonds - ShopQuote.Price);
        TestEqual(TEXT("Buying a ticket creates no equipment"), Equipment->GetState().Instances.Num(), BeforeInstances);
        if (!VerifyDisk(TEXT("Ticket shop purchase"))) return false;
        TArray<uint8> ShopDiskBefore, ShopDiskAfter;
        UGameplayStatics::LoadDataFromSlot(ShopDiskBefore, TEXT("GameSlot"), 0);
        const FEquipmentTicketShopResult ReplayedShop = Shop->PurchaseEquipmentTickets(ShopRequest);
        TestTrue(TEXT("Same shop operation returns original receipt"), FEquipmentTicketShopResult::StaticStruct()->CompareScriptStruct(&ReplayedShop, &ShopResult, 0));
        TestEqual(TEXT("Shop retry cannot double grant"), Items->GetItemCount(TicketTypes[TierIndex]), BeforeTickets + 10);
        TestEqual(TEXT("Shop retry cannot double charge"), Resources->GetResourceAmount(EResourceType::Diamond), BeforeDiamonds - ShopQuote.Price);
        TestTrue(TEXT("Shop retry never rewrites disk"), UGameplayStatics::LoadDataFromSlot(ShopDiskAfter, TEXT("GameSlot"), 0) && ShopDiskBefore == ShopDiskAfter);
        const TMap<FName, FEmployeeEquipmentPityState> BeforePity = Equipment->GetState().BannerStates;
        Request = Equipment->MakeRequest(EEquipmentCommand::Pull); Request.BannerId = TierBannerId; Request.Count = 10;
        if (!Execute(TEXT("Shop tickets open ten boxes"), Request, Result)) return false;
        TestEqual(TEXT("Pull charges only matching tier"), Items->GetItemCount(TicketTypes[TierIndex]), BeforeTickets);
        TestEqual(TEXT("Tier pull records exact tickets"), Result.SpentTickets, 10);
        for (const auto& Granted : Result.GrantedInstances)
        {
            const auto* GrantedDefinition = Tables->FindEmployeeEquipmentDefinition(Granted.DefinitionId);
            TestTrue(TEXT("Higher ticket never awards excluded lower grade"), GrantedDefinition && GrantedDefinition->Grade >= Floors[TierIndex]);
        }
        for (const auto& Pair : BeforePity)
            if (Pair.Key != TierBanner->PityGroupId)
            {
                const FEmployeeEquipmentPityState After = Equipment->GetState().BannerStates.FindRef(Pair.Key);
                TestTrue(TEXT("Other ticket tier pity remains untouched"), FEmployeeEquipmentPityState::StaticStruct()->CompareScriptStruct(&Pair.Value, &After, 0));
            }
    }

    FEquipmentTicketShopRequest WeeklyRequest;
    FEquipmentTicketShopQuote WeeklyQuote;
    FName WeeklyError;
    if (!TestTrue(TEXT("Weekly free supply is in the existing shop"), Shop->PrepareEquipmentTicketPurchase(TEXT("Weekly_EquipmentNormal_14"), WeeklyRequest, WeeklyQuote, WeeklyError))) return false;
    const int32 BeforeWeeklyTickets = Items->GetItemCount(EItemType::EmployeeEquipmentTicketNormal);
    const int64 BeforeWeeklyDiamonds = Resources->GetResourceAmount(EResourceType::Diamond);
    const FEquipmentTicketShopResult WeeklyResult = Shop->PurchaseEquipmentTickets(WeeklyRequest);
    TestTrue(TEXT("Weekly supply commits"), WeeklyResult.bCommitted);
    TestEqual(TEXT("Weekly supply grants normal fourteen"), Items->GetItemCount(EItemType::EmployeeEquipmentTicketNormal), BeforeWeeklyTickets + 14);
    TestEqual(TEXT("Weekly supply costs no diamonds"), Resources->GetResourceAmount(EResourceType::Diamond), BeforeWeeklyDiamonds);
    TestEqual(TEXT("Weekly supply is exhausted for this period"), Shop->GetRemainingCount(TEXT("Weekly_EquipmentNormal_14")), 0);
    FEquipmentTicketShopRequest SecondWeeklyRequest;
    TestFalse(TEXT("Second weekly purchase cannot be prepared"), Shop->PrepareEquipmentTicketPurchase(TEXT("Weekly_EquipmentNormal_14"), SecondWeeklyRequest, WeeklyQuote, WeeklyError));
    TestEqual(TEXT("Weekly limit is the rejection reason"), WeeklyError, FName(TEXT("PurchaseLimit")));
    if (!VerifyDisk(TEXT("Weekly free ticket supply"))) return false;

    FEquipmentTicketShopRequest FailedShopRequest;
    FEquipmentTicketShopQuote FailedShopQuote;
    FName FailedShopError;
    if (!Shop->PrepareEquipmentTicketPurchase(TEXT("Dia_EquipmentNormal_1"), FailedShopRequest, FailedShopQuote, FailedShopError)) return false;
    const FGameSaveData BeforeFailureCache = Save->GetCurrentSaveData()->GameData;
    const FEmployeeEquipmentState BeforeFailureEquipment = Equipment->GetState();
    const FShopSaveData BeforeFailureShop = Shop->GetShopData();
    const TMap<EItemType, int32> BeforeFailureItems = Items->GetAllItems();
    const TMap<EResourceType, int64> BeforeFailureResources = Resources->GetAllResources();
    TArray<uint8> FailureDiskBefore, FailureDiskAfter;
    UGameplayStatics::LoadDataFromSlot(FailureDiskBefore, TEXT("GameSlot"), 0);
    const FString SavePath = FPaths::ProjectSavedDir() / TEXT("SaveGames/GameSlot.sav");
    const bool bWasReadOnly = IFileManager::Get().IsReadOnly(*SavePath);
    {
        if (!TestTrue(TEXT("Make only the backed-up test save unwritable"), FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SavePath, true))) return false;
        ON_SCOPE_EXIT { FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*SavePath, bWasReadOnly); };
        AddExpectedError(TEXT("Failed to save game!"), EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("SaveFailed"), EAutomationExpectedErrorFlags::Contains, 1);
        const FEquipmentTicketShopResult Failed = Shop->PurchaseEquipmentTickets(FailedShopRequest);
        TestFalse(TEXT("Real disk failure rejects ticket purchase"), Failed.bCommitted);
        TestEqual(TEXT("Real disk failure reports SaveFailed"), Failed.ErrorCode, FName(TEXT("SaveFailed")));
    }
    TestTrue(TEXT("Failed purchase leaves full cache unchanged"), FGameSaveData::StaticStruct()->CompareScriptStruct(&Save->GetCurrentSaveData()->GameData, &BeforeFailureCache, 0));
    TestTrue(TEXT("Failed purchase leaves equipment unchanged"), FEmployeeEquipmentState::StaticStruct()->CompareScriptStruct(&Equipment->GetState(), &BeforeFailureEquipment, 0));
    TestTrue(TEXT("Failed purchase leaves stock and receipts unchanged"), FShopSaveData::StaticStruct()->CompareScriptStruct(&Shop->GetShopData(), &BeforeFailureShop, 0));
    TestTrue(TEXT("Failed purchase leaves item balances unchanged"), Items->GetAllItems().OrderIndependentCompareEqual(BeforeFailureItems));
    TestTrue(TEXT("Failed purchase leaves currency balances unchanged"), Resources->GetAllResources().OrderIndependentCompareEqual(BeforeFailureResources));
    TestTrue(TEXT("Failed purchase leaves actual disk bytes unchanged"), UGameplayStatics::LoadDataFromSlot(FailureDiskAfter, TEXT("GameSlot"), 0) && FailureDiskBefore == FailureDiskAfter);
    const FEquipmentTicketShopResult RetriedAfterFailure = Shop->PurchaseEquipmentTickets(FailedShopRequest);
    TestTrue(TEXT("Same operation can retry after disk recovers"), RetriedAfterFailure.bCommitted);
    if (!VerifyDisk(TEXT("Ticket purchase after storage recovery"))) return false;
    TestEqual(TEXT("Employee roster count remains intact"), Employees->GetEmployeeCount(), ExistingEmployees.Num());
    AddInfo(FString::Printf(TEXT("Real PIE integration completed for employees %d and %d; all committed steps read back GameSlot."), FirstEmployeeId, SecondEmployeeId));
    return RestoreScope.Restore();
}

namespace
{
    void RunEquipmentQAIntegration()
    {
        // Automation RunTests는 Controller::StopTests에서 PIE 종료를 예약한다.
        // Framework만 직접 호출하면 로그/예상 오류 검증을 유지하면서 현재 PIE를 보존한다.
        if (GIsAutomationTesting || GIsSlowTask || GIsPlayInEditorWorld)
        {
            UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] RunIntegration requires the editor console/remote Python context with no other automation or slow task."));
            return;
        }
        FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
        Framework.StartTestByName(TEXT("FEmployeeEquipmentPIEIntegrationTest"), 0, TEXT("CGR.Tools.EmployeeEquipmentPIEIntegration"));
        if (!GIsAutomationTesting)
        {
            UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] Integration test did not start."));
            return;
        }
        if (!Framework.IsLatentCommandQueueEmpty())
        {
            Framework.GetCurrentTest()->AddError(TEXT("Equipment integration must finish synchronously before restoration is reported."));
            Framework.DequeueAllCommands();
        }
        FAutomationTestExecutionInfo Execution;
        const bool bPassed = Framework.StopTest(Execution);
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetStringField(TEXT("test"), TEXT("CGR.Tools.EmployeeEquipmentPIEIntegration"));
        Report->SetStringField(TEXT("finishedUtc"), FDateTime::UtcNow().ToIso8601());
        Report->SetBoolField(TEXT("passed"), bPassed);
        Report->SetBoolField(TEXT("restoreAttempted"), bLastIntegrationRestoreAttempted);
        Report->SetBoolField(TEXT("restoreSucceeded"), bLastIntegrationRestoreSucceeded);
        Report->SetNumberField(TEXT("errors"), Execution.GetErrorTotal());
        Report->SetNumberField(TEXT("warnings"), Execution.GetWarningTotal());
        TArray<TSharedPtr<FJsonValue>> Entries;
        for (const FAutomationExecutionEntry& Entry : Execution.GetEntries())
            Entries.Add(MakeShared<FJsonValueString>(Entry.ToString()));
        Report->SetArrayField(TEXT("entries"), Entries);
        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        const FString ReportPath = FPaths::ProjectSavedDir() / TEXT("EmployeeEquipment/PIEIntegration.json");
        const bool bReportSaved = FJsonSerializer::Serialize(Report, Writer) &&
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true) &&
            FFileHelper::SaveStringToFile(Serialized, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        for (const FAutomationExecutionEntry& Entry : Execution.GetEntries())
            if (Entry.Event.Type == EAutomationEventType::Error)
                UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] %s"), *Entry.Event.Message);
        UE_LOG(LogTemp, Display, TEXT("[EquipmentQA] Integration passed=%d restoreAttempted=%d restored=%d errors=%d reportSaved=%d report=%s"),
            bPassed, bLastIntegrationRestoreAttempted, bLastIntegrationRestoreSucceeded, Execution.GetErrorTotal(), bReportSaved, *ReportPath);
    }

    FAutoConsoleCommand RunEquipmentQAIntegrationCommand(
        TEXT("CGR.EquipmentQA.RunIntegration"),
        TEXT("Runs the actual equipment integration test synchronously in an existing PIE without closing it; writes Saved/EmployeeEquipment/PIEIntegration.json."),
        FConsoleCommandDelegate::CreateStatic(&RunEquipmentQAIntegration));

    void OpenEquipmentQAOffice(const TArray<FString>& Arguments)
    {
        int32 EmployeeId = INDEX_NONE;
        if (Arguments.Num() != 1 || !LexTryParseString(EmployeeId, *Arguments[0]) || EmployeeId <= 0)
        {
            UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] Usage: CGR.EquipmentQA.OpenOffice <existing EmployeeID>"));
            return;
        }
        UWorld* PIEWorld = nullptr;
        int32 PIECount = 0;
        if (GEngine)
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::PIE && Context.World()) { PIEWorld = Context.World(); ++PIECount; }
        if (PIECount != 1 || !PIEWorld || !Cast<AMainMapPlayerController>(PIEWorld->GetFirstPlayerController()))
        {
            UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] OpenOffice requires exactly one MainMap PIE session."));
            return;
        }
        UCGGameInstance* GameInstance = Cast<UCGGameInstance>(PIEWorld->GetGameInstance());
        UEmployeeManager* Employees = GameInstance ? GameInstance->GetSubsystem<UEmployeeManager>() : nullptr;
        USaveLoadManager* Save = GameInstance ? GameInstance->GetSubsystem<USaveLoadManager>() : nullptr;
        const FEmployeeInstance* Employee = Employees ? Employees->FindEmployee(EmployeeId) : nullptr;
        if (!GameInstance || GameInstance->IsVisitMode() || !Save || !Employee || Employee->AssignedBuildingIndex <= 0)
        {
            UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] Employee %d has no valid owned building context."), EmployeeId);
            return;
        }
        const int32 BuildingIndex = Employee->AssignedBuildingIndex;
        UEntityManager* Entities = PIEWorld->GetSubsystem<UEntityManager>();
        ABuildingBaseActor* Building = Entities ? Entities->GetBuildingByIndex(BuildingIndex) : nullptr;
        if (!IsValid(Building) || Building->GetWorld() != PIEWorld || Building->GetCompanyType() == ECompanyType::None)
        {
            UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] No live MainMap building %d for employee %d; nothing changed."), BuildingIndex, EmployeeId);
            return;
        }
        if (!Save->SaveGameData())
        {
            UE_LOG(LogTemp, Error, TEXT("[EquipmentQA] Save failed; OfficeMap transition cancelled."));
            return;
        }
        GameInstance->SetCurrentManagedBuilding(Building);
        GameInstance->SetCurrentBuildingCompanyType(Building->GetCompanyType());
        GameInstance->SetOfficeMode(EOfficeMode::Normal);
        UE_LOG(LogTemp, Display, TEXT("[EquipmentQA] Opening existing office %d for employee %d."), BuildingIndex, EmployeeId);
        GameInstance->TransitionToLevel(TEXT("OfficeMap"));
    }

    FAutoConsoleCommand OpenEquipmentQAOfficeCommand(
        TEXT("CGR.EquipmentQA.OpenOffice"),
        TEXT("Opens the existing assigned office of an employee in MainMap PIE after a successful save."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&OpenEquipmentQAOffice));
}

#endif
