#include "UI/Element/Employee/EquipmentDeltaRowWidget.h"
#include "Components/Border.h"
#include "CommonTextBlock.h"

namespace
{
	// 목업 토큰 — 라이트 판 위의 증감색. sRGB 로 적어 두고 변환은 엔진에 맡긴다(분수 직접 입력 금지).
	const FLinearColor GainColor = FLinearColor::FromSRGBColor(FColor(0x4E, 0x7A, 0x02));
	const FLinearColor LossColor = FLinearColor::FromSRGBColor(FColor(0xB3, 0x26, 0x1E));
}

void UEquipmentDeltaRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
#if WITH_EDITOR
	if (IsDesignTime() && !PreviewName.IsEmpty())
	{
		FEquipmentDeltaRow PreviewRow;
		PreviewRow.Name = PreviewName;
		PreviewRow.Before = PreviewBefore;
		PreviewRow.After = PreviewAfter;
		PreviewRow.bUp = bPreviewBenefit;
		Configure(PreviewRow);
	}
#endif
}

void UEquipmentDeltaRowWidget::Configure(const FEquipmentDeltaRow& Row)
{
	// Before 가 비면 절대값 행 — 이전값·화살표를 접고 값 하나만 남긴다.
	const bool bComparison = !Row.Before.IsEmpty();
	const FLinearColor Trend = Row.bUp ? GainColor : LossColor;
	const ESlateVisibility CompareOnly = bComparison ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

	if (StatNameText) StatNameText->SetText(Row.Name);
	if (BeforeText)
	{
		BeforeText->SetText(Row.Before);
		BeforeText->SetVisibility(CompareOnly);
	}
	if (ArrowText)
	{
		ArrowText->SetText(FText::FromString(TEXT("→")));
		ArrowText->SetVisibility(CompareOnly);
	}
	if (TrendText)
	{
		TrendText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AfterText)
	{
		AfterText->SetText(Row.After);
		// 절대값 행은 증감이 없으므로 색을 입히지 않는다 — 전부 초록으로 보이면 비교로 읽힌다.
		AfterText->SetColorAndOpacity(bComparison ? FSlateColor(Trend) : FSlateColor(FLinearColor::FromSRGBColor(FColor(0x1B, 0x23, 0x30))));
	}
}

void UEquipmentUniqueChipWidget::Configure(const FEquipmentUniqueChange& Change)
{
	const FLinearColor Tone = Change.bGained ? GainColor : LossColor;
	if (ChipText)
	{
		ChipText->SetText(FText::Format(
			NSLOCTEXT("EmployeeEquipmentUI", "UniqueChip", "{0} {1}"),
			Change.bGained ? NSLOCTEXT("EmployeeEquipmentUI", "UniqueGained", "획득")
				: NSLOCTEXT("EmployeeEquipmentUI", "UniqueLost", "상실"),
			Change.Name));
		ChipText->SetColorAndOpacity(FSlateColor(Tone));
	}
	if (ChipBorder)
	{
		// 테두리는 브러시 안에 있어 채움 틴트로는 안 바뀐다 — 절차식 면일 때만 선 색을 같이 바꾼다.
		FSlateBrush Brush = ChipBorder->Background;
		if (Brush.DrawAs == ESlateBrushDrawType::RoundedBox)
		{
			Brush.OutlineSettings.Color = FSlateColor(FLinearColor(Tone.R, Tone.G, Tone.B, 0.40f));
			ChipBorder->SetBrush(Brush);
		}
		ChipBorder->SetBrushColor(FLinearColor(Tone.R, Tone.G, Tone.B, 0.10f));
	}
}
