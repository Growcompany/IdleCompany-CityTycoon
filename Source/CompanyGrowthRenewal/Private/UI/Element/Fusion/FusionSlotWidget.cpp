#include "UI/Element/Fusion/FusionSlotWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Fusion/FusionBuildingLabel.h"
#include "Table/CompanyInfoTable.h"

void UFusionSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PlateSizer.Bind(SlotFrameBG, SlotFrameLine);
}

void UFusionSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PlateSizer.Update();
}

void UFusionSlotWidget::Configure(int32 InSlotIndex, ECompanyType Industry, int32 BuildingIndex, float Power,
	float Requirement, bool bEditing, bool bMissingIndustry)
{
	CurrentSlotIndex = InSlotIndex;
	OnClicked().RemoveAll(this);
	OnClicked().AddUObject(this, &UFusionSlotWidget::HandleClicked);
	if (UTableManagerSubsystem* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		bool bFound = false;
		const FCompanyInfoTable Info = Tables->GetCompanyInfo(Industry, bFound);
		if (IndustryText) IndustryText->SetText(bFound ? Info.DisplayName : FText::GetEmpty());
		if (BuildingNameText) BuildingNameText->SetText(BuildingIndex != INDEX_NONE
			? CGFusionLabel::BuildingDisplayName(this, BuildingIndex)
			: NSLOCTEXT("Fusion", "SelectBuilding", "건물 선택"));
		if (IndustryPlate) IndustryPlate->SetColorAndOpacity(bFound ? Info.AccentColor : FLinearColor::White);
		if (IndustryGlyph)
		{
			IndustryGlyph->SetBrushFromTexture(bFound && !Info.GlyphIcon.IsNull() ? Info.GlyphIcon.LoadSynchronous() : nullptr);
		}
	}
	const bool bAssigned = BuildingIndex != INDEX_NONE;
	const bool bReady = bAssigned && Power >= Requirement;
	if (PowerText) PowerText->SetText(FText::Format(NSLOCTEXT("Fusion", "PowerRatio", "{0} / {1}"),
		UGlobalUtilFunctions::AbbreviateNumber(FMath::FloorToInt64(Power), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor),
		UGlobalUtilFunctions::AbbreviateNumber(FMath::CeilToInt64(Requirement), ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil)));
	if (StateText) StateText->SetText(bMissingIndustry && IndustryText
		? FText::Format(NSLOCTEXT("Fusion", "IndustryMissing", "{0} 건물 필요"), IndustryText->GetText())
		: !bAssigned ? NSLOCTEXT("Fusion", "EmptySlot", "건물을 골라 주세요")
		: bReady ? NSLOCTEXT("Fusion", "PowerReady", "조건 충족") : NSLOCTEXT("Fusion", "PowerShort", "팀 파워 부족"));
	if (RequirementBar)
	{
		RequirementBar->SetPercent(Requirement > 0.f ? FMath::Clamp(Power / Requirement, 0.f, 1.f) : 0.f);
		RequirementBar->SetFillColorAndOpacity(bReady ? ReadyColor : ShortColor);
	}
	if (SelectedRing) SelectedRing->SetVisibility(bEditing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UFusionSlotWidget::NativeDestruct()
{
	OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UFusionSlotWidget::HandleClicked()
{
	OnFusionSlotClicked.Broadcast(CurrentSlotIndex);
}
