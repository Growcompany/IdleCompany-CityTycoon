#include "UI/Panel/PotentialOddsWidget.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Data/EmployeePotentialData.h"
#include "Enum/ItemType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ShopItemTable.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Employee/PotentialOddsRowWidget.h"

void UPotentialOddsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UPotentialOddsWidget::OnCloseClicked);
	BackgroundBtn->OnClicked.AddDynamic(this, &UPotentialOddsWidget::HandleDimClicked);
	Rebuild();
}

void UPotentialOddsWidget::NativeDestruct()
{
	UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	BackgroundBtn->OnClicked.RemoveDynamic(this, &UPotentialOddsWidget::HandleDimClicked);
	Super::NativeDestruct();
}

void UPotentialOddsWidget::Rebuild()
{
	if (!OddsBox)
	{
		return;
	}

	OddsBox->ClearChildren();
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UpdateCardCeilings(TableMgr);
	TSubclassOf<UUserWidget> RowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PotentialOddsRow) : nullptr;
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PotentialOdds] DT_WidgetClass 에 PotentialOddsRow 행이 없습니다"));
		return;
	}

	const TArray<FPotentialRarityOdds> BaseOdds = UEmployeePotentialHelper::GetBaseRarityOdds();
	const ELootBoxRarity HighestCardCeiling = UEmployeePotentialHelper::GetCubeCeiling(EItemType::BusinessCardBlack);
	for (int32 Index = BaseOdds.Num() - 1; Index >= 0; --Index)
	{
		const FPotentialRarityOdds& Odds = BaseOdds[Index];
		UPotentialOddsRowWidget* RowWidget = Cast<UPotentialOddsRowWidget>(CreateWidget(this, RowClass));
		if (!RowWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PotentialOdds] PotentialOddsRow 클래스 타입이 맞지 않습니다"));
			continue;
		}
		const float UpgradePercent = UEmployeePotentialHelper::GetRarityUpgradeChancePercent(
			Odds.Rarity, Odds.Rarity, HighestCardCeiling);
		RowWidget->SetOdds(Odds.Rarity, UpgradePercent);
		OddsBox->AddChildToVerticalBox(RowWidget);
		if (Index > 0)
		{
			USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>();
			DividerSize->SetHeightOverride(2.f);
			UBorder* Divider = WidgetTree->ConstructWidget<UBorder>();
			Divider->SetBrushColor(FLinearColor(0.28f, 0.32f, 0.38f, 0.28f));
			DividerSize->AddChild(Divider);
			if (UVerticalBoxSlot* DividerSlot = OddsBox->AddChildToVerticalBox(DividerSize))
			{
				DividerSlot->SetPadding(FMargin(24.f, 0.f, 24.f, 0.f));
			}
		}
	}
}

void UPotentialOddsWidget::UpdateCardCeilings(UTableManagerSubsystem* TableMgr)
{
	UCommonTextBlock* CardNames[] = { CardName0, CardName1, CardName2 };
	UCommonTextBlock* CardCaps[] = { CardCap0, CardCap1, CardCap2 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		CardNames[Index]->SetText(FText::GetEmpty());
		CardCaps[Index]->SetText(FText::GetEmpty());
	}
	if (!TableMgr)
	{
		return;
	}

	const EItemType CardTypes[] = {
		EItemType::BusinessCardPaper,
		EItemType::BusinessCardGold,
		EItemType::BusinessCardBlack,
	};
	FShopItemTable ShopRows[3];
	ELootBoxRarity Ceilings[3];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!TableMgr->GetShopItemByItemType(CardTypes[Index], ShopRows[Index]) || ShopRows[Index].DisplayName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[PotentialOdds] DT_ShopItem 명함 표시명 누락 (ItemType=%d)"), static_cast<int32>(CardTypes[Index]));
			return;
		}
		Ceilings[Index] = UEmployeePotentialHelper::GetCubeCeiling(CardTypes[Index]);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		CardNames[Index]->SetText(ShopRows[Index].DisplayName);
		CardCaps[Index]->SetText(FText::Format(
			NSLOCTEXT("PotentialOdds", "CardCeiling", "{0}까지"),
			FText::FromString(FLootBoxRarityUtility::GetKoreanName(Ceilings[Index]))));
		FLinearColor GradeColor = FLootBoxRarityUtility::GetRarityColor(Ceilings[Index]);
		GradeColor.A = 1.f;
		CardCaps[Index]->SetColorAndOpacity(FSlateColor(GradeColor));
	}
}

void UPotentialOddsWidget::OnCloseClicked()
{
	RemoveFromParent();
}

void UPotentialOddsWidget::HandleDimClicked()
{
	RemoveFromParent();
}
