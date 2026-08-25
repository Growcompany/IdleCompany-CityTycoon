#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "ProjectStepType.generated.h"

/**
 * 프로젝트 진행 단계
 * 각 프로젝트는 4단계를 거쳐 완료됨
 * 단계 번호는 동일하지만, 회사 타입별로 표시되는 이름이 다름
 */
UENUM(BlueprintType)
enum class EProjectStepType : uint8
{
	Step1 UMETA(DisplayName = "1단계"),
	Step2 UMETA(DisplayName = "2단계"),
	Step3 UMETA(DisplayName = "3단계"),
	Step4 UMETA(DisplayName = "4단계"),

	Max UMETA(Hidden)
};

// 단계 번호(1~4)를 EProjectStepType으로 변환
inline EProjectStepType StepNumberToProjectStepType(int32 StepNumber)
{
	switch (StepNumber)
	{
	case 1: return EProjectStepType::Step1;
	case 2: return EProjectStepType::Step2;
	case 3: return EProjectStepType::Step3;
	case 4: return EProjectStepType::Step4;
	default: return EProjectStepType::Step1;
	}
}

// NOTE: Step 이름은 DT_StepDisplayName이 단일 진실 소스.
// 런타임 조회는 UTableManagerSubsystem::GetStepDisplayName(CompanyType, VariantKey, StepNumber) 사용.
// 코드에 산업별/variant별 이름 하드코딩 금지.
