#include "UI/Panel/EventChoicePanelWidget.h"
#include "UI/Element/Cards/EventChoiceCardWidget.h"
#include "CommonTextBlock.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void UEventChoicePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UIE_EventChoiceCard)   UIE_EventChoiceCard->OnCardSelected.AddDynamic(this, &UEventChoicePanelWidget::HandleCardSelected);
	if (UIE_EventChoiceCard_1) UIE_EventChoiceCard_1->OnCardSelected.AddDynamic(this, &UEventChoicePanelWidget::HandleCardSelected);
	if (UIE_EventChoiceCard_2) UIE_EventChoiceCard_2->OnCardSelected.AddDynamic(this, &UEventChoicePanelWidget::HandleCardSelected);
}

void UEventChoicePanelWidget::NativeDestruct()
{
	if (UIE_EventChoiceCard)   UIE_EventChoiceCard->OnCardSelected.RemoveAll(this);
	if (UIE_EventChoiceCard_1) UIE_EventChoiceCard_1->OnCardSelected.RemoveAll(this);
	if (UIE_EventChoiceCard_2) UIE_EventChoiceCard_2->OnCardSelected.RemoveAll(this);
	Super::NativeDestruct();
}

void UEventChoicePanelWidget::SetEventData(const FProjectEventData& EventData)
{
	if (TitleText)
	{
		TitleText->SetText(EventData.EventTitle);
	}

	if (UIE_EventChoiceCard   && EventData.Choices.IsValidIndex(0)) UIE_EventChoiceCard->SetChoiceData(EventData.Choices[0], 0);
	if (UIE_EventChoiceCard_1 && EventData.Choices.IsValidIndex(1)) UIE_EventChoiceCard_1->SetChoiceData(EventData.Choices[1], 1);
	if (UIE_EventChoiceCard_2 && EventData.Choices.IsValidIndex(2)) UIE_EventChoiceCard_2->SetChoiceData(EventData.Choices[2], 2);

	// 카드 등장 stagger 사운드 — 0/100/200ms 간격으로 CardSlide 3회
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>();
	if (!SoundMgr) return;
	TWeakObjectPtr<USoundManagerSubsystem> WeakMgr(SoundMgr);
	SoundMgr->PlaySound(FName("Event_CardSlide"));
	for (int32 i = 1; i < 3; ++i)
	{
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(
			[WeakMgr]() { if (WeakMgr.IsValid()) WeakMgr->PlaySound(FName("Event_CardSlide")); }),
			0.1f * i, false);
	}
}

void UEventChoicePanelWidget::HandleCardSelected(int32 ChoiceIndex)
{
	if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
	{
		SoundMgr->PlaySound(FName("Event_CardSelect"));
	}
	OnChoiceMade.Broadcast(ChoiceIndex);
	DeactivateWidget();
}
