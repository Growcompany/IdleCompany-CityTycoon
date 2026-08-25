// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Enum/LootBoxCategory.h"
#include "LootBoxMapGameMode.generated.h"

class ALootBoxActor;
class ULootBoxLayerWidget;

/**
 * LootBoxMap 전용 게임 모드
 * - 회전 카메라 Pawn 설정
 * - LootBox UI 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ALootBoxMapGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALootBoxMapGameMode();

protected:
	virtual void StartPlay() override;

	// 레벨에 배치된 현재 LootBox
	UPROPERTY(BlueprintReadOnly, Category = "LootBox")
	ALootBoxActor* CurrentLootBox = nullptr;

	// UI Widget 참조
	UPROPERTY()
	ULootBoxLayerWidget* LootBoxWidget = nullptr;

private:
	// 카테고리별 LootBox 맵 초기화
	void InitializeLootBoxMap(ELootBoxCategory Category);

	// 델리게이트 핸들러
	UFUNCTION()
	void OnLootBoxSelectionChanged(FName LootBoxID);

	UFUNCTION()
	void OnLootBoxOpeningStarted(FName LootBoxID, int32 RewardSkinID);

	UFUNCTION()
	void OnLootBoxAnimationFinished();
};
