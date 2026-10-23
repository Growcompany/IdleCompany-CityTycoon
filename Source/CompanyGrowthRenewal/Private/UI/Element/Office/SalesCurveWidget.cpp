#include "UI/Element/Office/SalesCurveWidget.h"
#include "Data/OperationData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Global/GlobalUtilFunctions.h"

TSharedRef<SWidget> USalesCurveWidget::RebuildWidget()
{
	// 무인 자가 트리 — 디자이너 작업 0. WBP가 트리를 넣었으면 그걸 존중.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CurveRoot"));
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void USalesCurveWidget::ResetCurve()
{
	Samples.Empty();
	PeakSeen = 1.0f;
	LastSampleTime = -1.0f;
}

void USalesCurveWidget::BackfillHistory(const FOperationData& Op)
{
	// 현재 rps에서 감쇠 역산 → 시작(t=0) 피크 추정(volatility 무시=평균보존). 감쇠식은 매니저와 동일.
	const float Total = FMath::Max(Op.TotalOperationTime, 1.0f);
	const float HLFrac = (Op.HalfLifeFrac > 0.0f) ? Op.HalfLifeFrac : 0.5f;
	const float DecayHalf = FMath::Max(1.0f, Total * HLFrac);
	const float DecayNow = FMath::Max(FMath::Pow(0.5f, Op.ElapsedTime / DecayHalf), 0.02f);   // 0 가드(피크 폭주 방지: 최대 50배)
	const float Peak = Op.ActualRevenuePerSecond / DecayNow;

	// 0~현재를 균등 샘플로 재구성 (마지막 현재점은 이어서 PushSample 본체가 실제값으로 추가)
	const int32 Steps = 40;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		const float T = (Op.ElapsedTime * Index) / static_cast<float>(Steps);
		const float V = Peak * FMath::Pow(0.5f, T / DecayHalf);
		Samples.Add(FVector2D(T, V));
		PeakSeen = FMath::Max(PeakSeen, V);
	}
}

void USalesCurveWidget::PushSample(const FOperationData& Op)
{
	// 새 운영 시작 감지 (경과시간 역행) → 리셋
	if (Op.ElapsedTime < LastSampleTime - 0.5f)
	{
		ResetCurve();
	}
	// 재진입: 히스토리 빈 채 운영 도중부터면 감쇠 모델로 0~현재 백필 (곡선이 중간부터 시작하는 것 방지)
	if (Samples.Num() == 0 && Op.ElapsedTime > 1.0f)
	{
		BackfillHistory(Op);
	}
	if (Op.ElapsedTime - LastSampleTime < MinSampleGapSec && Samples.Num() > 0)
	{
		return;
	}
	LastSampleTime = Op.ElapsedTime;

	TotalTime = FMath::Max(Op.TotalOperationTime, 1.0f);
	HalfLifeFrac = (Op.HalfLifeFrac > 0.0f) ? Op.HalfLifeFrac : 0.5f;
	PeakSeen = FMath::Max(PeakSeen, Op.ActualRevenuePerSecond);

	// 디자이너 크롬에 피크 라벨이 있으면 값 갱신 (없으면 무동작)
	if (PeakLabel)
	{
		const FText PeakTxt = UGlobalUtilFunctions::AbbreviateNumber(
			static_cast<int64>(PeakSeen), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor);
		PeakLabel->SetText(FText::Format(NSLOCTEXT("Office", "SalesCurvePeak", "최대 {0}/s"), PeakTxt));
	}

	Samples.Add(FVector2D(Op.ElapsedTime, Op.ActualRevenuePerSecond));
	if (Samples.Num() > MaxSamples)
	{
		// 앞쪽 절반을 2:1로 솎아 용량 유지 (모양 보존)
		TArray<FVector2D> Thinned;
		Thinned.Reserve(MaxSamples / 2 + Samples.Num() / 2);
		for (int32 Index = 0; Index < Samples.Num() / 2; Index += 2) { Thinned.Add(Samples[Index]); }
		for (int32 Index = Samples.Num() / 2; Index < Samples.Num(); ++Index) { Thinned.Add(Samples[Index]); }
		Samples = MoveTemp(Thinned);
	}
}

