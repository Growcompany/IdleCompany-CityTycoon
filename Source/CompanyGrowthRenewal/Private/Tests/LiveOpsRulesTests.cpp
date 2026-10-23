#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/LiveOpsRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsRulesTest, "CGR.LiveOps.ContentRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLiveOpsRulesTest::RunTest(const FString& Parameters)
{
	FLiveOpsEventDef Ev;
	Ev.Id = TEXT("EV_T"); Ev.Type = ELiveOpsEventType::CollectEvent; Ev.StartUnix = 1000; Ev.EndUnix = 2000;
	TestFalse(TEXT("before start"), FLiveOpsRules::IsActive(Ev, 999));
	TestTrue(TEXT("at start"), FLiveOpsRules::IsActive(Ev, 1000));
	TestTrue(TEXT("inside"), FLiveOpsRules::IsActive(Ev, 1999));
	TestFalse(TEXT("at end exclusive"), FLiveOpsRules::IsActive(Ev, 2000));

	FLiveOpsEventDef Hot;
	Hot.Type = ELiveOpsEventType::HotTimeEvent; Hot.StartUnix = 0; Hot.EndUnix = 1 << 30;
	{ FLiveOpsHotTimeWindow W; W.StartHourKst = 12; W.EndHourKst = 13; W.Multiplier = 1.5f; Hot.HotTime.Add(W); }
	{ FLiveOpsHotTimeWindow W; W.StartHourKst = 22; W.EndHourKst = 2; W.Multiplier = 1.3f; Hot.HotTime.Add(W); }
	TestEqual(TEXT("noon window"), FLiveOpsRules::HotTimeMultiplier(Hot, 12), 1.5f);
	TestEqual(TEXT("13 is outside [12,13)"), FLiveOpsRules::HotTimeMultiplier(Hot, 13), 1.0f);
	TestEqual(TEXT("midnight wrap 23"), FLiveOpsRules::HotTimeMultiplier(Hot, 23), 1.3f);
	TestEqual(TEXT("midnight wrap 1"), FLiveOpsRules::HotTimeMultiplier(Hot, 1), 1.3f);
	TestEqual(TEXT("outside 5"), FLiveOpsRules::HotTimeMultiplier(Hot, 5), 1.0f);

	FLiveOpsEventDef Degenerate;
	{ FLiveOpsHotTimeWindow W; W.StartHourKst = 9; W.EndHourKst = 9; W.Multiplier = 2.0f; Degenerate.HotTime.Add(W); }
	TestEqual(TEXT("start == end window ignored"), FLiveOpsRules::HotTimeMultiplier(Degenerate, 9), 1.0f);
	Degenerate.HotTime.Reset();
	{ FLiveOpsHotTimeWindow W; W.StartHourKst = 9; W.EndHourKst = 11; W.Multiplier = 0.0f; Degenerate.HotTime.Add(W); }
	TestEqual(TEXT("zero multiplier window ignored"), FLiveOpsRules::HotTimeMultiplier(Degenerate, 10), 1.0f);

	{ FLiveOpsMilestone M; M.Threshold = 5; FMissionReward R; R.ResourceType = EResourceType::Diamond; R.Amount = 20; M.Rewards.Add(R); Ev.Milestones.Add(M); }
	{ FLiveOpsMilestone M; M.Threshold = 15; FMissionReward R; R.ResourceType = EResourceType::Diamond; R.Amount = 30; M.Rewards.Add(R); Ev.Milestones.Add(M); }
	FLiveOpsEventProgress P; P.Points = 12;
	TestTrue(TEXT("milestone 0 claimable"), FLiveOpsRules::IsMilestoneClaimable(P, Ev, 0));
	TestFalse(TEXT("milestone 1 not reached"), FLiveOpsRules::IsMilestoneClaimable(P, Ev, 1));
	P.ClaimedMilestones.Add(0);
	TestFalse(TEXT("claimed once"), FLiveOpsRules::IsMilestoneClaimable(P, Ev, 0));
	P.ClaimedMilestones.Reset(); P.Points = 20;
	TArray<FMissionReward> Expired;
	FLiveOpsRules::ExpiredMilestoneRewards(P, Ev, Expired);
	TestEqual(TEXT("both unclaimed milestones expire"), Expired.Num(), 2);

	const FString Json = TEXT("[{\"Id\":\"EV_J\",\"Type\":\"CollectEvent\",\"Title\":\"t\",\"Desc\":\"d\",\"StartUnix\":10,\"EndUnix\":20,\"Counter\":\"ProjectLaunch\",\"Milestones\":[{\"Threshold\":3,\"Rewards\":[{\"ResourceType\":\"Diamond\",\"Amount\":7,\"ItemType\":\"None\",\"ItemAmount\":0}]}],\"HotTime\":[]}]");
	TArray<FLiveOpsEventDef> Parsed;
	TestTrue(TEXT("json parses"), FLiveOpsRules::ParseEvents(Json, Parsed));
	TestEqual(TEXT("json count"), Parsed.Num(), 1);
	TestEqual(TEXT("json enum"), Parsed[0].Counter, EQuestCounter::ProjectLaunch);
	TestEqual(TEXT("json nested reward"), Parsed[0].Milestones[0].Rewards[0].Amount, (int64)7);

	TestTrue(TEXT("empty json array parses"), FLiveOpsRules::ParseEvents(TEXT("[]"), Parsed));
	TestEqual(TEXT("empty json array count"), Parsed.Num(), 0);

	AddExpectedMessage(TEXT("Unable to parse"), EAutomationExpectedMessageFlags::Contains);
	TestFalse(TEXT("bad json rejected"), FLiveOpsRules::ParseEvents(TEXT("{not json"), Parsed));

	// 알 수 없는 enum 은 프로퍼티·구조체 두 단계에서 각각 로그를 남긴다 (횟수 0 = 1회 이상)
	AddExpectedMessage(TEXT("Unable to import"), EAutomationExpectedMessageFlags::Contains, 0);
	AddExpectedMessage(TEXT("Unable to convert element"), EAutomationExpectedMessageFlags::Contains, 0);
	AddExpectedMessage(TEXT("Error parsing one of the elements"), EAutomationExpectedMessageFlags::Contains, 0);
	const FString BadEnumJson = TEXT("[{\"Id\":\"EV_B\",\"Type\":\"NotAType\",\"StartUnix\":10,\"EndUnix\":20}]");
	TestFalse(TEXT("unknown enum rejected"), FLiveOpsRules::ParseEvents(BadEnumJson, Parsed));

	FString Error;
	FLiveOpsEventDef DupA; DupA.Id = TEXT("EV_D");
	TArray<FLiveOpsEventDef> DupEvents = { DupA, DupA };
	TestFalse(TEXT("duplicate event id rejected"), FLiveOpsRules::ValidateEvents(DupEvents, Error));
	FLiveOpsNoticeDef DupNotice; DupNotice.Id = TEXT("NT_D");
	TArray<FLiveOpsNoticeDef> DupNotices = { DupNotice, DupNotice };
	TestFalse(TEXT("duplicate notice id rejected"), FLiveOpsRules::ValidateNotices(DupNotices, Error));
	FLiveOpsMailDef DupMail; DupMail.Id = TEXT("GM_D");
	TArray<FLiveOpsMailDef> DupMails = { DupMail, DupMail };
	TestFalse(TEXT("duplicate mail id rejected"), FLiveOpsRules::ValidateMails(DupMails, Error));

	// T1 — 검증기가 실제로 거르는 네 가지(마일스톤 Money · 시각 범위 · End == Start · Multiplier <= 0)
	{
		FLiveOpsEventDef Bad; Bad.Id = TEXT("EV_MONEY");
		FLiveOpsMilestone M; M.Threshold = 1;
		FMissionReward R; R.ResourceType = EResourceType::Money; R.Amount = 100; M.Rewards.Add(R);
		Bad.Milestones.Add(M);
		TArray<FLiveOpsEventDef> Defs = { Bad };
		TestFalse(TEXT("milestone money reward rejected"), FLiveOpsRules::ValidateEvents(Defs, Error));
		TestTrue(TEXT("money error names the event"), Error.Contains(TEXT("EV_MONEY")));
	}
	{
		FLiveOpsEventDef Bad; Bad.Id = TEXT("EV_HOUR");
		FLiveOpsHotTimeWindow W; W.StartHourKst = 0; W.EndHourKst = 24; W.Multiplier = 1.5f; Bad.HotTime.Add(W);
		TArray<FLiveOpsEventDef> Defs = { Bad };
		TestFalse(TEXT("hour 24 rejected"), FLiveOpsRules::ValidateEvents(Defs, Error));
		Defs[0].HotTime[0].StartHourKst = -1; Defs[0].HotTime[0].EndHourKst = 5;
		TestFalse(TEXT("negative hour rejected"), FLiveOpsRules::ValidateEvents(Defs, Error));
		Defs[0].HotTime[0].StartHourKst = 9; Defs[0].HotTime[0].EndHourKst = 9;
		TestFalse(TEXT("End == Start rejected"), FLiveOpsRules::ValidateEvents(Defs, Error));
		Defs[0].HotTime[0].EndHourKst = 11; Defs[0].HotTime[0].Multiplier = 0.0f;
		TestFalse(TEXT("multiplier 0 rejected"), FLiveOpsRules::ValidateEvents(Defs, Error));
		Defs[0].HotTime[0].Multiplier = -1.0f;
		TestFalse(TEXT("negative multiplier rejected"), FLiveOpsRules::ValidateEvents(Defs, Error));
		Defs[0].HotTime[0].Multiplier = 1.5f;
		TestTrue(TEXT("valid window accepted"), FLiveOpsRules::ValidateEvents(Defs, Error));
	}
	{
		FLiveOpsMailDef Bad; Bad.Id = TEXT("GM_MONEY");
		FMissionReward R; R.ResourceType = EResourceType::Money; R.Amount = 50; Bad.Rewards.Add(R);
		TArray<FLiveOpsMailDef> Defs = { Bad };
		TestFalse(TEXT("mail money reward rejected"), FLiveOpsRules::ValidateMails(Defs, Error));
		Defs[0].Rewards[0].ResourceType = EResourceType::Diamond;
		TestTrue(TEXT("diamond mail accepted"), FLiveOpsRules::ValidateMails(Defs, Error));
	}

	// T6 — 공지/메일 JSON 왕복 1행씩
	{
		TArray<FLiveOpsNoticeDef> RoundNotices;
		const FString NoticeJson = TEXT("[{\"Id\":\"NT_R\",\"Title\":\"t\",\"Body\":\"b\",\"PostedUnix\":1700000000,\"bPinned\":true}]");
		TestTrue(TEXT("notice json parses"), FLiveOpsRules::ParseNotices(NoticeJson, RoundNotices));
		TestEqual(TEXT("notice count"), RoundNotices.Num(), 1);
		TestEqual(TEXT("notice id"), RoundNotices[0].Id, FString(TEXT("NT_R")));
		TestEqual(TEXT("notice posted"), RoundNotices[0].PostedUnix, (int64)1700000000);
		TestTrue(TEXT("notice pinned"), RoundNotices[0].bPinned);

		TArray<FLiveOpsMailDef> RoundMails;
		const FString MailJson = TEXT("[{\"Id\":\"GM_R\",\"Title\":\"t\",\"Body\":\"b\",\"Rewards\":[{\"ResourceType\":\"Diamond\",\"Amount\":9,\"ItemType\":\"None\",\"ItemAmount\":0}],\"StartUnix\":10,\"EndUnix\":20,\"ExpireDays\":3}]");
		TestTrue(TEXT("mail json parses"), FLiveOpsRules::ParseMails(MailJson, RoundMails));
		TestEqual(TEXT("mail count"), RoundMails.Num(), 1);
		TestEqual(TEXT("mail reward amount"), RoundMails[0].Rewards[0].Amount, (int64)9);
		TestEqual(TEXT("mail expire days"), RoundMails[0].ExpireDays, 3);
		TestTrue(TEXT("mail active inside window"), FLiveOpsRules::IsActive(RoundMails[0], 15));
		TestFalse(TEXT("mail inactive at end"), FLiveOpsRules::IsActive(RoundMails[0], 20));
	}

	// IsWithin 은 두 IsActive 오버로드의 공통 구현 — 경계 규약이 하나인지 직접 본다
	TestTrue(TEXT("IsWithin at start"), FLiveOpsRules::IsWithin(100, 200, 100));
	TestFalse(TEXT("IsWithin at end exclusive"), FLiveOpsRules::IsWithin(100, 200, 200));
	return true;
}

#endif
