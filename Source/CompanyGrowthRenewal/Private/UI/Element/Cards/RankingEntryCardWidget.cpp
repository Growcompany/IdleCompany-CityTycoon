#include "UI/Element/Cards/RankingEntryCardWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ProfileImageData.h"
#include "Enum/CompanyTitle.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/WidgetSwitcher.h"
#include "CommonButtonBase.h"

void URankingEntryCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RowButton) RowButton->OnClicked().AddUObject(this, &URankingEntryCardWidget::OnMoveBtnClicked);
	SetVisitEnabled(bVisitEnabled);

	if (MoveBtn)
	{
		MoveBtn->OnClicked().AddUObject(this, &URankingEntryCardWidget::OnMoveBtnClicked);
	}
}

void URankingEntryCardWidget::NativeDestruct()
{
	if (MoveBtn)
	{
		MoveBtn->OnClicked().RemoveAll(this);
	}

	if (RowButton) RowButton->OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void URankingEntryCardWidget::SetEntryData(const FRankingEntry& InEntry)
{
	CachedEntry = InEntry;
	if (PlayerNameText) PlayerNameText->SetText(FText::FromString(InEntry.DisplayName));
	if (UWidgetSwitcher* RankVisual = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("RankVisualSwitcher"))))
	{
		RankVisual->SetActiveWidgetIndex(InEntry.Rank >= 1 && InEntry.Rank <= 3 ? InEntry.Rank : 0);
	}

	if (RankText)
	{
		if (InEntry.Rank > 0)
		{
			RankText->SetText(FText::FromString(FString::Printf(TEXT("%d위"), InEntry.Rank)));
		}
		else
		{
			RankText->SetText(FText::FromString(TEXT("-")));
		}
	}

	if (HQLevelText)
	{
		HQLevelText->SetText(InEntry.HQLevel > 0 ? FText::FromString(FString::Printf(TEXT("Lv.%d"), InEntry.HQLevel)) : FText::GetEmpty());
	}

	if (RevenueText)
	{
		RevenueText->SetText(UGlobalUtilFunctions::AbbreviateNumber(InEntry.TotalRevenue, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	}

	// 프로필 이미지 로드
	if (IconImage)
	{
		IconImage->SetVisibility(InEntry.PlayFabId.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (TableMgr)
		{
			bool bSuccess = false;
			FProfileImageData ImgData = TableMgr->GetProfileImageData(InEntry.ProfileImageID, bSuccess);
			if (bSuccess && !ImgData.Icon.IsNull())
			{
				UTexture2D* Texture = ImgData.Icon.LoadSynchronous();
				if (Texture)
				{
					IconImage->SetBrushFromTexture(Texture);
				}
			}
		}
	}
}

void URankingEntryCardWidget::OnMoveBtnClicked()
{
	if (bVisitEnabled) OnEntryCardClicked.Broadcast(CachedEntry);
}

void URankingEntryCardWidget::PlayIntro(int32 OrderIndex)
{
	Intro.Play(*this, OrderIndex);
}

void URankingEntryCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Intro.Tick(*this, InDeltaTime);
}

void URankingEntryCardWidget::SetVisitEnabled(bool bEnabled)
{
	bVisitEnabled = bEnabled;
	if (RowButton) RowButton->SetIsEnabled(bEnabled);
	if (MoveBtn)
	{
		MoveBtn->SetIsEnabled(bEnabled);
		MoveBtn->SetVisibility(bEnabled ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void URankingEntryCardWidget::SetScoreAvailable(bool bAvailable)
{
	if (RevenueText) RevenueText->SetText(bAvailable ? UGlobalUtilFunctions::AbbreviateNumber(CachedEntry.TotalRevenue, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor) : FText::FromString(TEXT("-")));
}
