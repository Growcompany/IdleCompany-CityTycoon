// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/BuildingTraitCategory.h"
#include "BuildingTraitTarget.generated.h"

// 건물 특성 효과가 붙는 게임플레이 수량(대상). 19 분야 : 19 대상 1:1.
// ⚠ 대상을 추가하려면 소비처 배선을 같은 작업 단위에 만들 것 — 소비처 없는 대상은 UI 에 뜨는 거짓말이 된다.
UENUM(BlueprintType)
enum class EBuildingTraitTarget : uint8
{
	None				UMETA(DisplayName = "없음"),

	OperationRevenue	UMETA(DisplayName = "운영 수익"),			// 수익
	DevCost				UMETA(DisplayName = "개발비"),				// 경영
	DevScore			UMETA(DisplayName = "업무 산출"),			// 효율
	RevenueStability	UMETA(DisplayName = "낮은 평가 수익 보정"),	// 데이터
	Quality				UMETA(DisplayName = "출시 완성도"),			// R&D
	IdleIncome			UMETA(DisplayName = "방치 수익"),			// 방치
	MarketCap			UMETA(DisplayName = "시가총액"),			// 성장
	RevenueDecay		UMETA(DisplayName = "수익 감소 지연"),		// ESG
	OperationTime		UMETA(DisplayName = "운영 수명"),			// IP
	CritChance			UMETA(DisplayName = "크리티컬 확률"),		// 행운
	FatigueRecovery		UMETA(DisplayName = "피로 회복"),			// 복지
	TradeValue			UMETA(DisplayName = "제조품 판매가"),		// 글로벌 (유일한 전사 적용)
	HRPower				UMETA(DisplayName = "채용 잠재 등급 운"),	// HR
	AuraPower			UMETA(DisplayName = "특수 빌딩 버프 흡수"),	// 파트너십
	VaultCapacity		UMETA(DisplayName = "금고 용량 배율"),		// CRM
	RiskLoss			UMETA(DisplayName = "도박 실패 손실 감소"),	// 리스크
	MetaAmplify			UMETA(DisplayName = "다른 특성 효과 증폭"),	// 마인드셋
	ProductionCount		UMETA(DisplayName = "제조 산출"),			// 제조전용
	GambleSuccess		UMETA(DisplayName = "도박 성공률")			// 프로젝트전용
};

// 표시 방향. 값은 항상 양수로 저장하고 증가/감소 의미는 대상이 소유한다 — 부호 뒤집힘 사고 방지.
UENUM(BlueprintType)
enum class ETraitTargetPolarity : uint8
{
	Increase,
	Decrease
};

// 표시 단위. Point 는 확률 가산(%p), Count 는 배율이 아닌 개수 가산.
UENUM(BlueprintType)
enum class ETraitTargetUnit : uint8
{
	Percent,
	Point,
	Count
};

// 분야 → 주대상 단일 진실원천. v2 는 1:1 이며 매핑 없는 분야가 없다.
inline EBuildingTraitTarget GetTargetForCategory(EBuildingTraitCategory Category)
{
	switch (Category)
	{
	case EBuildingTraitCategory::Revenue:       return EBuildingTraitTarget::OperationRevenue;
	case EBuildingTraitCategory::Management:    return EBuildingTraitTarget::DevCost;
	case EBuildingTraitCategory::Efficiency:    return EBuildingTraitTarget::DevScore;
	case EBuildingTraitCategory::Analytics:     return EBuildingTraitTarget::RevenueStability;
	case EBuildingTraitCategory::RnD:           return EBuildingTraitTarget::Quality;
	case EBuildingTraitCategory::Idle:          return EBuildingTraitTarget::IdleIncome;
	case EBuildingTraitCategory::Growth:        return EBuildingTraitTarget::MarketCap;
	case EBuildingTraitCategory::ESG:           return EBuildingTraitTarget::RevenueDecay;
	case EBuildingTraitCategory::IP:            return EBuildingTraitTarget::OperationTime;
	case EBuildingTraitCategory::Fortune:       return EBuildingTraitTarget::CritChance;
	case EBuildingTraitCategory::Welfare:       return EBuildingTraitTarget::FatigueRecovery;
	case EBuildingTraitCategory::Global:        return EBuildingTraitTarget::TradeValue;
	case EBuildingTraitCategory::HR:            return EBuildingTraitTarget::HRPower;
	case EBuildingTraitCategory::Network:       return EBuildingTraitTarget::AuraPower;
	case EBuildingTraitCategory::CRM:           return EBuildingTraitTarget::VaultCapacity;
	case EBuildingTraitCategory::Risk:          return EBuildingTraitTarget::RiskLoss;
	case EBuildingTraitCategory::Mindset:       return EBuildingTraitTarget::MetaAmplify;
	case EBuildingTraitCategory::Manufacturing: return EBuildingTraitTarget::ProductionCount;
	case EBuildingTraitCategory::Project:       return EBuildingTraitTarget::GambleSuccess;
	default:                                    return EBuildingTraitTarget::None;
	}
}

