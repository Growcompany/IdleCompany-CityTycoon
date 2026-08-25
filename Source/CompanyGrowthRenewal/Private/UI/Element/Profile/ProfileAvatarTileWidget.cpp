#include "UI/Element/Profile/ProfileAvatarTileWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Table/ProfileImageData.h"

void UProfileAvatarTileWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsSelectable(true);
	SetIsToggleable(false);

	ApplySelectionVisual(GetSelected());
}

void UProfileAvatarTileWidget::SetProfileImage(const FProfileImageData& InData)
{
	ImageID = InData.ImageID;

	if (ThumbImage && !InData.Icon.IsNull())
	{
		if (UTexture2D* Tex = InData.Icon.LoadSynchronous())
		{
			ThumbImage->SetBrushFromTexture(Tex);
		}
	}
}

void UProfileAvatarTileWidget::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	ApplySelectionVisual(true);
}

void UProfileAvatarTileWidget::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	ApplySelectionVisual(false);
}

void UProfileAvatarTileWidget::ApplySelectionVisual(bool bChosen)
{
	// 브래킷/배지는 썸네일 클릭을 가로채면 안 되므로 보일 때도 HitTestInvisible.
	const ESlateVisibility Vis = bChosen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (SelectBracket)
	{
		SelectBracket->SetVisibility(Vis);
	}
	if (UseBadge)
	{
		UseBadge->SetVisibility(Vis);
	}
}
