// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Level/CGLevelScriptBase.h"
#include "Core/CGGameInstance.h"

ACGLevelScriptBase::ACGLevelScriptBase()
{
}

void ACGLevelScriptBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ACGLevelScriptBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

    UWorld* World = GetWorld();
    UE_LOG(LogTemp, Warning, TEXT("▶ World: %s"), *GetNameSafe(World));

    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("World is nullptr"));
        return;
    }

    UGameInstance* BaseGI = World->GetGameInstance();
    UE_LOG(LogTemp, Warning, TEXT("▶ BaseGI class: %s"), *GetNameSafe(BaseGI));

    UCGGameInstance* CGGI = Cast<UCGGameInstance>(BaseGI);
    if (!CGGI)
    {
        UE_LOG(LogTemp, Error, TEXT("Cast to UCGGameInstance Fail!"));
        return;
    }

    UCGGameInstance* gameInstance = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
	gameInstance->SetCurrentLevelScript(this);
}

void ACGLevelScriptBase::BeginPlay()
{
}

void ACGLevelScriptBase::BeginDestroy()
{
	Super::BeginDestroy();


}

void ACGLevelScriptBase::Destroyed()
{
	Super::Destroyed();
}
