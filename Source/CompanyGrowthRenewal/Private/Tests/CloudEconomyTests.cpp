#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Data/CloudAuthRetryState.h"
#include "Data/CloudEconomyData.h"
#include "Manager/AttendanceSubsystem.h"
#include "Manager/BuildingSkinManagerSubsystem.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/TradePort.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ShopManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "Enum/OfficeExpansionType.h"
#include "Table/CloudErrorTextTable.h"
#include "Utils/CloudHttp.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace CloudEconomyTestUtils
{
	/** 매니저가 붙은 전용 GameInstance. 서브시스템 컬렉션 없이 `GetOuter()` 만으로 동작하는 상태 세터만 쓴다 */
	struct FTargets
	{
		UGameInstance* Instance = nullptr;
		FCloudEconomyApplyTargets Targets;

		FTargets()
		{
			Instance = NewObject<UGameInstance>();
			Targets.Resources = NewObject<UResourceItemManager>(Instance);
			Targets.Items = NewObject<UItemInventoryManager>(Instance);
			Targets.Shop = NewObject<UShopManagerSubsystem>(Instance);
			Targets.Attendance = NewObject<UAttendanceSubsystem>(Instance);
			Targets.Recruitment = NewObject<URecruitmentManagerSubsystem>(Instance);
			// Save 는 null ― 디스크 저장은 이 테스트의 관심이 아니고 실제 세이브를 건드리면 안 된다
		}
	};

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

	FString FullStateJson(int64 Diamond, int32 Mileage, const FString& InventoryBody)
	{
		return FString::Printf(TEXT("{")
			TEXT("\"wallet\":{\"diamond\":%lld,\"mileage\":%d},")
			TEXT("\"inventory\":{%s},")
			TEXT("\"shop\":{\"counts\":{\"Diamond_Pack_S\":2},\"dailyKey\":\"2026-09-24\",\"weeklyKey\":\"2026-09-21\"},")
			TEXT("\"attendance\":{\"cycleDay\":5,\"lastClaimDayKey\":\"2026-09-24\",\"totalDays\":33},")
			TEXT("\"ranking\":{\"marketCapStr\":\"9007199254740993\",\"weeklyRevenue\":1200,\"weeklyKey\":\"2026-09-21\"},")
			TEXT("\"dayKey\":\"2026-09-24\",\"weekKey\":\"2026-09-21\"}"),
			Diamond, Mileage, *InventoryBody);
	}
}

// ============================================================================
// Task 1 ― int64 십진 문자열 (설계서 §6.0 · §8.3) + Review Focus #2
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudInt64Test, "CGR.Cloud.Int64String",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudInt64Test::RunTest(const FString& Parameters)
{
	int64 Out = -1;

	// 왕복 (§8.3)
	const int64 RoundTripValues[] = { 0, 1, -1, 12345678901234LL, MAX_int64, MIN_int64 };
	for (const int64 Value : RoundTripValues)
	{
		const FString Encoded = FCloudEconomyData::Int64ToString(Value);
		TestTrue(*FString::Printf(TEXT("왕복 파싱 '%s'"), *Encoded), FCloudEconomyData::ParseInt64(Encoded, Out));
		TestEqual(*FString::Printf(TEXT("왕복 값 '%s'"), *Encoded), Out, Value);
	}

	TestEqual(TEXT("MAX_int64 문자열"), FCloudEconomyData::Int64ToString(MAX_int64), FString(TEXT("9223372036854775807")));
	TestEqual(TEXT("MIN_int64 문자열"), FCloudEconomyData::Int64ToString(MIN_int64), FString(TEXT("-9223372036854775808")));

	// Review Focus #2 ― 실패를 0 으로 삼키면 시총이 조용히 리셋된 것처럼 보인다
	const TCHAR* const BadInputs[] = {
		TEXT(""), TEXT(" "), TEXT("  "), TEXT("abc"), TEXT("1.5"), TEXT("1e9"),
		TEXT("9223372036854775808"), TEXT("-9223372036854775809"), TEXT("0x10"),
		TEXT("1 000"), TEXT("+1"), TEXT("-"), TEXT("12a"), TEXT("1,000"), TEXT(" 1")
	};
	for (const TCHAR* Bad : BadInputs)
	{
		Out = 12345;
		TestFalse(*FString::Printf(TEXT("'%s' 는 실패해야 한다"), Bad), FCloudEconomyData::ParseInt64(Bad, Out));
		TestEqual(*FString::Printf(TEXT("'%s' 실패 시 Out 을 건드리지 않는다"), Bad), Out, (int64)12345);
	}

	return true;
}

// ============================================================================
// Task 2 ― 에러코드 → 안내 문구 (§8.3 `ErrorCodeMap`, 부록 A)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudErrorCodeMapTest, "CGR.Cloud.ErrorCodeMap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudErrorCodeMapTest::RunTest(const FString& Parameters)
{
	const FText Funds = FText::FromString(TEXT("재화가 부족합니다."));
	const FText Default = FText::FromString(TEXT("잠시 후 다시 시도해 주세요."));

	TMap<FName, FText> Texts;
	Texts.Add(CloudErr::InsufficientFunds, Funds);
	Texts.Add(FCloudErrorTextRules::DefaultCode(), Default);

	TestEqual(TEXT("등록 코드는 그 문구"),
		FCloudErrorTextRules::Resolve(Texts, CloudErr::InsufficientFunds).ToString(), Funds.ToString());

	TestEqual(TEXT("미등록 코드는 공통 폴백"),
		FCloudErrorTextRules::Resolve(Texts, FName(TEXT("NO_SUCH_CODE"))).ToString(), Default.ToString());

	TestEqual(TEXT("빈 코드는 공통 폴백"),
		FCloudErrorTextRules::Resolve(Texts, NAME_None).ToString(), Default.ToString());

	// DT 에셋이 없어도(맵이 비어도) 안내가 사라지지 않아야 한다 ― 빈 FText 는 죽은 버튼처럼 보인다
	const TMap<FName, FText> Empty;
	TestFalse(TEXT("DT 부재 시에도 폴백은 비어 있지 않다"),
		FCloudErrorTextRules::Resolve(Empty, CloudErr::InsufficientFunds).IsEmpty());
	TestEqual(TEXT("DT 부재 폴백은 최후 문구"),
		FCloudErrorTextRules::Resolve(Empty, CloudErr::InsufficientFunds).ToString(),
		FCloudErrorTextRules::LastResort().ToString());

	// 빈 문구가 등록된 행은 폴백으로 내려간다(임포트 누락이 빈 안내로 새지 않게)
	TMap<FName, FText> Blank = Texts;
	Blank.Add(CloudErr::Maintenance, FText::GetEmpty());
	TestEqual(TEXT("빈 문구 행은 공통 폴백"),
		FCloudErrorTextRules::Resolve(Blank, CloudErr::Maintenance).ToString(), Default.ToString());

	// 부록 A 전 코드가 문구를 얻는다(등록 or 폴백) + 코드 문자열 규약
	TestEqual(TEXT("부록 A 코드 개수"), CloudErr::AllCodes().Num(), 28);
	TSet<FName> Seen;
	for (const FName Code : CloudErr::AllCodes())
	{
		TestFalse(*FString::Printf(TEXT("'%s' 문구가 비지 않는다"), *Code.ToString()),
			FCloudErrorTextRules::Resolve(Texts, Code).IsEmpty());

		bool bAlreadySeen = false;
		Seen.Add(Code, &bAlreadySeen);
		TestFalse(*FString::Printf(TEXT("'%s' 중복 없음"), *Code.ToString()), bAlreadySeen);

		const FString AsString = Code.ToString();
		TestEqual(*FString::Printf(TEXT("'%s' 는 대문자 코드"), *AsString), AsString, AsString.ToUpper());
	}

	// 부록 A "영수증(영구)" 열 ― 비영구 코드는 조건 해소 후 같은 opId 로 성공해야 한다
	TestTrue(TEXT("BAD_REQUEST 는 영구"), CloudErr::IsPermanentFailure(CloudErr::BadRequest));
	TestTrue(TEXT("ALREADY_CLAIMED 는 영구"), CloudErr::IsPermanentFailure(CloudErr::AlreadyClaimed));
	TestTrue(TEXT("BALANCE_OVERFLOW 는 영구"), CloudErr::IsPermanentFailure(CloudErr::BalanceOverflow));
	TestFalse(TEXT("INSUFFICIENT_FUNDS 는 비영구"), CloudErr::IsPermanentFailure(CloudErr::InsufficientFunds));
	TestFalse(TEXT("INSUFFICIENT_ITEMS 는 비영구"), CloudErr::IsPermanentFailure(CloudErr::InsufficientItems));
	TestFalse(TEXT("FORBIDDEN 은 비영구"), CloudErr::IsPermanentFailure(CloudErr::Forbidden));

	return true;
}

