#include "UI/Panel/EmployeeGachaProbabilityWidget.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/RecruitmentManagerSubsystem.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/LootBoxRarity.h"

void UEmployeeGachaProbabilityWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UIE_CloseButton && !UIE_CloseButton->OnCloseClicked.IsBound())
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UEmployeeGachaProbabilityWidget::OnCloseClicked);
	}

	if (PityRuleText)
	{
		PityRuleText->SetText(FText::FromString(FString::Printf(
			TEXT("%d회부터 +2%%/회, %d회 Legendary 확정"),
			FGachaPityData::SoftPityStart, FGachaPityData::HardPity)));
	}

	BuildTierRows(NormalRows, EGachaTier::Normal);
	BuildTierRows(AdvancedRows, EGachaTier::Advanced);
	BuildTierRows(PremiumRows, EGachaTier::Premium);
}

void UEmployeeGachaProbabilityWidget::NativeDestruct()
{
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UEmployeeGachaProbabilityWidget::SetupTable(int32 InHRPower)
{
	HRPower = InHRPower;
	BuildTierRows(NormalRows, EGachaTier::Normal);   // HR 반영 재빌드
}

void UEmployeeGachaProbabilityWidget::BuildTierRows(UVerticalBox* Container, EGachaTier Tier)
{
	if (!Container) { return; }
	Container->ClearChildren();

	URecruitmentManagerSubsystem* RecMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<URecruitmentManagerSubsystem>() : nullptr;
	if (!RecMgr) { return; }

	const TArray<FGachaRarityChance> Rows = RecMgr->GetProbabilityTableForUI(Tier, HRPower);
	for (const FGachaRarityChance& Row : Rows)
	{
		UTextBlock* Line = NewObject<UTextBlock>(this);
		Line->SetText(FText::FromString(FString::Printf(TEXT("%s   %.0f%%"),
			*FLootBoxRarityUtility::GetKoreanName(Row.Rarity), Row.Percent)));
		Line->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityColor(Row.Rarity)));
		Container->AddChildToVerticalBox(Line);
	}
}

void UEmployeeGachaProbabilityWidget::OnCloseClicked()
{
	// 채용 패널 위에 뷰포트 오버레이로 떠 있으므로(스택 X) RemoveFromParent 로 닫음
	RemoveFromParent();
}
