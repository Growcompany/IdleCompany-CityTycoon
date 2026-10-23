// Fill out your copyright notice in the Description page of Project Settings.

#include "Utils/CloudHttp.h"
#include "Global/CGDevSettings.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Http.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FCloudHttp::JoinUrl(const FString& Base, const FString& Path)
{
	FString Normalized = Base;
	while (Normalized.RemoveFromEnd(TEXT("/")))
	{
	}

	FString Suffix = Path;
	while (Suffix.RemoveFromStart(TEXT("/")))
	{
	}

	if (Suffix.IsEmpty())
	{
		return Normalized;
	}

	return Normalized + TEXT("/") + Suffix;
}

bool FCloudHttp::IsAllowedBaseUrl(const FString& Url, bool bShipping)
{
	if (Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (!Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	if (bShipping)
	{
		return false;
	}

	// 에뮬레이터만 남긴다 ― 개발 편의로 임의 http 호스트를 열면 그게 그대로 출하된다
	const FString Host = Url.RightChop(7);
	return Host.StartsWith(TEXT("127.0.0.1"), ESearchCase::IgnoreCase)
		|| Host.StartsWith(TEXT("localhost"), ESearchCase::IgnoreCase)
		|| Host.StartsWith(TEXT("[::1]"), ESearchCase::IgnoreCase);
}

FString FCloudHttp::GetBaseUrl()
{
	// 채팅과 같은 ini 키를 공유한다 — URL 의 단일 진실 원천을 둘로 늘리지 않는다(설계서 §3.1)
	FString LoadedUrl;
	if (GConfig)
	{
		GConfig->GetString(
			TEXT("/Script/CompanyGrowthRenewal.ChatManagerSubsystem"),
			TEXT("FirebaseBaseUrl"), LoadedUrl, GGameIni);
	}

	// ini 값에 따옴표가 없으면 파서가 '//' 이후를 주석으로 잘라 "https:" 만 남는다(PLAYBOOK §3.10)
	if (!IsAllowedBaseUrl(LoadedUrl, UE_BUILD_SHIPPING != 0))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CloudHttp] FirebaseBaseUrl 이 유효하지 않음 (읽은 값: '%s') — ini 값을 큰따옴표로 감쌌는지, Shipping 이 https 인지 확인할 것"),
			*LoadedUrl);
		return FString();
	}

	return LoadedUrl;
}

FString FCloudHttp::GetClientVersion()
{
	FString Version;
	if (GConfig)
	{
		GConfig->GetString(
			TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("ProjectVersion"), Version, GGameIni);
	}

	return Version.IsEmpty() ? TEXT("0.0.0") : Version;
}

FString FCloudHttp::GetClientBuild()
{
	return LexToString(FApp::GetBuildConfiguration());
}

void FCloudHttp::ApplyCommonHeaders(
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request,
	const FString& IdToken,
	const FString& ContentType)
{
	if (!ContentType.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), ContentType);
	}

	if (!IdToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *IdToken));
	}

	// 1-B 서버가 클라 빌드를 검사한다 — 1-A 부터 보내 둔다(설계서 §6.0)
	Request->SetHeader(TEXT("X-Client-Version"), GetClientVersion());
	Request->SetHeader(TEXT("X-Client-Build"), GetClientBuild());
}

