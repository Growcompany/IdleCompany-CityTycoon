#include "UI/Element/Common/HelpTableRowWidget.h"

#include "CommonTextBlock.h"

void UHelpTableRowWidget::SetRow(const FHelpTableRow& InRow)
{
	if (LabelText) { LabelText->SetText(InRow.Label); }
	if (ValueText) { ValueText->SetText(InRow.Value); }
	if (NoteText)
	{
		NoteText->SetText(InRow.Note);
		// 빈 비고를 빈 칸으로 남기면 열 정렬만 흐트러진다
		NoteText->SetVisibility(InRow.Note.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}
