#pragma once

#include "CoreMinimal.h"
#include "Table/MissionTable.h"
#include "SideTrainingTypes.generated.h"

/** 도전을 막는 이유. UI 가 문구를 고르는 단일 기준 — 화면마다 다른 문장을 쓰지 않는다. */
UENUM(BlueprintType)
enum class ETrainingBlockReason : uint8
{
	None = 0,
	NoCrew,      // 직원 0명
	NoEntry,     // 입장권 0
	CompanyLocked,
	MaxCourse    // 최종 과정 도달 — 도전만 막힌다. 소탕은 계속 된다.
};
