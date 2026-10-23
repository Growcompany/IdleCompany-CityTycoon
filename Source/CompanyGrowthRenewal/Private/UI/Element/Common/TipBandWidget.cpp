#include "UI/Element/Common/TipBandWidget.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UI/Slate/SLoadingTipBand.h"

void UTipBandWidget::SetTip(const FText& InTip)
{
	PreviewTip = InTip;
	if (Band.IsValid())
	{
		Band->SetBody(InTip.ToString());
	}
}

TSharedRef<SWidget> UTipBandWidget::RebuildWidget()
{
	// 폰트를 안 지정하면 엔진 Roboto 로 떨어져 한글이 깨진다 ― 기본값을 여기서 깔아 둔다
	if (!BodyFont)
	{
		BodyFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();
	}
	if (!LabelFont)
	{
		LabelFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"))).LoadSynchronous();
	}
	if (!IconTexture)
	{
		IconTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/TipBulb.TipBulb"))).LoadSynchronous();
	}

	Band = SNew(SLoadingTipBand)
		.Style(Style)
		.Body(PreviewTip.ToString())
		.BodyFont(BodyFont)
		.LabelFont(LabelFont)
		.IconTexture(IconTexture);
	return Band.ToSharedRef();
}

void UTipBandWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!Band.IsValid())
	{
		return;
	}
	// 디자이너에서 숫자를 바꾸면 즉시 반영된다
	Band->SetStyle(Style);
	Band->SetBody(PreviewTip.ToString());
	Band->SetFonts(BodyFont, LabelFont);
	Band->SetIcon(IconTexture);
}

void UTipBandWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Band.Reset();
}
