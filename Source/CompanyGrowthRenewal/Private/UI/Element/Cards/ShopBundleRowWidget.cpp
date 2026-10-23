#include "UI/Element/Cards/ShopBundleRowWidget.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Engine/Font.h"
#include "Framework/Text/TextLayout.h"   // ETextOverflowPolicy
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Common/IconBrushUtils.h"

namespace
{
	// 기본 Slate 폰트에는 한글 글리프가 없다 — 지정하지 않으면 구성품 이름이 박스로 찍힌다
	UFont* LoadNexonFace(bool bBold)
	{
		return TSoftObjectPtr<UFont>(FSoftObjectPath(bBold
			? TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font")
			: TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();
	}
}

TSharedRef<SWidget> UShopBundleRowWidget::RebuildWidget()
{
	BuildSelfTree();
	ApplyEntry();
	return Super::RebuildWidget();
}

void UShopBundleRowWidget::ApplyInk(UCommonTextBlock* Text, const FLinearColor& Ink, float Size, bool bBold)
{
	if (!Text)
	{
		return;
	}
	// ⚠ CommonTextBlock 은 SynchronizeProperties 때마다 Style CDO 로 폰트와 색을 덮는다
	Text->SetStyle(nullptr);
	if (UFont* Face = LoadNexonFace(bBold))
	{
		// FontObject 가 없는 채로 SetFont 하면 텍스트가 통째로 사라진다 — 로드 성공일 때만
		FSlateFontInfo Font(Face, FMath::RoundToInt(Size));
		Font.TypefaceFontName = TEXT("Default");
		Text->SetFont(Font);
	}
	Text->SetColorAndOpacity(FSlateColor(Ink));
}

void UShopBundleRowWidget::BuildSelfTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		// 디자이너가 트리를 넣어 뒀으면 그쪽이 이긴다 — BindWidgetOptional 이 이미 물려 있다
		return;
	}

	UOverlay* RowRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RowRoot"));
	WidgetTree->RootWidget = RowRoot;

	// 아래로 RidgeDepth 만큼 밀어 둔 같은 모양 = 타일의 두께
	UImage* Ridge = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RowRidge"));
	{
		FSlateBrush RidgeBrush;
		RidgeBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		RidgeBrush.TintColor = FSlateColor(RidgeColor);
		RidgeBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		RidgeBrush.OutlineSettings.CornerRadii = FVector4(PlateRadius, PlateRadius, PlateRadius, PlateRadius);
		Ridge->SetBrush(RidgeBrush);
		Ridge->SetVisibility(ESlateVisibility::HitTestInvisible);
		FWidgetTransform RidgeT;
		RidgeT.Translation = FVector2D(0.f, RidgeDepth);
		Ridge->SetRenderTransform(RidgeT);
	}
	if (UOverlaySlot* RidgeSlot = RowRoot->AddChildToOverlay(Ridge))
	{
		RidgeSlot->SetHorizontalAlignment(HAlign_Fill);
		RidgeSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RowPlate"));
	{
		FSlateBrush PlateBrush;
		PlateBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PlateBrush.TintColor = FSlateColor(PlateColor);
		PlateBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		PlateBrush.OutlineSettings.CornerRadii = FVector4(PlateRadius, PlateRadius, PlateRadius, PlateRadius);
		PlateBrush.OutlineSettings.Color = FSlateColor(PlateRingColor);
		PlateBrush.OutlineSettings.Width = 1.5f;
		Plate->SetBrush(PlateBrush);
		Plate->SetPadding(FMargin(11.f, 7.f, 11.f, 7.f));
		Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (UOverlaySlot* PlateSlot = RowRoot->AddChildToOverlay(Plate))
	{
		PlateSlot->SetHorizontalAlignment(HAlign_Fill);
		PlateSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RowHBox"));
	Plate->SetContent(Row);

	USizeBox* Well = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IconWell"));
	Well->SetWidthOverride(IconSize + 16.0f);
	Well->SetHeightOverride(IconSize + 16.0f);
	IconWellBox = Well;

	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
	ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	Well->AddChild(ItemIcon);

	ItemNameText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("ItemNameText"));
	ItemNameText->SetVisibility(ESlateVisibility::HitTestInvisible);
	// 긴 이름이 수량을 밀어내던 것을 여기서 자른다 (선례 CodexPanelWidget.cpp).
	// Ellipsis 는 의도 표현이고, 겹침 금지는 ClipToBounds 가 보증한다 —
	// SHorizontalBox 는 과밀해도 자르지 않고 배치만 해서 글자가 수량 위로 올라탄다.
	ItemNameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	ItemNameText->SetAutoWrapText(false);
	ItemNameText->SetClipping(EWidgetClipping::ClipToBounds);
	ApplyInk(ItemNameText, InkColor, NameFontSize, /*bBold*/ false);

	ItemQtyText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("ItemQtyText"));
	ItemQtyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyInk(ItemQtyText, QtyColor, NameFontSize + 1.0f, /*bBold*/ true);

	// C4458 회피 — 부모 UWidget 에 Slot 멤버가 있어 지역명으로 쓰면 빌드가 깨진다
	if (UHorizontalBoxSlot* WellSlot = Row->AddChildToHorizontalBox(Well))
	{
		WellSlot->SetVerticalAlignment(VAlign_Center);
		WellSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
	}
	if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(ItemNameText))
	{
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	USizeBox* QtyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("QtyBox"));
	QtyBox->SetMinDesiredWidth(QtyMinWidth);
	QtyBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (USizeBoxSlot* QtyInner = Cast<USizeBoxSlot>(QtyBox->AddChild(ItemQtyText)))
	{
		QtyInner->SetHorizontalAlignment(HAlign_Right);
		QtyInner->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* QtySlot = Row->AddChildToHorizontalBox(QtyBox))
	{
		QtySlot->SetVerticalAlignment(VAlign_Center);
		QtySlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	}
}

void UShopBundleRowWidget::SetEntry(EItemType Item, int32 Quantity)
{
	PendingItem = Item;
	PendingQuantity = Quantity;
	ApplyEntry();
}

void UShopBundleRowWidget::ApplyEntry()
{
	const EItemType Item = PendingItem;
	const int32 Quantity = PendingQuantity;
	if (Item == EItemType::None)
	{
		return;
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(GetItemTypeDisplayName(Item));
	}

	if (ItemQtyText)
	{
		// 수량은 1개짜리도 붙인다 — 구성품은 "몇 개인지"가 정보다
		ItemQtyText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), Quantity)));
	}

	if (ItemIcon)
	{
		bool bFound = false;
		UObject* Resource = nullptr;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				Resource = TableMgr->GetItemIconResource(Item, bFound);
			}
		}
		// 텍스처/머티리얼 공용 경로
		CGIconBrush::ApplyResource(ItemIcon, Resource, FVector2D(IconSize, IconSize));
		// 못 찾으면 접는다 — Hidden 은 자리를 남겨 기본 흰 브러시가 빈 막대로 보인다
		ItemIcon->SetVisibility(bFound ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (IconWellBox)
		{
			IconWellBox->SetVisibility(bFound ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}
