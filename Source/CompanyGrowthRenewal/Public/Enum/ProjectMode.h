#pragma once

#include "CoreMinimal.h"
#include "ProjectMode.generated.h"

/**
 * 프로젝트 운영 모드
 * 수주: 외부 클라이언트 의뢰 수행 (안정적 수익, 제약 많음)
 * 자체개발: 내부 기획으로 제품 개발 (리스크 크지만 수익 상한 높음)
 */
UENUM(BlueprintType)
enum class EProjectMode : uint8
{
	None UMETA(DisplayName = "None"),
	Commissioned UMETA(DisplayName = "수주"),
	InHouse UMETA(DisplayName = "자체개발"),

	Max UMETA(Hidden)
};

inline FString ProjectModeToString(EProjectMode Mode)
{
	switch (Mode)
	{
	case EProjectMode::Commissioned: return TEXT("수주");
	case EProjectMode::InHouse: return TEXT("자체개발");
	default: return TEXT("None");
	}
}