// ============================================================================
// Task 3 ― 응답 JSON → 매니저 상태 적용 (§8.3 `ApplyServerState`) + Review Focus #1·#3
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudApplyServerStateTest, "CGR.Cloud.ApplyServerState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudApplyServerStateTest::RunTest(const FString& Parameters)
{
	using namespace CloudEconomyTestUtils;

	FTargets Fixture;
	FString Error;

	// --- 전 절이 담긴 응답을 적용한다 ---
	FServerEconomyState State;
	TestTrue(TEXT("전체 state 파싱"),
		ParseStateJson(FullStateJson(500, 7, TEXT("\"RecruitTicketNormal\":3")), State, Error));
	TestTrue(TEXT("적용 성공"), UCloudEconomySubsystem::ApplyServerEconomyStateTo(State, Fixture.Targets, Error));

	TestEqual(TEXT("Diamond 주입"), Fixture.Targets.Resources->GetResourceAmount(EResourceType::Diamond), (int64)500);
	TestEqual(TEXT("마일리지 주입"), Fixture.Targets.Recruitment->GetMileagePoints(), 7);
	TestEqual(TEXT("티켓 주입"), Fixture.Targets.Items->GetItemCount(EItemType::RecruitTicketNormal), 3);
	TestEqual(TEXT("상점 카운트 주입"), Fixture.Targets.Shop->GetShopData().PurchaseCounts.FindRef(FName(TEXT("Diamond_Pack_S"))), 2);
	// 서버가 준 키는 현재 유효 키 ― 그대로 넣어야 로컬이 다시 리셋하지 않는다
	TestEqual(TEXT("상점 일일 키"), Fixture.Targets.Shop->GetShopData().LastDailyKey, FString(TEXT("2026-09-24")));
	TestEqual(TEXT("상점 주간 키"), Fixture.Targets.Shop->GetShopData().LastWeeklyKey, FString(TEXT("2026-09-21")));
	TestEqual(TEXT("출석 일차"), Fixture.Targets.Attendance->GetCycleDay(), 5);
	TestEqual(TEXT("출석 누계"), Fixture.Targets.Attendance->GetTotalDays(), 33);
	// 2^53 초과 시총이 십진 문자열로 무손실 도착한다(§6.0)
	TestEqual(TEXT("시총 int64 무손실"), State.Ranking.MarketCap, (int64)9007199254740993LL);

	// --- Review Focus #1: 인벤토리는 델타가 아니라 맵 교체 ---
	Fixture.Targets.Items->AddItem(EItemType::SkinTicketNormal, 5, /*bShouldSave=*/false, /*bShouldBroadcast=*/false);
	TestEqual(TEXT("로컬 스킨권 선반영"), Fixture.Targets.Items->GetItemCount(EItemType::SkinTicketNormal), 5);

	FServerEconomyState WithoutSkin;
	TestTrue(TEXT("스킨권 없는 state 파싱"),
		ParseStateJson(FullStateJson(500, 7, TEXT("\"RecruitTicketNormal\":3")), WithoutSkin, Error));
	TestTrue(TEXT("맵 교체 적용 성공"),
		UCloudEconomySubsystem::ApplyServerEconomyStateTo(WithoutSkin, Fixture.Targets, Error));
	TestEqual(TEXT("응답에 없는 소모형 아이템은 0"), Fixture.Targets.Items->GetItemCount(EItemType::SkinTicketNormal), 0);
	TestEqual(TEXT("응답에 있는 아이템은 유지"), Fixture.Targets.Items->GetItemCount(EItemType::RecruitTicketNormal), 3);

	// --- Review Focus #3: 중간 실패 시 아무것도 적용되지 않는다 ---
	const int64 DiamondBefore = Fixture.Targets.Resources->GetResourceAmount(EResourceType::Diamond);
	const int32 TicketBefore = Fixture.Targets.Items->GetItemCount(EItemType::RecruitTicketNormal);

	FServerEconomyState Broken;
	TestTrue(TEXT("미지 아이템 키 state 는 파싱까지는 된다"),
		ParseStateJson(FullStateJson(999, 9, TEXT("\"NotARealItem\":1")), Broken, Error));
	TestFalse(TEXT("잘못된 상태는 거부"),
		UCloudEconomySubsystem::ApplyServerEconomyStateTo(Broken, Fixture.Targets, Error));
	TestFalse(TEXT("거부 이유가 남는다"), Error.IsEmpty());
	TestEqual(TEXT("반쪽 적용이 없다(Diamond)"), Fixture.Targets.Resources->GetResourceAmount(EResourceType::Diamond), DiamondBefore);
	TestEqual(TEXT("반쪽 적용이 없다(티켓)"), Fixture.Targets.Items->GetItemCount(EItemType::RecruitTicketNormal), TicketBefore);

	// 적용 대상 매니저가 빠져도 앞 절만 적용되고 마는 일이 없어야 한다
	FCloudEconomyApplyTargets Missing = Fixture.Targets;
	Missing.Items = nullptr;
	FServerEconomyState Good;
	TestTrue(TEXT("정상 state 파싱"),
		ParseStateJson(FullStateJson(4242, 11, TEXT("\"RecruitTicketNormal\":1")), Good, Error));
	TestFalse(TEXT("대상 누락은 거부"),
		UCloudEconomySubsystem::ApplyServerEconomyStateTo(Good, Missing, Error));
	TestEqual(TEXT("대상 누락 시 Diamond 미변경"), Fixture.Targets.Resources->GetResourceAmount(EResourceType::Diamond), DiamondBefore);

	// --- 부분 봉투: /wallet/spend 응답은 wallet 만 준다. 없는 절을 "빈 값"으로 적용하면 인벤토리가 날아간다 ---
	FServerEconomyState WalletOnly;
	TestTrue(TEXT("부분 봉투 파싱"),
		ParseStateJson(TEXT("{\"wallet\":{\"diamond\":350,\"mileage\":8},\"dayKey\":\"2026-09-24\",\"weekKey\":\"2026-09-21\"}"),
			WalletOnly, Error));
	TestFalse(TEXT("부분 봉투에 inventory 절이 없다"), WalletOnly.bHasInventory);
	TestTrue(TEXT("부분 봉투 적용 성공"),
		UCloudEconomySubsystem::ApplyServerEconomyStateTo(WalletOnly, Fixture.Targets, Error));
	TestEqual(TEXT("부분 봉투로 지갑만 갱신"), Fixture.Targets.Resources->GetResourceAmount(EResourceType::Diamond), (int64)350);
	TestEqual(TEXT("부분 봉투가 인벤토리를 지우지 않는다"),
		Fixture.Targets.Items->GetItemCount(EItemType::RecruitTicketNormal), TicketBefore);

	// --- Review Focus #2 의 적용 경로: marketCapStr 가 깨졌으면 파싱 자체가 실패해야 한다 ---
	FServerEconomyState BadRanking;
	TestFalse(TEXT("깨진 marketCapStr 는 파싱 실패"),
		ParseStateJson(TEXT("{\"ranking\":{\"marketCapStr\":\"12x3\"},\"dayKey\":\"2026-09-24\"}"), BadRanking, Error));
	TestFalse(TEXT("파싱 실패 이유가 남는다"), Error.IsEmpty());

	// 0 개 키를 스스로 만들지 않는다(서버 계약 미러) ― 로컬이 비면 목표 맵도 서버 것만 남는다
	TMap<FName, int32> ServerInventory;
	ServerInventory.Add(FName(TEXT("RecruitTicketAdvanced")), 2);
	const TMap<EItemType, int32> Target = FCloudEconomyData::BuildInventoryTarget(TMap<EItemType, int32>(), ServerInventory);
	TestEqual(TEXT("빈 로컬 + 서버 1키 = 1키"), Target.Num(), 1);
	TestEqual(TEXT("서버 값 그대로"), Target.FindRef(EItemType::RecruitTicketAdvanced), 2);

	return true;
}

