// Fill out your copyright notice in the Description page of Project Settings.

#include "Manager/FirebaseAuthSubsystem.h"
#include "Global/CGDevSettings.h"

#include "Manager/PlayFabManagerSubsystem.h"
#include "Utils/CloudHttp.h"
#include "Utils/CF1CaptureSavePolicy.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* const AuthIniSection = TEXT("/Script/CompanyGrowthRenewal.FirebaseAuthSubsystem");

	// 익명 네임스페이스라도 유니티 빌드에서 같은 blob 에 묶이면 전역처럼 충돌한다 - 파일별 고유 이름을 쓴다
	FString SerializeAuthJson(const TSharedRef<FJsonObject>& JsonObject)
	{
		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return Serialized;
	}
}

void UFirebaseAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GConfig)
	{
		GConfig->GetString(AuthIniSection, TEXT("WebApiKey"), WebApiKey, GGameIni);
	}

	UE_LOG(LogTemp, Log, TEXT("[FirebaseAuth] 초기화 완료 (WebApiKey %s)"),
		WebApiKey.IsEmpty() ? TEXT("미설정") : TEXT("설정됨"));

	// 백오프 잠금은 스스로 풀리지 않는다 ― 포그라운드 복귀를 해제 계기로 쓴다(헤더가 약속한 동작).
	ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
		this, &UFirebaseAuthSubsystem::ClearAuthBackoff);
}

void UFirebaseAuthSubsystem::Deinitialize()
{
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundHandle);
	ForegroundHandle.Reset();
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	PendingRetries.Empty();

	Super::Deinitialize();
}

bool UFirebaseAuthSubsystem::ShouldSuppressRequests(bool bIgnoreBackoffLock)
{
	if (UCGDevSettings::IsOfflineMode())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			GI->GetTimerManager().ClearTimer(RefreshTimerHandle);
		}
		const bool bWasReady = bAuthReady;
		bAuthReady = false;
		IdToken.Empty();
		RefreshToken.Empty();
		bRequestInFlight = false;
		FlushPendingRetries(/*bRecovered=*/false);
		if (bWasReady) OnAuthReady.Broadcast(false);
		return true;
	}

	// 백오프 잠금 중에는 교환·갱신을 아예 보내지 않는다 ― 3초 폴링이 401 마다 재진입해 영구 루프가 된다
	if (!bIgnoreBackoffLock && RetryState.bBackoffLocked)
	{
		return true;
	}

	if (!bCaptureIsolationEvaluated)
	{
		bSuppressHttpRequestsForCapture =
			CGR::CF1CaptureSavePolicy::ShouldSuppressOnlineRequests(
				FCommandLine::Get(), FPaths::ProjectSavedDir());
		bCaptureIsolationEvaluated = true;

		if (bSuppressHttpRequestsForCapture)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CF1Isolation] AUTH_HTTP_SUPPRESSED"));
		}
	}

	return bSuppressHttpRequestsForCapture;
}

