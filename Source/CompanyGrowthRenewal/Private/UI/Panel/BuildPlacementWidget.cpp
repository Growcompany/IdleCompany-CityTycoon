// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/BuildPlacementWidget.h"

#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Player/OfficePlayerController.h"
#include "Player/PlayerCamera.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/Components/PlacementHandler.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Panel/BuildingManagePanelWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonBorder.h"
#include "Components/CanvasPanelSlot.h"
#include "GameMode/CGGameModeBase.h"
#include "Table/ConstructionCost.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "UI/UIBase.h"
#include "Enum/WidgetType.h"
#include "Enum/ResourceType.h"

void UBuildPlacementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	APlayerController* CurrentPC = GameInstance->GetCurrentPlayerController();

	// MainMap인 경우
	if (AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(CurrentPC))
	{
		// OfficePlayerController도 MainMapPlayerController를 상속하므로
		// 먼저 OfficeCameraPawn 캐스팅 시도 후 실패하면 PlayerCamera 사용
		if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(MainPC->GetPawn()))
		{
			Player = OfficePawn;
		}
		else
		{
			Player = Cast<APlayerCamera>(MainPC->GetPawn());
		}
	}

	if (PlaceButton)
	{
		PlaceButton->OnClicked().AddUObject(this, &UBuildPlacementWidget::OnPlaceButtonClicked);
	}
	if (RotateButton)
	{
		RotateButton->OnClicked().AddUObject(this, &UBuildPlacementWidget::OnRotateButtonClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UBuildPlacementWidget::OnCancelButtonClicked);
	}
}

void UBuildPlacementWidget::NativeDestruct()
{
	Super::NativeDestruct();
	if (PlaceButton)
	{
		PlaceButton->OnClicked().RemoveAll(this);
	}
	if (RotateButton)
	{
		RotateButton->OnClicked().RemoveAll(this);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}
}

void UBuildPlacementWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Player && RootBorder && RootBorder->Slot)
	{
		TObjectPtr<UCanvasPanelSlot> canvasSlot = Cast<UCanvasPanelSlot>(RootBorder->Slot);
		if (canvasSlot)
		{
			FVector2D screenPosition = Player->PlacementHandler->GetPlacementTargetBottomScreenPosition();
			float viewportScale = UWidgetLayoutLibrary::GetViewportScale(GetWorld());
			canvasSlot->SetPosition(screenPosition / viewportScale);
		}
	}
}

void UBuildPlacementWidget::OnPlaceButtonClicked()
{
	if (!Player)
	{
		return;
	}

	bool bSuccess = false;

	if (bIsMovingExistingBuilding)
	{
		// 기존 건물 재배치 확정
		bSuccess = Player->PlacementHandler->ConfirmBuildingMove();

		if (bSuccess)
		{
			// UI 정리 및 모드 종료
			Player->EndBuildingRelocation();
			DeactivateWidget();
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Building moved successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementWidget] Failed to move building"));
		}
	}
	else if (bIsWorkstationMode)
	{
		// 업무공간 배치 (연속 배치 모드 - 모드 유지, 취소 버튼으로만 종료)
		bool bCapacityReached = false;
		bSuccess = Player->PlacementHandler->PlacingWorkstation(&bCapacityReached);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Workstation placed successfully (continuous mode)"));
			if (bCapacityReached)
			{
				if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Player))
				{
					OfficePawn->EndWorkstationPlacement();
				}
				DeactivateWidget();
				return;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementWidget] Failed to place workstation"));
		}
	}
	else if (bIsDecorationMode)
	{
		// 장식품 배치 (연속 배치 모드 - 모드 유지, 취소 버튼으로만 종료)
		// 벽 장식 모드인지 바닥 장식 모드인지에 따라 분기
		if (Player->PlacementHandler->IsWallDecorationMode())
		{
			bSuccess = Player->PlacementHandler->PlacingWallDecoration();
		}
		else
		{
			bSuccess = Player->PlacementHandler->PlacingDecoration();
		}

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Decoration placed successfully (continuous mode)"));
			SpendDecorationCost();
			// 모드 종료 안 함 - 미리보기 액터 유지하여 계속 배치 가능
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementWidget] Failed to place decoration"));
		}
	}
	else
	{
		// 신규 건물 배치
		bSuccess = Player->PlacementHandler->PlacingEntity();

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Building placed successfully"));
			SpendConstructionCost();
			Player->EndBuild();
			DeactivateWidget();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementWidget] Failed to place building"));
		}
	}
}

void UBuildPlacementWidget::OnRotateButtonClicked()
{
	if (Player)
	{
		Player->PlacementHandler->RotatePlacementEntity();
	}
}

