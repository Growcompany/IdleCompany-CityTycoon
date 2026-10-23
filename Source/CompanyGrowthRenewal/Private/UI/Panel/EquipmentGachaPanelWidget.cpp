#include "UI/Panel/EquipmentGachaPanelWidget.h"
#include "UI/UIBase.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "UI/Element/Gacha/GachaPanelFrameWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/GameInstance.h"

void UEquipmentGachaPanelWidget::Open(int32 EmployeeId)
{
	EquipmentEmployeeId = EmployeeId;
	ConfigureWorkspace();
}

UWidget* UEquipmentGachaPanelWidget::GetCloseButtonWidget() const
{
	return GachaFrame ? GachaFrame->GetCloseButton() : nullptr;
}

UGachaRevealPresentationWidget* UEquipmentGachaPanelWidget::GetActiveReveal() const
{
	return EquipmentWorkspace ? EquipmentWorkspace->GetRevealPresentation() : nullptr;
}

UWidget* UEquipmentGachaPanelWidget::GetPullOneButtonWidget() const
{
	return EquipmentWorkspace ? EquipmentWorkspace->GetPullOneButtonWidget() : nullptr;
}

UWidget* UEquipmentGachaPanelWidget::GetNormalTicketTabWidget() const
{
	return EquipmentWorkspace ? EquipmentWorkspace->GetNormalTicketButtonWidget() : nullptr;
}

bool UEquipmentGachaPanelWidget::IsNormalTicketSelected() const
{
	return EquipmentWorkspace && EquipmentWorkspace->IsNormalTicketSelected();
}

void UEquipmentGachaPanelWidget::ConfigureWorkspace()
{
	if (!EquipmentGachaHost) return;
	if (!EquipmentWorkspace)
	{
		// Open 은 NativeConstruct 전(PushPromptClass 초기화 람다)에도 오므로 서브시스템을 직접 찾는다
		const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		const auto WidgetClass = Tables ? Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentWorkspace) : nullptr;
		EquipmentWorkspace = WidgetClass ? CreateWidget<UEmployeeEquipmentWorkspaceWidget>(GetOwningPlayer(), WidgetClass) : nullptr;
		if (!EquipmentWorkspace)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EquipmentGachaPanel] EmployeeEquipmentWorkspace is not registered"));
			return;
		}
	}
	EquipmentWorkspace->ConfigureGachaHost(EquipmentEmployeeId);
	if (!EquipmentWorkspace->GetParent())
	{
		UOverlaySlot* HostSlot = EquipmentGachaHost->AddChildToOverlay(EquipmentWorkspace);
		HostSlot->SetHorizontalAlignment(HAlign_Fill);
		HostSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UEquipmentGachaPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCloseButtonWidget* CloseButton = GachaFrame ? GachaFrame->GetCloseButton() : nullptr)
	{
		CloseButton->OnCloseClicked.AddUniqueDynamic(this, &UEquipmentGachaPanelWidget::HandleCloseClicked);
	}

	// 초기화 람다 없이 열린 경우에도 본문이 비지 않게
	if (!EquipmentWorkspace) ConfigureWorkspace();
}

void UEquipmentGachaPanelWidget::NativeDestruct()
{
	if (UCloseButtonWidget* CloseButton = GachaFrame ? GachaFrame->GetCloseButton() : nullptr)
	{
		CloseButton->OnCloseClicked.RemoveAll(this);
	}
	if (EquipmentWorkspace)
	{
		EquipmentWorkspace->DismissDialog();
		EquipmentWorkspace->SuspendPreview();
	}
	EquipmentEmployeeId = INDEX_NONE;

	Super::NativeDestruct();
}

void UEquipmentGachaPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (auto* PC = Cast<AMainMapPlayerController>(GetOwningPlayer())) PC->GoToUIMode();
	if (EquipmentWorkspace) EquipmentWorkspace->Resume();
}

void UEquipmentGachaPanelWidget::NativeOnDeactivated()
{
	// 권이 모자라 상점에 다녀오는 동안은 UI 모드를 유지한다 — 복귀 후 같은 권종·배너로 돌아온다
	const bool bReturningFromShop = EquipmentWorkspace && EquipmentWorkspace->IsReturningFromShop();
	if (EquipmentWorkspace)
	{
		EquipmentWorkspace->DismissDialog();
		EquipmentWorkspace->SuspendPreview();
	}
	if (!bReturningFromShop)
	{
		if (auto* PC = Cast<AMainMapPlayerController>(GetOwningPlayer()))
		{
			if (PC->GetCurrentInputMode() == EInputMode::UI) PC->GoToNormalMode();
		}
	}
	Super::NativeOnDeactivated();
}

bool UEquipmentGachaPanelWidget::NativeOnHandleBackAction()
{
	if (!bIsBackHandler) return false;
	if (!CGBackNav::IsTopWindow(*this))
	{
		return false;
	}
	HandleCloseClicked();
	return true;
}

void UEquipmentGachaPanelWidget::HandleCloseClicked()
{
	// 결과 목록·상세 대화가 열려 있으면 그것부터 닫는다
	if (EquipmentWorkspace && EquipmentWorkspace->HandleBack()) return;
	DeactivateWidget();
}
