// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "AviationBeaconManager.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class ATimeCycleManager;

/**
 * 스카이라인 BP_MB 건물들의 메시 "Beacon"/"L_Beacon" 소켓을 읽어 야간 항공장애등을 자동 배치.
 * BP_MB 의 정체성/컴포넌트는 건드리지 않고(읽기 전용), 비콘은 매니저가 소유한 "건물별 ISM"에 둔다.
 * 형상/머티리얼은 플레이어 건물 비콘과 동일(엔진 Sphere + MI_AviationBeacon).
 * 레벨(MainMap)에 1개 배치해 사용. (TrafficManager 와 동일 패턴 — 이름 prefix 로 도시 건물 탐색)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AAviationBeaconManager : public AActor
{
	GENERATED_BODY()

public:
	AAviationBeaconManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 스카이라인 건물 식별 클래스명 prefix (TrafficManager 와 동일 관행)
	UPROPERTY(EditAnywhere, Category = "Beacon")
	FString CityBuildingClassPrefix = TEXT("BP_MB");

	// "L_Beacon" 소켓(큰 비콘) 베이스 스케일. 최종 = 소켓 월드스케일 × 이 값.
	UPROPERTY(EditAnywhere, Category = "Beacon")
	float BeaconScaleLarge = 1.5f;

	// "Beacon" 소켓(보통 비콘) 베이스 스케일.
	UPROPERTY(EditAnywhere, Category = "Beacon")
	float BeaconScaleNormal = 0.75f;

	// 소켓 위치에서 비콘을 살짝 띄우는 오프셋(cm). 소켓이 정확한 자리라 기본 0.
	UPROPERTY(EditAnywhere, Category = "Beacon")
	float BeaconZLift = 0.f;

	UPROPERTY()
	UStaticMesh* BeaconMesh;

	UPROPERTY()
	UMaterialInterface* BeaconMaterial;

	// 건물별 ISM (per-building ObjectPositionWS → 같은 건물 동기 / 건물 간 비동기 유지)
	UPROPERTY()
	TArray<UInstancedStaticMeshComponent*> BeaconISMs;

	// 회사 Key → 그 건물의 비콘 ISM. 철거(OnCompanyCleared) 시 해당 건물 비콘만 골라 제거하기 위한 역인덱스.
	UPROPERTY()
	TMap<int32, UInstancedStaticMeshComponent*> BeaconISMByKey;

	// 배치된 BP_MB 들을 훑어 소켓 위치에 비콘 생성. 철거(Cleared)된 회사 건물은 제외.
	void BuildSkylineBeacons();

	// 회사 철거 완료(UCityAcquisitionManager::OnCompanyCleared) 시 해당 건물 비콘 ISM 제거.
	void HandleCompanyCleared(int32 Key);

	// day/night 전환 시 비콘 ISM 가시성만 토글(낮=숨김=렌더0, 밤=표시) — 인스턴스는 보존.
	UFUNCTION()
	void HandleSunRise();
	UFUNCTION()
	void HandleSunSet();
	void ApplyNightVisibility(bool bNight);

	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	bool bGlowVisible = true; // 시계 못 찾으면 표시 유지(밤 비콘 회귀 방지)

	// 비콘 빌드 지연 타이머 — Director 매핑/인수매니저 세이브 로드 완료 후 실행해 Cleared 게이트 신뢰성 확보
	// (액터 BeginPlay ↔ WorldSubsystem OnWorldBeginPlay 초기화 순서 의존 제거, SpawnClickProxies 패턴 미러).
	FTimerHandle BuildTimerHandle;
};
