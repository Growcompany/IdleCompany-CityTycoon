#if WITH_EDITOR && !UE_BUILD_SHIPPING

#include "UI/Panel/PitchBoardWidget.h"
#include "UI/Element/Office/OfficeRequestCardWidget.h"
#include "UI/Element/Office/PitchTierTabWidget.h"
#include "UI/Element/Office/PitchTileWidget.h"
#include "UI/Element/Cards/ItemCardWidget.h"
#include "UI/Element/Common/LaunchLootOddsWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/ProgressBar.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace PitchBoardPolishValidation
{
constexpr int32 ProjectColumns = 1;

struct FBounds
{
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	bool bFound = false;

	double Right() const { return Position.X + Size.X; }
	double Bottom() const { return Position.Y + Size.Y; }
};

static FBounds Bounds(const UWidget* Item, const UWidget* Reference)
{
	FBounds Result;
	if (!Item || !Reference) return Result;
	const FGeometry& Geometry = Item->GetCachedGeometry();
	const FGeometry& RootGeometry = Reference->GetCachedGeometry();
	Result.Position = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
	Result.Size = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize())) - Result.Position;
	Result.bFound = Result.Size.X > 0 && Result.Size.Y > 0;
	return Result;
}

static TSharedPtr<FJsonObject> JsonBounds(const FBounds& Value)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("found"), Value.bFound);
	Result->SetNumberField(TEXT("left"), Value.Position.X);
	Result->SetNumberField(TEXT("top"), Value.Position.Y);
	Result->SetNumberField(TEXT("width"), Value.Size.X);
	Result->SetNumberField(TEXT("height"), Value.Size.Y);
	return Result;
}

static bool IsVisibleThroughParents(const UWidget* Item)
{
	if (!Item) return false;
	for (const UWidget* Ancestor = Item; Ancestor; Ancestor = Ancestor->GetParent())
	{
		if (!Ancestor->IsVisible()) return false;
	}
	return true;
}

static bool Draw(UUserWidget& Panel, int32 Width, const FString& Filename)
{
	FWidgetRenderer Renderer(true);
	const FVector2D DrawSize(Width, 1440);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
	if (!Target.IsValid()) return false;
	const TSharedRef<SWidget> SlateWidget = Panel.TakeWidget();
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		Renderer.DrawWidget(Target.Get(), SlateWidget, 1.f, DrawSize, 0.f);
		FlushRenderingCommands();
		// Scroll-into-view requests are consumed by SScrollBox::Tick, not Paint.
		TArray<UWidget*> ScrollWidgets = { Panel.GetWidgetFromName(TEXT("TierRailScroll")), Panel.GetWidgetFromName(TEXT("GridScroll")), Panel.GetWidgetFromName(TEXT("RewardPreviewScroll")) };
		if (UUserWidget* Detail = Cast<UUserWidget>(Panel.GetWidgetFromName(TEXT("Card"))))
		{
			ScrollWidgets.Add(Detail->GetWidgetFromName(TEXT("CardScroll")));
		}
		for (UWidget* Item : ScrollWidgets)
		{
			if (Item)
			{
				if (const TSharedPtr<SWidget> ScrollSlate = Item->GetCachedWidget())
				{
					ScrollSlate->Tick(ScrollSlate->GetCachedGeometry(), FSlateApplication::Get().GetCurrentTime(), .25f);
				}
			}
		}
	}
	FImage Pixels;
	if (!FImageUtils::GetRenderTargetImage(Target.Get(), Pixels)) return false;
	Pixels.GammaSpace = EGammaSpace::sRGB;
	TArray64<uint8> Bytes;
	return FImageUtils::CompressImage(Bytes, TEXT("png"), Pixels) && FFileHelper::SaveArrayToFile(Bytes, *Filename);
}

