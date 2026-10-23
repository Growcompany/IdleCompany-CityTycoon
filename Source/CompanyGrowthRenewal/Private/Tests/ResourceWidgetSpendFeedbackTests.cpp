#include "Misc/AutomationTest.h"
#include "CommonTextBlock.h"
#include "Components/ScaleBox.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Utils/FWidgetAnimationUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// 프로덕션 헤더에는 이 구조체 하나만 friend 로 열어 두고, 테스트는 전부 여기를 거친다.
struct FCGRResourceWidgetSpendTestHelper
{
	// HUD 자금 칩과 같은 조건 — 롤업은 bLiveCounterRollup + Money/Diamond + 시딩된 PeakDigitTier 에서만 산다.
	static UResourceWidget* MakeSeededChip(int64 Seed, EResourceType Type = EResourceType::Money)
	{
		UResourceWidget* Chip = NewObject<UResourceWidget>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!Chip)
		{
			return nullptr;
		}
		UScaleBox* Wrapper = NewObject<UScaleBox>(Chip, NAME_None, RF_Transient);
		UCommonTextBlock* Text = NewObject<UCommonTextBlock>(Chip, NAME_None, RF_Transient);
		if (!Wrapper || !Text)
		{
			return nullptr;
		}
		Wrapper->AddChild(Text);
		Chip->ResourceAmountText = Text;
		Chip->bLiveCounterRollup = true;
		Chip->ResourceType = Type;
		Chip->ResourceAmount = Seed;
		Chip->PeakDigitTier = UResourceWidget::GetDigitTier(Seed);
		return Chip;
	}

	static bool ShouldRollup(UResourceWidget* Chip, int64 NewAmount) { return Chip->ShouldRollup(NewAmount); }
	static void Tick(UResourceWidget* Chip, float DeltaTime) { Chip->UpdateFeedbackAnim(DeltaTime); }
};

namespace
{
	const int64 SeedAmount = 124000000;      // 1.24억 — 2% 임계 = 248만
	const int64 BigSpend = 32000000;         // 3200만, 임계 초과
}

// 지출(하강)도 목돈이면 굴러야 한다. 현재는 ShouldRollup 이 음수 델타를 전부 걸러낸다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRResourceWidgetSpendRollsTest,
	"CGR.ResourceWidget.SpendRollsDown",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRResourceWidgetSpendRollsTest::RunTest(const FString& Parameters)
{
	UResourceWidget* Chip = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(SeedAmount);
	if (!TestNotNull(TEXT("칩 생성"), Chip))
	{
		return true;
	}

	TestTrue(TEXT("목돈 지출은 굴러야 한다 (-3200만, 임계 248만 초과)"),
		FCGRResourceWidgetSpendTestHelper::ShouldRollup(Chip, SeedAmount - BigSpend));

	return true;
}

// 지출 눌림은 원 크기 아래로만 간다 — 1.0 을 넘으면 클립 부모에서 잘리고 확대 래스터가 깨진다(R29).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRResourceWidgetSpendDipTest,
	"CGR.ResourceWidget.SpendDipStaysUnderOne",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRResourceWidgetSpendDipTest::RunTest(const FString& Parameters)
{
	UResourceWidget* Chip = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(SeedAmount);
	if (!TestNotNull(TEXT("칩 생성"), Chip))
	{
		return true;
	}

	Chip->SetValue(SeedAmount - BigSpend);
	FCGRResourceWidgetSpendTestHelper::Tick(Chip, 0.01f);

	const double AfterFirstTick = Chip->GetRenderTransform().Scale.X;
	TestTrue(TEXT("지출 직후 칩이 눌린다 (스케일 < 1.0)"), AfterFirstTick < 1.0);

	double Peak = AfterFirstTick;
	for (int32 i = 0; i < 60; ++i)
	{
		FCGRResourceWidgetSpendTestHelper::Tick(Chip, 0.01f);
		Peak = FMath::Max(Peak, Chip->GetRenderTransform().Scale.X);
	}
	TestTrue(TEXT("눌림이 원 크기를 넘지 않는다 (R29)"), Peak <= 1.0001);
	TestEqual(TEXT("눌림이 끝나면 원 크기로 돌아온다"), Chip->GetRenderTransform().Scale.X, 1.0, 0.001);

	return true;
}

