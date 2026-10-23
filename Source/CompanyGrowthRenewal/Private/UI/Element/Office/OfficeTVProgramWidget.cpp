#include "UI/Element/Office/OfficeTVProgramWidget.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Enum/QualityGrade.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Common/QualityGradeMedalUtils.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"

#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace OfficeTVWidgetDetail
{
	const FName CoverParameter(TEXT("Cover"));
	const FName CellParameter(TEXT("CellPx"));
	const FName BrightParameter(TEXT("Bright"));
	const FVector2D PortraitBudget(384.f, 384.f);
	constexpr float MosaicBrightness = 0.72f;
	constexpr float StampDelay = 0.2f;
	constexpr float StampFall = 0.3f;
	constexpr float StampSettle = 0.12f;
	constexpr float StampFrom = 2.7f;
	constexpr float CardFlipStep = 0.35f;
	constexpr float CardFlipTime = 0.3f;
	constexpr float SlideTime = 0.45f;
	constexpr float BarGrowTime = 1.2f;
	constexpr float NeedleTime = 0.6f;
	constexpr float SlashTime = 0.35f;

	float EaseOut(float Alpha) { Alpha = FMath::Clamp(Alpha, 0.f, 1.f); return 1.f - FMath::Square(1.f - Alpha); }
	float EaseIn(float Alpha) { Alpha = FMath::Clamp(Alpha, 0.f, 1.f); return Alpha * Alpha; }

	void SetVisible(UWidget* Target, bool bVisible)
	{
		if (Target)
		{
			Target->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	void SetTransform(UWidget* Target, const FVector2D& Translation, const FVector2D& Scale, float Opacity)
	{
		if (!Target)
		{
			return;
		}
		FWidgetTransform Transform = Target->GetRenderTransform();
		Transform.Translation = Translation;
		Transform.Scale = Scale;
		Target->SetRenderTransform(Transform);
		Target->SetRenderOpacity(Opacity);
	}

	EQualityGrade GradeFromLetter(FName Letter, bool& bOutValid)
	{
		static const TPair<const TCHAR*, EQualityGrade> Letters[] = {
			{TEXT("S"), EQualityGrade::S}, {TEXT("A"), EQualityGrade::A}, {TEXT("B"), EQualityGrade::B},
			{TEXT("C"), EQualityGrade::C}, {TEXT("D"), EQualityGrade::D}, {TEXT("F"), EQualityGrade::F}};
		for (const TPair<const TCHAR*, EQualityGrade>& Pair : Letters)
		{
			if (Letter == FName(Pair.Key))
			{
				bOutValid = true;
				return Pair.Value;
			}
		}
		bOutValid = false;
		return EQualityGrade::S;
	}

	/** Places Target inside Bounds keeping the texture's aspect ratio, centred. */
	FBox2D FitInside(const FBox2D& Bounds, const UTexture2D* Texture)
	{
		const FVector2D Size = Bounds.GetSize();
		if (!Texture || Texture->GetSizeX() <= 0 || Texture->GetSizeY() <= 0 || Size.X <= 0.f || Size.Y <= 0.f)
		{
			return Bounds;
		}
		const float Scale = FMath::Min(Size.X / Texture->GetSizeX(), Size.Y / Texture->GetSizeY());
		const FVector2D Fitted(Texture->GetSizeX() * Scale, Texture->GetSizeY() * Scale);
		const FVector2D Min = Bounds.Min + (Size - Fitted) * 0.5f;
		return FBox2D(Min, Min + Fitted);
	}

	bool ReadCanvasRect(const UWidget* Target, FBox2D& Out)
	{
		const UCanvasPanelSlot* CanvasSlot = Target ? Cast<UCanvasPanelSlot>(Target->Slot) : nullptr;
		if (!CanvasSlot || CanvasSlot->GetSize().X <= 0.f || CanvasSlot->GetSize().Y <= 0.f)
		{
			return false;
		}
		Out = FBox2D(CanvasSlot->GetPosition(), CanvasSlot->GetPosition() + CanvasSlot->GetSize());
		return true;
	}

	void WriteCanvasRect(UWidget* Target, const FBox2D& Rect)
	{
		if (UCanvasPanelSlot* CanvasSlot = Target ? Cast<UCanvasPanelSlot>(Target->Slot) : nullptr)
		{
			CanvasSlot->SetPosition(Rect.Min);
			CanvasSlot->SetSize(Rect.GetSize());
		}
	}
}

// ---------------------------------------------------------------- shared slot helpers

void UOfficeTVSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	for (const FName& Role : RequiredRoles)
	{
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(GetClass(), Role);
		const UObject* Bound = Property ? Property->GetObjectPropertyValue_InContainer(this) : nullptr;
		if (!Bound)
		{
			UE_LOG(LogTemp, Error, TEXT("[OfficeTV] %s is missing required role %s. Re-run Tools/OfficeTV/author_programs.py."),
				*GetClass()->GetName(), *Role.ToString());
			ensureMsgf(false, TEXT("Office TV widget %s lacks role %s"), *GetClass()->GetName(), *Role.ToString());
		}
	}
}

UTexture2D* UOfficeTVSlotWidget::LoadArtwork(const TSoftObjectPtr<UTexture2D>& Artwork)
{
	UTexture2D* Texture = Artwork.IsNull() ? nullptr : Artwork.LoadSynchronous();
#if WITH_EDITOR
	if (Texture)
	{
		FTextureCompilingManager::Get().FinishCompilation({Texture});
	}
#endif
	return Texture;
}

void UOfficeTVSlotWidget::SetText(UCommonTextBlock* Target, const FText& Text) const
{
	if (Target)
	{
		Target->SetText(Text);
		OfficeTVWidgetDetail::SetVisible(Target, !Text.IsEmpty());
	}
}

void UOfficeTVSlotWidget::SetCover(UImage* Target, const TSoftObjectPtr<UTexture2D>& Artwork)
{
	if (!Target)
	{
		return;
	}
	UTexture2D* Texture = LoadArtwork(Artwork);
	if (Texture)
	{
		Target->SetBrushFromTexture(Texture, true);
	}
	OfficeTVWidgetDetail::SetVisible(Target, Texture != nullptr);
}

void UOfficeTVSlotWidget::SetMosaic(UImage* Target, const TSoftObjectPtr<UTexture2D>& Artwork,
	const FOfficeTVDrawContext& Context, float CellPx)
{
	if (!Target)
	{
		return;
	}
	UTexture2D* Texture = LoadArtwork(Artwork);
	if (!MosaicMID && Context.Art && Context.Art->MosaicMaterial)
	{
		if (UMaterialInterface* Material = Context.Art->MosaicMaterial)
		{
			MosaicMID = UMaterialInstanceDynamic::Create(Material, this);
			MosaicMID->SetScalarParameterValue(OfficeTVWidgetDetail::CellParameter, CellPx);
			MosaicMID->SetScalarParameterValue(OfficeTVWidgetDetail::BrightParameter, OfficeTVWidgetDetail::MosaicBrightness);
		}
	}
	if (MosaicMID && Texture)
	{
		MosaicMID->SetTextureParameterValue(OfficeTVWidgetDetail::CoverParameter, Texture);
		FSlateBrush Brush = Target->GetBrush();
		Brush.SetResourceObject(MosaicMID);
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Target->SetBrush(Brush);
	}
	OfficeTVWidgetDetail::SetVisible(Target, MosaicMID && Texture);
}

void UOfficeTVSlotWidget::SetPortrait(UImage* Target, int32 EmployeeId)
{
	if (!Target)
	{
		return;
	}
	if (EmployeeId != CachedPortraitId || !CachedPortrait)
	{
		CachedPortraitId = EmployeeId;
		CachedPortrait = EmployeeId == INDEX_NONE ? nullptr
			: UEmployeeRosterCardWidget::LoadPortraitTexture(EmployeeId, OfficeTVWidgetDetail::PortraitBudget);
		if (CachedPortrait)
		{
			Target->SetBrushFromTexture(CachedPortrait);
		}
	}
	OfficeTVWidgetDetail::SetVisible(Target, CachedPortrait != nullptr);
}

void UOfficeTVSlotWidget::SetTint(UWidget* Target, const FLinearColor& Color) const
{
	if (!Target || Color.A <= 0.f)
	{
		return;
	}
	if (UBorder* AsBorder = Cast<UBorder>(Target))
	{
		AsBorder->SetBrushColor(Color);
	}
	else if (UImage* AsImage = Cast<UImage>(Target))
	{
		AsImage->SetColorAndOpacity(Color);
	}
}

void UOfficeTVSlotWidget::SetEmblem(UBorder* Plate, UCommonTextBlock* Letter, FName Grade, const FOfficeTVDrawContext& Context)
{
	if (!Plate && !Letter)
	{
		return;
	}
	if (Grade.IsNone())
	{
		OfficeTVWidgetDetail::SetVisible(Plate, false);
		OfficeTVWidgetDetail::SetVisible(Letter, false);
		return;
	}
	bool bQualityGrade = false;
	const EQualityGrade Quality = OfficeTVWidgetDetail::GradeFromLetter(Grade, bQualityGrade);
	// Tier marks such as T2 wear the top grade's medal.
	CGQualityGradeMedal::Apply(Context.Game, Context.Industry, bQualityGrade ? Quality : EQualityGrade::S, Plate, Letter);
	if (Letter)
	{
		Letter->SetText(FText::FromName(Grade));
	}
}

void UOfficeTVSlotWidget::SetIcon(UImage* Target, int32 DisciplineSlot, const FOfficeTVDrawContext& Context)
{
	if (!Target)
	{
		return;
	}
	UTexture2D* Icon = Context.Art && Context.Art->DisciplineIcons.IsValidIndex(DisciplineSlot)
		? Context.Art->DisciplineIcons[DisciplineSlot].Get() : nullptr;
	if (Icon)
	{
		Target->SetBrushFromTexture(Icon);
	}
	OfficeTVWidgetDetail::SetVisible(Target, Icon != nullptr);
}

// ---------------------------------------------------------------- entries

void UOfficeTVEntryWidget::ShowEntry(const FOfficeTVBroadcastEntry& Entry, const FOfficeTVDrawContext& Context)
{
	SetText(TitleText, Entry.Title);
	SetText(ValueText, Entry.Value);
	SetText(CaptionText, Entry.Caption);
	if (RankText)
	{
		SetText(RankText, Entry.Slot >= 0 ? FText::AsNumber(Entry.Slot + 1) : FText::GetEmpty());
		if (RankColors.IsValidIndex(Entry.Slot))
		{
			RankText->SetColorAndOpacity(FSlateColor(RankColors[Entry.Slot]));
		}
	}

	if (Entry.bConcealed)
	{
		// Without a mosaic role the cover simply stays hidden behind the concealment art.
		if (MosaicImage)
		{
			SetMosaic(MosaicImage, Entry.Artwork, Context, MosaicCellPx);
		}
		OfficeTVWidgetDetail::SetVisible(CoverImage, false);
	}
	else
	{
		OfficeTVWidgetDetail::SetVisible(MosaicImage, false);
		SetCover(CoverImage, Entry.Artwork);
	}
	OfficeTVWidgetDetail::SetVisible(ConcealWidget, Entry.bConcealed);

	if (PortraitImage)
	{
		SetPortrait(PortraitImage, Entry.PortraitEmployeeId);
	}
	SetTint(PortraitFrame, Entry.Accent);
	SetTint(AccentPlate, Entry.Accent);
	SetEmblem(EmblemPlate, EmblemText, Entry.Grade, Context);
	if (IconImage)
	{
		SetIcon(IconImage, Entry.Slot, Context);
	}
	OfficeTVWidgetDetail::SetVisible(HighlightWidget, Entry.bHighlighted);

	if (BarBox && BarExtent > 0.f && Entry.Ratio >= 0.f)
	{
		const float Ratio = FMath::Min(Entry.Ratio, FMath::Max(BarCap, 0.f));
		const float Extent = FMath::Max(BarExtent * Ratio, 2.f);
		if (bHorizontalBar)
		{
			BarBox->SetWidthOverride(Extent);
		}
		else
		{
			BarBox->SetHeightOverride(Extent);
		}
		const FLinearColor Color = Entry.Ratio > 1.001f ? BarOverColor : Entry.Ratio >= 1.f ? BarReachedColor : BarUnderColor;
		SetTint(BarFill, Color);
	}
}

void UOfficeTVEntryWidget::SetBarGrowth(float Alpha)
{
	if (BarBox)
	{
		BarBox->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
		FWidgetTransform Transform = BarBox->GetRenderTransform();
		Transform.Scale = FVector2D(1.f, FMath::Clamp(Alpha, 0.f, 1.f));
		BarBox->SetRenderTransform(Transform);
	}
}

// ---------------------------------------------------------------- programmes

void UOfficeTVProgramWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bAuthoredLayoutCaptured)
	{
		bAuthoredLayoutCaptured = true;
		RevealFullWidth = RevealBox ? RevealBox->GetWidthOverride() : 0.f;
		if (const UCanvasPanelSlot* NeedleSlot = Needle ? Cast<UCanvasPanelSlot>(Needle->Slot) : nullptr)
		{
			NeedleBaseX = NeedleSlot->GetPosition().X;
		}
	}
}

