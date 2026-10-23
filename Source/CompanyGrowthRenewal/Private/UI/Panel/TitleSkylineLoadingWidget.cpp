// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/TitleSkylineLoadingWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<float> CVarTitleIntroTimeScale(
		TEXT("CGR.TitleIntro.TimeScale"), 1.f,
		TEXT("스카이라인 로딩 로고 등장 연출의 시간 배율 (검수용 슬로모션 — 0.1 이면 10배 느리게)"));

	constexpr float IntroPieceSettleTime = 0.36f;
	constexpr float IntroBoardResponseTime = 0.25f;
	constexpr float IntroFinalResponseTime = 0.42f;
	// 출발점을 화면 가장자리에서 이만큼 더 바깥에 둔다
	constexpr float IntroOffscreenMargin = 40.f;

	float IntroClamp01(float X) { return FMath::Clamp(X, 0.f, 1.f); }

	// 구간 [Start, Start+Duration] 안의 진행도 0..1
	float IntroSegment(float T, float Start, float Duration) { return IntroClamp01((T - Start) / Duration); }

	float IntroEaseInQuad(float X) { return X * X; }
	float IntroSmooth(float X) { X = IntroClamp01(X); return X * X * (3.f - 2.f * X); }
	float IntroBetween(float T, float Start, float End, float A, float B)
	{
		return FMath::Lerp(A, B, IntroSmooth((T - Start) / (End - Start)));
	}

	float IntroImpact(float Since, float Amplitude, float Duration)
	{
		if (Since < 0.f || Since > Duration)
		{
			return 0.f;
		}
		if (Since < Duration * 0.22f)
		{
			return IntroBetween(Since, 0.f, Duration * 0.22f, 0.f, Amplitude);
		}
		if (Since < Duration * 0.64f)
		{
			return IntroBetween(Since, Duration * 0.22f, Duration * 0.64f, Amplitude, -Amplitude * 0.38f);
		}
		return IntroBetween(Since, Duration * 0.64f, Duration, -Amplitude * 0.38f, 0.f);
	}

	float IntroFinalImpact(float Since)
	{
		if (Since < 0.f || Since > IntroFinalResponseTime)
		{
			return 0.f;
		}
		if (Since < 0.06f) return IntroBetween(Since, 0.f, 0.06f, 0.f, 1.f);
		if (Since < 0.17f) return IntroBetween(Since, 0.06f, 0.17f, 1.f, -0.7f);
		if (Since < 0.26f) return IntroBetween(Since, 0.17f, 0.26f, -0.7f, 0.28f);
		if (Since < 0.34f) return IntroBetween(Since, 0.26f, 0.34f, 0.28f, -0.18f);
		return IntroBetween(Since, 0.34f, IntroFinalResponseTime, -0.18f, 0.f);
	}

	void PoseIntroPiece(UWidget* Target, float Opacity, const FVector2D& Translation = FVector2D::ZeroVector,
		const FVector2D& Scale = FVector2D::UnitVector, float Angle = 0.f)
	{
		if (!Target)
		{
			return;
		}
		Target->SetRenderOpacity(Opacity);
		Target->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		FWidgetTransform Transform;
		Transform.Translation = Translation;
		Transform.Scale = Scale;
		Transform.Angle = Angle;
		Target->SetRenderTransform(Transform);
	}
}

void UTitleSkylineLoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bConstructed = true;
	ScreenSize = FVector2D::ZeroVector;
	ResetIntro();
}

void UTitleSkylineLoadingWidget::SetLogoVariant(ETitleLogoVariant InVariant)
{
	LogoVariant = InVariant;
	if (bConstructed)
	{
		ResetIntro();
	}
}

void UTitleSkylineLoadingWidget::ReplayIntro()
{
	if (bConstructed)
	{
		ResetIntro();
	}
}

