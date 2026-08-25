#pragma once

#include "CoreMinimal.h"
#include "AlertMark.generated.h"

UENUM(BlueprintType)
enum class EAlertMarkSize : uint8
{
	Small UMETA(DisplayName = "Small (text/tab/list)"),
	Large UMETA(DisplayName = "Large (button/icon)")
};

// Red = 긴급/신규(attention), Green = 완료/가용(affirmative) — 프로젝트 전역 색 의미 규약
UENUM(BlueprintType)
enum class EAlertMarkColor : uint8
{
	Red   UMETA(DisplayName = "Red (urgent/new)"),
	Green UMETA(DisplayName = "Green (done/available)")
};
