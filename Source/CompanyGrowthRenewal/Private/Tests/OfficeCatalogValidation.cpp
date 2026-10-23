#if WITH_EDITOR
#include "UI/Panel/OfficeDecorationPanelWidget.h"
#include "UI/Panel/OfficeWorkstationPanelWidget.h"
#include "UI/Element/Office/OfficeWorkstationCardWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Office/OfficeDecorationCardWidget.h"
#include "UI/Element/Cards/CardInfoWidget.h"
#include "Components/ListView.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/Texture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Data/EntityCardData.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Containers/Ticker.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"

static void PrimeCatalogSceneRendering(UWorld* GameWorld)
{
	// Slate UI 머티리얼을 그리려면 씬 텍스처 유니폼 버퍼가 먼저 생성되어야 한다.
	TStrongObjectPtr<UTextureRenderTarget2D> SceneTarget(NewObject<UTextureRenderTarget2D>());
	SceneTarget->RenderTargetFormat = RTF_RGBA8;
	SceneTarget->InitAutoFormat(64, 64);
	TStrongObjectPtr<USceneCaptureComponent2D> SceneCapture(NewObject<USceneCaptureComponent2D>());
	SceneCapture->TextureTarget = SceneTarget.Get();
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->RegisterComponentWithWorld(GameWorld);
	SceneCapture->CaptureScene();
	FlushRenderingCommands();
	SceneCapture->UnregisterComponent();
}

static void BindCatalogCardInfo(const TCHAR* CardPath)
{
	UBlueprint* CardAsset = TSoftObjectPtr<UBlueprint>(FSoftObjectPath(CardPath)).LoadSynchronous();
	UClass* InfoClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_DecorationCardInfo.UIE_DecorationCardInfo_C"))).LoadSynchronous();
	UWidgetTree* Tree = CardAsset ? FindObject<UWidgetTree>(CardAsset, TEXT("WidgetTree")) : nullptr;
	UUserWidget* Frame = Tree ? Cast<UUserWidget>(Tree->FindWidget(TEXT("UIE_EntityCardFrame"))) : nullptr;
	UWidget* PreviousInfo = Tree ? Tree->FindWidget(TEXT("CardInfoWidget")) : nullptr;
	if (!Frame || !PreviousInfo || !InfoClass) return;
	if (PreviousInfo->GetClass() != InfoClass)
	{
		Frame->SetContentForSlot(TEXT("ContentSlot"), nullptr);
		PreviousInfo->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
		UUserWidget* Info = Tree->ConstructWidget<UUserWidget>(InfoClass, TEXT("CardInfoWidget"));
		Frame->SetContentForSlot(TEXT("ContentSlot"), Info);
		CardAsset->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(CardAsset);
	}
}
static FAutoConsoleCommand BindOfficeCatalogInfo(
	TEXT("cg.OfficeCatalog.BindCardInfo"), TEXT("Bind the authored decoration info variant to the card named slot."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		BindCatalogCardInfo(TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_DecorationCard.UIE_DecorationCard"));
	}));
static FAutoConsoleCommand BindOfficeWorkstationInfo(
	TEXT("cg.OfficeWorkstation.BindCardInfo"), TEXT("Bind the authored catalog info variant to the workstation card named slot."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		BindCatalogCardInfo(TEXT("/Game/CompanyGrowth/UI/Elements/Cards/UIE_WorkstationCard.UIE_WorkstationCard"));
	}));

// 이름 글자는 DownOnly ScaleBox 안이라 칸보다 긴 이름만 줄어든다 ― 표의 모든 이름이 줄어도 하한 이상이어야 폰에서 읽힌다.
static constexpr float MinShownNameSize = UCardInfoWidget::MinShownNameSize;

template <typename FCardRow>
static bool AllDisplayNamesFit(const UTextBlock* SampleName, const TArray<FCardRow>& Rows, FString& OutDetail)
{
	const UScaleBox* NameFit = Cast<UScaleBox>(SampleName->GetParent());
	const UVerticalBoxSlot* FitSlot = NameFit ? Cast<UVerticalBoxSlot>(NameFit->Slot) : nullptr;
	// 배율 = 할당 폭 ÷ 원하는 폭이라 슬롯이 칸 폭을 꽉 줘야 줄어든다
	if (!FitSlot || NameFit->GetStretch() != EStretch::ScaleToFit || NameFit->GetStretchDirection() != EStretchDirection::DownOnly
		|| FitSlot->GetHorizontalAlignment() != HAlign_Fill)
	{
		OutDetail = TEXT("card name sits in a fill-width DownOnly ScaleBox");
		return false;
	}
	const float Room = NameFit->GetCachedGeometry().GetLocalSize().X;
	const float FontSize = SampleName->GetFont().Size;
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	TArray<FString> TooSmall;
	int32 Shrunk = 0;
	float Smallest = FontSize;
	for (const FCardRow& Row : Rows)
	{
		const float Need = Measure->Measure(Row.DisplayName, SampleName->GetFont()).X;
		const float Shown = Need > Room ? FontSize * Room / Need : FontSize;
		Shrunk += Need > Room ? 1 : 0;
		Smallest = FMath::Min(Smallest, Shown);
		if (Row.DisplayName.IsEmpty() || Shown < MinShownNameSize) TooSmall.Add(FString::Printf(TEXT("%s(%.1f)"), *Row.RowName.ToString(), Shown));
	}
	OutDetail = FString::Printf(TEXT("all %d display names shown at %.0f+ in name box %.0fpx (shrunk %d, smallest %.1f) %s"),
		Rows.Num(), MinShownNameSize, Room, Shrunk, Smallest, *FString::Join(TooSmall, TEXT(" ")));
	return TooSmall.IsEmpty() && !Rows.IsEmpty() && Room > 0.f;
}

