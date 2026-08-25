// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/MinePickerWidget.h"
#include "UI/Element/Cards/MineResourceCardWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MineManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Enum/ResourceType.h"
#include "UI/UISoundTags.h"
#include "Enum/WidgetType.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"

void UMinePickerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UMinePickerWidget::HandleCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UMinePickerWidget::HandleBackgroundClicked);
	}
}

void UMinePickerWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UMinePickerWidget::HandleCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UMinePickerWidget::HandleBackgroundClicked);
	}
	for (UMineResourceCardWidget* Card : SpawnedCards)
	{
		if (Card)
		{
			Card->OnSelected.RemoveDynamic(this, &UMinePickerWidget::HandleResourceSelected);
		}
	}
	SpawnedCards.Reset();
	Super::NativeDestruct();
}

void UMinePickerWidget::InitializeForCountry(ECountryType Country)
{
	CurrentCountry = Country;

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("채광 자원 추가")));
	}

	RebuildResourceList();
}

void UMinePickerWidget::RebuildResourceList()
{
	if (!ResourceScrollBox) return;

	// 기존 카드 정리
	for (UMineResourceCardWidget* Card : SpawnedCards)
	{
		if (Card)
		{
			Card->OnSelected.RemoveDynamic(this, &UMinePickerWidget::HandleResourceSelected);
		}
	}
	ResourceScrollBox->ClearChildren();
	SpawnedCards.Reset();

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UMineManager* MineMgr = GI->GetSubsystem<UMineManager>();
	if (!TableMgr || !MineMgr) return;

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::MineResourceCard);
	if (!CardClass) return;

	const TArray<EResourceType> Available = MineMgr->GetAvailableResources(CurrentCountry);
	for (EResourceType Resource : Available)
	{
		UMineResourceCardWidget* Card = CreateWidget<UMineResourceCardWidget>(this, CardClass);
		if (!Card) continue;

		Card->SetResource(CurrentCountry, Resource);
		Card->OnSelected.AddDynamic(this, &UMinePickerWidget::HandleResourceSelected);

		ResourceScrollBox->AddChild(Card);
		SpawnedCards.Add(Card);
	}
}

void UMinePickerWidget::HandleResourceSelected(EResourceType Resource, int64 Quantity)
{
	UGameInstance* GI = GetGameInstance();
	UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	if (!MineMgr) return;

	if (!MineMgr->CreateMineLine(CurrentCountry, Resource, Quantity))
	{
		// 실패인데 성공과 똑같이 닫으면 아무 일도 없던 것처럼 보인다 — 모달을 유지하고 이유를 알린다.
		UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
		if (!UIMgr) { return; }

		UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
		const int64 Cost = MineMgr->GetCreateLineCost(CurrentCountry, Resource, Quantity);
		if (Cost > 0 && ResMgr && !ResMgr->HasResource(EResourceType::Money, Cost))
		{
			UIMgr->NotifyInsufficientResource(EResourceType::Money, Cost);
			return;
		}

		// 슬롯 한도/중복/풀 미포함 — 어느 쪽이든 이 화면에서 플레이어가 바꿀 게 없다
		UIMgr->ShowRejectNotification(
			NSLOCTEXT("MinePicker", "CreateLineFailed", "채굴 라인을 만들 수 없습니다"));
		return;
	}

	// 성공 — 모달 닫기. CountryDetailWidget 의 OnLineStorageChanged 핸들러가 라인 위젯 동기화 책임지진 않으므로
	// 닫힌 후 CountryDetailWidget 가 RebuildMineTab 으로 라인 새로 spawn 함이 필요. 추후 매니저에 OnLineCreated 델리게이트 추가 가능.
	// MVP: 모달 닫고 사용자가 탭 다시 진입하거나, CountryDetailWidget 가 모달 close 시점에 갱신 트리거.
	// AddToViewport 로 추가됐으므로 RemoveFromParent 가 정통 close.
	// (DeactivateWidget 은 stack 안에 있을 때만 의미 있음)
	PlayCloseSound();
	RemoveFromParent();
}

void UMinePickerWidget::HandleCloseClicked()
{
	// AddToViewport 로 추가됐으므로 RemoveFromParent 가 정통 close.
	// (DeactivateWidget 은 stack 안에 있을 때만 의미 있음)
	PlayCloseSound();
	RemoveFromParent();
}

void UMinePickerWidget::HandleBackgroundClicked()
{
	// AddToViewport 로 추가됐으므로 RemoveFromParent 가 정통 close.
	// (DeactivateWidget 은 stack 안에 있을 때만 의미 있음)
	PlayCloseSound();
	RemoveFromParent();
}

void UMinePickerWidget::PlayCloseSound()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ModalClose);
		}
	}
}