void UOfficeTVProgramWidget::ShowFrame(const FOfficeTVBroadcastFrame& Frame, bool bNewShowing, const FOfficeTVDrawContext& Context)
{
	using namespace OfficeTVWidgetDetail;
	Program = Frame.Program;
	if (bNewShowing)
	{
		ShowStart = GetWorld() ? GetWorld()->GetAudioTimeSeconds() : 0.0;
		NeedleFrom = 0.f;
	}

	SetText(TitleText, Frame.Title);
	SetText(SubtitleText, Frame.Subtitle);
	SetText(ValueText, Frame.Value);
	SetText(SecondaryText, Frame.SecondaryValue);
	SetText(LabelText, Frame.Label);
	SetText(GradeLabelText, Frame.GradeLabel);
	SetText(PortraitBadgeText, Frame.PortraitBadge);
	if (PortraitImage)
	{
		SetPortrait(PortraitImage, Frame.PortraitEmployeeId);
	}
	SetTint(PortraitFrame, Frame.Accent);
	SetTint(AccentPlate, Frame.Accent);
	SetEmblem(EmblemPlate, EmblemText, Frame.Grade, Context);
	if (IconImage)
	{
		SetIcon(IconImage, Frame.IconSlot, Context);
	}

	// Cover, mosaic and reveal share one fitted rectangle so the revealed half lines up with the mosaic.
	UTexture2D* Cover = (CoverImage || MosaicImage || RevealImage) ? LoadArtwork(Frame.Artwork) : nullptr;
	FBox2D Authored;
	if (CoverImage)
	{
		SetCover(CoverImage, Frame.Artwork);
	}
	if (MosaicImage)
	{
		SetMosaic(MosaicImage, Frame.Artwork, Context, MosaicCellPx);
		if (RevealBox && ReadCanvasRect(RevealBox, Authored))
		{
			const FBox2D Fitted = FitInside(FBox2D(Authored.Min, Authored.Min + FVector2D(RevealFullWidth, Authored.GetSize().Y)), Cover);
			WriteCanvasRect(MosaicImage, Fitted);
			WriteCanvasRect(RevealBox, Fitted);
			RevealBox->SetHeightOverride(Fitted.GetSize().Y);
			const float Progress = FMath::Clamp(Frame.Progress, 0.f, 1.f);
			RevealBox->SetWidthOverride(Fitted.GetSize().X * Progress);
			if (RevealImage && Cover)
			{
				RevealImage->SetBrushFromTexture(Cover);
				FSlateBrush Brush = RevealImage->GetBrush();
				Brush.SetUVRegion(FBox2f(FVector2f::ZeroVector, FVector2f(Progress, 1.f)));
				RevealImage->SetBrush(Brush);
			}
			if (RevealEdge)
			{
				FBox2D EdgeRect;
				if (ReadCanvasRect(RevealEdge, EdgeRect))
				{
					const float EdgeWidth = EdgeRect.GetSize().X;
					const FVector2D EdgeMin(Fitted.Min.X + Fitted.GetSize().X * Progress - EdgeWidth * 0.5f, Fitted.Min.Y);
					WriteCanvasRect(RevealEdge, FBox2D(EdgeMin, EdgeMin + FVector2D(EdgeWidth, Fitted.GetSize().Y)));
				}
				SetVisible(RevealEdge, Progress > 0.002f && Progress < 0.998f);
			}
		}
	}

	if (Gauge)
	{
		const int32 Segments = Gauge->GetChildrenCount();
		const float Fraction = Frame.Total > 0 ? static_cast<float>(Frame.Count) / Frame.Total : FMath::Clamp(Frame.Progress, 0.f, 1.f);
		const int32 Lit = Frame.Total > 0 && Frame.Total == Segments ? Frame.Count
			: Frame.Total > 0 ? FMath::CeilToInt(Fraction * Segments - KINDA_SMALL_NUMBER)
			: FMath::FloorToInt(Fraction * Segments + KINDA_SMALL_NUMBER);
		const bool bWarning = GaugeWarningCount > 0 && Frame.Total > 0 && Frame.Count <= GaugeWarningCount;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			SetTint(Gauge->GetChildAt(Index), Index < Lit ? (bWarning ? GaugeWarningColor : GaugeOnColor) : GaugeOffColor);
		}
	}

	if (Entries)
	{
		const int32 Count = Entries->GetChildrenCount();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UOfficeTVEntryWidget* Entry = Cast<UOfficeTVEntryWidget>(Entries->GetChildAt(Index));
			if (!Entry)
			{
				continue;
			}
			const bool bShown = Frame.Entries.IsValidIndex(Index);
			if (bShown)
			{
				Entry->ShowEntry(Frame.Entries[Index], Context);
			}
			SetVisible(Entry, bShown);
		}
	}

	if (Needle)
	{
		NeedleTarget = NeedleTravel * FMath::Clamp(Frame.Progress, 0.f, 1.f);
		if (!bNewShowing)
		{
			NeedleFrom = NeedleTarget;
		}
	}
	if (ProgressBox && ProgressExtent > 0.f)
	{
		ProgressBox->SetWidthOverride(FMath::Max(2.f, ProgressExtent * FMath::Clamp(Frame.Progress, 0.f, 1.f)));
	}
	ApplyMotion(GetWorld() ? static_cast<float>(GetWorld()->GetAudioTimeSeconds() - ShowStart) : 0.f);
}

void UOfficeTVProgramWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (GetWorld())
	{
		ApplyMotion(static_cast<float>(GetWorld()->GetAudioTimeSeconds() - ShowStart));
	}
}

void UOfficeTVProgramWidget::SetMotionElapsed(float Seconds)
{
	if (const UWorld* CurrentWorld = GetWorld())
	{
		ShowStart = CurrentWorld->GetAudioTimeSeconds() - Seconds;
		ApplyMotion(Seconds);
	}
}

bool UOfficeTVProgramWidget::IsAnimating() const
{
	using namespace OfficeTVWidgetDetail;
	const double Now = GetWorld() ? GetWorld()->GetAudioTimeSeconds() : 0.0;
	const float Elapsed = static_cast<float>(Now - ShowStart);
	switch (Program)
	{
	case EOfficeTVProgram::LaunchCelebration:
	case EOfficeTVProgram::TierUp:
		return Elapsed < StampDelay + StampFall + StampSettle + 0.3f;
	case EOfficeTVProgram::CriticalFlash:
		return Elapsed < SlashTime + 0.2f;
	case EOfficeTVProgram::WorkFeed:
	case EOfficeTVProgram::Reactions:
		return Elapsed < SlideTime;
	case EOfficeTVProgram::CriticScores:
		return Elapsed < CardFlipStep * (Entries ? Entries->GetChildrenCount() : 0) + CardFlipTime;
	case EOfficeTVProgram::DevRace:
		return Elapsed < BarGrowTime;
	case EOfficeTVProgram::DevForecast:
		return Elapsed < NeedleTime;
	case EOfficeTVProgram::Credits:
	case EOfficeTVProgram::Fever:
	case EOfficeTVProgram::ProductionLine:
		return true;
	default:
		return false;
	}
}