// 표에 없는 긴 이름(10-11 넘쳤던 책상 이름)을 잠깐 넣어 렌더로 본다 ― 그려진 흰 글자가 칸 안에 들고, 이름 줄 높이는 그대로여야 아래 가격 줄이 안 밀린다.
static bool LongNameShrinksInside(UTextBlock* Name, TFunctionRef<void()> Redraw, TFunctionRef<bool(FImage&)> ReadShrunk, FString& OutDetail)
{
	const UWidget* Fit = Name->GetParent();
	if (!Fit)
	{
		OutDetail = TEXT("long name fit box found");
		return false;
	}
	const FText Original = Name->GetText();
	const double RowHeight = Fit->GetCachedGeometry().GetLocalSize().Y;
	Name->SetText(FText::FromString(TEXT("라벤더 판넬 책상")));
	Redraw();
	const FGeometry FitGeo = Fit->GetCachedGeometry();
	const FGeometry NameGeo = Name->GetCachedGeometry();
	const double Need = Name->GetDesiredSize().X;
	const double Scale = NameGeo.GetAccumulatedLayoutTransform().GetScale() / FitGeo.GetAccumulatedLayoutTransform().GetScale();
	const double ShownHeight = FitGeo.GetLocalSize().Y;
	// 1배 그리기라 절대 좌표가 곧 렌더 타깃 픽셀이다. 위젯 상자(원래 폭 × 배율)는 실제 글자보다 2~3px 넓게 잡혀 판정은 픽셀로 한다.
	const FVector2D BoxMin = FitGeo.GetAbsolutePosition();
	const FVector2D BoxMax = BoxMin + FitGeo.GetAbsoluteSize();
	int32 InkLeft = MAX_int32;
	int32 InkRight = MIN_int32;
	FImage Shot;
	FImage Pixels;
	if (ReadShrunk(Shot))
	{
		Shot.CopyTo(Pixels, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const TArrayView64<FColor> Colors = Pixels.AsBGRA8();
		// 칸 좌우 여백(슬롯 20)까지 넓게 훑는다 ― 칸 밖으로 번진 흰 글자를 놓치지 않게
		for (int32 Y = FMath::Max(0, FMath::FloorToInt(BoxMin.Y)); Y < FMath::Min(Pixels.SizeY, FMath::CeilToInt(BoxMax.Y)); ++Y)
			for (int32 X = FMath::Max(0, FMath::FloorToInt(BoxMin.X) - 20); X < FMath::Min(Pixels.SizeX, FMath::CeilToInt(BoxMax.X) + 20); ++X)
			{
				const FColor& C = Colors[int64(Y) * Pixels.SizeX + X];
				if (FMath::Min3(C.R, C.G, C.B) >= 160) { InkLeft = FMath::Min(InkLeft, X); InkRight = FMath::Max(InkRight, X); }
			}
	}
	Name->SetText(Original);
	Redraw();
	const bool bInk = InkLeft <= InkRight;
	const double InkCenterOffset = bInk ? (InkLeft + InkRight + 1) * 0.5 - (BoxMin.X + BoxMax.X) * 0.5 : 0.0;
	OutDetail = FString::Printf(TEXT("long name %.0fpx shrinks x%.3f, white ink %d..%d inside name box %.0f..%.0f (center offset %.1f), name row %.0f -> %.0f"),
		Need, Scale, InkLeft, InkRight, BoxMin.X, BoxMax.X, InkCenterOffset, RowHeight, ShownHeight);
	// ScaleBox 배율(0.800)이 칸÷글자 폭(0.793)보다 살짝 커 끝 글자가 1px 넘을 수 있다 ― 슬롯 좌우 여백 20 안이라 카드 밖으로는 안 나간다
	constexpr double InkSlack = 2.0;
	return bInk && Need > FitGeo.GetLocalSize().X && Scale < 1.0 && InkLeft >= BoxMin.X - InkSlack && InkRight + 1 <= BoxMax.X + InkSlack
		&& FMath::Abs(InkCenterOffset) <= 2.0 && FMath::IsNearlyEqual(ShownHeight, RowHeight, 0.5);
}

static bool ReadTargetImage(UTextureRenderTarget2D* RenderTarget, const FString& PngPath, FImage& OutImage)
{
	if (!FImageUtils::GetRenderTargetImage(RenderTarget, OutImage)) return false;
	OutImage.GammaSpace = EGammaSpace::sRGB;
	TArray64<uint8> Bytes;
	FImageUtils::CompressImage(Bytes, TEXT("png"), OutImage);
	return FFileHelper::SaveArrayToFile(Bytes, *PngPath);
}

// 행동 줄(재배치 + 공용 닫기 X)이 아래 목록 판에 붙지 않는지 ― 아래 여백 2px 로 붙어 보이던 회귀를 잡는다
static bool CatalogActionsClearPlate(const UUserWidget* Panel, FString& OutDetail)
{
	const UWidget* Relocate = Panel->GetWidgetFromName(TEXT("RelocateButton"));
	const UWidget* Close = Panel->GetWidgetFromName(TEXT("BackButton"));
	const UWidget* Plate = Panel->GetWidgetFromName(TEXT("CatalogContentPlate"));
	if (!Relocate || !Close || !Plate)
	{
		OutDetail = TEXT("action row widgets found");
		return false;
	}
	const FGeometry& PanelGeo = Panel->GetCachedGeometry();
	const FGeometry& RelocateGeo = Relocate->GetCachedGeometry();
	const FGeometry& CloseGeo = Close->GetCachedGeometry();
	const double RelocateTop = PanelGeo.AbsoluteToLocal(RelocateGeo.GetAbsolutePosition()).Y;
	const double RelocateBottom = PanelGeo.AbsoluteToLocal(RelocateGeo.LocalToAbsolute(FVector2D(0.0, RelocateGeo.GetLocalSize().Y))).Y;
	const double CloseCenter = PanelGeo.AbsoluteToLocal(CloseGeo.LocalToAbsolute(CloseGeo.GetLocalSize() * 0.5)).Y;
	const double PlateTop = PanelGeo.AbsoluteToLocal(Plate->GetCachedGeometry().GetAbsolutePosition()).Y;
	const double RelocateHeight = RelocateGeo.GetLocalSize().Y;
	const double CloseHeight = CloseGeo.GetLocalSize().Y;
	const double Gap = PlateTop - RelocateBottom;
	const double CenterOffset = CloseCenter - (RelocateTop + RelocateBottom) * 0.5;
	OutDetail = FString::Printf(TEXT("action row relocate h%.0f close h%.0f gap to plate %.0f close center offset %.1f"),
		RelocateHeight, CloseHeight, Gap, CenterOffset);
	return FMath::IsNearlyEqual(RelocateHeight, 86.0, 1.0) && FMath::IsNearlyEqual(CloseHeight, 72.0, 1.0)
		&& Gap >= 12.0 && FMath::Abs(CenterOffset) <= 2.0;
}

namespace OfficeCatalogValidation
{
struct FVisualCapture : TSharedFromThis<FVisualCapture>
{
	TStrongObjectPtr<UOfficeDecorationPanelWidget> Panel;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	TSharedPtr<SWidget> Surface;
	TUniquePtr<FWidgetRenderer> Renderer;
	TWeakObjectPtr<UWorld> GameWorld;
	int32 ViewIndex = 0;
	int32 Phase = 0;
	FString Report;
	bool Tick(float)
	{
		if (!GameWorld.IsValid() || !Panel.IsValid()) return false;
		const int32 Width = ViewIndex < 5 ? 2304 : 3120;
		const bool bCarpetView = ViewIndex % 5 == 4;
		const int32 GroupIndex = bCarpetView ? 0 : ViewIndex % 5;
		UListView* List = Cast<UListView>(Panel->GetWidgetFromName(TEXT("DecorationListView")));
		if (Phase == 0)
		{
			const FObjectProperty* Property = FindFProperty<FObjectProperty>(Panel->GetClass(), TEXT("CatalogGroupButtons"));
			CastChecked<UCommonButtonGroupBase>(Property->GetObjectPropertyValue_InContainer(Panel.Get()))->SelectButtonAtIndex(GroupIndex);
			if (GroupIndex == 0)
			{
				const FObjectProperty* FilterProperty = FindFProperty<FObjectProperty>(Panel->GetClass(), TEXT("DecorationTabButtonGroup"));
				UCommonButtonGroupBase* Filters = CastChecked<UCommonButtonGroupBase>(FilterProperty->GetObjectPropertyValue_InContainer(Panel.Get()));
				Filters->SelectButtonAtIndex(bCarpetView ? Filters->FindButtonIndex(Cast<UCommonButtonBase>(Panel->GetWidgetFromName(TEXT("CarpetTab")))) : 0);
			}
			Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(Width, 1440), TF_Bilinear, false));
		}
		Panel->SetRenderOpacity(1.f);
		Panel->SetRenderTransform(FWidgetTransform());
		Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(Width, 1440), 0.15f);
		if (const TSharedPtr<SWidget> ListSlate = List->GetCachedWidget())
			ListSlate->Tick(ListSlate->GetCachedGeometry(), FSlateApplication::Get().GetCurrentTime(), 0.15f);
		FlushRenderingCommands();
		if (++Phase < 5) return true;
		bool bIconsMatch = !List->GetDisplayedEntryWidgets().IsEmpty();
		for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
			if (UOfficeDecorationCardWidget* Card = Cast<UOfficeDecorationCardWidget>(Entry))
				bIconsMatch &= Card->GetEntityImage() && Card->GetDecorationInfo().UIIcon.IsValid()
					&& Card->GetEntityImage()->GetBrush().GetResourceObject() == Card->GetDecorationInfo().UIIcon.Get();
		Report += FString::Printf(TEXT("%s width %d group %d carpet %d visible card icon bindings\n"), bIconsMatch ? TEXT("PASS") : TEXT("FAIL"), Width, GroupIndex, bCarpetView);
		bool bNamesMatch = !List->GetDisplayedEntryWidgets().IsEmpty();
		UTextBlock* SampleName = nullptr;
		for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
			if (UOfficeDecorationCardWidget* Card = Cast<UOfficeDecorationCardWidget>(Entry))
			{
				const UUserWidget* Info = Cast<UUserWidget>(Entry->GetWidgetFromName(TEXT("CardInfoWidget")));
				UTextBlock* NameText = Info ? Cast<UTextBlock>(Info->GetWidgetFromName(TEXT("BuildEntityName"))) : nullptr;
				bNamesMatch &= NameText && !Card->GetDecorationInfo().DisplayName.IsEmpty() && NameText->GetText().EqualTo(Card->GetDecorationInfo().DisplayName);
				if (NameText && !SampleName) SampleName = NameText;
			}
		Report += FString::Printf(TEXT("%s width %d group %d carpet %d visible card names are DT display names\n"), bNamesMatch ? TEXT("PASS") : TEXT("FAIL"), Width, GroupIndex, bCarpetView);
		// 흐림은 뒤에 카드가 남은 목록에서만 켜진다. 카펫(3개)만 한 화면에 다 들어간다.
		const UWidget* Fade = Panel->GetWidgetFromName(TEXT("CatalogListFade"));
		const bool bFadeOn = Fade && Fade->GetVisibility() == ESlateVisibility::HitTestInvisible;
		Report += FString::Printf(TEXT("%s width %d group %d carpet %d edge fade %s\n"), bFadeOn != bCarpetView ? TEXT("PASS") : TEXT("FAIL"), Width, GroupIndex, bCarpetView, bFadeOn ? TEXT("on") : TEXT("off"));
		// 개수는 필터 줄과 같은 판의 칩 위에 둔다
		const UWidget* CountText = Panel->GetWidgetFromName(TEXT("CatalogCount"));
		const UWidget* CountPlate = Panel->GetWidgetFromName(TEXT("CatalogCountBG"));
		const bool bCountChip = CountText && CountPlate && CountPlate->IsVisible()
			&& CountPlate->GetCachedGeometry().GetLocalSize().X > CountText->GetCachedGeometry().GetLocalSize().X
			&& CountText->GetCachedGeometry().GetLocalSize().X > 0.0;
		Report += FString::Printf(TEXT("%s width %d group %d carpet %d item count text on chip plate\n"), bCountChip ? TEXT("PASS") : TEXT("FAIL"), Width, GroupIndex, bCarpetView);
		FString ActionDetail;
		const bool bActionsClear = CatalogActionsClearPlate(Panel.Get(), ActionDetail);
		Report += FString::Printf(TEXT("%s width %d group %d carpet %d %s\n"), bActionsClear ? TEXT("PASS") : TEXT("FAIL"), Width, GroupIndex, bCarpetView, *ActionDetail);
		if (ViewIndex == 0 && SampleName)
		{
			FString Detail;
			const bool bFit = AllDisplayNamesFit(SampleName, GameWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>()->GetDecorationCardInfos(), Detail);
			Report += FString::Printf(TEXT("%s %s\n"), bFit ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		}
		if (GroupIndex == 0 && !bCarpetView && SampleName)
		{
			FString Detail;
			const FString ShotPath = FPaths::ProjectSavedDir() / TEXT("OfficeCatalogValidation") / FString::Printf(TEXT("catalog_%d_longname.png"), Width);
			const bool bShrinks = LongNameShrinksInside(SampleName,
				[this, Width]
				{
					for (int32 Pass = 0; Pass < 3; ++Pass)
						Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(Width, 1440), 0.f);
					FlushRenderingCommands();
				},
				[this, &ShotPath](FImage& Shot) { return ReadTargetImage(Target.Get(), ShotPath, Shot); }, Detail);
			Report += FString::Printf(TEXT("%s width %d %s\n"), bShrinks ? TEXT("PASS") : TEXT("FAIL"), Width, *Detail);
		}
		FImage Image;
		if (FImageUtils::GetRenderTargetImage(Target.Get(), Image))
		{
			Image.GammaSpace = EGammaSpace::sRGB;
			TArray64<uint8> Bytes;
			FImageUtils::CompressImage(Bytes, TEXT("png"), Image);
			const FString FileName = bCarpetView ? FString::Printf(TEXT("catalog_%d_carpet.png"), Width) : FString::Printf(TEXT("catalog_%d_%d.png"), Width, GroupIndex);
			FFileHelper::SaveArrayToFile(Bytes, *(FPaths::ProjectSavedDir() / TEXT("OfficeCatalogValidation") / FileName));
		}
		Phase = 0;
		if (++ViewIndex < 10) return true;
		Panel->DeactivateWidget();
		FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("OfficeCatalogValidation/visual-report.txt")));
		UE_LOG(LogTemp, Display, TEXT("OfficeCatalogValidation visual capture completed across live frames"));
		return false;
	}
};
static void CaptureLiveFrames()
{
	UWorld* GameWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) { GameWorld = Context.World(); break; }
	if (!GameWorld || !GameWorld->GetGameInstance()) return;
	UTableManagerSubsystem* Tables = GameWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	TSharedRef<FVisualCapture> Capture = MakeShared<FVisualCapture>();
	Capture->GameWorld = GameWorld;
	Capture->Panel.Reset(CreateWidget<UOfficeDecorationPanelWidget>(GameWorld->GetGameInstance(), Tables->GetWidgetClass(EWidgetType::OfficeDecorationPanel)));
	Capture->Surface = Capture->Panel->TakeWidget();
	FindFProperty<FBoolProperty>(Capture->Panel->GetClass(), TEXT("bInstantAppear"))->SetPropertyValue_InContainer(Capture->Panel.Get(), true);
	Capture->Panel->ActivateWidget();
	Capture->Panel->SetRenderOpacity(1.f);
	Capture->Panel->SetRenderTransform(FWidgetTransform());
	Capture->Renderer = MakeUnique<FWidgetRenderer>(true);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Capture](float Delta) { return Capture->Tick(Delta); }), 0.15f);
}
static UCommonButtonGroupBase* Group(UObject* Object, const TCHAR* Name)
{
	const FObjectProperty* Property = FindFProperty<FObjectProperty>(Object->GetClass(), Name);
	return Property ? Cast<UCommonButtonGroupBase>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
}
static void Run()
{
	UWorld* GameWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) { GameWorld = Context.World(); break; }
	if (!GameWorld || !GameWorld->GetGameInstance()) return;
	PrimeCatalogSceneRendering(GameWorld);
	UTableManagerSubsystem* Tables = GameWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	TSoftClassPtr<UOfficeDecorationPanelWidget> ClassPath(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Panels/OfficeMap/UI_OfficeDecorationPanel.UI_OfficeDecorationPanel_C")));
	TStrongObjectPtr<UOfficeDecorationPanelWidget> Panel(CreateWidget<UOfficeDecorationPanelWidget>(GameWorld->GetGameInstance(), ClassPath.LoadSynchronous()));
	if (!Panel.IsValid()) return;
	TSharedRef<SWidget> Surface = Panel->TakeWidget();
	Panel->ActivateWidget();
	Panel->SetRenderOpacity(1.f);
	Panel->SetRenderTransform(FWidgetTransform());
	UListView* List = Cast<UListView>(Panel->GetWidgetFromName(TEXT("DecorationListView")));
	UCommonButtonGroupBase* Groups = Group(Panel.Get(), TEXT("CatalogGroupButtons"));
	UCommonButtonGroupBase* Filters = Group(Panel.Get(), TEXT("DecorationTabButtonGroup"));
	if (!List || !Groups || !Filters) return;
	FString Report;
	int32 Failures = 0;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("OfficeCatalogValidation");
	IFileManager::Get().MakeDirectory(*Directory, true);
	auto Check = [&](bool bOK, const FString& Name) { Report += FString(bOK ? TEXT("PASS ") : TEXT("FAIL ")) + Name + TEXT("\n"); if (!bOK) ++Failures; };
	Check(Tables->GetDecorationCatalog().Num() == 9, TEXT("nine category definitions"));
	const FDecorationCatalogRow* CarpetRow = Tables->GetDecorationCatalog().Find(TEXT("Carpet"));
	Check(CarpetRow && CarpetRow->Category == EDecorationCategory::Carpet && CarpetRow->Group == TEXT("Props")
		&& CarpetRow->GroupOrder == 0 && CarpetRow->CategoryOrder == 3 && !CarpetRow->CategoryLabel.IsEmpty(), TEXT("carpet classification belongs to Props"));
	Check(Groups->GetButtonCount() == 4, TEXT("four major tabs"));
	UCommonButtonBase* CarpetButton = Cast<UCommonButtonBase>(Panel->GetWidgetFromName(TEXT("CarpetTab")));
	const int32 CarpetFilterIndex = Filters->FindButtonIndex(CarpetButton);
	Check(CarpetButton && CarpetFilterIndex != INDEX_NONE, TEXT("carpet filter registered"));
	Check(Tables->GetDecorationCardsByCategory(EDecorationCategory::Carpet).Num() == 3, TEXT("three carpet products"));
	for (int32 Width : {2304, 3120})
	{
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Width, 1440), TF_Bilinear, false));
		for (int32 GroupIndex = 0; GroupIndex < 4; ++GroupIndex)
		{
			Groups->SelectButtonAtIndex(GroupIndex);
			Check(CarpetButton && (CarpetButton->GetVisibility() != ESlateVisibility::Collapsed) == (GroupIndex == 0),
				FString::Printf(TEXT("carpet filter only visible in Props group %d"), GroupIndex));
			TSet<EDecorationCategory> ExpectedCategories;
			for (const auto& Pair : Tables->GetDecorationCatalog())
				if (Pair.Value.GroupOrder == GroupIndex) ExpectedCategories.Add(Pair.Value.Category);
			int32 ExpectedCount = 0;
			for (const FDecorationCardTable& Card : Tables->GetDecorationCardInfos())
				if (ExpectedCategories.Contains(Card.Category)) { ++ExpectedCount; Card.UIIcon.LoadSynchronous(); }
			Check(List->GetNumItems() == ExpectedCount, FString::Printf(TEXT("group %d width %d count %d"), GroupIndex, Width, List->GetNumItems()));
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Width, 1440), 0.f);
				if (const TSharedPtr<SWidget> ListSlate = List->GetCachedWidget())
					ListSlate->Tick(ListSlate->GetCachedGeometry(), FSlateApplication::Get().GetCurrentTime(), 0.f);
				for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
					if (UOfficeDecorationCardWidget* Card = Cast<UOfficeDecorationCardWidget>(Entry))
						Card->SetDecorationInfo(Card->GetDecorationInfo());
				FlushRenderingCommands();
			}
			const FVector2D ListSize = List->GetCachedGeometry().GetLocalSize();
			Check(ListSize.X >= 2000 && ListSize.Y <= 440, TEXT("single card row fits safe width"));
			Check(!List->GetDisplayedEntryWidgets().IsEmpty(), TEXT("virtualized cards generated"));
			FImage Image;
			if (FImageUtils::GetRenderTargetImage(Target.Get(), Image))
			{
				Image.GammaSpace = EGammaSpace::sRGB;
				TArray64<uint8> Bytes;
				FImageUtils::CompressImage(Bytes, TEXT("png"), Image);
				FFileHelper::SaveArrayToFile(Bytes, *(Directory / FString::Printf(TEXT("catalog_%d_%d.png"), Width, GroupIndex)));
			}
		}
	}
	Groups->SelectButtonAtIndex(0);
	Filters->SelectButtonAtIndex(2);
	Check(List->GetNumItems() == Tables->GetDecorationCardsByCategory(EDecorationCategory::Chair).Num(), TEXT("chair filter count"));
	Panel->DeactivateWidget(); Panel->ActivateWidget();
	Check(List->GetNumItems() == Tables->GetDecorationCardsByCategory(EDecorationCategory::Chair).Num(), TEXT("reactivation preserves selected filter"));
	Filters->SelectButtonAtIndex(CarpetFilterIndex);
	Check(List->GetNumItems() == 3, TEXT("carpet filter isolates three products"));
	bool bOnlyCarpets = List->GetNumItems() > 0;
	for (UObject* Item : List->GetListItems())
	{
		const UEntityCardData* CardData = Cast<UEntityCardData>(Item);
		bOnlyCarpets &= CardData && CardData->DecorationInfo.Category == EDecorationCategory::Carpet;
	}
	Check(bOnlyCarpets, TEXT("carpet filter excludes other Props"));
	Panel->DeactivateWidget(); Panel->ActivateWidget();
	Check(Filters->GetSelectedButtonIndex() == CarpetFilterIndex && List->GetNumItems() == 3, TEXT("reactivation preserves carpet filter"));
	Groups->SelectButtonAtIndex(1);
	Check(CarpetButton && CarpetButton->GetVisibility() == ESlateVisibility::Collapsed, TEXT("carpet filter hides on other group"));
	Groups->SelectButtonAtIndex(0);
	Check(Filters->GetSelectedButtonIndex() == 0, TEXT("return to Props resets to all"));
	Panel->DeactivateWidget();
	FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("report.txt")));
	UE_LOG(LogTemp, Display, TEXT("OfficeCatalogValidation finished: %d failures"), Failures);
}
static FAutoConsoleCommand Command(TEXT("cg.OfficeCatalog.Validate"), TEXT("Validate and render the Office decoration catalog in PIE."), FConsoleCommandDelegate::CreateStatic(&Run));
static FAutoConsoleCommand CaptureCommand(TEXT("cg.OfficeCatalog.Capture"), TEXT("Capture the catalog after texture streaming and material updates across live frames."), FConsoleCommandDelegate::CreateStatic(&CaptureLiveFrames));
}