/**
 * 대상 표시명 — 런타임(UI) 표시는 반드시 이 함수를 경유할 것.
 * ⚠ `UEnum::GetDisplayNameTextByValue` 금지 (UMETA 는 WITH_EDITOR 전용 — 패키징 빌드에서 식별자로 떨어진다).
 */
inline FText GetTraitTargetDisplayName(EBuildingTraitTarget Target)
{
	switch (Target)
	{
	case EBuildingTraitTarget::OperationRevenue: return FText::FromString(TEXT("운영 수익"));
	case EBuildingTraitTarget::DevCost:          return FText::FromString(TEXT("개발비"));
	case EBuildingTraitTarget::DevScore:         return FText::FromString(TEXT("업무 산출"));
	case EBuildingTraitTarget::RevenueStability: return FText::FromString(TEXT("낮은 평가 수익 보정"));
	case EBuildingTraitTarget::Quality:          return FText::FromString(TEXT("출시 완성도"));
	case EBuildingTraitTarget::IdleIncome:       return FText::FromString(TEXT("방치 수익"));
	case EBuildingTraitTarget::MarketCap:        return FText::FromString(TEXT("시가총액"));
	case EBuildingTraitTarget::RevenueDecay:     return FText::FromString(TEXT("수익 감소 지연"));
	case EBuildingTraitTarget::OperationTime:    return FText::FromString(TEXT("운영 수명"));
	case EBuildingTraitTarget::CritChance:       return FText::FromString(TEXT("크리티컬 확률"));
	case EBuildingTraitTarget::FatigueRecovery:  return FText::FromString(TEXT("피로 회복"));
	case EBuildingTraitTarget::TradeValue:       return FText::FromString(TEXT("제조품 판매가"));
	case EBuildingTraitTarget::HRPower:          return FText::FromString(TEXT("채용 잠재 등급 운"));
	case EBuildingTraitTarget::AuraPower:        return FText::FromString(TEXT("특수 빌딩 버프 흡수"));
	case EBuildingTraitTarget::VaultCapacity:    return FText::FromString(TEXT("금고 용량 배율"));
	case EBuildingTraitTarget::RiskLoss:         return FText::FromString(TEXT("도박 실패 손실 감소"));
	case EBuildingTraitTarget::MetaAmplify:      return FText::FromString(TEXT("다른 특성 효과 증폭"));
	case EBuildingTraitTarget::ProductionCount:  return FText::FromString(TEXT("제조 산출"));
	case EBuildingTraitTarget::GambleSuccess:    return FText::FromString(TEXT("도박 성공률"));
	default:                                     return FText::GetEmpty();
	}
}

// 감소가 이득인 대상만 Decrease. 소비처는 Decrease 대상에 대해 (1 - v/100) 을 적용한다.
inline ETraitTargetPolarity GetTraitTargetPolarity(EBuildingTraitTarget Target)
{
	switch (Target)
	{
	case EBuildingTraitTarget::DevCost:
		return ETraitTargetPolarity::Decrease;
	default:
		return ETraitTargetPolarity::Increase;
	}
}

inline ETraitTargetUnit GetTraitTargetUnit(EBuildingTraitTarget Target)
{
	switch (Target)
	{
	case EBuildingTraitTarget::CritChance:
	case EBuildingTraitTarget::GambleSuccess:
		return ETraitTargetUnit::Point;
	case EBuildingTraitTarget::HRPower:
	case EBuildingTraitTarget::ProductionCount:
		return ETraitTargetUnit::Count;
	default:
		return ETraitTargetUnit::Percent;
	}
}

