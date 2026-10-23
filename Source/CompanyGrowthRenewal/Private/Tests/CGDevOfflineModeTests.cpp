#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Global/CGDevSettings.h"
#include "Core/CGGameInstance.h"
#include "Data/CloudEconomyData.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Utils/CloudHttp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGDevOfflineModeTest, "CGR.Dev.OfflineModeSelection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGDevOfflineModeTest::RunTest(const FString& Parameters)
{
	UCGDevSettings* Settings = GetMutableDefault<UCGDevSettings>();
	const ECGBackendMode PreviousMode = Settings->BackendMode;

	Settings->BackendMode = ECGBackendMode::Local;
	TestTrue(TEXT("Local selection disables the backend in development"), UCGDevSettings::IsOfflineMode());
	UGameInstance* Instance = NewObject<UGameInstance>();
	UGameClockSubsystem* Clock = NewObject<UGameClockSubsystem>(Instance);
	USaveLoadManager* Save = NewObject<USaveLoadManager>(Instance);
	TestTrue(TEXT("Local selection enables gameplay dates without a server sync"), Clock->IsServerTimeValid());
	TestEqual(TEXT("Local selection keeps the normal save"), Save->GetActiveSaveSlotName(), FString(TEXT("GameSlot")));
	TestEqual(TEXT("Local portraits remain in their normal folder"), UCGDevSettings::GetPortraitFolderName(), FString(TEXT("Portraits")));
	FCloudRequest Request;
	Request.AbsoluteUrl = TEXT("https://example.invalid/unused");
	TSharedRef<bool, ESPMode::ThreadSafe> TransportCompleted = MakeShared<bool, ESPMode::ThreadSafe>(false);
	TSharedRef<FString, ESPMode::ThreadSafe> TransportCode = MakeShared<FString, ESPMode::ThreadSafe>();
	FCloudHttp::Send(Request, [TransportCompleted, TransportCode](const FCloudResponse& Response)
	{
		*TransportCompleted = true;
		*TransportCode = Response.Code;
	});
	TestTrue(TEXT("Local transport completes without scheduling HTTP"), *TransportCompleted);
	if (*TransportCompleted)
	{
		TestEqual(TEXT("Local mode rejects cloud transport"), *TransportCode, FString(TEXT("AUTH_REQUIRED")));
	}

	Settings->BackendMode = ECGBackendMode::Server;
	TestFalse(TEXT("Server selection uses the backend"), UCGDevSettings::IsOfflineMode());
	TestFalse(TEXT("Server selection still requires a server time sync"), Clock->IsServerTimeValid());
	TestEqual(TEXT("Server testing uses its own save"), Save->GetActiveSaveSlotName(), FString(TEXT("GameSlot_Server")));
	TestEqual(TEXT("Server testing uses separate portraits"), UCGDevSettings::GetPortraitFolderName(), FString(TEXT("Portraits_Server")));

	Settings->BackendMode = PreviousMode;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGDevOfflineEconomyIntegrationTest, "CGR.Dev.OfflineEconomyIntegration",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGDevOfflineEconomyIntegrationTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_OfflineVerify")))
	{
		AddWarning(TEXT("Skipped: run with -saveddirsuffix=OfflineVerify to isolate the player's save"));
		return true;
	}

	const FString SlotName(TEXT("GameSlot"));
	if (!TestFalse(TEXT("The isolated save slot starts empty"),
		UGameplayStatics::DoesSaveGameExist(SlotName, 0))) return false;
	if (!TestNull(TEXT("No playable game instance may be active during the isolated test"),
		UCGGameInstance::GetInstance())) return false;

	UCGDevSettings* Settings = GetMutableDefault<UCGDevSettings>();
	const ECGBackendMode PreviousMode = Settings->BackendMode;
	Settings->BackendMode = ECGBackendMode::Local;
	UCGGameInstance* FirstInstance = nullptr;
	UCGGameInstance* SecondInstance = nullptr;
	auto CloseInstance = [](UCGGameInstance*& Instance)
	{
		if (!Instance) return;
		UWorld* TestWorld = static_cast<UGameInstance*>(Instance)->GetWorld();
		static_cast<UGameInstance*>(Instance)->Shutdown();
		if (TestWorld) GEngine->DestroyWorldContext(TestWorld);
		Instance = nullptr;
	};
	ON_SCOPE_EXIT
	{
		CloseInstance(SecondInstance);
		CloseInstance(FirstInstance);
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		Settings->BackendMode = PreviousMode;
	};

	FirstInstance = NewObject<UCGGameInstance>(GEngine);
	FirstInstance->InitializeStandalone();
	USaveLoadManager* FirstSave = FirstInstance->GetSubsystem<USaveLoadManager>();
	UCloudEconomySubsystem* FirstEconomy = FirstInstance->GetSubsystem<UCloudEconomySubsystem>();
	UResourceItemManager* FirstResources = FirstInstance->GetSubsystem<UResourceItemManager>();
	UItemInventoryManager* FirstItems = FirstInstance->GetSubsystem<UItemInventoryManager>();
	if (!TestTrue(TEXT("Local game instance has economy and save managers"),
		FirstSave && FirstEconomy && FirstResources && FirstItems)) return false;
	TestFalse(TEXT("New local game has no disk save yet"), FirstSave->LoadGameData());
	if (!TestTrue(TEXT("New local game unlocks economy after load"), FirstEconomy->IsEconomyReady())) return false;
	TestEqual(TEXT("Local play no longer reports server syncing"),
		FirstEconomy->GetBlockState(), ECloudBlockState::None);

	const int64 InitialDiamond = FirstResources->GetResourceAmount(EResourceType::Diamond);
	const int32 InitialCards = FirstItems->GetItemCount(EItemType::BusinessCardPaper);
	FirstResources->StoreResource(EResourceType::Diamond, 100, false);
	FirstItems->AddItem(EItemType::BusinessCardPaper, 2, false);

	FCloudOpResult ConsumeResult;
	int32 ConsumeCallbacks = 0;
	FirstEconomy->RequestConsume(FName(TEXT("employee.potentialReroll")),
		FCloudEconomyData::ItemTypeToName(EItemType::BusinessCardPaper), 1,
		UCloudEconomySubsystem::FOnCloudOp::CreateLambda([&](const FCloudOpResult& Result)
		{
			++ConsumeCallbacks;
			ConsumeResult = Result;
		}));
	TestEqual(TEXT("Card consume completes once"), ConsumeCallbacks, 1);
	if (!TestTrue(TEXT("Potential reroll card consume succeeds offline"), ConsumeResult.bOk)) return false;
	TestEqual(TEXT("Exactly one card was spent"), FirstItems->GetItemCount(EItemType::BusinessCardPaper), InitialCards + 1);

	FCloudOpResult SpendResult;
	FirstEconomy->RequestSpend(CloudCurrency::Diamond, 30, FName(TEXT("gacha.recruit.advanced")), FCloudMeta(),
		UCloudEconomySubsystem::FOnCloudOp::CreateLambda([&](const FCloudOpResult& Result) { SpendResult = Result; }));
	if (!TestTrue(TEXT("Diamond spend succeeds offline"), SpendResult.bOk)) return false;
	TestEqual(TEXT("Diamond spend uses the local wallet"),
		FirstResources->GetResourceAmount(EResourceType::Diamond), InitialDiamond + 70);

	const FString ClaimKey(TEXT("codex.tier.OfflineVerify"));
	FCloudOpResult CreditResult;
	FirstEconomy->RequestCredit(FName(TEXT("codex.tierMilestone")), ClaimKey, 20,
		UCloudEconomySubsystem::FOnCloudOp::CreateLambda([&](const FCloudOpResult& Result) { CreditResult = Result; }));
	if (!TestTrue(TEXT("Milestone credit succeeds offline"), CreditResult.bOk)) return false;
	TestEqual(TEXT("Milestone credit updates the local wallet"),
		FirstResources->GetResourceAmount(EResourceType::Diamond), InitialDiamond + 90);

	CloseInstance(FirstInstance);
	TestNull(TEXT("Shutdown clears the game instance singleton"), UCGGameInstance::GetInstance());
	SecondInstance = NewObject<UCGGameInstance>(GEngine);
	SecondInstance->InitializeStandalone();
	USaveLoadManager* SecondSave = SecondInstance->GetSubsystem<USaveLoadManager>();
	UCloudEconomySubsystem* SecondEconomy = SecondInstance->GetSubsystem<UCloudEconomySubsystem>();
	UResourceItemManager* SecondResources = SecondInstance->GetSubsystem<UResourceItemManager>();
	UItemInventoryManager* SecondItems = SecondInstance->GetSubsystem<UItemInventoryManager>();
	if (!TestTrue(TEXT("Restarted local game has economy and save managers"),
		SecondSave && SecondEconomy && SecondResources && SecondItems)) return false;
	if (!TestTrue(TEXT("Restarted game loads the local save"), SecondSave->LoadGameData())) return false;
	TestTrue(TEXT("Restarted game economy is ready"), SecondEconomy->IsEconomyReady());
	TestEqual(TEXT("Spent card persists after restart"),
		SecondItems->GetItemCount(EItemType::BusinessCardPaper), InitialCards + 1);
	TestEqual(TEXT("Wallet persists after restart"),
		SecondResources->GetResourceAmount(EResourceType::Diamond), InitialDiamond + 90);

	FCloudOpResult DuplicateCreditResult;
	SecondEconomy->RequestCredit(FName(TEXT("codex.tierMilestone")), ClaimKey, 20,
		UCloudEconomySubsystem::FOnCloudOp::CreateLambda([&](const FCloudOpResult& Result) { DuplicateCreditResult = Result; }));
	TestFalse(TEXT("The same claim cannot credit twice after restart"), DuplicateCreditResult.bOk);
	TestEqual(TEXT("Duplicate claim returns ALREADY_CLAIMED"), DuplicateCreditResult.Code, CloudErr::AlreadyClaimed);
	TestEqual(TEXT("Duplicate claim leaves the wallet unchanged"),
		SecondResources->GetResourceAmount(EResourceType::Diamond), InitialDiamond + 90);
	return true;
}

#endif
