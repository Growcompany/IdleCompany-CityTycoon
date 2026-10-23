#include "Misc/AutomationTest.h"
#include "Global/KoreanJosa.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * KoreanJosa 받침/조사 계약.
 *
 * 한계: 순수 함수만 본다. UTableManagerSubsystem::ApplyReviewTokens 가 이걸 호출한다는 연결은
 * 여기서 보증하지 않는다(그 함수는 private). 치환 규칙이 깨지면 이 테스트가, 배선이 끊기면
 * 출시 리뷰 화면이 알려준다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRKoreanJosaTest,
	"CGR.Text.KoreanJosa",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRKoreanJosaTest::RunTest(const FString& Parameters)
{
	using namespace KoreanJosa;

	// --- 받침 판정 ---
	TestTrue(TEXT("기획 받침"), HasBatchim(TEXT("기획")));
	TestTrue(TEXT("개발 받침"), HasBatchim(TEXT("개발")));
	TestTrue(TEXT("손맛 받침"), HasBatchim(TEXT("손맛")));
	TestTrue(TEXT("타격감 받침"), HasBatchim(TEXT("타격감")));
	TestFalse(TEXT("사운드 받침 없음"), HasBatchim(TEXT("사운드")));
	TestFalse(TEXT("설계 받침 없음"), HasBatchim(TEXT("설계")));
	TestFalse(TEXT("최적화 받침 없음"), HasBatchim(TEXT("최적화")));
	TestFalse(TEXT("빈 문자열"), HasBatchim(FString()));

	// 라틴은 조사를 못 고르므로 받침 없는 쪽("QA가") — 엔진 hpp 는 이 경우 조사를 아예 생략한다
	TestFalse(TEXT("QA 라틴"), HasBatchim(TEXT("QA")));
	TestFalse(TEXT("AI 라틴"), HasBatchim(TEXT("AI")));

	// 숫자는 발음 받침 — 엔진 hpp 가 틀리는 지점이다(hpp 는 전부 "받침 있음"으로 본다)
	TestTrue(TEXT("0 영"), HasBatchim(TEXT("350")));
	TestTrue(TEXT("1 일"), HasBatchim(TEXT("31")));
	TestTrue(TEXT("3 삼"), HasBatchim(TEXT("13")));
	TestTrue(TEXT("6 육"), HasBatchim(TEXT("16")));
	TestFalse(TEXT("2 이"), HasBatchim(TEXT("42")));
	TestFalse(TEXT("4 사"), HasBatchim(TEXT("14")));
	TestFalse(TEXT("5 오"), HasBatchim(TEXT("25")));
	TestFalse(TEXT("9 구"), HasBatchim(TEXT("49")));

	// --- ㄹ 받침 ((으)로 예외) ---
	TestTrue(TEXT("서울 리을"), EndsWithRieul(TEXT("서울")));
	TestTrue(TEXT("개발 리을"), EndsWithRieul(TEXT("개발")));
	TestFalse(TEXT("기획 리을 아님"), EndsWithRieul(TEXT("기획")));
	TestTrue(TEXT("1 일 리을"), EndsWithRieul(TEXT("1")));
	TestFalse(TEXT("3 삼 리을 아님"), EndsWithRieul(TEXT("3")));

	// --- Attach ---
	TestEqual(TEXT("기획이"), Attach(TEXT("기획"), TEXT("가"), TEXT("이")), FString(TEXT("기획이")));
	TestEqual(TEXT("사운드가"), Attach(TEXT("사운드"), TEXT("가"), TEXT("이")), FString(TEXT("사운드가")));
	TestEqual(TEXT("개발은"), Attach(TEXT("개발"), TEXT("는"), TEXT("은")), FString(TEXT("개발은")));
	TestEqual(TEXT("테스트는"), Attach(TEXT("테스트"), TEXT("는"), TEXT("은")), FString(TEXT("테스트는")));
	TestEqual(TEXT("검증을"), Attach(TEXT("검증"), TEXT("를"), TEXT("을")), FString(TEXT("검증을")));
	TestEqual(TEXT("설계를"), Attach(TEXT("설계"), TEXT("를"), TEXT("을")), FString(TEXT("설계를")));

	// (으)로 — ㄹ 받침은 "로"
	TestEqual(TEXT("기획으로"), Attach(TEXT("기획"), TEXT("로"), TEXT("으로")), FString(TEXT("기획으로")));
	TestEqual(TEXT("서울로"), Attach(TEXT("서울"), TEXT("로"), TEXT("으로")), FString(TEXT("서울로")));
	TestEqual(TEXT("설계로"), Attach(TEXT("설계"), TEXT("로"), TEXT("으로")), FString(TEXT("설계로")));
	TestEqual(TEXT("Lv.3 으로"), Attach(TEXT("3"), TEXT("로"), TEXT("으로")), FString(TEXT("3으로")));
	TestEqual(TEXT("Lv.1 로"), Attach(TEXT("1"), TEXT("로"), TEXT("으로")), FString(TEXT("1로")));
	TestEqual(TEXT("Lv.2 로"), Attach(TEXT("2"), TEXT("로"), TEXT("으로")), FString(TEXT("2로")));

	return true;
}

