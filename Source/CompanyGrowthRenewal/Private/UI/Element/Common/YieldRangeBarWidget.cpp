// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/YieldRangeBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/UISoundTags.h"

namespace
{
	// 기댓값이 본전 미만 = 평균 손해 도박이라 마커를 경고색으로 (#C22B2E)
	const FLinearColor HighRiskRed(0.540f, 0.024f, 0.027f, 1.f);

	// 중심 팬 삼각분할 채움 (화이트 브러시 — 정점색이 곧 표시색)
	void FillPolyFan(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& Center, const TArray<FVector2D>& Outer, const FLinearColor& Color)
	{
		if (Outer.Num() < 3)
		{
			return;
		}

		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
		const FSlateRenderTransform& RT = Geo.ToPaintGeometry().GetAccumulatedRenderTransform();
		const FColor VertColor = Color.ToFColor(true);

		TArray<FSlateVertex> Verts;
		Verts.Reserve(Outer.Num() + 1);
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Center), FVector2f(0.f, 0.f), VertColor));
		for (const FVector2D& P : Outer)
		{
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(P), FVector2f(0.f, 0.f), VertColor));
		}

		TArray<SlateIndex> Indexes;
		Indexes.Reserve(Outer.Num() * 3);
		for (int32 i = 0; i < Outer.Num(); ++i)
		{
			Indexes.Add(0);
			Indexes.Add(1 + i);
			Indexes.Add(1 + (i + 1) % Outer.Num());
		}
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
	}

	// 라운드 사각 채움 — 모서리 4개를 각 6분할한 외곽 점열 + 중심 팬
	void FillRoundedRect(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& Geo,
		const FVector2D& TopLeft, const FVector2D& BottomRight, float Radius, const FLinearColor& Color)
	{
		const float W = static_cast<float>(BottomRight.X - TopLeft.X);
		const float H = static_cast<float>(BottomRight.Y - TopLeft.Y);
		if (W <= 0.f || H <= 0.f)
		{
			return;
		}

		const float R = FMath::Clamp(Radius, 0.f, FMath::Min(W, H) * 0.5f);
		const FVector2D CornerCenters[4] = {
			FVector2D(TopLeft.X + R,     TopLeft.Y + R),
			FVector2D(BottomRight.X - R, TopLeft.Y + R),
			FVector2D(BottomRight.X - R, BottomRight.Y - R),
			FVector2D(TopLeft.X + R,     BottomRight.Y - R)
		};
		// 화면 y 하향 좌표계 기준 시계방향 (좌상 → 우상 → 우하 → 좌하)
		const float StartAngles[4] = { PI, -HALF_PI, 0.f, HALF_PI };

		constexpr int32 CornerSegments = 6;
		TArray<FVector2D> Outer;
		Outer.Reserve(4 * (CornerSegments + 1));
		for (int32 c = 0; c < 4; ++c)
		{
			for (int32 s = 0; s <= CornerSegments; ++s)
			{
				const float Angle = StartAngles[c] + (HALF_PI * s) / float(CornerSegments);
				Outer.Add(CornerCenters[c] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
			}
		}

		FillPolyFan(OutDrawElements, Layer, Geo, (TopLeft + BottomRight) * 0.5f, Outer, Color);
	}
}

