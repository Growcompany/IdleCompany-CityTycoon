// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/LootBoxPlayerController.h"
#include "Player/LootBoxOrbitPawn.h"
#include "Player/InputTypeManager.h"
#include "Player/CGCheatManager.h"
#include "Entity/LootBox/LootBoxActor.h"
#include "UI/Panel/LootBoxLayerWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Input/InputPriorities.h"

ALootBoxPlayerController::ALootBoxPlayerController()
{
	// 전역 치트 매니저 설정
	CheatClass = UCGCheatManager::StaticClass();

	// 마우스 커서 표시
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bEnableTouchEvents = true;

	// Enhanced Input 로드 (PC용)
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> imc_LootBox
	(TEXT("/Game/CompanyGrowth/Input/IMC_LootBox.IMC_LootBox"));
	if (imc_LootBox.Succeeded())
	{
		IMC_LootBox = imc_LootBox.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ia_Confirm
	(TEXT("/Game/CompanyGrowth/Input/IA_Released.IA_Released"));
	if (ia_Confirm.Succeeded())
	{
		IA_Confirm = ia_Confirm.Object;
	}
}

void ALootBoxPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 입력 모드 설정: UI와 게임 모두 가능
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	// PC용 Enhanced Input 설정
	if (UInputTypeManager::GetPlatformInputType() == EInputType::KeyMouse)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (IMC_LootBox)
			{
				Subsystem->AddMappingContext(IMC_LootBox, InputPriority::UI_BLOCKING);
			}
		}
	}

	// 레벨에 배치된 LootBoxActor 찾기
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALootBoxActor::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		CurrentLootBoxActor = Cast<ALootBoxActor>(FoundActors[0]);
		if (CurrentLootBoxActor)
		{
			// 애니메이션 완료 델리게이트 바인딩
			CurrentLootBoxActor->OnAnimationFinished.AddUObject(this, &ALootBoxPlayerController::OnAnimationFinished);

			// Pawn에 LootBoxActor 전달 (카메라 설정용)
			if (APawn* ControlledPawn = GetPawn())
			{
				if (ALootBoxOrbitPawn* OrbitPawn = Cast<ALootBoxOrbitPawn>(ControlledPawn))
				{
					OrbitPawn->SetLootBox(CurrentLootBoxActor);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LootBoxPlayerController] LootBoxActor not found in level"));
		}
	}

	// UI 델리게이트 바인딩
	BindLootBoxEvents();
}

void ALootBoxPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 플랫폼별 입력 바인딩
	if (UInputTypeManager::GetPlatformInputType() == EInputType::Touch)
	{
		// 모바일: 레거시 터치 이벤트
		InputComponent->BindTouch(IE_Released, this, &ALootBoxPlayerController::OnTouchReleased);
	}
	else if (UInputTypeManager::GetPlatformInputType() == EInputType::KeyMouse)
	{
		// PC: Enhanced Input
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			if (IA_Confirm)
			{
				EnhancedInputComponent->BindAction(IA_Confirm, ETriggerEvent::Completed,
					this, &ALootBoxPlayerController::OnConfirmReleased);
			}
		}
	}
}

void ALootBoxPlayerController::BindLootBoxEvents()
{
	// UIManager에서 LootBoxLayerWidget 가져오기
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxPlayerController] GameInstance is null"));
		return;
	}

	UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();
	if (!UIManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxPlayerController] UIManager not found"));
		return;
	}

	// UIManager에서 직접 LootBoxLayerWidget 가져오기
	LootBoxWidget = UIManager->GetLootBoxLayer();

	if (!LootBoxWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxPlayerController] LootBoxLayerWidget not created yet - will try ShowLootBoxUI()"));

		// LootBox UI가 아직 생성되지 않았으면 생성 요청
		LootBoxWidget = UIManager->ShowLootBoxUI();
	}

	if (LootBoxWidget)
	{
		// 델리게이트 바인딩
		LootBoxWidget->OnLootBoxSelected.AddUObject(this, &ALootBoxPlayerController::OnLootBoxSelected);
		LootBoxWidget->OnLootBoxOpening.AddUObject(this, &ALootBoxPlayerController::OnLootBoxOpening);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LootBoxPlayerController] Failed to get or create LootBoxLayerWidget"));
	}
}

void ALootBoxPlayerController::OnLootBoxSelected(FName LootBoxID)
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxPlayerController] LootBox selected: %s"), *LootBoxID.ToString());

	if (!CurrentLootBoxActor)
	{
		return;
	}

	// LootBoxActor의 비주얼 변경 (선택된 상자로)
	CurrentLootBoxActor->SetLootBoxVisual(LootBoxID);
}

void ALootBoxPlayerController::OnLootBoxOpening(FName LootBoxID, int32 RewardSkinID)
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxPlayerController] Opening LootBox: %s, RewardID: %d"),
		*LootBoxID.ToString(), RewardSkinID);

	if (!CurrentLootBoxActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxPlayerController] CurrentLootBoxActor is null!"));
		return;
	}

	// LootBoxActor의 오픈 애니메이션 재생
	CurrentLootBoxActor->OpenLootBox(RewardSkinID);
}

void ALootBoxPlayerController::OnTouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
	// LootBoxActor가 입력 대기 중인지 확인
	if (CurrentLootBoxActor && CurrentLootBoxActor->bWaitingForInput)
	{
		UE_LOG(LogTemp, Log, TEXT("[LootBoxPlayerController] Touch released - completing reward"));
		CurrentLootBoxActor->CompleteReward();
	}
}

void ALootBoxPlayerController::OnAnimationFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[LootBoxPlayerController] Animation finished - restoring UI"));

	// UI 복원
	if (LootBoxWidget)
	{
		LootBoxWidget->ShowUIAfterAnimation();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LootBoxPlayerController] LootBoxWidget is null - cannot restore UI"));
	}
}

void ALootBoxPlayerController::OnConfirmReleased(const FInputActionValue& Value)
{
	// LootBoxActor가 입력 대기 중인지 확인
	if (CurrentLootBoxActor && CurrentLootBoxActor->bWaitingForInput)
	{
		UE_LOG(LogTemp, Log, TEXT("[LootBoxPlayerController] PC Click released - completing reward"));
		CurrentLootBoxActor->CompleteReward();
	}
}