// 연출 감소가 켜지면 눌림은 재생되지 않고 값은 즉시 최종값이다 (FMotionGate).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRResourceWidgetSpendDipReducedTest,
	"CGR.ResourceWidget.SpendDipRespectsReduceMotion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRResourceWidgetSpendDipReducedTest::RunTest(const FString& Parameters)
{
	const bool bPrev = FMotionGate::IsReduced();
	FMotionGate::SetReduced(true);

	UResourceWidget* Chip = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(SeedAmount);
	if (!TestNotNull(TEXT("칩 생성"), Chip))
	{
		FMotionGate::SetReduced(bPrev);
		return true;
	}

	Chip->SetValue(SeedAmount - BigSpend);
	FCGRResourceWidgetSpendTestHelper::Tick(Chip, 0.01f);

	TestEqual(TEXT("연출 감소 중에는 눌리지 않는다"),
		Chip->GetRenderTransform().Scale.X, 1.0, 0.001);
	TestEqual(TEXT("연출 감소 중에는 값이 즉시 최종값이다"),
		Chip->ResourceAmount, SeedAmount - BigSpend);

	FMotionGate::SetReduced(bPrev);
	return true;
}

// 다이아는 뽑기 1회(-30) 급 소액으로 움직인다. 자금 기준 1000 을 공유하면 칩이 영구 무반응이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRResourceWidgetDiamondSmallSpendRollsTest,
	"CGR.ResourceWidget.DiamondSmallSpendRolls",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRResourceWidgetDiamondSmallSpendRollsTest::RunTest(const FString& Parameters)
{
	UResourceWidget* Diamond = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(350, EResourceType::Diamond);
	UResourceWidget* Money = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(350);
	UResourceWidget* Brick = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(350, EResourceType::Brick);
	if (!TestNotNull(TEXT("다이아 칩 생성"), Diamond) || !TestNotNull(TEXT("자금 칩 생성"), Money) || !TestNotNull(TEXT("벽돌 칩 생성"), Brick))
	{
		return true;
	}

	TestTrue(TEXT("다이아 뽑기 1회(-30)는 굴러야 한다"),
		FCGRResourceWidgetSpendTestHelper::ShouldRollup(Diamond, 320));
	TestFalse(TEXT("자금 30원은 여전히 즉시 대입이다"),
		FCGRResourceWidgetSpendTestHelper::ShouldRollup(Money, 320));
	TestFalse(TEXT("벽돌은 2페이즈 수집 연출이 피드백을 담당 — 칩 롤업 제외"),
		FCGRResourceWidgetSpendTestHelper::ShouldRollup(Brick, 320));

	return true;
}

// 보유가 커질수록 2% 상대 게이트가 커져 절대 임계(5)를 통째로 먹는다 — 고액 보유자에게 다이아 칩이 다시 죽는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRResourceWidgetDiamondRollsRegardlessOfHoldingTest,
	"CGR.ResourceWidget.DiamondRollsRegardlessOfHolding",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRResourceWidgetDiamondRollsRegardlessOfHoldingTest::RunTest(const FString& Parameters)
{
	UResourceWidget* Rich = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(6000, EResourceType::Diamond);
	UResourceWidget* Mid = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(1000, EResourceType::Diamond);
	if (!TestNotNull(TEXT("고액 다이아 칩 생성"), Rich) || !TestNotNull(TEXT("중액 다이아 칩 생성"), Mid))
	{
		return true;
	}

	TestTrue(TEXT("6000 보유에서도 뽑기 1회(-30)는 굴러야 한다"),
		FCGRResourceWidgetSpendTestHelper::ShouldRollup(Rich, 5970));
	TestTrue(TEXT("1000 보유에서 절대 임계 경계(-5)도 굴러야 한다"),
		FCGRResourceWidgetSpendTestHelper::ShouldRollup(Mid, 995));
	TestFalse(TEXT("절대 임계 미만(-4)은 보유와 무관하게 즉시 대입이다"),
		FCGRResourceWidgetSpendTestHelper::ShouldRollup(Mid, 996));

	// 자금의 상대 게이트는 유지한다 — 억 단위 보유에서 1000 원마다 칩이 구르면 계속 떠는 화면이 된다
	UResourceWidget* RichMoney = FCGRResourceWidgetSpendTestHelper::MakeSeededChip(100000000);
	if (TestNotNull(TEXT("고액 자금 칩 생성"), RichMoney))
	{
		TestFalse(TEXT("1억 보유에서 1000 원은 여전히 즉시 대입이다"),
			FCGRResourceWidgetSpendTestHelper::ShouldRollup(RichMoney, 100000000 - 1000));
	}

	return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
