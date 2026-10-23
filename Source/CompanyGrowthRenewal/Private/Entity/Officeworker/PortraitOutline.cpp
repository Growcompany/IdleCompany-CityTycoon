#include "Entity/Officeworker/PortraitOutline.h"

// 익명 ns 금지 — 유니티 TU 병합 시 같은 이름 헬퍼와 재정의 충돌
namespace PortraitOutlineInternal
{
	constexpr float DiagonalScale = 0.70710678f;  // 대각 탭이 축 탭과 같은 반경이 되도록 축별 1/√2

	// 소수 굵기를 지원하려고 t 를 정수 격자 두 샘플로 선형 보간한다.
	float SampleTap(const TArray<float>& Alpha, int32 Width, int32 Height, int32 X, int32 Y, int32 Dx, int32 Dy, float T)
	{
		const int32 Ti = FMath::FloorToInt32(T);
		const float Fr = T - static_cast<float>(Ti);
		auto At = [&](int32 Distance) -> float
		{
			const int32 Sx = X + Dx * Distance;
			const int32 Sy = Y + Dy * Distance;
			if (Sx < 0 || Sy < 0 || Sx >= Width || Sy >= Height) { return 0.f; }
			return Alpha[Sy * Width + Sx];
		};
		return At(Ti) * (1.f - Fr) + At(Ti + 1) * Fr;
	}

	uint8 MixChannel(uint8 Src, uint8 Ink, float K)
	{
		const float Mixed = static_cast<float>(Src) + (static_cast<float>(Ink) - static_cast<float>(Src)) * K;
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Mixed), 0, 255));
	}
}

void PortraitOutline::Apply(TArray<FColor>& Pixels, int32 Width, int32 Height, const FPortraitOutlineParams& Params)
{
	using namespace PortraitOutlineInternal;

	if (Width <= 0 || Height <= 0 || Pixels.Num() < Width * Height) { return; }
	if (Params.ThicknessPx <= 0.f || Params.Opacity <= 0.f) { return; }

	// 원본 알파 스냅샷 — 제자리 갱신한 알파를 이웃 탭이 다시 읽으면 외곽선이 번진다.
	TArray<float> Alpha;
	Alpha.SetNumUninitialized(Width * Height);
	for (int32 Index = 0; Index < Alpha.Num(); ++Index)
	{
		Alpha[Index] = static_cast<float>(Pixels[Index].A) / 255.f;
	}

	const float AxisT = Params.ThicknessPx;
	const float DiagT = Params.ThicknessPx * DiagonalScale;
	static const int32 AxisDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	static const int32 DiagDirs[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 Index = Y * Width + X;
			const float A = Alpha[Index];

			float Sum = 0.f;
			for (int32 Tap = 0; Tap < 4; ++Tap)
			{
				Sum += FMath::Abs(SampleTap(Alpha, Width, Height, X, Y, AxisDirs[Tap][0], AxisDirs[Tap][1], AxisT) - A);
			}
			for (int32 Tap = 0; Tap < 4; ++Tap)
			{
				Sum += 0.5f * FMath::Abs(SampleTap(Alpha, Width, Height, X, Y, DiagDirs[Tap][0], DiagDirs[Tap][1], DiagT) - A);
			}

			float Edge = FMath::Clamp(Sum, 0.f, 1.f);
			Edge = Edge * Edge * (3.f - 2.f * Edge);
			if (Edge <= 0.f) { continue; }

			const float K = Edge * (1.f + (Params.Opacity - 1.f) * A);
			FColor& Pixel = Pixels[Index];
			Pixel.R = MixChannel(Pixel.R, Params.Color.R, K);
			Pixel.G = MixChannel(Pixel.G, Params.Color.G, K);
			Pixel.B = MixChannel(Pixel.B, Params.Color.B, K);
			Pixel.A = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(FMath::Max(A, Edge * Params.Opacity) * 255.f), 0, 255));
		}
	}
}