static TSharedPtr<FJsonObject> CaptureCase(UPitchBoardWidget& Panel, int32 Width, const FString& Case,
	const FString& Directory, bool bExpectTwentyTiers, const TArray<FLaunchLootPreviewEntry>& ExpectedRewards)
{
	const FString Filename = FString::Printf(TEXT("pitch_%d_%s.png"), Width, *Case);
	const bool bSaved = Draw(Panel, Width, Directory / Filename);
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("image"), Filename);
	Result->SetBoolField(TEXT("saved"), bSaved);
	Result->SetStringField(TEXT("case"), Case);
	Result->SetNumberField(TEXT("width"), Width);
	const auto FindBounds = [&Panel](const TCHAR* Name) { return Bounds(Panel.GetWidgetFromName(Name), &Panel); };
	const FBounds Rail = FindBounds(TEXT("RailSizer"));
	const FBounds Grid = FindBounds(TEXT("GridColumn"));
	const FBounds Detail = FindBounds(TEXT("DetailColumnWidth"));
	const FBounds Shell = FindBounds(TEXT("RootSizeBox"));
	Result->SetObjectField(TEXT("rail"), JsonBounds(Rail));
	Result->SetObjectField(TEXT("grid"), JsonBounds(Grid));
	Result->SetObjectField(TEXT("detail"), JsonBounds(Detail));
	Result->SetObjectField(TEXT("shell"), JsonBounds(Shell));
	const bool bColumns = Rail.bFound && Grid.bFound && Detail.bFound
		&& Rail.Right() <= Grid.Position.X + 1 && Grid.Right() <= Detail.Position.X + 1;
	const bool bInside = Shell.bFound && Shell.Position.X >= 71 && Shell.Right() <= Width - 71
		&& Shell.Position.Y >= 71 && Shell.Bottom() <= 1369;
	UUniformGridPanel* TileGrid = Cast<UUniformGridPanel>(Panel.GetWidgetFromName(TEXT("TileGrid")));
	UScrollBox* GridScroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("GridScroll")));
	bool bSingleColumn = TileGrid && TileGrid->GetChildrenCount() == 10 && GridScroll;
	bool bOriginalTileParts = TileGrid && TileGrid->GetChildrenCount() > 0;
	bool bTextFits = true;
	TArray<TSharedPtr<FJsonValue>> TileRecords;
	if (TileGrid)
	{
		for (int32 Index = 0; Index < TileGrid->GetChildrenCount(); ++Index)
		{
			UWidget* Tile = TileGrid->GetChildAt(Index);
			const FBounds TileBox = Bounds(Tile, &Panel);
			TSharedPtr<FJsonObject> Record = JsonBounds(TileBox);
			Record->SetNumberField(TEXT("index"), Index);
			const bool bTouch = TileBox.bFound && TileBox.Size.X >= 141 && TileBox.Size.Y >= 141;
			Record->SetBoolField(TEXT("touch_minimum"), bTouch);
			if (UUserWidget* TileWidget = Cast<UUserWidget>(Tile))
			{
				for (const TCHAR* Name : {TEXT("GroundShadow"), TEXT("Plate"), TEXT("WellBG"), TEXT("SelectedGlow")})
				{
					bOriginalTileParts &= TileWidget->GetWidgetFromName(Name) != nullptr;
				}
				for (const TCHAR* Name : {TEXT("Text_Name"), TEXT("Text_ReleaseState"), TEXT("Text_Badge")})
				{
					const UWidget* Label = TileWidget->GetWidgetFromName(Name);
					if (!Label || !IsVisibleThroughParents(Label)) continue;
					const FBounds TextBox = Bounds(Label, &Panel);
					const bool bFit = TextBox.bFound && TextBox.Position.X >= TileBox.Position.X - 1
						&& TextBox.Right() <= TileBox.Right() + 1 && TextBox.Position.Y >= TileBox.Position.Y - 1
						&& TextBox.Bottom() <= TileBox.Bottom() + 1;
					Record->SetBoolField(FString(Name) + TEXT("_fits"), bFit);
					bTextFits &= bFit;
				}
			}
			const UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Tile->Slot);
			bSingleColumn &= GridSlot && GridSlot->GetColumn() == 0 && GridSlot->GetRow() == Index
				&& GridSlot->GetHorizontalAlignment() == HAlign_Fill;
			bSingleColumn &= bTouch && TileBox.Position.X >= Grid.Position.X - 1 && TileBox.Right() <= Grid.Right() + 1;
			if (Index > 0)
			{
				const FBounds Previous = Bounds(TileGrid->GetChildAt(Index - 1), &Panel);
				bSingleColumn &= Previous.Bottom() <= TileBox.Position.Y + 1
					&& FMath::Abs(Previous.Position.X - TileBox.Position.X) < 2
					&& FMath::Abs(Previous.Size.X - TileBox.Size.X) < 2;
			}
			TileRecords.Add(MakeShared<FJsonValueObject>(Record));
		}
	}
	Result->SetArrayField(TEXT("tiles"), TileRecords);
	UOfficeRequestCardWidget* Card = Cast<UOfficeRequestCardWidget>(Panel.GetWidgetFromName(TEXT("Card")));
	bool bOriginalCardParts = Card != nullptr;
	for (const TCHAR* Name : {TEXT("StatBorder"), TEXT("FrameInner"), TEXT("DevPlate"), TEXT("RowsPanel"), TEXT("WellInner")})
	{
		bOriginalCardParts &= Card && Card->GetWidgetFromName(Name);
	}
	const FBounds Cta = Card ? Bounds(Card->GetWidgetFromName(TEXT("AcceptButton")), &Panel) : FBounds();
	Result->SetObjectField(TEXT("cta"), JsonBounds(Cta));
	// A roster-less board intentionally collapses the CTA; do not fabricate affordability.
	const bool bCtaVisible = Card && IsVisibleThroughParents(Card->GetWidgetFromName(TEXT("AcceptButton")))
		&& IsVisibleThroughParents(Card) && IsVisibleThroughParents(&Panel);
	const bool bCtaFits = !bCtaVisible || (Cta.bFound && Cta.Position.X >= Detail.Position.X - 1
		&& Cta.Right() <= Detail.Right() + 1 && Cta.Position.Y >= Detail.Position.Y - 1
		&& Cta.Bottom() <= Detail.Bottom() + 1 && Cta.Bottom() <= Shell.Bottom() - 16);
	const UWidget* Title = Card ? Card->GetWidgetFromName(TEXT("Text_ProjectName")) : nullptr;
	const FBounds TitleBox = Bounds(Title, &Panel);
	const FBounds CardBox = Bounds(Card, &Panel);
	const bool bTitleFits = !bExpectTwentyTiers || (TitleBox.bFound && CardBox.bFound && Detail.bFound
		&& IsVisibleThroughParents(Title) && IsVisibleThroughParents(Card) && IsVisibleThroughParents(&Panel)
		&& TitleBox.Position.X >= CardBox.Position.X - 1 && TitleBox.Right() <= CardBox.Right() + 1
		&& TitleBox.Position.Y >= CardBox.Position.Y - 1 && TitleBox.Bottom() <= CardBox.Bottom() + 1
		&& TitleBox.Position.X >= Detail.Position.X - 1 && TitleBox.Right() <= Detail.Right() + 1
		&& TitleBox.Position.Y >= Detail.Position.Y - 1 && TitleBox.Bottom() <= Detail.Bottom() + 1);
	Result->SetObjectField(TEXT("detail_title"), JsonBounds(TitleBox));
	Result->SetBoolField(TEXT("detail_title_fits"), bTitleFits);
	if (Title)
	{
		const FVector2D Desired = Title->GetDesiredSize();
		const FVector2D Allocated = Title->GetCachedGeometry().GetLocalSize();
		Result->SetNumberField(TEXT("detail_title_desired_width"), Desired.X);
		Result->SetNumberField(TEXT("detail_title_desired_height"), Desired.Y);
		Result->SetNumberField(TEXT("detail_title_allocated_width"), Allocated.X);
		Result->SetNumberField(TEXT("detail_title_allocated_height"), Allocated.Y);
	}
	const FBounds GridView = Bounds(GridScroll, &Panel);
	bool bLastRowVisible = true;
	if (Case == TEXT("grid_end"))
	{
		const FBounds LastTile = TileGrid && TileGrid->GetChildrenCount() > 0
			? Bounds(TileGrid->GetChildAt(TileGrid->GetChildrenCount() - 1), &Panel) : FBounds();
		bLastRowVisible = GridView.bFound && LastTile.bFound && LastTile.Position.Y >= GridView.Position.Y - 1
			&& LastTile.Bottom() <= GridView.Bottom() + 1;
	}
	UScrollBox* RailScroll = Cast<UScrollBox>(Panel.GetWidgetFromName(TEXT("TierRailScroll")));
	UPanelWidget* RailBox = Cast<UPanelWidget>(Panel.GetWidgetFromName(TEXT("TierRailBox")));
	const bool bTwenty = !bExpectTwentyTiers || (RailScroll && RailBox && RailBox->GetChildrenCount() == 20
		&& RailScroll->GetScrollOffsetOfEnd() > 0);
	bool bCurrentVisible = true;
	if (bExpectTwentyTiers && RailBox && RailBox->GetChildrenCount() > 11)
	{
		const FBounds CurrentBox = Bounds(RailBox->GetChildAt(11), &Panel);
		const FBounds View = Bounds(RailScroll, &Panel);
		bCurrentVisible = CurrentBox.bFound && CurrentBox.Position.Y >= View.Position.Y - 1 && CurrentBox.Bottom() <= View.Bottom() + 1;
	}
	Result->SetBoolField(TEXT("columns_do_not_overlap"), bColumns);
	Result->SetBoolField(TEXT("safe_inset"), bInside);
	Result->SetBoolField(TEXT("single_column_equal_width_rows"), bSingleColumn);
	Result->SetBoolField(TEXT("original_tile_parts"), bOriginalTileParts);
	Result->SetBoolField(TEXT("original_card_parts"), bOriginalCardParts);
	Result->SetObjectField(TEXT("grid_view"), JsonBounds(GridView));
	Result->SetNumberField(TEXT("grid_scroll_end"), GridScroll ? GridScroll->GetScrollOffsetOfEnd() : 0);
	Result->SetBoolField(TEXT("last_row_visible"), bLastRowVisible);
	Result->SetBoolField(TEXT("cta_visible"), bCtaVisible);
	Result->SetStringField(TEXT("cta_validation"), bCtaVisible ? TEXT("exercised") : TEXT("not_exercised_roster_required"));
	Result->SetBoolField(TEXT("cta_fits"), bCtaFits);
	Result->SetBoolField(TEXT("twenty_tier_scroll"), bTwenty);
	Result->SetBoolField(TEXT("current_tier_visible"), bCurrentVisible);
	Result->SetBoolField(TEXT("tile_text_fits"), bTextFits);
	UPanelWidget* RewardBox = Cast<UPanelWidget>(Panel.GetWidgetFromName(TEXT("RewardPreviewBox")));
	const FBounds RewardPlate = FindBounds(TEXT("RewardPreviewPanel"));
	const FBounds RewardViewport = FindBounds(TEXT("RewardPreviewScroll"));
	bool bRewards = RewardBox && RewardBox->GetChildrenCount() == ExpectedRewards.Num()
		&& RewardPlate.bFound && RewardPlate.Position.Y >= GridView.Bottom() - 1
		&& RewardPlate.Bottom() <= Shell.Bottom() - 16;
	UTableManagerSubsystem* TableMgr = Panel.GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	TArray<TSharedPtr<FJsonValue>> RewardRecords;
	for (int32 Index = 0; RewardBox && Index < RewardBox->GetChildrenCount(); ++Index)
	{
		UItemCardWidget* Item = Cast<UItemCardWidget>(RewardBox->GetChildAt(Index));
		const UImage* Icon = Item ? Cast<UImage>(Item->GetWidgetFromName(TEXT("EntityImage"))) : nullptr;
		FText ExpectedName;
		UObject* ExpectedIcon = nullptr;
		if (TableMgr && ExpectedRewards.IsValidIndex(Index))
		{
			const FLaunchLootPreviewEntry& Entry = ExpectedRewards[Index];
			if (Entry.ItemType != EItemType::None)
			{
				FShopItemTable ItemRow;
				if (TableMgr->GetShopItemByItemType(Entry.ItemType, ItemRow)) ExpectedName = ItemRow.DisplayName;
				bool bFoundIcon = false;
				ExpectedIcon = TableMgr->GetItemIconResource(Entry.ItemType, bFoundIcon);
			}
			else
			{
				bool bFound = false;
				const FResourceInfo Info = TableMgr->GetResourceInfo(Entry.ResourceType, bFound);
				ExpectedName = Info.DisplayName;
				if (bFound && !Info.Icon.IsNull()) ExpectedIcon = Info.Icon.LoadSynchronous();
			}
		}
		const FBounds ItemBounds = Bounds(Item, &Panel);
		const bool bName = Item && !ExpectedName.IsEmpty() && Item->GetToolTipText().EqualTo(ExpectedName);
		const bool bIcon = Icon && ExpectedIcon && Icon->GetBrush().GetResourceObject() == ExpectedIcon;
		const bool bItemFits = ItemBounds.bFound && RewardViewport.bFound
			&& ItemBounds.Position.Y >= RewardViewport.Position.Y - 1
			&& ItemBounds.Bottom() <= RewardViewport.Bottom() + 1;
		const bool bIconOnly = Item && Item->GetWidgetFromName(TEXT("QuantityText")) == nullptr;
		bRewards &= bName && bIcon && bItemFits && bIconOnly;
		TSharedPtr<FJsonObject> Record = JsonBounds(ItemBounds);
		Record->SetStringField(TEXT("name"), Item ? Item->GetToolTipText().ToString() : FString());
		Record->SetBoolField(TEXT("matches_data_table"), bName && bIcon);
		Record->SetBoolField(TEXT("card_fits"), bItemFits);
		Record->SetBoolField(TEXT("icon_only"), bIconOnly);
		RewardRecords.Add(MakeShared<FJsonValueObject>(Record));
	}
	const UCommonTextBlock* RewardTitle = Cast<UCommonTextBlock>(Panel.GetWidgetFromName(TEXT("Text_RewardTitle")));
	bRewards &= RewardTitle && RewardTitle->GetText().ToString() == TEXT("출시 보상");
	bool bTierBadges = RailBox && RailBox->GetChildrenCount() > 0;
	for (int32 Index = 0; RailBox && Index < RailBox->GetChildrenCount(); ++Index)
	{
		UPitchTierTabWidget* Tab = Cast<UPitchTierTabWidget>(RailBox->GetChildAt(Index));
		const UCommonTextBlock* TierName = Tab ? Cast<UCommonTextBlock>(Tab->GetWidgetFromName(TEXT("Text_Tier"))) : nullptr;
		const UImage* TierCover = Tab ? Cast<UImage>(Tab->GetWidgetFromName(TEXT("TierCoverImage"))) : nullptr;
		bTierBadges &= TierName && TierName->GetText().EqualTo(
			FText::Format(NSLOCTEXT("PitchBoard", "TierTabName", "{0}단계"), Index + 1)) && TierCover;
		if (bExpectTwentyTiers && Index == 11)
		{
			UProgressBar* ProgressBar = Tab ? Cast<UProgressBar>(Tab->GetWidgetFromName(TEXT("TierProgressBar"))) : nullptr;
			bTierBadges &= ProgressBar && IsVisibleThroughParents(ProgressBar) && FMath::IsNearlyEqual(ProgressBar->GetPercent(), .4f);
		}
	}
	Result->SetArrayField(TEXT("reward_items"), RewardRecords);
	Result->SetObjectField(TEXT("reward_plate"), JsonBounds(RewardPlate));
	Result->SetObjectField(TEXT("reward_scroll_view"), JsonBounds(RewardViewport));
	Result->SetBoolField(TEXT("reward_data_and_bounds"), bRewards);
	Result->SetBoolField(TEXT("tier_names_and_progress"), bTierBadges);
	Result->SetBoolField(TEXT("passed"), bSaved && bColumns && bInside && bSingleColumn && bOriginalTileParts
		&& bOriginalCardParts && bLastRowVisible && bCtaFits && bTwenty && bCurrentVisible && bTextFits && bTitleFits && bRewards && bTierBadges);
	return Result;
}

