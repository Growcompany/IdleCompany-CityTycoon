#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BubbleAnchorProvider.generated.h"

UINTERFACE(MinimalAPI)
class UBubbleAnchorProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 버블 컨테이너가 추적할 월드 앵커를 제공하는 액터용 인터페이스.
 * BubbleContainerWidget이 빌딩(BuildingIndex)뿐 아니라 임의의 액터(책상 등)를
 * 앵커로 추적할 수 있게 해 준다. 구현 액터는 보통 자기 상단 위치를 반환한다.
 */
class IBubbleAnchorProvider
{
	GENERATED_BODY()

public:
	virtual FVector GetBubbleAnchorPosition() const = 0;
};
