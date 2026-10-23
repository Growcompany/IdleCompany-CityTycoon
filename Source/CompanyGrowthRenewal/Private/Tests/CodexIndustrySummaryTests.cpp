#include "Misc/AutomationTest.h"
#include "Data/CodexIndustrySummary.h"
#include "Data/GameSaveData.h"
#include "Data/ShippedRecord.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexIndustrySummaryTest,
	"CGR.Codex.IndustrySummary.Aggregation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	void AddOwnedBuilding(FGameSaveData& Save, int32 BuildingIndex, ECompanyType Industry, int32 Tier)
	{
		FBuildingEntitySaveData& Owned = Save.Buildings.AddDefaulted_GetRef();
		Owned.BuildingIndex = BuildingIndex;
		Owned.BuildingData.CompanyType = Industry;
		Save.OfficeDataMap.FindOrAdd(BuildingIndex).TierProgress.CurrentTier = Tier;
	}

	// OfficeDataMap 항목은 오피스 진입 시점에 지연 생성된다 — 소유 직후의 상태를 재현한다.
	void AddBuildingWithoutOffice(FGameSaveData& Save, int32 BuildingIndex, ECompanyType Industry)
	{
		FBuildingEntitySaveData& Owned = Save.Buildings.AddDefaulted_GetRef();
		Owned.BuildingIndex = BuildingIndex;
		Owned.BuildingData.CompanyType = Industry;
	}

	FShippedProjectRecord& AddShipped(FGameSaveData& Save, ECompanyType Industry, const TCHAR* Name, int64 Revenue, int32 Review)
	{
		FShippedProjectRecord& Rec = Save.ShippedProjects.AddDefaulted_GetRef();
		Rec.Industry = Industry;
		Rec.ProjectName = Name;
		Rec.CumulativeRevenue = Revenue;
		Rec.ReviewScore = Review;
		return Rec;
	}
}

