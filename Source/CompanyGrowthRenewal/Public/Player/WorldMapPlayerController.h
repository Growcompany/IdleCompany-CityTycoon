// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/MainMapPlayerController.h"
#include "WorldMapPlayerController.generated.h"

/**
 * WorldMap 전용 PlayerController
 *
 * 기능:
 * - MainMapPlayerController 상속으로 터치/줌/패닝 호환
 * - 메인맵 복귀 기능
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AWorldMapPlayerController : public AMainMapPlayerController
{
	GENERATED_BODY()

public:
	AWorldMapPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	// 메인맵으로 복귀
	UFUNCTION(BlueprintCallable, Category = "WorldMap")
	void ReturnToMainMap();
};