// ============================================================================
// Task 4 ― 경제 잠금 게이트 + pending 플래그 (Review Focus #4)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudEconomyGateTest, "CGR.Cloud.EconomyGate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudEconomyGateTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestInstance = NewObject<UGameInstance>();
	UCloudEconomySubsystem* Economy = NewObject<UCloudEconomySubsystem>(TestInstance);

	TestFalse(TEXT("동기화 전에는 경제가 잠겨 있다"), Economy->IsEconomyReady());

	int32 Calls = 0;
	FCloudOpResult Last;
	UCloudEconomySubsystem::FOnCloudOp Done;
	Done.BindLambda([&Calls, &Last](const FCloudOpResult& Result)
	{
		++Calls;
		Last = Result;
	});

	// 세 요청 모두 **동기적으로** 실패해야 한다 ― HTTP 를 탔다면 콜백이 이 프레임에 오지 않는다
	Economy->RequestSpend(CloudCurrency::Diamond, 30, FName(TEXT("gacha.recruit.advanced")), FCloudMeta(), Done);
	TestEqual(TEXT("spend 는 요청 없이 즉시 실패"), Calls, 1);
	TestFalse(TEXT("spend 결과는 실패"), Last.bOk);
	TestEqual(TEXT("spend 게이트 코드"), Last.Code.ToString(), CloudErr::AuthRequired.ToString());

	Economy->RequestCredit(FName(TEXT("codex.tierMilestone")), TEXT("codex.tier.3"), 20, Done);
	TestEqual(TEXT("credit 는 요청 없이 즉시 실패"), Calls, 2);
	TestEqual(TEXT("credit 게이트 코드"), Last.Code.ToString(), CloudErr::AuthRequired.ToString());

	Economy->RequestConsume(FName(TEXT("recruit.pull")), FName(TEXT("RecruitTicketNormal")), 1, Done);
	TestEqual(TEXT("consume 는 요청 없이 즉시 실패"), Calls, 3);
	TestEqual(TEXT("consume 게이트 코드"), Last.Code.ToString(), CloudErr::AuthRequired.ToString());

	// 게이트 문구는 DT 를 지난다 ― 서버 영문 message 를 화면에 쓰지 않는다
	TMap<FName, FText> Texts;
	Texts.Add(CloudErr::AuthRequired, FText::FromString(TEXT("서버와 다시 연결하고 있습니다.")));
	TestEqual(TEXT("게이트 안내 문구"),
		FCloudErrorTextRules::Resolve(Texts, Last.Code).ToString(), FString(TEXT("서버와 다시 연결하고 있습니다.")));

	// pending 플래그 ― 같은 액션 중복 클릭 차단(§6.3)
	const FName ActionKey(TEXT("gacha.recruit.advanced"));
	TestTrue(TEXT("첫 클릭은 통과"), Economy->BeginPendingAction(ActionKey));
	TestTrue(TEXT("진행 중 표시"), Economy->IsActionPending(ActionKey));
	TestFalse(TEXT("중복 클릭은 차단"), Economy->BeginPendingAction(ActionKey));
	Economy->EndPendingAction(ActionKey);
	TestFalse(TEXT("종료 후 표시 해제"), Economy->IsActionPending(ActionKey));
	TestTrue(TEXT("종료 후 다시 통과"), Economy->BeginPendingAction(ActionKey));
	Economy->EndPendingAction(ActionKey);
	TestFalse(TEXT("빈 키는 거부"), Economy->BeginPendingAction(NAME_None));

	return true;
}

// ============================================================================
// Task 6 (a) ― Shipping https 강제 (보안 리뷰 낮음12)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudBaseUrlPolicyTest, "CGR.Cloud.BaseUrlPolicy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudBaseUrlPolicyTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("https 통과"), FCloudHttp::IsAllowedBaseUrl(TEXT("https://x.run.app"), /*bShipping=*/true));
	TestFalse(TEXT("Shipping http 거부"), FCloudHttp::IsAllowedBaseUrl(TEXT("http://x.run.app"), /*bShipping=*/true));
	TestFalse(TEXT("Shipping 은 로컬 http 도 거부"), FCloudHttp::IsAllowedBaseUrl(TEXT("http://127.0.0.1:5001"), /*bShipping=*/true));

	TestTrue(TEXT("로컬 에뮬레이터 예외"), FCloudHttp::IsAllowedBaseUrl(TEXT("http://127.0.0.1:5001/demo-cgr/us-central1/api"), /*bShipping=*/false));
	TestTrue(TEXT("localhost 예외"), FCloudHttp::IsAllowedBaseUrl(TEXT("http://localhost:5001/demo-cgr"), /*bShipping=*/false));
	TestFalse(TEXT("비Shipping 도 임의 http 호스트는 거부"), FCloudHttp::IsAllowedBaseUrl(TEXT("http://staging.example.com"), /*bShipping=*/false));

	// ini 파서가 '//' 를 주석으로 잘라 "https:" 만 남는 회귀
	TestFalse(TEXT("스킴만 남은 값 거부"), FCloudHttp::IsAllowedBaseUrl(TEXT("https:"), /*bShipping=*/false));
	TestFalse(TEXT("빈 값 거부"), FCloudHttp::IsAllowedBaseUrl(TEXT(""), /*bShipping=*/false));
	// 'http://evil/...' 를 호스트 접두 검사로 통과시키는 우회를 막는다
	TestFalse(TEXT("localhost 를 흉내낸 호스트 거부"), FCloudHttp::IsAllowedBaseUrl(TEXT("http://evil.com/127.0.0.1"), /*bShipping=*/false));

	return true;
}

