// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/CountdownText.h"

#include "CommonTextBlock.h"
#include "Data/GameClockRules.h"
#include "Engine/World.h"
#include "TimerManager.h"

void FCountdownTicker::Start(UWorld* InWorld, UCommonTextBlock* InText, TFunction<int64()> InSecondsProvider,
	const FString& InPrefix)
{
	Stop();

	World = InWorld;
	Text = InText;
	SecondsProvider = MoveTemp(InSecondsProvider);
	Prefix = InPrefix;

	if (InWorld && InText && SecondsProvider)
	{
		InWorld->GetTimerManager().SetTimer(
			Handle, FTimerDelegate::CreateRaw(this, &FCountdownTicker::Refresh), 1.0f, true);
	}

	Refresh();
}

void FCountdownTicker::Stop()
{
	if (UWorld* OwningWorld = World.Get())
	{
		OwningWorld->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	World.Reset();
	Text.Reset();
	SecondsProvider = nullptr;
}

void FCountdownTicker::Refresh() const
{
	UCommonTextBlock* Target = Text.Get();
	if (!Target || !SecondsProvider)
	{
		return;
	}

	Target->SetText(FText::FromString(Prefix + FGameClockRules::FormatCountdown(SecondsProvider())));
}
