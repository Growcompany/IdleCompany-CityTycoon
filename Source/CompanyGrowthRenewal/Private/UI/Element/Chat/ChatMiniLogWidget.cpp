// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Chat/ChatMiniLogWidget.h"
#include "Manager/ChatManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/UIBase.h"
#include "Enum/WidgetType.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Player/MainMapPlayerController.h"

void UChatMiniLogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		ChatMgr = GI->GetSubsystem<UChatManagerSubsystem>();
	}

	if (ChatIconButton)
	{
		ChatIconButton->OnClicked.AddDynamic(this, &UChatMiniLogWidget::OnChatIconClicked);
	}

	if (ChatBtn)
	{
		ChatBtn->OnClicked.AddDynamic(this, &UChatMiniLogWidget::OnChatIconClicked);
	}

	// 채팅 메시지 수신 구독
	if (ChatMgr)
	{
		MessagesReceivedHandle = ChatMgr->OnMessagesReceived.AddUObject(this, &UChatMiniLogWidget::HandleMessagesReceived);
		ConnectionChangedHandle = ChatMgr->OnChatConnectionChanged.AddUObject(this, &UChatMiniLogWidget::HandleConnectionChanged);

		// 폴링 시작 (미니로그용, 채팅 패널 안 열어도 새 메시지 표시) — 간격은 ini PollIntervalSeconds
		ChatMgr->StartPolling();
	}

	// 캐시에 메시지가 있으면 최근 1개 표시, 없으면 빈 텍스트
	if (ChatMgr && ChatMgr->GetCachedMessages().Num() > 0)
	{
		UpdateLastMessage(ChatMgr->GetCachedMessages().Last());
	}
	else if (LastMessageText)
	{
		LastMessageText->SetText(FText::GetEmpty());
	}

	// 위젯이 늦게 생성됐을 때 이미 끊긴 상태면 방송을 못 받으므로 여기서 한 번 반영
	if (ChatMgr && !ChatMgr->IsConnectionHealthy())
	{
		HandleConnectionChanged(false);
	}
}

void UChatMiniLogWidget::NativeDestruct()
{
	if (ChatIconButton)
	{
		ChatIconButton->OnClicked.RemoveDynamic(this, &UChatMiniLogWidget::OnChatIconClicked);
	}

	if (ChatBtn)
	{
		ChatBtn->OnClicked.RemoveDynamic(this, &UChatMiniLogWidget::OnChatIconClicked);
	}

	if (ChatMgr)
	{
		ChatMgr->OnMessagesReceived.Remove(MessagesReceivedHandle);
		ChatMgr->OnChatConnectionChanged.Remove(ConnectionChangedHandle);
	}

	// 페이드아웃 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FadeOutTimerHandle);
	}

	Super::NativeDestruct();
}

void UChatMiniLogWidget::OnChatIconClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
		UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();

		if (UIMgr && UIMgr->UIBaseInstance && TableMgr)
		{
			// PromptStack에 이미 위젯이 있으면 중복 push 방지
			if (UIMgr->UIBaseInstance->GetPromptStackCount() > 0) return;

			TSubclassOf<UUserWidget> PanelClass = TableMgr->GetWidgetClass(EWidgetType::ChatPanel);
			if (PanelClass)
			{
				UIMgr->UIBaseInstance->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(PanelClass));

				// UI 모드 전환
				AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
				if (PC)
				{
					PC->GoToUIMode();
				}
			}
		}
	}
}

void UChatMiniLogWidget::HandleMessagesReceived(const TArray<FChatMessage>& Messages)
{
	if (Messages.Num() > 0)
	{
		UpdateLastMessage(Messages.Last());
	}
}

void UChatMiniLogWidget::HandleConnectionChanged(bool bConnected)
{
	if (!LastMessageText)
	{
		return;
	}

	if (!bConnected)
	{
		LastMessageText->SetText(FText::FromString(TEXT("채팅 서버에 연결할 수 없습니다")));
		LastMessageText->SetRenderOpacity(1.0f);
		return;
	}

	// 복구 시 경고에 덮였던 마지막 메시지를 되살린다 — 한산한 채널이면 다음 메시지가 안 와서 빈 채로 남는다
	if (ChatMgr && ChatMgr->GetCachedMessages().Num() > 0)
	{
		UpdateLastMessage(ChatMgr->GetCachedMessages().Last());
		return;
	}

	LastMessageText->SetText(FText::GetEmpty());
}

void UChatMiniLogWidget::UpdateLastMessage(const FChatMessage& Message)
{
	if (!LastMessageText || !MiniLogBorder)
	{
		return;
	}

	// "닉네임: 메시지" 형식으로 1줄 표시 (최대 30자, 초과 시 ... 처리)
	FString DisplayText = FString::Printf(TEXT("%s: %s"), *Message.SenderName, *Message.Content);
	constexpr int32 MaxDisplayLength = 30;
	if (DisplayText.Len() > MaxDisplayLength)
	{
		DisplayText = DisplayText.Left(MaxDisplayLength) + TEXT("...");
	}
	LastMessageText->SetText(FText::FromString(DisplayText));

	LastMessageText->SetRenderOpacity(1.0f);
}

void UChatMiniLogWidget::StartFadeOutTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FadeOutTimerHandle);
		World->GetTimerManager().SetTimer(
			FadeOutTimerHandle,
			this,
			&UChatMiniLogWidget::FadeOutMessage,
			5.0f,
			false
		);
	}
}

void UChatMiniLogWidget::FadeOutMessage()
{
	// 시간 지나면 텍스트만 투명하게 (Border는 유지)
	if (LastMessageText)
	{
		LastMessageText->SetRenderOpacity(0.0f);
	}
}