void UYieldRangeBarWidget::SetYieldRange(int32 InMinPct, int32 InMaxPct, int32 InEvPct, const FLinearColor& InGradeColor)
{
	MinPct = FMath::Max(0, InMinPct);
	MaxPct = FMath::Max(MinPct, InMaxPct);
	EvPct = InEvPct;
	GradeColor = InGradeColor;
	GradeColor.A = 1.f;
	bHasData = true;

	// 위젯 풀 재사용 — 리셋하지 않으면 이전 회사의 니들/결과 라벨이 굴리기도 전에 보인다
	bRevealActive = false;
	bRevealAnimating = false;
	RevealElapsed = 0.f;
	RevealPct = 0;
	NeedleDisplayPct = static_cast<float>(MinPct);
	bLandSoundPlayed = false;

	if (TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		Cached->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UYieldRangeBarWidget::PlayResultReveal(int32 RolledPct)
{
	// 데이터 없이 호출되면 그릴 축이 없다 — 소비자가 멈추지 않도록 즉시 완료 통지
	if (!bHasData)
	{
		OnRevealFinished.Broadcast();
		return;
	}

	RevealPct = FMath::Clamp(RolledPct, MinPct, MaxPct);
	bRevealActive = true;
	RevealElapsed = 0.f;
	NeedleDisplayPct = static_cast<float>(MinPct);
	bLandSoundPlayed = false;

	// 스윕 훑는 소리 — 가챠 스핀음(GachaPull)은 톤이 튀어 GachaShine(반짝임)으로 교체
	if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
	{
		SoundMgr->PlayUISound(CGUISoundTags::GachaShine);
	}

	// 구간 폭이 0 이면 훑을 곳이 없다 — 제자리에서 떠는 것처럼 보이므로 스윕을 건너뛴다
	bRevealAnimating = true;
	if (MaxPct <= MinPct)
	{
		RevealElapsed = SweepDuration;
	}
}

TSharedRef<SWidget> UYieldRangeBarWidget::RebuildWidget()
{
	// 자가 SizeBox 루트 (WBP 빈 트리 전제 — DisciplineRadar 패턴)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("YieldBarRoot"));
		Root->SetWidthOverride(BarWidth);
		Root->SetHeightOverride(BarHeight);
		WidgetTree->RootWidget = Root;
	}

	if (!LabelFont.FontObject)
	{
		TSoftObjectPtr<UFont> FontPath = TSoftObjectPtr<UFont>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font")));
		LabelFont = FSlateFontInfo(FontPath.LoadSynchronous(), 21);
	}

	return Super::RebuildWidget();
}

void UYieldRangeBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bRevealAnimating)
	{
		return;
	}

	RevealElapsed += InDeltaTime;

	const float SweepEnd = SweepDuration;
	const float LandEnd = SweepEnd + LandDuration;
	const float TotalEnd = LandEnd + ImpactDuration;

	const float Target = static_cast<float>(RevealPct);
	const float Lo = static_cast<float>(MinPct);
	const float Hi = static_cast<float>(FMath::Max(MaxPct, MinPct));

	if (RevealElapsed < SweepEnd)
	{
		// 구간을 왕복하며 감속 — 삼각파의 진폭을 남은 시간 비율로 줄인다
		const float T = RevealElapsed / SweepEnd;
		const float Phase = FMath::Frac(T * static_cast<float>(SweepCycles));
		const float Tri = 1.f - FMath::Abs(Phase * 2.f - 1.f);
		// T=1 에서 옛 Damp(1-T)=0 이면 스윕이 이미 Target 에 정확히 수렴해 착지 페이즈가 무동작(0.35초 데드비트)이 된다 — 0.15 를 잔류시켜 착지가 그 몫을 수렴시키게 함. 계수는 PIE 실측으로 확정할 것
		const float Damp = 0.15f + 0.85f * (1.f - T);
		NeedleDisplayPct = FMath::Lerp(Target, FMath::Lerp(Lo, Hi, Tri), Damp);
	}
	else if (RevealElapsed < LandEnd)
	{
		// 목표로 수렴 — 살짝 오버슛 후 정착
		const float T = (RevealElapsed - SweepEnd) / LandDuration;
		const float Eased = 1.f - FMath::Pow(1.f - T, 3.f);
		const float Overshoot = FMath::Sin(T * PI) * (Hi - Lo) * 0.03f;
		// 모델을 렌더의 XOf 클램프에 맞춘다 — RevealPct==MaxPct 일 때 오버슛이 Hi 를 새어나가지 않도록
		NeedleDisplayPct = FMath::Clamp(FMath::Lerp(NeedleDisplayPct, Target, Eased) + Overshoot * (1.f - T), Lo, Hi);
	}
	else
	{
		NeedleDisplayPct = Target;

		// 착지 1회 보장 — 탭 스킵이 RevealElapsed 를 LandEnd 로 점프시켜 프레임 델타 비교가 깨지므로 플래그로 가드
		if (!bLandSoundPlayed)
		{
			bLandSoundPlayed = true;
			// 착지 성패음 — 인수=투자 결과라 돈 계열: 본전 이상 성취음, 미만 은은한 경고 (가챠 결과음 GachaResultEpic 은 톤이 튐)
			if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
			{
				SoundMgr->PlayUISound(RevealPct >= 100 ? CGUISoundTags::RewardCompanyLevelUp : CGUISoundTags::NotificationWarning);
			}
		}

		if (RevealElapsed >= TotalEnd)
		{
			bRevealAnimating = false;
			OnRevealFinished.Broadcast();
		}
	}

	if (TSharedPtr<SWidget> Cached = GetCachedWidget())
	{
		Cached->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FReply UYieldRangeBarWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bRevealAnimating)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const float LandEnd = SweepDuration + LandDuration;
	if (RevealElapsed < LandEnd)
	{
		// 탭=기다림만 스킵 — 펀치+결과라벨(임팩트)은 틱이 그대로 재생해야 굴림값을 볼 수 있다
		RevealElapsed = LandEnd;
		NeedleDisplayPct = static_cast<float>(RevealPct);
		if (TSharedPtr<SWidget> Cached = GetCachedWidget())
		{
			Cached->Invalidate(EInvalidateWidgetReason::Paint);
		}
		return FReply::Handled();
	}

	// 임팩트 중 재탭 = 연타로 즉시 탈출
	bRevealAnimating = false;
	OnRevealFinished.Broadcast();
	return FReply::Handled();
}

