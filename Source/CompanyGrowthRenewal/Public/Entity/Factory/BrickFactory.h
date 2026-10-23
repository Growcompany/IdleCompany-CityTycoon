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
class ABrickLaborer;
class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UFactoryUpgradeEffectComponent;

// 업그레이드 완료 broadcast — 완료 타입을 파라미터로. AlertMark 같은 외부 UI가 즉시 동기화할 때 사용.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFactoryUpgraded, EFactoryUpgradeType, UpgradeType);

UCLASS()
class COMPANYGROWTHRENEWAL_API ABrickFactory : public AInteractableBaseActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABrickFactory();

	// 도시 진입·오프닝·강화 패널에서 현재 표시 외형의 DT 카메라 거리를 공유한다.
	float GetCameraFocusDistanceCm() const;

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

	// 현재 외형의 굴뚝마다 하나씩 붙여 재사용한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNiagaraComponent>> CachedSmokeVFX;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void ReCalcBoxExtent() const override;

	virtual void OnInteract_Implementation(APlayerController* PC) override;
	virtual void OnEndInteract_Implementation(APlayerController* PC) override;

	// 벽돌 1묶음을 월드 위치에서 수확하고 수집 연출을 표시한다.
	// 잡부가 적재지점에 내려놓는 순간과 자동수집이 같은 함수를 쓴다.
	void HarvestBricksAt(const FVector& WorldLocation, int32 Amount);

// ───────── 잡부 (벽돌 생산 주체) ─────────
public:
	// 잡부가 공장에 들어갔다 — 연기를 뿜고 굴뚝(BrickSpawnPoint)에서 벽돌을 뱉는다.
	void OnLaborerEntered(int32 Amount);

	// 해고된 잡부가 내부로 들어가 사라지기 직전에 스스로 알린다.
	void NotifyLaborerRetired(ABrickLaborer* Laborer);

	FVector GetLaborerEntryPoint() const;
	FVector GetLaborerExitPoint() const;
	// 문턱 위치와 +X 안쪽 방향. 현재 합본 메시의 소켓을 우선한다.
	FTransform GetLaborerDoorTransform(int32 DoorIndex) const;

	UFUNCTION(BlueprintPure, Category = "Factory|Laborer")
	int32 GetLaborerDoorCount() const;

	// 증축과 무관한 부지 중심과 공장 배율을 적용한 예약 사각형이다.
	UFUNCTION(BlueprintPure, Category = "Factory|Placement")
	FVector GetPlacementFootprintCenter(bool bCommitted = false) const;

	UFUNCTION(BlueprintPure, Category = "Factory|Placement")
	FTransform GetPlacementFootprintTransform(bool bCommitted = false) const;

	UFUNCTION(BlueprintPure, Category = "Factory|Placement")
	FVector2D GetPlacementFootprintHalfExtent(bool bWorldAxes = true, bool bCommitted = false) const;

	UFUNCTION(BlueprintPure, Category = "Factory|Placement")
	FName GetOwningPlotId() const { return FactoryData.OwningPlotId; }

	UFUNCTION(BlueprintPure, Category = "Factory|Placement")
	bool IsRelocating() const { return bIsRelocating; }
	bool IsHoldingBoost() const { return bIsInteracting && !bIsRelocating; }

	bool BeginRelocation();
	// 예약 중심을 고정한 채 미리보기의 방향을 90도 돌린다.
	void RotatePlacementQuarterTurn();
	void FinishRelocation(bool bCommit, FName DestinationPlotId = NAME_None);

	// 현재 강화 레벨이 요구하는 잡부 수를 공장 단계의 고용 정원으로 제한한다.
	UFUNCTION(BlueprintCallable, Category = "Factory|Laborer")
	int32 GetDesiredLaborerCount() const;

	UFUNCTION(BlueprintPure, Category = "Factory|Laborer")
	int32 GetLaborerCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Factory|Laborer")
	int32 GetLaborerCapacityAtLevel(int32 ExpansionLevel) const;

	// 잡부 수/배달량/왕복 주기를 현재 강화 값에 맞춘다. 강화·로드 직후에 부른다.
	void RefreshLaborerCrew();

