#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/OfficeTVBroadcast.h"
#include "Enum/CompanyType.h"
#include "OfficeTVProgramWidget.generated.h"

class UBorder;
class UGameInstance;
class UCommonTextBlock;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPanelWidget;
class USizeBox;
class UTexture2D;
class UWidget;

UENUM(BlueprintType)
enum class EOfficeTVTagTone : uint8
{
	Normal,
	Alert,
	Gold
};

/** Textures and materials the programme and entry widgets share. Authored on the screen widget. */
USTRUCT(BlueprintType)
struct FOfficeTVArt
{
	GENERATED_BODY()

	/** Discipline icons in slot order (plan, dev, art, sound, server, QA). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Office TV")
	TArray<TObjectPtr<UTexture2D>> DisciplineIcons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Office TV")
	TObjectPtr<UMaterialInterface> MosaicMaterial;
};

/** What a widget needs to draw one frame. */
struct FOfficeTVDrawContext
{
	ECompanyType Industry = ECompanyType::None;
	const FOfficeTVArt* Art = nullptr;
	UGameInstance* Game = nullptr;
};

/**
 * Role-named widgets filled from broadcast data. A role the WBP does not contain is simply not drawn;
 * roles listed in RequiredRoles must exist or construction logs an error.
 */
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API UOfficeTVSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	TArray<FName> RequiredRoles;

protected:
	virtual void NativeConstruct() override;

	void SetText(UCommonTextBlock* Target, const FText& Text) const;
	void SetCover(UImage* Target, const TSoftObjectPtr<UTexture2D>& Artwork);
	void SetMosaic(UImage* Target, const TSoftObjectPtr<UTexture2D>& Artwork, const FOfficeTVDrawContext& Context, float CellPx);
	void SetPortrait(UImage* Target, int32 EmployeeId);
	void SetTint(UWidget* Target, const FLinearColor& Color) const;
	void SetEmblem(UBorder* Plate, UCommonTextBlock* Letter, FName Grade, const FOfficeTVDrawContext& Context);
	void SetIcon(UImage* Target, int32 DisciplineSlot, const FOfficeTVDrawContext& Context);

	static UTexture2D* LoadArtwork(const TSoftObjectPtr<UTexture2D>& Artwork);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MosaicMID;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CachedPortrait;

	int32 CachedPortraitId = INDEX_NONE;
};

/** One item of a programme list: a crew card, a feed row, a critic card, a codex slot and so on. */
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API UOfficeTVEntryWidget : public UOfficeTVSlotWidget
{
	GENERATED_BODY()

public:
	void ShowEntry(const FOfficeTVBroadcastEntry& Entry, const FOfficeTVDrawContext& Context);

	/** Grows the bar from its foot; 1 is the full value. Other roles stay still. */
	void SetBarGrowth(float Alpha);

	/** Bars scale BarBox height (or width when bHorizontalBar) to Ratio; Ratio above BarCap is clamped. */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	float BarExtent = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	float BarCap = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	bool bHorizontalBar = false;

	/** Bar colours below, at and above the target (Ratio 1). */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	FLinearColor BarUnderColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	FLinearColor BarReachedColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	FLinearColor BarOverColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	float MosaicCellPx = 10.f;

	/** Rank ink by position (gold, silver, bronze). */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	TArray<FLinearColor> RankColors;

protected:
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> CaptionText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> RankText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CoverImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> MosaicImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> PortraitImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PortraitFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> EmblemPlate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> EmblemText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> IconImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> AccentPlate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> HighlightWidget;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ConcealWidget;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> BarBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> BarFill;
};

/** A whole programme on the TV. One WBP per programme, all parented to this class. */
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API UOfficeTVProgramWidget : public UOfficeTVSlotWidget
{
	GENERATED_BODY()

public:
	/** bNewShowing replays the opening motion; refreshes of the same showing only update values. */
	void ShowFrame(const FOfficeTVBroadcastFrame& Frame, bool bNewShowing, const FOfficeTVDrawContext& Context);

	/** True while a motion needs frame-rate redraws. */
	bool IsAnimating() const;

	/** Fixture capture: shows the opening motion as it looks Seconds after the showing began. */
	void SetMotionElapsed(float Seconds);

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Chrome")
	FText TagText;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Chrome")
	EOfficeTVTagTone TagTone = EOfficeTVTagTone::Normal;

	/** Segment colours for the gauge role. */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Gauge")
	FLinearColor GaugeOnColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Gauge")
	FLinearColor GaugeOffColor = FLinearColor::Black;

	/** Gauge colour once the count falls to GaugeWarningCount or below (countdown). 0 disables. */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Gauge")
	int32 GaugeWarningCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Gauge")
	FLinearColor GaugeWarningColor = FLinearColor::Yellow;

	/** Needle travel in canvas units from its authored position at Progress 0. */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Meter")
	float NeedleTravel = 0.f;

	/** ProgressBox width at Progress 1. */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Meter")
	float ProgressExtent = 0.f;

	/** Credits scroll speed in units per second after ScrollDelay; travel ends at the content's end, at most ScrollLimit. */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Motion")
	float ScrollSpeed = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Motion")
	float ScrollDelay = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Motion")
	float ScrollLimit = 0.f;

	/** Continuous drift for stripes or conveyors in units per second; wraps every LoopSpan. */
	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Motion")
	float LoopSpeed = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Motion")
	float LoopSpan = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Motion")
	float MosaicCellPx = 16.f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> SubtitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> SecondaryText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> LabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> GradeLabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> PortraitBadgeText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CoverImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> MosaicImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> RevealBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> RevealImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RevealEdge;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> PortraitImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PortraitFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> EmblemPlate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> EmblemText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> EmblemRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> CoverRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> FlashImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> IconImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> AccentPlate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Gauge;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> Entries;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Needle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> ProgressBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Scroller;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> LoopLayer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SlashA;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SlashB;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PopWidget;

private:
	void ApplyMotion(float Elapsed);
	double ShowStart = 0.0;
	float RevealFullWidth = 0.f;
	float NeedleBaseX = 0.f;
	float NeedleTarget = 0.f;
	float NeedleFrom = 0.f;
	bool bAuthoredLayoutCaptured = false;
	EOfficeTVProgram Program = EOfficeTVProgram::CompanyNews;
};
