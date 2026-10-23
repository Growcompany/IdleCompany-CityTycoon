#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Data/MailboxRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMailboxRulesTest, "CGR.LiveOps.MailRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMailboxRulesTest::RunTest(const FString& Parameters)
{
	FMailboxSaveData Save;
	auto Make = [](int64 Expire, const FString& Src = FString())
	{
		FMailEntry E; E.Id = FGuid::NewGuid(); E.SourceKey = Src; E.ReceivedUnix = 1000; E.ExpireUnix = Expire; return E;
	};
	FMailboxRules::Push(Save, Make(2000));
	FMailboxRules::Push(Save, Make(500));
	TestEqual(TEXT("two mails"), Save.Mails.Num(), 2);
	TestEqual(TEXT("purge removes expired"), FMailboxRules::PurgeExpired(Save, 1500), 1);
	TestEqual(TEXT("one left"), Save.Mails.Num(), 1);

	TestTrue(TEXT("global first time ok"), FMailboxRules::CanReceiveGlobal(Save, TEXT("GM_Launch")));
	FMailboxRules::Push(Save, Make(9999, TEXT("GM_Launch")));
	TestFalse(TEXT("global second time blocked"), FMailboxRules::CanReceiveGlobal(Save, TEXT("GM_Launch")));

	for (int32 i = 0; i < 60; ++i) FMailboxRules::Push(Save, Make(9999));
	TestEqual(TEXT("cap 50"), Save.Mails.Num(), FMailboxRules::MaxMails);
	TestTrue(TEXT("oldest dropped first (FIFO)"), Save.Mails[0].SourceKey.IsEmpty());

	// 만료 경계 — 만료 시각과 같은 순간은 이미 만료, 0 은 무기한
	const FMailEntry Bounded = Make(2000);
	TestFalse(TEXT("one second before expiry is alive"), FMailboxRules::IsExpired(Bounded, 1999));
	TestTrue(TEXT("now == expire is expired"), FMailboxRules::IsExpired(Bounded, 2000));
	TestTrue(TEXT("after expiry is expired"), FMailboxRules::IsExpired(Bounded, 2001));
	const FMailEntry NeverExpires = Make(0);
	TestFalse(TEXT("expire 0 never expires"), FMailboxRules::IsExpired(NeverExpires, 999999999));

	FMailboxSaveData Lookup;
	const FMailEntry Target = Make(9999);
	FMailboxRules::Push(Lookup, Target);
	FMailboxRules::Push(Lookup, Make(9999));
	const FMailEntry* Hit = FMailboxRules::FindMail(Lookup, Target.Id);
	TestNotNull(TEXT("FindMail hit"), Hit);
	if (Hit) TestTrue(TEXT("FindMail returns the right entry"), Hit->Id == Target.Id);
	TestNull(TEXT("FindMail miss"), FMailboxRules::FindMail(Lookup, FGuid::NewGuid()));

	// T5 — 상한 50 을 만료분·글로벌이 섞인 채로 넘겼을 때의 FIFO
	FMailboxSaveData Cap;
	FMailboxRules::Push(Cap, Make(500, TEXT("GM_Evicted")));   // 곧 밀려날 글로벌
	FMailboxRules::Push(Cap, Make(500));                        // 만료 예정 일반
	for (int32 i = 0; i < 48; ++i) FMailboxRules::Push(Cap, Make(9999));
	TestEqual(TEXT("exactly at cap"), Cap.Mails.Num(), FMailboxRules::MaxMails);
	TestEqual(TEXT("oldest is still the global one"), Cap.Mails[0].SourceKey, FString(TEXT("GM_Evicted")));

	FMailboxRules::Push(Cap, Make(9999, TEXT("GM_Latest")));
	TestEqual(TEXT("still capped after overflow"), Cap.Mails.Num(), FMailboxRules::MaxMails);
	TestTrue(TEXT("the global mail was evicted"), Cap.Mails[0].SourceKey.IsEmpty());
	// 소비 기록은 되돌리지 않는다(burned Id) — 밀려나도 다시 받을 수 없는 편도 동작이며, 그래서 세이브가 단조 증가한다
	TestFalse(TEXT("evicted global SourceKey stays consumed"), FMailboxRules::CanReceiveGlobal(Cap, TEXT("GM_Evicted")));
	TestFalse(TEXT("newest global also consumed"), FMailboxRules::CanReceiveGlobal(Cap, TEXT("GM_Latest")));

	// 만료분은 상한과 무관하게 정리로만 빠진다
	TestEqual(TEXT("expired entries purge out of a full box"), FMailboxRules::PurgeExpired(Cap, 1500), 1);
	TestEqual(TEXT("one slot freed"), Cap.Mails.Num(), FMailboxRules::MaxMails - 1);
	return true;
}

#endif
