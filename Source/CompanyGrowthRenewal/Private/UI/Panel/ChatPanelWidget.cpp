// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/ChatPanelWidget.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Chat/ChatMessageWidget.h"
#include "Manager/ChatManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"

UChatPanelWidget::UChatPanelWidget()
{
	// 좌측 가장자리에 붙는 드로어 — 그 가장자리에서 밀려 나온다 (거리/시간은 WBP Class Defaults 에서 조정 가능)
	AppearFrom = EAppearSlideFrom::Left;
	AppearRiseDistance = 420.f;
	AppearDuration = 0.24f;
}

void UChatPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		ChatMgr = GI->GetSubsystem<UChatManagerSubsystem>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	}

	// 버튼 바인딩
	if (SendButton)
	{
		SendButton->OnClicked().AddUObject(this, &UChatPanelWidget::OnSendButtonClicked);
	}

	if (BackButton)
	{
		BackButton->OnCloseClicked.AddDynamic(this, &UChatPanelWidget::OnBackButtonClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UChatPanelWidget::OnBackgroundClicked);
	}

	// 입력 필드 Enter 커밋
	if (MessageInputField)
	{
		MessageInputField->OnTextCommitted.AddDynamic(this, &UChatPanelWidget::OnMessageInputCommitted);
	}

	// 손으로 바닥까지 내려오는 것도 "읽었다"로 본다 - 안내 해제 경로가 수신 콜백뿐이면 카운트가 남는다
	if (MessageScrollBox)
	{
		MessageScrollBox->OnUserScrolled.AddDynamic(this, &UChatPanelWidget::OnMessageScrollUserScrolled);
	}

	// 채널 이름 표시
	if (ChannelNameText)
	{
		ChannelNameText->SetText(FText::FromString(TEXT("월드 채팅")));
	}

	// 채팅 채널 탭 그룹 셋업 — 등록 순서 = 레일 배치 순서 = ChannelNames 인덱스
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);

	const TArray<UIconWithButtonWidget*> Tabs = { WorldChatTab, GuildChatTab, FriendChatTab };
	for (UIconWithButtonWidget* Tab : Tabs)
	{
		if (!Tab) continue;
		TabButtonGroup->AddWidget(Tab);
		Tab->SetIsSelectable(true);
	}

	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UChatPanelWidget::OnChatTabSelectionChanged);
	TabButtonGroup->SelectButtonAtIndex(0);
}

void UChatPanelWidget::NativeDestruct()
{
	// 버튼 바인딩 해제
	if (SendButton)
	{
		SendButton->OnClicked().RemoveAll(this);
	}

	if (BackButton)
	{
		BackButton->OnCloseClicked.RemoveDynamic(this, &UChatPanelWidget::OnBackButtonClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UChatPanelWidget::OnBackgroundClicked);
	}

	if (MessageInputField)
	{
		MessageInputField->OnTextCommitted.RemoveDynamic(this, &UChatPanelWidget::OnMessageInputCommitted);
	}

	if (MessageScrollBox)
	{
		MessageScrollBox->OnUserScrolled.RemoveDynamic(this, &UChatPanelWidget::OnMessageScrollUserScrolled);
	}

	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UChatPanelWidget::OnChatTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	// TODO: ChatManagerSubsystem 에 채널별 폴링/메시지 분기 API 가 추가되면 여기서 채널 전환.
	// 지금은 채널 라벨만 갱신.
	if (!ChannelNameText) return;

	static const FString ChannelNames[] = { TEXT("월드 채팅"), TEXT("길드 채팅"), TEXT("친구 채팅") };
	if (ButtonIndex >= 0 && ButtonIndex < UE_ARRAY_COUNT(ChannelNames))
	{
		ChannelNameText->SetText(FText::FromString(ChannelNames[ButtonIndex]));
	}
}

void UChatPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 에디터 더미 + 이전 세션 위젯 제거
	if (MessageScrollBox)
	{
		MessageScrollBox->ClearChildren();
	}

	if (ChatMgr)
	{
		// 캐시된 메시지 표시
		const TArray<FChatMessage>& CachedMsgs = ChatMgr->GetCachedMessages();
		for (const FChatMessage& Msg : CachedMsgs)
		{
			AddMessageToUI(Msg);
		}

		// 새 메시지 수신 델리게이트 바인딩
		MessagesReceivedHandle = ChatMgr->OnMessagesReceived.AddUObject(this, &UChatPanelWidget::HandleMessagesReceived);

		// 폴링 시작 — 간격은 ini PollIntervalSeconds
		ChatMgr->StartPolling();
	}

	ScrollToBottom();

	// CommonUI 풀 재사용이라 닫고 다시 열어도 묵은 카운트가 따라온다
	PendingNewMessageCount = 0;
	RefreshNewMessageNotice();
}

