#include "UI/Element/Employee/DisciplineCellButtonWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

namespace
{
	// 스펙 §3 상태 팔레트 (sRGB → linear). 상수명은 전역 유일하게 (유니티 빌드 셰도잉 방지)
	const FLinearColor DiscCellStripGold      = FLinearColor::FromSRGBColor(FColor(0xF0, 0xA3, 0x2E));
	const FLinearColor DiscCellStripPressed   = FLinearColor::FromSRGBColor(FColor(0xD0, 0x84, 0x09));
	const FLinearColor DiscCellStripDisabled  = FLinearColor::FromSRGBColor(FColor(0xC4, 0xC6, 0xCA));
	const FLinearColor DiscCellPillBg         = FLinearColor::FromSRGBColor(FColor(0xFF, 0xFD, 0xF8));
	const FLinearColor DiscCellPillBgDisabled = FLinearColor::FromSRGBColor(FColor(0xE8, 0xE9, 0xEB));
	const FLinearColor DiscCellCostInk        = FLinearColor::FromSRGBColor(FColor(0x1B, 0x5E, 0x86));
	// 몸통이 그린 패턴 버튼(UIE_CostActionButton 레시피)이라 활성 잉크는 전부 크림 — 아웃라인은 WBP 폰트 소유
	const FLinearColor DiscCellLabelInk       = FLinearColor::FromSRGBColor(FColor(0xFF, 0xF8, 0xF0));
	const FLinearColor DiscCellInkDisabled    = FLinearColor::FromSRGBColor(FColor(0x82, 0x85, 0x8C));
	const FLinearColor DiscCellValueInk       = FLinearColor::FromSRGBColor(FColor(0xFF, 0xF8, 0xF0));
	const FLinearColor DiscCellValueDisabled  = FLinearColor::FromSRGBColor(FColor(0x8D, 0x90, 0x98));
	const FLinearColor DiscCellGlyphInk       = FLinearColor::FromSRGBColor(FColor(0xFF, 0xF8, 0xF0));
	const FLinearColor DiscCellGlyphDisabled  = FLinearColor::FromSRGBColor(FColor(0x9A, 0x9D, 0xA5));
}

void UDisciplineCellButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 인스턴스 지정 글리프 적용 — 디자이너 프리뷰에서도 보이도록 PreConstruct 에서
	if (IconImg && !IconTexture.IsNull())
	{
		if (UTexture2D* Tex = IconTexture.LoadSynchronous())
		{
			IconImg->SetBrushFromTexture(Tex);
		}
	}

	ApplyInvestVisuals(bInvestEnabled);
}

void UDisciplineCellButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 눌림 트랜스폼은 셀 자신도 base 이벤트를 구독해 처리 (카드의 홀드 구독과 별개)
	OnPressed().AddUObject(this, &UDisciplineCellButtonWidget::HandleCellPressed);
	OnReleased().AddUObject(this, &UDisciplineCellButtonWidget::HandleCellReleased);
}

void UDisciplineCellButtonWidget::NativeDestruct()
{
	OnPressed().RemoveAll(this);
	OnReleased().RemoveAll(this);
	Super::NativeDestruct();
}

void UDisciplineCellButtonWidget::SetDiscipline(const FText& Name, int32 Total, int32 Invested)
{
	if (NameText)
	{
		NameText->SetText(Name);
	}
	if (ValueText)
	{
		ValueText->SetText(FText::AsNumber(Total));
	}
	if (BonusText)
	{
		if (Invested > 0)
		{
			BonusText->SetText(FText::FromString(FString::Printf(TEXT("(+%d)"), Invested)));
			BonusText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			BonusText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UDisciplineCellButtonWidget::SetInvestEnabled(bool bEnabled)
{
	bInvestEnabled = bEnabled;
	SetEnabled(bEnabled);    // 내부 버튼 상호작용
	SetIsEnabled(bEnabled);  // 셀 자신 — M9 가이드가 GetIsEnabled 로 SP 유무를 판정 (EmployeeWindowWidget.cpp:398)
	if (!bEnabled)
	{
		HandleCellReleased();  // 홀드 중 SP 소진 시 눌림 트랜스폼 잔류 방지 (비활성은 Released 미발화)
	}
	ApplyInvestVisuals(bEnabled);
}

void UDisciplineCellButtonWidget::HandleCellPressed()
{
	if (FaceBox)
	{
		FaceBox->SetRenderTranslation(FVector2D(0.f, PressDepth));
	}
	if (StripBG && bInvestEnabled)
	{
		StripBG->SetColorAndOpacity(DiscCellStripPressed);
	}
}

void UDisciplineCellButtonWidget::HandleCellReleased()
{
	if (FaceBox)
	{
		FaceBox->SetRenderTranslation(FVector2D::ZeroVector);
	}
	if (StripBG)
	{
		StripBG->SetColorAndOpacity(bInvestEnabled ? DiscCellStripGold : DiscCellStripDisabled);
	}
}

void UDisciplineCellButtonWidget::ApplyInvestVisuals(bool bEnabled)
{
	if (StripBG)
	{
		StripBG->SetColorAndOpacity(bEnabled ? DiscCellStripGold : DiscCellStripDisabled);
	}
	if (CostPill)
	{
		CostPill->SetBrushColor(bEnabled ? DiscCellPillBg : DiscCellPillBgDisabled);
	}
	if (CostText)
	{
		CostText->SetColorAndOpacity(FSlateColor(bEnabled ? DiscCellCostInk : DiscCellInkDisabled));
	}
	if (BtnText)  // base 소유 — 스트립 "강화" 라벨 (WBP 에서 BtnText 로 명명)
	{
		BtnText->SetColorAndOpacity(FSlateColor(bEnabled ? DiscCellLabelInk : DiscCellInkDisabled));
	}
	if (ValueText)
	{
		ValueText->SetColorAndOpacity(FSlateColor(bEnabled ? DiscCellValueInk : DiscCellValueDisabled));
	}
	if (IconImg)
	{
		IconImg->SetColorAndOpacity(bEnabled ? DiscCellGlyphInk : DiscCellGlyphDisabled);
	}
}
