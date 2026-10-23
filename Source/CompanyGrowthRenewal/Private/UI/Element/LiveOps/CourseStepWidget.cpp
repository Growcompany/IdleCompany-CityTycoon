#include "UI/Element/LiveOps/CourseStepWidget.h"

#include "CommonTextBlock.h"
#include "Components/WidgetSwitcher.h"

void UCourseStepWidget::SetCourse(int32 Course, const FText& TopicName, const FText& RequirementLabel,
	ECourseStepState State)
{
	const int32 StateIndex = static_cast<int32>(State);
	if (PlateSwitcher) PlateSwitcher->SetActiveWidgetIndex(StateIndex);
	if (BadgeSwitcher) BadgeSwitcher->SetActiveWidgetIndex(StateIndex);

	if (CourseNumberText) CourseNumberText->SetText(FText::AsNumber(Course));

	if (NameText)
	{
		NameText->SetText(TopicName);
		NameText->SetColorAndOpacity(State == ECourseStepState::Current ? NameInkCurrent
			: State == ECourseStepState::Done ? NameInkDone : NameInkNext);
	}

	if (RequirementText)
	{
		// 밴드 밖이면 숫자를 지어내지 않는다 — 줄째 접는다
		RequirementText->SetText(RequirementLabel);
		RequirementText->SetVisibility(RequirementLabel.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		RequirementText->SetColorAndOpacity(State == ECourseStepState::Current ? RequirementInkCurrent
			: State == ECourseStepState::Done ? RequirementInkDone : RequirementInkNext);
	}
}
