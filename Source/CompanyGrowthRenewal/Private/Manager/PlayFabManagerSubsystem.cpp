// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/PlayFabManagerSubsystem.h"
#include "Core/PlayFabClientAPI.h"
#include "Core/PlayFabClientDataModels.h"
#include "Core/PlayFabLeaderboardsAPI.h"
#include "Core/PlayFabLeaderboardsDataModels.h"
#include "Core/PlayFabError.h"
#include "TimerManager.h"
#include "Engine/World.h"

// LastSync 데이터 키
static const FString PlayFab_LastSyncKey = TEXT("LastSync");

void UPlayFabManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PlayFabClientPtr = MakeShareable(new PlayFab::UPlayFabClientAPI());
	LeaderboardsApiPtr = MakeShareable(new PlayFab::UPlayFabLeaderboardsAPI());

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 초기화 완료 (Client + Leaderboards v2)"));
}

void UPlayFabManagerSubsystem::Deinitialize()
{
	PlayFabClientPtr.Reset();
	LeaderboardsApiPtr.Reset();

	Super::Deinitialize();
}

void UPlayFabManagerSubsystem::LoginAsGuest()
{
	if (!PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayFabManager] ClientAPI가 초기화되지 않음"));
		return;
	}

	LoginState = EPlayFabLoginState::LoggingIn;

	// 디바이스 고유 ID를 CustomID로 사용 (에디터에서는 빈 값일 수 있으므로 폴백)
	FString DeviceId = FPlatformMisc::GetDeviceId();
	if (DeviceId.IsEmpty())
	{
		DeviceId = FPlatformMisc::GetLoginId();
	}
	if (DeviceId.IsEmpty())
	{
		DeviceId = TEXT("Editor_") + FPlatformMisc::GetLoginId();
	}

	PlayFab::ClientModels::FLoginWithCustomIDRequest Request;
	Request.CustomId = DeviceId;
	Request.CreateAccount = true;

	// 로그인 시 프로필(DisplayName) 함께 요청
	auto InfoParams = MakeShared<PlayFab::ClientModels::FGetPlayerCombinedInfoRequestParams>();
	InfoParams->GetPlayerProfile = true;
	InfoParams->ProfileConstraints = MakeShared<PlayFab::ClientModels::FPlayerProfileViewConstraints>();
	InfoParams->ProfileConstraints->ShowDisplayName = true;
	Request.InfoRequestParameters = InfoParams;

	PlayFabClientPtr->LoginWithCustomID(
		Request,
		PlayFab::UPlayFabClientAPI::FLoginWithCustomIDDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnLoginSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnLoginError)
	);

	CurrentUser.LoginType = EPlayFabLoginType::Guest;

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 게스트 로그인 요청 (DeviceId: %s)"), *DeviceId);
}

void UPlayFabManagerSubsystem::LoginWithGoogle(const FString& ServerAuthCode)
{
	if (!PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayFabManager] ClientAPI가 초기화되지 않음"));
		return;
	}

	if (ServerAuthCode.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayFabManager] ServerAuthCode가 비어있음"));
		return;
	}

	LoginState = EPlayFabLoginState::LoggingIn;

	PlayFab::ClientModels::FLoginWithGoogleAccountRequest Request;
	Request.ServerAuthCode = ServerAuthCode;
	Request.CreateAccount = true;

	auto InfoParams = MakeShared<PlayFab::ClientModels::FGetPlayerCombinedInfoRequestParams>();
	InfoParams->GetPlayerProfile = true;
	InfoParams->ProfileConstraints = MakeShared<PlayFab::ClientModels::FPlayerProfileViewConstraints>();
	InfoParams->ProfileConstraints->ShowDisplayName = true;
	Request.InfoRequestParameters = InfoParams;

	PlayFabClientPtr->LoginWithGoogleAccount(
		Request,
		PlayFab::UPlayFabClientAPI::FLoginWithGoogleAccountDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnLoginSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnLoginError)
	);

	CurrentUser.LoginType = EPlayFabLoginType::Google;

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 구글 로그인 요청"));
}

