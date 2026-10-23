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

void UMaterialReqRowWidget::SetRequirement(EResourceType InType, int64 InHave, int64 InNeedPerUnit, int32 InQuantity,
	int32 InCapMax)
{
	FText ResolvedName;
	UTexture2D* ResolvedIcon = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bOk = false;
			const FResourceInfo Info = TableMgr->GetResourceInfo(InType, bOk);
			if (bOk)
			{
				ResolvedName = Info.DisplayName;
				if (!Info.Icon.IsNull()) ResolvedIcon = Info.Icon.LoadSynchronous();
			}
		}
	}

	ApplyRequirement(ResolvedName, ResolvedIcon, InHave, InNeedPerUnit, InQuantity, InCapMax);
}

void UMaterialReqRowWidget::SetProductRequirement(const FText& InName, const TSoftObjectPtr<UTexture2D>& InIcon,
	int64 InHave, int64 InNeedPerUnit, int32 InQuantity, int32 InCapMax)
{
	ApplyRequirement(InName, InIcon.IsNull() ? nullptr : InIcon.LoadSynchronous(), InHave, InNeedPerUnit, InQuantity, InCapMax);
}

void UMaterialReqRowWidget::ApplyRequirement(const FText& InName, UTexture2D* InIcon,
	int64 InHave, int64 InNeedPerUnit, int32 InQuantity, int32 InCapMax)
{
	const int64 NeedTotal = FMath::Max<int64>(0, InNeedPerUnit) * FMath::Max(1, InQuantity);
	const bool bLack = (NeedTotal > 0) && (InHave < NeedTotal);
	const bool bBottleneck = (InCapMax >= 0);

	// 행 위젯은 재사용된다(ProductionStartPopupWidget) ― 실패 경로에서 비우지 않으면
	// 이름·아이콘만 직전 재료로 남아 수치와 다른 재료를 가리킨다.
	if (NameText)
	{
		NameText->SetText(InName);
	}
	if (IconImage)
	{
		IconImage->SetBrushFromTexture(InIcon);
		IconImage->SetVisibility(InIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (NeedText)
	{
		// 비용 표기는 Ceil — 축약이 요구량을 실제보다 작게 보이면 "되는 줄 알았는데 안 됨"이 된다
		NeedText->SetText(UGlobalUtilFunctions::AbbreviateNumber(InNeedPerUnit, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil));
	}
	if (HaveText)
	{
		HaveText->SetText(FText::Format(NSLOCTEXT("MaterialReqRow", "HaveFmt", "보유 {0}"),
			UGlobalUtilFunctions::AbbreviateNumber(InHave)));
		HaveText->SetColorAndOpacity(bLack ? InkLack : (bBottleneck ? InkCap : InkOk));
	}
	if (LackText)
	{
		LackText->SetText(bLack
			? FText::Format(NSLOCTEXT("MaterialReqRow", "LackCountFmt", "{0}개 부족"),
				UGlobalUtilFunctions::AbbreviateNumber(NeedTotal - InHave, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil))
			: FText::GetEmpty());
		LackText->SetColorAndOpacity(InkLack);
		LackText->SetVisibility(bLack ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (FillBar)
	{
		// 보유 0 인데 필요가 있으면 가득(전부 모자람), 필요가 없으면 빈 막대
		const float Percent = (InHave > 0)
			? FMath::Clamp(static_cast<float>(static_cast<double>(NeedTotal) / static_cast<double>(InHave)), 0.0f, 1.0f)
			: (NeedTotal > 0 ? 1.0f : 0.0f);
		FillBar->SetPercent(Percent);
		FillBar->SetFillColorAndOpacity(bLack ? FillColorLack : (bBottleneck ? FillColorCap : FillColorOk));
	}

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
}