void UFirebaseAuthSubsystem::Exchange()
{
	if (ShouldSuppressRequests())
	{
		return;
	}

	if (bRequestInFlight)
	{
		return;
	}

	if (WebApiKey.IsEmpty())
	{
		LockAuth(TEXT("DefaultGame.ini [/Script/CompanyGrowthRenewal.FirebaseAuthSubsystem] WebApiKey 가 비어 있음"));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UPlayFabManagerSubsystem* PlayFabMgr = GI ? GI->GetSubsystem<UPlayFabManagerSubsystem>() : nullptr;
	const FString SessionTicket = PlayFabMgr ? PlayFabMgr->GetUserInfo().SessionTicket : FString();
	if (SessionTicket.IsEmpty())
	{
		LockAuth(TEXT("PlayFab SessionTicket 이 없음 (로그인 미완료)"));
		return;
	}

	const TSharedRef<FJsonObject> JsonBody = MakeShared<FJsonObject>();
	JsonBody->SetStringField(TEXT("sessionTicket"), SessionTicket);

	FCloudRequest Request;
	Request.Verb = TEXT("POST");
	Request.Path = TEXT("/auth/exchange");
	Request.Body = SerializeAuthJson(JsonBody);
	Request.Tag = TEXT("FirebaseAuth");
	Request.Action = TEXT("exchange");

	bRequestInFlight = true;

	TWeakObjectPtr<UFirebaseAuthSubsystem> WeakSelf(this);
	FCloudHttp::Send(Request, [WeakSelf](const FCloudResponse& Response)
	{
		UFirebaseAuthSubsystem* Self = WeakSelf.Get();
		if (!Self)
		{
			return;
		}
		// in-flight 해제가 억제 검사보다 먼저여야 한다 ― 잠금이 요청 중에 걸리면 플래그가 영구 true 로 남는다.
		Self->bRequestInFlight = false;
		if (Self->ShouldSuppressRequests()) return;

		if (!Response.bOk || !Response.Root.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[FirebaseAuth] exchange fail code=%s http=%d"),
				*Response.Code, Response.HttpStatus);
			Self->HandleAuthFailure(Response.Code);
			return;
		}

		FString CustomToken;
		Response.Root->TryGetStringField(TEXT("customToken"), CustomToken);
		Response.Root->TryGetStringField(TEXT("pid"), Self->Pid);

		// 서버 계약상 displayName 은 null 일 수 있다 ― 실패로 보지 않는다
		FString ParsedDisplayName;
		if (Response.Root->TryGetStringField(TEXT("displayName"), ParsedDisplayName))
		{
			Self->DisplayName = ParsedDisplayName;
		}

		if (CustomToken.IsEmpty() || Self->Pid.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[FirebaseAuth] exchange fail code=INTERNAL — customToken/pid 누락"));
			Self->HandleAuthFailure(TEXT("INTERNAL"));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[FirebaseAuth] exchange ok pid=%s"), *Self->Pid);
		Self->SignInWithCustomToken(CustomToken);
	});
}

void UFirebaseAuthSubsystem::SignInWithCustomToken(const FString& CustomToken)
{
	if (ShouldSuppressRequests()) return;

	const TSharedRef<FJsonObject> JsonBody = MakeShared<FJsonObject>();
	JsonBody->SetStringField(TEXT("token"), CustomToken);
	JsonBody->SetBoolField(TEXT("returnSecureToken"), true);

	FCloudRequest Request;
	Request.Verb = TEXT("POST");
	Request.AbsoluteUrl = FString::Printf(
		TEXT("https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken?key=%s"), *WebApiKey);
	Request.Body = SerializeAuthJson(JsonBody);
	Request.Tag = TEXT("FirebaseAuth");
	Request.Action = TEXT("signInWithCustomToken");

	bRequestInFlight = true;

	TWeakObjectPtr<UFirebaseAuthSubsystem> WeakSelf(this);
	FCloudHttp::Send(Request, [WeakSelf](const FCloudResponse& Response)
	{
		UFirebaseAuthSubsystem* Self = WeakSelf.Get();
		if (!Self)
		{
			return;
		}
		// in-flight 해제가 억제 검사보다 먼저여야 한다 ― 잠금이 요청 중에 걸리면 플래그가 영구 true 로 남는다.
		Self->bRequestInFlight = false;
		if (Self->ShouldSuppressRequests()) return;

		FString NewIdToken;
		FString NewRefreshToken;
		FString ExpiresIn;
		if (Response.Root.IsValid())
		{
			Response.Root->TryGetStringField(TEXT("idToken"), NewIdToken);
			Response.Root->TryGetStringField(TEXT("refreshToken"), NewRefreshToken);
			Response.Root->TryGetStringField(TEXT("expiresIn"), ExpiresIn);
		}

		if (!Response.bOk || NewIdToken.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[FirebaseAuth] signInWithCustomToken fail code=%s http=%d msg=%s"),
				*Response.Code, Response.HttpStatus, *Response.Message);
			Self->HandleAuthFailure(Response.Code);
			return;
		}

		const int64 ExpiresSeconds = ExpiresIn.IsEmpty() ? 3600 : FCString::Atoi64(*ExpiresIn);
		UE_LOG(LogTemp, Log, TEXT("[FirebaseAuth] signInWithCustomToken ok expiresIn=%lld"), ExpiresSeconds);
		Self->ApplyTokens(NewIdToken, NewRefreshToken, ExpiresSeconds);
	});
}

