#include "Misc/AutomationTest.h"
#include "Enum/BuildingTraitTarget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTraitLabelTest,
	"CGR.Trait.Label",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTraitLabelTest::RunTest(const FString& Parameters)
{
	// 불투명 라벨 6개가 결과중심 문안으로 바뀌었는지 — 내부 수량 이름이 남아 있으면 실패
	TestEqual(TEXT("수익 안정성 -> 낮은 평가 수익 보정"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::RevenueStability).ToString(),
		FString(TEXT("낮은 평가 수익 보정")));
	TestEqual(TEXT("수익 지속력 -> 수익 감소 지연"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::RevenueDecay).ToString(),
		FString(TEXT("수익 감소 지연")));
	TestEqual(TEXT("HR 파워 -> 채용 잠재 등급 운"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::HRPower).ToString(),
		FString(TEXT("채용 잠재 등급 운")));
	TestEqual(TEXT("주변 오라 효과 -> 특수 빌딩 버프 흡수"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::AuraPower).ToString(),
		FString(TEXT("특수 빌딩 버프 흡수")));
	TestEqual(TEXT("손실 방어 -> 도박 실패 손실 감소"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::RiskLoss).ToString(),
		FString(TEXT("도박 실패 손실 감소")));
	TestEqual(TEXT("특성 증폭 -> 다른 특성 효과 증폭"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::MetaAmplify).ToString(),
		FString(TEXT("다른 특성 효과 증폭")));

	// 게임 내 다른 개념과 이름이 겹쳐 오독되던 4건 (QA 직능 / 직능 표시명 / 강화 슬롯)
	TestEqual(TEXT("출시 품질 -> 출시 완성도"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::Quality).ToString(),
		FString(TEXT("출시 완성도")));
	TestEqual(TEXT("개발 점수 -> 업무 산출"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::DevScore).ToString(),
		FString(TEXT("업무 산출")));
	TestEqual(TEXT("전사 무역 판매가 -> 제조품 판매가"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::TradeValue).ToString(),
		FString(TEXT("제조품 판매가")));
	TestEqual(TEXT("금고 용량 -> 금고 용량 배율"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::VaultCapacity).ToString(),
		FString(TEXT("금고 용량 배율")));

	// 유지 축은 그대로 (라벨 교체가 번지지 않았는지)
	TestEqual(TEXT("운영 수익 유지"),
		GetTraitTargetDisplayName(EBuildingTraitTarget::OperationRevenue).ToString(),
		FString(TEXT("운영 수익")));

	// 모든 축에 표시명이 있어야 한다 — 빈 문자열 = UI 공백
	for (int32 i = 1; i <= static_cast<int32>(EBuildingTraitTarget::GambleSuccess); ++i)
	{
		const EBuildingTraitTarget T = static_cast<EBuildingTraitTarget>(i);
		TestFalse(FString::Printf(TEXT("표시명 비어있지 않음 (enum %d)"), i),
			GetTraitTargetDisplayName(T).IsEmpty());
	}

	// 글리프 사고 방지 — em-dash / 이모지가 라벨에 섞이면 박스로 렌더된다
	for (int32 i = 1; i <= static_cast<int32>(EBuildingTraitTarget::GambleSuccess); ++i)
	{
		const FString Label = GetTraitTargetDisplayName(static_cast<EBuildingTraitTarget>(i)).ToString();
		TestFalse(FString::Printf(TEXT("em-dash 없음: %s"), *Label),
			Label.Contains(TEXT("\u2014")));
	}

	// 전수 루프 상한 가드 — enum 뒤에 축을 붙이면 위 루프 상한(GambleSuccess)도 같이 올려야 한다
	const UEnum* TargetEnum = StaticEnum<EBuildingTraitTarget>();
	TestEqual(TEXT("GambleSuccess 가 enum 마지막 원소"),
		TargetEnum->GetValueByIndex(TargetEnum->NumEnums() - 2),
		static_cast<int64>(EBuildingTraitTarget::GambleSuccess));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRTraitDetailTextTest,
	"CGR.Trait.DetailText",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRTraitDetailTextTest::RunTest(const FString& Parameters)
{
	// 조건이 문장에 드러나야 한다 — 라벨만으로는 전달 불가한 정보
	TestEqual(TEXT("낮은 평가 수익 보정 문장"),
		GetTraitTargetDetailText(EBuildingTraitTarget::RevenueStability, 10.0f).ToString(),
		FString(TEXT("품질 평가가 낮게 나와도 수익이 S등급 수익 쪽으로 10% 당겨집니다. S등급에서는 효과가 없습니다.")));

	// 감소 대상은 부호를 붙이지 않고 방향을 문장이 소유한다 ("-3.5% 줄어듭니다" 는 이중부정 버그)
	TestEqual(TEXT("개발비 문장은 부호 없음"),
		GetTraitTargetDetailText(EBuildingTraitTarget::DevCost, 3.5f).ToString(),
		FString(TEXT("프로젝트 착수 비용이 3.5% 줄어듭니다.")));

	// %p 단위 — 이 문장이 %p 접미사 렌더링을 덮는 유일한 assertion 이다
	TestEqual(TEXT("크리티컬 확률 %p"),
		GetTraitTargetDetailText(EBuildingTraitTarget::CritChance, 1.2f).ToString(),
		FString(TEXT("직원이 점수를 낼 때 크리티컬이 터질 확률이 1.2%p 늘어납니다. 크리티컬 확률은 스탯·버프·피버를 모두 합쳐 30%까지만 오릅니다.")));

	// 개수 단위 — 접미사 없이 문장이 "개" 를 붙인다
	TestEqual(TEXT("제조 산출 개수"),
		GetTraitTargetDetailText(EBuildingTraitTarget::ProductionCount, 2.0f).ToString(),
		FString(TEXT("제조 주문 수량이 2개 늘어납니다.")));

	// 영향권 조건이 문장에 있어야 한다 — 이게 빠지면 실효과 0 인 상황을 플레이어가 알 수 없다
	TestTrue(TEXT("특수 빌딩 문장에 영향권 조건"),
		GetTraitTargetDetailText(EBuildingTraitTarget::AuraPower, 10.0f).ToString()
			.Contains(TEXT("영향권 밖에서는 효과가 없습니다")));

	// 리터럴 % 이스케이프 회귀 가드 — %% 를 % 하나로 쓰면 뒤 문자를 포맷 지정자로 먹어 30% 가 사라진다
	TestTrue(TEXT("크리티컬 확률 문장에 30% 상한"),
		GetTraitTargetDetailText(EBuildingTraitTarget::CritChance, 1.2f).ToString()
			.Contains(TEXT("30%까지만")));

	// 도달 불가 상한 광고 제거 확인 — 이 절이 되살아나면 실패한다
	TestFalse(TEXT("도박 실패 손실 문장에 70% 상한이 없다"),
		GetTraitTargetDetailText(EBuildingTraitTarget::RiskLoss, 10.0f).ToString()
			.Contains(TEXT("70%")));

	// 합산 예외(최댓값)가 문장에 남아 있는지 — 플레이어가 자원을 낭비하지 않게 하는 정보
	TestTrue(TEXT("제조품 판매가 문장에 최댓값 규칙"),
		GetTraitTargetDetailText(EBuildingTraitTarget::TradeValue, 1.5f).ToString()
			.Contains(TEXT("가장 높은 값 하나만")));

	// 강화 슬롯과의 구분이 문장에 남아 있는지
	TestTrue(TEXT("금고 용량 배율 문장에 강화 곱셈"),
		GetTraitTargetDetailText(EBuildingTraitTarget::VaultCapacity, 10.0f).ToString()
			.Contains(TEXT("금고용량 강화와 곱해집니다")));

	// 빈 입력 가드
	TestTrue(TEXT("None 은 빈 문자열"),
		GetTraitTargetDetailText(EBuildingTraitTarget::None, 10.0f).IsEmpty());
	TestTrue(TEXT("0 은 빈 문자열"),
		GetTraitTargetDetailText(EBuildingTraitTarget::OperationRevenue, 0.0f).IsEmpty());

	// 모든 축에 문장이 있어야 한다 — 빈 문장 = 팝업 공백
	for (int32 i = 1; i <= static_cast<int32>(EBuildingTraitTarget::GambleSuccess); ++i)
	{
		const EBuildingTraitTarget T = static_cast<EBuildingTraitTarget>(i);
		TestFalse(FString::Printf(TEXT("설명 문장 비어있지 않음 (enum %d)"), i),
			GetTraitTargetDetailText(T, 5.0f).IsEmpty());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
