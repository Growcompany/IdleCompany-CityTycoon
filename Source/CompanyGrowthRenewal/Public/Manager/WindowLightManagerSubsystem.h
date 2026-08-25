#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WindowLightManagerSubsystem.generated.h"

class UMaterialParameterCollection;
class ATimeCycleManager;

/**
 * 창문 조명 관리 서브시스템 (WorldSubsystem + FTickableGameObject)
 * TimeCycleManager와 연동하여 MPC로 창문 조명 제어
 * 모바일 호환성을 위해 FTSTicker 대신 FTickableGameObject 사용
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWindowLightManagerSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UWindowLightManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// FTickableGameObject 인터페이스
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bIsInitialized && !IsTemplate(); }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	// 현재 Night Intensity 반환 (0~1)
	UFUNCTION(BlueprintCallable, Category = "Window Light")
	float GetCurrentNightIntensity() const { return CurrentNightIntensity; }

private:
	// 시간 기반 Night Intensity 계산 (0~1)
	float CalculateNightIntensity(float CurrentHour) const;

	// MPC 파라미터 업데이트
	void UpdateMPCParameter();

	// MPC 런타임 로드
	void LoadMPC();

	// Material Parameter Collection
	UPROPERTY()
	UMaterialParameterCollection* WindowLightMPC = nullptr;

	// TimeCycleManager 캐싱 (성능 최적화)
	UPROPERTY()
	ATimeCycleManager* CachedTimeCycleManager = nullptr;

	// 현재 Night Intensity (0~1)
	float CurrentNightIntensity = 0.5f;

	// 초기화 완료 여부 (Tick 활성화 조건)
	bool bIsInitialized = false;

	// Tick 간격 제어 (0.1초마다 업데이트)
	float TickAccumulator = 0.0f;
	float TickInterval = 0.1f;
};
