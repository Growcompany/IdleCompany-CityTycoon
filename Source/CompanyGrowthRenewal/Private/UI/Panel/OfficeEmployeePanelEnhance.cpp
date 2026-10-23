// 사무실 직원 패널 — ★ 강화 카드 (EnhanceStarforceModalWidget 의 카드 WBP 를 CardHost 에 띄움). 설계 §6
#include "UI/Panel/OfficeEmployeePanelWidget.h"

#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Panel/EnhanceStarforceModalWidget.h"

void UOfficeEmployeePanelWidget::ShowEnhanceCard()
{
	if (!CardHost || SelectedEmployeeId == INDEX_NONE)
	{
		return;
	}
	if (!EnhanceCard)
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		const TSubclassOf<UUserWidget> CardClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::OfficeEnhanceCard) : nullptr;
		if (!CardClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] OfficeEnhanceCard 클래스 미등록 — DT_WidgetClass 확인"));
			return;
		}
		EnhanceCard = Cast<UEnhanceStarforceModalWidget>(CreateWidget(GetOwningPlayer(), CardClass.Get()));
		if (!EnhanceCard)
		{
			return;
		}
	}

	// 카드 자리는 한 번에 하나 — 장비 상세는 미리 입혀 보기까지 걷고, 그 밖의 카드는 내린다
	CloseEquipDetail();
	for (int32 Index = CardHost->GetChildrenCount() - 1; Index >= 0; --Index)
	{
		UWidget* Child = CardHost->GetChildAt(Index);
		if (Child && Child != EnhanceCard)
		{
			Child->RemoveFromParent();
		}
	}
	// ⚠ ActivateWidget 금지 — 패널(Menu 입력)이 계속 입력을 소유한다. 카드 X 는 스스로 RemoveFromParent 한다
	if (EnhanceCard->GetParent() != CardHost)
	{
		if (UOverlaySlot* CardSlot = CardHost->AddChildToOverlay(EnhanceCard))
		{
			// 카드 자리는 장비 상세 폭(680)에 맞춰 넓다 — ★ 카드는 제 폭(600)으로 패널 쪽에 붙인다
			CardSlot->SetHorizontalAlignment(HAlign_Right);
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	// 카드 판을 눌러도 뒤의 빈칸 닫기 버튼으로 새지 않게 카드가 히트를 받는다
	EnhanceCard->SetVisibility(ESlateVisibility::Visible);
	EnhanceCard->Configure(SelectedEmployeeId);
}

bool UOfficeEmployeePanelWidget::IsEnhanceCardShown() const
{
	return EnhanceCard && CardHost && EnhanceCard->GetParent() == CardHost;
}

void UOfficeEmployeePanelWidget::SyncEnhanceCard()
{
	if (IsEnhanceCardShown() && SelectedEmployeeId != INDEX_NONE)
	{
		EnhanceCard->Configure(SelectedEmployeeId);
	}
}

bool UOfficeEmployeePanelWidget::CloseEnhanceCard()
{
	if (!IsEnhanceCardShown())
	{
		return false;
	}
	EnhanceCard->RemoveFromParent();
	return true;
}
