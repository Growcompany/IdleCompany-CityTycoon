#include "UI/Panel/RankingPlayerDetailWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/RankingManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Enum/CompanyTitle.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "CommonButtonBase.h"

void URankingPlayerDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (VisitCityButton)
	{
		VisitCityButton->OnClicked().AddUObject(this, &URankingPlayerDetailWidget::OnVisitCityClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &URankingPlayerDetailWidget::OnCloseClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &URankingPlayerDetailWidget::OnBackgroundClicked);
	}
}

void URankingPlayerDetailWidget::NativeDestruct()
{
	if (VisitCityButton)
	{
		VisitCityButton->OnClicked().RemoveAll(this);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &URankingPlayerDetailWidget::OnCloseClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &URankingPlayerDetailWidget::OnBackgroundClicked);
	}

	Super::NativeDestruct();
}

void URankingPlayerDetailWidget::SetPlayerData(const FRankingEntry& InEntry)
{
	CachedEntry = InEntry;

	if (PlayerNameText)
	{
		PlayerNameText->SetText(FText::FromString(InEntry.DisplayName));
	}

	if (CompanyTitleText)
	{
		CompanyTitleText->SetText(FText::FromString(CompanyTitleToString(InEntry.CompanyTitle)));
	}

	if (HQLevelText)
	{
		HQLevelText->SetText(FText::FromString(FString::Printf(TEXT("HQ Lv.%d"), InEntry.HQLevel)));
	}

	if (BuildingInfoText)
	{
		BuildingInfoText->SetText(FText::FromString(
			FString::Printf(TEXT("빌딩: %d개  최고 T%d"), InEntry.BuildingCount, InEntry.MaxTier)));
	}

	if (EmployeeCountText)
	{
		EmployeeCountText->SetText(FText::FromString(
			FString::Printf(TEXT("직원: %d명"), InEntry.EmployeeCount)));
	}

	if (RevenueText)
	{
		RevenueText->SetText(FText::FromString(FString::Printf(TEXT("누적 매출: %s"),
			*UGlobalUtilFunctions::AbbreviateNumber(InEntry.TotalRevenue, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));
	}
}

void URankingPlayerDetailWidget::OnVisitCityClicked()
{
	if (URankingManagerSubsystem* RankingMgr = GetGameInstance()->GetSubsystem<URankingManagerSubsystem>())
	{
		RankingMgr->EnterVisitMode(CachedEntry.PlayFabId, CachedEntry.DisplayName);
	}
}

void URankingPlayerDetailWidget::OnCloseClicked()
{
	DeactivateWidget();
}

void URankingPlayerDetailWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}
