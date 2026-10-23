// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CloudAuthRetryState.h"

/** 401 로 튕긴 요청의 후처리. 복구되면 true, 재인증을 포기하면 false 로 **정확히 1회** 호출된다 */
using FAuthRetryCallback = TFunction<void(bool /*bRecovered*/)>;

#include "FirebaseAuthSubsystem.generated.h"

/**
 * Firebase 인증 매니저 (과도기 ― 설계서 §3.1)
 * - PlayFab SessionTicket → 서버 `/auth/exchange` → customToken → Firebase idToken
 * - idToken 수명 관리(50분 선갱신)와 401 복구 경로를 한곳에 모은다
 * - 서버를 부르는 모든 매니저는 `GetIdToken()` 으로 Bearer 를 얻는다
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UFirebaseAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 인증 준비/해제. 1-B 경제 매니저가 이 신호를 기다린다
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFirebaseAuthReady, bool);
	FOnFirebaseAuthReady OnAuthReady;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// PlayFab 로그인 완료 후 호출 (CGGameInstance 가 배선)
	void Exchange();

	UFUNCTION(BlueprintPure, Category = "Auth")
	bool IsAuthReady() const { return bAuthReady; }

	const FString& GetIdToken() const { return IdToken; }
	const FString& GetPid() const { return Pid; }

	/**
	 * 서버 401 을 받은 호출자가 알린다. AUTH_EXPIRED=갱신, 그 외=재교환.
	 * 반환값 true = RetryOnce 가 예약됐다(나중에 true/false 로 호출됨). false = 지금 즉시 호출자가 실패 처리해야 한다.
	 */
	bool NotifyAuthRejected(const FString& Code, FAuthRetryCallback RetryOnce = nullptr);

	/** 인증이 붙은 요청이 실제로 성공했다. **여기서만** 재인증 거절 카운터가 리셋된다(무한 루프 차단) */
	void NotifyAuthAccepted();

	/** 백오프 잠금 여부. true 면 교환·갱신·요청을 모두 중단한 상태다 */
	UFUNCTION(BlueprintPure, Category = "Auth")
	bool IsAuthBackoffLocked() const { return RetryState.bBackoffLocked; }

	/** 포그라운드 복귀 등 사용자 계기로만 잠금을 푼다(설계서 §3.2). 자동 해제는 루프를 되살린다 */
	void ClearAuthBackoff();

	// 포그라운드 복귀 구독 핸들 ― Deinitialize 에서 해제한다.
	FDelegateHandle ForegroundHandle;

private:
	void SignInWithCustomToken(const FString& CustomToken);
	void RefreshIdToken();
	void ApplyTokens(const FString& InIdToken, const FString& InRefreshToken, int64 ExpiresInSeconds);
	void HandleAuthFailure(const FString& Code);
	void LockAuth(const FString& Reason);
	void FlushPendingRetries(bool bRecovered);
	// bIgnoreBackoffLock=true 는 잠금 해제 경로용 ― 잠금 자체를 억제 사유에서 빼야 ClearAuthBackoff 가 도달한다.
	bool ShouldSuppressRequests(bool bIgnoreBackoffLock = false);

	FString WebApiKey;
	FString IdToken;
	FString RefreshToken;
	FString Pid;
	FString DisplayName;

	bool bAuthReady = false;
	bool bRequestInFlight = false;

	// 교환·갱신 연속 실패. 임계값에 닿으면 OnAuthReady(false) 로 경제 액션을 잠근다(설계서 §3.2)
	int32 ConsecutiveFailures = 0;
	static constexpr int32 FailureThreshold = 2;

	// 401 로 튕긴 요청의 1회 재시도. 폴링이 쌓아 올리지 않게 상한을 둔다
	TArray<FAuthRetryCallback> PendingRetries;
	static constexpr int32 MaxPendingRetries = 4;

	// 재인증 직후의 401 전용 카운터(보안 리뷰 낮음13). 토큰 재발급으로 리셋되지 않는다
	FCloudAuthRetryState RetryState;

	FTimerHandle RefreshTimerHandle;

	bool bCaptureIsolationEvaluated = false;
	bool bSuppressHttpRequestsForCapture = false;
};
