#include "UI/Element/Common/IconBrushUtils.h"
#include "Components/Image.h"

void CGIconBrush::ApplyResource(UImage* Image, UObject* Resource, FVector2D ImageSize)
{
	if (!Image || !Resource) { return; }
	FSlateBrush Brush = Image->GetBrush();
	Brush.SetResourceObject(Resource);
	if (!ImageSize.IsZero())
	{
		Brush.ImageSize = ImageSize;
	}
	Image->SetBrush(Brush);
}
