#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/OfficeTVBroadcast.h"
#include "UI/Element/Office/OfficeTVProgramWidget.h"
#include "OfficeTVScreenWidget.generated.h"

class UCanvasPanel;
class UCommonTextBlock;
class UImage;
class UWidget;

/**
 * Shared broadcast chrome (background, channel bug, programme tag, lower third) around one programme widget.
 * Programme widgets are created on first use from ProgramWidgets and kept for the visit.
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UOfficeTVScreenWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	TMap<EOfficeTVProgram, TSubclassOf<UOfficeTVProgramWidget>> ProgramWidgets;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV")
	FOfficeTVArt Art;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Tag")
	FLinearColor AlertTagColor = FLinearColor(0.79f, 0.06f, 0.07f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Tag")
	FLinearColor GoldTagColor = FLinearColor(0.93f, 0.6f, 0.07f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Tag")
	FLinearColor TagInk = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Office TV|Tag")
	FLinearColor GoldTagInk = FLinearColor(0.02f, 0.012f, 0.f, 1.f);

	/** A new showing replays the programme's opening motion; the same serial only refreshes values. */
	void SetBroadcast(const FOfficeTVBroadcastFrame& Frame, uint32 ShowSerial);

	/** Only programmes this screen can draw enter the schedule. */
	bool SupportsProgram(EOfficeTVProgram Program) const;

	bool IsAnimating() const;

	/** Tints the channel chrome and resolves grade medals for this industry. None keeps the authored colours. */
	void ApplyIndustry(ECompanyType InIndustry);

	/** Fixture capture: places the current programme's motion at a fixed time. */
	void SettleMotionForCapture(float Seconds);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> ProgramHost;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ChannelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> TagRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> TagPlate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> TagLabel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> HeadlineRoot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> HeadlineText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> HeadlineValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> AccentGlow;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> BugMark;

private:
	UPROPERTY(Transient)
	TMap<EOfficeTVProgram, TObjectPtr<UOfficeTVProgramWidget>> Instances;

	UPROPERTY(Transient)
	TObjectPtr<UOfficeTVProgramWidget> Current;

	uint32 LastSerial = 0;
	bool bHasShown = false;
	ECompanyType Industry = ECompanyType::None;
	FLinearColor Accent = FLinearColor::White;

	void ApplyIdentity();
	UOfficeTVProgramWidget* ResolveProgram(EOfficeTVProgram Program);
};
