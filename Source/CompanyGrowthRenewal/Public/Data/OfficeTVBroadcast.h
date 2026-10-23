#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "OfficeTVBroadcast.generated.h"

/** The first six values keep their v3 meaning and order; the screen widget's panel table depends on it. */
UENUM(BlueprintType)
enum class EOfficeTVProgram : uint8
{
	Advertisement,
	Development,
	EmployeeHighlight,
	Operations,
	CompanyNews,
	LaunchCelebration,
	DevRace,
	DevForecast,
	CriticalFlash,
	Fever,
	IssueResult,
	LaunchCountdown,
	Crew,
	WorkFeed,
	Mentoring,
	Promotion,
	NewHire,
	Anniversary,
	CriticScores,
	Reactions,
	FirstDiscovery,
	Credits,
	RevenueTicker,
	HotTime,
	TierCodex,
	TierUp,
	HallOfFame,
	TopRevenue,
	Upcoming,
	StaffGap,
	ProductionLine
};

/** Which rotation the current office state draws from. */
UENUM(BlueprintType)
enum class EOfficeTVPool : uint8
{
	None,
	Idle,
	Developing,
	Operating,
	Producing
};

USTRUCT(BlueprintType)
struct FOfficeTVBroadcastEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Caption;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	TSoftObjectPtr<UTexture2D> Artwork;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	int32 PortraitEmployeeId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FName Grade;

	/** Bar fill or reveal amount; 1 means the target was reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	float Ratio = -1.f;

	/** Discipline slot for icons, or rank for ordered lists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	int32 Slot = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	bool bHighlighted = false;

	/** Rarity or genre colour; transparent means none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FLinearColor Accent = FLinearColor::Transparent;

	/** Unreleased items are shown as a mosaic, never as their clear cover. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	bool bConcealed = false;
};

USTRUCT(BlueprintType)
struct FOfficeTVBroadcastFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	EOfficeTVProgram Program = EOfficeTVProgram::CompanyNews;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Value;

	/** Lower-third line. Empty hides the band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Headline;

	/** Right-aligned figure inside the lower third, such as 2/4 or a count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText HeadlineValue;

	/** Second figure beside the hero value, such as a peak, a clock or a duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText SecondaryValue;

	/** Small data caption, such as a genre. Fixed captions belong to the programme widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText Label;

	/** Level badge drawn on the portrait card. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText PortraitBadge;

	/** Discipline slot for the programme's icon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	int32 IconSlot = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	TSoftObjectPtr<UTexture2D> Artwork;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	int32 PortraitEmployeeId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	float Progress = -1.f;

	/** Quality grade letter (S/A/B/C/D/F) or a tier mark such as T2; NAME_None hides the emblem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FName Grade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FText GradeLabel;

	/** Data colour such as a genre colour; transparent means none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	FLinearColor Accent = FLinearColor::Transparent;

	/** Counts such as tier clears or produced units. Total 0 hides the pair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	int32 Count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	int32 Total = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office TV")
	TArray<FOfficeTVBroadcastEntry> Entries;
};

namespace OfficeTVProgram
{
	/** Visible seconds one showing lasts. */
	COMPANYGROWTHRENEWAL_API float GetDuration(EOfficeTVProgram Program);

	/** Readouts refresh during a showing; stories keep the frame they started with. */
	COMPANYGROWTHRENEWAL_API bool IsLiveReadout(EOfficeTVProgram Program);

	/** Interrupts older than this many visible seconds are no longer news. */
	COMPANYGROWTHRENEWAL_API float GetInterruptLifetime(EOfficeTVProgram Program);

	/** Visible seconds since the last interrupt ended before this one may cut in. */
	COMPANYGROWTHRENEWAL_API float GetInterruptGap(EOfficeTVProgram Program);
}
