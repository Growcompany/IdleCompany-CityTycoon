// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ChatMessageData.h"
#include "ChatMiniLogWidget.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class UChatManagerSubsystem;

/**
 * 채팅 미니로그 위젯
 * - 좌하단에 항상 표시되는 HUD 요소
 * - 채팅 아이콘 + 반투명 배경 + 최근 메시지 1줄
 * - 클릭 시 ChatPanelWidget 열기
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UChatMiniLogWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 채팅 아이콘 버튼 (좌측)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> ChatIconButton = nullptr;

	// 전체 영역 클릭용 투명 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ChatBtn = nullptr;

	// 반투명 메시지 배경 (아이콘 오른쪽)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UBorder> MiniLogBorder = nullptr;

	// 최근 메시지 1줄 텍스트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> LastMessageText = nullptr;

private:
	UPROPERTY()
	UChatManagerSubsystem* ChatMgr = nullptr;

	FDelegateHandle MessagesReceivedHandle;
	FDelegateHandle ConnectionChangedHandle;

	// 메시지 표시 후 페이드아웃 타이머
	FTimerHandle FadeOutTimerHandle;

	// 채팅 아이콘 클릭 → ChatPanel 열기
	UFUNCTION()
	void OnChatIconClicked();

	// 새 메시지 수신 콜백
	void HandleMessagesReceived(const TArray<FChatMessage>& Messages);

	// 연결 끊김/복구 — 상시 HUD라 여기 띄워야 사용자가 알아챈다
	void HandleConnectionChanged(bool bConnected);

	// 최근 메시지 1줄 갱신
	void UpdateLastMessage(const FChatMessage& Message);

	// 일정 시간 후 메시지 텍스트 페이드아웃
	void StartFadeOutTimer();
	void FadeOutMessage();
};
