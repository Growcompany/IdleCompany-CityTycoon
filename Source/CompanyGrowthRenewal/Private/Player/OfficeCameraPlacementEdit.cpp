#include "Player/OfficeCameraPawn.h"
#include "Player/OfficePlayerController.h"
#include "Player/Components/PlacementHandler.h"
#include "Player/Components/MovementInputHandler.h"
#include "Core/CGGameInstance.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Engine/World.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "Office/DecorationActor.h"
#include "Office/OfficeManager.h"
#include "Manager/SaveLoadManager.h"

bool AOfficeCameraPawn::BeginOfficePlacementEdit()
{
	AOfficePlayerController* OfficeController = Cast<AOfficePlayerController>(GetController());
	UCGGameInstance* Instance = Cast<UCGGameInstance>(GetGameInstance());
	UOfficeStageProgressManager* Stage = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (bOfficePlacementEditing || !PlacementHandler || PlacementHandler->HasActivePlacement()
		|| !OfficeController || OfficeController->GetCurrentInputMode() != EInputMode::Normal
		|| (Instance && Instance->IsVisitMode()) || (Stage && Stage->IsFocusLocked())) return false;
	StopFollowingEmployee();
	bOfficePlacementEditing = true;
	if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UOfficeLayerWidget* OfficeLayer = UIManager->GetOfficeLayer()) OfficeLayer->SetOfficePlacementBubblesSuppressed(true);
	}
	PlacementHandler->SetOfficeEditSession(true);
	OnOfficePlacementEditChanged.Broadcast();
	return true;
}

void AOfficeCameraPawn::EndOfficePlacementEdit()
{
	if (!bOfficePlacementEditing) return;
	CancelOfficeEditSelection();
	bOfficePlacementEditing = false;
	if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		if (UOfficeLayerWidget* OfficeLayer = UIManager->GetOfficeLayer()) OfficeLayer->SetOfficePlacementBubblesSuppressed(false);
	}
	PlacementHandler->SetOfficeEditSession(false);
	OnOfficePlacementEditChanged.Broadcast();
}

AActor* AOfficeCameraPawn::GetOfficeEditSelection() const
{
	return bOfficePlacementEditing && PlacementHandler ? PlacementHandler->GetPlacementTargetActor() : nullptr;
}

bool AOfficeCameraPawn::SelectOfficeEditActor(AActor* Actor)
{
	if (!bOfficePlacementEditing || !Actor || GetOfficeEditSelection()) return false;
	if (!PlacementHandler->BeginExistingOfficeMove(Actor)) return false;
	if (PlacementHandler->IsWallDecorationMode())
	{
		EnterWallEditMode();
		FocusOnWall(PlacementHandler->GetCurrentWallSide(), Actor->GetActorLocation());
	}
	if (AOfficePlayerController* OfficeController = Cast<AOfficePlayerController>(GetController())) OfficeController->GoToBuildPlaceMode();
	OnOfficePlacementEditChanged.Broadcast();
	return true;
}

bool AOfficeCameraPawn::ConfirmOfficeEditSelection()
{
	if (!GetOfficeEditSelection() || !PlacementHandler->FinishExistingOfficeMove(true)) return false;
	if (IsInWallEditMode()) ExitWallEditMode();
	if (AOfficePlayerController* OfficeController = Cast<AOfficePlayerController>(GetController())) OfficeController->GoToNormalMode();
	if (MovementInputHandler) MovementInputHandler->InitDragMoveIMC();
	OnOfficePlacementEditChanged.Broadcast();
	return true;
}

void AOfficeCameraPawn::CancelOfficeEditSelection()
{
	if (!bOfficePlacementEditing) return;
	if (PlacementHandler) PlacementHandler->FinishExistingOfficeMove(false);
	if (IsInWallEditMode()) ExitWallEditMode();
	if (AOfficePlayerController* OfficeController = Cast<AOfficePlayerController>(GetController())) OfficeController->GoToNormalMode();
	if (MovementInputHandler) MovementInputHandler->InitDragMoveIMC();
	OnOfficePlacementEditChanged.Broadcast();
}

void AOfficeCameraPawn::RotateOfficeEditSelection()
{
	if (GetOfficeEditSelection()) PlacementHandler->RotatePlacementEntity();
	OnOfficePlacementEditChanged.Broadcast();
}

bool AOfficeCameraPawn::CanRemoveOfficeEditSelection() const
{
	ADecorationActor* Decoration = Cast<ADecorationActor>(GetOfficeEditSelection());
	UOfficeManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr;
	return IsValid(Decoration) && Manager && Manager->GetPlacedDecorations().Contains(Decoration);
}

bool AOfficeCameraPawn::RemoveOfficeEditSelection()
{
	if (!CanRemoveOfficeEditSelection()) return false;
	ADecorationActor* Decoration = Cast<ADecorationActor>(GetOfficeEditSelection());
	UOfficeManager* Manager = GetWorld()->GetSubsystem<UOfficeManager>();
	// 파괴 전에 선택 상태를 해제하여 원본 위치 저장 가드와 부착된 하이라이트를 정리한다.
	CancelOfficeEditSelection();
	// 등록 해제를 먼저 한다. 파괴된 액터를 매니저에 넘기면 teardown 훅이 생기는 순간 깨진다.
	Manager->UnregisterPlacedDecoration(Decoration);
	if (!Decoration->Destroy()) return false;
	if (USaveLoadManager* Saves = GetGameInstance()->GetSubsystem<USaveLoadManager>()) Saves->SaveGameData();
	OnOfficePlacementEditChanged.Broadcast();
	return true;
}
