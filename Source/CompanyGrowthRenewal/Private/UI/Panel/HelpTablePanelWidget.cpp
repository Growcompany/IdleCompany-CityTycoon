#include "UI/Panel/HelpTablePanelWidget.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Enum/WidgetType.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Common/HelpTableRowWidget.h"

void UHelpTablePanelWidget::Configure(UObject* InProvider, FName InHelpKey, const FText& InCaption)
{
	ProviderObject = InProvider;
	HelpKey = InHelpKey;
	Caption = InCaption;
	PageIndex = 0;
	AxisIndex = 0;

	IHelpTableProvider* Source = Cast<IHelpTableProvider>(ProviderObject);
	Info = FHelpTableInfo();
	if (Source) { Source->GetHelpTableInfo(HelpKey, Info); }

	if (TitleText) { TitleText->SetText(Info.Title); }
	if (CaptionText)
	{
		CaptionText->SetText(Caption);
		CaptionText->SetVisibility(Caption.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (FootnoteText)
	{
		FootnoteText->SetText(Info.Footnote);
		FootnoteText->SetVisibility(Info.Footnote.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	RebuildAxisTabs();
	RefreshPager();
	RefreshRows();
}

int32 UHelpTablePanelWidget::GetRenderedRowCount() const
{
	return RowBox ? RowBox->GetChildrenCount() : 0;
}

void UHelpTablePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (PrevButton)
	{
		PrevButton->OnClicked.RemoveDynamic(this, &UHelpTablePanelWidget::HandlePrevClicked);
		PrevButton->OnClicked.AddDynamic(this, &UHelpTablePanelWidget::HandlePrevClicked);
	}
	if (NextButton)
	{
		NextButton->OnClicked.RemoveDynamic(this, &UHelpTablePanelWidget::HandleNextClicked);
		NextButton->OnClicked.AddDynamic(this, &UHelpTablePanelWidget::HandleNextClicked);
	}
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UHelpTablePanelWidget::HandleCloseClicked);
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UHelpTablePanelWidget::HandleCloseClicked);
	}
}

void UHelpTablePanelWidget::NativeDestruct()
{
	if (PrevButton)  { PrevButton->OnClicked.RemoveDynamic(this, &UHelpTablePanelWidget::HandlePrevClicked); }
	if (NextButton)  { NextButton->OnClicked.RemoveDynamic(this, &UHelpTablePanelWidget::HandleNextClicked); }
	if (UIE_CloseButton) { UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UHelpTablePanelWidget::HandleCloseClicked); }
	if (AxisGroup)   { AxisGroup->OnButtonBaseClicked.RemoveAll(this); }
	Super::NativeDestruct();
}

void UHelpTablePanelWidget::RebuildAxisTabs()
{
	if (!AxisBox) { return; }
	AxisBox->ClearChildren();
	if (AxisGroup) { AxisGroup->OnButtonBaseClicked.RemoveAll(this); }
	AxisGroup = nullptr;

	// 고를 게 하나뿐인 전환기는 자리만 먹는다
	if (!ShouldShowAxisTabs(Info.AxisLabels.Num()))
	{
		AxisBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	AxisBox->SetVisibility(ESlateVisibility::Visible);

	UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> TabClass = TableMgr
		? TableMgr->GetWidgetClass(EWidgetType::HelpAxisTab) : nullptr;
	if (!TabClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Help] EWidgetType::HelpAxisTab 미등록 ― 항목 탭을 그릴 수 없습니다"));
		AxisBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 배타 선택은 CommonButtonGroupBase 가 소유한다 ― SetIsSelected 수동 토글은
	// 클릭과 타이밍이 꼬여 두 탭이 동시에 눌린 상태가 된다 (CLAUDE.md 탭 패턴)
	AxisGroup = NewObject<UCommonButtonGroupBase>(this);
	AxisGroup->SetSelectionRequired(true);

	for (int32 Index = 0; Index < Info.AxisLabels.Num(); ++Index)
	{
		UTabButtonWidget* Tab = CreateWidget<UTabButtonWidget>(this, TabClass);
		if (!Tab) { continue; }
		UCommonButtonBase* Button = Tab->GetButton();
		if (!Button)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Help] HelpAxisTab 내부 TabButton 바인딩이 없습니다"));
			continue;
		}
		if (UHorizontalBoxSlot* BoxSlot = AxisBox->AddChildToHorizontalBox(Tab))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		Tab->SetTabText(Info.AxisLabels[Index]);
		Button->SetIsSelectable(true);
		AxisGroup->AddWidget(Button);
	}
	AxisGroup->OnButtonBaseClicked.AddDynamic(this, &UHelpTablePanelWidget::HandleAxisChanged);
	AxisGroup->SelectButtonAtIndex(0);
	AxisIndex = 0;
}

void UHelpTablePanelWidget::RefreshPager()
{
	if (!PagerBox) { return; }
	if (!ShouldShowPager(Info.PageLabels.Num()))
	{
		PagerBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	PagerBox->SetVisibility(ESlateVisibility::Visible);
	if (PageText && Info.PageLabels.IsValidIndex(PageIndex))
	{
		PageText->SetText(Info.PageLabels[PageIndex]);
	}
	// 양끝에서 비활성 ― 순환시키면 지금 몇 번째인지 감각이 사라진다
	if (PrevButton) { PrevButton->SetIsEnabled(PageIndex > 0); }
	if (NextButton) { NextButton->SetIsEnabled(PageIndex < Info.PageLabels.Num() - 1); }
}

void UHelpTablePanelWidget::RefreshRows()
{
	if (!RowBox) { return; }
	RowBox->ClearChildren();

	IHelpTableProvider* Source = Cast<IHelpTableProvider>(ProviderObject);
	if (!Source) { return; }

	UTableManagerSubsystem* TableMgr = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> RowClass = TableMgr
		? TableMgr->GetWidgetClass(EWidgetType::HelpTableRow) : nullptr;
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Help] EWidgetType::HelpTableRow 미등록 ― 표를 그릴 수 없습니다"));
		return;
	}

	// 보이는 칸만 그때그때 만든다 ― 10티어 x 6산업 x 2항목이면 섹션이 120개다
	TArray<FHelpTableRow> Rows;
	Source->GetHelpTableRows(HelpKey, PageIndex, AxisIndex, Rows);
	for (const FHelpTableRow& Row : Rows)
	{
		UHelpTableRowWidget* RowWidget = CreateWidget<UHelpTableRowWidget>(this, RowClass);
		if (!RowWidget) { continue; }
		RowWidget->SetRow(Row);
		RowBox->AddChildToVerticalBox(RowWidget);
	}
}

void UHelpTablePanelWidget::HandlePrevClicked()
{
	if (PageIndex <= 0) { return; }
	--PageIndex;
	RefreshPager();
	RefreshRows();
}

void UHelpTablePanelWidget::HandleNextClicked()
{
	if (PageIndex >= Info.PageLabels.Num() - 1) { return; }
	++PageIndex;
	RefreshPager();
	RefreshRows();
}

void UHelpTablePanelWidget::HandleAxisChanged(UCommonButtonBase* Button, int32 Index)
{
	if (AxisIndex == Index) { return; }
	AxisIndex = Index;
	RefreshRows();
}

void UHelpTablePanelWidget::HandleCloseClicked()
{
	DeactivateWidget();
}