void UTitleSkylineLoadingWidget::SelectLogoParts()
{
	const bool bHasBuildings3D = LogoClipD && LogoPlateD && LogoWordAD && LogoWordBD && LogoBrickD;
	const bool bUseBuildings3D = LogoVariant == ETitleLogoVariant::Buildings3D && bHasBuildings3D;
	if (LogoVariant == ETitleLogoVariant::Buildings3D && !bHasBuildings3D)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TitleSkyline] D안 조각(LogoClipD 등)이 WBP 에 없어 확정 로고로 보인다"));
	}

	Parts = bUseBuildings3D
		? FLogoParts{ LogoClipD, LogoPlateD, LogoWordAD, LogoWordBD, LogoBrickD }
		: FLogoParts{ LogoClip, LogoPlate, LogoWordA, LogoWordB, LogoBrick };
	bRestOnBadge = !bUseBuildings3D;
	if (UWidget* Unused = bUseBuildings3D ? LogoClip.Get() : LogoClipD.Get())
	{
		Unused->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTitleSkylineLoadingWidget::ResetIntro()
{
	SelectLogoParts();
	IntroClock = -IntroDelay;
	bIntroFinished = false;
	if (TitleLogo)
	{
		TitleLogo->SetRenderOpacity(0.f);
	}
	if (Parts.Clip)
	{
		Parts.Clip->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	ApplyIntro(0.f);
}

void UTitleSkylineLoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FVector2D ViewportSize = MyGeometry.GetLocalSize();
	if (!ViewportSize.Equals(ScreenSize) && ViewportSize.X > 0.f && ViewportSize.Y > 0.f)
	{
		ScreenSize = ViewportSize;
		ApplyResponsiveLayout(ViewportSize);
	}
	if (bIntroFinished)
	{
		return;
	}
	IntroClock += InDeltaTime * FMath::Max(0.f, CVarTitleIntroTimeScale.GetValueOnGameThread());
	const float T = FMath::Max(0.f, IntroClock);
	if (T >= GetIntroDuration())
	{
		FinishIntro();
		return;
	}
	ApplyIntro(T);
}

void UTitleSkylineLoadingWidget::ApplyResponsiveLayout(const FVector2D& ViewportSize)
{
	if (BackgroundImage)
	{
		if (UCanvasPanelSlot* BackgroundSlot = Cast<UCanvasPanelSlot>(BackgroundImage->Slot))
		{
			const FVector2D ArtSize = BackgroundImage->GetBrush().GetImageSize();
			if (ArtSize.X > 0.f && ArtSize.Y > 0.f)
			{
				const float FillScale = FMath::Max(ViewportSize.X / ArtSize.X, ViewportSize.Y / ArtSize.Y);
				BackgroundSlot->SetAnchors(FAnchors(BackgroundCenterUV.X, BackgroundCenterUV.Y));
				BackgroundSlot->SetAlignment(BackgroundCenterUV);
				BackgroundSlot->SetPosition(FVector2D::ZeroVector);
				BackgroundSlot->SetSize(ArtSize * FillScale);
				// 구름도 같은 그림 좌표를 사용해 화면비가 바뀔 때 산·건물 위로 내려오지 않는다.
				const FVector2D DrawSize = ArtSize * FillScale;
				const FVector2D ArtOrigin = (ViewportSize - DrawSize) * BackgroundCenterUV;
				for (UImage* Cloud : { CloudA.Get(), CloudB.Get() })
				{
					if (UCanvasPanelSlot* CloudSlot = Cloud ? Cast<UCanvasPanelSlot>(Cloud->Slot) : nullptr)
					{
						const bool bFirstCloud = Cloud == CloudA.Get();
						const FVector2D CloudUV = bFirstCloud ? FVector2D(0.47, 0.105) : FVector2D(0.80, 0.18);
						const FVector2D CloudArt = Cloud->GetBrush().GetImageSize();
						const double Width = DrawSize.X * (bFirstCloud ? 0.15 : 0.105);
						CloudSlot->SetAnchors(FAnchors(0.5f, 0.5f));
						CloudSlot->SetAlignment(FVector2D(0.5f, 0.5f));
						CloudSlot->SetPosition(ArtOrigin + DrawSize * CloudUV - ViewportSize * 0.5);
						CloudSlot->SetSize(FVector2D(Width, CloudArt.X > 0 ? Width * CloudArt.Y / CloudArt.X : Width * 0.5));
					}
				}
			}
		}
	}

	const float LogoWidth = FMath::Min(ViewportSize.X * LogoWidthFraction,
		ViewportSize.Y * LogoCanvasAspect * LogoWidthFraction);
	const FAnchors LogoAnchors(LogoCenterUV.X, LogoCenterUV.Y);
	for (UWidget* LogoWidget : { TitleLogo.Get(), LogoClip.Get(), LogoClipD.Get() })
	{
		if (UCanvasPanelSlot* LogoSlot = LogoWidget ? Cast<UCanvasPanelSlot>(LogoWidget->Slot) : nullptr)
		{
			const float BadgeAspect = LogoWidget == LogoClipD.Get() ? LogoBadgeAspectD : LogoBadgeAspect;
			LogoSlot->SetAnchors(LogoAnchors);
			LogoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			LogoSlot->SetPosition(FVector2D::ZeroVector);
			LogoSlot->SetSize(FVector2D(LogoWidth, LogoWidth * BadgeAspect));
		}
	}
}

void UTitleSkylineLoadingWidget::GetDrops(TArray<FPieceDrop, TInlineAllocator<4>>& OutDrops) const
{
	// 판 → 「회사」 → 「키우기」 → 벽돌 순서로 떨어져 쌓인다. 벽돌만 살짝 기운 채 떨어져 바로 선다.
	const auto MakeDrop = [](UImage* Piece, float Arrive, float Fall, float StartAngle, float Jolt)
	{
		FPieceDrop Drop;
		Drop.Piece = Piece;
		Drop.Arrive = Arrive;
		Drop.Fall = Fall;
		Drop.StartAngle = StartAngle;
		Drop.Jolt = Jolt;
		return Drop;
	};

	OutDrops.Reset();
	OutDrops.Add(MakeDrop(Parts.Plate, 0.28f, 0.28f, 0.f, 0.f));
	OutDrops.Add(MakeDrop(Parts.WordA, 0.60f, 0.27f, 0.f, 3.f));
	OutDrops.Add(MakeDrop(Parts.WordB, 0.92f, 0.27f, 0.f, 4.f));
	OutDrops.Add(MakeDrop(Parts.Brick, 1.25f, 0.30f, -20.f, 6.f));
}

float UTitleSkylineLoadingWidget::GetIntroDuration() const
{
	TArray<FPieceDrop, TInlineAllocator<4>> Drops;
	GetDrops(Drops);
	float LastArrive = 0.f;
	for (const FPieceDrop& Drop : Drops)
	{
		LastArrive = FMath::Max(LastArrive, Drop.Arrive);
	}
	return LastArrive + FMath::Max(IntroPieceSettleTime, IntroFinalResponseTime);
}

void UTitleSkylineLoadingWidget::ApplyIntro(float T)
{
	TArray<FPieceDrop, TInlineAllocator<4>> Drops;
	GetDrops(Drops);
	float JoltY = 0.f;
	for (const FPieceDrop& Drop : Drops)
	{
		PoseFallingPiece(Drop, T);
		JoltY += IntroImpact(T - Drop.Arrive, Drop.Jolt, IntroBoardResponseTime);
	}
	const float FinalSince = T - Drops.Last().Arrive;
	const float FinalImpact = IntroFinalImpact(FinalSince);
	const UCanvasPanelSlot* ClipSlot = Parts.Clip ? Cast<UCanvasPanelSlot>(Parts.Clip->Slot) : nullptr;
	const float ImpactScale = ClipSlot ? ClipSlot->GetSize().X / 1024.f : 1.f;
	PoseIntroPiece(Parts.Clip, 1.f, FVector2D(0.f, (JoltY + FinalImpact * 10.f) * ImpactScale),
		FVector2D(1.f + FinalImpact * 0.035f, 1.f - FinalImpact * 0.045f),
		IntroImpact(T - Drops[1].Arrive, 0.7f, IntroBoardResponseTime)
		+ IntroImpact(T - Drops[2].Arrive, -0.9f, IntroBoardResponseTime)
		+ FinalImpact * 1.5f);
}

bool UTitleSkylineLoadingWidget::GetPieceLayout(const UImage* Piece, FVector2D& OutCenter, FVector2D& OutHalfSize,
	float& OutBadgeToScreen) const
{
	const UCanvasPanelSlot* ClipSlot = Parts.Clip ? Cast<UCanvasPanelSlot>(Parts.Clip->Slot) : nullptr;
	const UOverlaySlot* PieceSlot = Piece ? Cast<UOverlaySlot>(Piece->Slot) : nullptr;
	if (!ClipSlot || !PieceSlot || ScreenSize.X <= 0.f || ScreenSize.Y <= 0.f)
	{
		return false;
	}

	// 조각 슬롯의 네 변 여백 + 이미지 크기 = 배지 캔버스 — 저작 도구가 이 방식으로 배지 좌표를 고정한다
	const FMargin Pad = PieceSlot->GetPadding();
	const FVector2D PieceSize = Piece->GetBrush().GetImageSize();
	const FVector2D Badge(Pad.Left + PieceSize.X + Pad.Right, Pad.Top + PieceSize.Y + Pad.Bottom);
	const FVector2D ClipSize = ClipSlot->GetSize();
	if (Badge.X <= 0.f || Badge.Y <= 0.f)
	{
		return false;
	}
	OutBadgeToScreen = static_cast<float>(FMath::Min(ClipSize.X / Badge.X, ClipSize.Y / Badge.Y));
	const FVector2D ClipTopLeft = ClipSlot->GetAnchors().Minimum * ScreenSize + ClipSlot->GetPosition()
		- ClipSlot->GetAlignment() * ClipSize;
	OutCenter = ClipTopLeft + (FVector2D(Pad.Left, Pad.Top) + PieceSize * 0.5f) * OutBadgeToScreen;
	OutHalfSize = PieceSize * 0.5f * OutBadgeToScreen;
	return OutBadgeToScreen > 0.f;
}

void UTitleSkylineLoadingWidget::PoseFallingPiece(const FPieceDrop& Drop, float T)
{
	const float Launch = Drop.Arrive - Drop.Fall;
	FVector2D Center;
	FVector2D Half;
	float BadgeToScreen = 0.f;
	// 아직 떨어지기 전이거나 화면 크기를 모르면(첫 틱 전) 숨겨 둔다
	if (!Drop.Piece || T < Launch || !GetPieceLayout(Drop.Piece, Center, Half, BadgeToScreen))
	{
		PoseIntroPiece(Drop.Piece, 0.f);
		return;
	}

	// 출발점 = 화면 위쪽 가장자리 밖. 기운 채 떨어지는 조각도 다 가려지게 반 대각선 + 여유만큼 올린다.
	const double Height = Center.Y + Half.Size() + IntroOffscreenMargin;
	// 떨어질수록 빨라져 붙는 순간이 가장 빠르다 — 박히는 속도감
	const float Remain = 1.f - IntroEaseInQuad(IntroSegment(T, Launch, Drop.Fall));
	float ScaleX = 1.f;
	float ScaleY = 1.f;
	float BounceY = 0.f;
	if (T >= Drop.Arrive && T < Drop.Arrive + IntroPieceSettleTime)
	{
		const float Since = T - Drop.Arrive;
		if (Since < 0.065f)
		{
			ScaleX = IntroBetween(Since, 0.f, 0.065f, 1.f, 1.105f);
			ScaleY = IntroBetween(Since, 0.f, 0.065f, 1.f, 0.865f);
			BounceY = IntroBetween(Since, 0.f, 0.065f, 0.f, 5.f);
		}
		else if (Since < 0.17f)
		{
			ScaleX = IntroBetween(Since, 0.065f, 0.17f, 1.105f, 0.965f);
			ScaleY = IntroBetween(Since, 0.065f, 0.17f, 0.865f, 1.065f);
			BounceY = IntroBetween(Since, 0.065f, 0.17f, 5.f, -8.f);
		}
		else if (Since < 0.25f)
		{
			ScaleX = IntroBetween(Since, 0.17f, 0.25f, 0.965f, 1.025f);
			ScaleY = IntroBetween(Since, 0.17f, 0.25f, 1.065f, 0.975f);
			BounceY = IntroBetween(Since, 0.17f, 0.25f, -8.f, 2.f);
		}
		else if (Since < 0.305f)
		{
			ScaleX = IntroBetween(Since, 0.25f, 0.305f, 1.025f, 0.99f);
			ScaleY = IntroBetween(Since, 0.25f, 0.305f, 0.975f, 1.018f);
			BounceY = IntroBetween(Since, 0.25f, 0.305f, 2.f, -2.f);
		}
		else
		{
			ScaleX = IntroBetween(Since, 0.305f, IntroPieceSettleTime, 0.99f, 1.f);
			ScaleY = IntroBetween(Since, 0.305f, IntroPieceSettleTime, 1.018f, 1.f);
			BounceY = IntroBetween(Since, 0.305f, IntroPieceSettleTime, -2.f, 0.f);
		}
	}
	// 조각은 로고 상자의 ScaleBox 안에 있어 화면 단위 이동량을 배지 좌표로 바꿔 준다
	PoseIntroPiece(Drop.Piece, 1.f, FVector2D(0.0, (-Height * Remain + BounceY) / BadgeToScreen), FVector2D(ScaleX, ScaleY),
		Drop.StartAngle * Remain);
}

void UTitleSkylineLoadingWidget::FinishIntro()
{
	bIntroFinished = true;
	for (UImage* Piece : { Parts.Plate, Parts.WordA, Parts.WordB, Parts.Brick })
	{
		PoseIntroPiece(Piece, 1.f);
	}
	PoseIntroPiece(Parts.Clip, 1.f);
	if (bRestOnBadge)
	{
		if (TitleLogo)
		{
			TitleLogo->SetRenderOpacity(1.f);
		}
		if (Parts.Clip)
		{
			Parts.Clip->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
