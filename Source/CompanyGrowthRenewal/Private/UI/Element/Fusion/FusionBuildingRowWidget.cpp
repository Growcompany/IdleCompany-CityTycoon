#include "UI/Element/Fusion/FusionBuildingRowWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Fusion/FusionBuildingLabel.h"
#include "Table/CompanyInfoTable.h"

void UFusionBuildingRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PlateSizer.Bind(BuildingRowFrameBG, BuildingRowFrameLine);
}

void UFusionBuildingRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PlateSizer.Update();
}

void UFusionBuildingRowWidget::Configure(int32 InBuildingIndex, ECompanyType Industry, float Power, int32 CrewCount,
	float Requirement, bool bInSelected, bool bUsedElsewhere)
{
	CurrentBuildingIndex = InBuildingIndex;
	OnClicked().RemoveAll(this);
	OnClicked().AddUObject(this, &UFusionBuildingRowWidget::HandleClicked);
	if (UTableManagerSubsystem* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		bool bFound = false;
		const FCompanyInfoTable Info = Tables->GetCompanyInfo(Industry, bFound);
		// 업종명은 같은 업종 건물끼리 구분이 안 된다 ― 이름은 건물 카드에서 가져온다.
		if (NameText) NameText->SetText(FText::Format(NSLOCTEXT("Fusion", "BuildingName", "{0} · 배치 {1}명"),
			CGFusionLabel::BuildingDisplayName(this, InBuildingIndex), FText::AsNumber(CrewCount)));
		if (IndustryGlyph)
		{
			IndustryGlyph->SetBrushFromTexture(bFound && !Info.GlyphIcon.IsNull() ? Info.GlyphIcon.LoadSynchronous() : nullptr);
			if (bFound) IndustryGlyph->SetColorAndOpacity(Info.AccentColor);
		}
	}
	if (PowerText) PowerText->SetText(FText::Format(NSLOCTEXT("Fusion", "TeamPower", "팀 파워 {0}"),
		UGlobalUtilFunctions::AbbreviateNumber(FMath::FloorToInt64(Power), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor)));
	if (StateText) StateText->SetText(bUsedElsewhere ? NSLOCTEXT("Fusion", "BuildingUsed", "다른 슬롯에서 참여 중")
		: CrewCount == 0 ? NSLOCTEXT("Fusion", "CrewMissing", "직원을 배치하면 시작할 수 있습니다")
		: Power >= Requirement ? NSLOCTEXT("Fusion", "PowerReady", "조건 충족")
		: NSLOCTEXT("Fusion", "PowerShort", "팀 파워 부족"));
	if (SelectedRing) SelectedRing->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetIsInteractionEnabled(!bUsedElsewhere && CrewCount > 0);
}

void UFusionBuildingRowWidget::NativeDestruct()
{
	OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UFusionBuildingRowWidget::HandleClicked()
{
	OnFusionBuildingClicked.Broadcast(CurrentBuildingIndex);
}
