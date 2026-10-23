#include "Manager/OverseasActions.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TradePort.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CountryInfoTable.h"
#include "Table/ProductRecipeTable.h"
#include "Data/WorldFactoryUpgradeData.h"
#include "Data/WorldMapTypes.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "Enum/ResourceType.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OverseasActionsTests
{
	constexpr double kOverseasLongAgoSec = 10.0 * 24.0 * 3600.0;

	// 새 게임 인스턴스는 개발 설정(세이브 지우기)·종료 저장으로 디스크를 건드린다 ― 격리된 Saved 에서만 돈다.
	bool IsIsolatedRun(FAutomationTestBase& Test)
	{
		if (FPaths::ProjectSavedDir().Contains(TEXT("Saved_OverseasDiorama")))
		{
			return true;
		}
		Test.AddWarning(TEXT("Skipped: -saveddirsuffix=OverseasDiorama 로 실행해야 합니다"));
		return false;
	}

	struct FFixture
	{
		UGameInstance* Instance = nullptr;
		UWorld* TestWorld = nullptr;
		UWorldFactoryManager* Factories = nullptr;
		UMineManager* Mines = nullptr;
		UWorldMapManager* Warehouse = nullptr;
		UTradePort* Trade = nullptr;
		UResourceItemManager* Resources = nullptr;
		UTableManagerSubsystem* Tables = nullptr;

		FFixture()
		{
			if (!GEngine) return;
			Instance = NewObject<UGameInstance>(GEngine);
			Instance->InitializeStandalone();
			TestWorld = Instance->GetWorld();
			if (USaveLoadManager* SaveMgr = Instance->GetSubsystem<USaveLoadManager>())
			{
				SaveMgr->SuppressSaving();
			}
			Factories = Instance->GetSubsystem<UWorldFactoryManager>();
			Mines = Instance->GetSubsystem<UMineManager>();
			Warehouse = Instance->GetSubsystem<UWorldMapManager>();
			Trade = Instance->GetSubsystem<UTradePort>();
			Resources = Instance->GetSubsystem<UResourceItemManager>();
			Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
		}

		~FFixture()
		{
			if (Instance) Instance->Shutdown();
			if (TestWorld)
			{
				TestWorld->SetGameInstance(nullptr);
				if (GEngine) GEngine->DestroyWorldContext(TestWorld);
				TestWorld->DestroyWorld(false);
			}
		}

		bool IsReady() const
		{
			return Factories && Mines && Warehouse && Trade && Resources && Tables;
		}

		void SetMarketCap(int64 Amount) const
		{
			TMap<EResourceType, int64> Bank = Resources->GetAllResources();
			Bank.Add(EResourceType::MarketCap, Amount);
			Resources->SetAllResources(Bank, /*bShouldBroadcast=*/false);
		}
	};

	FWorldFactoryLineState MakeFactoryLine(int32 LineId, float RatePerSecond, int64 TargetQty, double StartedSecondsAgo)
	{
		FWorldFactoryLineState Line;
		Line.LineId = LineId;
		Line.ProductIndex = 3;
		Line.CompanyType = ECompanyType::Electronics;
		Line.RatePerSecond = RatePerSecond;
		Line.TargetQty = TargetQty;
		Line.StartWallUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(StartedSecondsAgo);
		return Line;
	}

	FMineLineState MakeMineLine(EResourceType Resource, float BaseRatePerSecond, int64 OverrideMaxStorage, double StartedSecondsAgo)
	{
		FMineLineState Line;
		Line.Resource = Resource;
		Line.BaseRatePerSecond = BaseRatePerSecond;
		Line.OverrideMaxStorage = OverrideMaxStorage;
		Line.StartWallUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(StartedSecondsAgo);
		return Line;
	}

	// 중국 = 완료 공장 1 + 진행 중 공장 1 + 가득 찬 희토류 + 덜 찬 철광석 + 아직 0개인 구리.
	// 광산 속도를 낮게 두어 수령 뒤 다시 채광해도 검사 도중 정수 1개가 쌓이지 않게 한다.
	// 진행도의 원천이 시작 시각이라 세이브 입구로 시각을 직접 심는다 ― 덜 찬 광산은 공개 API 로 만들 수 없다.
	void SeedPendingWork(const FFixture& Fixture)
	{
		FCountryFactoryData ChinaFactory;
		ChinaFactory.ActiveLines.Add(MakeFactoryLine(1, 0.01f, 5, kOverseasLongAgoSec));
		ChinaFactory.ActiveLines.Add(MakeFactoryLine(2, 0.0001f, 1000, 0.0));
		ChinaFactory.NextLineId = 3;

		TMap<ECountryType, FCountryFactoryData> FactoryData;
		FactoryData.Add(ECountryType::China, ChinaFactory);
		Fixture.Factories->LoadFromSave(FactoryData);

		FCountryMineData ChinaMine;
		ChinaMine.ActiveLines.Add(MakeMineLine(EResourceType::RareEarth, 0.01f, 10, kOverseasLongAgoSec));
		ChinaMine.ActiveLines.Add(MakeMineLine(EResourceType::IronOre, 0.001f, 1000, 5500.0));
		ChinaMine.ActiveLines.Add(MakeMineLine(EResourceType::Copper, 0.001f, 1000, 0.0));

		FCountryMineData AustraliaMine;
		AustraliaMine.ActiveLines.Add(MakeMineLine(EResourceType::IronOre, 0.01f, 10, kOverseasLongAgoSec));
		AustraliaMine.UpgradeLevels.Add(EMineUpgradeType::AutoClaim, 1);

		TMap<ECountryType, FCountryMineData> MineData;
		MineData.Add(ECountryType::China, ChinaMine);
		MineData.Add(ECountryType::Australia, AustraliaMine);
		Fixture.Mines->LoadFromSave(MineData);
	}

	struct FSeededStates
	{
		FWorldFactoryLineState Done;
		FWorldFactoryLineState Running;
		FMineLineState Full;
		FMineLineState Partial;
		FMineLineState Empty;
		FMineLineState AutoFull;
	};

	bool ReadSeededStates(FAutomationTestBase& Test, const FFixture& Fixture, FSeededStates& Out)
	{
		const bool bRead = Fixture.Factories->GetLineState(ECountryType::China, 1, Out.Done)
			&& Fixture.Factories->GetLineState(ECountryType::China, 2, Out.Running)
			&& Fixture.Mines->GetLineState(ECountryType::China, EResourceType::RareEarth, Out.Full)
			&& Fixture.Mines->GetLineState(ECountryType::China, EResourceType::IronOre, Out.Partial)
			&& Fixture.Mines->GetLineState(ECountryType::China, EResourceType::Copper, Out.Empty)
			&& Fixture.Mines->GetLineState(ECountryType::Australia, EResourceType::IronOre, Out.AutoFull);
		if (!Test.TestTrue(TEXT("심은 라인 6개를 모두 읽는다"), bRead))
		{
			return false;
		}

		bool bOk = Test.TestTrue(TEXT("공장 1번 라인 = 완료"), Out.Done.bCompleted && Out.Done.CurrentQty > 0);
		bOk = Test.TestFalse(TEXT("공장 2번 라인 = 진행 중"), Out.Running.bCompleted) && bOk;
		bOk = Test.TestTrue(TEXT("중국 희토류 = 가득 참"), Out.Full.bSaturated && Out.Full.CurrentQty > 0) && bOk;
		bOk = Test.TestTrue(TEXT("중국 철광석 = 덜 찼지만 1개 이상"), !Out.Partial.bSaturated && Out.Partial.CurrentQty >= 1) && bOk;
		bOk = Test.TestEqual(TEXT("중국 구리 = 아직 0개"), Out.Empty.CurrentQty, int64(0)) && bOk;
		bOk = Test.TestTrue(TEXT("호주 철광석 = 가득 참"), Out.AutoFull.bSaturated && Out.AutoFull.CurrentQty > 0) && bOk;
		bOk = Test.TestTrue(TEXT("호주 = 자동 수령"), Fixture.Mines->IsAutoClaimEnabled(ECountryType::Australia)) && bOk;
		bOk = Test.TestFalse(TEXT("중국 = 자동 수령 아님"), Fixture.Mines->IsAutoClaimEnabled(ECountryType::China)) && bOk;
		return bOk;
	}

	void EmptyWarehouse(const FFixture& Fixture)
	{
		for (const FIntPoint& Key : Fixture.Warehouse->GetAllProductKeys())
		{
			Fixture.Warehouse->ConsumeProduct(Key, Fixture.Warehouse->GetProductAmount(Key), /*bShouldSave=*/false);
		}
	}

	void SeedGoods(const FFixture& Fixture)
	{
		EmptyWarehouse(Fixture);
		Fixture.Warehouse->AddProduct(MakeProductKey(ECompanyType::Electronics, 5), 120, EQualityGrade::B, /*bShouldSave=*/false);
		Fixture.Warehouse->AddProduct(MakeProductKey(ECompanyType::Automobile, 3), 40, EQualityGrade::C, /*bShouldSave=*/false);
	}

	TMap<FIntPoint, int64> SnapshotStock(const FFixture& Fixture)
	{
		TMap<FIntPoint, int64> Stock;
		for (const FIntPoint& Key : Fixture.Warehouse->GetAllProductKeys())
		{
			Stock.Add(Key, Fixture.Warehouse->GetProductAmount(Key));
		}
		return Stock;
	}

	int64 SumPreviewPerProduct(const FFixture& Fixture, ECountryType Port)
	{
		int64 Money = 0;
		for (const FIntPoint& Key : Fixture.Warehouse->GetAllProductKeys())
		{
			Money += Fixture.Trade->PreviewSell(Port, static_cast<ECompanyType>(Key.X), Key.Y,
				Fixture.Warehouse->GetProductAmount(Key)).MoneyGained;
		}
		return Money;
	}

	TArray<FCountryInfoTable> CollectHubs(const FFixture& Fixture, int64& OutMaxRequired)
	{
		TArray<FCountryInfoTable> Hubs;
		OutMaxRequired = 0;
		for (const FCountryInfoTable& Row : Fixture.Tables->GetAllCountryInfos())
		{
			OutMaxRequired = FMath::Max(OutMaxRequired, Row.RequiredMarketCap);
			if (Row.bIsTradeHub)
			{
				Hubs.Add(Row);
			}
		}
		return Hubs;
	}

	// 기대값은 구현과 따로 센다. 시설 없는 무역항의 잠금 = 필요 시가총액 미달(허브 규칙·시설 규칙 둘 다 같은 결론).
	void PickExpectedPort(const FFixture& Fixture, const TArray<FCountryInfoTable>& Hubs, int64 MarketCap,
		ECountryType Excluded, ECountryType& OutPort, int64& OutMoney)
	{
		OutPort = ECountryType::None;
		OutMoney = 0;
		for (const FCountryInfoTable& Hub : Hubs)
		{
			if (Hub.CountryType == Excluded || Hub.RequiredMarketCap > MarketCap)
			{
				continue;
			}
			const int64 Money = SumPreviewPerProduct(Fixture, Hub.CountryType);
			if (Money > OutMoney || (Money > 0 && Money == OutMoney && Hub.CountryType < OutPort))
			{
				OutPort = Hub.CountryType;
				OutMoney = Money;
			}
		}
	}

	bool RunCountPending(FAutomationTestBase& Test)
	{
		if (!IsIsolatedRun(Test)) return true;

		FFixture Fixture;
		if (!Test.TestTrue(TEXT("게임 인스턴스와 해외 매니저가 준비됐다"), Fixture.IsReady())) return false;
		SeedPendingWork(Fixture);
		FSeededStates States;
		if (!ReadSeededStates(Test, Fixture, States)) return false;

		const UGameInstance* GI = Fixture.Instance;
		Test.TestEqual(TEXT("중국 = 완료 공장 1 + 수량이 있는 광산 2 (0개 라인 제외)"),
			OverseasActions::CountPending(GI, ECountryType::China), 3);
		Test.TestEqual(TEXT("자동 수령 나라의 가득 찬 광산은 대기가 아니다"),
			OverseasActions::CountPending(GI, ECountryType::Australia), 0);
		Test.TestEqual(TEXT("시설이 없는 나라 = 0"), OverseasActions::CountPending(GI, ECountryType::Korea), 0);
		Test.TestEqual(TEXT("전체 = 나라별 합"), OverseasActions::CountAllPending(GI), 3);
		Test.TestEqual(TEXT("세는 것만으로는 라인이 줄지 않는다"),
			Fixture.Factories->GetActiveLineCount(ECountryType::China) + Fixture.Mines->GetActiveLineCount(ECountryType::China), 5);
		return true;
	}

	bool RunClaimAllZeroesPending(FAutomationTestBase& Test)
	{
		if (!IsIsolatedRun(Test)) return true;

		// 나라 하나 받기 ― 덜 찬 광산까지 받고, 다른 나라는 건드리지 않는다.
		{
			FFixture Fixture;
			if (!Test.TestTrue(TEXT("[나라] 게임 인스턴스와 해외 매니저가 준비됐다"), Fixture.IsReady())) return false;
			SeedPendingWork(Fixture);
			FSeededStates States;
			if (!ReadSeededStates(Test, Fixture, States)) return false;

			UGameInstance* GI = Fixture.Instance;
			const int64 IronBefore = Fixture.Resources->GetResourceAmount(EResourceType::IronOre);
			const int64 CopperBefore = Fixture.Resources->GetResourceAmount(EResourceType::Copper);
			const int64 Expected = States.Done.CurrentQty + States.Full.CurrentQty + States.Partial.CurrentQty;
			Test.TestEqual(TEXT("[나라] 반환 = 완료 공장 + 1개 이상 쌓인 광산 총량"),
				OverseasActions::ClaimCountry(GI, ECountryType::China), Expected);
			Test.TestEqual(TEXT("[나라] 받은 나라의 대기 = 0"), OverseasActions::CountPending(GI, ECountryType::China), 0);
			Test.TestEqual(TEXT("[나라] 덜 찬 광산의 생산 정수량만 지급"),
				Fixture.Resources->GetResourceAmount(EResourceType::IronOre) - IronBefore, States.Partial.CurrentQty);
			Test.TestEqual(TEXT("[나라] 0개 광산은 지급하지 않는다"),
				Fixture.Resources->GetResourceAmount(EResourceType::Copper) - CopperBefore, int64(0));

			FMineLineState Untouched;
			Test.TestTrue(TEXT("[나라] 다른 나라 광산은 그대로 가득 차 있다"),
				Fixture.Mines->GetLineState(ECountryType::Australia, EResourceType::IronOre, Untouched) && Untouched.bSaturated);
		}

		// 모두 받기 ― 완료 공장 + 정수 수량이 쌓인 광산, 받은 즉시 대기 0.
		{
			FFixture Fixture;
			if (!Test.TestTrue(TEXT("[모두] 게임 인스턴스와 해외 매니저가 준비됐다"), Fixture.IsReady())) return false;
			SeedPendingWork(Fixture);
			FSeededStates States;
			if (!ReadSeededStates(Test, Fixture, States)) return false;

			UGameInstance* GI = Fixture.Instance;
			const int64 IronBefore = Fixture.Resources->GetResourceAmount(EResourceType::IronOre);
			const int64 CopperBefore = Fixture.Resources->GetResourceAmount(EResourceType::Copper);
			const int64 Expected = States.Done.CurrentQty + States.Full.CurrentQty + States.Partial.CurrentQty + States.AutoFull.CurrentQty;
			Test.TestEqual(TEXT("[모두] 반환 = 완료 공장 + 부분 생산 광산 총량(자동 수령 나라 포함)"),
				OverseasActions::ClaimAll(GI), Expected);
			Test.TestEqual(TEXT("[모두] 철광석 지급 = 덜 찬 중국 + 가득 찬 호주"),
				Fixture.Resources->GetResourceAmount(EResourceType::IronOre) - IronBefore,
				States.Partial.CurrentQty + States.AutoFull.CurrentQty);
			Test.TestEqual(TEXT("[모두] 0개 구리는 지급하지 않는다"),
				Fixture.Resources->GetResourceAmount(EResourceType::Copper) - CopperBefore, int64(0));
			Test.TestEqual(TEXT("[모두] 받은 즉시 전체 대기 = 0"), OverseasActions::CountAllPending(GI), 0);
			Test.TestEqual(TEXT("[모두] 중국 대기 = 0"), OverseasActions::CountPending(GI, ECountryType::China), 0);

			FMineLineState PartialAfter;
			Test.TestTrue(TEXT("[모두] 덜 찬 광산도 수령 후 다시 채광한다"),
				Fixture.Mines->GetLineState(ECountryType::China, EResourceType::IronOre, PartialAfter)
				&& PartialAfter.CurrentQty == 0);
			Test.TestTrue(TEXT("[모두] 부분 수령은 다음 자원의 소수 진행도를 버리지 않는다"),
				FMath::IsNearlyEqual(PartialAfter.PendingFraction, States.Partial.PendingFraction, 0.01f));
			Test.TestEqual(TEXT("[모두] 곧바로 다시 누르면 받을 것이 없다"), OverseasActions::ClaimAll(GI), static_cast<int64>(0));
		}
		return true;
	}

	bool RunFactoryClaimsReportQualityBonus(FAutomationTestBase& Test)
	{
		if (!IsIsolatedRun(Test)) return true;
		for (int32 Mode = 0; Mode < 2; ++Mode)
		{
			FFixture Fixture;
			if (!Test.TestTrue(TEXT("Factory claim fixture is ready"), Fixture.IsReady())) return false;
			FWorldFactoryUpgradeDefinition QualityDef;
			if (!Test.TestTrue(TEXT("Quality upgrade definition is loaded"),
				Fixture.Tables->GetWorldFactoryUpgradeDefinition(EWorldFactoryUpgradeType::QualityControl, QualityDef))) return false;
			constexpr int64 BaseQty = 1000;
			const int64 BonusQty = static_cast<int64>(FMath::FloorToDouble(
				static_cast<double>(BaseQty) * static_cast<double>(QualityDef.PerLevel) + 1e-6));
			if (!Test.TestTrue(TEXT("Quality bonus is visible for the fixture quantity"), BonusQty > 0)) return false;

			FCountryFactoryData CountryFactory;
			CountryFactory.ActiveLines.Add(MakeFactoryLine(1, 1.0f, BaseQty, kOverseasLongAgoSec));
			CountryFactory.UpgradeLevels.Add(EWorldFactoryUpgradeType::QualityControl, 1);
			CountryFactory.NextLineId = 2;
			TMap<ECountryType, FCountryFactoryData> FactoryData;
			FactoryData.Add(ECountryType::China, CountryFactory);
			Fixture.Factories->LoadFromSave(FactoryData);
			Fixture.Mines->LoadFromSave(TMap<ECountryType, FCountryMineData>());
			const FIntPoint ProductKey(static_cast<int32>(ECompanyType::Electronics), 3);
			const int64 BeforeStock = Fixture.Warehouse->GetProductAmount(ProductKey);
			const int64 Claimed = Mode == 0
				? OverseasActions::ClaimCountryFactory(Fixture.Instance, ECountryType::China)
				: OverseasActions::ClaimAll(Fixture.Instance);
			Test.TestEqual(TEXT("Claim result includes the quality bonus"), Claimed, BaseQty + BonusQty);
			Test.TestEqual(TEXT("Claim result matches goods added to warehouse"),
				Fixture.Warehouse->GetProductAmount(ProductKey) - BeforeStock, Claimed);
			Test.TestEqual(TEXT("The consumed batch cannot be claimed again"),
				OverseasActions::ClaimCountryFactory(Fixture.Instance, ECountryType::China), int64(0));
		}
		return true;
	}

	bool RunBestPortPicksHighest(FAutomationTestBase& Test)
	{
		if (!IsIsolatedRun(Test)) return true;

		FFixture Fixture;
		if (!Test.TestTrue(TEXT("게임 인스턴스와 해외 매니저가 준비됐다"), Fixture.IsReady())) return false;

		int64 MaxRequired = 0;
		const TArray<FCountryInfoTable> Hubs = CollectHubs(Fixture, MaxRequired);
		if (!Test.TestTrue(TEXT("DT_CountryInfo 에 무역항이 둘 이상 있다"), Hubs.Num() >= 2)) return false;

		Fixture.SetMarketCap(MaxRequired);
		SeedGoods(Fixture);
		const UGameInstance* GI = Fixture.Instance;

		int64 LowestMoney = MAX_int64;
		for (const FCountryInfoTable& Hub : Hubs)
		{
			const FString HubName = UEnum::GetValueAsString(Hub.CountryType);
			Test.TestTrue(*FString::Printf(TEXT("%s 무역항이 열려 있다"), *HubName),
				OverseasActions::IsCountryUnlocked(GI, Hub.CountryType));
			const int64 Money = SumPreviewPerProduct(Fixture, Hub.CountryType);
			Test.TestEqual(*FString::Printf(TEXT("%s PreviewSellAll = 제품별 PreviewSell 합"), *HubName),
				OverseasActions::PreviewSellAll(GI, Hub.CountryType), Money);
			LowestMoney = FMath::Min(LowestMoney, Money);
		}

		ECountryType ExpectedPort = ECountryType::None;
		int64 ExpectedMoney = 0;
		PickExpectedPort(Fixture, Hubs, MaxRequired, ECountryType::None, ExpectedPort, ExpectedMoney);
		Test.TestTrue(TEXT("무역항끼리 값이 갈려야 최고값 선택을 가릴 수 있다"), ExpectedMoney > LowestMoney);

		const TMap<FIntPoint, int64> StockBefore = SnapshotStock(Fixture);
		ECountryType Port = ECountryType::None;
		int64 Money = 0;
		Test.TestTrue(TEXT("창고에 물건이 있고 무역항이 열려 있으면 찾는다"), OverseasActions::FindBestSalePort(GI, Port, Money));
		Test.TestEqual(TEXT("가장 비싸게 사는 무역항"), static_cast<int32>(Port), static_cast<int32>(ExpectedPort));
		Test.TestEqual(TEXT("그 무역항의 예상 금액"), Money, ExpectedMoney);
		Test.TestTrue(TEXT("미리보기는 창고를 바꾸지 않는다"), SnapshotStock(Fixture).OrderIndependentCompareEqual(StockBefore));
		return true;
	}

	bool RunBestPortSkipsLocked(FAutomationTestBase& Test)
	{
		if (!IsIsolatedRun(Test)) return true;

		FFixture Fixture;
		if (!Test.TestTrue(TEXT("게임 인스턴스와 해외 매니저가 준비됐다"), Fixture.IsReady())) return false;

		int64 MaxRequired = 0;
		const TArray<FCountryInfoTable> Hubs = CollectHubs(Fixture, MaxRequired);
		if (!Test.TestTrue(TEXT("DT_CountryInfo 에 무역항이 있다"), Hubs.Num() >= 1)) return false;

		const UGameInstance* GI = Fixture.Instance;
		for (const FCountryInfoTable& Hub : Hubs)
		{
			Test.TestEqual(*FString::Printf(TEXT("%s 에 시설이 없다(있으면 시가총액과 무관하게 진출)"),
				*UEnum::GetValueAsString(Hub.CountryType)), OverseasActions::CountFacilities(GI, Hub.CountryType), 0);
		}

		Fixture.SetMarketCap(MaxRequired);
		SeedGoods(Fixture);

		ECountryType TopPort = ECountryType::None;
		int64 TopMoney = 0;
		PickExpectedPort(Fixture, Hubs, MaxRequired, ECountryType::None, TopPort, TopMoney);
		const FCountryInfoTable* TopRow = Hubs.FindByPredicate(
			[TopPort](const FCountryInfoTable& Row) { return Row.CountryType == TopPort; });
		if (!Test.TestNotNull(TEXT("모두 열렸을 때 1등 무역항이 있다"), TopRow)) return false;
		if (!Test.TestTrue(TEXT("1등 무역항은 시가총액으로 잠글 수 있다(필요 시가총액 > 0)"), TopRow->RequiredMarketCap > 0)) return false;

		// 1등만 잠기는 시가총액
		const int64 JustBelowTop = TopRow->RequiredMarketCap - 1;
		Fixture.SetMarketCap(JustBelowTop);
		Test.TestFalse(TEXT("1등 무역항이 잠겼다"), OverseasActions::IsCountryUnlocked(GI, TopPort));

		ECountryType ExpectedPort = ECountryType::None;
		int64 ExpectedMoney = 0;
		PickExpectedPort(Fixture, Hubs, JustBelowTop, TopPort, ExpectedPort, ExpectedMoney);
		Test.TestTrue(TEXT("1등이 잠겨도 고를 무역항이 남는다(DT 필요 시가총액 순서)"), ExpectedPort != ECountryType::None);

		ECountryType Port = ECountryType::None;
		int64 Money = 0;
		const bool bFound = OverseasActions::FindBestSalePort(GI, Port, Money);
		Test.TestTrue(TEXT("열린 무역항이 있을 때만 찾는다"), bFound == (ExpectedPort != ECountryType::None));
		Test.TestTrue(TEXT("잠긴 1등은 고르지 않는다"), Port != TopPort);
		Test.TestEqual(TEXT("열린 무역항 중 최고"), static_cast<int32>(Port), static_cast<int32>(ExpectedPort));
		Test.TestEqual(TEXT("그 무역항의 예상 금액"), Money, ExpectedMoney);

		// 무역항이 전부 잠긴 시가총액
		Fixture.SetMarketCap(0);
		const bool bAnyHubOpenAtZero = Hubs.ContainsByPredicate(
			[](const FCountryInfoTable& Row) { return Row.RequiredMarketCap <= 0; });
		if (!bAnyHubOpenAtZero)
		{
			ECountryType LockedPort = ECountryType::Korea;
			int64 LockedMoney = 1;
			Test.TestFalse(TEXT("무역항이 전부 잠기면 찾지 못한다"), OverseasActions::FindBestSalePort(GI, LockedPort, LockedMoney));
			Test.TestEqual(TEXT("못 찾으면 나라 = None"), static_cast<int32>(LockedPort), static_cast<int32>(ECountryType::None));
			Test.TestEqual(TEXT("못 찾으면 금액 = 0"), LockedMoney, static_cast<int64>(0));
		}
		return true;
	}

	bool RunEmptyWarehouseNoPort(FAutomationTestBase& Test)
	{
		if (!IsIsolatedRun(Test)) return true;

		FFixture Fixture;
		if (!Test.TestTrue(TEXT("게임 인스턴스와 해외 매니저가 준비됐다"), Fixture.IsReady())) return false;

		int64 MaxRequired = 0;
		const TArray<FCountryInfoTable> Hubs = CollectHubs(Fixture, MaxRequired);
		if (!Test.TestTrue(TEXT("DT_CountryInfo 에 무역항이 있다"), Hubs.Num() >= 1)) return false;

		Fixture.SetMarketCap(MaxRequired);
		EmptyWarehouse(Fixture);
		if (!Test.TestEqual(TEXT("창고가 비었다"), Fixture.Warehouse->GetAllProductKeys().Num(), 0)) return false;

		UGameInstance* GI = Fixture.Instance;
		for (const FCountryInfoTable& Hub : Hubs)
		{
			const FString HubName = UEnum::GetValueAsString(Hub.CountryType);
			Test.TestTrue(*FString::Printf(TEXT("%s 무역항이 열려 있다"), *HubName),
				OverseasActions::IsCountryUnlocked(GI, Hub.CountryType));
			Test.TestEqual(*FString::Printf(TEXT("%s 예상 금액 = 0"), *HubName),
				OverseasActions::PreviewSellAll(GI, Hub.CountryType), static_cast<int64>(0));
		}

		ECountryType Port = ECountryType::Korea;
		int64 Money = 1;
		Test.TestFalse(TEXT("창고가 비면 판매할 무역항이 없다"), OverseasActions::FindBestSalePort(GI, Port, Money));
		Test.TestEqual(TEXT("못 찾으면 나라 = None"), static_cast<int32>(Port), static_cast<int32>(ECountryType::None));
		Test.TestEqual(TEXT("못 찾으면 금액 = 0"), Money, static_cast<int64>(0));
		Test.TestEqual(TEXT("빈 창고를 팔면 0"), OverseasActions::SellAllAt(GI, Hubs[0].CountryType), static_cast<int64>(0));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasActionsCountPendingTest, "CGR.Overseas.Actions.CountPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROverseasActionsCountPendingTest::RunTest(const FString& Parameters)
{
	return OverseasActionsTests::RunCountPending(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasActionsClaimAllZeroesPendingTest, "CGR.Overseas.Actions.ClaimAllZeroesPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROverseasActionsClaimAllZeroesPendingTest::RunTest(const FString& Parameters)
{
	return OverseasActionsTests::RunClaimAllZeroesPending(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasActionsFactoryClaimsReportQualityBonusTest,
	"CGR.Overseas.Actions.FactoryClaimsReportQualityBonus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROverseasActionsFactoryClaimsReportQualityBonusTest::RunTest(const FString& Parameters)
{
	return OverseasActionsTests::RunFactoryClaimsReportQualityBonus(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasActionsBestPortPicksHighestTest, "CGR.Overseas.Actions.BestPortPicksHighest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROverseasActionsBestPortPicksHighestTest::RunTest(const FString& Parameters)
{
	return OverseasActionsTests::RunBestPortPicksHighest(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasActionsBestPortSkipsLockedTest, "CGR.Overseas.Actions.BestPortSkipsLocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROverseasActionsBestPortSkipsLockedTest::RunTest(const FString& Parameters)
{
	return OverseasActionsTests::RunBestPortSkipsLocked(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROverseasActionsEmptyWarehouseNoPortTest, "CGR.Overseas.Actions.EmptyWarehouseNoPort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGROverseasActionsEmptyWarehouseNoPortTest::RunTest(const FString& Parameters)
{
	return OverseasActionsTests::RunEmptyWarehouseNoPort(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRTradeRecipeSalePriceTest, "CGR.Overseas.Trade.RecipeSalePrice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRTradeRecipeSalePriceTest::RunTest(const FString& Parameters)
{
	if (!OverseasActionsTests::IsIsolatedRun(*this)) return true;
	OverseasActionsTests::FFixture Fixture;
	if (!TestTrue(TEXT("Trade fixture is ready"), Fixture.IsReady())) return false;

	constexpr ECompanyType Industry = ECompanyType::Electronics;
	constexpr int32 ProductIndex = 1;
	bool bRecipeFound = false;
	const FProductRecipeTable Recipe = Fixture.Tables->GetProductRecipe(Industry, ProductIndex, bRecipeFound);
	if (!TestTrue(TEXT("Electronics first recipe is loaded"), bRecipeFound)) return false;
	if (!TestTrue(TEXT("Recipe has a tuned sale price"), Recipe.BaseSalePrice > 0)) return false;
	TestEqual(TEXT("Trade reads the recipe's sale price"), Fixture.Trade->GetBasePrice(Industry, ProductIndex), Recipe.BaseSalePrice);
	TestTrue(TEXT("Electronics price exceeds the unchanged automobile first recipe"),
		Recipe.BaseSalePrice > Fixture.Trade->GetBasePrice(ECompanyType::Automobile, ProductIndex));

	const FIntPoint ProductKey = MakeProductKey(Industry, ProductIndex);
	Fixture.Warehouse->AddProduct(ProductKey, 1, EQualityGrade::A, false);
	TestEqual(TEXT("Warehouse valuation reads the same recipe sale price"), Fixture.Warehouse->GetTotalWarehouseValue(),
		FMath::RoundToInt64(static_cast<double>(Recipe.BaseSalePrice) * GetQualityGradeRevenueMultiplier(EQualityGrade::A)));
	const FSellResult Preview = Fixture.Trade->PreviewSell(ECountryType::USA, Industry, ProductIndex, 1);
	const int64 BeforeMoney = Fixture.Resources->GetResourceAmount(EResourceType::Money);
	const FSellResult Actual = Fixture.Trade->SellProduct(ECountryType::USA, Industry, ProductIndex, 1);
	TestTrue(TEXT("Valid recipe sells for Money"), Preview.MoneyGained > 0);
	TestEqual(TEXT("Preview matches actual sale"), Actual.MoneyGained, Preview.MoneyGained);
	TestEqual(TEXT("Actual sale credits the previewed Money"),
		Fixture.Resources->GetResourceAmount(EResourceType::Money) - BeforeMoney, Preview.MoneyGained);
	TestEqual(TEXT("Actual sale consumes the product"), Fixture.Warehouse->GetProductAmount(ProductKey), int64(0));

	constexpr int32 MissingIndex = 101;
	const FIntPoint MissingKey = MakeProductKey(Industry, MissingIndex);
	Fixture.Warehouse->AddProduct(MissingKey, 1, EQualityGrade::A, false);
	TestEqual(TEXT("Missing recipe has no sale price"), Fixture.Trade->GetBasePrice(Industry, MissingIndex), int64(0));
	TestEqual(TEXT("Missing recipe cannot be previewed"),
		Fixture.Trade->PreviewSell(ECountryType::USA, Industry, MissingIndex, 1).QuantitySold, int64(0));
	const int64 MoneyBeforeFailure = Fixture.Resources->GetResourceAmount(EResourceType::Money);
	TestEqual(TEXT("Missing recipe cannot be sold"),
		Fixture.Trade->SellProduct(ECountryType::USA, Industry, MissingIndex, 1).QuantitySold, int64(0));
	TestEqual(TEXT("Failed sale preserves stock"), Fixture.Warehouse->GetProductAmount(MissingKey), int64(1));
	TestEqual(TEXT("Failed sale preserves Money"), Fixture.Resources->GetResourceAmount(EResourceType::Money), MoneyBeforeFailure);
	return true;
}

#endif