// ============================================================================
// Task 6 (b) ― 재인증 루프 종단 상태 (보안 리뷰 낮음13)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudAuthRetryBackoffTest, "CGR.Cloud.AuthRetryBackoff",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudAuthRetryBackoffTest::RunTest(const FString& Parameters)
{
	FCloudAuthRetryState State;

	// 성공한 요청 뒤의 첫 401 은 정상 만료다 ― 세지 않는다
	State.OnTokensApplied();
	State.OnRequestAccepted();
	TestFalse(TEXT("정상 만료 401 은 잠그지 않는다"), State.OnRequestRejected());
	TestEqual(TEXT("정상 만료는 카운트 0"), State.PostReauthRejections, 0);

	// 교환 성공이 카운터를 0 으로 만들면 무한 루프다 ― 401 → 교환 → 401 … 을 3바퀴 돌린다
	State.OnTokensApplied();
	TestFalse(TEXT("1회차는 아직 잠그지 않는다"), State.OnRequestRejected());
	TestEqual(TEXT("1회차 카운트"), State.PostReauthRejections, 1);

	State.OnTokensApplied();
	TestEqual(TEXT("교환 성공이 카운터를 리셋하지 않는다"), State.PostReauthRejections, 1);
	TestFalse(TEXT("2회차는 아직 잠그지 않는다"), State.OnRequestRejected());
	TestEqual(TEXT("2회차 카운트"), State.PostReauthRejections, 2);

	State.OnTokensApplied();
	TestTrue(TEXT("3회차에서 백오프 잠금"), State.OnRequestRejected());
	TestTrue(TEXT("잠금 상태"), State.bBackoffLocked);
	TestEqual(TEXT("임계값"), FCloudAuthRetryState::PostReauthRejectionThreshold, 3);

	// 잠긴 뒤에는 같은 거절이 반복돼도 잠금 신호가 두 번 나가지 않는다(안내 중복 방지)
	TestFalse(TEXT("잠금 신호는 1회만"), State.OnRequestRejected());
	TestTrue(TEXT("여전히 잠김"), State.bBackoffLocked);

	// 요청이 실제로 성공하면 리셋된다 ― 여기서만
	State.ClearBackoff();
	TestFalse(TEXT("해제 후 잠금 없음"), State.bBackoffLocked);
	TestEqual(TEXT("해제 후 카운트 0"), State.PostReauthRejections, 0);
	TestFalse(TEXT("해제 후 첫 401 은 세지 않는다"), State.OnRequestRejected());

	return true;
}

// ============================================================================
// 1-B-4a Task 1 ― 채용 뽑기 서버 배선의 순수 판정 (월드 없이 돈다)
// ============================================================================

// 로컬 RNG 횟수는 요청값이 아니라 서버 응답의 result.amount 를 따른다.
// 요청값을 믿으면 서버가 5장만 인정했는데 10명을 만들어 인벤토리와 로스터가 어긋난다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudGachaRollCountTest, "CGR.Cloud.GachaRollCount",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudGachaRollCountTest::RunTest(const FString& Parameters)
{
	using URM = URecruitmentManagerSubsystem;

	TestEqual(TEXT("서버 인정 수량을 따른다"), URM::ResolveRollCount(10, 5), 5);
	// 서버가 요청보다 크게 주는 일은 없어야 하지만, 오면 요청값으로 막는다(신뢰 하한)
	TestEqual(TEXT("서버가 더 줘도 요청값을 넘지 않는다"), URM::ResolveRollCount(5, 10), 5);
	// 응답을 못 읽었으면 아무 효과도 주지 않는다 ― 0 이면 로컬 결과가 생기지 않는다
	TestEqual(TEXT("수량 불명이면 0"), URM::ResolveRollCount(10, 0), 0);
	TestEqual(TEXT("음수 응답은 0"), URM::ResolveRollCount(10, -3), 0);
	TestEqual(TEXT("요청이 0 이면 0"), URM::ResolveRollCount(0, 5), 0);

	return true;
}

// 폴백은 409 에서만. 타임아웃에서 폴백하면 티켓과 다이아를 둘 다 낸다(폴백은 새 opId 라 멱등이 못 막는다).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudGachaFallbackTest, "CGR.Cloud.GachaFallback",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudGachaFallbackTest::RunTest(const FString& Parameters)
{
	using URM = URecruitmentManagerSubsystem;

	// 409 INSUFFICIENT_ITEMS = 영수증 없는 확정 거절 → 폴백 안전
	TestTrue(TEXT("409 면 폴백"), URM::ShouldFallbackToDiamond(409, CloudErr::InsufficientItems, true));
	// 타임아웃·네트워크 오류 = 서버에서 성공했을 수 있다 → 폴백 금지
	TestFalse(TEXT("타임아웃은 폴백 금지"), URM::ShouldFallbackToDiamond(0, CloudErr::Internal, true));
	TestFalse(TEXT("5xx 는 폴백 금지"), URM::ShouldFallbackToDiamond(503, CloudErr::Maintenance, true));
	// 같은 409 라도 다른 코드면 폴백 금지(잔액 부족은 티켓 문제가 아니다)
	TestFalse(TEXT("409 라도 다른 코드면 금지"), URM::ShouldFallbackToDiamond(409, CloudErr::OpIdMismatch, true));
	// 폴백이 허용되지 않는 경우(일반 티어·배치)는 409 여도 폴백 없음
	TestFalse(TEXT("폴백 미허용이면 금지"), URM::ShouldFallbackToDiamond(409, CloudErr::InsufficientItems, false));

	// 배치는 티켓 전용이다(ExecuteGachaPullBatch 가 bAllowDiamondFallback=false 를 넘긴다)
	TestTrue(TEXT("고급 단일은 다이아 허용"), URM::AllowsDiamondFallback(EGachaTier::Advanced, 1));
	TestTrue(TEXT("프리미엄 단일은 다이아 허용"), URM::AllowsDiamondFallback(EGachaTier::Premium, 1));
	TestFalse(TEXT("배치는 다이아 불허"), URM::AllowsDiamondFallback(EGachaTier::Premium, 5));
	TestFalse(TEXT("일반은 다이아 경로 없음"), URM::AllowsDiamondFallback(EGachaTier::Normal, 1));

	// reason 문자열은 spendRules.json 과 바이트 단위로 같아야 한다 ― 다르면 서버가 400
	TestEqual(TEXT("고급 reason"), URM::GachaSpendReason(EGachaTier::Advanced).ToString(),
		FString(TEXT("gacha.recruit.advanced")));
	TestEqual(TEXT("프리미엄 reason"), URM::GachaSpendReason(EGachaTier::Premium).ToString(),
		FString(TEXT("gacha.recruit.premium")));
	TestTrue(TEXT("일반 reason 없음"), URM::GachaSpendReason(EGachaTier::Normal).IsNone());

	return true;
}

// bReplayed=true 면 서버가 영수증을 재생한 것이다 ― RNG 를 다시 돌리면 플레이어가 본 직원이 바뀐다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudGachaReplayTest, "CGR.Cloud.GachaReplay",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudGachaReplayTest::RunTest(const FString& Parameters)
{
	using URM = URecruitmentManagerSubsystem;

	TestTrue(TEXT("리플레이 플래그가 로컬 효과를 막는다"), URM::ShouldSkipLocalEffect(/*bReplayed=*/true));
	TestFalse(TEXT("정상 응답은 로컬 효과 적용"), URM::ShouldSkipLocalEffect(/*bReplayed=*/false));
	// 리플레이가 아니면 인정 수량만큼 돌린다(두 판정은 독립이다)
	TestEqual(TEXT("리플레이 여부와 수량 판정은 별개"), URM::ResolveRollCount(5, 5), 5);

	return true;
}

// 서버 지갑은 콜백 **전에** 적용된다(HandleOpResponse: ApplyStateFromResponse → Done).
// 그래서 서버 권위 경로에서 로컬이 또 적립하면 old+2N 이 되고, 굴림마다 방송하므로 칩이 위로 튀었다가 되돌아온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudGachaLocalMileageTest, "CGR.Cloud.GachaLocalMileage",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudGachaLocalMileageTest::RunTest(const FString& Parameters)
{
	using URM = URecruitmentManagerSubsystem;

	// 서버 권위 경로 = 로컬 적립 금지(서버가 mileagePerItem 으로 이미 줬다)
	TestFalse(TEXT("서버 권위 경로는 로컬 적립 금지"), URM::ShouldGrantLocalMileage(/*bServerAuthoritative=*/true));
	// 동기 경로(로컬 차감) = 로컬이 적립한다. BlueprintCallable 이라 행동 불변이 중요하다
	TestTrue(TEXT("로컬 차감 경로는 로컬 적립"), URM::ShouldGrantLocalMileage(/*bServerAuthoritative=*/false));

	return true;
}

