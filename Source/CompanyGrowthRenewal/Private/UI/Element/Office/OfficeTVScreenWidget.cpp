#include "UI/Element/Office/OfficeTVScreenWidget.h"

#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Core/CGGameInstance.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"

#define LOCTEXT_NAMESPACE "OfficeTVScreen"

void UOfficeTVScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!ProgramHost)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeTV] Screen WBP has no ProgramHost. Re-run Tools/OfficeTV/author_programs.py."));
	}
	ApplyIdentity();
	for (const TPair<EOfficeTVProgram, TObjectPtr<UOfficeTVProgramWidget>>& Pair : Instances)
	{
		if (Pair.Value && Pair.Value != Current)
		{
			Pair.Value->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UOfficeTVScreenWidget::ApplyIdentity()
{
	UCGGameInstance* Game = Cast<UCGGameInstance>(GetGameInstance());
	ApplyIndustry(Game ? Game->GetCurrentBuildingCompanyType() : ECompanyType::None);
	if (ChannelText)
	{
		// 사무실 이름판과 같은 규칙
		ChannelText->SetText(Game ? Game->GetCompanyDisplayName(Game->GetCurrentManagedBuildingIndex()) : FText::GetEmpty());
	}
}

void UOfficeTVScreenWidget::ApplyIndustry(ECompanyType InIndustry)
{
	Industry = InIndustry;
	const UGameInstance* Game = GetGameInstance();
	const UTableManagerSubsystem* Tables = Game ? Game->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	bool bFound = false;
	const FCompanyInfoTable Info = Tables && Industry != ECompanyType::None ? Tables->GetCompanyInfo(Industry, bFound) : FCompanyInfoTable();
	if (!bFound)
	{
		Accent = BugMark ? BugMark->GetColorAndOpacity() : Accent;
		return;
	}
	Accent = Info.AccentColor;
	for (UImage* Target : {AccentGlow.Get(), BugMark.Get()})
	{
		if (Target)
		{
			const float Alpha = Target->GetColorAndOpacity().A;
			Target->SetColorAndOpacity(FLinearColor(Accent.R, Accent.G, Accent.B, Alpha));
		}
	}
}

bool UOfficeTVScreenWidget::SupportsProgram(EOfficeTVProgram Program) const
{
	const TSubclassOf<UOfficeTVProgramWidget>* Class = ProgramWidgets.Find(Program);
	return ProgramHost && Class && *Class;
}

UOfficeTVProgramWidget* UOfficeTVScreenWidget::ResolveProgram(EOfficeTVProgram Program)
{
	if (TObjectPtr<UOfficeTVProgramWidget>* Existing = Instances.Find(Program))
	{
		return *Existing;
	}
	const TSubclassOf<UOfficeTVProgramWidget>* Class = ProgramWidgets.Find(Program);
	if (!ProgramHost || !Class || !*Class)
	{
		return nullptr;
	}
	UOfficeTVProgramWidget* Created = CreateWidget<UOfficeTVProgramWidget>(this, *Class);
	if (!Created)
	{
		return nullptr;
	}
	if (UCanvasPanelSlot* CanvasSlot = ProgramHost->AddChildToCanvas(Created))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
	}
	Created->SetVisibility(ESlateVisibility::Collapsed);
	Instances.Add(Program, Created);
	return Created;
}

void UOfficeTVScreenWidget::SetBroadcast(const FOfficeTVBroadcastFrame& Frame, uint32 ShowSerial)
{
	UOfficeTVProgramWidget* Target = ResolveProgram(Frame.Program);
	const bool bNewShowing = !bHasShown || ShowSerial != LastSerial || Target != Current;
	bHasShown = true;
	LastSerial = ShowSerial;
	if (Current && Current != Target)
	{
		Current->SetVisibility(ESlateVisibility::Collapsed);
	}
	Current = Target;
	if (!Target)
	{
		if (TagRoot) { TagRoot->SetVisibility(ESlateVisibility::Collapsed); }
		if (HeadlineRoot) { HeadlineRoot->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}
	Target->SetVisibility(ESlateVisibility::HitTestInvisible);
	FOfficeTVDrawContext Context;
	Context.Industry = Industry;
	Context.Art = &Art;
	Context.Game = GetGameInstance();
	FOfficeTVBroadcastFrame DisplayFrame = Frame;
	if (IsManufacturingType(Industry)
		&& (Frame.Program == EOfficeTVProgram::Advertisement || Frame.Program == EOfficeTVProgram::Upcoming))
	{
		// 제조업 제품에는 장르가 없으므로 NAME_None을 보조 라벨로 표시하지 않는다.
		DisplayFrame.Label = FText::GetEmpty();
	}
	Target->ShowFrame(DisplayFrame, bNewShowing, Context);

	if (TagRoot)
	{
		TagRoot->SetVisibility(Target->TagText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (TagLabel)
	{
		TagLabel->SetText(Target->TagText);
		TagLabel->SetColorAndOpacity(Target->TagTone == EOfficeTVTagTone::Gold ? GoldTagInk : TagInk);
	}
	if (TagPlate)
	{
		TagPlate->SetColorAndOpacity(Target->TagTone == EOfficeTVTagTone::Alert ? AlertTagColor
			: Target->TagTone == EOfficeTVTagTone::Gold ? GoldTagColor : Accent);
	}
	if (HeadlineRoot)
	{
		HeadlineRoot->SetVisibility(Frame.Headline.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (HeadlineText)
	{
		HeadlineText->SetText(Frame.Headline);
	}
	if (HeadlineValueText)
	{
		HeadlineValueText->SetText(Frame.HeadlineValue);
		HeadlineValueText->SetVisibility(Frame.HeadlineValue.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

bool UOfficeTVScreenWidget::IsAnimating() const
{
	return Current && Current->IsAnimating();
}

void UOfficeTVScreenWidget::SettleMotionForCapture(float Seconds)
{
	if (Current)
	{
		Current->SetMotionElapsed(Seconds);
	}
}

#undef LOCTEXT_NAMESPACE