static void Capture(UWorld*)
{
	UPitchBoardWidget* Panel = nullptr;
	for (TObjectIterator<UPitchBoardWidget> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() && It->GetWorld()->WorldType == EWorldType::PIE && It->IsActivated())
		{
			Panel = *It;
			break;
		}
	}
	if (!Panel)
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.Capture requires the actual pitch board already open in PIE."));
		return;
	}
	UOfficeRequestCardWidget* Card = Cast<UOfficeRequestCardWidget>(Panel->GetWidgetFromName(TEXT("Card")));
	UPanelWidget* RailBox = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("TierRailBox")));
	UScrollBox* RailScroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("TierRailScroll")));
	UScrollBox* GridScroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("GridScroll")));
	UScrollBox* DetailScroll = Card ? Cast<UScrollBox>(Card->GetWidgetFromName(TEXT("CardScroll"))) : nullptr;
	if (!Card || !RailBox || !RailScroll || !GridScroll || RailBox->GetChildrenCount() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.Capture missing original-card layout/runtime contract."));
		return;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PitchBoardPolish/Captures"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const float PreviousScroll = RailScroll->GetScrollOffset();
	const float PreviousGridScroll = GridScroll->GetScrollOffset();
	const float PreviousDetailScroll = DetailScroll ? DetailScroll->GetScrollOffset() : 0.f;
	TArray<TSharedPtr<FJsonValue>> Captures;
	int32 PreviewTier = 1;
	for (UWidget* Row : RailBox->GetAllChildren())
	{
		if (const UPitchTierTabWidget* Tab = Cast<UPitchTierTabWidget>(Row))
		{
			if (Tab->GetSelected()) PreviewTier = Tab->GetTier();
		}
	}
	const TArray<FLaunchLootPreviewEntry> ExpectedRewards = Panel->GetGameInstance()->GetSubsystem<ULaunchLootManagerSubsystem>()->GetLootPreview(PreviewTier);
	for (const int32 Width : {2304, 3120})
	{
		GridScroll->SetScrollOffset(0.f);
		Captures.Add(MakeShared<FJsonValueObject>(CaptureCase(*Panel, Width, TEXT("original_cards"), Directory, false, ExpectedRewards)));
		GridScroll->ScrollToEnd();
		Captures.Add(MakeShared<FJsonValueObject>(CaptureCase(*Panel, Width, TEXT("grid_end"), Directory, false, ExpectedRewards)));
	}
	GridScroll->SetScrollOffset(0.f);

	// UI-only stress rows never modify FTierLayout, player progress, project data, or saves.
	TArray<UWidget*> OriginalRows = RailBox->GetAllChildren();
	TArray<FMargin> OriginalPadding;
	for (UWidget* Row : OriginalRows)
	{
		const UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Row->Slot);
		OriginalPadding.Add(RowSlot ? RowSlot->GetPadding() : FMargin());
	}
	UClass* TabClass = OriginalRows[0]->GetClass();
	RailBox->ClearChildren();
	TStrongObjectPtr<UCommonButtonGroupBase> Group(NewObject<UCommonButtonGroupBase>());
	Group->SetSelectionRequired(true);
	UPitchTierTabWidget* CurrentFixture = nullptr;
	for (int32 Tier = 1; Tier <= 20; ++Tier)
	{
		UPitchTierTabWidget* Tab = CreateWidget<UPitchTierTabWidget>(Panel, TabClass);
		if (!Tab) continue;
		FPitchTierTabData Data;
		Data.Tier = Tier; Data.bCurrent = Tier == 12; Data.bDone = Tier < 12; Data.bLocked = Tier > 12;
		Data.ProgressText = FText::FromString(TEXT("4/10"));
		Data.ProgressFraction = .4f;
		Tab->Configure(Data);
		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(RailBox->AddChild(Tab)))
		{
			RowSlot->SetPadding(OriginalPadding[0]);
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Group->AddWidget(Tab);
		if (Tier == 12) CurrentFixture = Tab;
	}
	Group->SelectButtonAtIndex(11);
	RailScroll->ScrollWidgetIntoView(CurrentFixture, false, EDescendantScrollDestination::Center);
	UCommonTextBlock* Title = Cast<UCommonTextBlock>(Card->GetWidgetFromName(TEXT("Text_ProjectName")));
	const FText OriginalTitle = Title ? Title->GetText() : FText::GetEmpty();
	if (Title) Title->SetText(FText::FromString(TEXT("아주긴프로젝트이름가독성검증")));
	for (const int32 Width : {2304, 3120})
	{
		Captures.Add(MakeShared<FJsonValueObject>(CaptureCase(*Panel, Width, TEXT("twenty_tiers_long_title"), Directory, true, ExpectedRewards)));
	}
	if (Title) Title->SetText(OriginalTitle);
	RailBox->ClearChildren();
	for (int32 Index = 0; Index < OriginalRows.Num(); ++Index)
	{
		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(RailBox->AddChild(OriginalRows[Index])))
		{
			RowSlot->SetPadding(OriginalPadding[Index]);
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
	RailScroll->ScrollWidgetIntoView(nullptr, false);
	RailScroll->EndInertialScrolling();
	RailScroll->SetScrollOffset(PreviousScroll);
	GridScroll->ScrollWidgetIntoView(nullptr, false);
	GridScroll->EndInertialScrolling();
	GridScroll->SetScrollOffset(PreviousGridScroll);
	if (DetailScroll)
	{
		DetailScroll->ScrollWidgetIntoView(nullptr, false);
		DetailScroll->EndInertialScrolling();
		DetailScroll->SetScrollOffset(PreviousDetailScroll);
	}
	bool bLayoutPassed = Captures.Num() == 6;
	bool bCtaCovered = Captures.Num() == 6;
	for (const TSharedPtr<FJsonValue>& Entry : Captures)
	{
		bLayoutPassed &= Entry->AsObject()->GetBoolField(TEXT("passed"));
		bCtaCovered &= Entry->AsObject()->GetBoolField(TEXT("cta_visible"));
	}
	const bool bPassed = bLayoutPassed && bCtaCovered;
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetBoolField(TEXT("layout_passed"), bLayoutPassed);
	Report->SetBoolField(TEXT("cta_coverage_complete"), bCtaCovered);
	Report->SetStringField(TEXT("scope"), TEXT("Existing PIE board rendered at virtual desktop sizes. UI-only 20-tier fixture; no data/save writes. Physical-device input is not tested."));
	Report->SetArrayField(TEXT("captures"), Captures);
	FString Json;
	FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	if (!FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json"))))
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.Capture passed=0: manifest save failed at %s"), *Directory);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.Capture passed=%d directory=%s"), bPassed, *Directory);
}

static FAutoConsoleCommandWithWorld CaptureCommand(TEXT("PitchBoardUI.Capture"),
	TEXT("Capture the already-open PIE board at 2304/3120 with original cards, grid end and UI-only 20-tier stress states."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&Capture));