void UPlayFabManagerSubsystem::Logout()
{
	// 로그아웃 직전 마지막 LastSync 저장 (다음 로그인 시 오프라인 계산 기준점)
	if (IsLoggedIn())
	{
		const int64 NowUtc = FDateTime::UtcNow().ToUnixTimestamp();
		UpdateLastSync(NowUtc);
	}

	StopHeartbeat();

	CurrentUser = FPlayFabUserInfo();
	LoginState = EPlayFabLoginState::NotLoggedIn;
	EntityId.Empty();
	EntityType.Empty();
	PendingTimeRequest = EServerTimeRequestType::None;
	bAwaitingLastSyncResponse = false;

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 로그아웃 완료"));
}

void UPlayFabManagerSubsystem::UpdateDisplayName(const FString& NewDisplayName)
{
	if (!IsLoggedIn() || !PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 로그인 상태가 아니므로 디스플레이 이름 변경 불가"));
		return;
	}

	PlayFab::ClientModels::FUpdateUserTitleDisplayNameRequest Request;
	Request.DisplayName = NewDisplayName;

	PlayFabClientPtr->UpdateUserTitleDisplayName(
		Request,
		PlayFab::UPlayFabClientAPI::FUpdateUserTitleDisplayNameDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnDisplayNameUpdateSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);
}