// Review Focus 4 ― BeginPendingAction 이 true 를 준 경로에 EndPendingAction 이 없으면 그 액션이 영구 잠긴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudPendingPairTest, "CGR.Cloud.PendingPair",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudPendingPairTest::RunTest(const FString& Parameters)
{
	using URM = URecruitmentManagerSubsystem;

	UGameInstance* TestInstance = NewObject<UGameInstance>();
	UCloudEconomySubsystem* Cloud = NewObject<UCloudEconomySubsystem>(TestInstance);

	const FName AdvancedKey = URM::GachaActionKey(EGachaTier::Advanced);
	const FName PremiumKey = URM::GachaActionKey(EGachaTier::Premium);

	// 티어별로 따로 잠근다 ― 고급 대기 중에 프리미엄이 막히면 안 된다
	TestNotEqual(TEXT("티어별 키가 다르다"), AdvancedKey, PremiumKey);
	TestFalse(TEXT("키가 비어 있지 않다"), AdvancedKey.IsNone());

	// 성공·실패·폴백 어느 경로든 종료는 FinishServerPull 한 곳이다. 그 종료가 하는 일을 여기서 단언한다.
	TestTrue(TEXT("첫 요청은 pending 을 잡는다"), Cloud->BeginPendingAction(AdvancedKey));
	TestTrue(TEXT("진행 중 표시"), Cloud->IsActionPending(AdvancedKey));

	// 중복 클릭은 차단되고, **pending 을 풀지 않는다**(내가 잡은 게 아니다)
	TestFalse(TEXT("중복 클릭 차단"), Cloud->BeginPendingAction(AdvancedKey));
	TestTrue(TEXT("중복 클릭이 남의 pending 을 풀지 않는다"), Cloud->IsActionPending(AdvancedKey));

	// 다른 티어는 독립적으로 잡힌다
	TestTrue(TEXT("다른 티어는 독립"), Cloud->BeginPendingAction(PremiumKey));
	TestTrue(TEXT("프리미엄도 진행 중"), Cloud->IsActionPending(PremiumKey));

	// 종료 ― 각 키가 독립적으로 풀린다
	Cloud->EndPendingAction(AdvancedKey);
	TestFalse(TEXT("고급 해제"), Cloud->IsActionPending(AdvancedKey));
	TestTrue(TEXT("고급 해제가 프리미엄을 건드리지 않는다"), Cloud->IsActionPending(PremiumKey));

	Cloud->EndPendingAction(PremiumKey);
	TestFalse(TEXT("프리미엄 해제"), Cloud->IsActionPending(PremiumKey));

	// 해제 후 재요청이 통과해야 한다 ― 안 통과하면 버튼이 영구히 죽는다
	TestTrue(TEXT("해제 후 다시 잡힌다"), Cloud->BeginPendingAction(AdvancedKey));
	Cloud->EndPendingAction(AdvancedKey);
	TestFalse(TEXT("최종 상태는 비어 있다"), Cloud->IsActionPending(AdvancedKey));

	return true;
}

// 경로 선택은 로컬 캐시로만 한다 ― None 은 요청을 보내지 않고 즉시 종료(pending 을 반드시 푸는 분기)다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudGachaRouteTest, "CGR.Cloud.GachaRoute",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudGachaRouteTest::RunTest(const FString& Parameters)
{
	using URM = URecruitmentManagerSubsystem;

	// 티켓이 있으면 티켓 우선 ― 다이아가 가능해도 티켓을 먼저 태운다(플레이어 이익)
	TestTrue(TEXT("티켓 보유 → 티켓"),
		URM::ResolvePullRoute(/*bHasTicket=*/true, /*bAllowDiamond=*/true) == EGachaPullRoute::Ticket);
	TestTrue(TEXT("티켓만 가능 → 티켓"),
		URM::ResolvePullRoute(true, false) == EGachaPullRoute::Ticket);
	// 티켓이 없고 다이아가 가능하면 바로 다이아 ― 409 를 굳이 받지 않는다
	TestTrue(TEXT("티켓 없음 + 다이아 가능 → 다이아"),
		URM::ResolvePullRoute(false, true) == EGachaPullRoute::Diamond);
	// 둘 다 안 되면 요청을 보내지 않는다(이 분기에서 pending 을 풀지 않으면 버튼이 영구 잠긴다)
	TestTrue(TEXT("둘 다 불가 → None"),
		URM::ResolvePullRoute(false, false) == EGachaPullRoute::None);

	return true;
}

// ============================================================================
// U2 ― 스킨·특성 뽑기 소모(1 op) + 특성 슬롯 해금 지출
// ============================================================================

// reason/item 문자열은 consumeRules.json·spendRules.json 과 바이트 단위로 같아야 한다 ― 다르면 서버가 400 을 준다.
// 하드코딩 오타는 컴파일러가 못 잡으므로 여기서 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudSkinTraitContractTest, "CGR.Cloud.SkinTraitContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudSkinTraitContractTest::RunTest(const FString& Parameters)
{
	using USkin = UBuildingSkinManagerSubsystem;
	using UTrait = UBuildingTraitManagerSubsystem;

	TestEqual(TEXT("스킨 소모 reason"), USkin::GetPullConsumeReason().ToString(), FString(TEXT("skin.pull")));
	TestEqual(TEXT("특성 소모 reason"), UTrait::GetPullConsumeReason().ToString(), FString(TEXT("trait.pull")));
	// 구 이름 gacha.trait 은 실체(UnlockSlotWithDiamond)와 달라 폐기됐다 ― 되살아나면 서버가 400 을 준다
	TestEqual(TEXT("특성 슬롯해금 reason"), UTrait::GetSlotUnlockSpendReason().ToString(), FString(TEXT("trait.slotUnlock")));

	// items 허용목록 미러 ― EItemType 식별자가 그대로 직렬화된다(표시명이 아니다)
	TestEqual(TEXT("스킨 일반 티켓"),
		FCloudEconomyData::ItemTypeToName(USkin::GetTicketType(false)).ToString(), FString(TEXT("SkinTicketNormal")));
	TestEqual(TEXT("스킨 고급 티켓"),
		FCloudEconomyData::ItemTypeToName(USkin::GetTicketType(true)).ToString(), FString(TEXT("SkinTicketAdvanced")));
	TestEqual(TEXT("특성 일반 티켓"),
		FCloudEconomyData::ItemTypeToName(UTrait::GetTicketType(false)).ToString(), FString(TEXT("BuildingTraitTicketNormal")));
	TestEqual(TEXT("특성 고급 티켓"),
		FCloudEconomyData::ItemTypeToName(UTrait::GetTicketType(true)).ToString(), FString(TEXT("BuildingTraitTicketAdvanced")));

	// 슬롯 해금 1회 최대 청구액이 spendRules 의 opCap(450) 과 같다 ― 비용을 올리면 서버가 전부 거절한다
	int32 MaxSlotCost = 0;
	for (int32 i = 0; i < UTrait::GetMaxSlotCount(); ++i)
	{
		MaxSlotCost = FMath::Max(MaxSlotCost, UTrait::GetDiamondCostForSlot(i));
	}
	TestEqual(TEXT("슬롯 해금 최대 비용 = opCap"), MaxSlotCost, 450);

	return true;
}