int32 USalesCurveWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Ret = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (Samples.Num() < 2)
	{
		return Ret;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 8.0f || Size.Y < 8.0f)
	{
		return Ret;
	}

	// 바탕(오목한 칸)은 호스트인 운영 카드가 그린다. 여기선 데이터 종속(면적채움/곡선/점선/현재점)만 얹는다.
	const float Pad = 6.0f;   // 카드 칸 안쪽 여백(12)과 더해 곡선이 칸 가장자리에서 18 떨어진다
	// 수익은 시작이 최고점이라 곡선이 늘 왼쪽 위 피크 라벨 자리를 지난다 — 라벨 아래 끝까지 위를 비운다.
	float TopPad = Pad;
	if (PeakLabel && PeakLabel->IsVisible())
	{
		const FGeometry& LabelGeo = PeakLabel->GetCachedGeometry();
		const float LabelBottom = AllottedGeometry.AbsoluteToLocal(LabelGeo.LocalToAbsolute(FVector2D(0.0f, LabelGeo.GetLocalSize().Y))).Y;
		TopPad = FMath::Max(Pad + PeakLabel->GetDesiredSize().Y, LabelBottom) + 2.0f;
	}
	const float T0 = Samples[0].X;   // 첫 샘플 시각 — 운영 도중부터 관측 시작해도 왼쪽 끝부터 그리게 정규화
	const float MaxT = FMath::Max(TotalTime * 1.02f, Samples.Last().X * 1.2f);
	const float SpanT = FMath::Max(MaxT - T0, 1.0f);
	const float MaxY = PeakSeen * 1.15f;
	auto SX = [&](float T) { return Pad + ((T - T0) / SpanT) * (Size.X - 2.0f * Pad); };
	auto SY = [&](float V) { return Size.Y - Pad - FMath::Clamp(V / MaxY, 0.0f, 1.0f) * FMath::Max(1.0f, Size.Y - Pad - TopPad); };

	const FLinearColor CurveColor = FLinearColor::FromSRGBColor(FColor(0x5F, 0xD3, 0xA8));   // 수익 = 카드의 초록 계열

	// 곡선 스크린 좌표
	TArray<FVector2D> RealPts;
	RealPts.Reserve(Samples.Num());
	for (const FVector2D& Sample : Samples)
	{
		RealPts.Add(FVector2D(SX(Sample.X), SY(Sample.Y)));
	}

	// 면적 채움 — 샘플마다 베이스라인까지 세로선(저알파). 곡선 아래를 블루로 채워 "그래프" 느낌.
	const float BaseY = SY(0.0f);
	const FLinearColor FillColor = CurveColor.CopyWithNewOpacity(0.16f);
	for (const FVector2D& Pt : RealPts)
	{
		TArray<FVector2D> Col;
		Col.Add(Pt);
		Col.Add(FVector2D(Pt.X, BaseY));
		FSlateDrawElement::MakeLines(OutDrawElements, Ret + 1, AllottedGeometry.ToPaintGeometry(), Col,
			ESlateDrawEffect::None, FillColor, false, 2.0f);
	}

	// 실현선
	FSlateDrawElement::MakeLines(OutDrawElements, Ret + 2, AllottedGeometry.ToPaintGeometry(), RealPts,
		ESlateDrawEffect::None, CurveColor, true, 4.0f);

	// 투영 점선 — 마지막 샘플에서 감쇠 외삽. 점선 = 짧은 세그먼트 나열
	const FVector2D LastSample = Samples.Last();
	const float DecayHalf = FMath::Max(1.0f, TotalTime * HalfLifeFrac);
	const float StepT = MaxT / 60.0f;
	bool bDash = true;
	for (float T = LastSample.X; T < MaxT - StepT; T += StepT, bDash = !bDash)
	{
		if (!bDash) { continue; }
		const float V0 = LastSample.Y * FMath::Pow(0.5f, (T - LastSample.X) / DecayHalf);
		const float V1 = LastSample.Y * FMath::Pow(0.5f, (T + StepT - LastSample.X) / DecayHalf);
		TArray<FVector2D> DashPts;
		DashPts.Add(FVector2D(SX(T), SY(V0)));
		DashPts.Add(FVector2D(SX(T + StepT), SY(V1)));
		FSlateDrawElement::MakeLines(OutDrawElements, Ret + 3, AllottedGeometry.ToPaintGeometry(), DashPts,
			ESlateDrawEffect::None, CurveColor.CopyWithNewOpacity(0.45f), true, 3.0f);
	}

	// 현재점 — 밝은 속 + 곡선색 테(남은 운영 시간은 카드의 「남은 시간」 칸이 보여 준다)
	{
		const FVector2D Center(SX(LastSample.X), SY(LastSample.Y));
		auto Ring = [&](float Radius, const FLinearColor& Color, float Thickness, int32 Layer)
		{
			TArray<FVector2D> RingPts;
			for (int32 VertIndex = 0; VertIndex <= 16; ++VertIndex)
			{
				const float Angle = 2.0f * PI * VertIndex / 16.0f;
				RingPts.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), RingPts,
				ESlateDrawEffect::None, Color, true, Thickness);
		};
		Ring(2.75f, FLinearColor::FromSRGBColor(FColor(0xE9, 0xFF, 0xF6)), 5.5f, Ret + 4);
		Ring(7.0f, CurveColor, 3.0f, Ret + 5);
	}

	return Ret + 6;
}
