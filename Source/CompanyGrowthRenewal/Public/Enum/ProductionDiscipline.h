#pragma once

#include "CoreMinimal.h"
#include "ProductionDiscipline.generated.h"

// 제작 6직능 — 슬롯 인덱스 고정(산업 불문). 부서와 1:1(EmployeeTypes.h 매핑).
// UMETA(DisplayName)은 에디터 드롭다운/BP 표시용, 런타임 표시 = TableManagerSubsystem::GetDisciplineDisplayName(산업별 DT).
UENUM(BlueprintType)
enum class EProductionDiscipline : uint8
{
	Plan     = 0 UMETA(DisplayName = "기획"),
	Dev      = 1 UMETA(DisplayName = "개발"),
	Graphics = 2 UMETA(DisplayName = "그래픽"),
	Sound    = 3 UMETA(DisplayName = "사운드"),
	Server   = 4 UMETA(DisplayName = "서버"),
	QA       = 5 UMETA(DisplayName = "QA"),
	Count    = 6 UMETA(Hidden)
};
