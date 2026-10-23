// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeWorkstationPanelWidget.h"
#include "Engine/GameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/ListView.h"
#include "Data/EntityCardData.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "UI/Panel/OfficeMainWidget.h"
#include "UI/Element/Office/CatalogEdgeFade.h"
#include "UI/Element/Office/OfficeWorkstationCardWidget.h"

UWidget* UOfficeWorkstationPanelWidget::GetDeskGuideCatalogWidget() const
{
	if (!IsActivated() || !WorkstationListView || !WorkstationListView->GetIsEnabled())
	{
		return nullptr;
	}
	const ESlateVisibility ListVisibility = WorkstationListView->GetVisibility();
	if (ListVisibility == ESlateVisibility::Hidden || ListVisibility == ESlateVisibility::Collapsed)
	{
		return nullptr;
	}

	for (UUserWidget* Entry : WorkstationListView->GetDisplayedEntryWidgets())
	{
		UOfficeWorkstationCardWidget* Card = Cast<UOfficeWorkstationCardWidget>(Entry);
		if (Card && Card->GetVisibility() != ESlateVisibility::Hidden
			&& Card->GetVisibility() != ESlateVisibility::Collapsed
			&& Card->GetIsEnabled() && !Card->IsLocked())
		{
			return WorkstationListView;
		}
	}
	return nullptr;
}

void UOfficeWorkstationPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RelocateButton) RelocateButton->OnClicked().AddUObject(this, &ThisClass::OnRelocateClicked);

	if (BackButton) BackButton->OnCloseClicked.AddUniqueDynamic(this, &ThisClass::OnBackButtonClicked);
	if (UWidget* ListFade = GetWidgetFromName(TEXT("CatalogListFade"))) BindCatalogEdgeFade(this, Cast<UTileView>(WorkstationListView), ListFade);
	else UE_LOG(LogTemp, Warning, TEXT("[OfficeWorkstationPanelWidget] CatalogListFade 미배치 ― 카드 끝 흐림 없음 (WBP 확인)"));

	// TableManager 가져오기
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	}
	else
	{
		TableManager = nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] NativeConstruct"));
}

void UOfficeWorkstationPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	RefreshWorkstationCards();

	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] NativeOnActivated"));
}

void UOfficeWorkstationPanelWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	if (UMissionManagerSubsystem* MissionMgr = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyPlacementCatalogClosed();
	}

	// ListView 정리
	if (WorkstationListView)
	{
		WorkstationListView->ClearListItems();
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] NativeOnDeactivated"));
	if (bPendingPlacementEdit)
	{
		bPendingPlacementEdit = false;
		if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
			if (UOfficeMainWidget* Main = UIManager->GetOfficeMain()) Main->RequestPlacementEdit();
	}
}

void UOfficeWorkstationPanelWidget::NativeDestruct()
{
	if (RelocateButton) RelocateButton->OnClicked().RemoveAll(this);
	if (BackButton) BackButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::OnBackButtonClicked);
	if (WorkstationListView) WorkstationListView->OnListViewScrolled().RemoveAll(this);

	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] NativeDestruct"));

	Super::NativeDestruct();
}

void UOfficeWorkstationPanelWidget::RefreshWorkstationCards()
{
	if (!WorkstationListView || !TableManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeWorkstationPanelWidget] WorkstationListView or TableManager is null"));
		return;
	}

	WorkstationListView->ClearListItems();

	TArray<FWorkstationCardTable> Workstations = TableManager->GetWorkstationCardsByType(EWorkstationType::Single);

	// 데이터 추가 (위젯은 ListView가 자동 생성)
	for (const FWorkstationCardTable& WorkstationData : Workstations)
	{
		UEntityCardData* Data = NewObject<UEntityCardData>(this);
		Data->WorkstationInfo = WorkstationData;
		WorkstationListView->AddItem(Data);
	}
	ScheduleCatalogEdgeFade(this, Cast<UTileView>(WorkstationListView), GetWidgetFromName(TEXT("CatalogListFade")));

	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] Loaded %d single workstations"), Workstations.Num());
}

UWidget* UOfficeWorkstationPanelWidget::GetCloseButtonWidget() const
{
	return BackButton;
}

void UOfficeWorkstationPanelWidget::OnBackButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] Back button clicked"));
	CloseWithAnimation();
}

void UOfficeWorkstationPanelWidget::OnRelocateClicked()
{
	if (IsCloseRequested()) return;
	bPendingPlacementEdit = true;
	CloseWithAnimation();
}
