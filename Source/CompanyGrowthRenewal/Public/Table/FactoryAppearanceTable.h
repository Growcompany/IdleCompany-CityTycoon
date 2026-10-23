#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FactoryAppearanceTable.generated.h"

class UStaticMesh;

/** One factory-wide upgrade level and its appearance and laborer capacity. */
USTRUCT(BlueprintType)
struct FFactoryAppearanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory", meta = (ClampMin = "1"))
	int32 LaborerCapacity = 0;

	// 현재 외형을 포커스할 카메라 거리(cm). SpringArm 소켓 후퇴를 포함한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory", meta = (ClampMin = "1.0"))
	float CameraFocusDistanceCm = 0.f;
};
