// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Interfaces/IInputHandler.h"
#include "InputActionValue.h"
#include "Player/Components/FactoryTapHoldPolicy.h"
#include "InteractableInputHandler.generated.h"

class APlayerCamera;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;
class UFactoryPanelWidget;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UInteractableInputHandler : public UActorComponent, public IInputHandler
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractableInputHandler();

	void Initialize();
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Interactable
	void OnInteractionStarted(const FInputActionValue& Value);
	void OnInteractionTriggered(const FInputActionValue& Value);
	void OnInteractionFinish(const FInputActionValue& Value);
	void OnInteractionCanceled(const FInputActionValue& Value);

	void OnTapCompleted(const FInputActionValue& Value);

	// 책상 클릭/빈 좌석 버블 클릭 시 우측 도킹 WorkstationInfo 패널 열기 (Normal 모드 전용)
	void OpenWorkstationPanel(class AWorkstationActorBase* Workstation);

private:
	bool bMobileTouchActive = false;

	// 클릭시작할 때 클릭된 위치랑 그걸 world위치로 반환해서 임시저장용
	FVector2D SavedTouchScreenPos;
	FVector SavedTouchWorldPos;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* IMC_Interactable = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_InteractableHold = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* IA_InteractableTap = nullptr;

	UPROPERTY()
	AActor* LastHitActor = nullptr;

private:
	APlayerCamera* Owner = nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = nullptr;
	FTimerHandle FactoryHoldDelayTimerHandle;
	CGRFactoryTapHoldPolicy::EPhase FactoryPressPhase = CGRFactoryTapHoldPolicy::EPhase::None;

	// Building Click Logic
	bool bIsDragging = false;
	bool bIsFirstClickBuilding = false;

	AActor* GetOverlappingActor(const FVector2D& ScreenPos);

	// 프로젝트 개발중/출시대기 = 오피스 "집중 모드" — 월드 오브젝트 상호작용/패널 오픈 차단.
	// 단일 진실 = OfficeStageProgressManager::GetLifecycle(). 방문 모드 가드와 동형.
	bool IsOfficeFocusLocked() const;

	void BeginFactoryHoldAfterDelay();
	bool FinishFactoryPress();
	void ResetFactoryPressState();
	UFactoryPanelWidget* GetTopFactoryPanel() const;
	UFactoryPanelWidget* GetActiveFactoryPanel() const;

};
