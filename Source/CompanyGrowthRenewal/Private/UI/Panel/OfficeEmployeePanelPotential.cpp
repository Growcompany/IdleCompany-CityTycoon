// 사무실 직원 패널 — 잠재능력 탭 (공용 잠재 워크스페이스 호스팅 + 확률표 오버레이 + 상점 왕복). 설계 §4
#include "UI/Panel/OfficeEmployeePanelWidget.h"

#include "CommonActivatableWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Core/CGGameInstance.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"
#include "UI/Element/Employee/PotentialResetWorkspaceWidget.h"
#include "UI/Panel/PotentialOddsWidget.h"

namespace OfficeEmployeePanelPotential
{
	// TabSwitcher 자식 순서 (0 성장 / 1 잠재능력 / 2 장비)
	constexpr int32 PotentialTabIndex = 1;
	// 직원창 확률표와 같은 층 — 패널(스택) 위, 확인 다이얼로그(10000) 아래
	constexpr int32 OddsOverlayZOrder = 120;
}

bool UOfficeEmployeePanelWidget::IsPotentialTabShown() const
{
	return TabSwitcher && TabSwitcher->GetActiveWidgetIndex() == OfficeEmployeePanelPotential::PotentialTabIndex;
}

void UOfficeEmployeePanelWidget::SyncPotentialWorkspace()
{
	// 활성화 전(push 직후 탭 지정)에는 선택 직원이 아직 없다 — 활성화의 SelectEmployee 가 다시 부른다
	if (!IsPotentialTabShown() || !IsActivated() || SelectedEmployeeId == INDEX_NONE || !PotentialHost)
	{
		return;
	}
	if (!PotentialWorkspace)
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		// 패널 전용 변형(한 화면·스크롤 없음) — 공용 직원창 워크스페이스는 고정 높이라 이 판에 안 들어간다
		const TSubclassOf<UUserWidget> WorkspaceClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::OfficePotentialWorkspace) : nullptr;
		if (!WorkspaceClass || !WorkspaceClass->IsChildOf(UPotentialResetWorkspaceWidget::StaticClass()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] OfficePotentialWorkspace 클래스 미등록/불일치 — DT_WidgetClass 확인"));
			return;
		}
		PotentialWorkspace = Cast<UPotentialResetWorkspaceWidget>(CreateWidget(GetOwningPlayer(), WorkspaceClass.Get()));
		if (!PotentialWorkspace)
		{
			return;
		}
	}
	// Configure 전에 붙여야 워크스페이스 NativeConstruct 가 매니저를 잡는다
	if (PotentialWorkspace->GetParent() != PotentialHost)
	{
		if (UOverlaySlot* HostSlot = PotentialHost->AddChildToOverlay(PotentialWorkspace))
		{
			HostSlot->SetHorizontalAlignment(HAlign_Fill);
			HostSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	PotentialWorkspace->OnRequestOddsTable.BindUObject(this, &ThisClass::OpenOddsTable);
	PotentialWorkspace->OnRequestShop.BindUObject(this, &ThisClass::HandlePotentialShopRequested);
	PotentialWorkspace->Configure(SelectedEmployeeId);
}

void UOfficeEmployeePanelWidget::SuspendPotentialWorkspace()
{
	if (PotentialWorkspace)
	{
		PotentialWorkspace->Suspend();
	}
}

void UOfficeEmployeePanelWidget::OpenOddsTable(int32 EmployeeId)
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> OddsClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PotentialOddsPanel) : nullptr;
	if (!OddsClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] PotentialOddsPanel 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}
	// 연타 시 딤이 겹치지 않게 열린 것을 걷고 새로 연다
	CloseOddsTable();
	// ⚠ ActivateWidget 금지 — 뷰포트 루트 활성화의 Menu 입력 설정이 닫힌 뒤에도 남는다(직원창 선례)
	if (UPotentialOddsWidget* Odds = Cast<UPotentialOddsWidget>(CreateWidget(GetOwningPlayer(), OddsClass.Get())))
	{
		Odds->AddToViewport(OfficeEmployeePanelPotential::OddsOverlayZOrder);
		OpenOddsPanel = Odds;
	}
}

