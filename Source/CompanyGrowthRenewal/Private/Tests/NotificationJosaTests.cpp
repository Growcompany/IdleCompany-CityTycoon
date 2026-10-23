#include "Misc/AutomationTest.h"
#include "Internationalization/Text.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 알림/확인창 문구의 한국어 조사 계약. 과거 "{0}이(가) 부족합니다" 괄호 폴백이 그대로 노출돼
 * "다이아이(가) 부족합니다" 로 읽혔다 — 엔진 hpp 수식어가 받침을 갈라주는지 재화명 전수로 고정한다.
 *
 * 한계: 포맷 계약만 본다. DT_Resource 조회·위젯 표시 경로는 타지 않으므로
 * 표시명이 DT에서 바뀌면 아래 목록도 같이 고쳐야 한다(= CSV 와 수동 동기화).
 */
namespace
{
	// 실제 문구와 같은 패턴이어야 테스트가 계약이 된다 — 위젯/매니저의 NSLOCTEXT 원문을 그대로 복사한다
	const FText ShortagePattern = FText::FromString(TEXT("{0}|hpp(이,가) 부족합니다"));
	const FText PurchasePattern = FText::FromString(TEXT("{0}|hpp(을,를) 구매하시겠습니까?"));

	FString Shortage(const TCHAR* Name)
	{
		return FText::Format(ShortagePattern, FText::FromString(Name)).ToString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRNotificationJosaFormatTest,
	"CGR.UI.NotificationJosaFormat",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRNotificationJosaFormatTest::RunTest(const FString& Parameters)
{
	// DT_Resource 받침 있는 표시명 → "이"
	const TCHAR* const Consonant[] = {
		TEXT("시가총액"), TEXT("벽돌"), TEXT("자금"), TEXT("직원"), TEXT("철광석"),
		TEXT("실리콘"), TEXT("리튬"), TEXT("알루미늄"), TEXT("금"), TEXT("다이아몬드 원석")
	};
	for (const TCHAR* Name : Consonant)
	{
		TestEqual(*FString::Printf(TEXT("%s 이"), Name), Shortage(Name), FString::Printf(TEXT("%s이 부족합니다"), Name));
	}

	// DT_Resource 받침 없는 표시명 → "가"
	const TCHAR* const Vowel[] = {
		TEXT("다이아"), TEXT("부지"), TEXT("박스"), TEXT("구리"),
		TEXT("석유"), TEXT("희토류"), TEXT("목재")
	};
	for (const TCHAR* Name : Vowel)
	{
		TestEqual(*FString::Printf(TEXT("%s 가"), Name), Shortage(Name), FString::Printf(TEXT("%s가 부족합니다"), Name));
	}

	// 괄호 폴백이 다시 섞여 들어오면 즉시 실패
	for (const TCHAR* Name : Consonant)
	{
		TestFalse(*FString::Printf(TEXT("%s 괄호 폴백 없음"), Name), Shortage(Name).Contains(TEXT("(")));
	}

	// 구매 확인창
	TestEqual(TEXT("다이아 를"), FText::Format(PurchasePattern, FText::FromString(TEXT("다이아"))).ToString(),
		FString(TEXT("다이아를 구매하시겠습니까?")));
	TestEqual(TEXT("성장 패키지 를"), FText::Format(PurchasePattern, FText::FromString(TEXT("성장 패키지"))).ToString(),
		FString(TEXT("성장 패키지를 구매하시겠습니까?")));
	TestEqual(TEXT("출석 보상 을"), FText::Format(PurchasePattern, FText::FromString(TEXT("출석 보상"))).ToString(),
		FString(TEXT("출석 보상을 구매하시겠습니까?")));

	// AbbreviateNumber 한국어 단위로 끝나는 인자 — 단위가 한글이면 받침 판정이 정확하다
	TestEqual(TEXT("12만 을"), FText::Format(PurchasePattern, FText::FromString(TEXT("12만"))).ToString(),
		FString(TEXT("12만을 구매하시겠습니까?")));
	TestEqual(TEXT("3.5억 을"), FText::Format(PurchasePattern, FText::FromString(TEXT("3.5억"))).ToString(),
		FString(TEXT("3.5억을 구매하시겠습니까?")));
	TestEqual(TEXT("7조 를"), FText::Format(PurchasePattern, FText::FromString(TEXT("7조"))).ToString(),
		FString(TEXT("7조를 구매하시겠습니까?")));

	// 라틴으로 끝나면 엔진은 조사를 아예 붙이지 않는다(PitchBoardText::HasBatchim 과 다른 동작)
	TestEqual(TEXT("K 단위 조사 생략"), FText::Format(PurchasePattern, FText::FromString(TEXT("1.2K"))).ToString(),
		FString(TEXT("1.2K 구매하시겠습니까?")));

	return true;
}

/**
 * 엔진 hpp 의 숫자 처리 한계를 "지금 이렇게 동작한다" 로 못 박는다 — 고쳐지면 이 테스트가 깨져서 알려준다.
 *
 * TextFormatArgumentModifier.cpp 의 받침 판정이 (LastArgChar - 0xAC00) % 28 != 0 인데,
 * 숫자 '0'~'9'(48~57)는 0xAC00 보다 작아 음수 나머지(-24 ~ -15)가 나온다. 그래서 항상 != 0,
 * 즉 뒤에 OR 로 적힌 0·1·3·6·7·8 목록과 무관하게 모든 숫자가 "받침 있음" 으로 간다.
 *
 * 결론: 숫자로 끝나는 인자에 hpp 를 쓰면 2·4·5·9 가 틀린다("42을"). 숫자를 받는 문구는
 * 한글 단위를 붙이거나(만/억/조) 조사 없는 어순으로 쓴다 — BuildingManagePanelHelpTable 의 "Lv.{0} → Lv.{1}" 처럼.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRNotificationJosaDigitLimitTest,
	"CGR.UI.NotificationJosaDigitLimit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRNotificationJosaDigitLimitTest::RunTest(const FString& Parameters)
{
	// 발음상 받침이 있는 끝자리(영·일·삼·육·칠·팔) — 우연히 맞는 쪽
	for (const TCHAR* Num : { TEXT("350"), TEXT("31"), TEXT("13"), TEXT("16"), TEXT("27"), TEXT("18") })
	{
		TestEqual(*FString::Printf(TEXT("%s 을"), Num), FText::Format(PurchasePattern, FText::FromString(Num)).ToString(),
			FString::Printf(TEXT("%s을 구매하시겠습니까?"), Num));
	}

	// 발음상 받침이 없는 끝자리(이·사·오·구) — 엔진이 "를" 을 못 낸다. 아래가 현재 동작이자 버그다.
	for (const TCHAR* Num : { TEXT("42"), TEXT("14"), TEXT("25"), TEXT("49") })
	{
		TestEqual(*FString::Printf(TEXT("%s 은 을 로 나온다(엔진 한계)"), Num),
			FText::Format(PurchasePattern, FText::FromString(Num)).ToString(),
			FString::Printf(TEXT("%s을 구매하시겠습니까?"), Num));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
