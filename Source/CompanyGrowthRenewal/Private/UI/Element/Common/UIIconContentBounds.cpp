#include "UI/Element/Common/UIIconContentBounds.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

FVector2D FUIIconContentFit::FitSize(FVector2D Box, FVector2D ContentAspectSize)
{
	if (ContentAspectSize.X <= 0.0 || ContentAspectSize.Y <= 0.0)
	{
		return Box;
	}
	const double Scale = FMath::Min(Box.X / ContentAspectSize.X, Box.Y / ContentAspectSize.Y);
	return ContentAspectSize * Scale;
}

bool FUIIconContentFit::Apply(UImage* Image, UObject* Resource, FVector2D Box)
{
	if (!Image)
	{
		return false;
	}

	FSlateBrush Brush = Image->GetBrush();
	if (Resource)
	{
		Brush.SetResourceObject(Resource);
	}

	const UUIIconContentBounds* Bounds = nullptr;
	FVector2D TextureSize(1.0, 1.0);
	if (UTexture2D* Texture = Cast<UTexture2D>(Resource))
	{
		Bounds = Texture->GetAssetUserData<UUIIconContentBounds>();
		// 크기를 모르면(플랫폼 데이터 없음) 정사각으로 보고 비율만 그림 상자에서 얻는다
		if (Texture->GetSizeX() > 0 && Texture->GetSizeY() > 0)
		{
			TextureSize = FVector2D(static_cast<double>(Texture->GetSizeX()), static_cast<double>(Texture->GetSizeY()));
		}
	}

	const bool bHasContent = Bounds && Bounds->Max.X > Bounds->Min.X && Bounds->Max.Y > Bounds->Min.Y;
	if (bHasContent)
	{
		const FVector2D UVMin(Bounds->Min);
		const FVector2D UVMax(Bounds->Max);
		Brush.SetUVRegion(FBox2d(UVMin, UVMax));
		Brush.SetImageSize(FitSize(Box, (UVMax - UVMin) * TextureSize));
	}
	else
	{
		// 정보 없음 = 텍스처 전체(머티리얼은 정사각)를 비율대로 ― 직사각 Box 에 늘이지 않는다
		Brush.SetUVRegion(FBox2d(ForceInit));
		Brush.SetImageSize(FitSize(Box, TextureSize));
	}
	Image->SetBrush(Brush);
	return bHasContent;
}
