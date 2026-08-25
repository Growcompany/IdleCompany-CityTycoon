// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/ManagerBase.h"

void UManagerBase::SetGameInstance(UCGGameInstance* instance)
{
	GameInstance = instance;
}

void UManagerBase::BeginDestroy()
{
	Super::BeginDestroy();

	if (OnBeginDestroyDelegate.IsValid())
	{
		OnBeginDestroyDelegate->Broadcast(this);
		OnBeginDestroyDelegate.Reset();
	}
}