int32 UYieldRangeBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	int32 Layer = MaxLayer + 1;

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 80.f || Size.Y < 30.f)
	{
		return MaxLayer;
	}

	// 눈금/마커가 트랙 위로 삐져나오므로 그만큼 내려 그린다 (상위가 ClipToBounds 여도 안 잘리게)
	const float TrackTop = FMath::Max(MarkerHeight, TickOverhang);
	const float TrackBottom = TrackTop + TrackHeight;

	// 트랙 (데이터 없어도 항상)
	FillRoundedRect(OutDrawElements, Layer, AllottedGeometry,
		FVector2D(0.f, TrackTop), FVector2D(Size.X, TrackBottom), CornerRadius, TrackColor);
	++Layer;

	if (!bHasData)
	{
		return Layer;
	}

	const int32 AxisMax = FMath::Max(MaxPct, FMath::Max(1, AxisMinSpan));
	auto XOf = [&Size, AxisMax](float Value) -> float
	{
		return static_cast<float>(Size.X) * FMath::Clamp(Value, 0.f, static_cast<float>(AxisMax)) / static_cast<float>(AxisMax);
	};

	// 회수 구간 채움
	FLinearColor Fill = GradeColor;
	Fill.A = FillAlpha;
	FillRoundedRect(OutDrawElements, Layer, AllottedGeometry,
		FVector2D(XOf(float(MinPct)), TrackTop), FVector2D(XOf(float(MaxPct)), TrackBottom), CornerRadius, Fill);
	++Layer;

	// 본전 눈금
	const float TickX = XOf(100.f);
	TArray<FVector2D> TickLine = { FVector2D(TickX, TrackTop - TickOverhang), FVector2D(TickX, TrackBottom + TickOverhang) };
	FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), TickLine,
		ESlateDrawEffect::None, TickColor, true, TickThickness);

	// 기댓값 마커 — 트랙 상단을 찍는 아래 방향 삼각형
	const float MarkerX = XOf(float(EvPct));
	const FLinearColor MarkerColor = (EvPct >= 100) ? TickColor : HighRiskRed;
	TArray<FVector2D> Marker = {
		FVector2D(MarkerX - MarkerWidth * 0.5f, TrackTop - MarkerHeight),
		FVector2D(MarkerX + MarkerWidth * 0.5f, TrackTop - MarkerHeight),
		FVector2D(MarkerX, TrackTop)
	};
	const FVector2D MarkerCentroid = (Marker[0] + Marker[1] + Marker[2]) / 3.f;
	FillPolyFan(OutDrawElements, Layer, AllottedGeometry, MarkerCentroid, Marker, MarkerColor);
	++Layer;

	// 결과 니들 — 트랙 하단에서 위를 찌르는 삼각형. EV(상단)/본전눈금(수직선)과 축이 갈려 셋이 동시에 읽힌다.
	const FLinearColor NeedleColor = (RevealPct >= 100) ? NeedleGoldColor : HighRiskRed;
	const float NeedleX = XOf(bRevealAnimating ? NeedleDisplayPct : static_cast<float>(RevealPct));
	if (bRevealActive)
	{
		// 착지 직후 잠깐 커졌다 돌아온다 — 임팩트 페이즈의 시각 신호.
		// ⚠ 조건에 !bRevealAnimating 을 쓰면 안 된다 — 임팩트 동안 그 플래그는 아직 true 라 펀치가 통째로 스킵된다.
		float PunchScale = 1.f;
		if (ImpactDuration > 0.f)
		{
			const float SinceLand = RevealElapsed - (SweepDuration + LandDuration);
			if (SinceLand >= 0.f && SinceLand < ImpactDuration)
			{
				PunchScale = 1.f + 0.35f * (1.f - SinceLand / ImpactDuration);
			}
		}
		const float HalfW = NeedleWidth * 0.5f * PunchScale;
		const float HeadH = NeedleHeight * PunchScale;

		TArray<FVector2D> Needle = {
			FVector2D(NeedleX - HalfW, TrackBottom + HeadH),
			FVector2D(NeedleX + HalfW, TrackBottom + HeadH),
			FVector2D(NeedleX, TrackBottom)
		};
		const FVector2D NeedleCentroid = (Needle[0] + Needle[1] + Needle[2]) / 3.f;
		FillPolyFan(OutDrawElements, Layer, AllottedGeometry, NeedleCentroid, Needle, NeedleColor);
		++Layer;
	}

	// 축 라벨 — 좌(0%) / 중앙(본전 100%) / 우(축 상한)
	if (LabelFont.FontObject)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		// 리빌 중에는 니들이 트랙 아래를 차지하므로 라벨 줄을 그만큼 내린다
		const float LabelY = TrackBottom + (bRevealActive ? NeedleHeight + 6.f : TickOverhang + 6.f);

		const FString LeftLabel = TEXT("0%");
		const FString RightLabel = FString::Printf(TEXT("%d%%"), AxisMax);
		const FString CenterLabel = TEXT("본전 100%");

		const FVector2D LeftSize = Measure->Measure(LeftLabel, LabelFont);
		const FVector2D RightSize = Measure->Measure(RightLabel, LabelFont);
		const FVector2D CenterSize = Measure->Measure(CenterLabel, LabelFont);

		const float RightX = static_cast<float>(Size.X - RightSize.X);
		const float CenterX = TickX - static_cast<float>(CenterSize.X) * 0.5f;

		// 파라미터명이 InkColor 면 유니티 빌드에서 이웃 파일(LaunchLootOddsWidget)의 익명 네임스페이스 InkColor 를
		// 셰도잉해 C4459 → -WarningsAsErrors 로 빌드가 깨진다.
		auto DrawLabel = [&](const FString& Label, const FVector2D& TextSize, float PosX, const FLinearColor& LabelInk)
		{
			FSlateDrawElement::MakeText(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(PosX, LabelY))),
				Label, LabelFont, ESlateDrawEffect::None, LabelInk);
		};

		constexpr float LabelGap = 8.f;

		FString ResultLabel;
		FVector2D ResultSize = FVector2D::ZeroVector;
		float ResultX = 0.f;
		bool bDrawLeftAxis = true;
		bool bDrawRightAxis = true;
		if (IsRevealLanded())
		{
			// 착지 후에는 중앙 "본전 100%" 대신 결과 라벨 — 같은 줄이라 둘을 동시에 두면 겹친다
			ResultLabel = (RevealPct >= 100)
				? FString::Printf(TEXT("본전 돌파 +%d%%"), RevealPct - 100)
				: FString::Printf(TEXT("회수 %d%%"), RevealPct);
			ResultSize = Measure->Measure(ResultLabel, LabelFont);
			ResultX = FMath::Clamp(
				NeedleX - static_cast<float>(ResultSize.X) * 0.5f,
				0.f,
				static_cast<float>(Size.X - ResultSize.X));

			// 결과 라벨이 리빌의 핵심이라 생략 대상이 아님 — 대신 겹치는 축 라벨을 양보시킨다
			bDrawLeftAxis = ResultX >= LeftSize.X + LabelGap;
			bDrawRightAxis = ResultX + ResultSize.X <= RightX - LabelGap;
		}

		if (bDrawLeftAxis)
		{
			DrawLabel(LeftLabel, LeftSize, 0.f, LabelColor);
		}
		if (bDrawRightAxis)
		{
			DrawLabel(RightLabel, RightSize, RightX, LabelColor);
		}

		if (IsRevealLanded())
		{
			DrawLabel(ResultLabel, ResultSize, ResultX, NeedleColor);
		}
		else
		{
			const bool bOverlaps = (CenterX < LeftSize.X + LabelGap) || (CenterX + CenterSize.X > RightX - LabelGap);
			if (!bOverlaps)
			{
				DrawLabel(CenterLabel, CenterSize, CenterX, LabelColor);
			}
		}
		++Layer;
	}

	return Layer;
}