void UPlayFabManagerSubsystem::SavePlayerData(const FString& Key, const FString& Value)
{
	if (!IsLoggedIn() || !PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 로그인 상태가 아니므로 데이터 저장 불가"));
		return;
	}

	PlayFab::ClientModels::FUpdateUserDataRequest Request;
	Request.Data.Add(Key, Value);
	Request.Permission = PlayFab::ClientModels::UserDataPermission::UserDataPermissionPublic;

	// SDK 규약: 성공·실패 델리게이트 쌍. 결과는 콜백으로만
	PlayFabClientPtr->UpdateUserData(
		Request,
		PlayFab::UPlayFabClientAPI::FUpdateUserDataDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnDataUpdateSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 데이터 저장 요청 (Key: %s)"), *Key);
}

void UPlayFabManagerSubsystem::LoadPlayerData(const FString& Key)
{
	if (!IsLoggedIn() || !PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 로그인 상태가 아니므로 데이터 로드 불가"));
		return;
	}

	PlayFab::ClientModels::FGetUserDataRequest Request;
	Request.Keys.Add(Key);

	PlayFabClientPtr->GetUserData(
		Request,
		PlayFab::UPlayFabClientAPI::FGetUserDataDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnDataGetSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 데이터 로드 요청 (Key: %s)"), *Key);
}

bool UPlayFabManagerSubsystem::IsLoggedIn() const
{
	return LoginState == EPlayFabLoginState::LoggedIn && CurrentUser.IsValid();
}

// ===== Private 콜백 =====

void UPlayFabManagerSubsystem::OnLoginSuccess(const PlayFab::ClientModels::FLoginResult& Result)
{
	CurrentUser.PlayFabId = Result.PlayFabId;
	CurrentUser.SessionTicket = Result.SessionTicket;
	LoginState = EPlayFabLoginState::LoggedIn;

	// Entity 정보 저장 (v2 API용)
	if (Result.EntityToken.IsValid())
	{
		if (Result.EntityToken->Entity.IsValid())
		{
			EntityId = Result.EntityToken->Entity->Id;
			EntityType = Result.EntityToken->Entity->Type;
			UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] Entity 정보: ID=%s, Type=%s"), *EntityId, *EntityType);
		}
	}

	// 응답에서 DisplayName 추출
	if (Result.InfoResultPayload.IsValid()
		&& Result.InfoResultPayload->PlayerProfile.IsValid())
	{
		CurrentUser.DisplayName = Result.InfoResultPayload->PlayerProfile->DisplayName;
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 로그인 성공 (PlayFabId: %s, DisplayName: %s, NewlyCreated: %s)"),
		*Result.PlayFabId, *CurrentUser.DisplayName, Result.NewlyCreated ? TEXT("Yes") : TEXT("No"));

	// 신규 유저면 랜덤 닉네임 자동 부여
	if (Result.NewlyCreated)
	{
		const int32 RandomNum = FMath::RandRange(1000, 9999);
		const FString AutoName = FString::Printf(TEXT("Player_%d"), RandomNum);
		UpdateDisplayName(AutoName);
	}

	OnLoginComplete.Broadcast(true);

	// 로그인 직후 오프라인 보상 시퀀스 시작 + 10분 주기 하트비트 작동
	// 신규 유저라도 LastSync 미존재 → OfflineSeconds=0 처리 (OnDataGetSuccess 에서 분기)
	StartOfflineGainsSequence();
	StartHeartbeat();
}

void UPlayFabManagerSubsystem::OnLoginError(const PlayFab::FPlayFabCppError& Error)
{
	LoginState = EPlayFabLoginState::Failed;

	const FString ErrorMsg = Error.GenerateErrorReport();
	UE_LOG(LogTemp, Error, TEXT("[PlayFabManager] 로그인 실패: %s"), *ErrorMsg);

	OnLoginComplete.Broadcast(false);
	OnError.Broadcast(ErrorMsg);
}

void UPlayFabManagerSubsystem::OnDataGetSuccess(const PlayFab::ClientModels::FGetUserDataResult& Result)
{
	// 오프라인 시퀀스에서 LastSync 를 요청한 응답이면 별도 처리 후 조기 리턴
	// (일반 LoadPlayerData 브로드캐스트와 섞이지 않도록 플래그 기반 구분)
	if (bAwaitingLastSyncResponse)
	{
		bAwaitingLastSyncResponse = false;

		const int64 ServerNow = PendingServerNow;
		PendingServerNow = 0;

		int64 LastSync = 0;
		if (const auto* Found = Result.Data.Find(PlayFab_LastSyncKey))
		{
			LexFromString(LastSync, *Found->Value);
		}

		// 최초 로그인/데이터 누락 시 OfflineSeconds=0 (보상 없음)
		int64 OfflineSeconds = 0;
		if (LastSync > 0 && ServerNow > LastSync)
		{
			// 서버 시간 기준 경과, 캡으로 무한 누적 차단
			OfflineSeconds = FMath::Min<int64>(ServerNow - LastSync, OfflineCapSeconds);
		}

		UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 오프라인 보상 계산: ServerNow=%lld, LastSync=%lld, OfflineSeconds=%lld (cap=%lld)"),
			ServerNow, LastSync, OfflineSeconds, OfflineCapSeconds);

		// 여기선 경과 초만 전달. 정산·지급은 SaveLoadManager
		OnOfflineGainsRequested.Broadcast(static_cast<float>(OfflineSeconds));

		// 보상 브로드캐스트 직후 LastSync 갱신 → 다음 시퀀스 기준점 확정
		UpdateLastSync(ServerNow);
		return;
	}

	// 일반 LoadPlayerData 응답
	for (const auto& Pair : Result.Data)
	{
		const FString& Key = Pair.Key;
		const FString& Value = Pair.Value.Value;

		UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 데이터 로드 성공 (Key: %s)"), *Key);
		OnDataLoaded.Broadcast(Key, Value);
	}
}

void UPlayFabManagerSubsystem::OnDataUpdateSuccess(const PlayFab::ClientModels::FUpdateUserDataResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 데이터 저장 성공 (Version: %d)"), Result.DataVersion);
}

void UPlayFabManagerSubsystem::OnDisplayNameUpdateSuccess(const PlayFab::ClientModels::FUpdateUserTitleDisplayNameResult& Result)
{
	CurrentUser.DisplayName = Result.DisplayName;

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 디스플레이 이름 변경 성공: %s"), *Result.DisplayName);
}

