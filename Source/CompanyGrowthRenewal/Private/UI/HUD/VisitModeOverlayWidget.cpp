#include "UI/HUD/VisitModeOverlayWidget.h"
#include "Manager/RankingManagerSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UVisitModeOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HomeBtn)
	{
		HomeBtn->OnClicked.AddDynamic(this, &UVisitModeOverlayWidget::OnHomeBtnClicked);
	}
}

void UVisitModeOverlayWidget::NativeDestruct()
{
	if (HomeBtn)
	{
		HomeBtn->OnClicked.RemoveDynamic(this, &UVisitModeOverlayWidget::OnHomeBtnClicked);
	}

	Super::NativeDestruct();
}

void UVisitModeOverlayWidget::SetVisitInfo(const FString& PlayerName, int32 HQLevel)
{
	if (PlayerNameText)
	{
		PlayerNameText->SetText(FText::FromString(PlayerName));
	}

	if (PlayerLevelText)
	{
		PlayerLevelText->SetText(FText::FromString(FString::Printf(TEXT("%d"), HQLevel)));
	}
}

void UVisitModeOverlayWidget::OnHomeBtnClicked()
{
	if (URankingManagerSubsystem* RankingMgr = GetGameInstance()->GetSubsystem<URankingManagerSubsystem>())
	{
		RankingMgr->ExitVisitMode();
	}
}
