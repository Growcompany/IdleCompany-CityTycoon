// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "GameFramework/Actor.h"
#include "StreetLampSpotPool.generated.h"

class USpotLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class ULevel;
class ATimeCycleManager;

/**
 * 가로등(HISM 인스턴스) 아래에 밤에만 켜지는 스포트라이트를 뿌린다.
 * 램프 1개당 광원 1개는 불가능하다 — 이 맵만 559개이고 모바일 포워드의 동적 광원 예산을 한참 넘는다.
 * 그래서 광원을 PoolSize 개만 만들고 매 갱신마다 카메라가 바라보는 지점에 가까운 램프로 옮긴다.
 * 동적 광원 수는 PoolSize 로 제한하며, 거리 컬링과 달리 켜진 개수가 줌에 따라 출렁이지 않는다.
 * 그림자는 끈다. PoolSize = 0 이면 동적 광원만 끄고 바닥 카드는 유지한다.
 * 스트리밍 레벨별로 위치를 캐시하고 레벨 추가/제거 또는 기둥 이동 때 카드 배치를 갱신한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AStreetLampSpotPool : public AActor
{
	GENERATED_BODY()

public:
	AStreetLampSpotPool();
	virtual void Tick(float DeltaSeconds) override;
	void RefreshLampSources(TConstArrayView<ULevel*> ChangedLevels);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 대상 가로등 메시. 이 목록에 든 메시의 ISM/HISM 인스턴스만 수집한다(표지판·공원등 제외).
	UPROPERTY(EditAnywhere, Category = "LampSpot")
	TArray<TSoftObjectPtr<UStaticMesh>> LampMeshes;

	// 동시에 켜는 스포트라이트 수. 0이면 바닥 카드만 사용한다.
	UPROPERTY(EditAnywhere, Category = "LampSpot", meta = (ClampMin = "0", ClampMax = "32"))
	int32 PoolSize = 6;

	// 램프 머리 높이 = 메시 바운드 높이 × 이 비율. 소켓이 없는 City_Pack 메시용 추정값.
	UPROPERTY(EditAnywhere, Category = "LampSpot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadHeightRatio = 0.92f;

	// 이 거리보다 먼 램프에는 광원을 붙이지 않는다(가까운 것만 비춘다는 전제).
	UPROPERTY(EditAnywhere, Category = "LampSpot", meta = (ClampMin = "100.0"))
	float MaxAssignDistance = 22000.f;

	// 재배치 간격(초). 매 프레임 옮길 필요가 없고, 광원 이동은 섀도 없이도 갱신 비용이 있다.
	UPROPERTY(EditAnywhere, Category = "LampSpot", meta = (ClampMin = "0.0"))
	float UpdateInterval = 0.25f;

	UPROPERTY(EditAnywhere, Category = "LampSpot|Light", meta = (ClampMin = "0.0"))
	float Intensity = 30.f;

	UPROPERTY(EditAnywhere, Category = "LampSpot|Light", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float OuterConeAngle = 50.f;

	UPROPERTY(EditAnywhere, Category = "LampSpot|Light", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float InnerConeAngle = 18.f;

	// 좁게 유지해야 한다 — 광원 비용은 화면에서 덮는 면적에 비례한다.
	UPROPERTY(EditAnywhere, Category = "LampSpot|Light", meta = (ClampMin = "100.0"))
	float AttenuationRadius = 1500.f;

	UPROPERTY(EditAnywhere, Category = "LampSpot|Light")
	FLinearColor LightColor = FLinearColor(1.f, 0.910f, 0.576f, 1.f);

	// 램프 머리에서 이만큼 내려 광원을 놓는다(머리 메시에 묻혀 콘이 잘리는 것 방지).
	UPROPERTY(EditAnywhere, Category = "LampSpot|Light")
	float LightDropFromHead = 40.f;

	// 노면에 눕힌 Additive 빛 웅덩이 카드. 고정 배치라 빌보드 갱신은 없지만,
	// GPU 비용은 화면을 덮는 반투명 픽셀 수와 겹침에 비례한다.
	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool")
	bool bUsePoolCards = true;

	// 웅덩이 반경(cm). 커질수록 반투명 픽셀 처리량이 늘어난다.
	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool", meta = (ClampMin = "1.0"))
	float PoolCardRadius = 900.f;

	// 노면 위로 살짝 띄워 z-fighting 방지.
	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool")
	float PoolCardZOffset = 8.f;

	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool")
	UStaticMesh* PoolCardMesh;

	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool")
	UMaterialInterface* PoolCardMaterial;

	// 카드 밝기 배율. 공유 자산(MI_StreetLampCard)을 고치지 않고 런타임 MID 로 색에만 곱한다.
	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool", meta = (ClampMin = "0.0"))
	float PoolCardBrightness = 0.35f;

	// 팔 방향(기둥 -> 램프 머리) 축으로 늘리는 비. 1 = 정원. 1 미만이면 반대 축으로 늘어난다.
	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool", meta = (ClampMin = "0.1"))
	float PoolCardAspect = 1.35f;

	// 기둥 밑동에서 팔 방향으로 이만큼 밀어 놓는다 - 실제 빛은 기둥 옆 노면에 떨어진다.
	UPROPERTY(EditAnywhere, Category = "LampSpot|Pool")
	float PoolCardForwardOffset = 260.f;

private:
	struct FLevelLampData
	{
		TArray<FVector> Heads;
		TArray<FTransform> CardAnchors;
	};

	void OnLevelAdded(ULevel* AddedLevel, UWorld* LevelWorld);
	void OnLevelRemoved(ULevel* RemovedLevel, UWorld* LevelWorld);
	void CollectLampHeads(ULevel* LampLevel);
	void RebuildLampData();
	void EnsureLightPool();
	void BuildPoolCards();
	void RefreshAssignments();
	void SetPoolEnabled(bool bEnabled);

	UPROPERTY()
	TArray<USpotLightComponent*> Pool;

	UPROPERTY()
	UInstancedStaticMeshComponent* PoolISM;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> ResolvedLampMeshes;

	TMap<TWeakObjectPtr<ULevel>, FLevelLampData> LevelLampData;
	TArray<FVector> LampHeads;
	double AverageLampHeadZ = 0.0;
	// 카드 앵커: 위치 = 기둥 밑동(노면), 회전 = 팔 방향 yaw. 소켓이 있어 팔 방향을 알 수 있다.
	TArray<FTransform> LampCardAnchors;
	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	float Accum = 0.f;
	bool bPoolEnabled = false;
	bool bLampDataDirty = false;
	// 동일한 카메라의 광원 배정은 재사용하고, 스트리밍/점등 전환 시 무효화한다.
	FVector LastAssignmentCameraLocation = FVector::ZeroVector;
	FRotator LastAssignmentCameraRotation = FRotator::ZeroRotator;
	float LastAssignmentMaxDistance = 0.f;
	float LastAssignmentLightDrop = 0.f;
	bool bAssignmentsDirty = true;
};
