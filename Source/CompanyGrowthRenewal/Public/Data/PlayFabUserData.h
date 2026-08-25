// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PlayFabUserData.generated.h"

/**
 * PlayFab 로그인 상태
 */
UENUM(BlueprintType)
enum class EPlayFabLoginState : uint8
{
	NotLoggedIn UMETA(DisplayName = "미로그인"),
	LoggingIn   UMETA(DisplayName = "로그인 중"),
	LoggedIn    UMETA(DisplayName = "로그인 완료"),
	Failed      UMETA(DisplayName = "로그인 실패")
};

/**
 * PlayFab 로그인 유형
 */
UENUM(BlueprintType)
enum class EPlayFabLoginType : uint8
{
	Guest  UMETA(DisplayName = "게스트"),
	Google UMETA(DisplayName = "구글")
};

/**
 * PlayFab 유저 정보
 * - 로그인 성공 시 채워지는 런타임 데이터
 */
USTRUCT(BlueprintType)
struct FPlayFabUserInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	FString PlayFabId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	FString SessionTicket;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab")
	EPlayFabLoginType LoginType = EPlayFabLoginType::Guest;

	// 유효한 세션인지 확인
	bool IsValid() const
	{
		return !PlayFabId.IsEmpty() && !SessionTicket.IsEmpty();
	}
};
