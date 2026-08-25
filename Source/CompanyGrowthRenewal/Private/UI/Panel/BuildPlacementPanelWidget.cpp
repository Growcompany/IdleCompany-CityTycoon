// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/BuildPlacementPanelWidget.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "UI/UISoundTags.h"

#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Player/OfficePlayerController.h"
#include "Player/PlayerCamera.h"
#include "Player/OfficeCameraPawn.h"
#include "Player/Components/PlacementHandler.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "UI/Panel/BuildingManagePanelWidget.h"
#include "Components/Button.h"
#include "Table/ConstructionCost.h"
#include "Table/ResourceInfo.h"
#include "Enum/NotificationType.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Office/OfficeManager.h"
#include "UI/UIBase.h"
#include "Enum/WidgetType.h"
#include "Enum/ResourceType.h"
#include "Blueprint/UserWidget.h"

void UBuildPlacementPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->RegisterBuildPlacementPanel(this);
	}

	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	APlayerController* CurrentPC = GameInstance->GetCurrentPlayerController();

	// MainMap인 경우
	if (AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(CurrentPC))
	{
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
		PlaceButton->OnClicked.AddDynamic(this, &UBuildPlacementPanelWidget::OnPlaceButtonClicked);
	}
	if (RotateButton)
	{
		RotateButton->OnClicked.AddDynamic(this, &UBuildPlacementPanelWidget::OnRotateButtonClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UBuildPlacementPanelWidget::OnCancelButtonClicked);
	}

	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		ResourceChangedHandle = UIMgr->OnUIResourceChanged.AddUObject(
			this, &UBuildPlacementPanelWidget::HandleResourceChanged);
	}

	// 건물 배치 시작 시 카메라 줌인 (기존 FocusOnBuildingForPlacement 재사용)
	if (Player && Player->PlacementHandler)
	{
		Player->PlacementHandler->BeginPlacementZoom();
	}
}

void UBuildPlacementPanelWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	RefreshPlaceButtonEnabled();
}

void UBuildPlacementPanelWidget::NativeDestruct()
{
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>() : nullptr)
	{
		MissionMgr->UnregisterBuildPlacementPanel(this);
	}

	Super::NativeDestruct();
	if (PlaceButton)
	{
		PlaceButton->OnClicked.RemoveAll(this);
	}
	if (RotateButton)
	{
		RotateButton->OnClicked.RemoveAll(this);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.RemoveAll(this);
	}
	if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIMgr->OnUIResourceChanged.Remove(ResourceChangedHandle);
	}
}

UWidget* UBuildPlacementPanelWidget::GetPlaceButtonWidget() const
{
	return PlaceButton;
}

void UBuildPlacementPanelWidget::OnPlaceButtonClicked()
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
			Player->EndBuildingRelocation();
			DeactivateWidget();
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Building moved successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementPanelWidget] Failed to move building"));
		}
	}
	else if (bIsWorkstationMode)
	{
		// 업무공간 배치 (연속 배치 모드)
		bool bCapacityReached = false;
		bSuccess = Player->PlacementHandler->PlacingWorkstation(&bCapacityReached);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Workstation placed successfully"));
			if (bCapacityReached)
			{
				if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Player))
				{
					OfficePawn->EndWorkstationPlacement();
				}
				CloseOfficePlacementUI();
				return;
			}

			RefreshPlaceButtonEnabled();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementPanelWidget] Failed to place workstation"));
		}
	}
	else if (bIsDecorationMode)
	{
		if (!TryAffordCurrent())
		{
			return;
		}

		// 장식품 배치 (1회 배치)
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
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Decoration placed successfully"));
			SpendDecorationCost();

			// 장식은 하나 놓으면 끝 — 배치 세션과 카탈로그를 함께 닫고 사무실로 돌아간다.
			if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Player))
			{
				OfficePawn->EndDecorationPlacement();
			}
			CloseOfficePlacementUI();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementPanelWidget] Failed to place decoration"));
		}
	}
	else
	{
		// 자재 재확인 — 배치 중 자원이 빠졌을 수 있으니 확정 직전 한 번 더 (2차 방어선). 자원명은 DT_Resource 단일 진실.
		if (UResourceItemManager* ResMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
		{
			EResourceType MissingType = EResourceType::None;
			if (!ResMgr->CanAffordCosts(BuildableInfo.ConstructionCosts, MissingType))
			{
				UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
				UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
				if (UIMgr)
				{
					bool bNameOk = false;
					const FResourceInfo ResInfo = TableMgr ? TableMgr->GetResourceInfo(MissingType, bNameOk) : FResourceInfo();
					const FText Msg = FText::Format(
						NSLOCTEXT("Notification", "BuildNotEnoughResource", "{0}이(가) 부족합니다"), ResInfo.DisplayName);
					UIMgr->ShowNotification(Msg, 3.0f, ENotificationType::Warning);
				}
				return;
			}
		}

		// 신규 건물 배치
		bSuccess = Player->PlacementHandler->PlacingEntity();

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Building placed successfully"));
			SpendConstructionCost();
			if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
			{
				SM->PlayUISound(CGUISoundTags::PurchaseSuccess);
			}
			// 건물 카운터 +1 연출은 확정 시점에만 (프리뷰 스폰도 EntityManager 카운트에 잡혀 diff 방식은 선택만 해도 발화)
			if (UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
			{
				if (UInGameLayerWidget* InGame = UIMgr->GetInGameLayer())
				{
					InGame->SpawnGainPopup(EResourceType::Building, 1);
					if (UResourceWidget* BuildingCounter = InGame->GetResourceWidget(EResourceType::Building))
					{
						BuildingCounter->PlayBump();
					}
				}
			}
			Player->EndBuild();
			DeactivateWidget();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildPlacementPanelWidget] Failed to place building"));
		}
	}
}

