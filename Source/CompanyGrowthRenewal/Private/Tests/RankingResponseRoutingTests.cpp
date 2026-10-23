#include "Misc/AutomationTest.h"
#include "Manager/RankingManagerSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRankingResponseRoutingTest,
	"CGR.Ranking.ResponseRouting.ReversedTabsAndDelayedProfile",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRankingResponseRoutingTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<URankingManagerSubsystem> Manager(NewObject<URankingManagerSubsystem>(GameInstance.Get()));
	TArray<int32> DeliveredTabs;
	Manager->OnLeaderboardLoaded.AddLambda([&DeliveredTabs](int32 TabIndex, const TArray<FRankingEntry>& Entries)
	{
		DeliveredTabs.Add(TabIndex);
	});

	FPlayFabLeaderboardEntry MarketEntry;
	MarketEntry.EntityId = TEXT("same-player");
	MarketEntry.DisplayName = TEXT("Company");
	MarketEntry.Rank = 2;
	MarketEntry.Score = 9000;
	FPlayFabLeaderboardEntry WeeklyEntry = MarketEntry;
	WeeklyEntry.Rank = 1;
	WeeklyEntry.Score = 700;

	// 두 탭 요청 후 마지막 요청과 반대 순서로 도착하는 응답을 주입한다.
	Manager->LastRequestedTabIndex = 1;
	Manager->HandleLeaderboardResult(TEXT("WeeklyRevenue"), {WeeklyEntry});
	Manager->HandleLeaderboardResult(TEXT("MarketCap"), {MarketEntry});
	TestEqual(TEXT("Both responses delivered"), DeliveredTabs.Num(), 2);
	if (DeliveredTabs.Num() == 2)
	{
		TestEqual(TEXT("Weekly response goes to weekly tab"), DeliveredTabs[0], 1);
		TestEqual(TEXT("Late market response goes to market tab"), DeliveredTabs[1], 0);
	}
	TestEqual(TEXT("Last requested cache remains weekly"), Manager->GetCachedLeaderboard()[0].TotalRevenue, int64(700));
	TestEqual(TEXT("Weekly score is not an HQ level"), Manager->GetCachedLeaderboard()[0].HQLevel, 1);

	TMap<FString, FString> Profile;
	Profile.Add(TEXT("RankingProfile"), TEXT("{\"hq\":8,\"pi\":3}"));
	DeliveredTabs.Reset();
	Manager->HandleOtherPlayerData(TEXT("same-player"), Profile);
	TestEqual(TEXT("Shared profile updates both tabs"), DeliveredTabs.Num(), 2);
	TestTrue(TEXT("Profile refresh includes market tab"), DeliveredTabs.Contains(0));
	TestTrue(TEXT("Profile refresh includes weekly tab"), DeliveredTabs.Contains(1));
	for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
	{
		Manager->LastRequestedTabIndex = TabIndex;
		const FRankingEntry& Entry = Manager->GetCachedLeaderboard()[0];
		TestEqual(TEXT("Profile HQ applied"), Entry.HQLevel, 8);
		TestEqual(TEXT("Profile image applied"), Entry.ProfileImageID, 3);
		TestEqual(TEXT("Per-tab score preserved"), Entry.TotalRevenue, TabIndex == 0 ? int64(9000) : int64(700));
		TestEqual(TEXT("Per-tab rank preserved"), Entry.Rank, TabIndex == 0 ? 2 : 1);
	}

	// 한 탭이 새 목록으로 교체돼도 늦은 프로필은 남아 있는 탭에만 전달한다.
	Manager->HandleLeaderboardResult(TEXT("MarketCap"), {});
	DeliveredTabs.Reset();
	Manager->HandleOtherPlayerData(TEXT("same-player"), Profile);
	TestEqual(TEXT("Removed player does not repopulate market cache"), Manager->CachedLeaderboards[0].Num(), 0);
	TestEqual(TEXT("Only matching tab receives late profile"), DeliveredTabs.Num(), 1);
	if (DeliveredTabs.Num() == 1)
	{
		TestEqual(TEXT("Late profile remains weekly"), DeliveredTabs[0], 1);
	}
	DeliveredTabs.Reset();
	Manager->HandleOtherPlayerData(TEXT("unknown-player"), Profile);
	TestTrue(TEXT("Unrelated profile does not refresh either tab"), DeliveredTabs.IsEmpty());
	Manager->PendingVisitPlayFabId = TEXT("same-player");
	Manager->PendingVisitDisplayName = TEXT("Company");
	Manager->CancelPendingVisit();
	TestTrue(TEXT("Closing panel clears pending visit ID"), Manager->PendingVisitPlayFabId.IsEmpty());
	TestTrue(TEXT("Closing panel clears pending visit name"), Manager->PendingVisitDisplayName.IsEmpty());
	int32 PreviewResponses = 0;
	Manager->OnCitySnapshotLoaded.AddLambda([&PreviewResponses](const FString&, const FCitySnapshot&) { ++PreviewResponses; });
	TMap<FString, FString> SnapshotData;
	SnapshotData.Add(TEXT("CitySnapshot"), TEXT("{}"));
	Manager->HandleOtherPlayerData(TEXT("same-player"), SnapshotData);
	TestEqual(TEXT("Canceled visit still delivers preview snapshot"), PreviewResponses, 1);
	TestTrue(TEXT("Late preview cannot recreate visit intent"), Manager->PendingVisitPlayFabId.IsEmpty());
	Manager->PendingVisitPlayFabId = TEXT("same-player");
	Manager->PendingVisitDisplayName = TEXT("Company");
	AddExpectedError(TEXT("City visit timed out"), EAutomationExpectedErrorFlags::Contains, 1);
	Manager->HandleVisitTimeout();
	TestTrue(TEXT("Timeout clears visit intent"), Manager->PendingVisitPlayFabId.IsEmpty());
	Manager->OnCitySnapshotLoaded.Clear();
	Manager->OnLeaderboardLoaded.Clear();
	return true;
}

#endif
