#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Utils/CloudHttp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudHttpJoinUrlTest, "CGR.Cloud.JoinUrl",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudHttpJoinUrlTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("말단 슬래시 없음"),
		FCloudHttp::JoinUrl(TEXT("https://x.run.app"), TEXT("/auth/exchange")),
		FString(TEXT("https://x.run.app/auth/exchange")));

	TestEqual(TEXT("말단 슬래시 있음"),
		FCloudHttp::JoinUrl(TEXT("https://x.run.app/"), TEXT("/auth/exchange")),
		FString(TEXT("https://x.run.app/auth/exchange")));

	TestEqual(TEXT("경로 앞 슬래시 없음"),
		FCloudHttp::JoinUrl(TEXT("https://x.run.app/"), TEXT("auth/exchange")),
		FString(TEXT("https://x.run.app/auth/exchange")));

	TestEqual(TEXT("말단 슬래시 여러 개"),
		FCloudHttp::JoinUrl(TEXT("https://x.run.app///"), TEXT("///auth/exchange")),
		FString(TEXT("https://x.run.app/auth/exchange")));

	TestEqual(TEXT("에뮬레이터처럼 경로가 붙은 Base"),
		FCloudHttp::JoinUrl(TEXT("http://127.0.0.1:5001/demo-cgr/us-central1/api/"), TEXT("/chat/send")),
		FString(TEXT("http://127.0.0.1:5001/demo-cgr/us-central1/api/chat/send")));

	TestEqual(TEXT("쿼리 문자열은 그대로 보존"),
		FCloudHttp::JoinUrl(TEXT("https://x.run.app/"), TEXT("/chat/messages?channel=global&limit=30")),
		FString(TEXT("https://x.run.app/chat/messages?channel=global&limit=30")));

	TestEqual(TEXT("빈 경로는 Base 만 남긴다"),
		FCloudHttp::JoinUrl(TEXT("https://x.run.app/"), TEXT("")),
		FString(TEXT("https://x.run.app")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudHttpEnvelopeTest, "CGR.Cloud.Envelope",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCloudHttpEnvelopeTest::RunTest(const FString& Parameters)
{
	FCloudResponse Success;
	FCloudHttp::ParseEnvelope(200,
		TEXT("{\"ok\":true,\"serverTime\":1758600000,\"customToken\":\"ct\",\"pid\":\"P1\",\"displayName\":null}"),
		Success);
	TestTrue(TEXT("성공 봉투"), Success.bOk);
	TestEqual(TEXT("serverTime 파싱"), Success.ServerTime, (int64)1758600000);
	TestTrue(TEXT("Root 보존"), Success.Root.IsValid());

	FCloudResponse Failure;
	FCloudHttp::ParseEnvelope(401,
		TEXT("{\"ok\":false,\"serverTime\":1758600000,\"code\":\"AUTH_EXPIRED\",\"message\":\"token expired\"}"),
		Failure);
	TestFalse(TEXT("실패 봉투"), Failure.bOk);
	TestEqual(TEXT("에러코드"), Failure.Code, FString(TEXT("AUTH_EXPIRED")));
	TestEqual(TEXT("HTTP 상태 보존"), Failure.HttpStatus, 401);

	// 구글 ID 플랫폼 응답은 ok 필드가 없다 — HTTP 상태로 판정해야 한다
	FCloudResponse Google;
	FCloudHttp::ParseEnvelope(200, TEXT("{\"idToken\":\"it\",\"refreshToken\":\"rt\",\"expiresIn\":\"3600\"}"), Google);
	TestTrue(TEXT("봉투가 아닌 200 은 성공"), Google.bOk);

	FCloudResponse Broken;
	FCloudHttp::ParseEnvelope(502, TEXT("<html>bad gateway</html>"), Broken);
	TestFalse(TEXT("파싱 불가 본문은 실패"), Broken.bOk);
	TestEqual(TEXT("파싱 불가 본문은 INTERNAL"), Broken.Code, FString(TEXT("INTERNAL")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