// 소수 0 은 떼고 표기 (10.0 -> "10", 3.5 -> "3.5"). 부호는 호출부가 붙이므로 절댓값만.
inline FString TraitValueToDisplayString(float Value)
{
	const float AbsValue = FMath::Abs(Value);
	return FMath::IsNearlyEqual(AbsValue, FMath::RoundToFloat(AbsValue))
		? FString::Printf(TEXT("%d"), FMath::RoundToInt(AbsValue))
		: FString::Printf(TEXT("%.1f"), AbsValue);
}

// 단위 접미사 단일 출처 — 카드 라벨(FormatTraitEffectText)과 팝업 문장(GetTraitTargetDetailText)이
// 같은 (Target, Value) 에 대해 다른 단위를 표시하는 일이 구조적으로 불가능해야 한다.
inline FString GetTraitTargetUnitSuffix(EBuildingTraitTarget Target)
{
	switch (GetTraitTargetUnit(Target))
	{
	case ETraitTargetUnit::Count:  return FString();
	case ETraitTargetUnit::Point:  return TEXT("%p");
	default:                       return TEXT("%");
	}
}

/**
 * 특성 효과 1줄 문구를 생성한다 — UI 는 CSV 텍스트 대신 이 함수만 쓴다.
 * 표시와 실효과가 같은 (Target, Value) 에서 나오므로 구조적으로 어긋날 수 없다.
 * 예: "운영 수익 +10%" / "개발비 -3.5%" / "크리티컬 확률 +1.2%p" / "HR 파워 +2"
 */
inline FText FormatTraitEffectText(EBuildingTraitTarget Target, float Value)
{
	if (Target == EBuildingTraitTarget::None || FMath::IsNearlyZero(Value))
	{
		return FText::GetEmpty();
	}

	const TCHAR* Sign = (GetTraitTargetPolarity(Target) == ETraitTargetPolarity::Decrease) ? TEXT("-") : TEXT("+");
	const FString ValueStr = TraitValueToDisplayString(Value);
	const FString Suffix = GetTraitTargetUnitSuffix(Target);

	return FText::FromString(FString::Printf(TEXT("%s %s%s%s"),
		*GetTraitTargetDisplayName(Target).ToString(), Sign, *ValueStr, *Suffix));
}

/**
 * 상세 팝업용 한 줄 설명. 카드 라벨(FormatTraitEffectText)이 못 담는 조건/상한을 문장이 싣는다.
 * ⚠ 부호를 붙이지 않는다 — 증가/감소 방향은 문장이 소유한다("3.5% 줄어듭니다").
 */