static void CaptureFrame(const TArray<FString>& Args, UWorld* CommandWorld)
{
	bool bValid = Args.Num() == 2 && !Args[0].IsEmpty() && Args[0].Len() <= 64
		&& (Args[1] == TEXT("2304") || Args[1] == TEXT("3120"));
	if (bValid)
	{
		for (const TCHAR Character : Args[0])
		{
			bValid &= (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z')
				|| (Character >= '0' && Character <= '9') || Character == '_';
		}
	}
	if (!bValid || !CommandWorld || CommandWorld->WorldType != EWorldType::PIE)
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.CaptureFrame requires PIE and arguments: [ASCII alnum_ label] [2304|3120]."));
		return;
	}
	for (TObjectIterator<UPitchBoardWidget> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == CommandWorld && It->IsActivated())
		{
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("PitchBoardPolish/RecommendFX");
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString Filename = Directory / (Args[0] + TEXT("_") + Args[1] + TEXT(".png"));
			const bool bSaved = Draw(**It, FCString::Atoi(*Args[1]), Filename);
			UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.CaptureFrame saved=%d file=%s"), bSaved, *Filename);
			return;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.CaptureFrame requires an active pitch board in this PIE world."));
}

static FAutoConsoleCommandWithWorldAndArgs CaptureFrameCommand(TEXT("PitchBoardUI.CaptureFrame"),
	TEXT("Capture current PIE pitch board state without fixtures: [label] [2304|3120]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CaptureFrame));

struct FTileFixture
{
	const TCHAR* Name;
	PitchBoardText::ETileVerdict Verdict;
	EQualityGrade Grade;
	bool bShipped;
	bool bRecommended;
};

static const FTileFixture TileFixtures[] = {
	{TEXT("released_s"), PitchBoardText::ETileVerdict::Redevelop, EQualityGrade::S, true, false},
	{TEXT("released_a"), PitchBoardText::ETileVerdict::Pass, EQualityGrade::A, true, false},
	{TEXT("released_b"), PitchBoardText::ETileVerdict::Short, EQualityGrade::B, true, false},
	{TEXT("released_c"), PitchBoardText::ETileVerdict::Wait, EQualityGrade::C, true, false},
	{TEXT("released_no_grade"), PitchBoardText::ETileVerdict::Pass, EQualityGrade::Max, true, false},
	{TEXT("unreleased_recommended"), PitchBoardText::ETileVerdict::Pass, EQualityGrade::Max, false, true},
	{TEXT("unreleased_short"), PitchBoardText::ETileVerdict::Short, EQualityGrade::Max, false, false},
	{TEXT("unreleased_wait"), PitchBoardText::ETileVerdict::Wait, EQualityGrade::Max, false, false},
	{TEXT("locked"), PitchBoardText::ETileVerdict::Locked, EQualityGrade::Max, false, false},
	{TEXT("released_recommended"), PitchBoardText::ETileVerdict::Pass, EQualityGrade::A, true, true},
	{TEXT("locked_recommend_input"), PitchBoardText::ETileVerdict::Locked, EQualityGrade::Max, false, true}
};

static FPitchTileData MakeTileFixture(const FTileFixture& Fixture, int32 Index, UTexture2D* CoverTexture)
{
	FPitchTileData Data;
	Data.ProjectIndex = Index + 1;
	Data.ProjectName = FText::FromString(FString::Printf(TEXT("프로젝트 %d"), Index + 1));
	const bool bHasMetadata = Fixture.Verdict != PitchBoardText::ETileVerdict::Wait
		&& Fixture.Verdict != PitchBoardText::ETileVerdict::Locked;
	Data.Combo = bHasMetadata ? INVTEXT("퍼즐 · 토이") : FText::GetEmpty();
	Data.Cover = CoverTexture;
	Data.Verdict = Fixture.Verdict;
	Data.CompanyType = ECompanyType::Game;
	Data.ReleaseGrade = Fixture.Grade;
	Data.bHasShipped = Fixture.bShipped;
	Data.bRecommended = Fixture.bRecommended;
	return Data;
}

static bool DoBoundsOverlap(const FBounds& A, const FBounds& B)
{
	return A.bFound && B.bFound && A.Position.X < B.Right() - 1 && B.Position.X < A.Right() - 1
		&& A.Position.Y < B.Bottom() - 1 && B.Position.Y < A.Bottom() - 1;
}

static FBounds VisualBounds(const UWidget* Item, const UWidget* Reference)
{
	FBounds Result;
	if (!Item || !Reference) return Result;
	const FGeometry& Geometry = Item->GetCachedGeometry();
	const FGeometry& Root = Reference->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Corners[] = {{0, 0}, {Size.X, 0}, {0, Size.Y}, Size};
	FVector2D Minimum(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D Maximum(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
	for (const FVector2D& Corner : Corners)
	{
		const FVector2D Point = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Corner));
		Minimum.X = FMath::Min(Minimum.X, Point.X);
		Minimum.Y = FMath::Min(Minimum.Y, Point.Y);
		Maximum.X = FMath::Max(Maximum.X, Point.X);
		Maximum.Y = FMath::Max(Maximum.Y, Point.Y);
	}
	Result.Position = Minimum;
	Result.Size = Maximum - Minimum;
	Result.bFound = Result.Size.X > 0 && Result.Size.Y > 0;
	return Result;
}

static bool ReadTileDefaultColor(const UPitchTileWidget& Tile, const TCHAR* PropertyName, FLinearColor& OutColor)
{
	const FStructProperty* Property = FindFProperty<FStructProperty>(Tile.GetClass(), PropertyName);
	if (!Property || Property->Struct != TBaseStructure<FLinearColor>::Get()) return false;
	const UPitchTileWidget* Defaults = Tile.GetClass()->GetDefaultObject<UPitchTileWidget>();
	if (!Defaults) return false;
	OutColor = *Property->ContainerPtrToValuePtr<FLinearColor>(Defaults);
	return true;
}

static bool HasExpectedPlateColor(const UPitchTileWidget& Tile, const FTileFixture& Fixture)
{
	const UBorder* PlateBorder = Cast<UBorder>(Tile.GetWidgetFromName(TEXT("Plate")));
	if (!PlateBorder) return false;
	FLinearColor Base;
	if (!ReadTileDefaultColor(Tile, Fixture.Verdict == PitchBoardText::ETileVerdict::Locked
		? TEXT("PlateMutedColor") : Fixture.bShipped ? TEXT("PlateBaseColor") : TEXT("PlateUnreleasedColor"), Base)) return false;
	if (Fixture.Verdict == PitchBoardText::ETileVerdict::Locked)
	{
		return PlateBorder->GetBrushColor().Equals(Base, 0.001f);
	}
	FLinearColor Wash;
	const TCHAR* WashProperty = Fixture.Verdict == PitchBoardText::ETileVerdict::Pass ? TEXT("TileTintPass")
		: Fixture.Verdict == PitchBoardText::ETileVerdict::Short ? TEXT("TileTintShort") : TEXT("TileTintNeutral");
	if (!ReadTileDefaultColor(Tile, WashProperty, Wash)) return false;
	const FLinearColor Expected = FMath::Lerp(Base, FLinearColor(Wash.R, Wash.G, Wash.B, 1.f), Wash.A);
	return PlateBorder->GetBrushColor().Equals(Expected, 0.001f);
}

static TSharedPtr<FJsonObject> InspectTileFixture(UPitchTileWidget& Tile, UPitchBoardWidget& Panel, UUniformGridPanel& Grid,
	const FTileFixture& Fixture, const TArray<UPitchTileWidget*>& Neighbors)
{
	TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
	Record->SetStringField(TEXT("case"), Fixture.Name);
	const UCommonTextBlock* Release = Cast<UCommonTextBlock>(Tile.GetWidgetFromName(TEXT("Text_ReleaseState")));
	const UBorder* StampFrame = Cast<UBorder>(Tile.GetWidgetFromName(TEXT("ReleaseStampFrame")));
	const UCommonTextBlock* Grade = Cast<UCommonTextBlock>(Tile.GetWidgetFromName(TEXT("Text_Badge")));
	const UCommonTextBlock* Name = Cast<UCommonTextBlock>(Tile.GetWidgetFromName(TEXT("Text_Name")));
	const UCommonTextBlock* Combo = Cast<UCommonTextBlock>(Tile.GetWidgetFromName(TEXT("Text_Combo")));
	const UBorder* GradeBadge = Cast<UBorder>(Tile.GetWidgetFromName(TEXT("Badge")));
	const UWidget* GradeSize = Tile.GetWidgetFromName(TEXT("TileBadgeSize"));
	const UWidget* Sticker = Tile.GetWidgetFromName(TEXT("RecommendSize"));
	const UWidget* StickerRoot = Tile.GetWidgetFromName(TEXT("RecommendationRoot"));
	const UWidget* ReleaseGroup = Tile.GetWidgetFromName(TEXT("ReleaseGroup"));
	const UWidget* VerdictRow = Tile.GetWidgetFromName(TEXT("VerdictRow"));
	const UWidget* Plate = Tile.GetWidgetFromName(TEXT("Plate"));
	const UImage* Cover = Cast<UImage>(Tile.GetWidgetFromName(TEXT("CoverImage")));
	const UWidget* OldDot = Tile.GetWidgetFromName(TEXT("Dot"));
	const UWidget* OldDotBox = Tile.GetWidgetFromName(TEXT("DotBox"));
	const bool bLocked = Fixture.Verdict == PitchBoardText::ETileVerdict::Locked;
	const bool bGradeVisible = !bLocked && Fixture.bShipped && Fixture.Grade != EQualityGrade::Max;
	const bool bGradeMaterials = GradeBadge && Grade
		&& (GradeBadge->Background.GetResourceObject() != nullptr) == bGradeVisible
		&& (Grade->GetFont().FontMaterial != nullptr) == bGradeVisible;
	const bool bGradeSlot = GradeSize
		&& GradeSize->GetVisibility() == (bGradeVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const bool bReleaseVisible = !bLocked;
	const bool bRecommendVisible = Fixture.bRecommended && !bLocked;
	const bool bGroupVisibility = ReleaseGroup && Sticker && VerdictRow && StampFrame
		&& ReleaseGroup->GetVisibility() == (bReleaseVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed)
		&& StampFrame->GetVisibility() == (bReleaseVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed)
		&& Sticker->GetVisibility() == (bRecommendVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed)
		&& VerdictRow->GetVisibility() == (bReleaseVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed)
		&& IsVisibleThroughParents(ReleaseGroup) == bReleaseVisible
		&& IsVisibleThroughParents(StampFrame) == bReleaseVisible
		&& IsVisibleThroughParents(Sticker) == bRecommendVisible
		&& IsVisibleThroughParents(VerdictRow) == bReleaseVisible;
	const bool bHasMetadata = Fixture.Verdict != PitchBoardText::ETileVerdict::Wait && !bLocked;
	const bool bMetadata = Combo
		&& Combo->GetText().ToString() == (bHasMetadata ? TEXT("퍼즐 · 토이") : TEXT(""))
		&& IsVisibleThroughParents(Combo) == bHasMetadata
		&& !Tile.GetWidgetFromName(TEXT("Text_Tagline"))
		&& !Tile.GetWidgetFromName(TEXT("Text_ExpectedGrade"));
	Record->SetBoolField(TEXT("metadata_reset_and_estimate_absent"), bMetadata);
	const FString ExpectedRelease = bReleaseVisible
		? (Fixture.bShipped ? TEXT("개발 완료") : TEXT("미개발")) : FString();
	FLinearColor ExpectedStampColor;
	const bool bStampColor = StampFrame && Release
		&& ReadTileDefaultColor(Tile, Fixture.bShipped ? TEXT("DevelopedStampColor") : TEXT("UndevelopedStampColor"), ExpectedStampColor)
		&& StampFrame->Background.OutlineSettings.Color.GetSpecifiedColor().Equals(ExpectedStampColor, 0.001f)
		&& Release->GetColorAndOpacity().GetSpecifiedColor().Equals(ExpectedStampColor, 0.001f);
	const bool bStampStyle = StampFrame
		&& StampFrame->Background.DrawAs == ESlateBrushDrawType::RoundedBox
		&& StampFrame->Background.TintColor.GetSpecifiedColor().A <= 0.001f
		&& FMath::Abs(StampFrame->GetRenderTransform().Angle) >= 1.f;
	Record->SetBoolField(TEXT("status_stamp_ink_and_outline"), bStampColor);
	Record->SetBoolField(TEXT("status_stamp_authored_style"), bStampStyle);
	const bool bVerdictWidgetsAbsent = !Tile.GetWidgetFromName(TEXT("VerdictSize"))
		&& !Tile.GetWidgetFromName(TEXT("VerdictBadge"))
		&& !Tile.GetWidgetFromName(TEXT("VerdictScale"))
		&& !Tile.GetWidgetFromName(TEXT("Text_Verdict"));
	const bool bState = Name && Name->GetText().ToString() == FString::Printf(TEXT("프로젝트 %d"), Tile.GetProjectIndex())
		&& Release && Release->GetText().ToString() == ExpectedRelease
		&& IsVisibleThroughParents(Release) == bReleaseVisible
		&& bStampColor && bStampStyle
		&& Grade && Grade->GetText().ToString() == (bGradeVisible ? QualityGradeToAlphabetString(Fixture.Grade) : FString())
		&& GradeBadge && IsVisibleThroughParents(GradeBadge) == bGradeVisible
		&& bVerdictWidgetsAbsent
		&& StickerRoot && IsVisibleThroughParents(StickerRoot) == bRecommendVisible
		&& bGroupVisibility
		&& !OldDot && !OldDotBox && Tile.GetIsEnabled() == !bLocked;
	Record->SetBoolField(TEXT("state_and_visibility"), bState);
	Record->SetBoolField(TEXT("grade_material_and_reset"), bGradeMaterials);
	Record->SetBoolField(TEXT("grade_slot_visibility"), bGradeSlot);
	Record->SetBoolField(TEXT("history_recommendation_row_visibility"), bGroupVisibility);
	Record->SetBoolField(TEXT("verdict_widgets_absent"), bVerdictWidgetsAbsent);
	const bool bPlateColor = HasExpectedPlateColor(Tile, Fixture);
	Record->SetBoolField(TEXT("plate_matches_class_defaults"), bPlateColor);
	Record->SetStringField(TEXT("release"), Release ? Release->GetText().ToString() : FString());
	Record->SetStringField(TEXT("grade"), Grade ? Grade->GetText().ToString() : FString());
	Record->SetStringField(TEXT("name"), Name ? Name->GetText().ToString() : FString());
	UObject* CoverResource = Cover ? Cover->GetBrush().GetResourceObject() : nullptr;
	UMaterialInstanceDynamic* Mosaic = Cast<UMaterialInstanceDynamic>(CoverResource);
	const bool bCoverStateCorrect = Cover && IsVisibleThroughParents(Cover)
		&& (Fixture.bShipped ? CoverResource && !Mosaic
			: Mosaic && FMath::IsNearlyEqual(Mosaic->K2_GetScalarParameterValue(TEXT("Bright")), 0.25f, 0.001f));
	Record->SetBoolField(TEXT("cover_state_correct"), bCoverStateCorrect);
	const FBounds TileBox = Bounds(&Tile, &Panel);
	const FBounds PlateBox = Bounds(Plate, &Panel);
	const FBounds ReleaseBox = Bounds(Release, &Panel);
	const FBounds StampBox = VisualBounds(StampFrame, &Panel);
	const FBounds GradeBox = Bounds(Grade, &Panel);
	const FBounds NameBox = Bounds(Name, &Panel);
	const FBounds ComboBox = Bounds(Combo, &Panel);
	const FBounds CoverBox = Bounds(Cover, &Panel);
	const FBounds WellBox = Bounds(Tile.GetWidgetFromName(TEXT("WellBox")), &Panel);
	const UTexture2D* SourceCover = Mosaic ? Cast<UTexture2D>(Mosaic->K2_GetTextureParameterValue(TEXT("Cover")))
		: Cast<UTexture2D>(CoverResource);
	const bool bCoverFits = SourceCover && CoverBox.bFound && WellBox.bFound
		&& CoverBox.Position.X >= WellBox.Position.X - 1 && CoverBox.Position.Y >= WellBox.Position.Y - 1
		&& CoverBox.Right() <= WellBox.Right() + 1 && CoverBox.Bottom() <= WellBox.Bottom() + 1
		&& FMath::IsNearlyEqual(CoverBox.Size.X / CoverBox.Size.Y,
			static_cast<double>(SourceCover->GetSizeX()) / SourceCover->GetSizeY(), 0.01);
	Record->SetBoolField(TEXT("cover_source_aspect_and_containment"), bCoverFits);
	const UWidget* WellBackground = Tile.GetWidgetFromName(TEXT("WellBG"));
	const bool bImageOnly = WellBackground && !IsVisibleThroughParents(WellBackground)
		&& CoverBox.bFound && WellBox.bFound
		&& CoverBox.Position.Equals(WellBox.Position, 1.f) && CoverBox.Size.Equals(WellBox.Size, 1.f);
	Record->SetBoolField(TEXT("cover_without_side_bands"), bImageOnly);
	Record->SetObjectField(TEXT("cover_bounds"), JsonBounds(CoverBox));
	const FBounds StickerBox = VisualBounds(Sticker, &Panel);
	Record->SetObjectField(TEXT("tile"), JsonBounds(TileBox));
	Record->SetObjectField(TEXT("release_bounds"), JsonBounds(ReleaseBox));
	Record->SetObjectField(TEXT("grade_bounds"), JsonBounds(GradeBox));
	Record->SetObjectField(TEXT("name_bounds"), JsonBounds(NameBox));
	Record->SetObjectField(TEXT("sticker_bounds"), JsonBounds(StickerBox));
	bool bTextFits = FSlateApplication::IsInitialized();
	if (bTextFits)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		for (const UCommonTextBlock* Label : {Name, Combo, Release, Grade})
		{
			if (!Label || !IsVisibleThroughParents(Label)) continue;
			const FGeometry& Geometry = Label->GetCachedGeometry();
			const float FontScale = Geometry.GetAccumulatedLayoutTransform().GetScale();
			FVector2D Measured = Measure->Measure(Label->GetText(), Label->GetFont(), FontScale)
				/ FMath::Max(FontScale, UE_SMALL_NUMBER);
			const FGeometry& PanelGeometry = Panel.GetCachedGeometry();
			const FVector2D Origin = PanelGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D Drawn = PanelGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Measured)) - Origin;
			const FBounds Allotted = Bounds(Label, &Panel);
			bTextFits &= Allotted.bFound && Allotted.Size.X + 1 >= FMath::Abs(Drawn.X)
				&& Allotted.Size.Y + 1 >= FMath::Abs(Drawn.Y);
		}
	}
	const bool bRow = NameBox.bFound && TileBox.bFound
		&& NameBox.Position.X >= TileBox.Position.X - 1 && NameBox.Right() <= TileBox.Right() + 1
		&& NameBox.Position.Y >= TileBox.Position.Y - 1
		&& (!bHasMetadata || (ComboBox.bFound
			&& NameBox.Bottom() <= ComboBox.Position.Y + 1
			&& ComboBox.Right() <= TileBox.Right() + 1 && ComboBox.Bottom() <= TileBox.Bottom() + 1))
		&& (!bReleaseVisible || (ReleaseBox.bFound && !DoBoundsOverlap(ReleaseBox, NameBox)
			&& (!bHasMetadata || !DoBoundsOverlap(ReleaseBox, ComboBox))
			&& ReleaseBox.Right() <= TileBox.Right() + 1
			&& StampBox.bFound && StampBox.Position.X >= TileBox.Position.X - 12
			&& StampBox.Right() <= TileBox.Right() + 12))
		&& (!bGradeVisible || (GradeBox.bFound && ReleaseBox.bFound
			&& !DoBoundsOverlap(GradeBox, ReleaseBox)
			&& !DoBoundsOverlap(GradeBox, StampBox)
			&& GradeBox.Position.X >= TileBox.Position.X - 1
			&& GradeBox.Right() <= TileBox.Right() + 1));
	Record->SetBoolField(TEXT("text_measured_fits"), bTextFits);
	Record->SetBoolField(TEXT("name_and_history_layout"), bRow);
	bool bSticker = true;
	if (Fixture.bRecommended && !bLocked)
	{
		const FBounds GridBox = Bounds(&Grid, &Panel);
		const UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Tile.Slot);
		const FMargin SlotPadding = Grid.GetSlotPadding();
		const FGeometry& GridGeometry = Grid.GetCachedGeometry();
		const FGeometry& PanelGeometry = Panel.GetCachedGeometry();
		const FVector2D GridOrigin = PanelGeometry.AbsoluteToLocal(GridGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D PaddingScale = PanelGeometry.AbsoluteToLocal(GridGeometry.LocalToAbsolute(
			FVector2D(SlotPadding.Left, SlotPadding.Top))) - GridOrigin;
		const FVector2D PaddingFarScale = PanelGeometry.AbsoluteToLocal(GridGeometry.LocalToAbsolute(
			FVector2D(SlotPadding.Right, SlotPadding.Bottom))) - GridOrigin;
		const double CellWidth = GridBox.Size.X / ProjectColumns;
		const int32 GridRows = FMath::DivideAndRoundUp(Grid.GetChildrenCount(), ProjectColumns);
		const double CellHeight = GridBox.Size.Y / FMath::Max(1, GridRows);
		const double CellLeft = GridBox.Position.X + (GridSlot ? GridSlot->GetColumn() : 0) * CellWidth;
		const double CellTop = GridBox.Position.Y + (GridSlot ? GridSlot->GetRow() : 0) * CellHeight;
		Record->SetNumberField(TEXT("slot_padding_left_scaled"), PaddingScale.X);
		Record->SetNumberField(TEXT("slot_padding_top_scaled"), PaddingScale.Y);
		Record->SetNumberField(TEXT("slot_padding_right_scaled"), PaddingFarScale.X);
		Record->SetNumberField(TEXT("slot_padding_bottom_scaled"), PaddingFarScale.Y);
		bSticker = StickerBox.bFound && TileBox.bFound && PlateBox.bFound
			&& GridBox.bFound && GridSlot && StickerBox.Position.X >= CellLeft - 1
			&& StickerBox.Position.Y >= CellTop - 1
			&& StickerBox.Right() <= CellLeft + CellWidth + 1
			&& StickerBox.Bottom() <= CellTop + CellHeight + 1
			&& TileBox.Position.X >= CellLeft + PaddingScale.X - 1
			&& TileBox.Position.Y >= CellTop + PaddingScale.Y - 1
			&& TileBox.Right() <= CellLeft + CellWidth - PaddingFarScale.X + 1
			&& TileBox.Bottom() <= CellTop + CellHeight - PaddingFarScale.Y + 1
			&& StickerBox.Position.X >= PlateBox.Position.X - 12
			&& StickerBox.Position.Y >= PlateBox.Position.Y - 12
			&& StickerBox.Right() <= PlateBox.Right() + 12 && StickerBox.Bottom() <= PlateBox.Bottom() + 12
			&& DoBoundsOverlap(StickerBox, CoverBox)
			&& StickerBox.Position.X <= CoverBox.Position.X + CoverBox.Size.X * 0.5f
			&& StickerBox.Position.Y <= CoverBox.Position.Y + CoverBox.Size.Y * 0.5f
			&& !DoBoundsOverlap(StickerBox, NameBox)
			&& (!bHasMetadata || !DoBoundsOverlap(StickerBox, ComboBox))
			&& (!bReleaseVisible || !DoBoundsOverlap(StickerBox, ReleaseBox))
			&& (!bGradeVisible || !DoBoundsOverlap(StickerBox, GradeBox));
		const FBounds ScrollBox = Bounds(Panel.GetWidgetFromName(TEXT("GridScroll")), &Panel);
		bSticker &= ScrollBox.bFound;
		int32 CulledNeighbors = 0;
		for (const UPitchTileWidget* Neighbor : Neighbors)
		{
			if (Neighbor && Neighbor != &Tile)
			{
				const UUniformGridSlot* NeighborSlot = Cast<UUniformGridSlot>(Neighbor->Slot);
				if (!NeighborSlot)
				{
					bSticker = false;
					continue;
				}
				FBounds NeighborCell;
				NeighborCell.Position = GridBox.Position + FVector2D(
					NeighborSlot->GetColumn() * CellWidth, NeighborSlot->GetRow() * CellHeight);
				NeighborCell.Size = FVector2D(CellWidth, CellHeight);
				NeighborCell.bFound = GridBox.bFound;
				// 화면 밖 행은 이전 Draw의 캐시 좌표를 유지하므로 현재 셀의 클리핑으로 판별한다.
				if (!DoBoundsOverlap(NeighborCell, ScrollBox))
				{
					++CulledNeighbors;
					continue;
				}
				bSticker &= !DoBoundsOverlap(StickerBox, Bounds(Neighbor->GetWidgetFromName(TEXT("Plate")), &Panel));
			}
		}
		Record->SetNumberField(TEXT("culled_neighbor_count"), CulledNeighbors);
	}
	Record->SetBoolField(TEXT("cover_corner_recommendation_inside_and_clear"), bSticker);
	Record->SetBoolField(TEXT("passed"), bState && bGradeMaterials && bGradeSlot && bMetadata && bPlateColor
		&& bCoverStateCorrect && bCoverFits && bImageOnly && bTextFits && bRow && bSticker);
	return Record;
}

static TSharedPtr<FJsonObject> CheckTierCoverReuse(UPitchBoardWidget& Panel, UTexture2D* FirstCover)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	UPanelWidget* Rail = Cast<UPanelWidget>(Panel.GetWidgetFromName(TEXT("TierRailBox")));
	UPitchTierTabWidget* Original = Rail && Rail->GetChildrenCount() > 1
		? Cast<UPitchTierTabWidget>(Rail->GetChildAt(1)) : nullptr;
	UPitchTierTabWidget* Reused = Original ? CreateWidget<UPitchTierTabWidget>(&Panel, Original->GetClass()) : nullptr;
	UImage* Image = Reused ? Cast<UImage>(Reused->GetWidgetFromName(TEXT("TierCoverImage"))) : nullptr;
	const UImage* OtherImage = Original ? Cast<UImage>(Original->GetWidgetFromName(TEXT("TierCoverImage"))) : nullptr;
	UObject* OtherResource = OtherImage ? OtherImage->GetBrush().GetResourceObject() : nullptr;
	UMaterialInstanceDynamic* OtherMID = Cast<UMaterialInstanceDynamic>(OtherResource);
	UTexture2D* OtherCover = OtherMID ? Cast<UTexture2D>(OtherMID->K2_GetTextureParameterValue(TEXT("Cover")))
		: Cast<UTexture2D>(OtherResource);
	bool bPassed = Image && FirstCover && OtherCover && FirstCover != OtherCover;
	if (bPassed)
	{
		const FSlateBrush OriginalBrush = Image->GetBrush();
		auto Check = [&](const TCHAR* Name, bool bLocked, bool bDone, UTexture2D* Texture)
		{
			FPitchTierTabData Data;
			Data.bLocked = bLocked;
			Data.bDone = bDone;
			Data.bCurrent = !bLocked && !bDone;
			Data.Cover = Texture;
			Reused->Configure(Data);
			const FSlateBrush& Brush = Image->GetBrush();
			UMaterialInstanceDynamic* Mosaic = Cast<UMaterialInstanceDynamic>(Brush.GetResourceObject());
			const bool bResourceMatches = Texture && bLocked
				? Mosaic && Mosaic->K2_GetTextureParameterValue(TEXT("Cover")) == Texture
				: Brush.GetResourceObject() == Texture;
			const bool bCasePassed = bResourceMatches
				&& Image->GetVisibility() == (Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed)
				&& Brush.DrawAs == OriginalBrush.DrawAs && Brush.ImageSize == OriginalBrush.ImageSize
				&& Brush.OutlineSettings.CornerRadii == OriginalBrush.OutlineSettings.CornerRadii
				&& Reused->GetIsEnabled();
			Result->SetBoolField(Name, bCasePassed);
			bPassed &= bCasePassed;
		};
		Check(TEXT("current_original"), false, false, FirstCover);
		const FBox2f OriginalUV = Image->GetBrush().GetUVRegion();
		Check(TEXT("locked_mosaic"), true, false, FirstCover);
		const bool bCropPreserved = Image->GetBrush().GetUVRegion() == OriginalUV;
		Result->SetBoolField(TEXT("crop_preserved"), bCropPreserved);
		bPassed &= bCropPreserved;
		Check(TEXT("unlocked_original"), false, false, FirstCover);
		Check(TEXT("relocked_other_cover"), true, false, OtherCover);
		Check(TEXT("missing_cover_cleared"), true, false, nullptr);
		Check(TEXT("restored_mosaic"), true, false, FirstCover);
		Check(TEXT("completed_original"), false, true, FirstCover);
	}
	Result->SetBoolField(TEXT("passed"), bPassed);
	return Result;
}

