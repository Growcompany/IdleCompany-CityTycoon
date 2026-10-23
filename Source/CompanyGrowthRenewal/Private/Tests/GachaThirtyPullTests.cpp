#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/CloudEconomyData.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Table/BuildingTraitTable.h"
#include "UObject/UnrealType.h"
#include "Utils/GachaBatchMath.h"

struct FGachaThirtyPullTestAccessor
{
	// 서브시스템 컬렉션이 없는 GameInstance 라 GetSubsystem 이 비어 있다 ― 캐시 포인터를 직접 꽂는다
	static void Wire(UBuildingTraitManagerSubsystem& Trait, UTableManagerSubsystem* Tables, UItemInventoryManager* Items)
	{
		Trait.TableManager = Tables;
		Trait.ItemInventoryManager = Items;
	}

	static void FinishAccepted(UBuildingTraitManagerSubsystem& Trait, bool bAdvanced, int32 Rolls,
		UBuildingTraitManagerSubsystem::FOnTraitPullDone Done)
	{
		Trait.FinishServerPull(bAdvanced, /*bOk=*/true, Rolls, /*bReplayed=*/false,
			UBuildingTraitManagerSubsystem::GachaActionKey(bAdvanced), MoveTemp(Done));
	}
};

namespace GachaThirtyPullTestUtils
{
	// 등급마다 1행 ― 롤이 어느 등급을 골라도 후보가 있어 결과 수 = 굴린 수가 된다(실제 DT 내용과 무관한 검사)
	bool SeedTraitPool(FAutomationTestBase& Test, UTableManagerSubsystem* Tables)
	{
		const FMapProperty* PoolProperty = FindFProperty<FMapProperty>(UTableManagerSubsystem::StaticClass(), TEXT("BuildingTraitTable"));
		if (!Test.TestNotNull(TEXT("TableManager 의 BuildingTraitTable 캐시"), PoolProperty))
		{
			return false;
		}
		TMap<FName, FBuildingTraitTableRow>* Pool = PoolProperty->ContainerPtrToValuePtr<TMap<FName, FBuildingTraitTableRow>>(Tables);
		const ELootBoxRarity Rarities[] = {
			ELootBoxRarity::Common, ELootBoxRarity::Unusual, ELootBoxRarity::Rare, ELootBoxRarity::Epic, ELootBoxRarity::Legendary };
		for (const ELootBoxRarity Rarity : Rarities)
		{
			FBuildingTraitTableRow Row;
			Row.TraitID = FName(*FString::Printf(TEXT("ThirtyPullFixture_%d"), static_cast<int32>(Rarity)));
			Row.Rarity = Rarity;
			Pool->Add(Row.TraitID, Row);
		}
		return true;
	}

