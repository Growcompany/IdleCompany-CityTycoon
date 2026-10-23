#include "Misc/AutomationTest.h"
#include "UI/Element/Common/HelpButtonWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRHelpResolveModeTest,
	"CGR.Help.ResolveMode",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRHelpResolveModeTest::RunTest(const FString& Parameters)
{
	// 인자 순서 = (표 있음, 연결 패널 있음, 글 있음)

	// 단독 ― 하나씩만 있을 때
	TestEqual(TEXT("표만"),   ResolveHelpMode(true,  false, false), EHelpMode::Table);
	TestEqual(TEXT("링크만"), ResolveHelpMode(false, true,  false), EHelpMode::Panel);
	TestEqual(TEXT("글만"),   ResolveHelpMode(false, false, true),  EHelpMode::Prose);

	// 내용이 없으면 버튼을 내보내지 않는다 ― 13개 패널에서 "?" 가 죽어 있던 사고의 재발 방지
	TestEqual(TEXT("내용 없음"), ResolveHelpMode(false, false, false), EHelpMode::None);

	// 우선순위 ― 숫자가 있으면 숫자가 답이다
	TestEqual(TEXT("표+글 → 표"),      ResolveHelpMode(true,  false, true), EHelpMode::Table);
	TestEqual(TEXT("링크+글 → 링크"),  ResolveHelpMode(false, true,  true), EHelpMode::Panel);

	// 패널이 직접 만든 표가 그 화면 문맥에 맞으므로 링크보다 앞선다
	TestEqual(TEXT("표+링크 → 표"),    ResolveHelpMode(true,  true,  true), EHelpMode::Table);

	return true;
}

#endif