inline FText GetTraitTargetDetailText(EBuildingTraitTarget Target, float Value)
{
	if (Target == EBuildingTraitTarget::None || FMath::IsNearlyZero(Value))
	{
		return FText::GetEmpty();
	}

	const FString V = TraitValueToDisplayString(Value) + GetTraitTargetUnitSuffix(Target);

	switch (Target)
	{
	case EBuildingTraitTarget::OperationRevenue:
		return FText::FromString(FString::Printf(TEXT("이 건물이 출시한 프로젝트의 운영 수익이 %s 늘어납니다."), *V));
	case EBuildingTraitTarget::DevCost:
		return FText::FromString(FString::Printf(TEXT("프로젝트 착수 비용이 %s 줄어듭니다."), *V));
	case EBuildingTraitTarget::DevScore:
		return FText::FromString(FString::Printf(TEXT("직원이 개발 중 쌓는 점수가 %s 늘어납니다. 직능 구분 없이 모든 점수에 함께 적용됩니다."), *V));
	case EBuildingTraitTarget::RevenueStability:
		return FText::FromString(FString::Printf(TEXT("품질 평가가 낮게 나와도 수익이 S등급 수익 쪽으로 %s 당겨집니다. S등급에서는 효과가 없습니다."), *V));
	case EBuildingTraitTarget::Quality:
		return FText::FromString(FString::Printf(TEXT("출시 등급을 정하는 완성도가 %s 높게 계산됩니다. 직능 전체의 달성률 평균이며, 등급이 오르면 수익이 오릅니다."), *V));
	case EBuildingTraitTarget::IdleIncome:
		return FText::FromString(FString::Printf(TEXT("직원이 방치 중 벌어들이는 수익과 게임을 꺼 둔 동안의 정산 수익이 %s 늘어납니다. 오프라인 정산은 운영 중인 프로젝트가 있을 때만 발생하며, 금고 여유분까지만 받습니다."), *V));
	case EBuildingTraitTarget::MarketCap:
		return FText::FromString(FString::Printf(TEXT("출시를 마칠 때 오르는 시가총액이 %s 늘어납니다. 브랜드파워 강화와 곱해집니다."), *V));
	case EBuildingTraitTarget::RevenueDecay:
		return FText::FromString(FString::Printf(TEXT("출시 후 수익이 절반으로 줄어드는 데 걸리는 시간이 %s 길어집니다. 출시하는 순간의 특성으로 고정되며, 산업별 상한에 닿으면 더 늘어나지 않습니다."), *V));
	case EBuildingTraitTarget::OperationTime:
		return FText::FromString(FString::Printf(TEXT("출시한 프로젝트의 운영 기간이 %s 길어집니다. 히트작수명 강화와 곱해지며, 출시하는 순간의 특성으로 고정됩니다."), *V));
	case EBuildingTraitTarget::CritChance:
		return FText::FromString(FString::Printf(TEXT("직원이 점수를 낼 때 크리티컬이 터질 확률이 %s 늘어납니다. 크리티컬 확률은 스탯·버프·피버를 모두 합쳐 30%%까지만 오릅니다."), *V));
	case EBuildingTraitTarget::FatigueRecovery:
		return FText::FromString(FString::Printf(TEXT("직원의 피로가 %s 빨리 회복됩니다."), *V));
	case EBuildingTraitTarget::TradeValue:
		return FText::FromString(FString::Printf(TEXT("항구에서 제조품을 팔 때 받는 금액이 %s 올라갑니다. 회사 전체에 적용되는 유일한 특성이지만, 여러 건물에 장착해도 가장 높은 값 하나만 적용됩니다."), *V));
	case EBuildingTraitTarget::HRPower:
		return FText::FromString(FString::Printf(TEXT("일반 채용의 잠재 등급 확률 표를 결정하는 HR 파워가 %s 늘어납니다. HR 부서 직원의 강화 레벨 합과 같은 자리에 더해지고, 10/20/30/50 을 넘길 때 확률 표가 좋아집니다. 고급·프리미엄 채용에는 영향이 없습니다."), *V));
	case EBuildingTraitTarget::AuraPower:
		return FText::FromString(FString::Printf(TEXT("특수 빌딩 영향권 안에 있을 때, 특수 빌딩이 주는 버프가 %s 강해집니다. 영향권 밖에서는 효과가 없습니다."), *V));
	case EBuildingTraitTarget::VaultCapacity:
		return FText::FromString(FString::Printf(TEXT("금고에 담아 둘 수 있는 금액이 %s 늘어납니다. 금고용량 강화와 곱해집니다. 사무실 밖에서 쌓이는 운영 수익만 금고를 거치며, 꽉 차면 초과분은 사라집니다."), *V));
	case EBuildingTraitTarget::RiskLoss:
		return FText::FromString(FString::Printf(TEXT("개발 중 돌발 이벤트에서 도박에 실패했을 때 손실 폭이 %s 줄어듭니다. 산업에 따라 줄어드는 것이 개발 점수일 수도, 출시 후 수익 배율일 수도 있습니다. 성공률은 오르지 않습니다."), *V));
	case EBuildingTraitTarget::MetaAmplify:
		return FText::FromString(FString::Printf(TEXT("이 건물의 다른 특성 효과와 받고 있는 특수 빌딩 오라가 모두 %s 강해집니다. 특수 빌딩 버프 흡수와는 서로 증폭하지 않고, 개수로 표시되는 효과는 정수 단위로 반영됩니다."), *V));
	case EBuildingTraitTarget::ProductionCount:
		return FText::FromString(FString::Printf(TEXT("제조 주문 수량이 %s개 늘어납니다."), *V));
	case EBuildingTraitTarget::GambleSuccess:
		return FText::FromString(FString::Printf(TEXT("개발 중 돌발 이벤트의 도박 성공률이 %s 올라갑니다."), *V));
	default:
		return FText::GetEmpty();
	}
}