void UChatPanelWidget::NativeOnDeactivated()
{
	if (ChatMgr)
	{
		// 델리게이트 해제 (폴링은 MiniLog가 관리하므로 중지하지 않음)
		ChatMgr->OnMessagesReceived.Remove(MessagesReceivedHandle);
	}

	// 메시지 위젯 클리어
	if (MessageScrollBox)
	{
		MessageScrollBox->ClearChildren();
	}

	// 메뉴/HUD 체인은 닫히는 창이 UI 모드를 해제한다 ― 단독 모달이므로 여기서 해제
	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		if (PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}
	Super::NativeOnDeactivated();
}

void UChatPanelWidget::OnSendButtonClicked()
{
	if (!MessageInputField || !ChatMgr)
	{
		return;
	}

	const FString Content = MessageInputField->GetText().ToString().TrimStartAndEnd();
	if (Content.IsEmpty())
	{
		return;
	}

	ChatMgr->SendMessage(Content);
	MessageInputField->SetText(FText::GetEmpty());
}

void UChatPanelWidget::OnBackButtonClicked()
{
	CloseWithAnimation();
}

void UChatPanelWidget::OnBackgroundClicked()
{
	CloseWithAnimation();
}

void UChatPanelWidget::OnMessageInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Enter 키로 전송
	if (CommitMethod == ETextCommit::OnEnter)
	{
		OnSendButtonClicked();
	}
}

void UChatPanelWidget::HandleMessagesReceived(const TArray<FChatMessage>& Messages)
{
	for (const FChatMessage& Msg : Messages)
	{
		AddMessageToUI(Msg);
	}

	// 3초 폴링이라 위로 올려 읽는 중 새 글 하나에도 읽던 자리를 잃는다 - 바닥 근처일 때만 따라간다
	if (IsNearBottom())
	{
		PendingNewMessageCount = 0;
		ScrollToBottom();
	}
	else
	{
		PendingNewMessageCount += Messages.Num();
	}
	RefreshNewMessageNotice();
}

void UChatPanelWidget::AddMessageToUI(const FChatMessage& Message)
{
	if (!MessageScrollBox || !TableMgr)
	{
		return;
	}

	// 맨 위를 자르면 그만큼 내용이 위로 밀려 읽던 줄이 어긋난다 - 바닥으로 돌아올 때 따라잡는다
	if (IsNearBottom())
	{
		TrimOldMessages();
	}

	// ChatMessageWidget 생성
	TSubclassOf<UUserWidget> MsgWidgetClass = TableMgr->GetWidgetClass(EWidgetType::ChatMessage);
	if (!MsgWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ChatPanel] ChatMessage 위젯 클래스를 찾을 수 없음"));
		return;
	}

	// Spacer (메시지 간 간격 15px)
	USpacer* SpacerWidget = NewObject<USpacer>(this);
	SpacerWidget->SetSize(FVector2D(1.0f, 15.0f));
	MessageScrollBox->AddChild(SpacerWidget);

	// ChatMessageWidget
	UChatMessageWidget* MsgWidget = CreateWidget<UChatMessageWidget>(this, MsgWidgetClass);
	if (MsgWidget)
	{
		MsgWidget->SetMessageData(Message);
		MessageScrollBox->AddChild(MsgWidget);
	}
}

bool UChatPanelWidget::IsNearBottom() const
{
	if (!MessageScrollBox)
	{
		return true;
	}
	return (MessageScrollBox->GetScrollOffsetOfEnd() - MessageScrollBox->GetScrollOffset()) <= NearBottomThreshold;
}

void UChatPanelWidget::RefreshNewMessageNotice()
{
	if (!NewMessageNotice)
	{
		return;
	}
	if (PendingNewMessageCount > 0)
	{
		NewMessageNotice->SetText(FText::Format(
			NSLOCTEXT("Chat", "NewMessageCount", "새 메시지 {0}개"), FText::AsNumber(PendingNewMessageCount)));
		NewMessageNotice->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		NewMessageNotice->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UChatPanelWidget::TrimOldMessages()
{
	if (!MessageScrollBox)
	{
		return;
	}

	// Spacer + Message 세트로 2개씩
	while (MessageScrollBox->GetChildrenCount() >= MaxVisibleMessages * 2)
	{
		MessageScrollBox->RemoveChildAt(0);
		MessageScrollBox->RemoveChildAt(0);
	}
}

void UChatPanelWidget::OnMessageScrollUserScrolled(float CurrentOffset)
{
	if (!IsNearBottom())
	{
		return;
	}

	PendingNewMessageCount = 0;
	RefreshNewMessageNotice();
	TrimOldMessages();
}

void UChatPanelWidget::ScrollToBottom()
{
	if (MessageScrollBox)
	{
		MessageScrollBox->ScrollToEnd();
	}
}
