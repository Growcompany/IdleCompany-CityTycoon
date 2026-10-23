#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Entity/Officeworker/PortraitOutline.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌
namespace PortraitOutlineTests
{
	constexpr int32 GridSize = 64;
	constexpr int32 CenterXY = 32;
	constexpr float CircleRadius = 20.f;
	const FColor FillColor(200, 150, 100, 255);

	float DistanceFromCenter(int32 X, int32 Y)
	{
		const int32 Dx = X - CenterXY;
		const int32 Dy = Y - CenterXY;
		return FMath::Sqrt(static_cast<float>(Dx * Dx + Dy * Dy));
	}

	TArray<FColor> MakeCircle()
	{
		TArray<FColor> Result;
		Result.SetNum(GridSize * GridSize);
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			for (int32 X = 0; X < GridSize; ++X)
			{
				const uint8 Coverage = DistanceFromCenter(X, Y) <= CircleRadius ? static_cast<uint8>(255) : static_cast<uint8>(0);
				Result[Y * GridSize + X] = FColor(FillColor.R, FillColor.G, FillColor.B, Coverage);
			}
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRPortraitAlphaOutlineTest, "CGR.Portrait.AlphaOutline",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRPortraitAlphaOutlineTest::RunTest(const FString& Parameters)
{
	using namespace PortraitOutlineTests;

	const FPortraitOutlineParams Params;  // 기본값 = 튜너 확정(1.5px / 0.8 / sRGB #615A5A)
	const TArray<FColor> Original = MakeCircle();
	TArray<FColor> Pixels = Original;
	PortraitOutline::Apply(Pixels, GridSize, GridSize, Params);

	// (a) 원 밖 외곽선 밴드(반경 20 초과 ~ 21 이내의 격자 링) = 불투명한 선색
	int32 BandSamples = 0;
	int32 BandFailures = 0;
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			const float Distance = DistanceFromCenter(X, Y);
			if (Distance <= CircleRadius || Distance > CircleRadius + 1.f) { continue; }
			++BandSamples;
			const FColor& Edge = Pixels[Y * GridSize + X];
			const bool bInk = FMath::Abs(static_cast<int32>(Edge.R) - static_cast<int32>(Params.Color.R)) <= 8
				&& FMath::Abs(static_cast<int32>(Edge.G) - static_cast<int32>(Params.Color.G)) <= 8
				&& FMath::Abs(static_cast<int32>(Edge.B) - static_cast<int32>(Params.Color.B)) <= 8;
			if (Edge.A < 150 || !bInk) { ++BandFailures; }
		}
	}
	TestTrue(TEXT("(a) 외곽선 밴드 표본이 존재"), BandSamples > 0);
	TestEqual(TEXT("(a) 원 밖 밴드 = A >= 150 + RGB 가 선색과 채널별 8 이내"), BandFailures, 0);

	// (b) 원 중심 = 이웃이 전부 커버리지 1 이라 에지 0
	const int32 CenterIndex = CenterXY * GridSize + CenterXY;
	TestTrue(TEXT("(b) 원 중심 = 원본 그대로"), Pixels[CenterIndex] == Original[CenterIndex]);

	// (c) 반경 30 = 밴드 밖
	const FColor& Outside = Pixels[CenterXY * GridSize + (CenterXY + 30)];
	TestEqual(TEXT("(c) 반경 30 = A 0"), static_cast<int32>(Outside.A), 0);
	TestTrue(TEXT("(c) 반경 30 = RGB 원본"), Outside.R == FillColor.R && Outside.G == FillColor.G && Outside.B == FillColor.B);

	// (d) Opacity 0 = no-op
	FPortraitOutlineParams Disabled;
	Disabled.Opacity = 0.f;
	TArray<FColor> Untouched = Original;
	PortraitOutline::Apply(Untouched, GridSize, GridSize, Disabled);
	TestTrue(TEXT("(d) Opacity 0 = 전 픽셀 원본과 동일"), Untouched == Original);

	// (e) 원 안 반경 19 = 알파는 불투명 유지, 색만 선색 쪽으로 어두워짐
	const FColor& Inner = Pixels[CenterXY * GridSize + (CenterXY + 19)];
	TestEqual(TEXT("(e) 원 안 반경 19 = A 255 유지"), static_cast<int32>(Inner.A), 255);
	TestTrue(TEXT("(e) 원 안 반경 19 = RGB 가 원본과 선색 사이(어두워짐)"),
		Inner.R < FillColor.R && Inner.R >= Params.Color.R
		&& Inner.G < FillColor.G && Inner.G >= Params.Color.G
		&& Inner.B < FillColor.B && Inner.B >= Params.Color.B);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRPortraitAlphaOutlineProbeTest, "CGR.Portrait.AlphaOutlineProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRPortraitAlphaOutlineProbeTest::RunTest(const FString& Parameters)
{
	const FString SourceDirectory = FPaths::ProjectSavedDir() / TEXT("Portraits");
	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *(SourceDirectory / TEXT("*.png")), true, false);
	if (FileNames.Num() == 0)
	{
		AddInfo(FString::Printf(TEXT("초상 PNG 없음 — 건너뜀 (%s)"), *SourceDirectory));
		return true;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const FString OutputDirectory = FPaths::ProjectSavedDir() / TEXT("PortraitOutlineProbe");
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);

	int32 WrittenCount = 0;
	for (const FString& FileName : FileNames)
	{
		TArray<uint8> Encoded;
		if (!FFileHelper::LoadFileToArray(Encoded, *(SourceDirectory / FileName)))
		{
			AddError(FString::Printf(TEXT("읽기 실패 %s"), *FileName));
			continue;
		}

		TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		TArray64<uint8> RawData;
		if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Encoded.GetData(), Encoded.Num())
			|| !Wrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
		{
			AddError(FString::Printf(TEXT("PNG 디코드 실패 %s"), *FileName));
			continue;
		}

		const int32 ImageWidth = static_cast<int32>(Wrapper->GetWidth());
		const int32 ImageHeight = static_cast<int32>(Wrapper->GetHeight());
		if (RawData.Num() < static_cast<int64>(ImageWidth) * ImageHeight * 4)
		{
			AddError(FString::Printf(TEXT("픽셀 수 부족 %s"), *FileName));
			continue;
		}

		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(ImageWidth * ImageHeight);
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			const uint8* Source = RawData.GetData() + Index * 4;  // BGRA8
			Pixels[Index] = FColor(Source[2], Source[1], Source[0], Source[3]);
		}

		FPortraitOutlineParams Params;
		Params.ThicknessPx = 1.5f;
		PortraitOutline::Apply(Pixels, ImageWidth, ImageHeight, Params);

		TArray64<uint8> Compressed;
		FImageUtils::PNGCompressImageArray(ImageWidth, ImageHeight, Pixels, Compressed);
		if (FFileHelper::SaveArrayToFile(Compressed, *(OutputDirectory / FileName)))
		{
			++WrittenCount;
		}
		else
		{
			AddError(FString::Printf(TEXT("저장 실패 %s"), *FileName));
		}
	}

	AddInfo(FString::Printf(TEXT("외곽선 프로브 %d/%d 장 저장 — %s"), WrittenCount, FileNames.Num(), *OutputDirectory));
	return true;
}

#endif
