#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/RankingData.h"
#include "RankingManagerSubsystem.generated.h"

class UPlayFabManagerSubsystem;
class USaveLoadManager;
struct FPlayFabLeaderboardEntry;

/**
 * 랭킹 & 도시 방문 매니저
 * - 누적 매출 기반 리더보드 업로드/조회
 * - 도시 스냅샷 업로드/다운로드
 * - 방문 모드 진입/퇴출
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URankingManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 리더보드 조회 완료 (TabIndex, Entries)
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLeaderboardLoaded, int32, const TArray<FRankingEntry>&);
	FOnLeaderboardLoaded OnLeaderboardLoaded;

	// 도시 스냅샷 조회 완료 (PlayFabId, Snapshot)
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCitySnapshotLoaded, const FString&, const FCitySnapshot&);
	FOnCitySnapshotLoaded OnCitySnapshotLoaded;

	// 에러 (ErrorMessage)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRankingError, const FString&);
	FOnRankingError OnRankingError;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ===== 매출 업로드 =====

	// 현재 시총을 "MarketCap" 리더보드에 업로드 (Last 모드)
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void UploadMarketCap(int64 MarketCapValue);

	// Money 증가분을 "WeeklyRevenue" 리더보드에 업로드 (Sum 모드, Weekly Reset)
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void UploadWeeklyRevenueDelta(int64 Delta);

	// 레거시 호환 — 내부에서 시총 현재값 + 프로필 업로드로 대체
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void UploadRevenueScore();

	// 쓰로틀 무시하고 즉시 업로드 (랭킹 패널 진입 시 사용)
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void ForceUploadRevenueScore();

	// ===== 리더보드 조회 =====

	// 상위 N명 조회 (TabIndex: 0=시총, 1=주간매출)
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void FetchLeaderboard(int32 TabIndex, int32 Count = 100);

	// 내 주변 순위 조회
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void FetchLeaderboardAroundPlayer(int32 TabIndex, int32 Count = 20);

	// 캐시된 리더보드
	UFUNCTION(BlueprintPure, Category = "Ranking")
	const TArray<FRankingEntry>& GetCachedLeaderboard() const { return CachedLeaderboard; }

	// ===== 도시 스냅샷 =====

	// 현재 도시 스냅샷을 PlayFab에 업로드 (쓰로틀 적용)
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void UploadCitySnapshot();

	// 대상 플레이어의 도시 스냅샷 다운로드
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void FetchCitySnapshot(const FString& TargetPlayFabId);

	// ===== 방문 모드 =====

	// 방문 모드 진입 (스냅샷 다운로드 → 맵 전환)
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void EnterVisitMode(const FString& TargetPlayFabId, const FString& TargetDisplayName);

	// 방문 모드 퇴출 (정상 MainMap 복귀)
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void ExitVisitMode();

	// ===== 프로필 =====

	// 랭킹 프로필 JSON 업로드
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void UploadRankingProfile();

private:
	UPROPERTY()
	UPlayFabManagerSubsystem* PlayFabMgr = nullptr;

	// 캐시된 리더보드 데이터
	TArray<FRankingEntry> CachedLeaderboard;

	// 업로드 쓰로틀 (최소 60초 간격)
	double LastScoreUploadTime = 0.0;
	double LastSnapshotUploadTime = 0.0;
	static constexpr double UploadThrottleSeconds = 60.0;

	// 주간매출 누산 버퍼 — Money 가 오를 때마다 HTTP 를 쏘면 오피스 31인 방치에서 초당 31건이 된다.
	// 리더보드가 Sum 모드라 합쳐 보내도 값이 동일해 배치화에 손실이 없다.
	int64 PendingWeeklyDelta = 0;
	double LastWeeklyFlushTime = 0.0;
	void FlushWeeklyRevenue(bool bForce);

	// 현재 조회 중인 탭 인덱스 (콜백에서 사용)
	int32 PendingTabIndex = 0;

	// 리더보드 통계 이름
	static const FString StatName_Weekly;
	static const FString StatName_AllTime;

	// PlayFab 콜백 바인딩/해제
	void BindPlayFabDelegates();
	void UnbindPlayFabDelegates();

	// PlayFab 콜백 핸들러
	void HandleLeaderboardResult(const FString& StatName, const TArray<FPlayFabLeaderboardEntry>& Entries);
	void HandleOtherPlayerData(const FString& PlayFabId, const TMap<FString, FString>& Data);
	void HandlePlayFabError(const FString& ErrorMsg);

	// 스냅샷 빌드 헬퍼
	FCitySnapshot BuildCitySnapshot() const;

	// RankingProfile JSON 직렬화/역직렬화
	FString SerializeRankingProfile() const;
	FRankingEntry ParseRankingProfile(const FString& JsonStr, const FPlayFabLeaderboardEntry& LeaderboardEntry) const;

	// 도시 스냅샷 다운로드 대기 (방문용)
	FString PendingVisitPlayFabId;
	FString PendingVisitDisplayName;
};
