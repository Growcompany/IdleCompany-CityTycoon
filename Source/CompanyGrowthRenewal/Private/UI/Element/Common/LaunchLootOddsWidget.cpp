#include "UI/Element/Common/LaunchLootOddsWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Enum/WidgetType.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"

namespace
{
	const FColor RowFill(0xFF, 0xFF, 0xFF);
	const FColor InkColor(0xEC, 0xEE, 0xF0);
	const FColor SubColor(0xB9, 0xC2, 0xCF);
	const FColor MutedColor(0x97, 0xA3, 0xB6);
	constexpr float PopupWidth = 1064.0f;

	FSlateBrush MakeRoundedBrush(const FColor& Fill, float FillAlpha, float Radius, const FColor& Line, float LineAlpha, float LineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FLinearColor::FromSRGBColor(Fill).CopyWithNewOpacity(FillAlpha);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FLinearColor::FromSRGBColor(Line).CopyWithNewOpacity(LineAlpha);
		Brush.OutlineSettings.Width = LineWidth;
		return Brush;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, UFont* Font, int32 Size, const FColor& Color, const FText& Text)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo FontInfo(Font, Size);
		Block->SetFont(FontInfo);
		Block->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(Color)));
		Block->SetText(Text);
		return Block;
	}
}

FString ULaunchLootOddsWidget::GetBandAmountSuffix(const FLaunchLootPreviewEntry& Entry, int32 BandIndex)
{
	// public static — 외부 호출자의 잘못된 인덱스가 고정 배열[3] 밖을 읽지 않게 가드
	if (BandIndex < 0 || BandIndex > 2)
	{
		return FString();
	}
	const int32 AmtMin = Entry.BandAmountMin[BandIndex];
	const int32 AmtMax = Entry.BandAmountMax[BandIndex];
	if (AmtMax <= 1)
	{
		return FString();
	}
	return (AmtMin == AmtMax)
		? FString::Printf(TEXT("(%d장)"), AmtMax)
		: FString::Printf(TEXT("(%d~%d장)"), AmtMin, AmtMax);
}

TSharedRef<SWidget> ULaunchLootOddsWidget::RebuildWidget()
{
	EnsureTree();
	return Super::RebuildWidget();
}

void ULaunchLootOddsWidget::EnsureTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	BoldFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"))).LoadSynchronous();
	RegularFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> FrameClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::ModalFrame) : nullptr;
	ModalFrame = FrameClass ? CreateWidget<UModalFrameWidget>(this, FrameClass) : nullptr;
	if (!ModalFrame)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchLootOdds] DT_WidgetClass ModalFrame is missing or has the wrong type."));
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("OddsOverlay"));
	RootOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootOverlay;
	BackdropButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OddsBackdrop"));
	if (UOverlaySlot* BackdropSlot = RootOverlay->AddChildToOverlay(BackdropButton))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UScaleBox* FitBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("OddsFit"));
	FitBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	FitBox->SetStretch(EStretch::ScaleToFit);
	FitBox->SetStretchDirection(EStretchDirection::DownOnly);
	if (UOverlaySlot* FitSlot = RootOverlay->AddChildToOverlay(FitBox))
	{
		FitSlot->SetPadding(FMargin(72.f));
		FitSlot->SetHorizontalAlignment(HAlign_Fill);
		FitSlot->SetVerticalAlignment(VAlign_Fill);
	}
	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OddsWidthBox"));
	WidthBox->SetWidthOverride(PopupWidth);
	if (UScaleBoxSlot* CenterSlot = Cast<UScaleBoxSlot>(FitBox->AddChild(WidthBox)))
	{
		CenterSlot->SetHorizontalAlignment(HAlign_Center);
		CenterSlot->SetVerticalAlignment(VAlign_Center);
	}
	WidthBox->AddChild(ModalFrame);
	ModalFrame->SetTitle(NSLOCTEXT("LaunchLoot", "OddsWindowTitle", "출시 보상 확률"));
	ModalFrame->SetHelpKey(NAME_None);
	ModalFrame->SetAppearance(TEXT("PitchBoard"), NAME_None, NAME_None);

	UBorder* BodyPadding = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OddsBodyPadding"));
	FSlateBrush EmptyBrush;
	EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	BodyPadding->SetBrush(EmptyBrush);
	BodyPadding->SetPadding(FMargin(32.f));
	ModalFrame->SetContentForSlot(TEXT("Body"), BodyPadding);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OddsStack"));
	BodyPadding->SetContent(Stack);

	UTextBlock* Caption = MakeText(WidgetTree, RegularFont, 24, MutedColor,
		NSLOCTEXT("LaunchLoot", "OddsCaption", "출시 평점이 높을수록 좋은 보상이 나옵니다. 확률은 보상 1칸 기준입니다."));
	Caption->SetAutoWrapText(true);
	if (UVerticalBoxSlot* CaptionSlot = Stack->AddChildToVerticalBox(Caption))
	{
		CaptionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OddsRows"));
	USizeBox* RowsExtent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OddsRowsExtent"));
	RowsExtent->SetMaxDesiredHeight(760.f);
	UScrollBox* RowsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("OddsRowsScroll"));
	RowsScroll->AddChild(RowsBox);
	RowsExtent->AddChild(RowsScroll);
	Stack->AddChildToVerticalBox(RowsExtent);
}

void ULaunchLootOddsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BackdropButton) { BackdropButton->OnClicked.AddUniqueDynamic(this, &ULaunchLootOddsWidget::HandleCloseClicked); }
	if (UCloseButtonWidget* WindowClose = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		WindowClose->OnCloseClicked.AddUniqueDynamic(this, &ULaunchLootOddsWidget::HandleCloseClicked);
	}
}

