#include "UI/Element/SideContent/BusinessTripCrewCardWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"

void UBusinessTripCrewCardWidget::SetContextScore(const FText& InDisciplineName, int32 InScore)
{
	if (!ContextScoreText) return;

	if (InScore < 0)
	{
		// 카드를 고르기 전에는 어느 직능을 보여줄지 정해지지 않았다 — 숨기면 줄이 접혀 카드 높이가 흔들린다
		ContextScoreText->SetText(FText::FromString(TEXT("―")));
		return;
	}
	ContextScoreText->SetText(InDisciplineName.IsEmpty()
		? FText::AsNumber(InScore)
		: FText::Format(NSLOCTEXT("BusinessTrip", "CrewContextScore", "{0} {1}"),
			InDisciplineName, FText::AsNumber(InScore)));
}

void UBusinessTripCrewCardWidget::SetBuildingTeam(bool bInBuildingTeam)
{
	if (RoleRing)
	{
		RoleRing->SetColorAndOpacity(bInBuildingTeam ? BuildingTeamRingColor : BenchRingColor);
	}
}
