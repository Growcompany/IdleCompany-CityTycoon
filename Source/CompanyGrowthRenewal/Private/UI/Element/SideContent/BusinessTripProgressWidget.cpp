#include "UI/Element/SideContent/BusinessTripProgressWidget.h"

#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Table/SideDungeonLootTable.h"
#include "UI/Element/SideContent/BusinessTripCardWidget.h"

namespace
{
	// unity 빌드가 익명 namespace 를 한 블롭으로 합치므로 이름을 모듈 고유로 둔다
	constexpr float TripCrewPortraitSize = 132.f;

	UVerticalBox* BuildTripCrewCell(UWidgetTree& Tree, const FBusinessTripCrewView& Crew, const UCommonTextBlock* NameSample)
	{
		UVerticalBox* Cell = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		USizeBox* PortraitBox = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PortraitBox->SetWidthOverride(TripCrewPortraitSize * 0.75f);
		PortraitBox->SetHeightOverride(TripCrewPortraitSize);

		UImage* Portrait = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		if (Crew.Portrait) Portrait->SetBrushFromTexture(Crew.Portrait, /*bMatchSize=*/false);
		PortraitBox->AddChild(Portrait);
		Cell->AddChildToVerticalBox(PortraitBox)->SetHorizontalAlignment(HAlign_Center);

		// 폰트/색은 디자인타임 샘플에서 복사한다 — 코드가 폰트를 쥐지 않는다(타이포 §7)
		UCommonTextBlock* Name = Tree.ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
		Name->SetText(FText::FromString(Crew.Name));
		if (NameSample)
		{
			Name->SetFont(NameSample->GetFont());
			Name->SetColorAndOpacity(NameSample->GetColorAndOpacity());
		}
		Name->SetJustification(ETextJustify::Center);
		Cell->AddChildToVerticalBox(Name);

		UCommonTextBlock* Origin = Tree.ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
		Origin->SetText(Crew.bBuildingTeam
			? NSLOCTEXT("BusinessTrip", "FromTeam", "배치 팀")
			: NSLOCTEXT("BusinessTrip", "FromBench", "벤치"));
		if (NameSample)
		{
			Origin->SetFont(NameSample->GetFont());
			Origin->SetColorAndOpacity(NameSample->GetColorAndOpacity());
		}
		Origin->SetJustification(ETextJustify::Center);
		Cell->AddChildToVerticalBox(Origin);

		return Cell;
	}
}

void UBusinessTripProgressWidget::NativeDestruct()
{
	// 소유자보다 오래 사는 타이머를 남기지 않는다
	RemainTicker.Stop();
	Super::NativeDestruct();
}

void UBusinessTripProgressWidget::SetTrip(const FBusinessTripCardTable& InRow, const FText& InDisciplineName,
	const TArray<FBusinessTripCrewView>& InCrew, TFunction<int64()> InRemainingProvider)
{
	if (DestinationText) DestinationText->SetText(InRow.DisplayName);

	if (ConditionText)
	{
		ConditionText->SetText(FText::Format(
			NSLOCTEXT("BusinessTrip", "ProgCondition", "인원 {0} · {1} 합 {2} 이상"),
			FText::AsNumber(InRow.CrewCount), InDisciplineName, FText::AsNumber(InRow.RequiredScoreSum)));
	}

	if (DurationText)
	{
		DurationText->SetText(UBusinessTripCardWidget::FormatTripDuration(InRow.DurationSeconds));
	}

	if (ExperienceText)
	{
		ExperienceText->SetText(FText::Format(
			NSLOCTEXT("BusinessTrip", "ProgExp", "경험치 +{0}"), FText::AsNumber(FMath::RoundToInt(InRow.ExperienceReward))));
	}

	if (CrewHeadText)
	{
		CrewHeadText->SetText(FText::Format(
			NSLOCTEXT("BusinessTrip", "CrewHead", "보낸 직원 {0}명"), FText::AsNumber(InCrew.Num())));
	}

	if (CrewBox && WidgetTree)
	{
		CrewBox->ClearChildren();
		const UCommonTextBlock* NameSample = Cast<UCommonTextBlock>(CrewSample);
		for (const FBusinessTripCrewView& Crew : InCrew)
		{
			if (UVerticalBox* Cell = BuildTripCrewCell(*WidgetTree, Crew, NameSample))
			{
				if (UHorizontalBoxSlot* CrewSlot = CrewBox->AddChildToHorizontalBox(Cell))
				{
					CrewSlot->SetPadding(FMargin(CrewBox->GetChildrenCount() > 1 ? 16.f : 0.f, 0.f, 0.f, 0.f));
				}
			}
		}
	}

	if (RemainText)
	{
		RemainTicker.Stop();
		RemainTicker.Start(GetWorld(), RemainText, MoveTemp(InRemainingProvider));
		RemainTicker.Refresh();
	}
}

void UBusinessTripProgressWidget::SetProgress(float InRatio)
{
	if (ProgressBar) ProgressBar->SetPercent(FMath::Clamp(InRatio, 0.f, 1.f));
}

void UBusinessTripProgressWidget::SetLootPreview(const TArray<FSideDungeonLootRow>& InRows, int32 InStage)
{
	if (!RewardList || !WidgetTree) return;
	UBusinessTripCardWidget::BuildLootPreview(*WidgetTree, GetGameInstance(), *RewardList, InRows, InStage, RewardRowSample);
}
