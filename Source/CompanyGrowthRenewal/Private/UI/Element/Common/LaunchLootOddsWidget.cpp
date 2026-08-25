#include "UI/Element/Common/LaunchLootOddsWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

namespace
{
	// 목업 v2 팔레트 (sRGB -> linear 는 FromSRGBColor 로)
	const FColor PopupFill(0x23, 0x2A, 0x35);
	const FColor PopupLine(0xFF, 0xFF, 0xFF);
	const FColor RowFill(0xFF, 0xFF, 0xFF);
	const FColor InkColor(0xEC, 0xEE, 0xF0);
	const FColor SubColor(0xB9, 0xC2, 0xCF);
	const FColor MutedColor(0x97, 0xA3, 0xB6);
	constexpr float PopupWidth = 760.0f;

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

	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OddsWidthBox"));
	WidthBox->SetWidthOverride(PopupWidth);   // 팝업 폭 규격 — 행 텍스트는 이 안에서 감긴다
	WidgetTree->RootWidget = WidthBox;

	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OddsPlate"));
	Plate->SetBrush(MakeRoundedBrush(PopupFill, 1.0f, 20.0f, PopupLine, 0.13f, 1.5f));
	// UBorder 패딩 이중 함정 — 위젯 Padding 이 슬롯을 되덮으므로 위젯 쪽에 지정
	Plate->SetPadding(FMargin(30.0f, 26.0f, 30.0f, 22.0f));
	WidthBox->AddChild(Plate);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OddsStack"));
	Plate->SetContent(Stack);

	UTextBlock* Title = MakeText(WidgetTree, BoldFont, 28, InkColor, NSLOCTEXT("LaunchLoot", "OddsTitle", "출시 보상"));
	if (UVerticalBoxSlot* TitleSlot = Stack->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	UTextBlock* Caption = MakeText(WidgetTree, RegularFont, 20, MutedColor,
		NSLOCTEXT("LaunchLoot", "OddsCaption", "출시 평점이 높을수록 좋은 보상이 나옵니다. 확률은 보상 1칸 기준입니다."));
	if (UVerticalBoxSlot* CaptionSlot = Stack->AddChildToVerticalBox(Caption))
	{
		CaptionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OddsRows"));
	Stack->AddChildToVerticalBox(RowsBox);
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
		UTexture2D* Icon = nullptr;
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
			Icon = TableMgr->GetItemIcon(Entry.ItemType, bFound);
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
			Cond += FString::Printf(TEXT(" · 티어 %d까지"), Entry.MaxTier);
		}

		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		// 홀수 행만 옅은 줄무늬 — 행 구분 (목업 3.5%)
		Row->SetBrush(MakeRoundedBrush(RowFill, (RowIndex % 2 == 0) ? 0.035f : 0.0f, 12.0f, RowFill, 0.0f, 0.0f));
		Row->SetPadding(FMargin(10.0f, 9.0f, 10.0f, 9.0f));

		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(RowBox);

		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(48.0f);
		IconBox->SetHeightOverride(48.0f);
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Icon) { IconImage->SetBrushFromTexture(Icon); }
		IconBox->AddChild(IconImage);
		if (UHorizontalBoxSlot* IconSlot = RowBox->AddChildToHorizontalBox(IconBox))
		{
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* NameBlock = MakeText(WidgetTree, BoldFont, 23, InkColor, Name);
		if (UHorizontalBoxSlot* NameSlot = RowBox->AddChildToHorizontalBox(NameBlock))
		{
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
			NameSlot->SetVerticalAlignment(VAlign_Center);
			NameBlock->SetMinDesiredWidth(200.0f);   // 이름 열 정렬 기준선
		}

		UTextBlock* CondBlock = MakeText(WidgetTree, RegularFont, 20, SubColor, FText::FromString(Cond));
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

void ULaunchLootOddsWidget::ShowAt(FVector2D AbsoluteScreenPos, const TArray<FLaunchLootPreviewEntry>& Entries, float DurationSec, int32 ZOrder)
{
	EnsureTree();
	BuildRows(Entries);

	if (!IsInViewport())
	{
		AddToViewport(ZOrder);
	}

	// ItemTooltipWidget::ShowAt 의 BelowAnchor 배치 복제 — AbsoluteToLocal 로 SafeZone/DPI 자동 보정
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D Anchor = ViewportGeo.AbsoluteToLocal(AbsoluteScreenPos);

	ForceLayoutPrepass();
	const FVector2D PopupSize = GetDesiredSize();
	const FVector2D ViewportSize = ViewportGeo.GetLocalSize();

	const float EdgeMargin = 12.0f;
	const float Gap = 14.0f;
	FVector2D Pos;
	Pos.X = Anchor.X;
	const float SpaceBelow = (ViewportSize.Y - EdgeMargin) - (Anchor.Y + Gap);
	const float SpaceAbove = (Anchor.Y - Gap) - EdgeMargin;
	const bool bFlipUp = (PopupSize.Y > SpaceBelow) && (SpaceAbove > SpaceBelow);
	Pos.Y = bFlipUp ? (Anchor.Y - Gap - PopupSize.Y) : (Anchor.Y + Gap);
	Pos.X = FMath::Clamp(Pos.X, EdgeMargin, FMath::Max(EdgeMargin, ViewportSize.X - PopupSize.X - EdgeMargin));
	Pos.Y = FMath::Clamp(Pos.Y, EdgeMargin, FMath::Max(EdgeMargin, ViewportSize.Y - PopupSize.Y - EdgeMargin));

	SetPositionInViewport(Pos, /*bRemoveDPIScale=*/false);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
		if (DurationSec > 0.0f)
		{
			TWeakObjectPtr<ULaunchLootOddsWidget> WeakThis(this);
			World->GetTimerManager().SetTimer(DismissTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (ULaunchLootOddsWidget* Strong = WeakThis.Get()) { Strong->Hide(); }
			}), DurationSec, false);
		}
	}
}

void ULaunchLootOddsWidget::Hide()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void ULaunchLootOddsWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
	}
	Super::NativeDestruct();
}
