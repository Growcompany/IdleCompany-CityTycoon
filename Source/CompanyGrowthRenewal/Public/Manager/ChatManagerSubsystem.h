// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ChatMessageData.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "ChatManagerSubsystem.generated.h"

class UPlayFabManagerSubsystem;

/**
 * 채팅 매니저 서브시스템
 * - Firebase Functions REST API를 통해 글로벌 채팅 처리
 * - HTTP 폴링 방식으로 새 메시지 수신 (기본 3초 간격)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UChatManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 새 메시지 수신 델리게이트
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessagesReceived, const TArray<FChatMessage>&);
	FOnChatMessagesReceived OnMessagesReceived;

	// 메시지 전송 결과 델리게이트
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatSendResult, bool);
	FOnChatSendResult OnSendResult;

	// 채팅 에러 델리게이트
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatError, const FString&);
	FOnChatError OnChatError;

	// 폴링 연속 실패 ↔ 복구. 상태가 바뀔 때만 방송 — 조용히 죽은 채팅을 UI에 드러내는 신호
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatConnectionChanged, bool);
	FOnChatConnectionChanged OnChatConnectionChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 메시지 전송
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SendMessage(const FString& Content, const FString& Channel = TEXT("global"));

	// 최근 메시지 수동 조회
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void FetchRecentMessages(const FString& Channel = TEXT("global"), int32 Limit = 30);

	// 폴링 시작. IntervalSeconds 미지정(-1) 시 ini PollIntervalSeconds 사용, 양수면 그 값 우선
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void StartPolling(float IntervalSeconds = -1.0f);

	// 폴링 중지
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void StopPolling();

	UFUNCTION(BlueprintPure, Category = "Chat")
	const TArray<FChatMessage>& GetCachedMessages() const { return CachedMessages; }

	UFUNCTION(BlueprintPure, Category = "Chat")
	bool IsPolling() const { return bIsPolling; }

	// 늦게 생성된 위젯이 현재 연결 상태를 즉시 반영할 수 있게
	UFUNCTION(BlueprintPure, Category = "Chat")
	bool IsConnectionHealthy() const { return bConnectionHealthy; }

private:
	// Firebase Functions 기본 URL (Config에서 로드)
	FString FirebaseBaseUrl;

	// 캐시된 메시지 목록
	UPROPERTY()
	TArray<FChatMessage> CachedMessages;

	// 마지막으로 받은 메시지의 createdAt (폴링 최적화)
	FString LastMessageTimestamp;

	// 폴링 상태
	bool bIsPolling = false;
	FTimerHandle PollingTimerHandle;
	FString CurrentPollingChannel = TEXT("global");
	bool bCaptureIsolationEvaluated = false;
	bool bSuppressHttpRequestsForCapture = false;
	bool bCaptureIsolationMarkerLogged = false;

	// 최대 캐시 메시지 수
	int32 MaxCachedMessages = 100;

	// ini에서 읽은 폴링 기본 간격 (StartPolling 인자 기본값으로 사용)
	float ConfigPollIntervalSeconds = 3.0f;

	// 폴링 연속 실패 집계. 일시적 끊김으로 경고가 튀지 않게 임계값을 두고, 상태 전이에만 방송
	int32 ConsecutiveFetchFailures = 0;
	bool bConnectionHealthy = true;
	static constexpr int32 FetchFailureThreshold = 3;

	// Firebase idToken 대기 로그를 폴링마다 찍지 않게 하는 1회 래치
	bool bUnauthenticatedLogged = false;

	// 401 재시도용 직전 전송 바디. 재시도의 재시도는 막는다

	// Config에서 Firebase URL 로드
	void LoadFirebaseConfig();

	// 인증 준비 상태면 idToken, 아니면 빈 문자열(경고 1회)
	FString AcquireIdTokenOrLogOnce();

	// 전송 요청 발사. 반환값은 사용한 URL
	FString DispatchSendRequest(const FString& RequestBody, bool bAllowRetry);

	// 서버 401 → 토큰 갱신/재교환 위임, 필요하면 1회 재전송 예약
	void HandleAuthRejection(const FString& ResponseBody, const FString& RetryBody, bool bAllowResend);

	// /chat/send 응답의 서버 createdAt 으로 폴링 커서를 전진. 로컬 시계는 커서에 쓰지 않는다
	void AdvanceCursorFromSendResponse(const FString& ResponseBody);

	// 폴링 타이머 콜백
	void PollMessages();
	bool ShouldSuppressHttpRequests();
	void ProcessChatHttpRequest(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& HttpRequest);

	// HTTP 응답 핸들러
	void OnSendMessageResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful,
		const FString& SentBody, bool bAllowResend);
	void OnFetchMessagesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// 폴링 성공/실패 집계
	void HandleFetchFailure();
	void HandleFetchSuccess();

	// JSON 응답에서 메시지 배열 파싱
	TArray<FChatMessage> ParseMessagesFromJson(const FString& JsonString);
};
