#include "UI/Element/GoalTrackerRowWidget.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Buttons/ButtonWidget.h"

void UGoalTrackerRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RowButton) { RowButton->OnClicked.AddDynamic(this, &UGoalTrackerRowWidget::HandleRowClicked); }
	if (ActionButton) { ActionButton->OnClicked().AddUObject(this, &UGoalTrackerRowWidget::HandleActionClicked); }
	if (GuideDismissButton) { GuideDismissButton->OnClicked().AddUObject(this, &UGoalTrackerRowWidget::HandleDismissClicked); }
}

void UGoalTrackerRowWidget::NativeDestruct()
{
	if (RowButton) { RowButton->OnClicked.RemoveDynamic(this, &UGoalTrackerRowWidget::HandleRowClicked); }
	if (ActionButton) { ActionButton->OnClicked().RemoveAll(this); }
	if (GuideDismissButton) { GuideDismissButton->OnClicked().RemoveAll(this); }
	Super::NativeDestruct();
}

void UGoalTrackerRowWidget::Configure(const FGoalBoardEntry& InEntry, bool bInTracked, bool bInExpanded, const FText& InPlaceHint)
{
	Entry = InEntry;
	const bool bReadyToClaim = Entry.State == EGoalState::ReadyToClaim;
	bTracked = bInTracked && Entry.State == EGoalState::InProgress;
	if (TitleText) { TitleText->SetText(Entry.Row.Title); }
	if (ExpandRoot) { ExpandRoot->SetVisibility(bInExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (DescText)
	{
		DescText->SetText(Entry.bPracticed ? FText::GetEmpty() : Entry.Row.Desc);
		DescText->SetVisibility(Entry.bPracticed ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	FText TagLabel;
	FLinearColor TagInk(1.f, 1.f, 1.f, 0.62f);
	FLinearColor TagFill(1.f, 1.f, 1.f, 0.10f);
	if (bReadyToClaim)
	{
		TagLabel = NSLOCTEXT("GoalTracker", "TagClaimReady", "보상 받기");
		TagInk = FLinearColor(0.57758f, 0.87059f, 0.46905f, 1.f);
		TagFill = FLinearColor(0.05613f, 0.58412f, 0.11191f, 0.22f);
	}
	else if (bTracked && !InPlaceHint.IsEmpty())
	{
		TagLabel = InPlaceHint;
	}
	else if (bTracked)
	{
		TagLabel = NSLOCTEXT("GoalTracker", "TagTracking", "안내 중");
		TagInk = FLinearColor(0.520946f, 0.745394f, 0.930117f, 1.f);
		TagFill = FLinearColor(0.046650f, 0.327780f, 0.745440f, 0.22f);
	}
	const ESlateVisibility TagVisibility = TagLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	if (StateTagText)
	{
		StateTagText->SetText(TagLabel);
		StateTagText->SetColorAndOpacity(FSlateColor(TagInk));
		StateTagText->SetVisibility(TagVisibility);
	}
	if (StateTagPlate)
	{
		StateTagPlate->SetBrushColor(TagFill);
		StateTagPlate->SetVisibility(TagVisibility);
	}

	if (ActionButton)
	{
		const bool bShowAction = bReadyToClaim ||
			(Entry.State == EGoalState::InProgress && !Entry.Row.StepLines.IsEmpty());
		ActionButton->SetVisibility(bShowAction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bShowAction)
		{
			ActionButton->SetButtonText(bReadyToClaim
				? NSLOCTEXT("GoalTracker", "BtnClaim", "보상 받기")
				: bTracked ? NSLOCTEXT("GoalTracker", "BtnStopGuide", "안내 끄기")
					: NSLOCTEXT("GoalTracker", "BtnStart", "안내하기"));
			if (UClass* StyleClass = (bReadyToClaim ? ActionStyle_Claim : bTracked ? ActionStyle_Stop : ActionStyle_Start).LoadSynchronous())
			{
				ActionButton->SetStyle(TSubclassOf<UCommonButtonStyle>(StyleClass));
			}
			ActionButton->SetTextOutlineEnabled(false);
		}
	}
	if (GuideDismissButton)
	{
		GuideDismissButton->SetVisibility(bTracked || bReadyToClaim ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		GuideDismissButton->SetButtonText(NSLOCTEXT("GoalTracker", "DismissHint", "나중에"));
		if (UClass* StyleClass = ActionStyle_Stop.LoadSynchronous())
		{
			GuideDismissButton->SetStyle(TSubclassOf<UCommonButtonStyle>(StyleClass));
		}
		GuideDismissButton->SetTextOutlineEnabled(false);
	}

	const bool bHasProgress = Entry.ProgressTarget > 0;
	const int64 DisplayCurrent = bReadyToClaim
		? Entry.ProgressTarget : FMath::Clamp(Entry.ProgressCurrent, int64(0), Entry.ProgressTarget);
	const FText ProgressValue = bHasProgress
		? FText::Format(NSLOCTEXT("GoalTracker", "ProgressValue", "{0} / {1}"),
			FText::AsNumber(DisplayCurrent), FText::AsNumber(Entry.ProgressTarget)) : FText::GetEmpty();
	if (ProgressShortText)
	{
		ProgressShortText->SetText(ProgressValue);
		ProgressShortText->SetVisibility(bHasProgress ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ProgressLongText)
	{
		ProgressLongText->SetText(ProgressValue);
		ProgressLongText->SetVisibility(bHasProgress ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (BarBox) { BarBox->SetVisibility(bHasProgress ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
	if (ProgressBar && bHasProgress)
	{
		ProgressBar->SetPercent(FMath::Clamp(static_cast<float>(DisplayCurrent) /
			static_cast<float>(Entry.ProgressTarget), 0.f, 1.f));
	}
	const bool bHasReward = Entry.Row.RewardMoney > 0;
	if (RewardRow) { RewardRow->SetVisibility(bHasReward ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
	if (RewardSampleValue) { RewardSampleValue->SetText(FText::AsNumber(Entry.Row.RewardMoney)); }

	const FLinearColor Accent = bReadyToClaim
		? FLinearColor(0.056130f, 0.584120f, 0.111910f, 1.f)
		: bTracked ? FLinearColor(0.046650f, 0.327780f, 0.745440f, 1.f)
			: FLinearColor(1.f, 0.723055f, 0.194618f, 0.95f);
	if (UImage* CardLine = Cast<UImage>(GetWidgetFromName(TEXT("CardLine"))))
	{
		if (UMaterialInstanceDynamic* LineMaterial = CardLine->GetDynamicMaterial())
		{
			LineMaterial->SetVectorParameterValue(TEXT("AccentColor"), Accent);
		}
	}
	if (StateDot) { StateDot->SetColorAndOpacity(Accent); }
}

void UGoalTrackerRowWidget::HandleRowClicked()
{
	OnRowTapped.Broadcast(Entry.GoalID);
}

void UGoalTrackerRowWidget::HandleActionClicked()
{
	if (Entry.State == EGoalState::ReadyToClaim)
	{
		OnClaimRequested.Broadcast(Entry.GoalID, Entry.ClaimCount);
		return;
	}
	if (bTracked) { OnStopRequested.Broadcast(Entry.GoalID); }
	else { OnStartRequested.Broadcast(Entry.GoalID); }
}

void UGoalTrackerRowWidget::HandleDismissClicked()
{
	OnDismissRequested.Broadcast(Entry.GoalID);
}