void UBuildPlacementPanelWidget::OnRotateButtonClicked()
{
	if (Player)
	{
		Player->PlacementHandler->RotatePlacementEntity();
	}
}

void UBuildPlacementPanelWidget::OnCancelButtonClicked()
{
	if (!Player)
	{
		DeactivateWidget();
		return;
	}

	if (bIsMovingExistingBuilding)
	{
		// 기존 건물 재배치 취소 -> BuildManagePanel로 복귀.
		// CancelBuildingMove 가 PlacementTargetEntity 를 null 로 리셋하므로, 대상 건물을 먼저 확보한 뒤 취소한다.
		AInteractableBaseActor* TargetEntity = Player->PlacementHandler->GetPlacementTargetEntity();
		ABuildingBaseActor* ExistingBuilding = Cast<ABuildingBaseActor>(TargetEntity);

		Player->PlacementHandler->CancelBuildingMove();

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
						DeactivateWidget();

						UCommonActivatableWidget* PushedWidget = UIBase->PushBottomClass(ManagePanelClass.Get());
						UBuildingManagePanelWidget* ManagePanel = Cast<UBuildingManagePanelWidget>(PushedWidget);
						if (ManagePanel && ExistingBuilding)
						{
							ManagePanel->SetTargetBuilding(ExistingBuilding);
							Player->EndBuildingRelocation();

							AMainMapPlayerController* MainPC = Cast<AMainMapPlayerController>(Player->GetController());
							if (MainPC)
							{
								MainPC->GoToUIMode();
							}

							UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Returned to BuildManagePanel after cancel"));
						}
					}
				}
			}
		}
	}
	else if (bIsWorkstationMode)
	{
		if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Player))
		{
			OfficePawn->EndWorkstationPlacement();
		}
		// 배치 바만 닫으면 아래 깔린 카탈로그가 다시 드러난다 — 꾸미기의 종착지는 사무실이어야 한다.
		CloseOfficePlacementUI();
		UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Workstation placement ended"));
	}
	else if (bIsDecorationMode)
	{
		if (AOfficeCameraPawn* OfficePawn = Cast<AOfficeCameraPawn>(Player))
		{
			OfficePawn->EndDecorationPlacement();
		}
		CloseOfficePlacementUI();
		UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Decoration placement cancelled"));
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

void UBuildPlacementPanelWidget::SetBuildableInfo(const FBuildableCardTable& InBuildableInfo, bool bIsMoving)
{
	BuildableInfo = InBuildableInfo;
	bIsMovingExistingBuilding = bIsMoving;
}

void UBuildPlacementPanelWidget::CheckHasConstructionCost() const
{
	// BuildEntityCardWidget에서 이미 체크
}

void UBuildPlacementPanelWidget::SpendConstructionCost() const
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildPlacementPanelWidget] ResourceItemManager not found!"));
		return;
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	UInGameLayerWidget* InGame = UIMgr ? UIMgr->GetInGameLayer() : nullptr;

	for (const FConstructionCost& Cost : BuildableInfo.ConstructionCosts)
	{
		ResourceMgr->SpendResource(Cost.ResourceType, Cost.Cost);
		if (InGame)
		{
			InGame->SpawnSpendPopup(Cost.ResourceType, Cost.Cost);
		}
		UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Spent %lld of ResourceType %d"),
			Cost.Cost, (int32)Cost.ResourceType);
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] All construction costs paid"));
}

