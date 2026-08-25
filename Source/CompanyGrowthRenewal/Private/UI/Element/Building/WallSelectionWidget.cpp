// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/WallSelectionWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UWallSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 클릭 이벤트 바인딩
	if (LeftWallButton)
	{
		LeftWallButton->OnClicked.AddDynamic(this, &UWallSelectionWidget::OnLeftWallButtonClicked);
	}

	if (RightWallButton)
	{
		RightWallButton->OnClicked.AddDynamic(this, &UWallSelectionWidget::OnRightWallButtonClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UWallSelectionWidget::OnCancelButtonClicked);
	}

	// 초기 텍스트 설정 (블루프린트에서 설정하지 않은 경우)
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("어느 벽에 배치할까요?")));
	}

	if (LeftWallText)
	{
		LeftWallText->SetText(FText::FromString(TEXT("왼쪽 벽")));
	}

	if (RightWallText)
	{
		RightWallText->SetText(FText::FromString(TEXT("오른쪽 벽")));
	}

	if (CancelText)
	{
		CancelText->SetText(FText::FromString(TEXT("취소")));
	}

	// 처음에는 숨김 상태
	SetVisibility(ESlateVisibility::Collapsed);
}

void UWallSelectionWidget::NativeDestruct()
{
	if (LeftWallButton)
	{
		LeftWallButton->OnClicked.RemoveDynamic(this, &UWallSelectionWidget::OnLeftWallButtonClicked);
	}
	if (RightWallButton)
	{
		RightWallButton->OnClicked.RemoveDynamic(this, &UWallSelectionWidget::OnRightWallButtonClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.RemoveDynamic(this, &UWallSelectionWidget::OnCancelButtonClicked);
	}

	Super::NativeDestruct();
}

void UWallSelectionWidget::ShowWidget()
{
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Log, TEXT("[WallSelectionWidget] Showing wall selection UI"));
}

void UWallSelectionWidget::HideWidget()
{
	SetVisibility(ESlateVisibility::Collapsed);
	UE_LOG(LogTemp, Log, TEXT("[WallSelectionWidget] Hiding wall selection UI"));
}

void UWallSelectionWidget::OnLeftWallButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[WallSelectionWidget] Left wall selected"));

	// 델리게이트 브로드캐스트
	OnWallSelected.Broadcast(EWallSide::Left);

	// UI 숨김
	HideWidget();
}

void UWallSelectionWidget::OnRightWallButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[WallSelectionWidget] Right wall selected"));

	// 델리게이트 브로드캐스트
	OnWallSelected.Broadcast(EWallSide::Right);

	// UI 숨김
	HideWidget();
}

void UWallSelectionWidget::OnCancelButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[WallSelectionWidget] Wall selection cancelled"));

	// 델리게이트 브로드캐스트
	OnSelectionCancelled.Broadcast();

	// UI 숨김
	HideWidget();
}
