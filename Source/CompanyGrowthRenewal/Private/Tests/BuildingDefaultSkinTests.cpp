#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Entity/Building/BuildingDefaultSkin.h"
#include "Table/BuildingData.h"
#include "Table/BuildingSkinData.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildingDefaultSkinTests
{
UDataTable* LoadLiveBuildingTable()
{
	const UObject* BuildingCDO = ABuildingBaseActor::StaticClass()->GetDefaultObject();
	const FObjectPropertyBase* TableProperty =
		FindFProperty<FObjectPropertyBase>(ABuildingBaseActor::StaticClass(), TEXT("BuildingDataTable"));
	return TableProperty ? Cast<UDataTable>(TableProperty->GetObjectPropertyValue_InContainer(BuildingCDO)) : nullptr;
}

UDataTable* LoadLiveSkinTable()
{
	return TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Table/Building/DT_BuildingSkin.DT_BuildingSkin"))).LoadSynchronous();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingDefaultSkinBandTest,
	"CGR.Building.DefaultSkin.FootprintBand",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingDefaultSkinBandTest::RunTest(const FString& Parameters)
{
	using namespace CGBuildingDefaultSkin;

	auto Band = [](int32 W, int32 D) { return static_cast<int32>(BandForFootprint(W, D)); };
	const int32 Common = static_cast<int32>(ELootBoxRarity::Common);
	const int32 Unusual = static_cast<int32>(ELootBoxRarity::Unusual);
	const int32 Rare = static_cast<int32>(ELootBoxRarity::Rare);
	const int32 Epic = static_cast<int32>(ELootBoxRarity::Epic);

	TestEqual(TEXT("1x1 = Common"), Band(1, 1), Common);
	TestEqual(TEXT("2x1 = Unusual"), Band(2, 1), Unusual);
	TestEqual(TEXT("1x2 는 2x1 과 같은 넓이라 같은 밴드"), Band(1, 2), Unusual);
	TestEqual(TEXT("2x2 = Rare"), Band(2, 2), Rare);
	TestEqual(TEXT("3x3 = Epic"), Band(3, 3), Epic);

	// DT 가 0 칸을 들고 있어도 밴드는 나와야 한다 — 호출부가 스킨 없는 건물을 만들지 않도록
	TestEqual(TEXT("0 칸은 최소 밴드로 떨어진다"), Band(0, 0), Common);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingDefaultSkinFixedTest,
	"CGR.Building.DefaultSkin.FixedPerBuildingType",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingDefaultSkinFixedTest::RunTest(const FString& Parameters)
{
	using namespace CGBuildingDefaultSkin;

	const int32 FirstCall = FindDefaultSkinID(TEXT("B1"));
	const int32 SecondCall = FindDefaultSkinID(TEXT("B1"));
	TestNotEqual(TEXT("B1 에 기본 스킨이 배정돼 있다"), FirstCall, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("같은 건물 종류는 호출할 때마다 같은 스킨 — 랜덤이 아니다"), SecondCall, FirstCall);

	TestNotEqual(TEXT("같은 밴드의 다음 건물은 다른 스킨을 받는다"),
		FindDefaultSkinID(TEXT("B2")), FindDefaultSkinID(TEXT("B1")));

	TestEqual(TEXT("표에 없는 행은 INDEX_NONE — 호출부가 조용한 회색 대신 경고를 띄우도록"),
		FindDefaultSkinID(TEXT("NotABuildingRow")), static_cast<int32>(INDEX_NONE));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingDefaultSkinLowRarityTest,
	"CGR.Building.DefaultSkin.LowRarityPoolsFullyUsed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingDefaultSkinLowRarityTest::RunTest(const FString& Parameters)
{
	TSet<int32> Used;
	for (const TPair<FName, int32>& Entry : CGBuildingDefaultSkin::GetTable())
	{
		Used.Add(Entry.Value);
	}

	// 이 작업의 목적 자체 — 저등급 스킨이 한 장도 안 놀고 전부 어딘가에 쓰이고 있어야 한다
	for (int32 SkinID = 100; SkinID <= 104; ++SkinID)
	{
		TestTrue(FString::Printf(TEXT("Common %d 이 쓰인다"), SkinID), Used.Contains(SkinID));
	}
	for (int32 SkinID = 200; SkinID <= 207; ++SkinID)
	{
		TestTrue(FString::Printf(TEXT("Unusual %d 이 쓰인다"), SkinID), Used.Contains(SkinID));
	}
	for (int32 SkinID = 300; SkinID <= 307; ++SkinID)
	{
		TestTrue(FString::Printf(TEXT("Rare %d 이 쓰인다"), SkinID), Used.Contains(SkinID));
	}

	// Legendary/Mythic 은 스킨 가챠의 상품 — 기본으로 깔리면 뽑을 이유가 없어진다
	for (const int32 SkinID : Used)
	{
		TestTrue(FString::Printf(TEXT("%d 은 Legendary/Mythic 이 아니다"), SkinID), SkinID < 500);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRBuildingDefaultSkinLiveTableTest,
	"CGR.Building.DefaultSkin.MatchesLiveBuildingTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCGRBuildingDefaultSkinLiveTableTest::RunTest(const FString& Parameters)
{
	UDataTable* BuildingTable = BuildingDefaultSkinTests::LoadLiveBuildingTable();
	UDataTable* SkinTable = BuildingDefaultSkinTests::LoadLiveSkinTable();
	if (!TestNotNull(TEXT("live BuildingDataTable"), BuildingTable)) { return false; }
	if (!TestNotNull(TEXT("live DT_BuildingSkin"), SkinTable)) { return false; }

	TMap<int32, ELootBoxRarity> RarityBySkinID;
	for (const FName SkinRowName : SkinTable->GetRowNames())
	{
		if (const FBuildingSkinData* SkinRow =
			SkinTable->FindRow<FBuildingSkinData>(SkinRowName, TEXT("DefaultSkinLiveTableTest")))
		{
			RarityBySkinID.Add(SkinRow->SkinID, SkinRow->Rarity);
		}
	}

	const TArray<FName> BuildingRowNames = BuildingTable->GetRowNames();
	TestTrue(TEXT("라이브 건물 행이 비어 있지 않다"), BuildingRowNames.Num() > 0);

	for (const FName RowName : BuildingRowNames)
	{
		const FBuildingData* Row =
			BuildingTable->FindRow<FBuildingData>(RowName, TEXT("DefaultSkinLiveTableTest"));
		if (!TestNotNull(*RowName.ToString(), Row)) { continue; }

		const int32 SkinID = CGBuildingDefaultSkin::FindDefaultSkinID(RowName);
		if (!TestNotEqual(FString::Printf(TEXT("%s 에 기본 스킨이 배정돼 있다"), *RowName.ToString()),
			SkinID, static_cast<int32>(INDEX_NONE)))
		{
			continue;
		}

		const ELootBoxRarity* ActualRarity = RarityBySkinID.Find(SkinID);
		if (!TestNotNull(FString::Printf(TEXT("%s 의 스킨 %d 이 DT_BuildingSkin 에 있다"),
			*RowName.ToString(), SkinID), ActualRarity))
		{
			continue;
		}

		const ELootBoxRarity ExpectedBand =
			CGBuildingDefaultSkin::BandForFootprint(Row->FootprintWidthCells, Row->FootprintDepthCells);
		TestEqual(FString::Printf(TEXT("%s (%dx%d) 의 스킨 %d 등급이 발자국 밴드와 맞는다"),
			*RowName.ToString(), Row->FootprintWidthCells, Row->FootprintDepthCells, SkinID),
			static_cast<int32>(*ActualRarity), static_cast<int32>(ExpectedBand));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