static void CheckTileStates(UWorld* CommandWorld)
{
	UPitchBoardWidget* Panel = nullptr;
	for (TObjectIterator<UPitchBoardWidget> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld() == CommandWorld && It->IsActivated())
		{
			Panel = *It;
			break;
		}
	}
	UUniformGridPanel* Grid = Panel ? Cast<UUniformGridPanel>(Panel->GetWidgetFromName(TEXT("TileGrid"))) : nullptr;
	UScrollBox* GridScroll = Panel ? Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("GridScroll"))) : nullptr;
	if (!CommandWorld || (CommandWorld->WorldType != EWorldType::PIE && CommandWorld->WorldType != EWorldType::Game)
		|| !Panel || !Grid || !GridScroll
		|| Grid->GetChildrenCount() == 0 || !Cast<UPitchTileWidget>(Grid->GetChildAt(0)))
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.CheckTileStates requires an open PIE or Standalone board with actual pitch tiles."));
		return;
	}
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("PitchBoardPolish/TileStatusValidation");
	IFileManager::Get().MakeDirectory(*Directory, true);
	struct FOriginalTileSlot
	{
		UWidget* Child;
		int32 Row;
		int32 Column;
		EHorizontalAlignment Horizontal;
		EVerticalAlignment Vertical;
	};
	TArray<FOriginalTileSlot> Original;
	for (UWidget* Child : Grid->GetAllChildren())
	{
		const UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Child->Slot);
		Original.Add({Child, GridSlot ? GridSlot->GetRow() : 0, GridSlot ? GridSlot->GetColumn() : 0,
			GridSlot ? GridSlot->GetHorizontalAlignment() : HAlign_Fill,
			GridSlot ? GridSlot->GetVerticalAlignment() : VAlign_Fill});
	}
	const float PreviousScroll = GridScroll->GetScrollOffset();
	UClass* TileClass = Original[0].Child->GetClass();
	TArray<TSharedPtr<FJsonValue>> OriginalCovers;
	bool bOriginalCoverSizes = true;
	for (const FOriginalTileSlot& Saved : Original)
	{
		const UUserWidget* ExistingTile = Cast<UUserWidget>(Saved.Child);
		const UImage* ExistingCover = ExistingTile
			? Cast<UImage>(ExistingTile->GetWidgetFromName(TEXT("CoverImage"))) : nullptr;
		if (!ExistingCover || ExistingCover->GetVisibility() == ESlateVisibility::Collapsed) continue;
		UObject* Resource = ExistingCover->GetBrush().GetResourceObject();
		UMaterialInstanceDynamic* Mosaic = Cast<UMaterialInstanceDynamic>(Resource);
		const UTexture2D* Source = Mosaic ? Cast<UTexture2D>(Mosaic->K2_GetTextureParameterValue(TEXT("Cover")))
			: Cast<UTexture2D>(Resource);
		const FVector2D BrushSize = ExistingCover->GetBrush().ImageSize;
		const bool bMatches = Source && Source->GetSizeX() > 0 && Source->GetSizeY() > 0
			&& FMath::IsNearlyEqual(BrushSize.X, static_cast<double>(Source->GetSizeX()), 0.01)
			&& FMath::IsNearlyEqual(BrushSize.Y, static_cast<double>(Source->GetSizeY()), 0.01);
		TSharedPtr<FJsonObject> CoverRecord = MakeShared<FJsonObject>();
		CoverRecord->SetStringField(TEXT("tile"), ExistingTile->GetName());
		CoverRecord->SetNumberField(TEXT("brush_width"), BrushSize.X);
		CoverRecord->SetNumberField(TEXT("brush_height"), BrushSize.Y);
		CoverRecord->SetBoolField(TEXT("initial_brush_matches_source"), bMatches);
		OriginalCovers.Add(MakeShared<FJsonValueObject>(CoverRecord));
		bOriginalCoverSizes &= bMatches;
	}
	UTexture2D* CoverTexture = nullptr;
	for (const FOriginalTileSlot& Saved : Original)
	{
		const UUserWidget* ExistingTile = Cast<UUserWidget>(Saved.Child);
		const UImage* ExistingCover = ExistingTile
			? Cast<UImage>(ExistingTile->GetWidgetFromName(TEXT("CoverImage"))) : nullptr;
		UObject* Resource = ExistingCover ? ExistingCover->GetBrush().GetResourceObject() : nullptr;
		CoverTexture = Cast<UTexture2D>(Resource);
		if (!CoverTexture)
		{
			UMaterialInstanceDynamic* ExistingMosaic = Cast<UMaterialInstanceDynamic>(Resource);
			CoverTexture = ExistingMosaic ? Cast<UTexture2D>(ExistingMosaic->K2_GetTextureParameterValue(TEXT("Cover"))) : nullptr;
		}
		if (CoverTexture) break;
	}
	const TSharedPtr<FJsonObject> TierCoverReuse = CheckTierCoverReuse(*Panel, CoverTexture);
	Grid->ClearChildren();
	TArray<UPitchTileWidget*> Tiles;
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(TileFixtures)); ++Index)
	{
		UPitchTileWidget* Tile = CreateWidget<UPitchTileWidget>(Panel, TileClass);
		if (!Tile) continue;
		Tile->Configure(MakeTileFixture(TileFixtures[Index], Index, CoverTexture));
		Tile->SetSelected(Index == 0);
		Grid->AddChildToUniformGrid(Tile, Index / ProjectColumns, Index % ProjectColumns);
		Tiles.Add(Tile);
	}
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<TSharedPtr<FJsonValue>> Cases;
	bool bPassed = Tiles.Num() == static_cast<int32>(UE_ARRAY_COUNT(TileFixtures)) && CoverTexture != nullptr
		&& bOriginalCoverSizes && !OriginalCovers.IsEmpty() && TierCoverReuse->GetBoolField(TEXT("passed"));
	for (int32 Width : {2304, 3120})
	{
		GridScroll->SetScrollOffset(0.f);
		const FString Filename = FString::Printf(TEXT("tile_states_%d.png"), Width);
		const bool bSaved = Draw(*Panel, Width, Directory / Filename);
		TSharedPtr<FJsonObject> CaptureRecord = MakeShared<FJsonObject>();
		CaptureRecord->SetStringField(TEXT("image"), Filename);
		CaptureRecord->SetNumberField(TEXT("width"), Width);
		CaptureRecord->SetBoolField(TEXT("saved"), bSaved);
		Captures.Add(MakeShared<FJsonValueObject>(CaptureRecord));
		bPassed &= bSaved;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			// 화면 밖에서 culled 된 텍스트는 이전 기하가 남으므로 실제 배치 후 측정한다.
			GridScroll->ScrollWidgetIntoView(Tiles[Index], false, EDescendantScrollDestination::Center);
			const FString CaseFilename = FString::Printf(TEXT("tile_%s_%d.png"), TileFixtures[Index].Name, Width);
			const bool bCaseSaved = Draw(*Panel, Width, Directory / CaseFilename);
			TSharedPtr<FJsonObject> Record = InspectTileFixture(*Tiles[Index], *Panel, *Grid, TileFixtures[Index], Tiles);
			Record->SetStringField(TEXT("image"), CaseFilename);
			Record->SetBoolField(TEXT("saved"), bCaseSaved);
			Record->SetNumberField(TEXT("viewport_width"), Width);
			Cases.Add(MakeShared<FJsonValueObject>(Record));
			bPassed &= bCaseSaved && Record->GetBoolField(TEXT("passed"));
		}
		bool bReleasePlatesMatch = Tiles.Num() == static_cast<int32>(UE_ARRAY_COUNT(TileFixtures));
		bool bLabelOpacityPreserved = bReleasePlatesMatch;
		for (const TPair<int32, int32>& Pair : {TPair<int32, int32>(1, 5), TPair<int32, int32>(2, 6), TPair<int32, int32>(3, 7)})
		{
			const UBorder* ReleasedPlate = bReleasePlatesMatch ? Cast<UBorder>(Tiles[Pair.Key]->GetWidgetFromName(TEXT("Plate"))) : nullptr;
			const UBorder* UnreleasedPlate = bReleasePlatesMatch ? Cast<UBorder>(Tiles[Pair.Value]->GetWidgetFromName(TEXT("Plate"))) : nullptr;
			bReleasePlatesMatch &= ReleasedPlate && UnreleasedPlate
				&& ReleasedPlate->GetBrushColor().Equals(UnreleasedPlate->GetBrushColor(), 0.001f);
			for (const TCHAR* Name : {TEXT("Text_Name")})
			{
				const UWidget* ReleasedLabel = bLabelOpacityPreserved ? Tiles[Pair.Key]->GetWidgetFromName(Name) : nullptr;
				const UWidget* UnreleasedLabel = bLabelOpacityPreserved ? Tiles[Pair.Value]->GetWidgetFromName(Name) : nullptr;
				bLabelOpacityPreserved &= ReleasedLabel && UnreleasedLabel
					&& ReleasedLabel->GetRenderOpacity() > 0.f
					&& FMath::IsNearlyEqual(ReleasedLabel->GetRenderOpacity(), UnreleasedLabel->GetRenderOpacity());
			}
		}
		CaptureRecord->SetBoolField(TEXT("same_verdict_release_plates_match"), bReleasePlatesMatch);
		CaptureRecord->SetBoolField(TEXT("same_verdict_name_opacity_preserved"), bLabelOpacityPreserved);
		bPassed &= bReleasePlatesMatch && bLabelOpacityPreserved;
		GridScroll->ScrollToEnd();
		const FString BottomFilename = FString::Printf(TEXT("tile_states_bottom_%d.png"), Width);
		const bool bBottomSaved = Draw(*Panel, Width, Directory / BottomFilename);
		TSharedPtr<FJsonObject> BottomRecord = MakeShared<FJsonObject>();
		BottomRecord->SetStringField(TEXT("image"), BottomFilename);
		BottomRecord->SetNumberField(TEXT("width"), Width);
		BottomRecord->SetBoolField(TEXT("saved"), bBottomSaved);
		Captures.Add(MakeShared<FJsonValueObject>(BottomRecord));
		bPassed &= bBottomSaved;
	}
	bool bRepeatedGradeStrengthStable = false;
	if (Tiles.Num() == static_cast<int32>(UE_ARRAY_COUNT(TileFixtures)))
	{
		UPitchTileWidget* Reused = Tiles[0];
		UBorder* GradePlate = Cast<UBorder>(Reused->GetWidgetFromName(TEXT("Badge")));
		UCommonTextBlock* GradeText = Cast<UCommonTextBlock>(Reused->GetWidgetFromName(TEXT("Text_Badge")));
		UMaterialInstanceDynamic* FrameBefore = GradePlate ? GradePlate->GetDynamicMaterial() : nullptr;
		UMaterialInstanceDynamic* LetterBefore = GradeText ? GradeText->GetDynamicFontMaterial() : nullptr;
		const float FrameStrengthBefore = FrameBefore ? FrameBefore->K2_GetScalarParameterValue(TEXT("Strength")) : 0.f;
		const float LetterStrengthBefore = LetterBefore ? LetterBefore->K2_GetScalarParameterValue(TEXT("Strength")) : 0.f;
		Reused->Configure(MakeTileFixture(TileFixtures[0], 0, CoverTexture));
		UMaterialInstanceDynamic* FrameAfter = GradePlate ? GradePlate->GetDynamicMaterial() : nullptr;
		UMaterialInstanceDynamic* LetterAfter = GradeText ? GradeText->GetDynamicFontMaterial() : nullptr;
		bRepeatedGradeStrengthStable = FrameAfter && LetterAfter
			&& FrameStrengthBefore > 0.f && LetterStrengthBefore > 0.f
			&& FMath::IsNearlyEqual(FrameAfter->K2_GetScalarParameterValue(TEXT("Strength")), FrameStrengthBefore, 0.001f)
			&& FMath::IsNearlyEqual(LetterAfter->K2_GetScalarParameterValue(TEXT("Strength")), LetterStrengthBefore, 0.001f);
		bPassed &= bRepeatedGradeStrengthStable;
		Reused->SetSelected(false);
		const int32 TransitionIndices[] = {0, 5, 8, 1};
		for (int32 Step = 0; Step < static_cast<int32>(UE_ARRAY_COUNT(TransitionIndices)); ++Step)
		{
			const int32 FixtureIndex = TransitionIndices[Step];
			Reused->Configure(MakeTileFixture(TileFixtures[FixtureIndex], 0, CoverTexture));
			GridScroll->ScrollWidgetIntoView(Reused, false, EDescendantScrollDestination::Center);
			const FString Filename = FString::Printf(TEXT("reuse_%d_%s.png"), Step, TileFixtures[FixtureIndex].Name);
			const bool bSaved = Draw(*Panel, 2304, Directory / Filename);
			TSharedPtr<FJsonObject> Record = InspectTileFixture(*Reused, *Panel, *Grid, TileFixtures[FixtureIndex], Tiles);
			Record->SetStringField(TEXT("transition_image"), Filename);
			Record->SetBoolField(TEXT("saved"), bSaved);
			Record->SetNumberField(TEXT("transition_step"), Step);
			Cases.Add(MakeShared<FJsonValueObject>(Record));
			bPassed &= bSaved && Record->GetBoolField(TEXT("passed"));
		}
		// Reuse the same native tile across product states, then restore a project.
		for (int32 Width : {2304, 3120})
		{
			for (int32 StateIndex = 0; StateIndex < 6; ++StateIndex)
			{
				FPitchTileData Product = MakeTileFixture(TileFixtures[0], 0, CoverTexture);
				Product.bManufacturing = StateIndex < 5;
				Product.bHasShipped = StateIndex >= 2;
				Product.bManufacturingResearchInProgress = StateIndex == 1;
				Product.bManufacturingQuantitySecured = StateIndex == 3;
				Product.Verdict = StateIndex == 4 ? PitchBoardText::ETileVerdict::Locked : PitchBoardText::ETileVerdict::Pass;
				Reused->Configure(Product);
				GridScroll->ScrollWidgetIntoView(Reused, false, EDescendantScrollDestination::Center);
				const FString Filename = FString::Printf(TEXT("manufacturing_reuse_%d_%d.png"), StateIndex, Width);
				const bool bSaved = Draw(*Panel, Width, Directory / Filename);
				const UCommonTextBlock* ProductName = Cast<UCommonTextBlock>(Reused->GetWidgetFromName(TEXT("Text_Name")));
				const UCommonTextBlock* StatusText = Cast<UCommonTextBlock>(Reused->GetWidgetFromName(TEXT("ManufacturingStatusText")));
				const UWidget* StatusFrame = Reused->GetWidgetFromName(TEXT("ManufacturingStatusFrame"));
				const UWidget* CoverFrame = Reused->GetWidgetFromName(TEXT("ManufacturingCoverFrame"));
				const bool bStatusExpected = StateIndex < 4;
				bool bStatePassed = bSaved && StatusText && StatusFrame && CoverFrame && ProductName
					&& IsVisibleThroughParents(StatusFrame) == bStatusExpected
					&& IsVisibleThroughParents(StatusText) == bStatusExpected
					&& IsVisibleThroughParents(CoverFrame) == Product.bManufacturing
					&& IsVisibleThroughParents(Reused->GetWidgetFromName(TEXT("ReleaseStampFrame"))) == !Product.bManufacturing
					&& IsVisibleThroughParents(Reused->GetWidgetFromName(TEXT("Text_Combo"))) == !Product.bManufacturing
					&& Reused->GetIsEnabled() == (StateIndex != 4);
				if (bStatusExpected && StatusText && ProductName)
				{
					const FBounds StatusBox = Bounds(StatusFrame, Panel);
					const FBounds ProductBox = Bounds(Reused, Panel);
					const FBounds NameBox = Bounds(ProductName, Panel);
					const FGeometry& TextGeometry = StatusText->GetCachedGeometry();
					const float TextScale = TextGeometry.GetAccumulatedLayoutTransform().GetScale();
					const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(
						StatusText->GetText(), StatusText->GetFont(), TextScale) / FMath::Max(TextScale, UE_SMALL_NUMBER);
					bStatePassed &= !StatusText->GetText().IsEmpty() && StatusBox.bFound && ProductBox.bFound
						&& !DoBoundsOverlap(StatusBox, NameBox) && StatusBox.Bottom() <= ProductBox.Bottom() + 1
						&& StatusBox.Right() <= ProductBox.Right() + 1
						&& TextSize.X <= TextGeometry.GetLocalSize().X + 1
						&& TextSize.Y <= TextGeometry.GetLocalSize().Y + 1
						&& FMath::IsNearlyZero(StatusFrame->GetRenderTransform().Angle);
				}
				TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
				Record->SetStringField(TEXT("case"), TEXT("manufacturing_project_reuse"));
				Record->SetStringField(TEXT("image"), Filename);
				Record->SetNumberField(TEXT("state"), StateIndex);
				Record->SetNumberField(TEXT("viewport_width"), Width);
				Record->SetBoolField(TEXT("passed"), bStatePassed);
				Cases.Add(MakeShared<FJsonValueObject>(Record));
				bPassed &= bStatePassed;
			}
		}
	}
	Grid->ClearChildren();
	for (const FOriginalTileSlot& Saved : Original)
	{
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(Saved.Child, Saved.Row, Saved.Column))
		{
			GridSlot->SetHorizontalAlignment(Saved.Horizontal);
			GridSlot->SetVerticalAlignment(Saved.Vertical);
		}
	}
	GridScroll->SetScrollOffset(PreviousScroll);
	Panel->ForceLayoutPrepass();
	const bool bRestoredImage = Draw(*Panel, 2304, Directory / TEXT("restored_board.png"));
	bool bRestored = bRestoredImage && Grid->GetChildrenCount() == Original.Num()
		&& FMath::IsNearlyEqual(GridScroll->GetScrollOffset(), PreviousScroll, 2.f);
	for (int32 Index = 0; Index < Original.Num() && bRestored; ++Index)
	{
		const UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Original[Index].Child->Slot);
		bRestored &= Grid->GetChildAt(Index) == Original[Index].Child && GridSlot
			&& GridSlot->GetRow() == Original[Index].Row && GridSlot->GetColumn() == Original[Index].Column
			&& GridSlot->GetHorizontalAlignment() == Original[Index].Horizontal
			&& GridSlot->GetVerticalAlignment() == Original[Index].Vertical;
	}
	bPassed &= bRestored;
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetBoolField(TEXT("original_board_restored"), bRestored);
	Report->SetBoolField(TEXT("original_cover_sizes_match_source"), bOriginalCoverSizes);
	Report->SetBoolField(TEXT("repeated_grade_strength_stable"), bRepeatedGradeStrengthStable);
	Report->SetObjectField(TEXT("tier_cover_reuse"), TierCoverReuse);
	Report->SetArrayField(TEXT("original_covers"), OriginalCovers);
	Report->SetStringField(TEXT("scope"), TEXT("Actual game tile WBP, UI-only fixture and Slate font measurements; no data/save writes or physical pointer input."));
	Report->SetArrayField(TEXT("captures"), Captures);
	Report->SetArrayField(TEXT("cases"), Cases);
	FString Json;
	FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	const bool bReportSaved = FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("report.json")));
	UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.CheckTileStates passed=%d report_saved=%d directory=%s"), bPassed, bReportSaved, *Directory);
}

