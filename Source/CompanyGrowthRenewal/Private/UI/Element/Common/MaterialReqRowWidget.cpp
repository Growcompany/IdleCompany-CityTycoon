// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/MaterialReqRowWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Global/GlobalUtilFunctions.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"

void UMaterialReqRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 프리뷰가 "부족" 상태로 보이면 오독을 부른다 — 기본은 충분 상태.
	ApplyLackVisual(false);
}

void UMaterialReqRowWidget::SetRequirement(EResourceType InType, int64 InHave, int64 InNeed, int32 InCapMax)
{
	const bool bLack = (InNeed > 0) && (InHave < InNeed);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bOk = false;
			const FResourceInfo Info = TableMgr->GetResourceInfo(InType, bOk);
			if (bOk)
			{
				if (NameText)
				{
					NameText->SetText(Info.DisplayName);
				}
				if (IconImage && !Info.Icon.IsNull())
				{
					if (UTexture2D* Icon = Info.Icon.LoadSynchronous())
					{
						IconImage->SetBrushFromTexture(Icon);
					}
				}
			}
		}
	}

	if (HaveText)
	{
		HaveText->SetText(UGlobalUtilFunctions::AbbreviateNumber(InHave));
	}
	if (NeedText)
	{
		// 비용 표기는 Ceil — 축약이 요구량을 실제보다 작게 보이면 "되는 줄 알았는데 안 됨"이 된다
		NeedText->SetText(FText::Format(
			NSLOCTEXT("MaterialReqRow", "NeedFmt", "/ {0}"),
			UGlobalUtilFunctions::AbbreviateNumber(InNeed, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil)));
	}
	if (LackText)
	{
		LackText->SetText(bLack
			? FText::Format(NSLOCTEXT("MaterialReqRow", "LackFmt", "-{0}"),
				UGlobalUtilFunctions::AbbreviateNumber(InNeed - InHave, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil))
			: FText::GetEmpty());
	}

	if (FillBar)
	{
		const float Percent = (InNeed > 0)
			? FMath::Clamp(static_cast<float>(static_cast<double>(InHave) / static_cast<double>(InNeed)), 0.0f, 1.0f)
			: 1.0f;
		FillBar->SetPercent(Percent);
		FillBar->SetFillColorAndOpacity(bLack ? FillColorLack : FillColorOk);
	}

	const bool bBottleneck = (InCapMax >= 0);
	if (CapMarkBorder)
	{
		CapMarkBorder->SetVisibility(bBottleneck ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CapMarkText && bBottleneck)
	{
		CapMarkText->SetText(FText::Format(NSLOCTEXT("MaterialReqRow", "CapFmt", "최대 {0}"), FText::AsNumber(InCapMax)));
	}

	ApplyLackVisual(bLack);
}

void UMaterialReqRowWidget::ApplyLackVisual(bool bLack)
{
	const ESlateVisibility LackVis = bLack ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (LackTint) LackTint->SetVisibility(LackVis);
	if (LackEdge) LackEdge->SetVisibility(LackVis);

	// 숫자만 색이 바뀐다 — 이름까지 붉히면 행 전체가 경보가 되어 어느 값이 문제인지 흐려진다
	if (HaveText) HaveText->SetColorAndOpacity(bLack ? InkLack : InkOk);
	if (LackText) LackText->SetColorAndOpacity(InkLack);
}
