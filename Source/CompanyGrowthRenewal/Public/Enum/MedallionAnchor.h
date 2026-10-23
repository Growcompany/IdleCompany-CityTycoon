#pragma once

#include "CoreMinimal.h"
#include "MedallionAnchor.generated.h"

/** 국가 메달리온 라벨이 붙는 방향. 이웃과 겹치지 않도록 국가별로 다르게 건다. */
UENUM(BlueprintType)
enum class EMedallionAnchor : uint8
{
	Bottom	UMETA(DisplayName = "아래"),
	Top		UMETA(DisplayName = "위"),
	Left	UMETA(DisplayName = "왼쪽"),
	Right	UMETA(DisplayName = "오른쪽")
};