static FAutoConsoleCommandWithWorld CheckTileStatesCommand(TEXT("PitchBoardUI.CheckTileStates"),
	TEXT("Render the actual pitch tile WBP at two widths and verify status layout plus widget reuse without changing game data."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&CheckTileStates));

static void CheckRewards(UWorld*)
{
	UPitchBoardWidget* Panel = nullptr;
	for (TObjectIterator<UPitchBoardWidget> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld()
			&& It->GetWorld()->WorldType == EWorldType::PIE && It->IsActivated()) { Panel = *It; break; }
	}
	UPanelWidget* Preview = Panel ? Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("RewardPreviewBox"))) : nullptr;
	UItemCardWidget* FirstReward = Preview && Preview->GetChildrenCount() > 0
		? Cast<UItemCardWidget>(Preview->GetChildAt(0)) : nullptr;
	if (!FirstReward)
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.CheckRewards requires an open PIE board with actual reward items."));
		return;
	}
	// Exercise the original card's click forwarding without simulating pointer input or granting loot.
	const FVector2D BoardSizeBefore = Panel->GetDesiredSize();
	FirstReward->OnItemCardClicked.ExecuteIfBound(NAME_None);
	ULaunchLootOddsWidget* Popup = nullptr;
	for (TObjectIterator<ULaunchLootOddsWidget> It; It; ++It)
	{
		if (It->GetWorld() == Panel->GetWorld() && It->IsInViewport() && It->IsVisible()) { Popup = *It; break; }
	}
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("card_click_forwarded"), Popup != nullptr);
	Panel->ForceLayoutPrepass();
	const bool bBoardUnchanged = Panel->GetDesiredSize().Equals(BoardSizeBefore, .1f);
	const bool bSeparateWindow = Popup && Popup->IsInViewport() && Popup->GetParent() == nullptr;
	Report->SetBoolField(TEXT("board_size_unchanged"), bBoardUnchanged);
	Report->SetBoolField(TEXT("independent_viewport_window"), bSeparateWindow);
	const FAnchors PopupAnchors = Popup ? Popup->GetAnchorsInViewport() : FAnchors();
	const bool bFullscreenHost = Popup && PopupAnchors.Minimum.Equals(FVector2D::ZeroVector)
		&& PopupAnchors.Maximum.Equals(FVector2D(1.f, 1.f));
	Report->SetBoolField(TEXT("fullscreen_modal_host"), bFullscreenHost);
	Report->SetBoolField(TEXT("odds_button_exists"), Panel->GetWidgetFromName(TEXT("RewardOddsButton")) != nullptr);
	bool bFits = false;
	if (Popup)
	{
		Popup->ForceLayoutPrepass();
		const FVector2D Desired = Popup->GetDesiredSize();
		const FVector2D Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Popup).GetLocalSize();
		bFits = Desired.X <= Viewport.X - 24.f && Desired.Y <= Viewport.Y - 24.f;
		Report->SetNumberField(TEXT("popup_width"), Desired.X);
		Report->SetNumberField(TEXT("popup_height"), Desired.Y);
		Report->SetNumberField(TEXT("viewport_width"), Viewport.X);
		Report->SetNumberField(TEXT("viewport_height"), Viewport.Y);
		const FString ImagePath = FPaths::ProjectSavedDir() / TEXT("PitchBoardPolish/Captures/reward_odds.png");
		Report->SetBoolField(TEXT("image_saved"), Draw(*Popup, 2304, ImagePath));
	}
	Report->SetBoolField(TEXT("popup_size_fits_viewport"), bFits);
	const bool bOpenPassed = Popup && bFits && bBoardUnchanged && bSeparateWindow && bFullscreenHost;
	Report->SetBoolField(TEXT("open_passed"), bOpenPassed);
	Report->SetStringField(TEXT("scope"), TEXT("Actual reward card delegate forwarding, not physical pointer input. No launch/reward grant. Run CheckRewardsClosed immediately after closing the board."));
	FString Json;
	FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("PitchBoardPolish/reward_interactions.json")));
	UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.CheckRewards open_passed=%d"), bOpenPassed);
}