// 1·10·30회는 N번 왕복이 아니라 1 op 이다. 그 밖의 수는 줄여 보내지 않고 거절한다(CGR.Gacha.Trait.ThirtyAllowed).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudSkinTraitBatchTest, "CGR.Cloud.SkinTraitBatch",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudSkinTraitBatchTest::RunTest(const FString& Parameters)
{
	using USkin = UBuildingSkinManagerSubsystem;
	using UTrait = UBuildingTraitManagerSubsystem;

	// consumeRules.json 의 maxPerOp 과 같은 수 ― 허용 수 1·10·30 의 최댓값
	TestEqual(TEXT("스킨 maxPerOp"), USkin::MaxPullsPerOp, 30);
	TestEqual(TEXT("특성 maxPerOp"), UTrait::MaxPullsPerOp, 30);

	// 로컬 RNG 횟수는 요청값이 아니라 서버가 인정한 수량이다(응답의 result.amount)
	TestEqual(TEXT("서버 인정 수량을 따른다"), USkin::ResolveRollCount(10, 4), 4);
	TestEqual(TEXT("서버가 더 줘도 요청값이 상한"), UTrait::ResolveRollCount(10, 20), 10);
	TestEqual(TEXT("수량 불명이면 0"), USkin::ResolveRollCount(10, 0), 0);
	TestEqual(TEXT("음수 응답은 0"), UTrait::ResolveRollCount(10, -1), 0);

	// 리플레이 영수증에 RNG 를 다시 돌리면 플레이어가 본 결과가 바뀐다
	TestTrue(TEXT("스킨 리플레이는 로컬 효과 금지"), USkin::ShouldSkipLocalEffect(true));
	TestFalse(TEXT("스킨 정상 응답은 로컬 효과 적용"), USkin::ShouldSkipLocalEffect(false));
	TestTrue(TEXT("특성 리플레이는 로컬 효과 금지"), UTrait::ShouldSkipLocalEffect(true));
	TestFalse(TEXT("특성 정상 응답은 로컬 효과 적용"), UTrait::ShouldSkipLocalEffect(false));

	return true;
}

// pending 키는 카테고리마다 갈라야 한다 ― 스킨 뽑기 대기가 특성·채용 버튼까지 막으면 죽은 UI 로 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudSkinTraitPendingKeyTest, "CGR.Cloud.SkinTraitPendingKey",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudSkinTraitPendingKeyTest::RunTest(const FString& Parameters)
{
	using USkin = UBuildingSkinManagerSubsystem;
	using UTrait = UBuildingTraitManagerSubsystem;
	using URM = URecruitmentManagerSubsystem;

	const FName SkinNormal = USkin::GachaActionKey(false);
	const FName SkinAdvanced = USkin::GachaActionKey(true);
	const FName TraitNormal = UTrait::GachaActionKey(false);
	const FName TraitAdvanced = UTrait::GachaActionKey(true);
	const FName RecruitNormal = URM::GachaActionKey(EGachaTier::Normal);
	const FName SlotUnlock = UTrait::SlotUnlockActionKey(3, 1);

	TArray<FName> Keys = { SkinNormal, SkinAdvanced, TraitNormal, TraitAdvanced, RecruitNormal, SlotUnlock };
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		TestFalse(TEXT("키가 비어 있지 않다"), Keys[i].IsNone());
		for (int32 j = i + 1; j < Keys.Num(); ++j)
		{
			TestNotEqual(TEXT("서로 다른 액션은 서로 다른 키"), Keys[i], Keys[j]);
		}
	}

	// 슬롯 해금은 건물·슬롯별로 갈린다 ― 한 슬롯 대기가 옆 슬롯을 막으면 안 된다
	TestNotEqual(TEXT("슬롯이 다르면 다른 키"), UTrait::SlotUnlockActionKey(3, 1), UTrait::SlotUnlockActionKey(3, 2));
	TestNotEqual(TEXT("건물이 다르면 다른 키"), UTrait::SlotUnlockActionKey(3, 1), UTrait::SlotUnlockActionKey(4, 1));

	// 한 카테고리 요청이 다른 카테고리를 막지 않는다
	UGameInstance* TestInstance = NewObject<UGameInstance>();
	UCloudEconomySubsystem* Cloud = NewObject<UCloudEconomySubsystem>(TestInstance);

	TestTrue(TEXT("스킨 일반 요청"), Cloud->BeginPendingAction(SkinNormal));
	TestFalse(TEXT("같은 배너 중복 클릭은 차단"), Cloud->BeginPendingAction(SkinNormal));
	TestTrue(TEXT("스킨 고급은 독립"), Cloud->BeginPendingAction(SkinAdvanced));
	TestTrue(TEXT("특성은 독립"), Cloud->BeginPendingAction(TraitNormal));
	TestTrue(TEXT("채용은 독립"), Cloud->BeginPendingAction(RecruitNormal));
	TestTrue(TEXT("슬롯 해금은 독립"), Cloud->BeginPendingAction(SlotUnlock));

	// 종료는 각자 풀린다 ― 빠지면 그 버튼이 영구 잠긴다
	for (const FName& Key : Keys)
	{
		Cloud->EndPendingAction(Key);
		TestFalse(TEXT("해제 후 진행 중이 아니다"), Cloud->IsActionPending(Key));
	}
	TestTrue(TEXT("해제 후 재요청 통과"), Cloud->BeginPendingAction(SkinNormal));
	Cloud->EndPendingAction(SkinNormal);

	return true;
}

// ============================================================================
// U3 ― 직원 + 도시 + 오피스 + 무역 (지출 2 · 소모 1 · 지급 3)
// ============================================================================

// reason/source 문자열은 spendRules.json·consumeRules.json·creditRules.json 과 바이트 단위로 같아야 한다.
// 구 이름(city.acquire·office.stage)은 실체와 달라 개명됐다 ― 되살아나면 서버가 400 을 준다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudEconomyReasonContractTest, "CGR.Cloud.EconomyReasonContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudEconomyReasonContractTest::RunTest(const FString& Parameters)
{
	// 지출 2종
	TestEqual(TEXT("도시 즉시완료 reason"),
		UCityAcquisitionManager::GetSkipRecoverySpendReason().ToString(), FString(TEXT("city.skipRecovery")));
	TestEqual(TEXT("오피스 컨티뉴 reason"),
		UOfficeStageProgressManager::GetRetryDevelopmentSpendReason().ToString(), FString(TEXT("office.retryDevelopment")));

	// 소모 1종 + 허용 아이템(명함 3종)
	TestEqual(TEXT("잠재 리롤 reason"),
		UEmployeeManager::GetPotentialRerollConsumeReason().ToString(), FString(TEXT("employee.potentialReroll")));
	TestTrue(TEXT("종이 명함은 리롤 재료"), UEmployeeManager::IsRerollCard(EItemType::BusinessCardPaper));
	TestTrue(TEXT("골드 명함은 리롤 재료"), UEmployeeManager::IsRerollCard(EItemType::BusinessCardGold));
	TestTrue(TEXT("블랙 명함은 리롤 재료"), UEmployeeManager::IsRerollCard(EItemType::BusinessCardBlack));
	TestFalse(TEXT("채용권은 리롤 재료가 아니다"), UEmployeeManager::IsRerollCard(EItemType::RecruitTicketNormal));
	TestEqual(TEXT("명함 식별자"),
		FCloudEconomyData::ItemTypeToName(EItemType::BusinessCardGold).ToString(), FString(TEXT("BusinessCardGold")));

	// 지급 3종 + claimKey 형식(서버 정규식 ^[A-Za-z0-9][A-Za-z0-9._:-]{0,63}$)
	TestEqual(TEXT("코덱스 티어 source"),
		UOfficeStageProgressManager::GetCodexTierCreditSource().ToString(), FString(TEXT("codex.tierMilestone")));
	TestEqual(TEXT("코덱스 전체 source"),
		UOfficeStageProgressManager::GetCodexFullCreditSource().ToString(), FString(TEXT("codex.fullMilestone")));
	TestEqual(TEXT("무역 보석 source"),
		UTradePort::GetDiamondCreditSource().ToString(), FString(TEXT("trade.diamond")));
	TestEqual(TEXT("코덱스 티어 claimKey"),
		UOfficeStageProgressManager::CodexTierClaimKey(3), FString(TEXT("codex.tier.3")));
	TestEqual(TEXT("코덱스 전체 claimKey"),
		UOfficeStageProgressManager::CodexFullClaimKey(), FString(TEXT("codex.full")));
	// 주문 1건은 한 번만 완전 충족된다 ― 키가 주문을 식별해야 중복 지급 흔적이 남는다
	TestEqual(TEXT("무역 claimKey 는 주문을 식별한다"),
		UTradePort::OrderClaimKey(42), FString(TEXT("trade.order.42")));
	TestNotEqual(TEXT("주문이 다르면 키가 다르다"),
		UTradePort::OrderClaimKey(42), UTradePort::OrderClaimKey(43));

	return true;
}

