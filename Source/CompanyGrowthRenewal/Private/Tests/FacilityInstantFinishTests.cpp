#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Core/CGGameInstance.h"
#include "Data/FacilityInstantFinishRules.h"
#include "Global/CGDevSettings.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/MineManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityFinishQuoteTest, "CGR.Overseas.InstantFinish.Quote",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFacilityFinishQuoteTest::RunTest(const FString& Parameters)
{
	FInstantFinishBalanceTable Balance;
	Balance.SecondsPerDiamond = 60;
	Balance.MaxCost = 1000000;
	Balance.MaxDiscountBasisPoints = 9500;
	TestEqual(TEXT("Finished timers have no charge"), FFacilityInstantFinishRules::CalculateCost(0, Balance, 0), int64(0));
	TestEqual(TEXT("One second still costs one diamond"), FFacilityInstantFinishRules::CalculateCost(1, Balance, 0), int64(1));
	TestEqual(TEXT("One minute boundary"), FFacilityInstantFinishRules::CalculateCost(60, Balance, 0), int64(1));
	TestEqual(TEXT("Partial second minute rounds up"), FFacilityInstantFinishRules::CalculateCost(61, Balance, 0), int64(2));
	TestEqual(TEXT("Ten minutes cost ten diamonds"), FFacilityInstantFinishRules::CalculateCost(600, Balance, 0), int64(10));
	TestEqual(TEXT("Rush discount applies before rounding"), FFacilityInstantFinishRules::CalculateCost(600, Balance, 5000), int64(5));
	TestEqual(TEXT("Maximum discount never makes a pending timer free"), FFacilityInstantFinishRules::CalculateCost(1, Balance, 9500), int64(1));
	TestEqual(TEXT("Invalid full discount is rejected"), FFacilityInstantFinishRules::CalculateCost(600, Balance, 10000), int64(0));
	Balance.MaxCost = 2;
	TestEqual(TEXT("Excessive duration is rejected, not discounted by a cap"), FFacilityInstantFinishRules::CalculateCost(121, Balance, 0), int64(0));
	Balance.SecondsPerDiamond = 0;
	TestEqual(TEXT("Missing rate does not produce a free completion"), FFacilityInstantFinishRules::CalculateCost(1, Balance, 0), int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityFinishPurchaseTest, "CGR.Overseas.InstantFinish.Purchase",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFacilityFinishPurchaseTest::RunTest(const FString& Parameters)
{
	if (!FPaths::ProjectSavedDir().Contains(TEXT("Saved_OverseasDioramaInstantFinish")))
	{
		AddWarning(TEXT("Skipped: use -saveddirsuffix=OverseasDioramaInstantFinish<unique>"));
		return true;
	}
	if (!TestNull(TEXT("No playable instance"), UCGGameInstance::GetInstance())) return false;
	const FString SaveSlot(TEXT("GameSlot"));
	if (!TestFalse(TEXT("Dedicated fixture save starts empty"), UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))) return false;
	UCGDevSettings* Settings = GetMutableDefault<UCGDevSettings>();
	const ECGBackendMode PreviousMode = Settings->BackendMode;
	Settings->BackendMode = ECGBackendMode::Local;
	UCGGameInstance* Instance = nullptr;
	auto CloseInstance = [&Instance]()
	{
		if (!Instance) return;
		UWorld* TestWorld = static_cast<UGameInstance*>(Instance)->GetWorld();
		static_cast<UGameInstance*>(Instance)->Shutdown();
		if (TestWorld) GEngine->DestroyWorldContext(TestWorld);
		Instance = nullptr;
	};
	ON_SCOPE_EXIT
	{
		CloseInstance();
		UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
		Settings->BackendMode = PreviousMode;
	};
	Instance = NewObject<UCGGameInstance>(GEngine);
	Instance->InitializeStandalone();
	USaveLoadManager* Save = Instance->GetSubsystem<USaveLoadManager>();
	UMineManager* Mine = Instance->GetSubsystem<UMineManager>();
	UWorldFactoryManager* Factory = Instance->GetSubsystem<UWorldFactoryManager>();
	UResourceItemManager* Resources = Instance->GetSubsystem<UResourceItemManager>();
	UCloudEconomySubsystem* Cloud = Instance->GetSubsystem<UCloudEconomySubsystem>();
	if (!TestTrue(TEXT("Managers available"), Save && Mine && Factory && Resources && Cloud)) return false;
	Save->LoadGameData();
	if (!TestTrue(TEXT("Local wallet ready"), Cloud->IsEconomyReady())) return false;

	TMap<EResourceType, int64> Bank = Resources->GetAllResources();
	Bank.Add(EResourceType::Money, 1000000000);
	Bank.Add(EResourceType::Diamond, 0);
	Resources->SetAllResources(Bank, false);
	constexpr ECountryType MineCountry = ECountryType::Australia;
	constexpr EResourceType Ore = EResourceType::Lithium;
	constexpr int64 StartingDiamonds = 20000;
	Mine->EnsureCountryLines(MineCountry);
	TMap<ECountryType, FCountryMineData> MineData;
	Mine->SerializeForSave(MineData);
	MineData.FindChecked(MineCountry).UpgradeLevels.Add(EMineUpgradeType::MiningRate, 2);
	MineData.FindChecked(MineCountry).UpgradeLevels.Add(EMineUpgradeType::MiningStorage, 99);
	Mine->LoadFromSave(MineData);
	if (!TestTrue(TEXT("Mine is installed"), Mine->CreateMineLine(MineCountry, Ore, 10000))) return false;
	const int64 MineCost = Mine->GetInstantFinishCost(MineCountry, Ore);
	if (!TestTrue(TEXT("Mine displays a nonzero time price"), MineCost > 0)) return false;
	bool bSucceeded = true;
	FName Failure;
	auto Done = FOnCloudEconomyOpDone::CreateLambda([&](bool bOk, FName Code) { bSucceeded = bOk; Failure = Code; });
	Mine->RequestInstantFinish(MineCountry, Ore, MineCost, Done);
	TestFalse(TEXT("Insufficient diamonds reject completion"), bSucceeded);
	TestEqual(TEXT("Insufficient balance is identified"), Failure, CloudErr::InsufficientFunds);
	FMineLineState MineState;
	Mine->GetLineState(MineCountry, Ore, MineState);
	TestFalse(TEXT("Failed payment does not fill storage"), MineState.bSaturated);
	TestEqual(TEXT("Failed payment does not debit"), Resources->GetResourceAmount(EResourceType::Diamond), int64(0));
	Resources->StoreResource(EResourceType::Diamond, StartingDiamonds, false);
	bool bObservedAtomicCompletion = false;
	const FDelegateHandle WalletObserver = Resources->OnResourceChanged.AddLambda(
		[&](EResourceType Type, int64 Amount)
		{
			if (Type != EResourceType::Diamond) return;
			FMineLineState Observed;
			bObservedAtomicCompletion = Mine->GetLineState(MineCountry, Ore, Observed)
				&& Observed.bSaturated && Amount == StartingDiamonds - MineCost;
		});
	Mine->RequestInstantFinish(MineCountry, Ore, MineCost, Done);
	Resources->OnResourceChanged.Remove(WalletObserver);
	if (!TestTrue(TEXT("Paid mine completion succeeds"), bSucceeded)) return false;
	TestTrue(TEXT("Wallet observers see completion already applied"), bObservedAtomicCompletion);
	TestEqual(TEXT("Exactly quoted mine cost debited"), Resources->GetResourceAmount(EResourceType::Diamond), StartingDiamonds - MineCost);
	Mine->GetLineState(MineCountry, Ore, MineState);
	TestTrue(TEXT("Paid slow mine becomes full without float-rounded final unit loss"), MineState.bSaturated);
	TestEqual(TEXT("Large mine receives its final unit"), MineState.CurrentQty, int64(10000));
	TestEqual(TEXT("Completed mine has no purchase price"), Mine->GetInstantFinishCost(MineCountry, Ore), int64(0));
	Mine->RequestInstantFinish(MineCountry, Ore, MineCost, Done);
	TestFalse(TEXT("Repeated completion rejected"), bSucceeded);
	TestEqual(TEXT("Repeated completion does not charge"), Resources->GetResourceAmount(EResourceType::Diamond), StartingDiamonds - MineCost);
	const FName PendingKey(*FString::Printf(TEXT("mine.instantFinish.%d.%d"),
		static_cast<int32>(MineCountry), static_cast<int32>(Ore)));
	TestTrue(TEXT("Pending guard can be acquired"), Cloud->BeginPendingAction(PendingKey));
	TestFalse(TEXT("Pending purchase blocks manual/automatic claim"), Mine->ClaimResource(MineCountry, Ore));
	TestFalse(TEXT("Pending purchase blocks rate mutation"), Mine->UpgradeLevelUp(MineCountry, EMineUpgradeType::MiningRate));
	Mine->RequestInstantFinish(MineCountry, Ore, MineCost, Done);
	TestFalse(TEXT("Concurrent purchase rejected"), bSucceeded);
	TestTrue(TEXT("Rejected duplicate does not release another request"), Cloud->IsActionPending(PendingKey));
	Cloud->EndPendingAction(PendingKey);
	TestTrue(TEXT("Completed mine can be claimed normally"), Mine->ClaimResource(MineCountry, Ore));

	const int32 LineId = Factory->StartProduction(ECountryType::Korea, ECompanyType::Electronics, 1,
		FText::FromString(TEXT("InstantFinishTest")), TSoftObjectPtr<UTexture2D>(), 1.0f, 600);
	if (!TestTrue(TEXT("Factory starts"), LineId > 0)) return false;
	TMap<ECountryType, FCountryFactoryData> FactoryData;
	Factory->SerializeForSave(FactoryData);
	FactoryData.FindChecked(ECountryType::Korea).UpgradeLevels.Add(EWorldFactoryUpgradeType::RushProduction, 250);
	Factory->LoadFromSave(FactoryData);
	const int64 FactoryCost = Factory->GetInstantFinishCost(ECountryType::Korea, LineId);
	TestEqual(TEXT("Live RushProduction reduces ten-minute quote to five"), FactoryCost, int64(5));
	const int64 BeforeFactory = Resources->GetResourceAmount(EResourceType::Diamond);
	Factory->RequestInstantFinish(ECountryType::Korea, LineId, FactoryCost - 1, Done);
	TestFalse(TEXT("More expensive than accepted quote rejected"), bSucceeded);
	TestEqual(TEXT("Stale low quote does not charge"), Resources->GetResourceAmount(EResourceType::Diamond), BeforeFactory);
	Factory->RequestInstantFinish(ECountryType::Korea, LineId, FactoryCost, Done);
	if (!TestTrue(TEXT("Paid factory completion succeeds"), bSucceeded)) return false;
	TestEqual(TEXT("Exactly factory cost debited"), Resources->GetResourceAmount(EResourceType::Diamond), BeforeFactory - FactoryCost);
	FWorldFactoryLineState FactoryState;
	Factory->GetLineState(ECountryType::Korea, LineId, FactoryState);
	TestTrue(TEXT("Factory completes after payment"), FactoryState.bCompleted);
	Factory->RequestInstantFinish(ECountryType::Korea, LineId, FactoryCost, Done);
	TestFalse(TEXT("Repeated factory payment rejected"), bSucceeded);
	const int64 FinalBalance = BeforeFactory - FactoryCost;
	TestEqual(TEXT("Factory duplicate leaves wallet unchanged"), Resources->GetResourceAmount(EResourceType::Diamond), FinalBalance);

	CloseInstance();
	Instance = NewObject<UCGGameInstance>(GEngine);
	Instance->InitializeStandalone();
	Save = Instance->GetSubsystem<USaveLoadManager>();
	if (!TestTrue(TEXT("Paid completion save loads"), Save->LoadGameData())) return false;
	Resources = Instance->GetSubsystem<UResourceItemManager>();
	Factory = Instance->GetSubsystem<UWorldFactoryManager>();
	TestEqual(TEXT("Debit survives restart"), Resources->GetResourceAmount(EResourceType::Diamond), FinalBalance);
	if (!TestTrue(TEXT("Completed line survives restart"), Factory->GetLineState(ECountryType::Korea, LineId, FactoryState))) return false;
	TestTrue(TEXT("Completion survives restart"), FactoryState.bCompleted);
	TestTrue(TEXT("Paid product can be claimed"), Factory->ClaimLine(ECountryType::Korea, LineId));
	const int32 UnsavedLine = Factory->StartProduction(ECountryType::Korea, ECompanyType::Electronics, 1,
		FText::FromString(TEXT("PersistenceFailure")), TSoftObjectPtr<UTexture2D>(), 1.0f, 600);
	if (!TestTrue(TEXT("Persistence failure fixture starts"), UnsavedLine > 0)) return false;
	Save->SuppressSaving();
	Factory->RequestInstantFinish(ECountryType::Korea, UnsavedLine,
		Factory->GetInstantFinishCost(ECountryType::Korea, UnsavedLine), Done);
	TestFalse(TEXT("Failed persistence rejects purchase"), bSucceeded);
	TestEqual(TEXT("Failed persistence cannot deduct diamonds"),
		Resources->GetResourceAmount(EResourceType::Diamond), FinalBalance);
	Factory->GetLineState(ECountryType::Korea, UnsavedLine, FactoryState);
	TestFalse(TEXT("Failed persistence cannot complete the line"), FactoryState.bCompleted);
	TestFalse(TEXT("Failed local purchase releases pending lock"),
		Factory->IsInstantFinishPending(ECountryType::Korea, UnsavedLine));
	return true;
}

#endif
