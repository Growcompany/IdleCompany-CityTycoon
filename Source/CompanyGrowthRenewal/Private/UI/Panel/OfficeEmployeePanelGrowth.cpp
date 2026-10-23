// 사무실 직원 패널 — 성장 탭 (레벨업 카드 + 보유 SP + 능력치 6행). 설계 §3
#include "UI/Panel/OfficeEmployeePanelWidget.h"

#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Enum/WidgetType.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Employee/EmployeeLevelUpCardWidget.h"
#include "UI/Element/Employee/OfficeEmployeeStatRowWidget.h"

namespace OfficeEmployeePanelGrowth
{
	// 직원창과 같은 표시 순서 — 크리티컬 두 줄을 붙여 둔다
	constexpr EEmployeeStatIndex DisplayOrder[] = {
		EEmployeeStatIndex::WorkSpeed, EEmployeeStatIndex::CritChance, EEmployeeStatIndex::CritDamage,
		EEmployeeStatIndex::Composure, EEmployeeStatIndex::ExpGain, EEmployeeStatIndex::Stamina };
	static_assert(UE_ARRAY_COUNT(DisplayOrder) == static_cast<int32>(EEmployeeStatIndex::Count),
		"능력치 행 수가 EEmployeeStatIndex 와 어긋난다");

	// 목업 .orows2 gap
	constexpr float RowGap = 16.f;
	// 끝까지 내렸을 때 마지막 행이 아래 페이드(WBP StatFade, 실효 ≈100)에 덮이지 않게 남기는 여백
	constexpr float ScrollEndPadding = 100.f;
}

void UOfficeEmployeePanelWidget::BuildGrowthRows()
{
	// 스택이 위젯을 재사용하면 NativeConstruct 가 다시 온다 — 행은 한 번만 만든다
	if (!StatScroll || StatRows.Num() > 0)
	{
		return;
	}
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> RowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::OfficeEmployeeStatRow) : nullptr;
	if (!RowClass || !RowClass->IsChildOf(UOfficeEmployeeStatRowWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeePanel] OfficeEmployeeStatRow 클래스 미등록 — DT_WidgetClass 확인"));
		return;
	}

	const int32 LastIndex = UE_ARRAY_COUNT(OfficeEmployeePanelGrowth::DisplayOrder) - 1;
	for (const EEmployeeStatIndex Stat : OfficeEmployeePanelGrowth::DisplayOrder)
	{
		UOfficeEmployeeStatRowWidget* Row = CreateWidget<UOfficeEmployeeStatRowWidget>(this, RowClass);
		if (!Row)
		{
			continue;
		}
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(StatScroll->AddChild(Row)))
		{
			RowSlot->SetPadding(FMargin(0.f, StatRows.Num() > 0 ? OfficeEmployeePanelGrowth::RowGap : 0.f, 0.f,
				StatRows.Num() == LastIndex ? OfficeEmployeePanelGrowth::ScrollEndPadding : 0.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Row->Configure(SelectedEmployeeId, Stat);
		StatRows.Add(Row);
	}
}

void UOfficeEmployeePanelWidget::RefreshGrowth()
{
	if (LevelUpCard)
	{
		LevelUpCard->Configure(SelectedEmployeeId);
	}
	if (StatPointText)
	{
		const int32 AvailablePoints = EmployeeManager && SelectedEmployeeId != INDEX_NONE
			? EmployeeManager->GetAvailableStatPoints(SelectedEmployeeId) : 0;
		StatPointText->SetText(FText::AsNumber(AvailablePoints));
	}
	for (UOfficeEmployeeStatRowWidget* Row : StatRows)
	{
		if (Row)
		{
			Row->Configure(SelectedEmployeeId, Row->GetStat());
		}
	}
}

UWidget* UOfficeEmployeePanelWidget::GetCloseButtonWidget() const
{
	return CloseButton;
}

UWidget* UOfficeEmployeePanelWidget::GetLevelUpButtonWidget() const
{
	return IsActivated() && LevelUpCard ? LevelUpCard->GetLevelUpButtonWidget() : nullptr;
}

UWidget* UOfficeEmployeePanelWidget::GetStatPointTargetWidget() const
{
	if (!IsActivated())
	{
		return nullptr;
	}
	// 투자할 수 있는 첫 버튼, 없으면 첫 버튼 — 꺼진 버튼이면 안내가 「직원 바꾸기」 단계로 떨어진다
	UWidget* FirstButton = nullptr;
	for (const UOfficeEmployeeStatRowWidget* Row : StatRows)
	{
		UButtonWidget* Button = Row ? Row->GetInvestButton() : nullptr;
		if (!Button)
		{
			continue;
		}
		if (Button->GetIsEnabled())
		{
			return Button;
		}
		FirstButton = FirstButton ? FirstButton : Button;
	}
	return FirstButton;
}