// 지급 응답 해석 ― ALREADY_CLAIMED 는 오류가 아니고, 표시는 요청 상수가 아니라 result.credited 다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudCreditSemanticsTest, "CGR.Cloud.CreditSemantics",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudCreditSemanticsTest::RunTest(const FString& Parameters)
{
	using UCE = UCloudEconomySubsystem;

	// 이미 받은 것 = 실패 안내를 띄우면 플레이어가 버그로 읽는다
	TestTrue(TEXT("ALREADY_CLAIMED 는 오류가 아니다"), UCE::IsClaimBenign(CloudErr::AlreadyClaimed));
	TestFalse(TEXT("잔액 부족은 오류다"), UCE::IsClaimBenign(CloudErr::InsufficientFunds));
	TestFalse(TEXT("네트워크/내부 오류는 오류다"), UCE::IsClaimBenign(CloudErr::Internal));
	TestFalse(TEXT("코드 없음은 오류다"), UCE::IsClaimBenign(NAME_None));

	// trade.diamond 는 캡 초과를 거절이 아니라 클램프한다 ― 요청 80 인데 50 만 들어올 수 있다
	TestEqual(TEXT("클램프되면 표시는 실제 지급액"), UCE::ResolveCreditedAmount(80, 50), 50);
	TestEqual(TEXT("클램프 없으면 요청액 그대로"), UCE::ResolveCreditedAmount(80, 80), 80);
	// 서버가 요청보다 크게 주는 일은 없어야 하지만, 오면 요청값으로 막는다(신뢰 하한)
	TestEqual(TEXT("서버가 더 줘도 요청값 상한"), UCE::ResolveCreditedAmount(20, 200), 20);
	// 응답을 못 읽었으면 0 ― 없는 지급을 표시하지 않는다
	TestEqual(TEXT("수량 불명이면 0"), UCE::ResolveCreditedAmount(20, 0), 0);
	TestEqual(TEXT("음수 응답은 0"), UCE::ResolveCreditedAmount(20, -5), 0);

	// 고정 금액 지급(코덱스)의 액수는 creditRules.json 이 생성기로 헤더 상수를 따라간다 ―
	// 여기서는 클라가 그 값을 그대로 보내는지(클램프 규칙과 섞이지 않는지)만 본다
	TestEqual(TEXT("고정 지급은 클램프 대상이 아니다"), UCE::ResolveCreditedAmount(20, 20), 20);

	return true;
}

// pending 키는 각 접점끼리, 그리고 기존 스킨·특성·채용과도 갈라야 한다 ― 한 액션 대기가 다른 버튼을 막으면 죽은 UI 로 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudEconomyPendingKeysTest, "CGR.Cloud.EconomyPendingKeys",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudEconomyPendingKeysTest::RunTest(const FString& Parameters)
{
	TArray<FName> Keys = {
		// U3 접점
		UEmployeeManager::PotentialRerollActionKey(7),
		UCityAcquisitionManager::SkipRecoveryActionKey(7),
		UOfficeStageProgressManager::RetryDevelopmentActionKey(),
		FName(*UOfficeStageProgressManager::CodexTierClaimKey(7)),
		FName(*UOfficeStageProgressManager::CodexFullClaimKey()),
		// 기존 단위(U1·U2)
		URecruitmentManagerSubsystem::GachaActionKey(EGachaTier::Normal),
		UBuildingSkinManagerSubsystem::GachaActionKey(false),
		UBuildingTraitManagerSubsystem::GachaActionKey(false),
		UBuildingTraitManagerSubsystem::SlotUnlockActionKey(7, 1),
	};

	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		TestFalse(TEXT("키가 비어 있지 않다"), Keys[i].IsNone());
		for (int32 j = i + 1; j < Keys.Num(); ++j)
		{
			TestNotEqual(TEXT("서로 다른 액션은 서로 다른 키"), Keys[i], Keys[j]);
		}
	}

	// 대상별로 갈린다 ― 한 직원/회사의 대기가 옆 것을 막으면 안 된다
	TestNotEqual(TEXT("회사가 다르면 다른 키"),
		UCityAcquisitionManager::SkipRecoveryActionKey(7), UCityAcquisitionManager::SkipRecoveryActionKey(8));
	TestNotEqual(TEXT("티어가 다르면 다른 마일스톤 키"),
		FName(*UOfficeStageProgressManager::CodexTierClaimKey(7)),
		FName(*UOfficeStageProgressManager::CodexTierClaimKey(8)));

	// 서로를 막지 않는다
	UGameInstance* TestInstance = NewObject<UGameInstance>();
	UCloudEconomySubsystem* Cloud = NewObject<UCloudEconomySubsystem>(TestInstance);
	for (const FName& Key : Keys)
	{
		TestTrue(TEXT("독립적으로 잡힌다"), Cloud->BeginPendingAction(Key));
	}
	TestFalse(TEXT("같은 키 중복은 차단"), Cloud->BeginPendingAction(Keys[0]));
	for (const FName& Key : Keys)
	{
		Cloud->EndPendingAction(Key);
		TestFalse(TEXT("해제 후 진행 중이 아니다"), Cloud->IsActionPending(Key));
	}

	return true;
}

// ============================================================================
// U4 ― UI 직접 지출 2 + 3상태 안내 (배치 1-B-4c)
// ============================================================================