protected:
	// 잡부가 재생할 보행 클립. ABP 대신 단일 노드 재생을 쓰는 이유는
	// 걷기↔뛰기 전환과 재생 속도를 C++ 에서 직접 쥐기 위해서다 (BP 그래프 배선 불필요).
	UPROPERTY(EditDefaultsOnly, Category = "Factory|Laborer")
	TSoftObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Factory|Laborer")
	TSoftObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Factory|Laborer")
	TSoftObjectPtr<UAnimSequence> CarryWalkAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Factory|Laborer")
	TSoftObjectPtr<UAnimSequence> CarryRunAnim;

	// 이 배속을 넘으면 걷기 대신 뛰기를 재생한다.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "0.1"))
	float RunPaceThreshold = 1.0f;

	// 잡부가 들어갈 때 터지는 연기 지속(초).
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "0.1"))
	float SmokePulseSeconds = 0.24f;

	// 연기 재점화 최소 간격(초). 잡부가 여럿이면 매 입장마다 터뜨려 상시 분출이 된다.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "0.0"))
	float SmokeMinIntervalSeconds = 2.4f;

	// 굴뚝이 가려지지 않는 짧은 연기 크기.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "0.1"))
	float SmokePulseScale = 1.35f;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX", meta = (ClampMin = "0.1"))
	float SmokeParticleLifetime = 0.65f;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX", meta = (ClampMin = "0.3"))
	float HoldSmokeInterval = 0.95f;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX", meta = (ClampMin = "0.1"))
	float HoldSmokeScale = 1.6f;

	// 직렬화된 에셋 참조이므로 BP와 함께 쿠킹된다.
	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX")
	TSoftObjectPtr<UMaterialInterface> BoostStreakMaterial;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX")
	TSoftObjectPtr<UStaticMesh> BoostStreakMesh;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX", meta = (ClampMin = "4", ClampMax = "16"))
	int32 BoostStreakCount = 10;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX", meta = (ClampMin = "1.0"))
	float BoostStreakWidth = 34.f;

	UPROPERTY(EditAnywhere, Category = "BrickFactory|VFX", meta = (ClampMin = "100.0"))
	float BoostStreakTravelHeight = 1400.f;

	// 잡부 BP. 소프트 참조라 쿠킹 등록 폴더(FunnyWorker/)에 둘 것.
	UPROPERTY(EditDefaultsOnly, Category = "Factory|Laborer")
	TSoftClassPtr<ABrickLaborer> LaborerClass;

	// 잡부가 걷는 실제 지면 속도(cm/s). 왕복 시간은 강화 곡선이 아니라 **거리 ÷ 이 속도**로 정한다.
	// 곡선에서 바로 뽑으면 문 간격 62m 를 1.2초에 가게 돼(초속 50m) 발이 땅에서 미끄러진다.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "20.0"))
	float LaborerWalkSpeed = 200.f;

	// 홀드 중 이동 속도 배수. 보행 모션도 이 배수에 맞춰 뛰기로 바뀐다.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float HoldSpeedMultiplier = 2.0f;

	// 생산율 배수(곡선 대비). 이동 속도와 분리돼 있다 — 속도로 못 채우는 몫은 1회 운반량이 가져간다.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float IdleProductionScale = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "0.1"))
	float HoldProductionScale = 2.0f;

	// 베지에 경로가 직선보다 길어지는 비율. 왕복 시간 계산에 쓴다.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float PathLengthFudge = 1.35f;

	// 걷기/뛰기 클립이 자연스러워 보이는 기준 지면 속도(cm/s). 재생 속도 = 실제속도 / 이 값.
	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "20.0"))
	float WalkClipRefSpeed = 160.f;

	UPROPERTY(EditAnywhere, Category = "Factory|Laborer", meta = (ClampMin = "20.0"))
	float RunClipRefSpeed = 420.f;

	UPROPERTY()
	TArray<ABrickLaborer*> Laborers;

	// 메시 소켓이 없는 배치에서 사용하는 문 컴포넌트. 이름순으로 번호를 고정한다.
	UPROPERTY()
	TArray<USceneComponent*> LaborerDoors;