void UFirebaseAuthSubsystem::RefreshIdToken()
{
	if (ShouldSuppressRequests())
	{
		return;
	}

	if (bRequestInFlight)
	{
		return;
	}

	if (RefreshToken.IsEmpty() || WebApiKey.IsEmpty())
	{
		Exchange();
		return;
	}

	FCloudRequest Request;
	Request.Verb = TEXT("POST");
	Request.AbsoluteUrl = FString::Printf(TEXT("https://securetoken.googleapis.com/v1/token?key=%s"), *WebApiKey);
	Request.ContentType = TEXT("application/x-www-form-urlencoded");
	Request.Body = FString::Printf(TEXT("grant_type=refresh_token&refresh_token=%s"),
		*FGenericPlatformHttp::UrlEncode(RefreshToken));
	Request.Tag = TEXT("FirebaseAuth");
	Request.Action = TEXT("refresh");

	bRequestInFlight = true;

	TWeakObjectPtr<UFirebaseAuthSubsystem> WeakSelf(this);
	FCloudHttp::Send(Request, [WeakSelf](const FCloudResponse& Response)
	{
		UFirebaseAuthSubsystem* Self = WeakSelf.Get();
		if (!Self)
		{
			return;
		}
		// in-flight 해제가 억제 검사보다 먼저여야 한다 ― 잠금이 요청 중에 걸리면 플래그가 영구 true 로 남는다.
		Self->bRequestInFlight = false;
		if (Self->ShouldSuppressRequests()) return;

		FString NewIdToken;
		FString NewRefreshToken;
		FString ExpiresIn;
		if (Response.Root.IsValid())
		{
			Response.Root->TryGetStringField(TEXT("id_token"), NewIdToken);
			Response.Root->TryGetStringField(TEXT("refresh_token"), NewRefreshToken);
			Response.Root->TryGetStringField(TEXT("expires_in"), ExpiresIn);
		}

		if (!Response.bOk || NewIdToken.IsEmpty())
		{
			// 갱신 실패는 교환부터 재수행한다(설계서 §3.2)
			UE_LOG(LogTemp, Warning, TEXT("[FirebaseAuth] refresh fail code=%s http=%d — 교환부터 재수행"),
				*Response.Code, Response.HttpStatus);
			Self->bAuthReady = false;
			Self->RefreshToken.Empty();
			Self->Exchange();
			return;
		}

		const int64 ExpiresSeconds = ExpiresIn.IsEmpty() ? 3600 : FCString::Atoi64(*ExpiresIn);
		UE_LOG(LogTemp, Log, TEXT("[FirebaseAuth] refresh ok expiresIn=%lld"), ExpiresSeconds);
		Self->ApplyTokens(NewIdToken, NewRefreshToken, ExpiresSeconds);
	});
}

void UFirebaseAuthSubsystem::ApplyTokens(const FString& InIdToken, const FString& InRefreshToken, int64 ExpiresInSeconds)
{
	if (ShouldSuppressRequests()) return;

	IdToken = InIdToken;
	if (!InRefreshToken.IsEmpty())
	{
		RefreshToken = InRefreshToken;
	}

	bAuthReady = true;
	ConsecutiveFailures = 0;

	// ⚠ 교환 성공은 "요청 401" 카운터를 리셋하지 않는다 ― 갓 발급된 토큰에도 401 이면 그게 무한 루프였다
	RetryState.OnTokensApplied();

	// 만료 10분 전, 최대 50분 뒤 선갱신(설계서 §3.2)
	const float RefreshDelay = FMath::Clamp(static_cast<float>(ExpiresInSeconds) - 600.0f, 60.0f, 3000.0f);
	if (UGameInstance* GI = GetGameInstance())
	{
		FTimerManager& Timers = GI->GetTimerManager();
		Timers.ClearTimer(RefreshTimerHandle);
		Timers.SetTimer(RefreshTimerHandle, this, &UFirebaseAuthSubsystem::RefreshIdToken, RefreshDelay, false);
	}

	OnAuthReady.Broadcast(true);
	FlushPendingRetries(/*bRecovered=*/true);
}