void ULaunchLootOddsWidget::BuildRows(const TArray<FLaunchLootPreviewEntry>& Entries)
{
	if (!RowsBox)
	{
		return;
	}
	RowsBox->ClearChildren();

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return;
	}

	int32 RowIndex = 0;
	for (const FLaunchLootPreviewEntry& Entry : Entries)
	{
		UObject* Icon = nullptr;
		FText Name;
		if (Entry.ResourceType != EResourceType::None)
		{
			bool bFound = false;
			const FResourceInfo Info = TableMgr->GetResourceInfo(Entry.ResourceType, bFound);
			Icon = (bFound && !Info.Icon.IsNull()) ? Info.Icon.LoadSynchronous() : nullptr;
			Name = Info.DisplayName;
		}
		else if (Entry.ItemType != EItemType::None)
		{
			bool bFound = false;
			Icon = TableMgr->GetItemIconResource(Entry.ItemType, bFound);
			FShopItemTable ItemRow;
			if (TableMgr->GetShopItemByItemType(Entry.ItemType, ItemRow)) { Name = ItemRow.DisplayName; }
		}
		else { continue; }

		// 조건·확률 한 줄 — 단일 등급이면 밴드명 반복 없이 "확률 N%"
		int32 BandCount = 0, LastBand = 0;
		for (int32 Band = 0; Band < 3; ++Band)
		{
			if (Entry.ChancePercent[Band] > 0) { ++BandCount; LastBand = Band; }
		}
		FString Cond;
		if (BandCount == 1 && Entry.MinScore > 0)
		{
			Cond = FString::Printf(TEXT("평점 %d점 이상 출시 · 확률 %d%%%s"),
				Entry.MinScore, Entry.ChancePercent[LastBand], *GetBandAmountSuffix(Entry, LastBand));
		}
		else
		{
			Cond = (Entry.MinScore > 0)
				? FString::Printf(TEXT("평점 %d점 이상 출시"), Entry.MinScore)
				: TEXT("모든 출시에서 등장");
			for (int32 Band = 0; Band < 3; ++Band)
			{
				if (Entry.ChancePercent[Band] <= 0) { continue; }
				Cond += FString::Printf(TEXT(" · %s %d%%%s"),
					*ULaunchLootManagerSubsystem::GetScoreBandLabel(Band), Entry.ChancePercent[Band],
					*GetBandAmountSuffix(Entry, Band));
			}
		}
		if (Entry.MaxTier > 0)
		{
			Cond += FString::Printf(TEXT(" · %d단계까지"), Entry.MaxTier);
		}

		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		// 홀수 행만 옅은 줄무늬 — 행 구분 (목업 3.5%)
		Row->SetBrush(MakeRoundedBrush(RowFill, (RowIndex % 2 == 0) ? 0.035f : 0.0f, 12.0f, RowFill, 0.0f, 0.0f));
		Row->SetPadding(FMargin(10.0f, 9.0f, 10.0f, 9.0f));

		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(RowBox);

		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(64.0f);
		IconBox->SetHeightOverride(64.0f);
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		CGIconBrush::ApplyResource(IconImage, Icon);
		IconBox->AddChild(IconImage);
		if (UHorizontalBoxSlot* IconSlot = RowBox->AddChildToHorizontalBox(IconBox))
		{
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* NameBlock = MakeText(WidgetTree, BoldFont, 26, InkColor, Name);
		if (UHorizontalBoxSlot* NameSlot = RowBox->AddChildToHorizontalBox(NameBlock))
		{
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
			NameSlot->SetVerticalAlignment(VAlign_Center);
			NameBlock->SetMinDesiredWidth(200.0f);   // 이름 열 정렬 기준선
		}

		UTextBlock* CondBlock = MakeText(WidgetTree, RegularFont, 24, SubColor, FText::FromString(Cond));
		CondBlock->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* CondSlot = RowBox->AddChildToHorizontalBox(CondBlock))
		{
			FSlateChildSize FillSize;
			FillSize.SizeRule = ESlateSizeRule::Fill;
			CondSlot->SetSize(FillSize);
			CondSlot->SetVerticalAlignment(VAlign_Center);
		}

		RowsBox->AddChildToVerticalBox(Row);
		++RowIndex;
	}
}

void ULaunchLootOddsWidget::Show(const TArray<FLaunchLootPreviewEntry>& Entries, const FButtonStyle& DimButtonStyle, int32 ZOrder)
{
	EnsureTree();
	if (!RowsBox || !BackdropButton) { return; }
	BuildRows(Entries);
	BackdropButton->SetStyle(DimButtonStyle);

	if (!IsInViewport())
	{
		AddToViewport(ZOrder);
	}

	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetPositionInViewport(FVector2D::ZeroVector, false);
	SetDesiredSizeInViewport(FVector2D::ZeroVector);
	// UE5.4의 위치·크기 setter는 앵커를 좌상단으로 바꾸므로 전체 화면 앵커를 마지막에 적용한다.
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void ULaunchLootOddsWidget::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void ULaunchLootOddsWidget::HandleCloseClicked()
{
	Hide();
}

void ULaunchLootOddsWidget::NativeDestruct()
{
	if (BackdropButton) { BackdropButton->OnClicked.RemoveDynamic(this, &ULaunchLootOddsWidget::HandleCloseClicked); }
	if (UCloseButtonWidget* WindowClose = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		WindowClose->OnCloseClicked.RemoveDynamic(this, &ULaunchLootOddsWidget::HandleCloseClicked);
	}
	Super::NativeDestruct();
}
