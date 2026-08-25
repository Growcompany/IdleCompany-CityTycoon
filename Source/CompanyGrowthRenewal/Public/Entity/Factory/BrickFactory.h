// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/InteractableBaseActor.h"
#include "GameFramework/Actor.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "Components/SceneComponent.h"
#include "Data/FactorySaveData.h"
#include "BrickFactory.generated.h"

class UInstancedStaticMeshComponent;
class ATimeCycleManager;

// 업그레이드 완료 broadcast — 완료 타입을 파라미터로. AlertMark 같은 외부 UI가 즉시 동기화할 때 사용.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFactoryUpgraded, EFactoryUpgradeType, UpgradeType);

UCLASS()
class COMPANYGROWTHRENEWAL_API ABrickFactory : public AInteractableBaseActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABrickFactory();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Input
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> IMC_Factory;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Factory;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void NotifyActorOnClicked(FKey ButtonPressed) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsFocusable;

	// 벽돌 스폰되는 지점
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "BrickFactory")
	TArray<USceneComponent*> SpawnPoints;

	// 벽돌 생성 시 재생할 연기 VFX
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "BrickFactory|VFX")
	class UNiagaraSystem* BrickSmokeVFX;

	// 캐싱된 VFX 컴포넌트 (재사용)
	UPROPERTY()
	class UNiagaraComponent* CachedSmokeVFX = nullptr;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void OnInteract_Implementation(APlayerController* PC) override;
	virtual void OnEndInteract_Implementation(APlayerController* PC) override;

	UFUNCTION()
	void SpawnBrick();

	// Factory 업그레이드 함수 — Count>1 이면 벌크(합산 비용 1회 차감, 사이드이펙트 1회). 기존 단건 호출부는 Count=1 기본값으로 무수정.
	UFUNCTION(BlueprintCallable, Category = "Factory")
	bool UpgradeFactory(EFactoryUpgradeType UpgradeType, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Factory")
	int32 GetUpgradeLevel(EFactoryUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintCallable, Category = "Factory")
	float GetUpgradeValue(EFactoryUpgradeType UpgradeType) const;

	// 하나라도 강화 가능한 업그레이드 타입이 있는지 (자금 충분 + MaxLevel 미만 + 잠금 해제 조건 만족)
	UFUNCTION(BlueprintCallable, Category = "Factory")
	bool CanUpgradeAny() const;

	UPROPERTY(BlueprintAssignable, Category = "Factory")
	FOnFactoryUpgraded OnFactoryUpgraded;

	// 트레일러/디버그용 홀드 생산 간격 오버라이드(초). >0 이면 강화 곡선 대신 이 값을 버스트 간격으로 사용.
	// -1(기본)=미사용(곡선값). 치트 FactoryMax [AmountLevel] [SpeedSeconds] 가 세팅. 저장 안 함(Transient).
	UPROPERTY(Transient)
	float TrailerSpawnIntervalOverride = -1.0f;

	// 자동 수집 관련
	UFUNCTION(BlueprintCallable, Category = "Factory")
	void UnlockAutoCollection();

	UFUNCTION(BlueprintCallable, Category = "Factory")
	bool IsAutoCollectionUnlocked() const;

	UFUNCTION(BlueprintCallable, Category = "Factory")
	int64 GetAutoCollectedAmount() const;

	UFUNCTION(BlueprintCallable, Category = "Factory")
	void CollectAutoResources();

	// 방금 해금됐는지 1회만 알려준다(소비형). 패널이 다음 오픈에서 해금 연출을 태울 때 사용
	bool ConsumeAutoUnlockCelebration();

	// Factory 데이터 저장/로드
	UFUNCTION(BlueprintCallable, Category = "Factory")
	FFactorySaveData GetFactoryData() const;

	UFUNCTION(BlueprintCallable, Category = "Factory")
	void SetFactoryData(const FFactorySaveData& InData);

private:
	bool bIsInteracting = false;
	FTimerHandle BrickSpawnTimerHandle;
	FTimerHandle AutoCollectionTimerHandle;

	UPROPERTY(EditAnywhere, Category = "BrickFactory")
	float BrickSpawnInterval = 0.5f;

	// Factory 고유 ID
	UPROPERTY(EditAnywhere, Category = "BrickFactory")
	FName FactoryID = TEXT("BrickFactory");

	// Factory 저장 데이터
	FFactorySaveData FactoryData;

	// 자동 수집 내부 함수
	void AutoCollectResources();
	void UpdateProductionValues();
	void ApplyAutoCollectionUnlock(bool bAllowCelebration);

	// 자동 수집 타이머 (재)설정 — 간격 하한을 여기 한 곳에서만 강제한다.
	// 곡선 바닥이 0.001초라 그대로 SetTimer 하면 초당 1000틱이 된다
	void RestartAutoCollectionTimer();

	// 자동 계열 슬롯이 공유하는 DT 기반 HQ 요구 레벨을 조회한다.
	bool TryGetAutoUnlockRequiredHQLevel(int32& OutRequiredHQLevel) const;

	// 현재 HQ 레벨이 DT 조건을 넘겼는지 확인 후 해금. bAllowCelebration=false 면 연출 플래그 없이 조용히
	void EvaluateAutoCollectionUnlock(bool bAllowCelebration);

	UFUNCTION()
	void HandleHQLevelUp(int32 NewLevel);

	// 해금 연출 대기 플래그 — 세션 내 1회성이라 저장하지 않음
	bool bPendingAutoUnlockCelebration = false;

	// 자동 수집 최소 간격(초)
	static constexpr float MinAutoCollectionInterval = 0.25f;

// Bounce 효과 관련
private:
	// 바운스 강도 (0.01 ~ 0.2 권장)
	UPROPERTY(EditAnywhere, Category = "BrickFactory|Bounce", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BounceIntensity = 0.03f;

	// Timeline 업데이트를 오버라이드하여 Z축 바운스 구현
	virtual void Wooble_TimelineUpdate(float Value) override;
	virtual void Wooble_TimelineFinished() override;

	// 바운스 효과 재생
	void PlayBounce();

	// 연기 VFX 시작/정지 (Continuous 타입 VFX용)
	void StartSmokeVFX();
	void StopSmokeVFX();

	// 증기 사운드 — VFX 라이프사이클에 동기화 (Looping 은 SoundWave 자산 설정 의존)
	UPROPERTY()
	class UAudioComponent* CachedSteamAudio = nullptr;

	void StartSteamSound(const FVector& Location);
	void StopSteamSound();

	// 산업 앰비언스 — Steam 과 병렬로 깔리는 배경 루프. 별도 채널이라 DT 행에서 볼륨 독립 조정 가능
	UPROPERTY()
	class UAudioComponent* CachedAmbienceAudio = nullptr;

	void StartAmbienceSound(const FVector& Location);
	void StopAmbienceSound();

// 야간 발광 조명 — 메시의 "Light*" 소켓에 상시 점등 광구 배치 (항공장애등과 동일한 Unlit+Additive 방식, 동적 광원 0)
private:
	UPROPERTY(VisibleAnywhere, Category = "BrickFactory|Light")
	UInstancedStaticMeshComponent* LightISM;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|Light")
	float LightScale = 0.5f;

	// 바닥 소켓은 지면에 묻히므로 지붕등보다 작게 + 살짝 띄운다
	UPROPERTY(EditAnywhere, Category = "BrickFactory|Light")
	float GroundLightScale = 0.4f;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|Light")
	float GroundLightZLift = 25.f;

	// 소켓 로컬 Z 가 이 값 미만이면 바닥 소켓으로 판정
	UPROPERTY(EditAnywhere, Category = "BrickFactory|Light")
	float GroundSocketZThreshold = 10.f;

	// 소켓 번호순으로 이웃한 두 소켓 사이를 이 간격(cm)으로 채운다. 0 이면 소켓 위치에만 배치.
	UPROPERTY(EditAnywhere, Category = "BrickFactory|Light")
	float LightSpacing = 0.f;

	// 이웃 소켓의 로컬 Z 차이가 이 값을 넘으면 다른 층으로 보고 런을 끊는다
	UPROPERTY(EditAnywhere, Category = "BrickFactory|Light")
	float LayerZTolerance = 20.f;

	// 본체 야간 자체발광 — 밤에 공장이 검은 덩어리로 묻히는 것 방지. 실제 광원 없이 emissive 만 올린다.
	UPROPERTY(EditAnywhere, Category = "BrickFactory|Light")
	FLinearColor NightEmissiveTint = FLinearColor(0.15f, 0.095f, 0.05f);

	// 본체 메시 MID — 머티리얼의 MPC 게이트 대신 여기서 밤/낮 값을 직접 주입한다
	UPROPERTY()
	UMaterialInstanceDynamic* BodyMID;

	void SetupBodyNightEmissive();

	void BuildNightLights();

	UFUNCTION()
	void HandleSunRise();
	UFUNCTION()
	void HandleSunSet();
	void ApplyNightVisibility(bool bNight);

	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	bool bLightVisible = true; // 시계를 못 찾으면 표시 유지(밤 조명 회귀 방지)
};
