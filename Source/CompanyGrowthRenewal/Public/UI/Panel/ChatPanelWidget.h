// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Data/ChatMessageData.h"
#include "ChatPanelWidget.generated.h"

class UScrollBox;
class UEditableTextBox;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UButton;
class UTextBlock;
class UChatManagerSubsystem;
class UTableManagerSubsystem;
class UChatMessageWidget;
class UCloseButtonWidget;
class UIconWithButtonWidget;

/**
 * 글로벌 채팅 패널 위젯 — 화면 좌측에 붙는 드로어 시트
 * - 좌측 레일 탭(월드/길드/친구) + 메시지 리스트(ScrollBox) + 입력 필드 + 전송 버튼
 * - 활성화 시 폴링 시작, 비활성화 시 폴링 중지
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UChatPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	UChatPanelWidget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// 메시지 리스트 스크롤 박스
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> MessageScrollBox = nullptr;

	// 메시지 입력 필드
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> MessageInputField = nullptr;

	// 전송 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> SendButton = nullptr;

	// 배경 클릭 시 닫기용 투명 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

	// 돌아가기 버튼 (좌상단, CloseButtonWidget 재사용)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> BackButton = nullptr;

	// 채널 이름 표시 (선택적)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChannelNameText = nullptr;

	// 채널 탭 — 설정창과 공용인 UIE_RailTab (CommonButtonBase 파생). 선택 순서 = 등록 순서.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UIconWithButtonWidget> WorldChatTab = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UIconWithButtonWidget> GuildChatTab = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UIconWithButtonWidget> FriendChatTab = nullptr;

private:
	UPROPERTY()
	UCommonButtonGroupBase* TabButtonGroup = nullptr;

	// 채팅 탭 선택 변경
	UFUNCTION()
	void OnChatTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UPROPERTY()
	UChatManagerSubsystem* ChatMgr = nullptr;

	UPROPERTY()
	UTableManagerSubsystem* TableMgr = nullptr;

	// 메시지 수신 델리게이트 핸들
	FDelegateHandle MessagesReceivedHandle;

	// 전송 버튼 클릭
	UFUNCTION()
	void OnSendButtonClicked();

	// 돌아가기 버튼 클릭
	UFUNCTION()
	void OnBackButtonClicked();

	// 배경 클릭 시 닫기
	UFUNCTION()
	void OnBackgroundClicked();

	// 입력 필드 Enter 키 처리
	UFUNCTION()
	void OnMessageInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// 새 메시지 수신 콜백
	void HandleMessagesReceived(const TArray<FChatMessage>& Messages);

	// UI에 메시지 위젯 추가
	void AddMessageToUI(const FChatMessage& Message);

	// 스크롤 맨 아래로 이동
	void ScrollToBottom();

	// 최대 표시 메시지 수
	int32 MaxVisibleMessages = 100;
};
