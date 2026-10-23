#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"
#include "UI/Element/Common/UIIconContentBounds.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace UIIconContentFitTests
{
// 엔진 TestEqual 에는 FVector2D 허용오차 판이 없다
void TestVec2(FAutomationTestBase& Test, const TCHAR* What, const FVector2D& Actual, const FVector2D& Expected, double Tolerance)
{
	Test.TestEqual(FString::Printf(TEXT("%s.X"), What), Actual.X, Expected.X, Tolerance);
	Test.TestEqual(FString::Printf(TEXT("%s.Y"), What), Actual.Y, Expected.Y, Tolerance);
}

FVector2D BrushSize(const UImage& Image)
{
	const FVector2f Size = Image.GetBrush().GetImageSize();
	return FVector2D(Size);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRUIIconContentFitTest, "CGR.UI.IconContentFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRUIIconContentFitTest::RunTest(const FString& Parameters)
{
	using namespace UIIconContentFitTests;

	const FVector2D Box(146.0, 108.0);

	// 가로형 248×146 → 가로가 먼저 닿는다
	TestVec2(*this, TEXT("Wide"), FUIIconContentFit::FitSize(Box, FVector2D(248.0, 146.0)), FVector2D(146.0, 146.0 * 146.0 / 248.0), 0.01);
	TestVec2(*this, TEXT("Square"), FUIIconContentFit::FitSize(Box, FVector2D(512.0, 512.0)), FVector2D(108.0, 108.0), 0.01);
	TestVec2(*this, TEXT("Degenerate"), FUIIconContentFit::FitSize(Box, FVector2D(0.0, 146.0)), Box, 0.01);

	TStrongObjectPtr<UTexture2D> Texture(UTexture2D::CreateTransient(288, 288));
	if (!TestNotNull(TEXT("transient texture"), Texture.Get()))
	{
		return false;
	}
	UUIIconContentBounds* Bounds = NewObject<UUIIconContentBounds>(Texture.Get());
	Bounds->Min = FVector2f(20.f / 288.f, 71.f / 288.f);
	Bounds->Max = FVector2f(268.f / 288.f, 217.f / 288.f);
	Texture->AddAssetUserData(Bounds);

	TStrongObjectPtr<UImage> Image(NewObject<UImage>(GetTransientPackage()));
	TestTrue(TEXT("Apply: bounds → true"), FUIIconContentFit::Apply(Image.Get(), Texture.Get(), Box));
	TestTrue(TEXT("Apply: resource set"), Image->GetBrush().GetResourceObject() == Texture.Get());
	{
		const FBox2f UV = Image->GetBrush().GetUVRegion();
		TestTrue(TEXT("Apply: UVRegion valid"), UV.bIsValid);
		TestVec2(*this, TEXT("Apply: UV Min"), FVector2D(UV.Min), FVector2D(20.0 / 288.0, 71.0 / 288.0), 1e-4);
		TestVec2(*this, TEXT("Apply: UV Max"), FVector2D(UV.Max), FVector2D(268.0 / 288.0, 217.0 / 288.0), 1e-4);
	}
	TestVec2(*this, TEXT("Apply: DesiredSize"), BrushSize(*Image), FVector2D(146.0, 146.0 * 146.0 / 248.0), 0.01);

	// 같은 이미지에 머티리얼을 다시 넣으면 앞서 자른 UV 가 남으면 안 된다
	TStrongObjectPtr<UMaterialInstanceDynamic> Mid(
		UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), GetTransientPackage()));
	TestFalse(TEXT("MaterialResourceFallsBack: false"), FUIIconContentFit::Apply(Image.Get(), Mid.Get(), Box));
	TestTrue(TEXT("MaterialResourceFallsBack: resource set"), Image->GetBrush().GetResourceObject() == Mid.Get());
	{
		const FBox2f UV = Image->GetBrush().GetUVRegion();
		TestFalse(TEXT("MaterialResourceFallsBack: UVRegion invalid"), UV.bIsValid);
	}
	// 정보 없음 = 직사각 Box 에 늘이지 않고 정사각(머티리얼)·텍스처 비율로
	TestVec2(*this, TEXT("MaterialResourceFallsBack: DesiredSize"), BrushSize(*Image), FVector2D(108.0, 108.0), 0.01);

	TStrongObjectPtr<UTexture2D> Plain(UTexture2D::CreateTransient(64, 64));
	TestFalse(TEXT("NoBoundsFallsBack: false"), FUIIconContentFit::Apply(Image.Get(), Plain.Get(), Box));
	{
		const FBox2f UV = Image->GetBrush().GetUVRegion();
		TestFalse(TEXT("NoBoundsFallsBack: UVRegion invalid"), UV.bIsValid);
	}
	TestVec2(*this, TEXT("NoBoundsFallsBack: DesiredSize"), BrushSize(*Image), FVector2D(108.0, 108.0), 0.01);

	TStrongObjectPtr<UTexture2D> WidePlain(UTexture2D::CreateTransient(288, 144));
	TestFalse(TEXT("NoBoundsWideFitsAspect: false"), FUIIconContentFit::Apply(Image.Get(), WidePlain.Get(), Box));
	{
		const FBox2f UV = Image->GetBrush().GetUVRegion();
		TestFalse(TEXT("NoBoundsWideFitsAspect: UVRegion invalid"), UV.bIsValid);
	}
	TestVec2(*this, TEXT("NoBoundsWideFitsAspect: DesiredSize"), BrushSize(*Image), FVector2D(146.0, 73.0), 0.01);

	return true;
}

#endif
