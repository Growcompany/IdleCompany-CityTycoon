// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimeCycle/TimeCycleCode.h"
#include "TimeCycle/TimeCycleContinuation.h"
#include "GameFramework/Actor.h"
#include "TimeCycleManager.generated.h"

USTRUCT()
struct FTimeCycleData
{
	GENERATED_BODY()

	int32 GetCurrentSeconds() const { return FMath::Floor(CurrentTime); }

	// Current time in seconds, as a float because incremented each frame
	UPROPERTY()
	float CurrentTime = 0;

	UPROPERTY()
	int32 NumCycles = 0;

	UPROPERTY()
	bool bIsCyclePaused = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTimeCycleDelegate);

/*
 * 시간 진행을 관리하는 Actor
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ATimeCycleManager : public AActor
{
	GENERATED_BODY()

public:
	ATimeCycleManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// 사이클 시작, 시간 진행
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycle)
	void StartCycle();

	// 사이클 일시정지
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycle)
	void PauseCycle();

	// 현재 시간 설정
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycle)
	void SetCurrentTime(FTimeCycleCode NewTime);

	// 초기 시간으로 리셋
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycle)
	void ResetToInitialTime();

	// 현재 시간 반환
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	FTimeCycleCode GetCurrentTime() const;

	// 연출용 시간은 게임 시계의 저장·사이클·이벤트에 영향을 주지 않는다.
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	FTimeCycleCode GetVisualTime() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycle)
	bool SetVisualTimeOverride(FTimeCycleCode NewTime);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycle)
	void ClearVisualTimeOverride();

	UFUNCTION(BlueprintPure, Category=TimeCycle)
	bool HasVisualTimeOverride() const { return bHasVisualTimeOverride; }

	UFUNCTION(BlueprintPure, Category=TimeCycle)
	bool IsVisualNight() const;

	// 경과한 사이클 수 반환
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	int32 GetNumCycles() const;

	// 최대 시간 반환 (설정된 경우)
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	FTimeCycleCode GetMaxTime() const;

	// 하루 사이클 소요 게임 시간 반환
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	FTimeCycleCode GetCycleDuration() const;

	// 최대 시간에 도달했는지 확인
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	bool HasReachedMaxTime() const;

	// 일출 시간 반환
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	FTimeCycleCode GetSunRiseTime() const;

	// 일몰 시간 반환
	UFUNCTION(BlueprintPure, Category=TimeCycle)
	FTimeCycleCode GetSunSetTime() const;

	// 에디터 캡처 game world에 일몰 시간을 transient 적용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=TimeCycle, meta=(DevelopmentOnly))
	bool ApplyTransientSunSetTimeForPreview(FTimeCycleCode NewTime);

	// 사이클 시작 시 호출
	UPROPERTY(BlueprintAssignable, Category=TimeCycle)
	FTimeCycleDelegate OnCycleStarted;

	// 사이클 정지/일시정지 시 호출
	UPROPERTY(BlueprintAssignable, Category=TimeCycle)
	FTimeCycleDelegate OnCycleStopped;

	// 일출 시간 도달 시 호출
	UPROPERTY(BlueprintAssignable, Category=TimeCycle)
	FTimeCycleDelegate OnSunRise;

	// 일몰 시간 도달 시 호출
	UPROPERTY(BlueprintAssignable, Category=TimeCycle)
	FTimeCycleDelegate OnSunSet;

	// 장식 조명만 구독한다. 실제 일출·일몰과 연출 변경을 모두 반영한다.
	UPROPERTY(BlueprintAssignable, Category=TimeCycle)
	FTimeCycleDelegate OnVisualTimeChanged;

	// 정오 도달 시 호출
	UPROPERTY(BlueprintAssignable, Category=TimeCycle)
	FTimeCycleDelegate OnNoon;

	// 자정 도달 시 호출
	UPROPERTY(BlueprintAssignable, Category=TimeCycle)
	FTimeCycleDelegate OnMidnight;

private:
	void CacheTimeSettings();
	FTimeCycleContinuation MakeContinuation() const;
	void UpdateNumCycles(const FTimeCycleData& PreviousTimeCycle);

	UFUNCTION()
	void OnRep_TimeCycleData(const FTimeCycleData& OldTimeCycleData);

	// 시작 시간
	UPROPERTY(EditAnywhere, Category=TimeCycle)
	FTimeCycleCode InitialTime;

	// 최대 시간 사용 여부
	UPROPERTY(EditAnywhere, Category=TimeCycle)
	bool bHasMaxTime = false;

	// 최대 시간 (도달 시 정지)
	UPROPERTY(EditAnywhere, Category=TimeCycle, meta=(EditCondition="bHasMaxTime", EditConditionHides))
	FTimeCycleCode MaxTime;

	// 하루 사이클 소요 게임 시간
	UPROPERTY(EditAnywhere, Category=TimeCycle)
	FTimeCycleCode CycleDuration;

	// 일출 시간
	UPROPERTY(EditAnywhere, Category=TimeCycle)
	FTimeCycleCode SunRiseTime;

	// 일몰 시간
	UPROPERTY(EditAnywhere, Category=TimeCycle)
	FTimeCycleCode SunSetTime;

	UPROPERTY(ReplicatedUsing=OnRep_TimeCycleData)
	FTimeCycleData TimeCycleData;

	bool bHasVisualTimeOverride = false;
	FTimeCycleCode VisualTimeOverride;

	// 캐시된 초 단위 값
	int32 InitialTimeSeconds = 0;
	int32 MaxTimeSeconds = 0;

	int32 SunRiseSeconds = 0;
	int32 SunSetSeconds = 0;
	static constexpr int32 NoonSeconds = 43200;
	static constexpr int32 MidnightSeconds = 86400;
};
