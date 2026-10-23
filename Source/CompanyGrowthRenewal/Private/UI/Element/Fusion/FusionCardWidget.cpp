#include "UI/Element/Fusion/FusionCardWidget.h"

#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Table/FusionProjectTable.h"
#include "UI/Element/Common/RewardChipUtils.h"
#include "UI/Element/SideContent/BusinessTripCardWidget.h"

void UFusionCardWidget::Configure(FName InKey, const FFusionProjectTable& Row, int32 Level, float Requirement,
	const TArray<FMissionReward>& Rewards, const FText& Status, bool bInLocked, bool bActive)
{
	FusionKey = InKey;
	OnClicked().RemoveAll(this);
	OnClicked().AddUObject(this, &UFusionCardWidget::HandleClicked);
	if (NameText) NameText->SetText(Row.DisplayName);
	if (LevelText) LevelText->SetText(FText::Format(NSLOCTEXT("Fusion", "Level", "Lv.{0}"), FText::AsNumber(Level)));
	if (DurationText) DurationText->SetText(UBusinessTripCardWidget::FormatTripDuration(Row.DurationSeconds));
	if (RequirementText) RequirementText->SetText(FText::Format(NSLOCTEXT("Fusion", "Requirement", "필요 팀 파워 {0}"),
		UGlobalUtilFunctions::AbbreviateNumber(FMath::CeilToInt64(Requirement), ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil)));
	if (StatusText)
	{
		StatusText->SetText(Status);
		StatusText->SetColorAndOpacity(FSlateColor(bInLocked && !bActive ? LockedColor : AvailableColor));
	}
	if (SelectedRing) SelectedRing->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (RewardBox && WidgetTree) CGRewardChip::RenderIcons(*WidgetTree, GetGameInstance(), *RewardBox, Rewards);
	if (IndustryText)
	{
		TArray<FString> Names;
		if (UTableManagerSubsystem* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
		{
			UImage* Glyphs[] = {IndustryGlyph0, IndustryGlyph1, IndustryGlyph2};
			UImage* Plates[] = {IndustryPlate0, IndustryPlate1, IndustryPlate2};
			for (int32 IndustryIndex = 0; IndustryIndex < Row.RequiredIndustries.Num(); ++IndustryIndex)
			{
				bool bFound = false;
				const FCompanyInfoTable Info = Tables->GetCompanyInfo(Row.RequiredIndustries[IndustryIndex], bFound);
				if (bFound) Names.Add(Info.DisplayName.ToString());
				if (IndustryIndex < 3)
				{
					if (Glyphs[IndustryIndex]) Glyphs[IndustryIndex]->SetBrushFromTexture(bFound && !Info.GlyphIcon.IsNull() ? Info.GlyphIcon.LoadSynchronous() : nullptr);
					if (Plates[IndustryIndex] && bFound) Plates[IndustryIndex]->SetColorAndOpacity(bInLocked ? LockedPlateColor : Info.AccentColor);
				}
			}
		}
		IndustryText->SetText(FText::FromString(FString::Join(Names, TEXT(" + "))));
	}
	if (IndustryTile2) IndustryTile2->SetVisibility(Row.RequiredIndustries.Num() > 2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Connector2) Connector2->SetVisibility(Row.RequiredIndustries.Num() > 2 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	// 잠금 사유는 읽을 수 있어야 하므로 글자까지 희미하게 만들지 않는다.
	SetIsInteractionEnabled(true);
}

void UFusionCardWidget::NativeDestruct()
{
	OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UFusionCardWidget::HandleClicked()
{
	OnFusionClicked.Broadcast(FusionKey);
}
