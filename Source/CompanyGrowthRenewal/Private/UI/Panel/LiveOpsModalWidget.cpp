// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/LiveOpsModalWidget.h"
#include "UI/UIBase.h"
#include "UI/Element/Common/ModalFrameWidget.h"

#include "Components/Button.h"
#include "Manager/GameClockSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"

ULiveOpsModalWidget::ULiveOpsModalWidget()
{
	bIsBackHandler = true;
}

bool ULiveOpsModalWidget::NativeOnHandleBackAction()
{
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	// 튜토리얼이 화면 입력을 막는 동안은 X 를 못 누르는 것처럼 삼킨다
	if (!CGBackNav::IsBlockedByGuide(*this, GetPanelCloseButton()))
	{
		ClosePanel();
	}
	return true;
}

void ULiveOpsModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &ULiveOpsModalWidget::OnBackgroundClicked);
	}

	if (UCloseButtonWidget* PanelCloseButton = GetPanelCloseButton())
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &ULiveOpsModalWidget::OnCloseClicked);
	}
}

void ULiveOpsModalWidget::NativeDestruct()
{
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &ULiveOpsModalWidget::OnBackgroundClicked);
	}

	if (UCloseButtonWidget* PanelCloseButton = GetPanelCloseButton())
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &ULiveOpsModalWidget::OnCloseClicked);
	}

	Super::NativeDestruct();
}

void ULiveOpsModalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UGameClockSubsystem* Clock = GetGameClock())
	{
		Clock->OnServerTimeValidityChanged.AddUObject(this, &ULiveOpsModalWidget::HandleServerTimeValidity);
		ApplyServerTimeWarn(Clock->IsServerTimeValid());
	}

	RefreshAll();
}

void ULiveOpsModalWidget::NativeOnDeactivated()
{
	if (UGameClockSubsystem* Clock = GetGameClock())
	{
		Clock->OnServerTimeValidityChanged.RemoveAll(this);
	}

	// 다음 창을 이어 열지 않는 단독 모달 — UI 모드 해제
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
	{
		PC->GoToNormalMode();
	}

	Super::NativeOnDeactivated();
}

void ULiveOpsModalWidget::ClosePanel()
{
	CloseWithAnimation();
}

bool ULiveOpsModalWidget::IsServerTimeValidNow() const
{
	const UGameClockSubsystem* Clock = GetGameClock();
	return Clock && Clock->IsServerTimeValid();
}

void ULiveOpsModalWidget::OnBackgroundClicked()
{
	ClosePanel();
}

void ULiveOpsModalWidget::OnCloseClicked()
{
	ClosePanel();
}

UGameClockSubsystem* ULiveOpsModalWidget::GetGameClock() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UGameClockSubsystem>() : nullptr;
}

void ULiveOpsModalWidget::ApplyServerTimeWarn(bool bValid)
{
	// 바인딩 대상은 밴드를 감싼 바깥 SizeBox 다 — 안쪽 플레이트만 접으면 슬롯 하단 패딩이 빈 띠로 남는다
	if (ServerTimeWarn)
	{
		ServerTimeWarn->SetVisibility(bValid ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void ULiveOpsModalWidget::HandleServerTimeValidity(bool bValid)
{
	ApplyServerTimeWarn(bValid);
	RefreshAll();
}

UCloseButtonWidget* ULiveOpsModalWidget::GetPanelCloseButton() const
{
	return ModalFrame ? ModalFrame->GetCloseButton() : nullptr;
}
