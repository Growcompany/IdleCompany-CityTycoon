// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/MenuPanelWidget.h"
#include "Components/Button.h"
#include "CommonButtonBase.h"
#include "UI/Element/Building/BuildingSkinCardWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Table/MenuUnlockData.h"
#include "Enum/WidgetType.h"
#include "Player/MainMapPlayerController.h"

void UMenuPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 바인딩
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UMenuPanelWidget::OnBackgroundClicked);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UMenuPanelWidget::OnCancelButtonClicked);
	}

	if (RankBtn)
	{
		RankBtn->OnClicked().AddUObject(this, &UMenuPanelWidget::OnRankButtonClicked);
	}

	if (GachaBtn)
	{
		GachaBtn->OnClicked().AddUObject(this, &UMenuPanelWidget::OnGachaButtonClicked);
	}

	if (SettingsBtn)
	{
		SettingsBtn->OnClicked.AddDynamic(this, &UMenuPanelWidget::OnSettingsButtonClicked);
	}
}

void UMenuPanelWidget::NativeDestruct()
{
	// 버튼 언바인딩
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UMenuPanelWidget::OnBackgroundClicked);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UMenuPanelWidget::OnCancelButtonClicked);
	}

	if (RankBtn)
	{
		RankBtn->OnClicked().RemoveAll(this);
	}

	if (GachaBtn)
	{
		GachaBtn->OnClicked().RemoveAll(this);
	}

	if (SettingsBtn)
	{
		SettingsBtn->OnClicked.RemoveDynamic(this, &UMenuPanelWidget::OnSettingsButtonClicked);
	}

	Super::NativeDestruct();
}

void UMenuPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 메뉴 버튼은 액션 버튼 — 선택 토글 비활성화
	if (RankBtn) { RankBtn->SetIsSelectable(false); RankBtn->SetIsSelected(false); }
	if (GachaBtn) { GachaBtn->SetIsSelectable(false); GachaBtn->SetIsSelected(false); }

	UpdateAllButtonLockStates();
}

void UMenuPanelWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// 닫기 애니메이션 완료 후 대기 중인 위젯 열기
	if (PendingOpenWidget != EWidgetType::None)
	{
		EWidgetType WidgetToOpen = PendingOpenWidget;
		PendingOpenWidget = EWidgetType::None;

		UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (UIMgr && UIMgr->GetUIBase() && TableMgr)
		{
			TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(WidgetToOpen);
			if (WidgetClass)
			{
				TSubclassOf<UCommonActivatableWidget> ActivatableClass(WidgetClass);
				UIMgr->GetUIBase()->PushPromptClass(ActivatableClass);
			}
		}
	}
	else
	{
		// 다음 위젯이 없으면 UI 모드 해제
		AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
		if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}
}

void UMenuPanelWidget::OnBackgroundClicked()
{
	ClosePanel();
}

void UMenuPanelWidget::OnCancelButtonClicked()
{
	ClosePanel();
}

void UMenuPanelWidget::OnRankButtonClicked()
{
	// 잠금 상태면 무시
	if (RankBtn && !RankBtn->IsUnlocked()) return;

	// 닫기 애니메이션 후 NativeOnDeactivated에서 랭킹 패널 열기
	PendingOpenWidget = EWidgetType::RankingPanel;
	ClosePanel();
}

void UMenuPanelWidget::OnGachaButtonClicked()
{
	// 글로벌 진입 — 컨텍스트 건물 없이 허브 기본값(특성/일반)으로 열림
	// 닫기 애니메이션 후 NativeOnDeactivated에서 특성 가챠 패널 열기
	PendingOpenWidget = EWidgetType::BuildingTraitGachaPanel;
	ClosePanel();
}

void UMenuPanelWidget::OnSettingsButtonClicked()
{
	// 닫기 애니메이션 후 NativeOnDeactivated에서 설정창 열기
	PendingOpenWidget = EWidgetType::SettingsPanel;
	ClosePanel();
}

void UMenuPanelWidget::UpdateAllButtonLockStates()
{
	UpdateButtonLockState(RankBtn, TEXT("RankBtn"));
}

void UMenuPanelWidget::UpdateButtonLockState(UBuildingSkinCardWidget* Btn, FName BtnName)
{
	if (!Btn) return;

	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!TableMgr || !SaveMgr) return;

	bool bSuccess = false;
	FMenuUnlockData Data = TableMgr->GetMenuUnlockData(BtnName, bSuccess);

	// 테이블에 없으면 기본 해금 (RequiredHQLevel=0)
	bool bLocked = bSuccess && (SaveMgr->GetHQLevel() < Data.RequiredHQLevel);
	Btn->SetLocked(bLocked);
}

void UMenuPanelWidget::ClosePanel()
{
	CloseWithAnimation();
}
