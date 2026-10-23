#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/RankingData.h"
#include "RankingBuildingPreviewRenderer.generated.h"

class UTableManagerSubsystem;
class UTextureRenderTarget2D;

/** 도시 스냅샷의 실제 모듈을 일회성 촬영한다. 게임플레이 건물은 생성하지 않는다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API URankingBuildingPreviewRenderer : public UObject
{
	GENERATED_BODY()
public:
	UTextureRenderTarget2D* CaptureRepresentative(UWorld* CaptureWorld, UTableManagerSubsystem* Tables, const FCitySnapshot& Snapshot);
	// 유효한 실제 건물 중 누적 수익 우선. 동률 또는 수익 기록이 없으면 높이, 행 이름, 입력 순서로 결정한다.
	static bool SelectRepresentative(const FCitySnapshot& Snapshot, UTableManagerSubsystem* Tables, FCitySnapshotBuilding& OutBuilding);
	void Release();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> CapturedTargets;
};