// Review Focus 5 ― 영구 차단에 "동기화 중" 이 계속 뜨면 거짓말이고, 아무것도 안 뜨면 재화 분실과 구분되지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudBlockStateTest, "CGR.Cloud.BlockState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudBlockStateTest::RunTest(const FString& Parameters)
{
	using UCE = UCloudEconomySubsystem;

	// 3분류 ― 설계서 §7.2 표
	TestEqual(TEXT("FORBIDDEN 은 영구 차단"),
		UCE::ClassifyBlock(FName(TEXT("FORBIDDEN")), false), ECloudBlockState::PermanentlyBlocked);
	TestEqual(TEXT("조회 진행 중은 동기화"),
		UCE::ClassifyBlock(NAME_None, true), ECloudBlockState::Syncing);
	TestEqual(TEXT("MAINTENANCE 는 영구 차단이 아니다(재시도로 풀린다)"),
		UCE::ClassifyBlock(FName(TEXT("MAINTENANCE")), false), ECloudBlockState::RetryPending);
	TestEqual(TEXT("코드 없이 멈춘 상태(백오프 대기)는 재시도"),
		UCE::ClassifyBlock(NAME_None, false), ECloudBlockState::RetryPending);
	// 재시도가 돌고 있어도 FORBIDDEN 은 풀리지 않는다 ― 동기화로 보이면 영구히 거짓말이 된다
	TestEqual(TEXT("FORBIDDEN 은 조회 중에도 영구 차단"),
		UCE::ClassifyBlock(CloudErr::Forbidden, true), ECloudBlockState::PermanentlyBlocked);
	// 다른 실패 코드는 전부 재시도 쪽 ― 부록 A 에서 영구는 FORBIDDEN 하나다(안내 축 기준)
	TestEqual(TEXT("INTERNAL 은 재시도"),
		UCE::ClassifyBlock(CloudErr::Internal, false), ECloudBlockState::RetryPending);
	TestEqual(TEXT("AUTH_EXPIRED 는 재시도"),
		UCE::ClassifyBlock(CloudErr::AuthExpired, false), ECloudBlockState::RetryPending);

	// 세 상태가 서로 다른 문구를 낸다 ― 같으면 분기 미구현이다
	TMap<FName, FText> Texts;
	Texts.Add(FCloudErrorTextRules::DefaultCode(), FText::FromString(TEXT("잠시 후 다시 시도해 주세요.")));
	Texts.Add(CloudErr::Forbidden, FText::FromString(TEXT("이용이 제한된 계정입니다.")));

	const FText SyncMsg = UCE::BlockStateMessage(ECloudBlockState::Syncing, FText::GetEmpty());
	const FText RetryMsg = UCE::BlockStateMessage(ECloudBlockState::RetryPending, FText::GetEmpty());
	const FText ForbidMsg = UCE::BlockStateMessage(ECloudBlockState::PermanentlyBlocked,
		FCloudErrorTextRules::Resolve(Texts, CloudErr::Forbidden));

	TestFalse(TEXT("동기화 문구가 비어 있지 않다"), SyncMsg.IsEmpty());
	TestFalse(TEXT("재시도 문구가 비어 있지 않다"), RetryMsg.IsEmpty());
	TestFalse(TEXT("영구 차단 문구가 비어 있지 않다"), ForbidMsg.IsEmpty());
	TestNotEqual(TEXT("동기화 != 재시도"), SyncMsg.ToString(), RetryMsg.ToString());
	TestNotEqual(TEXT("동기화 != 영구 차단"), SyncMsg.ToString(), ForbidMsg.ToString());
	TestNotEqual(TEXT("재시도 != 영구 차단"), RetryMsg.ToString(), ForbidMsg.ToString());
	TestTrue(TEXT("잠기지 않았으면 문구가 없다"),
		UCE::BlockStateMessage(ECloudBlockState::None, FText::GetEmpty()).IsEmpty());

	// 영구 차단 안내의 핵심 ― 이 문장이 없으면 플레이어가 차단을 재화 분실로 읽는다
	TestTrue(TEXT("영구 차단은 서버 보관을 알린다"),
		ForbidMsg.ToString().Contains(TEXT("재화는 서버에 안전하게 보관되어 있습니다")));
	// "동기화 중" 이 영구 차단 문구에 섞이면 멈춘 상태를 진행 중으로 오독시킨다
	TestFalse(TEXT("영구 차단에 동기화 문구가 섞이지 않는다"),
		ForbidMsg.ToString().Contains(TEXT("동기화 중")));
	// DT 부재에서도 빈 문구가 아니다(LastResort 로 떨어진다)
	TestFalse(TEXT("코드 문구가 없어도 영구 차단 안내가 비지 않는다"),
		UCE::BlockStateMessage(ECloudBlockState::PermanentlyBlocked, FText::GetEmpty()).IsEmpty());

	// 코드별 문구 선택 ― 부족만 접점 고유 문구, 나머지는 DT 조회
	TestTrue(TEXT("INSUFFICIENT_FUNDS 는 기존 부족 문구 유지"),
		UCE::ShouldKeepShortfallText(CloudErr::InsufficientFunds));
	TestTrue(TEXT("INSUFFICIENT_ITEMS 는 기존 부족 문구 유지"),
		UCE::ShouldKeepShortfallText(CloudErr::InsufficientItems));
	TestFalse(TEXT("INTERNAL 은 DT 조회"), UCE::ShouldKeepShortfallText(CloudErr::Internal));
	TestFalse(TEXT("MAINTENANCE 는 DT 조회"), UCE::ShouldKeepShortfallText(CloudErr::Maintenance));
	TestFalse(TEXT("AUTH_REQUIRED 는 DT 조회"), UCE::ShouldKeepShortfallText(CloudErr::AuthRequired));
	TestFalse(TEXT("코드 없음은 DT 조회"), UCE::ShouldKeepShortfallText(NAME_None));

	// DT 조회 경로 ― 미등록 코드는 DEFAULT, 등록 코드는 그 문구
	Texts.Add(CloudErr::Internal, FText::FromString(TEXT("내부 문구")));
	TestEqual(TEXT("등록 코드는 그 문구"),
		FCloudErrorTextRules::Resolve(Texts, CloudErr::Internal).ToString(), FString(TEXT("내부 문구")));
	TestEqual(TEXT("미등록 코드는 DEFAULT 로"),
		FCloudErrorTextRules::Resolve(Texts, FName(TEXT("NOT_A_REAL_CODE"))).ToString(),
		FString(TEXT("잠시 후 다시 시도해 주세요.")));

	return true;
}

// UI 직접 지출 2건을 매니저로 옮긴 결과 ― reason 문자열과 pending 키가 계약과 맞는지.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudUiSpendContractTest, "CGR.Cloud.UiSpendContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudUiSpendContractTest::RunTest(const FString& Parameters)
{
	// spendRules.json 과 바이트 단위로 같아야 한다(구 이름 office.upgrade 는 폐기)
	TestEqual(TEXT("오피스 확장 reason"),
		UOfficeManager::GetExpandSpendReason().ToString(), FString(TEXT("office.expand")));
	TestNotEqual(TEXT("구 이름 office.upgrade 는 되살아나지 않는다"),
		UOfficeManager::GetExpandSpendReason().ToString(), FString(TEXT("office.upgrade")));

	// 방향별로 갈린다 ― 좌 확장 대기가 우 확장을 막으면 죽은 UI 로 보인다
	const FName LeftKey = UOfficeManager::ExpandActionKey(EOfficeExpandDirection::Left);
	const FName RightKey = UOfficeManager::ExpandActionKey(EOfficeExpandDirection::Right);
	TestFalse(TEXT("좌 확장 키가 비어 있지 않다"), LeftKey.IsNone());
	TestNotEqual(TEXT("방향이 다르면 다른 키"), LeftKey, RightKey);

	// 기존 U1~U3 키들과도 갈린다
	TArray<FName> Keys = {
		LeftKey,
		RightKey,
		UEmployeeManager::PotentialRerollActionKey(4),
		UCityAcquisitionManager::SkipRecoveryActionKey(4),
		UOfficeStageProgressManager::RetryDevelopmentActionKey(),
		URecruitmentManagerSubsystem::GachaActionKey(EGachaTier::Premium),
		UBuildingSkinManagerSubsystem::GachaActionKey(true),
		UBuildingTraitManagerSubsystem::GachaActionKey(true),
		UBuildingTraitManagerSubsystem::SlotUnlockActionKey(4, 2),
	};
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		for (int32 j = i + 1; j < Keys.Num(); ++j)
		{
			TestNotEqual(TEXT("서로 다른 액션은 서로 다른 키"), Keys[i], Keys[j]);
		}
	}

	UGameInstance* TestInstance = NewObject<UGameInstance>();
	UCloudEconomySubsystem* Cloud = NewObject<UCloudEconomySubsystem>(TestInstance);
	TestTrue(TEXT("좌 확장 요청"), Cloud->BeginPendingAction(LeftKey));
	TestFalse(TEXT("좌 확장 중복은 차단"), Cloud->BeginPendingAction(LeftKey));
	TestTrue(TEXT("우 확장은 독립"), Cloud->BeginPendingAction(RightKey));
	Cloud->EndPendingAction(LeftKey);
	Cloud->EndPendingAction(RightKey);
	TestFalse(TEXT("해제 후 진행 중이 아니다"), Cloud->IsActionPending(LeftKey));

	// 룩박스 지출(lootbox.purchase)은 이 단위에서 옮기지 않았다 ― 담당 매니저가 없어 보고로 돌렸다.
	// 서버 reason 은 이미 등록돼 있고 접점 함수/PurchasePrice 리터럴이 생성기 앵커라 그대로 남아 있다.

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
