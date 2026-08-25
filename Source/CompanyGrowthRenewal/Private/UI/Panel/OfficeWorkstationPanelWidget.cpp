// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeWorkstationPanelWidget.h"
#include "Engine/GameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/ListView.h"
#include "Data/EntityCardData.h"
#include "UI/Element/Buttons/ButtonWidget.h"

void UOfficeWorkstationPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Back 버튼 이벤트 바인딩
	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &UOfficeWorkstationPanelWidget::OnBackButtonClicked);
	}

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

	// ListView 정리
	if (WorkstationListView)
	{
		WorkstationListView->ClearListItems();
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] NativeOnDeactivated"));
}

void UOfficeWorkstationPanelWidget::NativeDestruct()
{
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}

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

	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] Loaded %d single workstations"), Workstations.Num());
}

void UOfficeWorkstationPanelWidget::OnBackButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeWorkstationPanelWidget] Back button clicked"));
	CloseWithAnimation();
}