void UBuildPlacementWidget::OnCancelButtonClicked()
{
	if (!Player)
	{
		DeactivateWidget();
		return;
	}

	if (bIsMovingExistingBuilding)
	{
		// 기존 건물 재배치 취소 -> BuildManagePanel로 복귀
		Player->PlacementHandler->CancelBuildingMove();

		// EndBuildingRelocation은 Normal 모드로 전환하므로, ManagePanel 열고 나서 UI 모드로 다시 전환

		// BuildManagePanel 다시 띄우기
		UGameInstance* GameInstance = GetGameInstance();
		if (GameInstance)
		{
			UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();
			UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();

			if (UIManager && TableManager)
			{
				TSubclassOf<UUserWidget> ManagePanelClass = TableManager->GetWidgetClass(EWidgetType::BuildingManage);
				if (ManagePanelClass)
				{
					UUIBase* UIBase = UIManager->GetUIBase();
					if (UIBase)
					{
						// BuildPlacement 패널 닫기
						DeactivateWidget();

						// BuildManagePanel 다시 열기
						AInteractableBaseActor* TargetEntity = Player->PlacementHandler->GetPlacementTargetEntity();
						ABuildingBaseActor* ExistingBuilding = Cast<ABuildingBaseActor>(TargetEntity);

						UCommonActivatableWidget* PushedWidget = UIBase->PushBottomClass(ManagePanelClass.Get());
						UBuildingManagePanelWidget* ManagePanel = Cast<UBuildingManagePanelWidget>(PushedWidget);
						if (ManagePanel && ExistingBuilding)
						{
							ManagePanel->SetTargetBuilding(ExistingBuilding);

							// EndBuildingRelocation 후 UI 모드로 다시 전환
							Player->EndBuildingRelocation(); // Normal 모드로 전환됨

							AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(Player->GetController());
							if (MainPC)
							{
								MainPC->GoToUIMode();
							}

							UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Returned to BuildManagePanel after cancel with UI mode"));
						}
					}
				}
			}
		}
	}
	else if (bIsWorkstationMode)
	{
		// 업무공간 배치 취소
		if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Player))
		{
			OfficePawn->EndWorkstationPlacement();
		}
		DeactivateWidget();
		UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Workstation placement cancelled"));
	}
	else if (bIsDecorationMode)
	{
		// 장식품 배치 취소
		if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Player))
		{
			OfficePawn->EndDecorationPlacement();
		}
		DeactivateWidget();
		UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Decoration placement cancelled"));
	}
	else
	{
		// 신규 건물 건설 취소 → BuildPanelWidget으로 복귀
		Player->EndBuild();
		DeactivateWidget();

		UCGGameInstance* GI = Cast<UCGGameInstance>(GetWorld()->GetGameInstance());
		if (GI)
		{
			UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
			UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
			if (UIMgr && TableMgr)
			{
				TSubclassOf<UUserWidget> BuildPanelClass = TableMgr->GetWidgetClass(EWidgetType::BuildPanel);
				if (BuildPanelClass)
				{
					UIMgr->GetUIBase()->PushBottomClass(BuildPanelClass.Get());

					AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
					if (MainPC)
					{
						MainPC->GoToUIMode();
					}
				}
			}
		}
	}
}

void UBuildPlacementWidget::SetBuildableInfo(const FBuildableCardTable& buildableInfo, bool bIsMoving)
{
	BuildableInfo = buildableInfo;
	bIsMovingExistingBuilding = bIsMoving;
}

void UBuildPlacementWidget::CheckHasConstructionCost() const
{
	// BuildEntityCardWidget에서 이미 체크했으므로 여기서는 별도 처리 불필요
}

void UBuildPlacementWidget::SpendConstructionCost() const
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildPlacementWidget] ResourceItemManager not found!"));
		return;
	}

	for (const FConstructionCost& Cost : BuildableInfo.ConstructionCosts)
	{
		ResourceMgr->SpendResource(Cost.ResourceType, Cost.Cost);
		UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Spent %lld of ResourceType %d"),
			Cost.Cost, (int32)Cost.ResourceType);
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] All construction costs paid"));
}

void UBuildPlacementWidget::SetWorkstationInfo(const FWorkstationCardTable& InWorkstationInfo)
{
	WorkstationInfo = InWorkstationInfo;
	bIsWorkstationMode = true;
	bIsMovingExistingBuilding = false;

	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] SetWorkstationInfo: %s"), *WorkstationInfo.DisplayName.ToString());
}

void UBuildPlacementWidget::SetDecorationInfo(const FDecorationCardTable& InDecorationInfo)
{
	DecorationInfo = InDecorationInfo;
	bIsDecorationMode = true;
	bIsWorkstationMode = false;
	bIsMovingExistingBuilding = false;

	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] SetDecorationInfo: %s"), *DecorationInfo.Name.ToString());
}

void UBuildPlacementWidget::SpendDecorationCost() const
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildPlacementWidget] ResourceItemManager not found!"));
		return;
	}

	// 장식품은 단일 가격 (Money 타입)
	ResourceMgr->SpendResource(EResourceType::Money, DecorationInfo.Price);
	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementWidget] Spent %d money for decoration"), DecorationInfo.Price);
}
