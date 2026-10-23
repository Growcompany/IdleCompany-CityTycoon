#include "UI/Element/Trade/DrawerInfoRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

void UDrawerInfoRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ActionButton)
	{
		ActionButton->OnClicked().AddUObject(this, &UDrawerInfoRowWidget::HandleActionClicked);
	}

	if (!bWarnedMissingBindings && (!NameText || !ValueText))
	{
		bWarnedMissingBindings = true;
		UE_LOG(LogTemp, Warning, TEXT("[DrawerInfoRow] %s 바인딩 누락 Name=%d Value=%d — author_drawer_onepage.py 저작 확인"),
			*GetClass()->GetName(), NameText != nullptr, ValueText != nullptr);
	}
}

void UDrawerInfoRowWidget::NativeDestruct()
{
	if (ActionButton)
	{
		ActionButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UDrawerInfoRowWidget::SetRow(UTexture2D* Icon, const FText& Number, const FText& Name, bool bDimName,
	const FText& Detail, const FText& Value, EDrawerRowTone ValueTone)
{
	auto ApplyText = [](UCommonTextBlock* Block, const FText& InText)
	{
		if (!Block)
		{
			return;
		}
		Block->SetText(InText);
		Block->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	};

	if (IconImage)
	{
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	ApplyText(NumberText, Number);
	ApplyText(NameText, Name);
	bNameDimmed = bDimName;
	ApplyNameInk();
	ApplyText(DetailText, Detail);
	ApplyText(ValueText, Value);
	if (ValueText)
	{
		ValueText->SetColorAndOpacity(InkForTone(ValueTone));
	}
}

void UDrawerInfoRowWidget::SetAction(const FText& Label, bool bEnabled)
{
	if (!ActionButton)
	{
		return;
	}
	ActionButton->SetButtonText(Label);
	ActionButton->SetIsEnabled(bEnabled);
	bHasAction = !Label.IsEmpty();
	ApplyActionVisibility();
}

void UDrawerInfoRowWidget::SetLocked(bool bInLocked)
{
	bLocked = bInLocked;
	if (RowHeight)
	{
		RowHeight->SetMinDesiredHeight(bLocked ? LockedRowMinHeight : NormalRowMinHeight);
	}
	if (LockIcon)
	{
		LockIcon->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	ApplyNameInk();
	ApplyActionVisibility();
}

void UDrawerInfoRowWidget::SetEmptyPlate(bool bShow)
{
	if (EmptyPlate)
	{
		EmptyPlate->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UDrawerInfoRowWidget::ApplyNameInk()
{
	if (NameText)
	{
		NameText->SetColorAndOpacity(bLocked || bNameDimmed ? NameDimInk : NameInk);
	}
}

void UDrawerInfoRowWidget::ApplyActionVisibility()
{
	const bool bShow = bHasAction && !bLocked;
	if (ActionButton)
	{
		ActionButton->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ActionSize)
	{
		ActionSize->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UDrawerInfoRowWidget::HandleActionClicked()
{
	ActionClicked.Broadcast(this);
}

const FSlateColor& UDrawerInfoRowWidget::InkForTone(EDrawerRowTone Tone) const
{
	switch (Tone)
	{
	case EDrawerRowTone::Good: return ToneGoodInk;
	case EDrawerRowTone::Pending: return TonePendingInk;
	case EDrawerRowTone::Muted: return ToneMutedInk;
	default: return ToneNormalInk;
	}
}
