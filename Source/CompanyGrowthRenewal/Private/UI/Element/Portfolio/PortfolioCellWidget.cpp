#include "UI/Element/Portfolio/PortfolioCellWidget.h"

#include "CommonTextBlock.h"
#include "Enum/QualityGrade.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FLinearColor PortfolioCreamFill(0.906f, 0.839f, 0.694f, 1.0f);
	const FLinearColor PlateDark(0.070f, 0.088f, 0.120f, 1.0f);
	const FLinearColor InkText(0.020f, 0.032f, 0.052f, 1.0f);
	const FLinearColor PortfolioInkSub(0.230f, 0.170f, 0.095f, 1.0f);
	const FLinearColor WellDarkUndiscovered(0.020f, 0.028f, 0.042f, 1.0f);
	const FLinearColor NameMuted(0.72f, 0.76f, 0.82f, 1.0f);
	const FLinearColor HintMuted(0.45f, 0.50f, 0.58f, 1.0f);

	FLinearColor Darken(const FLinearColor& In, float Amount)
	{
		return FMath::Lerp(In, FLinearColor(0.02f, 0.03f, 0.05f, 1.f), Amount);
	}

}

void UPortfolioCellWidget::ConfigureDiscovered(const FText& Name, const FString& Rarity, const FText& Sales, const FLinearColor& IndustryColor, UTexture2D* IconTex)
{
	if (Plate) { Plate->SetBrushColor(PortfolioCreamFill); }
	if (WellBG) { WellBG->SetBrushTintColor(FSlateColor(Darken(IndustryColor, 0.52f))); }
	if (NameText)
	{
		NameText->SetText(Name);
		NameText->SetColorAndOpacity(FSlateColor(InkText));
	}
	if (SalesText)
	{
		SalesText->SetText(Sales);
		SalesText->SetColorAndOpacity(FSlateColor(PortfolioInkSub));
	}
	// 등급 출처(출시 기록)가 없으면 빈 배지 대신 통째로 접는다 — 자리채움이 등급인 척하지 않게
	const bool bHasGrade = !Rarity.IsEmpty();
	if (RarityBadge)
	{
		RarityBadge->SetBrushColor(GradeLetterToColor(Rarity));
		RarityBadge->SetVisibility(bHasGrade ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (BadgeText)
	{
		BadgeText->SetText(FText::FromString(Rarity));
		BadgeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	if (QText) { QText->SetVisibility(ESlateVisibility::Collapsed); }
	if (GroundShadow) { GroundShadow->SetVisibility(ESlateVisibility::HitTestInvisible); }
	if (WellGloss) { WellGloss->SetVisibility(ESlateVisibility::HitTestInvisible); }

	// 실제 게임 이미지 주입 — 라운드 머티리얼(M_UI_RoundedThumb) MID 의 Tex 파라미터. Icon 없으면 디자인타임 기본 유지.
	if (ThumbnailImage && IconTex)
	{
		UMaterialInterface* Base = Cast<UMaterialInterface>(ThumbnailImage->GetBrush().GetResourceObject());
		if (!Base)
		{
			Base = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
				TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_RoundedThumb.M_UI_RoundedThumb"))).LoadSynchronous();
		}
		if (Base)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetTextureParameterValue(FName("Tex"), IconTex);
			ThumbnailImage->SetBrushFromMaterial(MID);
		}
	}
	if (ThumbnailImage) { ThumbnailImage->SetVisibility(ESlateVisibility::HitTestInvisible); }
}

void UPortfolioCellWidget::ConfigureUndiscovered(const FString& RarityHint, const FText& MaterialHint)
{
	if (Plate) { Plate->SetBrushColor(PlateDark); }
	if (WellBG) { WellBG->SetBrushTintColor(FSlateColor(WellDarkUndiscovered)); }
	if (NameText)
	{
		NameText->SetText(FText::FromString(TEXT("???")));
		NameText->SetColorAndOpacity(FSlateColor(NameMuted));
	}
	if (SalesText)
	{
		SalesText->SetText(MaterialHint);
		SalesText->SetColorAndOpacity(FSlateColor(HintMuted));
	}
	if (RarityBadge) { RarityBadge->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.10f)); }
	if (BadgeText)
	{
		BadgeText->SetText(FText::FromString(RarityHint));
		BadgeText->SetColorAndOpacity(FSlateColor(HintMuted));
	}
	if (QText) { QText->SetVisibility(ESlateVisibility::HitTestInvisible); }
	if (GroundShadow) { GroundShadow->SetVisibility(ESlateVisibility::Collapsed); }
	if (ThumbnailImage) { ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed); }
}

void UPortfolioCellWidget::ConfigureLocked(const FString& RarityHint)
{
	// 잠금 = 미발견 골격 + 더 뮤트한 이름색 + '잠금' 힌트. 클릭 비활성은 CodexPanel 이 델리게이트 미바인딩으로.
	ConfigureUndiscovered(RarityHint, NSLOCTEXT("Codex", "CellLocked", "잠금"));
	if (NameText) { NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.44f, 0.50f, 1.f))); }
}