namespace OfficeWorkstationValidation
{
struct FVisualCapture : TSharedFromThis<FVisualCapture>
{
	TStrongObjectPtr<UOfficeWorkstationPanelWidget> Panel;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	TSharedPtr<SWidget> Surface;
	TUniquePtr<FWidgetRenderer> Renderer;
	TWeakObjectPtr<UWorld> GameWorld;
	int32 ViewIndex = 0;
	int32 Phase = 0;
	int32 ExpectedCount = 0;
	int32 Failures = 0;
	bool bEndView = false;
	FString Report;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("OfficeWorkstationValidation");

	void Check(bool bOK, const FString& Name)
	{
		Report += FString(bOK ? TEXT("PASS ") : TEXT("FAIL ")) + Name + TEXT("\n");
		if (!bOK) ++Failures;
	}
	bool Tick(float)
	{
		if (!GameWorld.IsValid() || !Panel.IsValid()) return false;
		UListView* List = Cast<UListView>(Panel->GetWidgetFromName(TEXT("WorkstationListView")));
		if (!List) return false;
		const int32 Width = ViewIndex == 0 ? 2304 : 3120;
		if (Phase == 0)
			Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(Width, 1440), TF_Bilinear, false));
		Panel->SetRenderOpacity(1.f);
		Panel->SetRenderTransform(FWidgetTransform());
		Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(Width, 1440), 0.15f);
		if (const TSharedPtr<SWidget> ListSlate = List->GetCachedWidget())
			ListSlate->Tick(ListSlate->GetCachedGeometry(), FSlateApplication::Get().GetCurrentTime(), 0.15f);
		FlushRenderingCommands();
		if (++Phase < 8) return true;

		if (bEndView) Report += TEXT("-- scrolled to list end --\n");
		Check(List->GetNumItems() == ExpectedCount && ExpectedCount > 0,
			FString::Printf(TEXT("width %d Single DT count %d/%d"), Width, List->GetNumItems(), ExpectedCount));
		const FVector2D ListSize = List->GetCachedGeometry().GetLocalSize();
		Check(ListSize.X >= 2000 && ListSize.Y > 0 && ListSize.Y <= 440,
			FString::Printf(TEXT("width %d single-row list %.0fx%.0f"), Width, ListSize.X, ListSize.Y));
		bool bIconsMatch = !List->GetDisplayedEntryWidgets().IsEmpty();
		bool bCompactFrames = bIconsMatch;
		bool bCostsHidden = bIconsMatch;
		UTextBlock* SampleName = nullptr;
		for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
		{
			UOfficeWorkstationCardWidget* Card = Cast<UOfficeWorkstationCardWidget>(Entry);
			bIconsMatch &= Card && Card->GetEntityImage() && Card->GetWorkstationInfo().UIIcon.IsValid()
				&& Card->GetEntityImage()->GetBrush().GetResourceObject() == Card->GetWorkstationInfo().UIIcon.Get();
			UUserWidget* Frame = Cast<UUserWidget>(Entry->GetWidgetFromName(TEXT("UIE_EntityCardFrame")));
			UWidget* FrameSizeBox = Frame ? Frame->GetWidgetFromName(TEXT("SizeBox_37")) : nullptr;
			const FVector2D FrameSize = FrameSizeBox ? FrameSizeBox->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
			bCompactFrames &= FMath::IsNearlyEqual(FrameSize.X, 300.0, 1.0) && FMath::IsNearlyEqual(FrameSize.Y, 400.0, 1.0);
			UUserWidget* Info = Cast<UUserWidget>(Entry->GetWidgetFromName(TEXT("CardInfoWidget")));
			UWidget* Cost = Info ? Info->GetWidgetFromName(TEXT("UIE_Resource")) : nullptr;
			bCostsHidden &= Cost && Cost->GetVisibility() == ESlateVisibility::Collapsed;
			if (!SampleName && Info) SampleName = Cast<UTextBlock>(Info->GetWidgetFromName(TEXT("BuildEntityName")));
		}
		Check(bIconsMatch, FString::Printf(TEXT("width %d displayed card icon bindings"), Width));
		Check(bCompactFrames, FString::Printf(TEXT("width %d card frames 300x400"), Width));
		Check(bCostsHidden, FString::Printf(TEXT("width %d workstation cost is collapsed"), Width));
		if (!bEndView)
		{
			FString Detail = FString::Printf(TEXT("width %d card name text found"), Width);
			const bool bFit = SampleName && AllDisplayNamesFit(SampleName, GameWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>()->GetWorkstationCardsByType(EWorkstationType::Single), Detail);
			Check(bFit, Detail);
			FString LongDetail = TEXT("card name text found");
			const bool bShrinks = SampleName && LongNameShrinksInside(SampleName,
				[this, Width]
				{
					for (int32 Pass = 0; Pass < 3; ++Pass)
						Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(Width, 1440), 0.f);
					FlushRenderingCommands();
				},
				[this, Width](FImage& Shot) { return ReadTargetImage(Target.Get(), Directory / FString::Printf(TEXT("workstation_%d_longname.png"), Width), Shot); }, LongDetail);
			Check(bShrinks, FString::Printf(TEXT("width %d %s"), Width, *LongDetail));
		}
		FString ActionDetail;
		// 인자 평가 순서가 정해져 있지 않다 ― 판정을 먼저 끝내야 문구에 실측값이 실린다
		const bool bActionsClear = CatalogActionsClearPlate(Panel.Get(), ActionDetail);
		Check(bActionsClear, FString::Printf(TEXT("width %d %s"), Width, *ActionDetail));
		FImage Image;
		bool bSaved = false;
		if (FImageUtils::GetRenderTargetImage(Target.Get(), Image))
		{
			Image.GammaSpace = EGammaSpace::sRGB;
			TArray64<uint8> Bytes;
			FImageUtils::CompressImage(Bytes, TEXT("png"), Image);
			bSaved = FFileHelper::SaveArrayToFile(Bytes, *(Directory / FString::Printf(TEXT("workstation_%d%s.png"), Width, bEndView ? TEXT("_end") : TEXT(""))));
		}
		Check(bSaved, FString::Printf(TEXT("width %d PNG saved"), Width));
		const UWidget* Fade = Panel->GetWidgetFromName(TEXT("CatalogListFade"));
		if (!bEndView)
		{
			Check(Fade && Fade->GetVisibility() == ESlateVisibility::HitTestInvisible, FString::Printf(TEXT("width %d edge fade on while cards remain"), Width));
			bEndView = true;
			List->ScrollToBottom();
			Phase = 0;
			return true;
		}
		Check(Fade && Fade->GetVisibility() == ESlateVisibility::Collapsed, FString::Printf(TEXT("width %d edge fade off at list end"), Width));
		bEndView = false;
		Panel->DeactivateWidget();
		Phase = 0;
		if (++ViewIndex < 2)
		{
			// 패널은 다시 열 때 스크롤 위치를 되돌리지 않는다 ― 다음 폭은 처음부터 본다.
			List->ScrollToTop();
			Panel->ActivateWidget();
			Check(List->GetNumItems() == ExpectedCount, TEXT("reactivation restores Single DT count"));
			return true;
		}
		FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("report.txt")));
		UE_LOG(LogTemp, Display, TEXT("OfficeWorkstationValidation capture completed: %d failures"), Failures);
		return false;
	}
};
static void CaptureLiveFrames()
{
	UWorld* GameWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) { GameWorld = Context.World(); break; }
	if (!GameWorld || !GameWorld->GetGameInstance()) return;
	UTableManagerSubsystem* Tables = GameWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!Tables) return;
	TSharedRef<FVisualCapture> Capture = MakeShared<FVisualCapture>();
	Capture->GameWorld = GameWorld;
	Capture->ExpectedCount = Tables->GetWorkstationCardsByType(EWorkstationType::Single).Num();
	Capture->Panel.Reset(CreateWidget<UOfficeWorkstationPanelWidget>(GameWorld->GetGameInstance(), Tables->GetWidgetClass(EWidgetType::OfficeWorkstationPanel)));
	if (!Capture->Panel.IsValid()) return;
	PrimeCatalogSceneRendering(GameWorld);
	Capture->Surface = Capture->Panel->TakeWidget();
	if (FBoolProperty* InstantAppear = FindFProperty<FBoolProperty>(Capture->Panel->GetClass(), TEXT("bInstantAppear")))
		InstantAppear->SetPropertyValue_InContainer(Capture->Panel.Get(), true);
	Capture->Panel->ActivateWidget();
	Capture->Renderer = MakeUnique<FWidgetRenderer>(true);
	IFileManager::Get().MakeDirectory(*Capture->Directory, true);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Capture](float Delta) { return Capture->Tick(Delta); }), 0.15f);
}
static FAutoConsoleCommand CaptureCommand(TEXT("cg.OfficeWorkstation.Capture"), TEXT("Validate and capture the workstation catalog across live PIE frames."), FConsoleCommandDelegate::CreateStatic(&CaptureLiveFrames));
}
#endif
