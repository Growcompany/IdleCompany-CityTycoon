#include "UI/Element/Gacha/GachaPanelFrameWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"

void UGachaPanelFrameWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyHeader();
}

void UGachaPanelFrameWidget::SetTitle(const FText& InTitle)
{
	Title = InTitle;
	if (PanelHeader) { PanelHeader->SetTitle(Title); }
}

void UGachaPanelFrameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 중첩 부품은 이 틀보다 먼저 Construct 되어 자기 키로 이미 판정을 끝냈다 ― 그래서 그 뒤에 넘겨 다시 풀게 한다
	if (PanelHeader)
	{
		PanelHeader->SetHelpKey(HelpKey);
	}
	if (Title.IsEmpty() && !IsDesignTime() && !bWarnedEmptyTitle)
	{
		bWarnedEmptyTitle = true;
		UE_LOG(LogTemp, Warning, TEXT("UGachaPanelFrameWidget %s: Title is empty, so the panel header shows no title. Set Title on the GachaFrame instance."), *GetPathName());
	}
}

UCloseButtonWidget* UGachaPanelFrameWidget::GetCloseButton() const
{
	return PanelHeader ? PanelHeader->GetCloseButton() : nullptr;
}

void UGachaPanelFrameWidget::ApplyHeader()
{
	if (!PanelHeader) { return; }
	PanelHeader->SetTitle(Title);
	PanelHeader->SetHeaderIcon(HeaderIconTexture);
	PanelHeader->SetHelpKey(HelpKey);
	PanelHeader->SetAppearance(ThemeKey, HeaderPatternKey, SideGradientKey);
}
