// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

class FJsonObject;

/**
 * 서버 응답 봉투(설계서 §6.0) 파싱 결과.
 * 구글 ID 플랫폼처럼 봉투가 아닌 응답도 Root/HttpStatus 로 그대로 읽을 수 있다.
 */
struct COMPANYGROWTHRENEWAL_API FCloudResponse
{
	bool bOk = false;
	int32 HttpStatus = 0;
	FString Code;
	FString Message;
	int64 ServerTime = 0;

	TSharedPtr<FJsonObject> Root;
	TSharedPtr<FJsonObject> State;
	TSharedPtr<FJsonObject> Result;
};

/** 클라우드 요청 1건. Path 는 Base URL 과 합치고, AbsoluteUrl 이 있으면 그쪽이 이긴다. */
struct COMPANYGROWTHRENEWAL_API FCloudRequest
{
	FString Verb = TEXT("POST");
	FString Path;
	FString AbsoluteUrl;
	FString Body;
	FString ContentType = TEXT("application/json");

	/** 비어 있으면 Authorization 헤더를 붙이지 않는다(무인증 라우트) */
	FString IdToken;

	/** 로그 계약(설계서 §0.5)의 태그·동작 이름 */
	FString Tag = TEXT("Cloud");
	FString Action;
	FString OpId;

	float TimeoutSeconds = 15.0f;
};

/**
 * Firebase Functions 호출 공용 헬퍼.
 * - URL 조립(말단 슬래시 정규화), 공통 헤더, 응답 봉투 파싱을 한곳에 모은다.
 */
class COMPANYGROWTHRENEWAL_API FCloudHttp
{
public:
	/** Base 말단 슬래시와 Path 앞 슬래시가 겹쳐 `//경로`(404)가 되는 걸 막는다 */
	static FString JoinUrl(const FString& Base, const FString& Path);

	/**
	 * 평문 http 는 `Authorization: Bearer <idToken>` 을 그대로 흘린다 ― 같은 망의 누구나 1시간짜리 전권 토큰을 얻는다.
	 * Shipping 은 https 만. 비Shipping 은 로컬 에뮬레이터(127.0.0.1/localhost/[::1])만 예외.
	 */
	static bool IsAllowedBaseUrl(const FString& Url, bool bShipping);

	/** ini 의 FirebaseBaseUrl(채팅 섹션이 SOT — 설계서 §3.1). 허용되지 않는 스킴이면 빈 문자열 */
	static FString GetBaseUrl();

	static FString GetClientVersion();
	static FString GetClientBuild();

	static void ApplyCommonHeaders(
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request,
		const FString& IdToken,
		const FString& ContentType);

	static bool ParseEnvelope(int32 HttpStatus, const FString& Body, FCloudResponse& OutResponse);

	static void Send(const FCloudRequest& Request, TFunction<void(const FCloudResponse&)> OnDone);
};
