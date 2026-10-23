#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/TileView.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "InputCoreTypes.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"

void UEmployeeEquipmentDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ModalCloseButton) ModalCloseButton->OnCloseClicked.AddDynamic(this, &ThisClass::HandleClose);
	if (PrimaryButton) PrimaryButton->OnClicked().AddUObject(this, &ThisClass::HandlePrimary);
	if (SecondaryButton) SecondaryButton->OnClicked().AddUObject(this, &ThisClass::HandleSecondary);
	if (TertiaryButton) TertiaryButton->OnClicked().AddUObject(this, &ThisClass::HandleTertiary);
	if (ExtraButton) ExtraButton->OnClicked().AddUObject(this, &ThisClass::HandleExtra);
	if (ResultView)
	{
		ResultView->OnItemClicked().AddUObject(this, &ThisClass::HandleItemClicked);
		ResultView->OnItemSelectionChanged().AddUObject(this, &ThisClass::HandleItemSelection);
	}
}

void UEmployeeEquipmentDialogWidget::NativeDestruct()
{
	if (ModalCloseButton) ModalCloseButton->OnCloseClicked.RemoveAll(this);
	if (PrimaryButton) PrimaryButton->OnClicked().RemoveAll(this);
	if (SecondaryButton) SecondaryButton->OnClicked().RemoveAll(this);
	if (TertiaryButton) TertiaryButton->OnClicked().RemoveAll(this);
	if (ExtraButton) ExtraButton->OnClicked().RemoveAll(this);
	if (ResultView)
	{
		ResultView->OnItemClicked().RemoveAll(this);
		ResultView->OnItemSelectionChanged().RemoveAll(this);
	}
	CGEquipmentIcon::Apply(CurrentIconImage, nullptr);
	CGEquipmentIcon::Apply(SelectedIconImage, nullptr);
	CGEquipmentIcon::ApplyGradeFrame(CurrentGradeFrame, nullptr, FLinearColor::Transparent);
	CGEquipmentIcon::ApplyGradeFrame(SelectedGradeFrame, nullptr, FLinearColor::Transparent);
	CurrentIconMaterial = nullptr;
	SelectedIconMaterial = nullptr;
	Super::NativeDestruct();
}