bool FCodexIndustrySummaryTest::RunTest(const FString& Parameters)
{
	// 표시 순서 권위 = ECompanyType 선언 순서:
	//   [0]Game [1]Electronics [2]Finance [3]IT [4]Semiconductor [5]Automobile
	FGameSaveData Save;
	// 게임 2채(3티어·5티어) + IT 1채. 나머지 4종은 미진출.
	AddOwnedBuilding(Save, 1, ECompanyType::Game, 3);
	AddOwnedBuilding(Save, 2, ECompanyType::Game, 5);
	AddOwnedBuilding(Save, 3, ECompanyType::IT, 1);
	// 자동차 1채 — 건물은 있는데 오피스 레코드가 아직 없는 상태
	AddBuildingWithoutOffice(Save, 4, ECompanyType::Automobile);
	Save.OfficeDataMap.FindOrAdd(1).TierProgress.ClearedProjects = { 21, 22 };
	Save.OfficeDataMap.FindOrAdd(2).TierProgress.ClearedProjects = { 22, 41 };
	// 소유 건물과 연결되지 않은 고아 오피스는 집계에 들어오면 안 된다.
	Save.OfficeDataMap.FindOrAdd(99).TierProgress.CurrentTier = 9;

	AddShipped(Save, ECompanyType::Game, TEXT("워존"), 120000000, 31);
	AddShipped(Save, ECompanyType::Game, TEXT("던전크롤러"), 39000000, 28);
	AddShipped(Save, ECompanyType::Game, TEXT("섀도우 에이전트"), 64000000, 30);
	AddShipped(Save, ECompanyType::Game, TEXT("동률 낮은평점"), 39000000, 12);
	AddShipped(Save, ECompanyType::IT, TEXT("사내 메신저"), 31000000, 22);
	// 매출·평점이 완전히 같은 2건 — 3차 키가 없으면 비안정 정렬이 순서를 실행마다 바꾼다
	AddShipped(Save, ECompanyType::Finance, TEXT("동률 뒤"), 5000000, 20).ProjectIndex = 52;
	AddShipped(Save, ECompanyType::Finance, TEXT("동률 앞"), 5000000, 20).ProjectIndex = 51;

	TArray<FCodexIndustrySummary> Rows;
	CodexSummary::BuildAllIndustrySummaries(Save, Rows);

	if (!TestEqual(TEXT("미진출 포함 6산업이 항상 나온다"), Rows.Num(), 6)) return false;
	// 순서가 선언 순서로 고정돼야 회사를 인수해도 레일이 튀지 않는다
	TestEqual(TEXT("[0] 게임"), Rows[0].Industry, ECompanyType::Game);
	TestEqual(TEXT("[1] 전자"), Rows[1].Industry, ECompanyType::Electronics);
	TestEqual(TEXT("[2] 금융"), Rows[2].Industry, ECompanyType::Finance);
	TestEqual(TEXT("[3] IT"), Rows[3].Industry, ECompanyType::IT);
	TestEqual(TEXT("[4] 반도체"), Rows[4].Industry, ECompanyType::Semiconductor);
	TestEqual(TEXT("[5] 자동차"), Rows[5].Industry, ECompanyType::Automobile);

	TestTrue(TEXT("게임은 보유"), Rows[0].bOwned);
	TestEqual(TEXT("같은 산업 2채면 티어는 최댓값"), Rows[0].Tier, 5);
	TestEqual(TEXT("같은 산업 수익은 합산 (1.2억+3900만+6400만+3900만)"), Rows[0].CumulativeRevenue, (int64)262000000);
	TestEqual(TEXT("출시작 수는 그 산업만 센다"), Rows[0].ShippedCount, 4);

	TestTrue(TEXT("IT 는 보유"), Rows[3].bOwned);
	TestEqual(TEXT("IT 티어"), Rows[3].Tier, 1);
	TestEqual(TEXT("IT 수익"), Rows[3].CumulativeRevenue, (int64)31000000);

	// 소유 권위는 Buildings 다. 지연 생성되는 OfficeDataMap 을 기다리면 갓 세운 회사가 잠금 줄이 된다
	TestTrue(TEXT("오피스 레코드가 없어도 건물이 있으면 보유"), Rows[5].bOwned);
	TestEqual(TEXT("오피스 레코드 없는 신생 회사는 1티어"), Rows[5].Tier, 1);

	TestFalse(TEXT("전자는 미진출"), Rows[1].bOwned);
	TestEqual(TEXT("미진출 티어는 0 — 고아 오피스의 9를 주워오지 않는다"), Rows[1].Tier, 0);
	TestEqual(TEXT("미진출 수익 0"), Rows[1].CumulativeRevenue, (int64)0);
	TestEqual(TEXT("미진출 출시작 0"), Rows[1].ShippedCount, 0);

	TArray<FShippedProjectRecord> Top;
	CodexSummary::BuildTopShipped(Save, ECompanyType::Game, 3, Top);
	if (!TestEqual(TEXT("대표작은 요청 개수로 잘린다"), Top.Num(), 3)) return false;
	TestEqual(TEXT("1위는 최고 매출"), Top[0].ProjectName, FString(TEXT("워존")));
	TestEqual(TEXT("2위"), Top[1].ProjectName, FString(TEXT("섀도우 에이전트")));
	TestEqual(TEXT("매출 동률은 평점이 높은 쪽"), Top[2].ProjectName, FString(TEXT("던전크롤러")));

	TArray<FShippedProjectRecord> Ties;
	CodexSummary::BuildTopShipped(Save, ECompanyType::Finance, 3, Ties);
	if (!TestEqual(TEXT("완전 동률 2건이 모두 나온다"), Ties.Num(), 2)) return false;
	TestEqual(TEXT("매출·평점까지 같으면 ProjectIndex 오름차순"), Ties[0].ProjectIndex, 51);
	TestEqual(TEXT("완전 동률 2위"), Ties[1].ProjectIndex, 52);

	TArray<FShippedProjectRecord> Empty;
	CodexSummary::BuildTopShipped(Save, ECompanyType::Automobile, 3, Empty);
	TestEqual(TEXT("기록 없는 산업은 빈 목록"), Empty.Num(), 0);

	TArray<int32> Cleared;
	CodexSummary::BuildClearedProjects(Save, ECompanyType::Game, Cleared);
	if (!TestEqual(TEXT("두 건물의 클리어가 합집합으로 합쳐진다"), Cleared.Num(), 3)) return false;
	TestTrue(TEXT("21 포함"), Cleared.Contains(21));
	TestTrue(TEXT("중복 22는 한 번만"), Cleared.Contains(22));
	TestTrue(TEXT("41 포함"), Cleared.Contains(41));

	FGameSaveData Fresh;
	TArray<FCodexIndustrySummary> FreshRows;
	CodexSummary::BuildAllIndustrySummaries(Fresh, FreshRows);
	if (!TestEqual(TEXT("빈 세이브도 6행"), FreshRows.Num(), 6)) return false;
	TestFalse(TEXT("빈 세이브는 전부 미진출"), FreshRows[0].bOwned);
	return true;
}
#endif