/**
 * DT_ReviewComment 실제 문구 패턴 — CSV 는 조사를 한 쪽만 적고, 받침은 치환이 맞춘다.
 * CSV 를 고치면 아래 패턴 문자열도 같이 고쳐야 한다(수동 동기화).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRReviewTokenJosaTest,
	"CGR.Text.ReviewTokenJosa",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRReviewTokenJosaTest::RunTest(const FString& Parameters)
{
	auto Apply = [](const TCHAR* Pattern, const TCHAR* Token, const TCHAR* Value)
	{
		FString Out = Pattern;
		KoreanJosa::ReplaceTokenWithJosa(Out, Token, Value);
		return Out;
	};

	// Sns031 / Crt017 / Crt027 — "{최저분야}가"
	TestEqual(TEXT("기획이 발목"), Apply(TEXT("{최저분야}가 발목을 잡는다"), TEXT("{최저분야}"), TEXT("기획")),
		FString(TEXT("기획이 발목을 잡는다")));
	TestEqual(TEXT("사운드가 발목"), Apply(TEXT("{최저분야}가 발목을 잡는다"), TEXT("{최저분야}"), TEXT("사운드")),
		FString(TEXT("사운드가 발목을 잡는다")));
	TestEqual(TEXT("손맛이 평균"), Apply(TEXT("{최저분야}가 평균을 깎아 먹었다"), TEXT("{최저분야}"), TEXT("손맛")),
		FString(TEXT("손맛이 평균을 깎아 먹었다")));

	// Sns032 / Crt028 / Tst011 — "{최저분야}는"
	TestEqual(TEXT("개발은 미완성"), Apply(TEXT("{최저분야}는 누가 봐도 미완성이다"), TEXT("{최저분야}"), TEXT("개발")),
		FString(TEXT("개발은 누가 봐도 미완성이다")));
	TestEqual(TEXT("테스트는 미완성"), Apply(TEXT("{최저분야}는 누가 봐도 미완성이다"), TEXT("{최저분야}"), TEXT("테스트")),
		FString(TEXT("테스트는 누가 봐도 미완성이다")));
	TestEqual(TEXT("검증은 기준 미달"), Apply(TEXT("{최저분야}는 출시 기준 미달이다"), TEXT("{최저분야}"), TEXT("검증")),
		FString(TEXT("검증은 출시 기준 미달이다")));

	// Crt005 / Crt006 — "{최고분야}가" · "{최고분야}는"
	TestEqual(TEXT("그래픽이 캐리"), Apply(TEXT("{최고분야}가 이 작품을 캐리한다"), TEXT("{최고분야}"), TEXT("그래픽")),
		FString(TEXT("그래픽이 이 작품을 캐리한다")));
	TestEqual(TEXT("최적화는 교과서"), Apply(TEXT("{최고분야}는 교과서로 써도 된다"), TEXT("{최고분야}"), TEXT("최적화")),
		FString(TEXT("최적화는 교과서로 써도 된다")));

	// Crt035 — "{프로젝트}는". 프로젝트명은 100종이라 CSV 에서 받침을 미리 알 수 없다
	TestEqual(TEXT("받침 프로젝트명"), Apply(TEXT("{프로젝트}는 잊고 다음을 보자"), TEXT("{프로젝트}"), TEXT("드래곤 레이드")),
		FString(TEXT("드래곤 레이드는 잊고 다음을 보자")));
	TestEqual(TEXT("무받침 프로젝트명"), Apply(TEXT("{프로젝트}는 잊고 다음을 보자"), TEXT("{프로젝트}"), TEXT("픽셀 던전")),
		FString(TEXT("픽셀 던전은 잊고 다음을 보자")));

	// CSV 가 반대쪽 조사를 적어도 교정된다 — 기획자가 "{최저분야}이" 로 써도 같은 결과
	TestEqual(TEXT("반대쪽 조사도 교정"), Apply(TEXT("{최저분야}이 발목을 잡는다"), TEXT("{최저분야}"), TEXT("사운드")),
		FString(TEXT("사운드가 발목을 잡는다")));

	// 토큰이 여러 번 나와도 각각 교정
	TestEqual(TEXT("토큰 2회"), Apply(TEXT("{최저분야}가 문제다 ― {최저분야}를 먼저 보라"), TEXT("{최저분야}"), TEXT("기획")),
		FString(TEXT("기획이 문제다 ― 기획을 먼저 보라")));

	// 값이 비면 조사까지 지운다 — "가 발목을 잡는다" 로 시작하지 않게
	TestEqual(TEXT("빈 값"), Apply(TEXT("{최저분야}가 발목을 잡는다"), TEXT("{최저분야}"), TEXT("")),
		FString(TEXT(" 발목을 잡는다")));

	// 조사 없이 토큰만 쓰는 문구는 그대로 치환
	TestEqual(TEXT("조사 없는 토큰"), Apply(TEXT("{최고분야} 쪽은 합격이다"), TEXT("{최고분야}"), TEXT("기획")),
		FString(TEXT("기획 쪽은 합격이다")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
