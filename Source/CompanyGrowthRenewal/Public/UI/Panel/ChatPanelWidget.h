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

	// 위로 올려 읽는 중 도착한 메시지 수 안내. WBP 에 없으면 조용히 생략되고 스크롤 정책은 그대로 동작한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NewMessageNotice = nullptr;

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

	// 맨 위 오래된 메시지 잘라내기 - 위로 올려 읽는 중에는 읽던 줄이 밀리므로 바닥 근처일 때만 호출한다
	void TrimOldMessages();

	// 하단 근처인지 - 읽던 자리를 뺏지 않기 위한 판정
	bool IsNearBottom() const;

	// 대기 중인 새 메시지 수를 안내 문구에 반영한다
	void RefreshNewMessageNotice();

	// 손으로 바닥까지 내려온 경우 - 안내를 지우고 미뤄둔 트리밍을 따라잡는다
	UFUNCTION()
	void OnMessageScrollUserScrolled(float CurrentOffset);

	// 하단에서 이만큼 안쪽이면 "바닥에 있다"로 본다
	static constexpr float NearBottomThreshold = 24.f;
	int32 PendingNewMessageCount = 0;

	// 스크롤 맨 아래로 이동
	void ScrollToBottom();

	// 최대 표시 메시지 수
	int32 MaxVisibleMessages = 100;
};
