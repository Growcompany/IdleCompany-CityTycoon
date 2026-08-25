// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChatMessageData.generated.h"

/**
 * 채팅 메시지 데이터
 * - Firebase에서 수신한 메시지를 UE 측에서 관리하는 구조체
 */
USTRUCT(BlueprintType)
struct FChatMessage
{
	GENERATED_BODY()

	// Firebase 문서 ID
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString MessageId;

	// 발신자 PlayFabId
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString SenderId;

	// 발신자 표시 이름
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString SenderName;

	// 메시지 내용
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Content;

	// 메시지 생성 시간 (ISO 8601)
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString CreatedAt;

	// 채널 ("global", "guild" 등)
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString Channel;

	// 발신자 프로필 이미지 (DT_ProfileImage.ImageID). 0 = 기본
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	int32 ProfileImageID = 0;
};
