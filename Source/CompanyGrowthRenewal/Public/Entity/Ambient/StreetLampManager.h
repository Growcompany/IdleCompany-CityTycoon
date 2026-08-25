// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StreetLampManager.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class ATimeCycleManager;

/**
 * 도시 거리에 폴리지(HISM)로 깔린 가로등 메시의 소켓 위치에
 * 밤에만 켜지는 따뜻한 발광 글로우(Unlit+Additive 빌보드 카드)를 자동 배치.
 *  - 근거리: 메시의 모든 "Lamp*" 소켓(5구 가로등=Lamp0..Lamp4)마다 카드 1개(전구 디테일).
 *  - 원거리: 메시의 "LODLamp" 중앙 소켓에 카드 1장만(겹친 5장의 농축 밝기/오버드로우 제거).
 * 빌보드 페이싱과 거리 스케일은 카메라 이동 시 "스로틀"로만 갱신(정지 시 비용 0) — 매 프레임
 * 인스턴스 버퍼 재업로드로 인한 저사양 기기 프레임드롭을 피하면서도, 평면 카드가 카메라를 계속
 * 바라보게 해 이동 시 "빛이 작아졌다 커졌다" 하는 빌보드 고정 울렁임을 제거한다(동적 광원 0).
 * 점등/밤전용은 머티리얼(MI_StreetLampCard, M_CarLight 자식)이 처리. MainMap에 1개 배치.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AStreetLampManager : public AActor
{
	GENERATED_BODY()

public:
	AStreetLampManager();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 가로등으로 인식할 메시들(폴리지로 깔린 것). 비면 아무것도 안 함.
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	TArray<TSoftObjectPtr<UStaticMesh>> LampMeshes;

	// 글로우 기본 크기(소켓 월드스케일 × 이 값). RefDistance 안쪽에서의 크기.
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampGlowScale = 1.0f;

	// 원거리 LOD 카드(중앙 1장)는 5알을 대표하므로 더 크게 — 가로등 특유의 빛 번짐(bloom 확산) 유지.
	// 작은 1장이 아니라 큰 1장이라야 번짐도 살고 서브픽셀 fizzing도 덜하다. 디테일 카드 대비 배율.
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LODGlowScale = 2.5f;

	// 이 거리(cm)에서 기본 크기. 더 멀면 비례해 커져 화면상 일정 크기 유지(멀리서 안 사라짐).
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampRefDistance = 12000.f;

	// 거리 스케일 상한.
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampMaxGrow = 6.f;

	// 거리 스케일 하한 — 멀어도 카드가 화면상 일정 크기 이하로 작아지지 않게 해 서브픽셀 피징(잔떨림)을 줄임. 1=원래.
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampMinFactor = 1.5f;

	// 이 거리(cm) 너머의 가로등은 5알 디테일 대신 중앙 "LODLamp" 카드 1장만 표시(겹침 농축·오버드로우↓).
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LODSwitchDistance = 35000.f;

	// 빌보드 재페이싱/LOD 갱신 주기(초) — 매 프레임 대신 스로틀(이동 시 프레임드롭 방지).
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampUpdateInterval = 0.2f;

	// 카메라가 이 거리(cm) 미만으로만 움직였으면 갱신 스킵(정지 시 비용 0).
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampMoveThreshold = 50.f;

	// [Perf, 기본 끔] 거리 컬링 — End>0 일 때만 그 거리(cm) 밖 가로등 컬. 전경 맵은 0(전부 보임).
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampCullStartDistance = 0.f;

	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampCullEndDistance = 0.f;

	// 소켓 위치 미세 보정(cm, 월드 Z).
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float LampZLift = 0.f;

	// LevelInstance 로드 지연 대비 재시도 간격(초). 0 이하면 재시도 없음.
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	float RetryInterval = 0.5f;

	// 최대 재시도 횟수.
	UPROPERTY(EditAnywhere, Category = "StreetLamp")
	int32 MaxRetries = 10;

	UPROPERTY()
	UStaticMesh* GlowMesh;

	UPROPERTY()
	UMaterialInterface* GlowMaterial;

	// 근거리 5알 디테일 카드 / 원거리 중앙 LOD 카드 — 각각 공유 ISM 1개.
	UPROPERTY()
	UInstancedStaticMeshComponent* DetailISM;

	UPROPERTY()
	UInstancedStaticMeshComponent* LODISM;

	// 폴리지 가로등 인스턴스를 훑어 소켓 위치에 글로우 생성(정적 위치/기본 스케일 저장)
	void BuildStreetLamps();

private:
	// 가로등 1기(폴리지 인스턴스 1개) 단위 — 디테일 5알 + 중앙 LOD 1장 + 거리판정 기준점.
	struct FLampUnit
	{
		TArray<int32> DetailInstances;        // DetailISM 인스턴스 인덱스(=DetailPositions 인덱스)
		int32 LODInstance = INDEX_NONE;       // LODISM 인스턴스 인덱스(중앙 1장). 없으면 INDEX_NONE
		FVector Anchor = FVector::ZeroVector; // LOD 거리 판정 기준(중앙 소켓 위치 또는 디테일 중심)
		bool bFar = false;                    // 히스테리시스 상태 — 경계 근방 카메라 왕복 시 5알↔1장 교대 팝 방지
	};
	TArray<FLampUnit> LampUnits;

	// 각 카드의 정적 월드 위치 + 기본 스케일(거리 팩터 적용 전).
	TArray<FVector> DetailPositions, DetailBaseScales;
	TArray<FVector> LODPositions, LODBaseScales;

	// 공유 ISM 1개 생성(컬링/그림자/충돌 등 공통 세팅) — 디테일/LOD 양쪽에서 재사용.
	UInstancedStaticMeshComponent* MakeGlowISM();

	// 카메라 기준 빌보드 페이싱 + 거리 스케일 + 근/원 LOD 가시성을 1회 갱신.
	void RefreshGlows(const FVector& CamLoc);

	int32 RetriesLeft = 0;
	FTimerHandle RetryTimer;
	FVector LastCamLoc = FVector(TNumericLimits<float>::Max());
	float AccumTime = 0.f;
	bool bBakedOnce = false; // 카메라 안정 후 최소 1회 페이싱했는지(첫 갱신 강제용)

	// day/night 전환 시 글로우 ISM 가시성 토글(낮=숨김=렌더0, 밤=표시) — 인스턴스는 보존.
	UFUNCTION()
	void HandleSunRise();
	UFUNCTION()
	void HandleSunSet();
	void ApplyNightVisibility(bool bNight);

	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	bool bGlowVisible = true; // 시계 못 찾으면 표시 유지(밤 글로우 회귀 방지)
};