void UPlayFabManagerSubsystem::OnApiError(const PlayFab::FPlayFabCppError& Error)
{
	const FString ErrorMsg = Error.GenerateErrorReport();
	UE_LOG(LogTemp, Error, TEXT("[PlayFabManager] API 에러: %s"), *ErrorMsg);

	// 모든 PlayFab API 에러가 이 핸들러로 모이므로, 시간 시퀀스가 진행 중이었다면 보수적으로 리셋.
	// stuck 시 다음 LoadPlayerData 응답이 LastSync 시퀀스로 오인되어 잘못된 시간으로 덮어쓰는 것을 방지.
	if (PendingTimeRequest != EServerTimeRequestType::None || bAwaitingLastSyncResponse)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] API 에러로 오프라인 시퀀스 리셋 (PendingType=%d, bAwaiting=%d)"),
			(int32)PendingTimeRequest, bAwaitingLastSyncResponse);
		PendingTimeRequest = EServerTimeRequestType::None;
		bAwaitingLastSyncResponse = false;
		PendingServerNow = 0;
	}

	OnError.Broadcast(ErrorMsg);
}

// ===== Leaderboards v2 API =====

void UPlayFabManagerSubsystem::UpdateLeaderboardScore(const FString& LeaderboardName, int64 Score)
{
	if (!IsLoggedIn() || !PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 리더보드 업데이트 불가 (로그인=%d)"), IsLoggedIn());
		return;
	}

	// Classic UpdatePlayerStatistics로 업로드 (클라이언트 허용됨)
	// v2 Leaderboard에서 "Linked statistic"으로 연결하면 자동 반영
	PlayFab::ClientModels::FUpdatePlayerStatisticsRequest Request;

	PlayFab::ClientModels::FStatisticUpdate StatUpdate;
	StatUpdate.StatisticName = LeaderboardName;
	StatUpdate.Value = static_cast<int32>(FMath::Clamp(Score, (int64)0, (int64)INT32_MAX));
	Request.Statistics.Add(StatUpdate);

	PlayFabClientPtr->UpdatePlayerStatistics(
		Request,
		PlayFab::UPlayFabClientAPI::FUpdatePlayerStatisticsDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnUpdateLeaderboardSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] Classic 통계 업데이트 요청 (Name: %s, Score: %lld)"), *LeaderboardName, Score);
}

void UPlayFabManagerSubsystem::GetLeaderboard(const FString& LeaderboardName, int32 PageSize)
{
	if (!IsLoggedIn() || !PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 리더보드 조회 불가"));
		return;
	}

	PendingLeaderboardName = LeaderboardName;

	PlayFab::ClientModels::FGetLeaderboardRequest Request;
	Request.StatisticName = LeaderboardName;
	Request.StartPosition = 0;
	Request.MaxResultsCount = PageSize;

	PlayFabClientPtr->GetLeaderboard(
		Request,
		PlayFab::UPlayFabClientAPI::FGetLeaderboardDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnGetLeaderboardSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] Classic 리더보드 조회 요청 (Name: %s, Max: %d)"), *LeaderboardName, PageSize);
}

void UPlayFabManagerSubsystem::GetLeaderboardAroundPlayer(const FString& LeaderboardName, int32 MaxResults)
{
	if (!IsLoggedIn() || !PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 리더보드 조회 불가"));
		return;
	}

	PendingLeaderboardName = LeaderboardName;

	PlayFab::ClientModels::FGetLeaderboardAroundPlayerRequest Request;
	Request.StatisticName = LeaderboardName;
	Request.MaxResultsCount = MaxResults;

	PlayFabClientPtr->GetLeaderboardAroundPlayer(
		Request,
		PlayFab::UPlayFabClientAPI::FGetLeaderboardAroundPlayerDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnGetLeaderboardAroundSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] Classic 내 주변 리더보드 조회 요청 (Name: %s)"), *LeaderboardName);
}

