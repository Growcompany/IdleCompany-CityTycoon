// Fill out your copyright notice in the Description page of Project Settings.

#include "MainMapPlayerController.h"
#include "Player/InputTypeManager.h"
#include "Player/CGCheatManager.h"
#include "PlayerCamera.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include <Util/CoordinateUtils.h>
#include "Table/InteractableInfo.h"
#include "Player/Components/InteractableInputHandler.h"
#include "Player/Components/PlacementHandler.h"

AMainMapPlayerController::AMainMapPlayerController()
{
    UE_LOG(LogTemp, Log, TEXT("AMainMapPlayerController AMainMapPlayerController."));

    CoordinateUtils::SetPlayerController(this);

    // 전역 치트 매니저 설정 (모든 맵에서 콘솔 명령어 사용 가능)
    CheatClass = UCGCheatManager::StaticClass();
}

void AMainMapPlayerController::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("AMainMapPlayerController BeginPlay."));
    CameraPawn = Cast<APlayerCamera>(GetPawn());

    UGameInstance* GIBase = GetGameInstance();
    auto* CGGI = Cast<UCGGameInstance>(GIBase);
    CGGI->SetCurrentPlayerController(this);

    bShowMouseCursor = true;
    bEnableTouchEvents = true;
    bEnableTouchOverEvents = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;  // 레이캐스트를 위해 true 필요
    DefaultMouseCursor = EMouseCursor::Default;

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false); // 마우스 숨김 방지
    SetInputMode(InputMode);
	Super::SetInputMode(InputMode);

    // 게임 데이터 로드는 GameMode::StartPlay()에서 수행 (정석적인 위치)
    // PlayerController는 입력 처리만 담당
}

void AMainMapPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

    // 모바일에서만 레거시 터치 이벤트 바인딩
    if (GetCurrentInputType() == EInputType::Touch)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TEXT("Touch type detected - binding touch events"));
        }

        InputComponent->BindTouch(IE_Pressed, this, &AMainMapPlayerController::OnTouchPressed);
        InputComponent->BindTouch(IE_Released, this, &AMainMapPlayerController::OnTouchReleased);
        InputComponent->BindTouch(IE_Repeat, this, &AMainMapPlayerController::OnTouchMoved);
    }
}

void AMainMapPlayerController::Destroyed()
{
	Super::Destroyed();

	CoordinateUtils::SetPlayerController(nullptr);
}

void AMainMapPlayerController::SetGameInputMode(EInputMode NewMode)
{
	if (CurrentInputMode == NewMode)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Input Mode changed: %s -> %s"),
		*StaticEnum<EInputMode>()->GetNameStringByValue((int32)CurrentInputMode),
		*StaticEnum<EInputMode>()->GetNameStringByValue((int32)NewMode));

	CurrentInputMode = NewMode;
}

void AMainMapPlayerController::GoToNormalMode()
{
    SetGameInputMode(EInputMode::Normal);
}

void AMainMapPlayerController::GoToUIMode()
{
    SetGameInputMode(EInputMode::UI);
}

void AMainMapPlayerController::GoToBuildPlaceMode()
{
    SetGameInputMode(EInputMode::BuildPlace);
}

void AMainMapPlayerController::GoToFactoryMode()
{
    SetGameInputMode(EInputMode::Factory);
}


void AMainMapPlayerController::OnTouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (GEngine)
    {
        FString msg = FString::Printf(TEXT("TOUCH PRESSED! Finger: %d, Loc: %.1f,%.1f"),
            (int32)FingerIndex, Location.X, Location.Y);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, msg);
    }

    if (FingerIndex != ETouchIndex::Touch1) return;  // 첫 번째 터치만 처리

    // UI 모드에서는 게임 월드 인터랙션 처리하지 않음
    if (GetCurrentInputMode() == EInputMode::UI)
    {
        bIsTouchActive = false;
        return;
    }

    TouchStartTime = GetWorld()->GetTimeSeconds();
    TouchStartLocation = FVector2D(Location.X, Location.Y);
    bIsTouchActive = true;

    UE_LOG(LogTemp, Error, TEXT("Touch Pressed at: %s"), *TouchStartLocation.ToString());

    // InteractableInputHandler에 터치 시작 알림
    APlayerCamera* PlayerCamera = Cast<APlayerCamera>(GetPawn());
    if (PlayerCamera && PlayerCamera->InteractableInputHandler)
    {
        FInputActionValue DummyValue;  // 빈 값으로 생성
        PlayerCamera->InteractableInputHandler->OnInteractionStarted(DummyValue);
    }
}

void AMainMapPlayerController::OnTouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (GEngine)
    {
        FString msg = FString::Printf(TEXT("TOUCH RELEASED! Finger: %d"), (int32)FingerIndex);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, msg);
    }

    // UI 모드에서 시작된 터치는 무시 (bIsTouchActive가 false)
    if (!bIsTouchActive) return;

    float touchDuration = GetWorld()->GetTimeSeconds() - TouchStartTime;

    APlayerCamera* PlayerCamera = Cast<APlayerCamera>(GetPawn());
    if (PlayerCamera && PlayerCamera->InteractableInputHandler)
    {
        FInputActionValue DummyValue;

        // 짧은 터치면 Tap, 긴 터치면 Hold 완료
        if (touchDuration < 0.1f)
        {
            PlayerCamera->InteractableInputHandler->OnInteractionCanceled(DummyValue);
            PlayerCamera->InteractableInputHandler->OnTapCompleted(DummyValue);
        }
        else
        {
            PlayerCamera->InteractableInputHandler->OnInteractionFinish(DummyValue);
        }
    }

    bIsTouchActive = false;
}

void AMainMapPlayerController::OnTouchMoved(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (FingerIndex != ETouchIndex::Touch1 || !bIsTouchActive) return;

    APlayerCamera* PlayerCamera = Cast<APlayerCamera>(GetPawn());
    if (PlayerCamera && PlayerCamera->InteractableInputHandler)
    {
        FInputActionValue DummyValue;
        PlayerCamera->InteractableInputHandler->OnInteractionTriggered(DummyValue);
    }
}

void AMainMapPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Warning, TEXT("[MainMapPlayerController] OnPossess - Reinitializing input settings"));

	// 마우스 이벤트 재설정 (레벨 전환 시 초기화될 수 있음)
	bShowMouseCursor = true;
	bEnableTouchEvents = true;
	bEnableTouchOverEvents = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;  // 레이캐스트 활성화
	DefaultMouseCursor = EMouseCursor::Default;

	// Input Mode 재설정
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	// PlayerCamera 재초기화
	if (APlayerCamera* PlayerCamera = Cast<APlayerCamera>(InPawn))
	{
		// Collision 및 Input 재설정
		PlayerCamera->OnChangedInputType();
		// SetupPlayerInputComponent는 Pawn Possess 시 자동 호출됨
	}
}
