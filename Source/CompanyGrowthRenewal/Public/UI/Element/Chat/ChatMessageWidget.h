// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ChatMessageData.h"
#include "ChatMessageWidget.generated.h"

class UTextBlock;
class UImage;

/**
 * 개별 채팅 메시지 엘리먼트 위젯
 * - ScrollBox 내에서 메시지 한 줄을 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UChatMessageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 메시지 데이터 설정
	void SetMessageData(const FChatMessage& Message);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> SenderNameText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageContentText = nullptr;

	// 발신 시각 (로컬 HH:MM) — 트리엔 있었으나 배선이 없어 계속 비어 있던 칸
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimestampText = nullptr;

	// 발신자 프로필 이미지 (DT_ProfileImage 조회로 주입)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> EntityImage = nullptr;

private:
	// 내가 보낸 메시지인지 (PlayFabId 대조) — 이름 색만 액센트로 구분
	bool IsFromLocalPlayer(const FString& SenderId) const;

	void ApplyProfileImage(int32 ProfileImageID);
};
