#include "UI/Element/Skyline/SkylineCompanyRowWidget.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"

void USkylineCompanyRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RowButton)
	{
		RowButton->OnClicked().AddUObject(this, &USkylineCompanyRowWidget::HandleRowClicked);
	}
}

void USkylineCompanyRowWidget::NativeDestruct()
{
	if (RowButton)
	{
		RowButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void USkylineCompanyRowWidget::SetCompany(int32 BuildingIndex, ECompanyType Industry, float Power, int32 CrewCount, bool bSelected)
{
	CurrentBuildingIndex = BuildingIndex;

	// 산업 표시명/글리프/색은 DT_CompanyInfo 가 단일 진실이다.
	// UMETA(DisplayName) 은 에디터 전용이라 패키징 빌드에서 영어 식별자로 떨어진다 — 쓰지 않는다.
	FCompanyInfoTable CompanyInfo;
	bool bCompanyFound = false;
	if (Industry != ECompanyType::None)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				CompanyInfo = TableMgr->GetCompanyInfo(Industry, bCompanyFound);
			}
		}
	}

	const ESlateVisibility IndustryVis =
		bCompanyFound ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (IndustryText)
	{
		if (bCompanyFound)
		{
			IndustryText->SetText(CompanyInfo.DisplayName);
		}
		IndustryText->SetVisibility(IndustryVis);
	}

	if (IndustryGlyph)
	{
		if (bCompanyFound)
		{
			if (UTexture2D* GlyphTex = CompanyInfo.GlyphIcon.LoadSynchronous())
			{
				IndustryGlyph->SetBrushFromTexture(GlyphTex);
			}
			IndustryGlyph->SetColorAndOpacity(CompanyInfo.AccentColor);
		}
		IndustryGlyph->SetVisibility(IndustryVis);
	}

	if (AccentImage)
	{
		if (bCompanyFound)
		{
			AccentImage->SetColorAndOpacity(CompanyInfo.AccentColor);
		}
		AccentImage->SetVisibility(IndustryVis);
	}

	if (PowerText)
	{
		PowerText->SetText(UGlobalUtilFunctions::AbbreviateNumber(
			FMath::RoundToInt64(Power), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	}

	if (CrewCountText)
	{
		CrewCountText->SetText(FText::Format(
			NSLOCTEXT("Skyline", "CompanyCrewCount", "배치 {0}명"), FText::AsNumber(CrewCount)));
	}

	if (SelectedMark)
	{
		SelectedMark->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USkylineCompanyRowWidget::HandleRowClicked()
{
	OnCompanyRowClicked.Broadcast(CurrentBuildingIndex);
}
