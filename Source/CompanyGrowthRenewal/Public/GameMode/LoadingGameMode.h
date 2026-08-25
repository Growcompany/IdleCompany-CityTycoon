// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LoadingGameMode.generated.h"

class ULoadingWidget;

/**
 * 부팅 로딩 맵(LoadingMap) 전용 경량 GameMode.
 * BeginPlay 에서 ULoadingWidget 을 생성/표시한다. (위젯이 부팅 오케스트레이션을 소유)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ALoadingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALoadingGameMode();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<ULoadingWidget> LoadingWidgetInstance;
};