void UPlayFabManagerSubsystem::GetOtherPlayerData(const FString& PlayFabId, const TArray<FString>& Keys)
{
	if (!IsLoggedIn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 로그인 상태가 아니므로 타 플레이어 데이터 조회 불가"));
		return;
	}

	PlayFab::ClientModels::FGetUserDataRequest Request;
	Request.PlayFabId = PlayFabId;
	Request.Keys = Keys;

	// 각 요청마다 PlayFabId를 캡처하는 람다 사용 (공유 변수 경합 방지)
	PlayFabClientPtr->GetUserData(
		Request,
		PlayFab::UPlayFabClientAPI::FGetUserDataDelegate::CreateLambda(
			[this, CapturedId = PlayFabId](const PlayFab::ClientModels::FGetUserDataResult& Result)
			{
				TMap<FString, FString> DataMap;
				for (const auto& Pair : Result.Data)
				{
					DataMap.Add(Pair.Key, Pair.Value.Value);
				}
				UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 타 플레이어 데이터 로드 성공 (ID: %s, Keys: %d)"),
					*CapturedId, DataMap.Num());
				OnOtherPlayerDataLoaded.Broadcast(CapturedId, DataMap);
			}),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 타 플레이어 데이터 조회 요청 (ID: %s)"), *PlayFabId);
}

// ===== Leaderboards v2 콜백 =====

void UPlayFabManagerSubsystem::OnUpdateLeaderboardSuccess(const PlayFab::ClientModels::FUpdatePlayerStatisticsResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 통계 업데이트 성공 (Linked → v2 리더보드 자동 반영)"));
}

void UPlayFabManagerSubsystem::OnGetLeaderboardSuccess(const PlayFab::ClientModels::FGetLeaderboardResult& Result)
{
	TArray<FPlayFabLeaderboardEntry> Entries;
	Entries.Reserve(Result.Leaderboard.Num());

	for (const auto& Item : Result.Leaderboard)
	{
		FPlayFabLeaderboardEntry Entry;
		Entry.EntityId = Item.PlayFabId;
		Entry.DisplayName = Item.DisplayName.IsEmpty() ? TEXT("???") : Item.DisplayName;
		Entry.Rank = Item.Position + 1;  // 0-based → 1-based
		Entry.Score = Item.StatValue;
		Entries.Add(MoveTemp(Entry));
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] Classic 리더보드 조회 성공 (%d명)"), Entries.Num());
	OnLeaderboardResult.Broadcast(PendingLeaderboardName, Entries);
}

void UPlayFabManagerSubsystem::OnGetLeaderboardAroundSuccess(const PlayFab::ClientModels::FGetLeaderboardAroundPlayerResult& Result)
{
	TArray<FPlayFabLeaderboardEntry> Entries;
	Entries.Reserve(Result.Leaderboard.Num());

	for (const auto& Item : Result.Leaderboard)
	{
		FPlayFabLeaderboardEntry Entry;
		Entry.EntityId = Item.PlayFabId;
		Entry.DisplayName = Item.DisplayName.IsEmpty() ? TEXT("???") : Item.DisplayName;
		Entry.Rank = Item.Position + 1;  // 0-based → 1-based
		Entry.Score = Item.StatValue;
		Entries.Add(MoveTemp(Entry));
	}

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] Classic 내 주변 리더보드 조회 성공 (%d명)"), Entries.Num());
	OnLeaderboardResult.Broadcast(PendingLeaderboardName, Entries);
}

// OnOtherPlayerDataGetSuccess는 람다로 대체됨 (GetOtherPlayerData 내부)

// ===== 서버 시간 & 오프라인 보상 =====