	bool ParseStateJson(const FString& Json, FServerEconomyState& Out, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("테스트 JSON 파싱 실패");
			return false;
		}
		return FCloudEconomyData::ParseState(Root, Out, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRGachaTraitThirtyAllowedTest, "CGR.Gacha.Trait.ThirtyAllowed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaTraitThirtyAllowedTest::RunTest(const FString& Parameters)
{
	using UTrait = UBuildingTraitManagerSubsystem;
	using USkin = UBuildingSkinManagerSubsystem;

	TestTrue(TEXT("1회 허용"), GachaBatchMath::IsAllowedPullCount(1));
	TestTrue(TEXT("10회 허용"), GachaBatchMath::IsAllowedPullCount(10));
	TestTrue(TEXT("30회 허용"), GachaBatchMath::IsAllowedPullCount(30));
	for (const int32 Count : { -1, 0, 2, 5, 9, 11, 20, 29, 31, 100 })
	{
		TestFalse(*FString::Printf(TEXT("%d회는 거절"), Count), GachaBatchMath::IsAllowedPullCount(Count));
	}

	// 서버 상한 = 허용 수의 최댓값 ― 작으면 30회가 400, 크면 클라가 보내지 않는 수까지 서버가 받는다
	int32 LargestAllowed = 0;
	for (int32 Count = 1; Count <= 1000; ++Count)
	{
		if (GachaBatchMath::IsAllowedPullCount(Count))
		{
			LargestAllowed = Count;
		}
	}
	TestEqual(TEXT("허용 최댓값"), LargestAllowed, 30);
	TestEqual(TEXT("특성 MaxPullsPerOp = 허용 최댓값"), UTrait::MaxPullsPerOp, LargestAllowed);
	TestEqual(TEXT("스킨 MaxPullsPerOp = 허용 최댓값"), USkin::MaxPullsPerOp, LargestAllowed);

	// 요청 경로: 허용 수는 관문을 지나 다음 검사(클라우드 없음)에서 끝나고, 그 밖의 수는 관문에서 Warning 과 함께 끝난다
	const TArray<int32> AllowedCounts = { 1, 10, 30 };
	const TArray<int32> RejectedCounts = { 0, 20, 31 };
	AddExpectedMessagePlain(TEXT("특성 뽑기 실패 - CloudEconomySubsystem 없음"), ELogVerbosity::Error,
		EAutomationExpectedMessageFlags::Contains, AllowedCounts.Num());
	AddExpectedMessagePlain(TEXT("스킨 뽑기 실패 - CloudEconomySubsystem 없음"), ELogVerbosity::Error,
		EAutomationExpectedMessageFlags::Contains, AllowedCounts.Num());
	AddExpectedMessagePlain(TEXT("특성 뽑기 거절"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, RejectedCounts.Num());
	AddExpectedMessagePlain(TEXT("스킨 뽑기 거절"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, RejectedCounts.Num());

	UGameInstance* TestInstance = NewObject<UGameInstance>();
	UTrait* Trait = NewObject<UTrait>(TestInstance);
	USkin* Skin = NewObject<USkin>(TestInstance);

	int32 DoneCalls = 0;
	int32 DoneSucceeded = 0;
	int32 DoneWithResults = 0;
	auto Record = [&DoneCalls, &DoneSucceeded, &DoneWithResults](bool bOk, int32 ResultNum)
	{
		++DoneCalls;
		DoneSucceeded += bOk ? 1 : 0;
		DoneWithResults += ResultNum > 0 ? 1 : 0;
	};

	auto RequestBoth = [&](int32 Count)
	{
		Trait->RequestGachaPull(false, Count, UTrait::FOnTraitPullDone::CreateLambda(
			[&Record](bool bOk, const TArray<FBuildingTraitGachaResult>& Results) { Record(bOk, Results.Num()); }));
		Skin->RequestGachaPull(false, Count, USkin::FOnSkinPullDone::CreateLambda(
			[&Record](bool bOk, const TArray<FBuildingSkinGachaResult>& Results) { Record(bOk, Results.Num()); }));
	};
	for (const int32 Count : AllowedCounts) { RequestBoth(Count); }
	for (const int32 Count : RejectedCounts) { RequestBoth(Count); }

	const int32 Expected = 2 * (AllowedCounts.Num() + RejectedCounts.Num());
	TestEqual(TEXT("요청마다 Done 정확히 1회"), DoneCalls, Expected);
	TestEqual(TEXT("클라우드 없는 환경이라 성공 0"), DoneSucceeded, 0);
	TestEqual(TEXT("실패 경로는 결과를 싣지 않는다"), DoneWithResults, 0);

	return true;
}

// 서버가 30 중 일부만 처리해도 화면·보유량은 실제 처리 수를 따른다 ― 요청 수 30 으로 굴리면 받지 않은 특성이 생긴다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRGachaTraitPartialAcceptTest, "CGR.Gacha.Trait.PartialAccept",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaTraitPartialAcceptTest::RunTest(const FString& Parameters)
{
	using UTrait = UBuildingTraitManagerSubsystem;
	constexpr int32 RequestedPulls = 30;
	constexpr int32 AcceptedPulls = 20;
	const EItemType TicketType = UTrait::GetTicketType(false);

	UGameInstance* TestInstance = NewObject<UGameInstance>();
	UTableManagerSubsystem* Tables = NewObject<UTableManagerSubsystem>(TestInstance);
	UItemInventoryManager* Items = NewObject<UItemInventoryManager>(TestInstance);
	UTrait* Trait = NewObject<UTrait>(TestInstance);
	if (!GachaThirtyPullTestUtils::SeedTraitPool(*this, Tables))
	{
		return false;
	}
	FGachaThirtyPullTestAccessor::Wire(*Trait, Tables, Items);
	Items->AddItem(TicketType, RequestedPulls, /*bShouldSave=*/false);

	// HandleOpResponse 순서 그대로 ― 응답 state(남은 10장)를 먼저 적용하고, 콜백이 result.amount 로 굴릴 수를 정한다
	FServerEconomyState State;
	FString Error;
	if (!TestTrue(TEXT("부분 처리 응답 state 파싱"), GachaThirtyPullTestUtils::ParseStateJson(
		TEXT("{\"inventory\":{\"BuildingTraitTicketNormal\":10},\"dayKey\":\"2026-10-17\",\"weekKey\":\"2026-10-12\"}"),
		State, Error)))
	{
		AddError(Error);
		return false;
	}
	FCloudEconomyApplyTargets Targets;
	Targets.Items = Items;
	TestTrue(TEXT("응답 state 적용"), UCloudEconomySubsystem::ApplyServerEconomyStateTo(State, Targets, Error));

	const int32 Rolls = UTrait::ResolveRollCount(RequestedPulls, AcceptedPulls);
	TestEqual(TEXT("굴릴 수 = 서버 처리 수"), Rolls, AcceptedPulls);

	bool bDoneCalled = false;
	bool bDoneOk = false;
	int32 ResultCount = INDEX_NONE;
	FGachaThirtyPullTestAccessor::FinishAccepted(*Trait, false, Rolls, UTrait::FOnTraitPullDone::CreateLambda(
		[&bDoneCalled, &bDoneOk, &ResultCount](bool bOk, const TArray<FBuildingTraitGachaResult>& Results)
	{
		bDoneCalled = true;
		bDoneOk = bOk;
		ResultCount = Results.Num();
	}));

	TestTrue(TEXT("Done 호출"), bDoneCalled);
	TestTrue(TEXT("성공으로 끝남"), bDoneOk);
	TestEqual(TEXT("결과 수 = 처리 수(요청 30 아님)"), ResultCount, AcceptedPulls);
	TestEqual(TEXT("티켓은 처리 수만큼만 줄었다(30 → 10, 결과 단계가 더 깎거나 돌려주지 않음)"),
		Items->GetItemCount(TicketType), RequestedPulls - AcceptedPulls);
	TestEqual(TEXT("누적 뽑기 수 = 처리 수"), Trait->GetInventory().GachaData.TotalPullsAllTime, AcceptedPulls);
	TestEqual(TEXT("마일리지 = 1회당 1"), Trait->GetMileagePoints(), AcceptedPulls);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
