// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/MainMapPlayerController.h"
#include "OfficePlayerController.generated.h"

/**
 * OfficeMap 전용 PlayerController
 *
 * 기능:
 * - UI 모드와 카메라 모드 전환
 * - 입력 처리
 * - MainMapPlayerController 상속으로 CoordinateUtils 호환
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficePlayerController : public AMainMapPlayerController
{
	GENERATED_BODY()

public:
	AOfficePlayerController();

protected:
	virtual void BeginPlay() override;

public:
	// ========== Input Mode Management ==========

	// UI 모드로 전환 (마우스 커서 표시, 카메라 이동 비활성화)
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetUIMode();

	// 게임 모드로 전환 (마우스 커서 숨김, 카메라 이동 활성화)
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetGameMode();

	// 현재 UI 모드인지 확인
	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsUIMode() const { return bIsUIMode; }

private:
	// 현재 UI 모드 상태
	bool bIsUIMode = false;
};