bool UOfficeEmployeePanelWidget::CloseOddsTable()
{
	UPotentialOddsWidget* Odds = OpenOddsPanel.Get();
	OpenOddsPanel.Reset();
	// 확률표가 스스로 닫혔으면 부모가 없다
	if (!Odds || !Odds->IsInViewport())
	{
		return false;
	}
	Odds->RemoveFromParent();
	return true;
}

void UOfficeEmployeePanelWidget::HandlePotentialShopRequested()
{
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> ShopClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::ShopPanel) : nullptr;
	if (ShopClass)
	{
		PushPromptAndReturn(ShopClass.Get(), nullptr, OfficeEmployeePanelPotential::PotentialTabIndex);
	}
}

UCommonActivatableWidget* UOfficeEmployeePanelWidget::TestOpenLegacyEmployeeWindow(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	const TSubclassOf<UUserWidget> WindowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::EmployeeWindow) : nullptr;
	if (!UIBase || !WindowClass || !WindowClass->IsChildOf(UCommonActivatableWidget::StaticClass()))
	{
		return nullptr;
	}
	return UIBase->PushPromptClass(WindowClass.Get());
}

void UOfficeEmployeePanelWidget::PushPromptAndReturn(UClass* PromptClass, TFunction<void(UCommonActivatableWidget&)> InitPrompt, int32 ReturnTabIndex)
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	UUIManagerSubsystem* UIManager = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	if (!UIBase || !PromptClass || !PromptClass->IsChildOf(UCommonActivatableWidget::StaticClass()))
	{
		return;
	}

	// 스택 전환 = 닫기 먼저, push 나중. 패널은 BottomStack 이라 PromptStack 의 창이 닫혀도 저절로 살아나지 않는다
	// — 그 창이 닫히면 같은 직원·같은 탭으로 다시 연다(직원창 상점 왕복과 같은 결과)
	const int32 ReturnEmployeeId = SelectedEmployeeId;
	const TWeakObjectPtr<UWorld> WeakWorld = GetWorld();
	DeactivateWidget();
	UCommonActivatableWidget* Prompt = UIBase->PushPromptClass(PromptClass, [&InitPrompt](UCommonActivatableWidget& Widget)
	{
		if (InitPrompt)
		{
			InitPrompt(Widget);
		}
	});
	if (!Prompt)
	{
		PushForEmployee(GI, ReturnEmployeeId);
		return;
	}

	// 창이 스택에서 빠져 슬레이트가 해제될 때(닫기 전환 완료) 한 번만 돌아온다. 그 위에 다른 창을 쌓은 경우는
	// 해제되지 않으므로 끼어들지 않는다. 창 인스턴스는 풀에서 재사용되니 쏘고 나면 스스로 뗀다
	TSharedRef<FDelegateHandle> ReturnHandle = MakeShared<FDelegateHandle>();
	*ReturnHandle = Prompt->OnNativeDestruct.AddWeakLambda(GI, [ReturnHandle, WeakWorld, ReturnEmployeeId, ReturnTabIndex](UUserWidget* ClosedPrompt)
	{
		if (ClosedPrompt)
		{
			ClosedPrompt->OnNativeDestruct.Remove(*ReturnHandle);
		}
		UWorld* World = WeakWorld.Get();
		if (!World || World->bIsTearingDown)
		{
			return;
		}
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(World, [World, ReturnEmployeeId, ReturnTabIndex]()
		{
			UGameInstance* ReturnGI = World->GetGameInstance();
			UUIManagerSubsystem* ReturnUI = ReturnGI ? ReturnGI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
			if (!ReturnUI || !ReturnUI->GetUIBase() || ReturnUI->GetUIBase()->GetPromptStackCount() > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[OfficeEmployeePanel] 닫힌 창 뒤에 다른 창이 남아 패널 복귀 생략"));
				return;
			}
			if (UOfficeEmployeePanelWidget* Panel = PushForEmployee(World, ReturnEmployeeId))
			{
				Panel->SelectTab(ReturnTabIndex);
			}
		}));
	});
}
