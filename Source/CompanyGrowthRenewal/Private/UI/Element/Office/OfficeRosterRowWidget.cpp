#include "UI/Element/Office/OfficeRosterRowWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"

void UOfficeRosterRowWidget::SetLine(UTexture2D* Icon, int32 Headcount, const FText& PowerLabel)
{
	const bool bNobody = Headcount == 0;
	DisciplineIcon->SetBrushFromTexture(Icon, /*bMatchSize*/ false);
	DisciplineIcon->SetRenderOpacity(bNobody ? ZeroIconOpacity : 1.f);
	CountText->SetText(FText::AsNumber(Headcount));
	CountText->SetColorAndOpacity(bNobody ? ZeroCountInk : CountInk);
	UnitText->SetColorAndOpacity(bNobody ? ZeroCountInk : UnitInk);
	PowerText->SetText(PowerLabel);
	PowerText->SetRenderOpacity(bNobody ? ZeroPowerOpacity : 1.f);
}
