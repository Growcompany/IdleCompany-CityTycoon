#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/TrendState.h"
#include "TrendManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrendChanged, ECompanyType, Industry, FName, NewMaterial);

/**
 * 산업별 트렌드(소재 유행) 관리 — 착수 3회마다 교체, 매칭 출시 = 운영 피크 x1.5.
 * 상태는 세이브(FGameSaveData.TrendStates)와 싱크. 스펙: specs/2026-07-02-gds-light-loop-design.md §3
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTrendManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 2026-07-30: 1.5 → 1.25. BaseRevenuePerSecond 에 그대로 곱해져 회수율을 통째로 1.5배 올린다
	// (ProjectOperationManager.cpp:125,138). 1.5 면 B궁합 C등급이 2.35 가 되어
	// "C 는 흑자지만 다음 판을 못 채운다"는 반복 설계가 무너진다. 1.25 면 1.96 으로 2.0 아래 유지.
	// 매칭 인센티브 +25% 는 등급 한 칸 상승분(+128%)보다 작아 등급 축을 침범하지 않는다.
	static constexpr float TrendPeakMult = 1.25f;
	static constexpr int32 LaunchesPerTrend = 3;

	UFUNCTION(BlueprintCallable, Category = "Trend")
	FName GetTrendMaterial(ECompanyType Industry);

	UFUNCTION(BlueprintCallable, Category = "Trend")
	int32 GetLaunchesLeft(ECompanyType Industry);

	UFUNCTION(BlueprintCallable, Category = "Trend")
	bool IsTrendMaterial(ECompanyType Industry, FName Material);

	// 착수 시 호출 (스펙런치 경로 단일 소비점) — 차감, 0이면 로테이션+OnTrendChanged
	UFUNCTION(BlueprintCallable, Category = "Trend")
	void NotifyProjectLaunched(ECompanyType Industry);

	// 갱신 스크롤 — 현재 소재를 제외하고 재추첨. 풀이 1개 이하면 false. 카운터는 LaunchesPerTrend 로 리셋.
	UFUNCTION(BlueprintCallable, Category = "Trend")
	bool RerollTrend(ECompanyType Industry);

	static FName PickTrendFromPool(const TArray<FName>& Pool, FName Exclude, int32 RandomIndex);

	UPROPERTY(BlueprintAssignable, Category = "Trend")
	FOnTrendChanged OnTrendChanged;

private:
	// 세이브가 서브시스템 Initialize보다 늦게 로드되므로 첫 접근 시 lazy 로드
	void EnsureLoaded();
	FTrendState& EnsureTrend(ECompanyType Industry);
	void RotateTrend(ECompanyType Industry, FTrendState& State);
	void SyncToSave();

	TMap<ECompanyType, FTrendState> States;
	bool bLoaded = false;
};
