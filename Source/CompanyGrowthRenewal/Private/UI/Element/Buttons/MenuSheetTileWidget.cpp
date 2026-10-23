#include "UI/Element/Buttons/MenuSheetTileWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInterface.h"

void UMenuSheetTileWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 크기는 WBP 의 ImageSize 가 정한다 ― 리소스만 바꾼다
	if (IsValid(MedallionImage) && IsValid(MedallionMaterial))
	{
		MedallionImage->SetBrushFromMaterial(MedallionMaterial);
	}

	// 아이콘 리소스는 Super 가 IconImage 에 넣었다 ― 같은 리소스를 잠금 글리프에 옮긴다. 비면 흰 사각이 그려지니 접는다
	if (IsValid(LockedIconImage))
	{
		UObject* GlyphResource = IsValid(IconImage) ? IconImage->GetBrush().GetResourceObject() : nullptr;
		LockedIconImage->SetBrushResourceObject(GlyphResource);
		LockedIconImage->SetVisibility(GlyphResource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