void UEmployeeEquipmentDialogWidget::Configure(const FEmployeeEquipmentDialogView& View)
{
	if (TitleText) TitleText->SetText(View.Title);
	if (CurrentHeaderText) CurrentHeaderText->SetText(View.CurrentHeader);
	if (SelectedHeaderText) SelectedHeaderText->SetText(View.SelectedHeader);
	if (EffectsText)
	{
		EffectsText->SetText(View.Effects);
		EffectsText->SetVisibility(View.Effects.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ComparisonText)
	{
		ComparisonText->SetText(View.Comparison);
		ComparisonText->SetVisibility(View.Comparison.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(View.Description);
		DescriptionText->SetVisibility(View.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (CurrentHeaderBox) CurrentHeaderBox->SetVisibility(View.bShowCurrent ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	CGEquipmentIcon::Apply(CurrentIconImage, CGEquipmentIcon::Resolve(this, View.CurrentIcon,
		Tables ? Tables->FindEmployeeEquipmentGrade(View.CurrentVisualGrade) : nullptr, EEmployeeEquipmentIconContext::Focused, CurrentIconMaterial));
	CGEquipmentIcon::Apply(SelectedIconImage, CGEquipmentIcon::Resolve(this, View.SelectedIcon,
		Tables ? Tables->FindEmployeeEquipmentGrade(View.SelectedVisualGrade) : nullptr, EEmployeeEquipmentIconContext::Focused, SelectedIconMaterial));
	const auto* CurrentGrade = Tables && View.bShowCurrent && View.CurrentIcon ? Tables->FindEmployeeEquipmentGrade(View.CurrentGrade) : nullptr;
	const auto* SelectedGrade = Tables && View.SelectedIcon ? Tables->FindEmployeeEquipmentGrade(View.SelectedGrade) : nullptr;
	CGEquipmentIcon::ApplyGradeFrame(CurrentGradeFrame, CurrentGrade, CurrentGrade ? CurrentGrade->Color : FLinearColor::Transparent);
	CGEquipmentIcon::ApplyGradeFrame(SelectedGradeFrame, SelectedGrade, SelectedGrade ? SelectedGrade->Color : FLinearColor::Transparent);
	if (UWidget* SelectedHeaderBox = GetWidgetFromName(TEXT("SelectedHeaderBox"))) SelectedHeaderBox->SetVisibility(View.SelectedHeader.IsEmpty() && !View.SelectedIcon ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (CurrentIconImage) CurrentIconImage->SetVisibility(View.CurrentIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (SelectedIconImage) SelectedIconImage->SetVisibility(View.SelectedIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetNotice(View.Notice);
	UButtonWidget* Buttons[] = {PrimaryButton, SecondaryButton, TertiaryButton, ExtraButton};
	const FText Labels[] = {View.Primary, View.Secondary, View.Tertiary, View.Extra};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!Buttons[Index]) continue;
		Buttons[Index]->SetButtonText(Labels[Index]);
		Buttons[Index]->SetVisibility(Labels[Index].IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Buttons[Index]->SetIsEnabled(Index == 0 ? View.bPrimaryEnabled : Index != 1 || View.bSecondaryEnabled);
	}
	if (ModalBodySwitcher) ModalBodySwitcher->SetActiveWidgetIndex(0);
	if (ModalBody) ModalBody->ScrollToStart();
	if (PreviewImage) PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
	if (PreviewSize) PreviewSize->SetVisibility(ESlateVisibility::Collapsed);
}

void UEmployeeEquipmentDialogWidget::SetPreviewMaterial(UMaterialInterface* Material)
{
	if (!PreviewImage) return;
	PreviewImage->SetBrushFromMaterial(Material);
	PreviewImage->SetVisibility(Material ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (PreviewSize) PreviewSize->SetVisibility(Material ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UEmployeeEquipmentDialogWidget::ShowItems(const TArray<UObject*>& Items, bool bMultiSelect)
{
	if (ModalBodySwitcher) ModalBodySwitcher->SetActiveWidgetIndex(1);
	if (ResultView)
	{
		ResultView->SetSelectionMode(bMultiSelect ? ESelectionMode::Multi : ESelectionMode::Single);
		ResultView->SetListItems(Items);
		ResultView->ClearSelection();
	}
}

void UEmployeeEquipmentDialogWidget::RestoreSelectedItem(UObject* Item)
{
	if (ResultView && Item)
	{
		ResultView->SetSelectedItem(Item);
		ResultView->RequestScrollItemIntoView(Item);
	}
}

TArray<UObject*> UEmployeeEquipmentDialogWidget::GetSelectedItems() const
{
	TArray<UObject*> Items;
	if (ResultView) ResultView->GetSelectedItems(Items);
	return Items;
}

void UEmployeeEquipmentDialogWidget::RestoreSelectedItems(const TArray<UObject*>& Items)
{
	if (!ResultView) return;
	for (UObject* Item : Items) ResultView->SetItemSelection(Item, true);
}

void UEmployeeEquipmentDialogWidget::SetNotice(const FText& Text)
{
	if (NoticeText)
	{
		NoticeText->SetText(Text);
		NoticeText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UEmployeeEquipmentDialogWidget::SetPrimaryEnabled(bool bEnabled)
{
	if (PrimaryButton) PrimaryButton->SetIsEnabled(bEnabled);
}

FReply UEmployeeEquipmentDialogWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Android_Back)
	{
		HandleClose();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UEmployeeEquipmentDialogWidget::HandleClose() { OnClose.ExecuteIfBound(); }
void UEmployeeEquipmentDialogWidget::HandlePrimary() { OnPrimary.ExecuteIfBound(); }
void UEmployeeEquipmentDialogWidget::HandleSecondary() { OnSecondary.ExecuteIfBound(); }
void UEmployeeEquipmentDialogWidget::HandleTertiary() { OnTertiary.ExecuteIfBound(); }
void UEmployeeEquipmentDialogWidget::HandleExtra() { OnExtra.ExecuteIfBound(); }
void UEmployeeEquipmentDialogWidget::HandleItemClicked(UObject* Item) { OnItemClicked.ExecuteIfBound(Item); }
void UEmployeeEquipmentDialogWidget::HandleItemSelection(UObject* Item) { OnSelectionChanged.ExecuteIfBound(); }
