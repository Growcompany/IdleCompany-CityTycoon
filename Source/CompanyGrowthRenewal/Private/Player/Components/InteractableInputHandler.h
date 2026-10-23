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
	bool TryGetFactoryHoldAnchor(FVector2D& OutAbsolutePosition) const;
	void OnInteractionStarted(const FInputActionValue& Value);
	void OnInteractionTriggered(const FInputActionValue& Value);
	void OnInteractionFinish(const FInputActionValue& Value);
	void OnInteractionCanceled(const FInputActionValue& Value);

	void OnTapCompleted(const FInputActionValue& Value);

	// 책상 클릭/빈 좌석 버블 클릭 시 우측 도킹 WorkstationInfo 패널 열기 (Normal 모드 전용)
	// FocusEmployeeID 를 주면 그 직원을, 아니면 0번 좌석 직원을 보여준다
	void OpenWorkstationPanel(class AWorkstationActorBase* Workstation, int32 FocusEmployeeID = INDEX_NONE);

	// 자동화(-game 회귀 하네스) 전용 — 화면 히트 판정 없이 직원 탭·책상 클릭 경로를 그대로 탄다
	UFUNCTION(BlueprintCallable, Category = "Test")
	void TestSimulateEmployeeTap(class AOfficeworker* Worker) { HandleEmployeeTap(Worker); }

	UFUNCTION(BlueprintCallable, Category = "Test")
	void TestOpenWorkstationPanel(class AWorkstationActorBase* Workstation, int32 FocusEmployeeID) { OpenWorkstationPanel(Workstation, FocusEmployeeID); }

private:
	bool bOfficeEditSelectionPress = false;
	bool FinishOfficeEditSelectionPress();
	bool bMobileTouchActive = false;
	bool bHasPressScreenPosition = false;
	bool ShouldCancelAircraftTap() const;

	// 클릭시작할 때 클릭된 위치랑 그걸 world위치로 반환해서 임시저장용
	FVector2D SavedTouchScreenPos;
	FVector2D SavedTouchAbsolutePos = FVector2D::ZeroVector;
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

	// 직원 탭 — 이모트 반응 후 사무실 직원 패널을 연다 (패널 미등록이면 소속 책상 패널)
	void HandleEmployeeTap(class AOfficeworker* Worker);

	// 개발 중 폭주 직원 위에서 시작한 누름 — 카메라 끌기를 끄고, 문턱을 넘게 끌면 직원을 들어 올린다.
	// 캐치 링(UI 버튼)이 덮지 못한 몸 부분을 누른 경우의 경로. 짧은 탭은 기존 경로가 그대로 처리한다.
	bool bEmployeeCarryPress = false;
	TWeakObjectPtr<class UEmployeeReactionComponent> CarryPressReaction;
	void BeginEmployeeCarryPress(AActor* PressedActor);
	void UpdateEmployeeCarryPress();
	// 끌어 올린 누름이면 놓기까지 처리하고 true(호출부는 나머지 해제 경로를 건너뛴다)
	bool FinishEmployeeCarryPress();

	// 프로젝트 개발중/출시대기 = 오피스 "집중 모드" — 월드 오브젝트 상호작용/패널 오픈 차단.
	// 단일 진실 = OfficeStageProgressManager::GetLifecycle(). 방문 모드 가드와 동형.
	bool IsOfficeFocusLocked() const;

	void BeginFactoryHoldAfterDelay();
	bool FinishFactoryPress();
	void ResetFactoryPressState();

	// 패널이 닫힌 상태의 짧은 탭 — 공장 관리 패널을 연다.
	// 오프라인 생산분은 정산 때 바로 지급되므로 탭으로 수령할 재고가 없다.
	void HandleFactoryTapOpen();
	UFactoryPanelWidget* GetTopFactoryPanel() const;
	UFactoryPanelWidget* GetActiveFactoryPanel() const;

};