void UPlayFabManagerSubsystem::RequestServerTime()
{
	if (!IsLoggedIn() || !PlayFabClientPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 서버 시간 요청 불가 (로그인 상태 아님)"));
		PendingTimeRequest = EServerTimeRequestType::None;
		return;
	}

	PlayFab::ClientModels::FGetTimeRequest Request;

	PlayFabClientPtr->GetTime(
		Request,
		PlayFab::UPlayFabClientAPI::FGetTimeDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnGetTimeSuccess),
		PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabManagerSubsystem::OnApiError)
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 서버 시간 요청 (Type=%d)"), (int32)PendingTimeRequest);
}

void UPlayFabManagerSubsystem::OnGetTimeSuccess(const PlayFab::ClientModels::FGetTimeResult& Result)
{
	const int64 ServerUnix = Result.Time.ToUnixTimestamp();

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 서버 시간 수신: %lld (Type=%d)"), ServerUnix, (int32)PendingTimeRequest);

	OnServerTimeReceived.Broadcast(ServerUnix);

	// 요청 타입별 분기 → 동일 GetTime 콜백 재사용 (상태머신 패턴)
	const EServerTimeRequestType Handled = PendingTimeRequest;
	// 분기 전 요청 타입 초기화 (후속 호출의 새 요청과 충돌 방지)
	PendingTimeRequest = EServerTimeRequestType::None;

	switch (Handled)
	{
	case EServerTimeRequestType::Login:
		// 로그인: 서버 시간 확보 → LastSync 조회 → OnDataGetSuccess에서 보상 계산
		PendingServerNow = ServerUnix;
		bAwaitingLastSyncResponse = true;
		LoadPlayerData(PlayFab_LastSyncKey);
		break;

	case EServerTimeRequestType::Heartbeat:
		// 하트비트: 갱신된 서버 시간을 LastSync 에 즉시 덮어쓰기 (크래시 시 오차 최대 10분)
		UpdateLastSync(ServerUnix);
		break;

	default:
		// 순수 시간 조회 (외부 호출자가 OnServerTimeReceived 만 받으려는 경우)
		break;
	}
}

void UPlayFabManagerSubsystem::UpdateLastSync(int64 ServerUtcTimestamp)
{
	const FString ValueStr = LexToString(ServerUtcTimestamp);
	SavePlayerData(PlayFab_LastSyncKey, ValueStr);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] LastSync 갱신: %lld"), ServerUtcTimestamp);
}

void UPlayFabManagerSubsystem::StartHeartbeat()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 하트비트 시작 실패: World 없음"));
		return;
	}

	// 중복 등록 방지
	World->GetTimerManager().ClearTimer(HeartbeatTimerHandle);

	World->GetTimerManager().SetTimer(
		HeartbeatTimerHandle,
		this,
		&UPlayFabManagerSubsystem::OnHeartbeatTick,
		HeartbeatIntervalSeconds,
		true  // Loop
	);

	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 하트비트 시작 (주기: %.0f초)"), HeartbeatIntervalSeconds);
}

void UPlayFabManagerSubsystem::StopHeartbeat()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
	}
	UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 하트비트 중지"));
}

void UPlayFabManagerSubsystem::OnHeartbeatTick()
{
	if (!IsLoggedIn())
	{
		StopHeartbeat();
		return;
	}

	// Login 시퀀스와 겹치면 Login 우선 (보상 계산 중 LastSync 덮어쓰기 방지)
	if (PendingTimeRequest != EServerTimeRequestType::None || bAwaitingLastSyncResponse)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayFabManager] 하트비트 스킵 (시퀀스 진행 중)"));
		return;
	}

	PendingTimeRequest = EServerTimeRequestType::Heartbeat;
	RequestServerTime();
}

void UPlayFabManagerSubsystem::StartOfflineGainsSequence()
{
	if (!IsLoggedIn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayFabManager] 오프라인 시퀀스 불가 (로그인 상태 아님)"));
		return;
	}

	PendingTimeRequest = EServerTimeRequestType::Login;
	RequestServerTime();
}