void UFirebaseAuthSubsystem::HandleAuthFailure(const FString& Code)
{
	bAuthReady = false;
	++ConsecutiveFailures;

	if (ConsecutiveFailures >= FailureThreshold)
	{
		UE_LOG(LogTemp, Error, TEXT("[FirebaseAuth] auth fail code=%s — %d회 연속 실패로 서버 기능을 잠금"),
			*Code, ConsecutiveFailures);
		FlushPendingRetries(/*bRecovered=*/false);
		OnAuthReady.Broadcast(false);
		return;
	}

	Exchange();
}

void UFirebaseAuthSubsystem::LockAuth(const FString& Reason)
{
	bAuthReady = false;
	UE_LOG(LogTemp, Error, TEXT("[FirebaseAuth] exchange fail — %s"), *Reason);
	FlushPendingRetries(/*bRecovered=*/false);
	OnAuthReady.Broadcast(false);
}

bool UFirebaseAuthSubsystem::NotifyAuthRejected(const FString& Code, FAuthRetryCallback RetryOnce)
{
	if (ShouldSuppressRequests())
	{
		if (RetryOnce) RetryOnce(false);
		return false;
	}

	bAuthReady = false;

	// 재인증 직후에 또 401 이면 루프다 ― N회에서 요청을 끊는다(보안 리뷰 낮음13)
	if (RetryState.OnRequestRejected())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[FirebaseAuth] backoff lock code=%s — 갓 발급된 토큰이 %d회 연속 401. 재인증을 중단한다(포그라운드 복귀 시 재시도)"),
			*Code, FCloudAuthRetryState::PostReauthRejectionThreshold);
		FlushPendingRetries(/*bRecovered=*/false);
		OnAuthReady.Broadcast(false);

		if (RetryOnce)
		{
			RetryOnce(false);
		}
		return false;
	}

	if (RetryState.bBackoffLocked)
	{
		// 이미 잠겨 있다 ― 교환을 새로 시작하지 않는다
		if (RetryOnce)
		{
			RetryOnce(false);
		}
		return false;
	}

	bool bScheduled = false;
	if (RetryOnce)
	{
		if (PendingRetries.Num() < MaxPendingRetries)
		{
			PendingRetries.Add(RetryOnce);
			bScheduled = true;
		}
		else
		{
			// 상한을 넘겼다 ― 조용히 버리면 호출부의 pending 플래그가 영구히 남는다
			RetryOnce(false);
		}
	}

	if (Code == TEXT("AUTH_EXPIRED"))
	{
		RefreshIdToken();
		return bScheduled;
	}

	// AUTH_INVALID·AUTH_REQUIRED 는 교환부터 재수행한다(설계서 §3.2)
	Exchange();
	return bScheduled;
}

void UFirebaseAuthSubsystem::NotifyAuthAccepted()
{
	RetryState.OnRequestAccepted();
}

void UFirebaseAuthSubsystem::ClearAuthBackoff()
{
	if (ShouldSuppressRequests(/*bIgnoreBackoffLock=*/true)) return;

	if (!RetryState.bBackoffLocked)
	{
		return;
	}

	RetryState.ClearBackoff();
	ConsecutiveFailures = 0;
	UE_LOG(LogTemp, Warning, TEXT("[FirebaseAuth] backoff cleared — 재인증을 다시 시도한다"));
	Exchange();
}

void UFirebaseAuthSubsystem::FlushPendingRetries(bool bRecovered)
{
	if (PendingRetries.Num() == 0)
	{
		return;
	}

	TArray<FAuthRetryCallback> Retries = MoveTemp(PendingRetries);
	PendingRetries.Empty();

	for (const FAuthRetryCallback& Retry : Retries)
	{
		if (Retry)
		{
			Retry(bRecovered);
		}
	}
}
