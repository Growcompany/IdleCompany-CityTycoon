#pragma once

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

// 위젯 렌더 시험 공용 측정 도구 (FWidgetRenderer 하네스 · 픽셀 · 지오메트리)
namespace LiveOpsRenderTestUtils
{
	inline constexpr uint8 InkOpaqueAlpha = 128;

	inline const FColor* PixelAt(const FImage& Image, int32 X, int32 Y)
	{
		if (X < 0 || Y < 0 || X >= Image.SizeX || Y >= Image.SizeY)
		{
			return nullptr;
		}
		return reinterpret_cast<const FColor*>(Image.RawData.GetData()) + static_cast<int64>(Y) * Image.SizeX + X;
	}

	inline double Luma(double R, double G, double B) { return 0.2126 * R + 0.7152 * G + 0.0722 * B; }

	// 알림 점(Alert_Text_l_Red) 잉크 ― 묶음색 받침 테(도전 #EE7A52 섞임)도 걸릴 수 있어 찾는 창은 호출부가 좁힌다
	inline bool IsBadgeRed(const FColor& P)
	{
		return P.A >= InkOpaqueAlpha && P.R > 150 && P.G < 90 && P.B < 110;
	}

	// 커맨드릿(면 투명)과 에디터(면 래스터) 어느 문맥이든 같은 기준으로 글리프 유무를 센다.
	// bBright=false 면 Threshold 이하의 어두운 잉크(글리프 아웃라인) 를 센다.
	inline int32 CountInk(const FImage& Image, int32 Left, int32 Top, int32 Right, int32 Bottom, bool bBright,
		double Threshold)
	{
		int32 Count = 0;
		for (int32 Y = Top; Y < Bottom; ++Y)
		{
			for (int32 X = Left; X < Right; ++X)
			{
				const FColor* Pixel = PixelAt(Image, X, Y);
				if (Pixel == nullptr || Pixel->A < InkOpaqueAlpha)
				{
					continue;
				}
				const double PixelLuma = Luma(Pixel->R, Pixel->G, Pixel->B);
				if (bBright ? (PixelLuma >= Threshold) : (PixelLuma <= Threshold))
				{
					++Count;
				}
			}
		}
		return Count;
	}

	inline bool SaveTilePNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	inline bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, float Scale, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		// 첫 회는 레이아웃만 세운다 — desired size 가 정해지기 전 픽셀은 판정 대상이 아니다
		Renderer.DrawWidget(Target.Get(), Widget, Scale, DrawSize, 0.f);
		FlushRenderingCommands();
		Renderer.DrawWidget(Target.Get(), Widget, Scale, DrawSize, 0.f);
		FlushRenderingCommands();

		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		// FWidgetRenderer(true) 는 linear 로 태그된 타겟에 이미 감마 인코딩된 바이트를 써 둔다
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	// 큰 델타로 한 번 그려 등장 애니(알림 점 0.7 팝 등)를 끝낸다 ― DrawToImage 는 델타 0 이라 애니가 첫 프레임에 멈춰 있다
	inline bool SettleDraw(const TSharedRef<SWidget>& Widget, FIntPoint Size, float Scale)
	{
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		Renderer.DrawWidget(Target.Get(), Widget, Scale, FVector2D(Size.X, Size.Y), 1.f);
		FlushRenderingCommands();
		return true;
	}

	// 레이아웃 패스 뒤의 실측 배치 — 엔진 전역 FBox(TBox<double>) 와 이름이 겹치지 않게 따로 둔다
	struct FWidgetBox
	{
		bool bFound = false;
		double Top = 0.0, Left = 0.0, Width = 0.0, Height = 0.0;
		double Bottom() const { return Top + Height; }
		double Right() const { return Left + Width; }
	};

	inline FWidgetBox MeasureWidget(UUserWidget& Root, const TCHAR* Name, FVector2D Origin)
	{
		FWidgetBox Out;
		UWidget* Widget = Root.GetWidgetFromName(FName(Name));
		if (Widget == nullptr)
		{
			return Out;
		}
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D Position = FVector2D(Geometry.GetAbsolutePosition()) - Origin;
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		Out.bFound = true;
		Out.Left = Position.X;
		Out.Top = Position.Y;
		Out.Width = Size.X;
		Out.Height = Size.Y;
		return Out;
	}
}

#endif