static void CheckRewardsClosed(UWorld*)
{
	bool bVisible = false;
	bool bBoardOpen = false;
	for (TObjectIterator<UPitchBoardWidget> It; It; ++It)
	{
		bBoardOpen |= !It->HasAnyFlags(RF_ClassDefaultObject) && It->GetWorld()
			&& It->GetWorld()->WorldType == EWorldType::PIE && It->IsActivated();
	}
	for (TObjectIterator<ULaunchLootOddsWidget> It; It; ++It)
	{
		bVisible |= It->GetWorld() && It->GetWorld()->WorldType == EWorldType::PIE && It->IsInViewport() && It->IsVisible();
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("PitchBoardPolish/reward_interactions.json");
	FString Json;
	TSharedPtr<FJsonObject> Report;
	if (FFileHelper::LoadFileToString(Json, *Path)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Report) && Report)
	{
		Report->SetBoolField(TEXT("closed_with_board"), !bVisible && !bBoardOpen);
		Report->SetBoolField(TEXT("passed"), Report->GetBoolField(TEXT("open_passed")) && !bVisible && !bBoardOpen);
		Json.Reset();
		FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		FFileHelper::SaveStringToFile(Json, *Path);
	}
	UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.CheckRewardsClosed passed=%d"), !bVisible && !bBoardOpen);
}

