#include "Misc/AutomationTest.h"
#include "Entity/Officeworker/FunnyPartPicker.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFunnyPartPickerTest,
	"CGR.Employee.Cosmetics.FunnyPartPicker",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyPartPickerTest::RunTest(const FString& Parameters)
{
	const TArray<int32> Even = { 1, 1, 1, 1, 1 };

	// 결정성 — 같은 직원은 세션이 바뀌어도 같은 파츠를 입는다(세이브에 외형을 안 넣는 근거).
	const int32 A1 = FFunnyPartPicker::PickWeightedIndex(42, EFunnyPartSlot::Hair, Even);
	const int32 A2 = FFunnyPartPicker::PickWeightedIndex(42, EFunnyPartSlot::Hair, Even);
	TestEqual(TEXT("같은 ID+슬롯은 항상 같은 인덱스"), A1, A2);

	// 축 독립 — 슬롯마다 솔트가 달라야 헤어와 신발이 같이 움직이지 않는다.
	int32 SameCount = 0;
	for (int32 Id = 0; Id < 200; ++Id)
	{
		if (FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Hair, Even)
			== FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Shoe, Even))
		{
			++SameCount;
		}
	}
	TestTrue(TEXT("헤어와 신발이 상관되지 않는다 (200명 중 일치 100건 미만)"), SameCount < 100);

	// 범위 — 항상 유효 인덱스
	for (int32 Id = -50; Id < 50; ++Id)
	{
		const int32 Idx = FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Body, Even);
		TestTrue(TEXT("인덱스가 풀 범위 안"), Idx >= 0 && Idx < Even.Num());
	}

	// 빈 풀 / 전부 0가중치 → INDEX_NONE (호출자가 폴백하도록)
	TestEqual(TEXT("빈 풀은 INDEX_NONE"),
		FFunnyPartPicker::PickWeightedIndex(1, EFunnyPartSlot::Body, TArray<int32>()), (int32)INDEX_NONE);
	TestEqual(TEXT("전부 0가중치면 INDEX_NONE"),
		FFunnyPartPicker::PickWeightedIndex(1, EFunnyPartSlot::Body, TArray<int32>{ 0, 0, 0 }), (int32)INDEX_NONE);

	// 가중치 반영 — 0 가중치 항목은 절대 안 뽑힌다
	const TArray<int32> Skewed = { 0, 5, 0 };
	for (int32 Id = 0; Id < 100; ++Id)
	{
		TestEqual(TEXT("0가중치는 뽑히지 않는다"),
			FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Outerwear, Skewed), 1);
	}

	// 성별 매칭 — Any 는 양쪽 다 통과
	TestTrue(TEXT("Any 는 남성 직원에 매치"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Any, EEmployeeGender::Male));
	TestTrue(TEXT("Any 는 여성 직원에 매치"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Any, EEmployeeGender::Female));
	TestTrue(TEXT("Female 행은 여성 직원에 매치"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Female, EEmployeeGender::Female));
	TestFalse(TEXT("Female 행은 남성 직원에 매치되지 않는다"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Female, EEmployeeGender::Male));

	// 표정 우선순위 — Neutral 만 개인 눈썹을 살린다
	TestFalse(TEXT("Neutral 은 개인 눈썹을 덮지 않는다"),
		FFunnyPartPicker::ExpressionOverridesBrow(EWorkerFaceExpression::Neutral));
	TestTrue(TEXT("Angry 는 개인 눈썹을 덮는다"),
		FFunnyPartPicker::ExpressionOverridesBrow(EWorkerFaceExpression::Angry));
	TestTrue(TEXT("Tired 는 개인 눈썹을 덮는다"),
		FFunnyPartPicker::ExpressionOverridesBrow(EWorkerFaceExpression::Tired));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
