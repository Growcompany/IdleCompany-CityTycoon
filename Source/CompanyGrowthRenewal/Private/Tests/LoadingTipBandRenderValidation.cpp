#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "UI/Slate/SLevelTransitionOverlay.h"

/**
 * 전환 하단 팁 밴드의 **픽셀** 검증. 존재·크기 단언만으로는 글자 겹침/클립 잘림을 못 잡는다.
 * ⚠ 실제 RHI 가 필요하다 ― `-nullrhi` 에서는 렌더 타깃이 전부 0 이라 "어두운 밴드"와 "빈 화면"이 구별되지 않는다.
 *   그래서 먼저 블루 패널이 실제로 찍혔는지 확인하고, 아니면 **통과시키지 않고 에러**로 끝낸다.
 */
namespace LoadingTipBandRender
{
	constexpr int32 CanvasHeight = 1440;
	// UI_CREATION_PLAYBOOK §4.5 의 가상 가로 하한/중간/상한
	const int32 CanvasWidths[] = { 2304, 2560, 3120 };

	struct FPixels
	{
		FImage Image;

		// BGRA8 바이트를 FColor 로 보는 건 엔진이 제공한다 ― 손으로 오프셋 계산하면 채널이 뒤집힌다
		FColor At(int32 X, int32 Y) const
		{
			const TArrayView64<const FColor> Colors = Image.AsBGRA8();
			const int64 Index = static_cast<int64>(Y) * Image.SizeX + X;
			return Colors.IsValidIndex(Index) ? Colors[Index] : FColor(0, 0, 0, 0);
		}

		static int32 Luma(const FColor& C)
		{
			return (C.R * 299 + C.G * 587 + C.B * 114) / 1000;
		}
	};

	bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		// 첫 패스는 레이아웃만 세운다 (CodexRailRenderValidation 과 같은 관용구)
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, Pass == 0 ? 1.f : 0.f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage)
			|| OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	UFont* LoadNexon(bool bBold)
	{
		return TSoftObjectPtr<UFont>(FSoftObjectPath(bBold
			? TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font")
			: TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font")))
			.LoadSynchronous();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingTipBandRenderTest, "CGR.LoadingTip.BandRender",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLoadingTipBandRenderTest::RunTest(const FString& Parameters)
{
	using namespace LoadingTipBandRender;

	// ⚠ 날 포인터로 들면 그리기 전에 GC 가 가져간다 ― 위젯의 FSlateBrush 는 참조를 지켜주지 않는다.
	//   런타임은 GameInstance 의 UPROPERTY 가 들고 있어 안전하지만, 테스트는 여기서 직접 루팅해야 한다.
	//   (2026-10-07: 이걸 빠뜨려 "자리는 잡히고 전구만 안 그려지는" 증상을 두 번 쫓았다)
	TStrongObjectPtr<UFont> BodyFont(LoadNexon(false));
	TStrongObjectPtr<UFont> LabelFont(LoadNexon(true));
	TStrongObjectPtr<UTexture2D> BulbIcon(TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/TipBulb.TipBulb"))).LoadSynchronous());
	if (!BulbIcon.IsValid())
	{
		AddError(TEXT("TipBulb 로드 실패 ― Tools/LoadingTip/import_tip_bulb.py 를 먼저 돌릴 것"));
		return false;
	}
	if (!BodyFont.IsValid() || !LabelFont.IsValid())
	{
		AddError(TEXT("NEXON 폰트 로드 실패 ― 팁 밴드는 이 폰트 없이는 아예 안 그려진다"));
		return false;
	}

	// ── 원인 격리: 전구가 밴드에서 안 보일 때, 텍스처/브러시 자체가 그려지는지 맨 SImage 로 먼저 본다.
	//    여기서 0 이면 밴드 코드가 아니라 텍스처 쪽이고, 양수면 밴드의 배치/색 문제다.
	{
		FSlateBrush Probe;
		Probe.SetResourceObject(BulbIcon.Get());
		Probe.DrawAs = ESlateBrushDrawType::Image;
		Probe.SetImageSize(FVector2f(96.f, 96.f));
		FImage ProbeImage;
		if (DrawToImage(SNew(SImage).Image(&Probe), FIntPoint(96, 96), ProbeImage))
		{
			FPixels Probed;
			Probed.Image = MoveTemp(ProbeImage);
			int32 Opaque = 0;
			for (int32 Y = 0; Y < 96; ++Y)
			{
				for (int32 X = 0; X < 96; ++X)
				{
					Opaque += Probed.At(X, Y).A > 32 ? 1 : 0;
				}
			}
			AddInfo(FString::Printf(TEXT("[격리] TipBulb 단독 SImage 렌더 불투명 %d px"), Opaque));
		}
		else
		{
			AddInfo(TEXT("[격리] TipBulb 단독 렌더 자체가 실패"));
		}
	}

	// 저작 상한(40자)에 가까운 최장 문구로 재서 가장 빡빡한 경우를 본다
	const FText Tip = FText::FromString(TEXT("랭킹에 오른 다른 회사의 도시는 직접 방문할 수 있습니다."));

	const FString OutDir = FPaths::ProjectSavedDir() / TEXT("LoadingTipBand");
	for (const int32 Width : CanvasWidths)
	{
		FLevelTransitionSnapshot Snapshot;
		Snapshot.Phase = ELevelTransitionPhase::Holding;
		Snapshot.Progress = 1.0f;
		// 페이드인/스피너 깜빡임을 시간에 의존하지 않게 고정한다
		Snapshot.bReduceMotion = true;
		Snapshot.PatternStartTime = Snapshot.HoldStartTime = FPlatformTime::Seconds();

		TSharedPtr<FLevelTransitionVisualState, ESPMode::ThreadSafe> State =
			MakeShared<FLevelTransitionVisualState, ESPMode::ThreadSafe>();
		State->SetSnapshot(Snapshot);

		// PatternTexture 를 비워 배경을 단색 블루로 ― 패턴 스탬프가 섞이면 밴드 판정이 흐려진다
		const TSharedRef<SWidget> Overlay = SNew(SLevelTransitionOverlay)
			.VisualState(State)
			.PatternTexture(nullptr)
			.TipText(Tip)
			.TipBodyFont(BodyFont.Get())
			.TipLabelFont(LabelFont.Get())
			.TipIcon(BulbIcon.Get());

		FPixels Shot;
		if (!DrawToImage(Overlay, FIntPoint(Width, CanvasHeight), Shot.Image))
		{
			AddError(FString::Printf(TEXT("%d폭 렌더 실패 ― 실제 RHI 로 돌릴 것(-nullrhi 금지)"), Width));
			continue;
		}

		const FString Label = FString::Printf(TEXT("%dx%d"), Width, CanvasHeight);
		const int32 MidX = Width / 2;
		const int32 BandLeftPx = static_cast<int32>(Width * 0.090f);
		const int32 BandRightPx = static_cast<int32>(Width * 0.910f);
		const int32 FadePx = static_cast<int32>((BandRightPx - BandLeftPx) * 0.22f);

		// ── 전제: 블루 패널이 실제로 찍혔는가. 0 으로 가득 찬 이미지를 "밴드 어두움" 으로 읽으면 거짓 통과한다
		const FColor Pane = Shot.At(static_cast<int32>(Width * 0.05f),
			static_cast<int32>(CanvasHeight * 0.20f));
		if (Pane.B < 120 || Pane.B <= Pane.R)
		{
			AddError(FString::Printf(
				TEXT("%s: 배경이 블루 패널이 아니다 (R=%d G=%d B=%d) ― 판정 불가, nullRHI 의심"),
				*Label, Pane.R, Pane.G, Pane.B));
			continue;
		}
		const int32 PaneLuma = FPixels::Luma(Pane);

		// 한 행이 밴드인지 = 평평 구간을 훑어 **가장 어두운** 픽셀이 밴드색인가.
		// 단일 열로 재면 글리프의 밝은 픽셀에 걸려 밴드가 잘려 측정된다(2026-10-07 실측).
		// 글자는 밴드 위에만 찍히므로, 글자가 아무리 넓어져도 글자 사이 틈의 밴드색이 최소값으로 남는다.
		const auto RowIsBand = [&](int32 Y)
		{
			int32 MinLuma = 255;
			for (int32 X = BandLeftPx + FadePx; X <= BandRightPx - FadePx; X += 7)
			{
				MinLuma = FMath::Min(MinLuma, FPixels::Luma(Shot.At(X, Y)));
			}
			return MinLuma < PaneLuma - 40;
		};

		// ── 밴드 탐지: 화면 중앙 열을 아래에서 위로 올라가며 패널보다 확실히 어두운 구간을 찾는다
		int32 BandTop = -1;
		int32 BandBottom = -1;
		for (int32 Y = CanvasHeight - 1; Y >= CanvasHeight / 2; --Y)
		{
			const bool bDark = RowIsBand(Y);
			if (bDark && BandBottom < 0)
			{
				BandBottom = Y;
			}
			if (bDark)
			{
				BandTop = Y;
			}
			else if (BandBottom >= 0 && BandTop >= 0 && Y < BandTop - 2)
			{
				break;
			}
		}
		if (!TestTrue(*FString::Printf(TEXT("%s: 어두운 팁 밴드가 존재"), *Label),
			BandTop > 0 && BandBottom > BandTop))
		{
			continue;
		}

		const float BandBottomRatio = static_cast<float>(BandBottom) / CanvasHeight;
		const float BandHeightRatio = static_cast<float>(BandBottom - BandTop) / CanvasHeight;
		// 하단 여백 계약: 밴드 하단 0.930H (헤어라인 3px 포함 오차 허용)
		TestTrue(*FString::Printf(TEXT("%s: 밴드 하단 0.930H 근처 (실측 %.4f)"), *Label, BandBottomRatio),
			FMath::IsNearlyEqual(BandBottomRatio, 0.930f, 0.012f));
		TestTrue(*FString::Printf(TEXT("%s: 밴드 높이 5~13%%H (실측 %.4f)"), *Label, BandHeightRatio),
			BandHeightRatio > 0.05f && BandHeightRatio < 0.13f);
		// 스피너(0.46~0.54H)와 겹치지 않는가
		TestTrue(*FString::Printf(TEXT("%s: 밴드가 스피너 아래 (top %.4f)"),
			*Label, static_cast<float>(BandTop) / CanvasHeight),
			static_cast<float>(BandTop) / CanvasHeight > 0.60f);

		// ── 양끝 소멸: 밴드 가장자리는 패널색, 중앙은 어둡다
		const int32 BandMidY = (BandTop + BandBottom) / 2;
		const int32 EdgeX = BandLeftPx + 2;
		const int32 OutsideX = BandLeftPx - 8;
		TestTrue(*FString::Printf(TEXT("%s: 밴드 좌단이 패널색으로 소멸"), *Label),
			FPixels::Luma(Shot.At(EdgeX, BandMidY)) > PaneLuma - 22);
		TestTrue(*FString::Printf(TEXT("%s: 밴드 바깥은 순수 패널색"), *Label),
			FPixels::Luma(Shot.At(OutsideX, BandMidY)) > PaneLuma - 8);
		TestTrue(*FString::Printf(TEXT("%s: 밴드 평평구간은 어둡다"), *Label), RowIsBand(BandMidY));

		// ── 글자 잉크: 밴드 안의 밝은 픽셀 바운딩 박스
		int32 InkLeft = Width;
		int32 InkRight = -1;
		int32 InkTop = CanvasHeight;
		int32 InkBottom = -1;
		int32 InkCount = 0;
		int32 WarmCount = 0;
		int32 WarmLeft = Width;
		int32 WarmRight = -1;
		// 글자만의 좌단 ― 잉크 바운딩에는 전구가 섞여 있어 "전구가 글자 왼쪽"을 못 가린다
		int32 TextLeft = Width;
		for (int32 Y = BandTop + 3; Y <= BandBottom - 3; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const FColor C = Shot.At(X, Y);
				// 전구는 와이프 패널의 주황 틴트라 글자(거의 흰색)와 색으로 갈린다
				const bool bWarm = C.R > 190 && C.B + 60 < C.R;
				if (bWarm)
				{
					++WarmCount;
					WarmLeft = FMath::Min(WarmLeft, X);
					WarmRight = FMath::Max(WarmRight, X);
				}
				// 밴드는 #090D14 대역이라 밝은 픽셀은 글자뿐이다 (헤어라인은 ±3px 로 제외)
				if (FPixels::Luma(C) > 150)
				{
					++InkCount;
					if (!bWarm)
					{
						TextLeft = FMath::Min(TextLeft, X);
					}
					InkLeft = FMath::Min(InkLeft, X);
					InkRight = FMath::Max(InkRight, X);
					InkTop = FMath::Min(InkTop, Y);
					InkBottom = FMath::Max(InkBottom, Y);
				}
			}
		}
		if (!TestTrue(*FString::Printf(TEXT("%s: 밴드 안에 글자 잉크 존재 (count %d)"), *Label, InkCount),
			InkCount > 200))
		{
			continue;
		}

		// 전구를 안 그려도 글자만으로 위 단언들이 전부 통과한다 ― 존재를 따로 못 박는다
		TestTrue(*FString::Printf(TEXT("%s: 전구 글리프가 그려짐 (warm %d px)"), *Label, WarmCount),
			WarmCount > 300);
		TestTrue(*FString::Printf(TEXT("%s: 전구가 글자 왼쪽 (전구 %d~%d, 글자 %d~)"),
			*Label, WarmLeft, WarmRight, TextLeft),
			WarmRight > 0 && TextLeft < Width && WarmRight < TextLeft);

		const int32 InkCenterX = (InkLeft + InkRight) / 2;
		TestTrue(*FString::Printf(TEXT("%s: 글자가 가로 중앙 (중심 %d, 화면중앙 %d)"),
			*Label, InkCenterX, MidX),
			FMath::Abs(InkCenterX - MidX) < static_cast<int32>(Width * 0.02f));
		// 글자가 밴드를 넘지 않는가 ― 넘으면 블루 위로 삐져나와 읽기 어려워진다
		TestTrue(*FString::Printf(TEXT("%s: 글자가 밴드 가로 안쪽 (ink %d~%d)"), *Label, InkLeft, InkRight),
			InkLeft > BandLeftPx && InkRight < BandRightPx);
		// 라벨과 본문이 **한 줄**이다 ― 두 줄로 벌어지면 높이가 본문 글꼴의 2배를 넘는다
		const float InkHeightRatio = static_cast<float>(InkBottom - InkTop) / CanvasHeight;
		TestTrue(*FString::Printf(TEXT("%s: 글자가 한 줄 높이 (실측 %.4fH)"), *Label, InkHeightRatio),
			InkHeightRatio > 0.017f && InkHeightRatio < 0.045f);
		TestTrue(*FString::Printf(TEXT("%s: 글자 하단이 0.93H 위 (ink bottom %.4f)"),
			*Label, static_cast<float>(InkBottom) / CanvasHeight),
			static_cast<float>(InkBottom) / CanvasHeight < 0.930f);

		FImageUtils::SaveImageAutoFormat(*(OutDir / FString::Printf(TEXT("TipBand_%d.png"), Width)),
			Shot.Image);
		AddInfo(FString::Printf(
			TEXT("%s: band y=%d~%d (%.4f~%.4f) ink x=%d~%d y=%d~%d count=%d bulb x=%d~%d(%d px)"),
			*Label, BandTop, BandBottom, static_cast<float>(BandTop) / CanvasHeight,
			BandBottomRatio, InkLeft, InkRight, InkTop, InkBottom, InkCount,
			WarmLeft, WarmRight, WarmCount));
	}

	AddInfo(FString::Printf(TEXT("캡처 저장 위치: %s"), *OutDir));
	return true;
}

#endif
