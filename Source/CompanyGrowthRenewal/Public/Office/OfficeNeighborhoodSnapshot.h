#pragma once

#include "CoreMinimal.h"
#include "OfficeNeighborhoodSnapshot.generated.h"

class ABuildingBaseActor;
class UOfficeDistantBackdrop;

USTRUCT(BlueprintType)
struct COMPANYGROWTHRENEWAL_API FOfficeNeighborhoodSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bValid = false;
	UPROPERTY(BlueprintReadOnly) int32 ManagedBuildingIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FTransform SourceAnchorWorldTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) float OfficeHeightCm = 0.f;
	// 도시 월드를 유지하지 않고 사무실과 채용 왕복에서 재사용하는 사진이다.
	UPROPERTY(Transient) TObjectPtr<UOfficeDistantBackdrop> SourceBackdrop = nullptr;
};

class COMPANYGROWTHRENEWAL_API FOfficeNeighborhoodSnapshotBuilder
{
public:
	static constexpr float MaxCaptureDistanceCm = 60000.f;
	static FOfficeNeighborhoodSnapshot Capture(ABuildingBaseActor* ManagedBuilding);
};