static FAutoConsoleCommandWithWorld CheckRewardsCommand(TEXT("PitchBoardUI.CheckRewards"),
	TEXT("Exercise the first actual PIE reward card delegate and capture its existing odds popup without granting rewards."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&CheckRewards));
static FAutoConsoleCommandWithWorld CheckRewardsClosedCommand(TEXT("PitchBoardUI.CheckRewardsClosed"),
	TEXT("Verify the reward popup is hidden immediately after the board is closed."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&CheckRewardsClosed));

static void CheckAssets()
{
	TArray<TSharedPtr<FJsonValue>> Results;
	bool bPassed = true;
	const TCHAR* Paths[] = {
		TEXT("/Game/CompanyGrowth/UI/Elements/Office/UIE_PitchTierTab.UIE_PitchTierTab"),
		TEXT("/Game/CompanyGrowth/UI/Elements/Office/UIE_PitchTile.UIE_PitchTile"),
		TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_OfficeProjectCardRequest.UIE_OfficeProjectCardRequest"),
		TEXT("/Game/CompanyGrowth/UI/Panels/OfficeMap/UI_PitchBoardPanel.UI_PitchBoardPanel") };
	for (const TCHAR* Path : Paths)
	{
		UBlueprint* Blueprint = TSoftObjectPtr<UBlueprint>(FSoftObjectPath(Path)).LoadSynchronous();
		if (Blueprint) FKismetEditorUtilities::CompileBlueprint(Blueprint);
		const bool bCompiled = Blueprint && Blueprint->GeneratedClass
			&& (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("asset"), Path);
		Result->SetBoolField(TEXT("compiled"), bCompiled);
		Result->SetNumberField(TEXT("status"), Blueprint ? static_cast<int32>(Blueprint->Status) : -1);
		Results.Add(MakeShared<FJsonValueObject>(Result));
		bPassed &= bCompiled;
	}
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetArrayField(TEXT("assets"), Results);
	FString Json;
	FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("PitchBoardPolish");
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("compile_status.json"))))
	{
		UE_LOG(LogTemp, Error, TEXT("PitchBoardUI.CheckAssets passed=0: compile status save failed"));
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("PitchBoardUI.CheckAssets passed=%d; no packages saved"), bPassed);
}

static FAutoConsoleCommand CheckAssetsCommand(TEXT("PitchBoardUI.CheckAssets"),
	TEXT("Compile the four pitch assets and report native Blueprint status without saving."),
	FConsoleCommandDelegate::CreateStatic(&CheckAssets));
}

#endif
