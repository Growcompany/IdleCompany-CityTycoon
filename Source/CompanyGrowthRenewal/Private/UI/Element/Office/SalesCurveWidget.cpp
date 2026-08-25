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
		PeakLabel->SetText(FText::FromString(FString::Printf(TEXT("Max %s원/s"), *PeakTxt.ToString())));
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

	// 배경판/그리드/축은 디자이너 크롬(UIE_SalesCurve 트리)이 담당 → Super::NativePaint 가 이미 그림(Ret 이하 레이어).
	// 여기선 데이터 종속(면적채움/곡선/점선/현재점)만 위에 얹는다.
	const float Pad = 6.0f;   // 디자이너 축 여백과 맞춤
	const float T0 = Samples[0].X;   // 첫 샘플 시각 — 운영 도중부터 관측 시작해도 왼쪽 끝부터 그리게 정규화
	const float MaxT = FMath::Max(TotalTime * 1.02f, Samples.Last().X * 1.2f);
	const float SpanT = FMath::Max(MaxT - T0, 1.0f);
	const float MaxY = PeakSeen * 1.15f;
	auto SX = [&](float T) { return Pad + ((T - T0) / SpanT) * (Size.X - 2.0f * Pad); };
	auto SY = [&](float V) { return Size.Y - Pad - FMath::Clamp(V / MaxY, 0.0f, 1.0f) * (Size.Y - 2.0f * Pad); };

	const FLinearColor CurveColor = FLinearColor::FromSRGBColor(FColor(0x3D, 0x9B, 0xE0));

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

	// 실현선 — 글로우(두꺼운 저알파) + 선명선 2중
	FSlateDrawElement::MakeLines(OutDrawElements, Ret + 2, AllottedGeometry.ToPaintGeometry(), RealPts,
		ESlateDrawEffect::None, CurveColor.CopyWithNewOpacity(0.20f), true, 6.0f);
	FSlateDrawElement::MakeLines(OutDrawElements, Ret + 2, AllottedGeometry.ToPaintGeometry(), RealPts,
		ESlateDrawEffect::None, CurveColor, true, 2.6f);

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
			ESlateDrawEffect::None, CurveColor.CopyWithNewOpacity(0.45f), true, 1.4f);
	}

	// 현재점 — 흰 12각 링
	{
		const FVector2D Center(SX(LastSample.X), SY(LastSample.Y));
		TArray<FVector2D> RingPts;
		const float Radius = 3.8f;
		for (int32 VertIndex = 0; VertIndex <= 12; ++VertIndex)
		{
			const float Angle = 2.0f * PI * VertIndex / 12.0f;
			RingPts.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Ret + 4, AllottedGeometry.ToPaintGeometry(), RingPts,
			ESlateDrawEffect::None, FLinearColor::White, true, 2.0f);
	}

	// 수명 바 — 그래프 하단 얇은 바, 남은시간/총시간 소진형 (곡선=수익 감쇠, 이 바=남은 운영시간. 한 시야에)
	if (TotalTime > 0.0f)
	{
		const float RemFrac = FMath::Clamp((TotalTime - LastSample.X) / TotalTime, 0.0f, 1.0f);
		const float LifeY = Size.Y - 3.0f;
		const float LifeL = Pad;
		const float LifeR = Size.X - Pad;

		TArray<FVector2D> Track;
		Track.Add(FVector2D(LifeL, LifeY));
		Track.Add(FVector2D(LifeR, LifeY));
		FSlateDrawElement::MakeLines(OutDrawElements, Ret + 5, AllottedGeometry.ToPaintGeometry(), Track,
			ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.10f), false, 3.0f);

		const FLinearColor LifeCol = (RemFrac < 0.2f)
			? FLinearColor(0.921582f, 0.679542f, 0.270498f, 1.0f)    // 임박 = 골드
			: FLinearColor(0.327778f, 0.806952f, 0.508881f, 0.9f);   // 정상 = 그린
		TArray<FVector2D> Life;
		Life.Add(FVector2D(LifeL, LifeY));
		Life.Add(FVector2D(LifeL + (LifeR - LifeL) * RemFrac, LifeY));
		FSlateDrawElement::MakeLines(OutDrawElements, Ret + 6, AllottedGeometry.ToPaintGeometry(), Life,
			ESlateDrawEffect::None, LifeCol, false, 3.0f);
	}

	return Ret + 7;
}
