#include "UI/Element/Employee/EquipmentFilterComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UEquipmentFilterComboBox::RebuildWidget()
{
	TSharedRef<SWidget> ComboWidget = Super::RebuildWidget();
	ResetSelectedLabel();
	return ComboWidget;
}

void UEquipmentFilterComboBox::HandleSelectionChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectionType)
{
	Super::HandleSelectionChanged(Item, SelectionType);
	ResetSelectedLabel();
}

void UEquipmentFilterComboBox::ResetSelectedLabel()
{
	if (!ComboBoxContent.IsValid()) return;

	TSharedRef<STextBlock> SelectedLabel = SNew(STextBlock)
		.Text(FText::FromString(GetSelectedOption()))
		.Font(GetFont())
		.ColorAndOpacity(GetForegroundColor());

	// ComboBoxString reuses this text block during SetSelectedIndex's second content update.
	DefaultComboBoxContent = SelectedLabel;
	ComboBoxContent->SetContent(SelectedLabel);
}