void UOfficeTVProgramWidget::ApplyMotion(float Elapsed)
{
	using namespace OfficeTVWidgetDetail;
	if (EmblemRoot && (Program == EOfficeTVProgram::LaunchCelebration || Program == EOfficeTVProgram::TierUp))
	{
		// Drop from 2.7x outside the cover, accelerating; land with a short squash. No rotation.
		float Scale = 1.f;
		float Opacity = 1.f;
		if (Elapsed < StampDelay)
		{
			Opacity = 0.f;
			Scale = StampFrom;
		}
		else if (Elapsed < StampDelay + StampFall)
		{
			Scale = FMath::Lerp(StampFrom, 1.f, EaseIn((Elapsed - StampDelay) / StampFall));
		}
		else if (Elapsed < StampDelay + StampFall + StampSettle)
		{
			Scale = 1.f - 0.06f * FMath::Sin(PI * (Elapsed - StampDelay - StampFall) / StampSettle);
		}
		SetTransform(EmblemRoot, FVector2D::ZeroVector, FVector2D(Scale), Opacity);
		const float Impact = Elapsed - StampDelay - StampFall;
		if (CoverRoot)
		{
			const float Jolt = Impact >= 0.f && Impact < 0.2f ? 7.f * FMath::Sin(PI * Impact / 0.2f) * (1.f - Impact / 0.2f) : 0.f;
			SetTransform(CoverRoot, FVector2D(0.f, Jolt), FVector2D(1.f), 1.f);
		}
		if (FlashImage)
		{
			FlashImage->SetRenderOpacity(Impact >= 0.f && Impact < 0.3f ? 0.95f * (1.f - Impact / 0.3f) : 0.f);
		}
	}

	if (Program == EOfficeTVProgram::CriticalFlash)
	{
		const float SlideA = EaseOut(Elapsed / SlashTime);
		const float SlideB = EaseOut((Elapsed - 0.08f) / SlashTime);
		SetTransform(SlashA, FVector2D(-1100.f * (1.f - SlideA), 0.f), FVector2D(1.f), 1.f);
		SetTransform(SlashB, FVector2D(-1100.f * (1.f - SlideB), 0.f), FVector2D(1.f), 1.f);
		const float Pop = EaseOut((Elapsed - 0.1f) / 0.25f);
		SetTransform(PopWidget, FVector2D::ZeroVector, FVector2D(FMath::Lerp(0.94f, 1.f, Pop)), Pop);
	}

	if (Entries && (Program == EOfficeTVProgram::WorkFeed || Program == EOfficeTVProgram::Reactions)
		&& Entries->GetChildrenCount() > 0)
	{
		const float Slide = EaseOut(Elapsed / SlideTime);
		SetTransform(Entries->GetChildAt(0), FVector2D(60.f * (1.f - Slide), 0.f), FVector2D(1.f), Slide);
	}

	if (Entries && Program == EOfficeTVProgram::CriticScores)
	{
		for (int32 Index = 0; Index < Entries->GetChildrenCount(); ++Index)
		{
			const float Flip = EaseOut((Elapsed - CardFlipStep * Index) / CardFlipTime);
			SetTransform(Entries->GetChildAt(Index), FVector2D::ZeroVector, FVector2D(FMath::Max(0.08f, Flip), 1.f), Flip > 0.f ? 1.f : 0.f);
		}
	}

	if (Entries && Program == EOfficeTVProgram::DevRace)
	{
		const float Grow = FMath::Max(0.04f, EaseOut(Elapsed / BarGrowTime));
		for (int32 Index = 0; Index < Entries->GetChildrenCount(); ++Index)
		{
			if (UOfficeTVEntryWidget* Bar = Cast<UOfficeTVEntryWidget>(Entries->GetChildAt(Index)))
			{
				Bar->SetBarGrowth(Grow);
			}
		}
	}

	if (Needle)
	{
		const float Travel = FMath::Lerp(NeedleFrom, NeedleTarget, EaseOut(Elapsed / NeedleTime));
		SetTransform(Needle, FVector2D(Travel, 0.f), FVector2D(1.f), 1.f);
	}

	if (Scroller && ScrollSpeed > 0.f)
	{
		// The roll stops once its last line reaches the bottom of the clip, however many people are credited.
		const UWidget* Clip = Scroller->GetParent();
		const float ClipHeight = Clip ? Clip->GetCachedGeometry().GetLocalSize().Y : 0.f;
		const float Overflow = ClipHeight > 0.f ? Scroller->GetDesiredSize().Y - ClipHeight : ScrollLimit;
		const float Travel = FMath::Min(FMath::Clamp(Overflow, 0.f, ScrollLimit), ScrollSpeed * FMath::Max(0.f, Elapsed - ScrollDelay));
		SetTransform(Scroller, FVector2D(0.f, -Travel), FVector2D(1.f), 1.f);
	}

	if (LoopLayer && LoopSpeed != 0.f && LoopSpan > 0.f)
	{
		SetTransform(LoopLayer, FVector2D(-FMath::Fmod(Elapsed * LoopSpeed, LoopSpan), 0.f), FVector2D(1.f), 1.f);
	}
}
