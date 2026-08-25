// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/PlayFabUserData.h"
#include "PlayFabManagerSubsystem.generated.h"

namespace PlayFab
{
	class UPlayFabClientAPI;
	class UPlayFabLeaderboardsAPI;

	namespace ClientModels
	{
		struct FLoginResult;
		struct FGetUserDataResult;
		struct FUpdateUserDataResult;
		struct FUpdateUserTitleDisplayNameResult;
		struct FUpdatePlayerStatisticsResult;
		struct FGetLeaderboardResult;
		struct FGetLeaderboardAroundPlayerResult;
		struct FPlayerLeaderboardEntry;
		struct FGetTimeResult;
	}

	namespace LeaderboardsModels
	{
		struct FEmptyResponse;
		struct FGetEntityLeaderboardResponse;
	}

	struct FPlayFabCppError;
}

// 리더보드 항목 (PlayFab → 게임)
USTRUCT(BlueprintType)
struct FPlayFabLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString EntityId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly)
	int64 Score = 0;
};

// TMap 매크로 쉼표 문제 회피용 typedef
typedef TMap<FString, FString> FStringStringMap;

/**
 * PlayFab 연동 매니저
 * - 게스트/구글 로그인, 유저 데이터 저장/로드, 디스플레이 이름 변경
 * - Leaderboards v2 API로 리더보드 관리
 * - 오프라인 플레이 미지원 — 로그인 필수
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPlayFabManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 오프라인 최대 적립 시간(12시간) — SaveLoadManager 의 캡 도달 판정이 같은 값을 읽어야 해서 public
	static constexpr int64 OfflineCapSeconds = 12 * 60 * 60;

	// 로그인 결과 델리게이트 (bSuccess)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayFabLoginComplete, bool);
	FOnPlayFabLoginComplete OnLoginComplete;

	// 데이터 로드 결과 델리게이트 (Key, Value)
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayFabDataLoaded, const FString&, const FString&);
	FOnPlayFabDataLoaded OnDataLoaded;

	// 에러 델리게이트 (ErrorMessage)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayFabError, const FString&);
	FOnPlayFabError OnError;

	// 리더보드 조회 결과 (LeaderboardName, Entries)
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLeaderboardResult, const FString&, const TArray<FPlayFabLeaderboardEntry>&);
	FOnLeaderboardResult OnLeaderboardResult;

	// 타 플레이어 데이터 조회 결과 (PlayFabId, Key-Value 맵)
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOtherPlayerDataLoaded, const FString&, const FStringStringMap&);
	FOnOtherPlayerDataLoaded OnOtherPlayerDataLoaded;

	// 서버 시간 조회 결과 (UtcUnixTimestamp). 치팅 방지용 서버 권위 시간.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayFabServerTimeReceived, int64);
	FOnPlayFabServerTimeReceived OnServerTimeReceived;

	// 오프라인 보상 계산 요청 이벤트 (OfflineSeconds). USaveLoadManager 가 리스닝해서 실 계산 수행.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayFabOfflineGainsRequested, float);
	FOnPlayFabOfflineGainsRequested OnOfflineGainsRequested;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 게스트 로그인 (디바이스 ID 기반 CustomID)
	UFUNCTION(BlueprintCallable, Category = "PlayFab")
	void LoginAsGuest();

	// 구글 계정 로그인
	UFUNCTION(BlueprintCallable, Category = "PlayFab")
	void LoginWithGoogle(const FString& ServerAuthCode);

	// 로그아웃 (상태 초기화)
	UFUNCTION(BlueprintCallable, Category = "PlayFab")
	void Logout();

	// 디스플레이 이름 변경
	UFUNCTION(BlueprintCallable, Category = "PlayFab")
	void UpdateDisplayName(const FString& NewDisplayName);

	// 서버에 유저 데이터 저장
	UFUNCTION(BlueprintCallable, Category = "PlayFab")
	void SavePlayerData(const FString& Key, const FString& Value);

	// 서버에서 유저 데이터 로드
	UFUNCTION(BlueprintCallable, Category = "PlayFab")
	void LoadPlayerData(const FString& Key);

	UFUNCTION(BlueprintPure, Category = "PlayFab")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintPure, Category = "PlayFab")
	const FPlayFabUserInfo& GetUserInfo() const { return CurrentUser; }

	UFUNCTION(BlueprintPure, Category = "PlayFab")
	EPlayFabLoginState GetLoginState() const { return LoginState; }

	// ===== Leaderboards v2 API =====

	// 리더보드에 점수 업로드
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Leaderboard")
	void UpdateLeaderboardScore(const FString& LeaderboardName, int64 Score);

	// 리더보드 상위 N명 조회
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Leaderboard")
	void GetLeaderboard(const FString& LeaderboardName, int32 PageSize);

	// 현재 플레이어 주변 리더보드 조회
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Leaderboard")
	void GetLeaderboardAroundPlayer(const FString& LeaderboardName, int32 MaxResults);

	// ===== 타 플레이어 데이터 조회 =====

	// 다른 플레이어의 공개 데이터 조회
	UFUNCTION(BlueprintCallable, Category = "PlayFab|PlayerData")
	void GetOtherPlayerData(const FString& PlayFabId, const TArray<FString>& Keys);

	// Entity ID (v2 API에서 사용)
	UFUNCTION(BlueprintPure, Category = "PlayFab")
	const FString& GetEntityId() const { return EntityId; }

	UFUNCTION(BlueprintPure, Category = "PlayFab")
	const FString& GetEntityType() const { return EntityType; }

	// ===== 서버 시간 & 오프라인 보상 (치팅 방지 기반) =====

	// 서버 권위 시간 조회 (PlayFab GetTime). 완료 시 OnServerTimeReceived 브로드캐스트.
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Time")
	void RequestServerTime();

	// LastSync(마지막 동기화) 키에 현재 서버 시간 저장. 오프라인 시작/끝 기준점.
	void UpdateLastSync(int64 ServerUtcTimestamp);

	// 하트비트 시작/중지 — 10분 주기로 LastSync 갱신 (앱 크래시 시 오차 제한)
	void StartHeartbeat();
	void StopHeartbeat();

	// 오프라인 보상 시퀀스 트리거 (로그인 직후 1회 호출) — 서버 시간 조회 → LastSync 로드 → OfflineSeconds 계산 → OnOfflineGainsRequested 브로드캐스트 → LastSync 갱신
	void StartOfflineGainsSequence();

private:
	TSharedPtr<PlayFab::UPlayFabClientAPI> PlayFabClientPtr;
	TSharedPtr<PlayFab::UPlayFabLeaderboardsAPI> LeaderboardsApiPtr;

	FPlayFabUserInfo CurrentUser;
	EPlayFabLoginState LoginState = EPlayFabLoginState::NotLoggedIn;

	// Entity 정보 (v2 API용, 로그인 시 저장)
	FString EntityId;
	FString EntityType;

	// 로그인 성공/실패 공통 처리
	void OnLoginSuccess(const PlayFab::ClientModels::FLoginResult& Result);
	void OnLoginError(const PlayFab::FPlayFabCppError& Error);

	// 데이터 로드/저장 콜백
	void OnDataGetSuccess(const PlayFab::ClientModels::FGetUserDataResult& Result);
	void OnDataUpdateSuccess(const PlayFab::ClientModels::FUpdateUserDataResult& Result);

	// 디스플레이 이름 변경 콜백
	void OnDisplayNameUpdateSuccess(const PlayFab::ClientModels::FUpdateUserTitleDisplayNameResult& Result);

	// 범용 API 에러 콜백
	void OnApiError(const PlayFab::FPlayFabCppError& Error);

	// Classic API 콜백
	void OnUpdateLeaderboardSuccess(const PlayFab::ClientModels::FUpdatePlayerStatisticsResult& Result);
	void OnGetLeaderboardSuccess(const PlayFab::ClientModels::FGetLeaderboardResult& Result);
	void OnGetLeaderboardAroundSuccess(const PlayFab::ClientModels::FGetLeaderboardAroundPlayerResult& Result);

	// 현재 조회 중인 리더보드 이름 (콜백에서 사용)
	FString PendingLeaderboardName;

	// ===== 오프라인 보상 내부 상태 =====
	// GetTime 결과를 어디에 쓸지 구분 (로그인 직후 시퀀스 vs 하트비트)
	enum class EServerTimeRequestType : uint8 { None, Login, Heartbeat };
	EServerTimeRequestType PendingTimeRequest = EServerTimeRequestType::None;

	// 로그인 시퀀스에서 GetTime 결과 임시 저장 → LastSync 조회 완료 시점에 사용
	int64 PendingServerNow = 0;

	// LoadPlayerData 응답이 오프라인 시퀀스의 LastSync 인지 구분
	bool bAwaitingLastSyncResponse = false;

	// 하트비트 10분 주기 타이머
	FTimerHandle HeartbeatTimerHandle;
	static constexpr float HeartbeatIntervalSeconds = 600.0f;

	void OnGetTimeSuccess(const PlayFab::ClientModels::FGetTimeResult& Result);
	void OnHeartbeatTick();
};