void UBuildPlacementPanelWidget::SetWorkstationInfo(const FWorkstationCardTable& InWorkstationInfo)
{
	WorkstationInfo = InWorkstationInfo;
	bIsWorkstationMode = true;
	bIsMovingExistingBuilding = false;

	RefreshPlaceButtonEnabled();

	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] SetWorkstationInfo: %s"), *WorkstationInfo.DisplayName.ToString());
}

void UBuildPlacementPanelWidget::SetDecorationInfo(const FDecorationCardTable& InDecorationInfo)
{
	DecorationInfo = InDecorationInfo;
	bIsDecorationMode = true;
	bIsWorkstationMode = false;
	bIsMovingExistingBuilding = false;

	RefreshPlaceButtonEnabled();

	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] SetDecorationInfo: %s"), *DecorationInfo.Name.ToString());
}

void UBuildPlacementPanelWidget::SpendDecorationCost() const
{
	UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>();
	if (!ResourceMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildPlacementPanelWidget] ResourceItemManager not found!"));
		return;
	}

	ResourceMgr->SpendResource(EResourceType::Money, DecorationInfo.Price);
	UE_LOG(LogTemp, Log, TEXT("[BuildPlacementPanelWidget] Spent %d money for decoration"), DecorationInfo.Price);
}

bool UBuildPlacementPanelWidget::CanAffordCurrent() const
{
	if (bIsWorkstationMode)
	{
		return true;
	}

	UResourceItemManager* ResMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!ResMgr)
	{
		return true; // 매니저 부재는 잔액 문제가 아니다 — 기존 경로 그대로 통과시키고 로그로 드러나게 둔다
	}

	if (bIsDecorationMode)
	{
		return ResMgr->HasResource(EResourceType::Money, DecorationInfo.Price);
	}

	return true; // 건물 신축/이동은 각자 게이트가 따로 있다
}

bool UBuildPlacementPanelWidget::TryAffordCurrent() const
{
	if (CanAffordCurrent())
	{
		return true;
	}

	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!UIMgr)
	{
		return false;
	}

	// 부족한 자원 이름은 DT_Resource 단일 진실 — 건물 경로와 같은 문구를 쓴다.
	const EResourceType MissingType = EResourceType::Money;

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	bool bNameOk = false;
	const FResourceInfo ResInfo = TableMgr ? TableMgr->GetResourceInfo(MissingType, bNameOk) : FResourceInfo();
	const FText Msg = FText::Format(
		NSLOCTEXT("Notification", "BuildNotEnoughResource", "{0}이(가) 부족합니다"), ResInfo.DisplayName);
	UIMgr->ShowNotification(Msg, 3.0f, ENotificationType::Warning);

	return false;
}

void UBuildPlacementPanelWidget::RefreshPlaceButtonEnabled()
{
	if (PlaceButton)
	{
		PlaceButton->SetIsEnabled(CanAffordCurrent());
	}
}

void UBuildPlacementPanelWidget::CloseOfficePlacementUI()
{
	// 자기 자신만 닫으면 바로 OfficeMain 이 드러난다 — 카탈로그는 배치 시작 시점에 이미 닫혔다
	// (카드 위젯이 push 전에 pop 한다). 여기서 카탈로그까지 닫으려 하면 안 된다:
	// CommonUI 는 표시 중인 위젯의 제거를 전환 완료 후로 미루므로, 연속 pop 은 같은 위젯을 두 번 겨냥하고
	// 두 번째는 아무 일도 하지 않는다(RemoveWidget 의 IsActivated() 분기).
	DeactivateWidget();
}