private:
	void PulseSmokeVFX();
	void ClearSmokeVFX();
	void TickHoldSmoke();
	void StartBoostStreaks();
	void StopBoostStreaks();

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> BoostStreakISM;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoostStreakMID;

	FTimerHandle HoldSmokeTimerHandle;
	FTimerHandle BoostStreakHideTimerHandle;

	void SpawnLaborer(int32 LaneIndex, int32 LaneCount);
	void ApplyLaborerPace(bool bHolding);
	float GetLaborerPathLength() const;
	float GetLaborerLegSeconds() const;

public:

	// Factory 업그레이드 함수 — Count>1 이면 벌크(합산 비용 1회 차감, 사이드이펙트 1회). 기존 단건 호출부는 Count=1 기본값으로 무수정.
	UFUNCTION(BlueprintCallable, Category = "Factory")
	bool UpgradeFactory(EFactoryUpgradeType UpgradeType, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Factory")
	int32 GetUpgradeLevel(EFactoryUpgradeType UpgradeType) const;

	// 현재 단계의 구매 상한. INDEX_NONE은 데이터 누락, 0은 인부 수 이외의 무제한 강화다.
	UFUNCTION(BlueprintPure, Category = "Factory")
	int32 GetUpgradeMaxLevel(EFactoryUpgradeType UpgradeType) const;

	// 현재 단계의 실제 구매 가능 횟수. 영구 상한과 초기 튜토리얼 구매 범위를 함께 적용한다.
	int32 GetAllowedUpgradeCount(EFactoryUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintCallable, Category = "Factory")
	float GetUpgradeValue(EFactoryUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintPure, Category = "Factory")
	float GetUpgradeValueAtLevel(EFactoryUpgradeType UpgradeType, int32 Level) const;

	UFUNCTION(BlueprintPure, Category = "Factory|Appearance")
	int32 GetAppearanceLevel() const { return CurrentAppearanceLevel; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Factory|Appearance")
	TObjectPtr<UFactoryUpgradeEffectComponent> UpgradeEffect;

	// 하나라도 강화 가능한 업그레이드 타입이 있는지 (자금 충분 + MaxLevel 미만 + 잠금 해제 조건 만족)
	UFUNCTION(BlueprintCallable, Category = "Factory")
	bool CanUpgradeAny() const;

	UPROPERTY(BlueprintAssignable, Category = "Factory")
	FOnFactoryUpgraded OnFactoryUpgraded;

	// 트레일러/디버그용 홀드 생산 간격 오버라이드(초). >0 이면 강화 곡선 대신 이 값을 버스트 간격으로 사용.
	// -1(기본)=미사용(곡선값). 치트 FactoryMax [AmountLevel] [SpeedSeconds] 가 세팅. 저장 안 함(Transient).
	UPROPERTY(Transient)
	float TrailerSpawnIntervalOverride = -1.0f;

	// 자동(오프라인) 카테고리 해금 관련
	UFUNCTION(BlueprintCallable, Category = "Factory")
	void UnlockAutoCollection();

	UFUNCTION(BlueprintCallable, Category = "Factory")
	bool IsAutoCollectionUnlocked() const;

	// 자리를 비운 동안 받을 수 있는 벽돌 상한. DT 값이 0 이하면 기본 100
	// (정산 지급과 패널 표시가 같은 값을 보게 하는 단일 경로)
	UFUNCTION(BlueprintCallable, Category = "Factory")
	int32 GetAutoCollectionCapacity() const;

	// 자리를 비운 동안의 초당 벽돌 생산량. 오프라인 정산과 패널 안내 문구가 같은 식을 봐야
	// 표시와 실지급이 어긋나지 않는다. 문 간격(액터 지오메트리)에 의존하므로 세이브만으론 못 구한다.
	UFUNCTION(BlueprintCallable, Category = "Factory")
	float GetOfflineBricksPerSecond() const;

	// 방금 해금됐는지 1회만 알려준다(소비형). 패널이 다음 오픈에서 해금 연출을 태울 때 사용
	bool ConsumeAutoUnlockCelebration();

	// Factory 데이터 저장/로드
	UFUNCTION(BlueprintCallable, Category = "Factory")
	FFactorySaveData GetFactoryData() const;

	UFUNCTION(BlueprintCallable, Category = "Factory")
	void SetFactoryData(const FFactorySaveData& InData);

private:
	bool bIsInteracting = false;
	bool bIsRelocating = false;
	FTransform RelocationOriginalTransform = FTransform::Identity;

	// 최고 단계의 본체 중심을 로컬 XY로 저작하며 외형 교체로 갱신하지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "Factory|Placement")
	FVector2D PlacementFootprintOffset = FVector2D::ZeroVector;

	// 원본 외형과 기준 2×2 Plot 부지를 같은 비율로 축소한다. 인부 크기는 유지한다.
	UPROPERTY(EditDefaultsOnly, Category = "Factory|Placement", meta = (ClampMin = "0.1"))
	float FactoryScale = 0.9f;

	void UpdateAppearanceTransform(float Pulse = 1.f);
	void RefreshFactoryAppearance(bool bAnimate);
	void ApplyPendingFactoryAppearance();
	void HandleUpgradeRevealed();
	void HandleUpgradeEffectFinished();
	void PlayUpgradeSound(FName SoundId);
	void RebuildAppearanceSpawnPoints();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PendingAppearanceMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> AppearanceSpawnPoints;

	int32 CurrentAppearanceLevel = 0;
	int32 PendingAppearanceLevel = 0;
	bool bAppearanceInitialized = false;
	FTimerHandle SmokePulseTimerHandle;
	float LastSmokePulseTime = -1000.f;

	UPROPERTY(EditAnywhere, Category = "BrickFactory")
	float BrickSpawnInterval = 0.5f;

	// Factory 고유 ID
	UPROPERTY(EditAnywhere, Category = "BrickFactory")
	FName FactoryID = TEXT("BrickFactory");

	// Factory 저장 데이터
	FFactorySaveData FactoryData;

	// 자리를 비운 동안의 생산분을 한 번에 지급한다 (SaveLoadManager::OnOfflineGainsApplied 구독).
	// 접속 중엔 잡부가 직접 HUD 로 나르므로 적립 타이머는 없다 — 둘 다 돌리면 이중 생산이 된다.
	void SettleOfflineBricks(float TotalGained, float OfflineSeconds);
	void UpdateProductionValues();
	void ApplyAutoCollectionUnlock(bool bAllowCelebration);

	// 자동 계열 슬롯이 공유하는 DT 기반 HQ 요구 레벨을 조회한다.
	bool TryGetAutoUnlockRequiredHQLevel(int32& OutRequiredHQLevel) const;

	// 현재 HQ 레벨이 DT 조건을 넘겼는지 확인 후 해금. bAllowCelebration=false 면 연출 플래그 없이 조용히
	void EvaluateAutoCollectionUnlock(bool bAllowCelebration);

	UFUNCTION()
	void HandleHQLevelUp(int32 NewLevel);

	// 해금 연출 대기 플래그 — 세션 내 1회성이라 저장하지 않음
	bool bPendingAutoUnlockCelebration = false;

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

	// 홀드 중 짧은 분출을 반복하고 해제 시 분출을 멈춘다.
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
	// 밤에 공장이 검은 덩어리로 묻히는 것만 막을 정도. 과하면 건물이 야광처럼 뜬다 — 2026-10-02 에 1/4 로 내림.
	FLinearColor NightEmissiveTint = FLinearColor(0.038f, 0.024f, 0.013f);

	// 본체 메시 MID — 머티리얼의 MPC 게이트 대신 여기서 밤/낮 값을 직접 주입한다
	UPROPERTY()
	UMaterialInstanceDynamic* BodyMID;

	void SetupBodyNightEmissive();

	void BuildNightLights();

	UFUNCTION()
	void HandleVisualTimeChanged();
	void ApplyNightVisibility(bool bNight);

	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	bool bLightVisible = true; // 시계를 못 찾으면 표시 유지(밤 조명 회귀 방지)
};
