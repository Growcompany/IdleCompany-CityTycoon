#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Element/Effects/ScoreOrbContainerWidget.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeDevelopmentEffectsLifecycleTest,
	"CompanyGrowth.UI.OfficeDevelopmentEffects.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeDevelopmentEffectsLifecycleTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UScoreOrbContainerWidget> Effects(NewObject<UScoreOrbContainerWidget>());
	Effects->bDevelopmentPresentation = true;
	Effects->SetDevelopmentStripRect(FVector2D(100.f, 160.f), FVector2D(2200.f, 310.f));
	int32 ArrivalCount = 0;
	Effects->OnScoreOrbArrived.AddLambda([&ArrivalCount](int32, bool) { ++ArrivalCount; });

	Effects->PlayDevelopmentGoalTab(0, FVector2D(300.f, 310.f), 280.f);
	TestTrue(TEXT("Goal tab without a widget table stays silent"), Effects->ActiveGoalTabs.IsEmpty());

	Effects->PlayDevelopmentFireworks(1.f);
	TestEqual(TEXT("Approved three bursts use 73 streaks"), Effects->DevelopmentStreaks.Num(), 73);
	TestEqual(TEXT("Twelve embers per burst"), Effects->DevelopmentEmbers.Num(), 36);
	FString Result;
	TestTrue(TEXT("Finite trajectories and budgets"), Effects->ValidateDevelopmentEffects(Result));
	Effects->PlayDevelopmentFireworks(2.f);
	TestTrue(TEXT("Maximum intensity stays inside the streak budget"), Effects->DevelopmentStreaks.Num() <= 96);

	// 결과 창은 개발 종료 0.9초 뒤에 열리고, 그 페이드 0.22초 동안 연출 막이 같이 빠져야 한다
	Effects->PlayDevelopmentFireworks(1.f);
	Effects->TickDevelopmentEffects(.18f);
	TestTrue(TEXT("Dim reaches full strength after its ramp"), Effects->GetDevelopmentDimAlpha() > .99f);
	Effects->TickDevelopmentEffects(.72f);
	Effects->FadeDevelopmentDim();
	Effects->TickDevelopmentEffects(.23f);
	TestTrue(TEXT("Launch modal fade retires the dim within 0.22s"), Effects->GetDevelopmentDimAlpha() < .001f);
	TestTrue(TEXT("Fireworks may finish behind the modal"), Effects->DevelopmentFireworkElapsed > 0.f);
	Effects->TickDevelopmentEffects(.5f);
	TestTrue(TEXT("Firework clock retires after its lifetime"),
		Effects->DevelopmentFireworkElapsed < 0.f && Effects->DevelopmentStreaks.IsEmpty() && Effects->DevelopmentEmbers.IsEmpty());

	Effects->PlayDevelopmentFireworks(1.f);
	Effects->TickDevelopmentEffects(1.5f);
	const float TailDim = Effects->GetDevelopmentDimAlpha();
	TestTrue(TEXT("Without a modal the dim tails off on its own"), TailDim > 0.f && TailDim < .3f);
	Effects->TickDevelopmentEffects(.2f);
	TestTrue(TEXT("Dim is gone once the clock retires"), Effects->GetDevelopmentDimAlpha() < .001f);

	Effects->PlayDevelopmentFireworks(1.f);
	Effects->ResetDevelopmentEffects();
	TestTrue(TEXT("Reset releases every presentation record"), Effects->DevelopmentStreaks.IsEmpty()
		&& Effects->DevelopmentEmbers.IsEmpty() && Effects->ActiveGoalTabs.IsEmpty() && Effects->GetDevelopmentDimAlpha() < .001f);
	Effects->OnScoreOrbArrived.Broadcast(1, false);
	TestEqual(TEXT("Presentation reset preserves scoring delegate"), ArrivalCount, 1);
	Effects->OnScoreOrbArrived.Clear();
	return true;
}
#endif
