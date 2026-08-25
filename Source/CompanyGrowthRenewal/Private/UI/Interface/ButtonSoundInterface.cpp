// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Interface/ButtonSoundInterface.h"
#include "UI/UISoundTags.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"

FGameplayTag IButtonSoundInterface::GetClickSoundTag() const
{
	return CGUISoundTags::ButtonClick;
}

FGameplayTag IButtonSoundInterface::GetHoverSoundTag() const
{
	return CGUISoundTags::ButtonHover;
}

void IButtonSoundInterface::PlayClickSound(const UObject* WorldContext)
{
	if (!ShouldPlayClickSound())
	{
		return;
	}

	PlayButtonSound(WorldContext, GetClickSoundTag());
}

void IButtonSoundInterface::PlayHoverSound(const UObject* WorldContext)
{
	if (!ShouldPlayHoverSound())
	{
		return;
	}

	PlayButtonSound(WorldContext, GetHoverSoundTag());
}

void IButtonSoundInterface::PlayButtonSound(const UObject* WorldContext, FGameplayTag SoundTag)
{
	if (!WorldContext || !SoundTag.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContext);
	if (!GameInstance)
	{
		return;
	}

	if (USoundManagerSubsystem* SoundManager = GameInstance->GetSubsystem<USoundManagerSubsystem>())
	{
		SoundManager->PlayUISound(SoundTag);
	}
}
