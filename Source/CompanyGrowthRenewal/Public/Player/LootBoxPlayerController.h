// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LootBoxPlayerController.generated.h"

class ALootBoxActor;
class ULootBoxLayerWidget;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * LootBoxMap 전용 플레이어 컨트롤러
 * - UI 입력 처리
 * - 룩박스 오픈 로직
 * - LootBoxActor와 UI 간 연결
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ALootBoxPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALootBoxPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	// 입력 컴포넌트 설정
	virtual void SetupInputComponent() override;

private:
	// 현재 표시 중인 LootBoxActor 참조
	UPROPERTY()
	ALootBoxActor* CurrentLootBoxActor = nullptr;

	// LootBoxLayerWidget 참조
	UPROPERTY()
	ULootBoxLayerWidget* LootBoxWidget = nullptr;

	// Enhanced Input (PC용)
	UPROPERTY()
	UInputMappingContext* IMC_LootBox = nullptr;

	UPROPERTY()
	UInputAction* IA_Confirm = nullptr;

	// 델리게이트 바인딩
	void BindLootBoxEvents();

	// 레거시 터치 Released 처리 (모바일용)
	void OnTouchReleased(ETouchIndex::Type FingerIndex, FVector Location);

	// Enhanced Input 처리 (PC용)
	void OnConfirmReleased(const FInputActionValue& Value);

	// 룩박스 선택 시 호출 (3D 모델 변경)
	UFUNCTION()
	void OnLootBoxSelected(FName LootBoxID);

	// 룩박스 오픈 시 호출 (애니메이션 재생)
	UFUNCTION()
	void OnLootBoxOpening(FName LootBoxID, int32 RewardSkinID);

	// 애니메이션 완료 시 호출 (UI 복원)
	UFUNCTION()
	void OnAnimationFinished();
};
