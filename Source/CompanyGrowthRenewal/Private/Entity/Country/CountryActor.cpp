// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Country/CountryActor.h"
#include "Components/StaticMeshComponent.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Player/PlayerCamera.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ACountryActor::ACountryActor()
{
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);
}

void ACountryActor::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bSuccess = false;
			FCountryInfoTable Info = TableMgr->GetCountryInfo(CountryType, bSuccess);
			if (bSuccess)
			{
				CachedFlagIcon = Info.FlagIcon.LoadSynchronous();
			}
		}
	}
}

ACountryActor* ACountryActor::FindCountryActor(const UObject* WorldContextObject, ECountryType InCountry)
{
	if (!WorldContextObject || InCountry == ECountryType::None) return nullptr;

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World) return nullptr;

	for (TActorIterator<ACountryActor> It(World); It; ++It)
	{
		if (It->CountryType == InCountry)
		{
			return *It;
		}
	}
	return nullptr;
}

void ACountryActor::RequestFocus(bool bAnimate)
{
	// C++ 측 기본 동작: WorldMap 카메라에 FocusOnLocation 직접 호출
	// (PlayerCamera 패턴: BuildOpenWidget.cpp의 카메라 포커싱과 동일)
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (APlayerCamera* PlayerCamera = Cast<APlayerCamera>(PC->GetPawn()))
		{
			PlayerCamera->FocusOnLocation(GetActorLocation(), FocusDistance);
		}
	}

	// BP에서 추가 연출(페이드/줌/사운드 등) 필요하면 오버라이드
	OnFocusRequested(bAnimate);
}