bool FCloudHttp::ParseEnvelope(int32 HttpStatus, const FString& Body, FCloudResponse& OutResponse)
{
	OutResponse.HttpStatus = HttpStatus;

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutResponse.bOk = false;
		OutResponse.Code = TEXT("INTERNAL");
		OutResponse.Message = FString::Printf(TEXT("unparseable response body (HTTP %d)"), HttpStatus);
		return false;
	}

	OutResponse.Root = RootObject;

	// 봉투가 아닌 응답(구글 ID 플랫폼)은 ok 필드가 없어 HTTP 상태로 판정한다
	bool bEnvelopeOk = false;
	OutResponse.bOk = RootObject->TryGetBoolField(TEXT("ok"), bEnvelopeOk)
		? bEnvelopeOk
		: (HttpStatus >= 200 && HttpStatus < 300);

	double ParsedServerTime = 0.0;
	if (RootObject->TryGetNumberField(TEXT("serverTime"), ParsedServerTime))
	{
		OutResponse.ServerTime = static_cast<int64>(ParsedServerTime);
	}

	RootObject->TryGetStringField(TEXT("code"), OutResponse.Code);
	RootObject->TryGetStringField(TEXT("message"), OutResponse.Message);

	const TSharedPtr<FJsonObject>* NestedObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("state"), NestedObject) && NestedObject)
	{
		OutResponse.State = *NestedObject;
	}
	if (RootObject->TryGetObjectField(TEXT("result"), NestedObject) && NestedObject)
	{
		OutResponse.Result = *NestedObject;
	}

	if (!OutResponse.bOk && OutResponse.Code.IsEmpty())
	{
		// 구글 엔드포인트의 실패 본문은 { error: { message } } 라 봉투 코드가 없다
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (RootObject->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject)
		{
			(*ErrorObject)->TryGetStringField(TEXT("message"), OutResponse.Message);
		}
		OutResponse.Code = TEXT("INTERNAL");
	}

	return OutResponse.bOk;
}

void FCloudHttp::Send(const FCloudRequest& Request, TFunction<void(const FCloudResponse&)> OnDone)
{
	if (UCGDevSettings::IsOfflineMode())
	{
		FCloudResponse Failed;
		Failed.Code = TEXT("AUTH_REQUIRED");
		Failed.Message = TEXT("cloud transport disabled in CG Dev local mode");
		if (OnDone) OnDone(Failed);
		return;
	}

	const FString Tag = Request.Tag;
	const FString Action = Request.Action;
	const FString OpShort = Request.OpId.Left(8);

	FString Url = Request.AbsoluteUrl;
	if (Url.IsEmpty())
	{
		const FString Base = GetBaseUrl();
		if (Base.IsEmpty())
		{
			FCloudResponse Failed;
			Failed.Code = TEXT("INTERNAL");
			Failed.Message = TEXT("firebase base url is not configured");
			UE_LOG(LogTemp, Error, TEXT("[%s] %s fail code=%s op=%s ms=0"), *Tag, *Action, *Failed.Code, *OpShort);
			if (OnDone)
			{
				OnDone(Failed);
			}
			return;
		}
		Url = JoinUrl(Base, Request.Path);
	}

	const double StartSeconds = FPlatformTime::Seconds();

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(Request.Verb);
	ApplyCommonHeaders(HttpRequest, Request.IdToken, Request.ContentType);
	if (!Request.Body.IsEmpty())
	{
		HttpRequest->SetContentAsString(Request.Body);
	}
	if (Request.TimeoutSeconds > 0.0f)
	{
		HttpRequest->SetTimeout(Request.TimeoutSeconds);
	}

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnDone = MoveTemp(OnDone), StartSeconds, Tag, Action, OpShort]
		(FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bWasSuccessful)
		{
			const int32 ElapsedMs = static_cast<int32>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);

			FCloudResponse Parsed;
			if (!bWasSuccessful || !HttpResponse.IsValid())
			{
				Parsed.Code = TEXT("INTERNAL");
				Parsed.Message = TEXT("http transport failure");
			}
			else
			{
				ParseEnvelope(HttpResponse->GetResponseCode(), HttpResponse->GetContentAsString(), Parsed);
			}

			// 성공 로그는 호출자가 남긴다(§0.5 의 동작별 문구) — 여기선 실패만 소리 낸다
			if (!Parsed.bOk)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] %s fail code=%s http=%d op=%s ms=%d msg=%s"),
					*Tag, *Action, *Parsed.Code, Parsed.HttpStatus, *OpShort, ElapsedMs, *Parsed.Message);
			}

			if (OnDone)
			{
				OnDone(Parsed);
			}
		});

	HttpRequest->ProcessRequest();
}
