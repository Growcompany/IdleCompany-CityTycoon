#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Enum/BuildingTraitTarget.h"
#include "KeystoneAuraSubsystem.generated.h"

class UEntityManager;
class UTableManagerSubsystem;
class ABuildingBaseActor;

// 특수 모뉴먼트의 원형 영향권 오라를 이웃 빌딩별/대상수량별 %로 집계 캐시.
// 배치/철거/레벨업 시에만 재계산(EntityManager::OnBuildingCountChanged 구독 + 명시적 호출).
UCLASS()
class COMPANYGROWTHRENEWAL_API UKeystoneAuraSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// 효과레이어가 호출: 해당 빌딩이 받는 누적 오라 %(대상 수량별).
	float GetAuraPercent(int32 BuildingIndex, EBuildingTraitTarget Target) const;

	// 전 빌딩 순회 재계산(배치/철거/모뉴먼트 레벨업 시 호출).
	void RecomputeAuras();

	// 이 빌딩이 어떤 축이든 특수 빌딩 오라를 받고 있는가 (슬롯 배지 = "영향권 밖" 판정).
	bool IsInAnyAuraRange(int32 BuildingIndex) const;

	// 영향권 재계산 완료 알림 — 특수 빌딩 건립/철거/증축이 모두 RecomputeAuras 를 지난다.
	FSimpleMulticastDelegate OnAurasRecomputed;

	// 이 키스톤의 영향권에 드는 비-키스톤 빌딩 목록(선택 시 바닥 하이라이트용). RecomputeAuras 와 동일 반경식.
	TArray<ABuildingBaseActor*> GetBuildingsInKeystoneZone(ABuildingBaseActor* Keystone) const;

private:
	void RecomputeAurasInternal();
	void HandleBuildingCountChanged();
	UEntityManager* GetEntityManager() const;
	UTableManagerSubsystem* GetTableManager() const;

	// BuildingIndex -> (Target -> 누적 Percent)
	TMap<int32, TMap<EBuildingTraitTarget, float>> AuraCache;

	UPROPERTY()
	mutable UEntityManager* CachedEntityManager = nullptr;

	UPROPERTY()
	mutable UTableManagerSubsystem* CachedTableManager = nullptr;

	FDelegateHandle BuildingCountChangedHandle;
};
