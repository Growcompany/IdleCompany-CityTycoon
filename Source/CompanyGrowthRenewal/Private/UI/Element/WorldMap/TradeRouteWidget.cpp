// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/WorldMap/TradeRouteWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "UI/Panel/WorldMapPanelWidget.h"
#include "Utils/FWidgetAnimationUtils.h"

namespace
{
	// 유니티 빌드에서 남의 파일 스코프 심볼과 충돌하지 않게 접두사를 붙인다 (C4459 / 중복 정의).
	FVector2D TradeRouteSampleAt(const TArray<FVector2D>& Points, const TArray<double>& Cumulative, double Distance)
	{
		if (Points.Num() == 0)
		{
			return FVector2D::ZeroVector;
		}
		if (Distance <= 0.0)
		{
			return Points[0];
		}

		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			if (Cumulative[Index] >= Distance)
			{
				const double SegmentLength = Cumulative[Index] - Cumulative[Index - 1];
				const double Alpha = (SegmentLength > KINDA_SMALL_NUMBER)
					? (Distance - Cumulative[Index - 1]) / SegmentLength
					: 0.0;
				return FMath::Lerp(Points[Index - 1], Points[Index], Alpha);
			}
		}
		return Points.Last();
	}

	/**
	 * 폴리라인을 대시 조각들로 쪼개 그린다. 조각마다 원래 곡선의 중간 꼭짓점을 그대로 물려 주므로
	 * 대시가 짧아도 곡률이 살아 있고, 세그먼트 길이는 ResolveSegmentCount 가 보장한 하한을 유지한다.
	 */
	void TradeRouteEmitDashes(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FPaintGeometry& PaintGeo,
		const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness,
		double DashLength, double GapLength, double Phase)
	{
		if (Points.Num() < 2 || DashLength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		TArray<double> Cumulative;
		Cumulative.Reserve(Points.Num());
		Cumulative.Add(0.0);
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			Cumulative.Add(Cumulative.Last() + (Points[Index] - Points[Index - 1]).Size());
		}

		const double TotalLength = Cumulative.Last();
		const double Period = DashLength + GapLength;
		if (TotalLength <= KINDA_SMALL_NUMBER || Period <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		// 0 길이 세그먼트는 Slate 가 스트립을 쪼개 캡 2개로 그리게 만들어, 알파 0.24 점선이
		// 그 지점만 0.42 로 밝아진다. 대시 경계에 원곡선 꼭짓점이 붙을 때마다 생기므로 하한으로 막는다.
		constexpr double MinSegmentPx = 0.5;

		for (double DashStart = Phase - Period; DashStart < TotalLength; DashStart += Period)
		{
			const double Begin = FMath::Max(DashStart, 0.0);
			const double Finish = FMath::Min(DashStart + DashLength, TotalLength);
			if (Finish - Begin <= MinSegmentPx)
			{
				continue;
			}

			TArray<FVector2D> Dash;
			Dash.Add(TradeRouteSampleAt(Points, Cumulative, Begin));
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				if (Cumulative[Index] > Begin + MinSegmentPx && Cumulative[Index] < Finish - MinSegmentPx)
				{
					Dash.Add(Points[Index]);
				}
			}
			Dash.Add(TradeRouteSampleAt(Points, Cumulative, Finish));

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeo, Dash,
				ESlateDrawEffect::None, Color, true, Thickness);
		}
	}

	// 작은 원은 굵은 폴리라인으로 그리면 짧은 마디에서 미터가 쪼개진다(플레이북 MakeLines 미터 한계) ― 둥근 상자 SDF 로.
	void TradeRouteDrawDisc(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geo,
		const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		if (Radius <= 0.f)
		{
			return;
		}
		// 테 색 = 같은 RGB 의 알파 0. 검은 투명이면 가장자리 안티앨리어싱이 어둡게 번진다.
		const FSlateRoundedBoxBrush Brush(Color, Radius, FLinearColor(Color.R, Color.G, Color.B, 0.f), 0.f);
		const FVector2D Size(Radius * 2.0, Radius * 2.0);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
			Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5)), &Brush, ESlateDrawEffect::None, Color);
	}

	// SVG `stroke-dasharray: Length Gap` + 오프셋 −Phase 와 같은 자리: k번째 점 중심 = Phase + k·Period + Length/2.
	void TradeRouteEmitDots(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geo,
		const TArray<FVector2D>& Points, const FLinearColor& Color, float Radius,
		double DotLength, double Period, double Phase)
	{
		if (Points.Num() < 2 || Radius <= 0.f || Period <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		TArray<double> Cumulative;
		Cumulative.Reserve(Points.Num());
		Cumulative.Add(0.0);
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			Cumulative.Add(Cumulative.Last() + (Points[Index] - Points[Index - 1]).Size());
		}

		const double TotalLength = Cumulative.Last();
		for (double Center = FMath::Fmod(FMath::Max(Phase, 0.0) + DotLength * 0.5, Period); Center <= TotalLength;
			Center += Period)
		{
			TradeRouteDrawDisc(OutDrawElements, LayerId, Geo, TradeRouteSampleAt(Points, Cumulative, Center), Radius, Color);
		}
	}
}

bool UTradeRouteWidget::IsProductionSource(const FProductionRouteInput& Entry)
{
	return Entry.Country != ECountryType::None && Entry.bFactory && Entry.bUnlocked && !Entry.bHub;
}

bool UTradeRouteWidget::IsProductionHub(const FProductionRouteInput& Entry)
{
	return Entry.Country != ECountryType::None && Entry.bHub && Entry.bUnlocked;
}

TArray<FProductionRoute> UTradeRouteWidget::ResolveProductionRoutes(const TArray<FProductionRouteInput>& Countries,
	const FVector2D& BoardSize)
{
	const FVector2D Scale(BoardSize.X > 0.0 ? BoardSize.X : 1.0, BoardSize.Y > 0.0 ? BoardSize.Y : 1.0);

	TArray<FProductionRoute> Out;
	for (const FProductionRouteInput& Source : Countries)
	{
		if (!IsProductionSource(Source))
		{
			continue;
		}

		const FProductionRouteInput* Nearest = nullptr;
		double NearestDistSq = 0.0;
		for (const FProductionRouteInput& Candidate : Countries)
		{
			if (!IsProductionHub(Candidate) || Candidate.Country == Source.Country)
			{
				continue;
			}

			const double DistSq = ((Candidate.Anchor - Source.Anchor) * Scale).SizeSquared();
			const bool bTie = Nearest && FMath::IsNearlyEqual(DistSq, NearestDistSq, static_cast<double>(KINDA_SMALL_NUMBER));
			if (!Nearest || (!bTie && DistSq < NearestDistSq) || (bTie && Candidate.Country < Nearest->Country))
			{
				Nearest = &Candidate;
				NearestDistSq = DistSq;
			}
		}
		if (!Nearest)
		{
			continue;
		}

		FProductionRoute Route;
		Route.From = Source.Country;
		Route.Hub = Nearest->Country;
		Route.FromAnchor = Source.Anchor;
		Route.HubAnchor = Nearest->Anchor;
		Out.Add(Route);
	}
	return Out;
}

TArray<FVector2D> UTradeRouteWidget::BuildLiftedCurvePoints(const FVector2D& Start, const FVector2D& End, double LiftRatio)
{
	TArray<FVector2D> Points;

	const double Length = FVector2D::Distance(Start, End);
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return Points;
	}

	// Slate 는 Y 가 아래로 자란다 ― 화면 위 = −Y.
	const FVector2D Control = (Start + End) * 0.5 - FVector2D(0.0, Length * LiftRatio);

	const int32 Segments = ResolveSegmentCount(Length);
	Points.Reserve(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const double T = static_cast<double>(Index) / static_cast<double>(Segments);
		const double InvT = 1.0 - T;
		Points.Add(Start * (InvT * InvT) + Control * (2.0 * InvT * T) + End * (T * T));
	}
	return Points;
}

void UTradeRouteWidget::SetProductionRoutes(const TArray<FProductionRouteInput>& Countries)
{
	// 판 종횡비는 고정이라 설계 크기로 재도 실측 크기와 짝이 같다 ― 지오메트리 0 인 첫 프레임에도 정할 수 있다.
	ProductionRoutes = ResolveProductionRoutes(Countries, UWorldMapPanelWidget::DesignBoardSize());

	ProductionHubAnchors.Reset();
	for (const FProductionRouteInput& Entry : Countries)
	{
		if (IsProductionHub(Entry))
		{
			ProductionHubAnchors.Add(Entry.Anchor);
		}
	}

	// 움직임 줄이기 중에는 틱이 무효화를 걸지 않으므로 데이터가 바뀐 프레임을 여기서 직접 다시 그린다.
	Invalidate(EInvalidateWidget::Paint);
}

FTradeRoute UTradeRouteWidget::ResolveRoute(ECompanyType Industry, const TArray<FCountryProduction>& Producers,
	const TArray<FCountryDemand>& Demands)
{
	FTradeRoute Route;
	Route.Industry = Industry;
	Route.bValid = false;
	if (Industry == ECompanyType::None || Producers.Num() == 0 || Demands.Num() == 0)
	{
		return Route;
	}

	// 가동 라인이 가장 많은 국가. 동률이면 enum 값이 작은 쪽 ― 여기엔 쓸 만한 2차 축이 없어서
	// 프레임마다 선이 바뀌지 않게 하는 용도로만 쓴다.
	const FCountryProduction* Best = nullptr;
	for (const FCountryProduction& Producer : Producers)
	{
		if (Producer.ActiveLines <= 0)
		{
			continue;
		}
		if (!Best || Producer.ActiveLines > Best->ActiveLines ||
			(Producer.ActiveLines == Best->ActiveLines && Producer.Country < Best->Country))
		{
			Best = &Producer;
		}
	}
	if (!Best)
	{
		return Route;
	}

	const FCountryDemand* Top = nullptr;
	for (const FCountryDemand& Demand : Demands)
	{
		// ★ 출발지는 도착 후보가 아니다. 이 한 줄이 없으면 만수요(1.0) 동률 구간에서 1등이 출발지 자신이 되어
		//   주문이 통째로 버려진다 ― 한국만 해금된 오프닝에서 그 결과가 항로 0개다.
		if (Demand.Country == Best->Country)
		{
			continue;
		}

		if (!Top)
		{
			Top = &Demand;
			continue;
		}

		// 수요 비율 → PriceMul → enum. enum 은 의미 축이 아니라 마지막 결정론 장치다.
		if (!FMath::IsNearlyEqual(Demand.Ratio, Top->Ratio))
		{
			if (Demand.Ratio > Top->Ratio) { Top = &Demand; }
			continue;
		}
		if (!FMath::IsNearlyEqual(Demand.PriceMul, Top->PriceMul))
		{
			if (Demand.PriceMul > Top->PriceMul) { Top = &Demand; }
			continue;
		}
		if (Demand.Country < Top->Country)
		{
			Top = &Demand;
		}
	}

	if (!Top)
	{
		return Route;   // 출발지 말고는 갈 곳이 없다 ― 길이 0 베지어 방지
	}

	Route.From = Best->Country;
	Route.To = Top->Country;
	Route.bValid = true;
	return Route;
}

TArray<FTradeRoute> UTradeRouteWidget::BuildRoutes(const TArray<FTradeOrder>& Orders,
	TFunctionRef<TArray<FCountryProduction>(ECompanyType)> GetProducers,
	TFunctionRef<TArray<FCountryDemand>(ECompanyType)> GetDemands)
{
	TArray<FTradeRoute> Out;
	for (const FTradeOrder& Order : Orders)
	{
		if (!Order.bAccepted)
		{
			continue;   // 파란 선 = 수락한 것만
		}

		const FTradeRoute Route = ResolveRoute(Order.CompanyType, GetProducers(Order.CompanyType), GetDemands(Order.CompanyType));
		if (!Route.bValid)
		{
			continue;
		}

		// 같은 쌍을 두 번 그리면 알파가 겹쳐 더 진해진다 ― 화면이 항로 수를 거짓으로 말한다.
		bool bDuplicate = false;
		for (const FTradeRoute& Existing : Out)
		{
			if (Existing.From == Route.From && Existing.To == Route.To)
			{
				bDuplicate = true;
				break;
			}
		}
		if (!bDuplicate)
		{
			Out.Add(Route);
		}
	}
	return Out;
}

TArray<FTradeRoute> UTradeRouteWidget::BuildIdleRoutes(const TArray<FTradeOrder>& Orders,
	TFunctionRef<TArray<FCountryProduction>(ECompanyType)> GetProducers,
	TFunctionRef<TArray<FCountryDemand>(ECompanyType)> GetDemands)
{
	TArray<FTradeRoute> Out;
	for (const FTradeOrder& Order : Orders)
	{
		if (Order.bAccepted)
		{
			continue;
		}

		const FTradeRoute Route = ResolveRoute(Order.CompanyType, GetProducers(Order.CompanyType), GetDemands(Order.CompanyType));
		if (!Route.bValid)
		{
			continue;
		}

		bool bDuplicate = false;
		for (const FTradeRoute& Existing : Out)
		{
			if (Existing.From == Route.From && Existing.To == Route.To)
			{
				bDuplicate = true;
				break;
			}
		}
		if (!bDuplicate)
		{
			Out.Add(Route);
		}
	}
	return Out;
}

bool UTradeRouteWidget::ContainsPair(const TArray<FTradeRoute>& Routes, ECountryType A, ECountryType B)
{
	for (const FTradeRoute& Route : Routes)
	{
		if ((Route.From == A && Route.To == B) || (Route.From == B && Route.To == A))
		{
			return true;
		}
	}
	return false;
}

int32 UTradeRouteWidget::ResolveSegmentCount(double LengthPx)
{
	// 24px 는 목표 간격이지 보장이 아니다 ― 길이가 48 미만이면 아래 하한 2 가 이겨서 간격이 더 좁아진다.
	// 실제로 지키는 건 끝값 둘뿐: 하한 2 = BuildCurvePoints 의 0 나눗셈·직선화 방지, 상한 32 = 드로우 예산.
	constexpr double TargetSegmentPx = 24.0;
	const int32 Segments = FMath::FloorToInt32(LengthPx / TargetSegmentPx);
	return FMath::Clamp(Segments, 2, 32);
}

TArray<FVector2D> UTradeRouteWidget::BuildCurvePoints(const FVector2D& Start, const FVector2D& End, double BowRatio)
{
	TArray<FVector2D> Points;

	const FVector2D Chord = End - Start;
	const double Length = Chord.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return Points;
	}

	// 휘는 쪽이 진행 방향에 묶여 있어 A→B 와 B→A 가 반대로 휜다 = 왕복 항로가 한 줄로 뭉치지 않는다.
	const FVector2D Normal(-Chord.Y / Length, Chord.X / Length);
	const FVector2D Control = (Start + End) * 0.5 + Normal * (Length * BowRatio);

	const int32 Segments = ResolveSegmentCount(Length);
	Points.Reserve(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const double T = static_cast<double>(Index) / static_cast<double>(Segments);
		const double InvT = 1.0 - T;
		Points.Add(Start * (InvT * InvT) + Control * (2.0 * InvT * T) + End * (T * T));
	}
	return Points;
}

void UTradeRouteWidget::SetCountryAnchors(const TMap<ECountryType, FVector2D>& InAnchors)
{
	CountryAnchors = InAnchors;
	ValidateAnchors(Routes, TEXT("활성"));
	ValidateAnchors(IdleRoutes, TEXT("비활성"));
}

void UTradeRouteWidget::SetRoutes(const TArray<FTradeRoute>& InRoutes)
{
	Routes = InRoutes;
	ValidateAnchors(Routes, TEXT("활성"));
}

void UTradeRouteWidget::SetIdleRoutes(const TArray<FTradeRoute>& InRoutes)
{
	IdleRoutes = InRoutes;
	ValidateAnchors(IdleRoutes, TEXT("비활성"));
}

void UTradeRouteWidget::ValidateAnchors(const TArray<FTradeRoute>& InRoutes, const TCHAR* Label) const
{
	// 앵커를 아직 못 받은 상태(패널 배치 전)는 실패가 아니다.
	if (CountryAnchors.Num() == 0)
	{
		return;
	}

	for (const FTradeRoute& Route : InRoutes)
	{
		if (!Route.bValid)
		{
			continue;
		}
		if (!CountryAnchors.Contains(Route.From) || !CountryAnchors.Contains(Route.To))
		{
			// 그리기는 const 라 여기서만 짖을 수 있다 ― 안 그러면 "선이 없다"가 흔적 없이 지나간다.
			UE_LOG(LogTemp, Warning,
				TEXT("[TradeRoute] %s 항로의 끝점 국가가 판에 배치돼 있지 않아 그리지 않습니다. DT_CountryInfo 의 MapAnchor 를 확인하세요."),
				Label);
		}
	}
}

bool UTradeRouteWidget::ResolveEndpoints(const FTradeRoute& Route, const FVector2D& BoardSize, FVector2D& OutStart,
	FVector2D& OutEnd) const
{
	if (!Route.bValid || Route.From == Route.To)
	{
		return false;
	}

	const FVector2D* FromAnchor = CountryAnchors.Find(Route.From);
	const FVector2D* ToAnchor = CountryAnchors.Find(Route.To);
	if (!FromAnchor || !ToAnchor)
	{
		return false;
	}

	const FVector2D FromPixels = UWorldMapPanelWidget::AnchorToCanvasPosition(*FromAnchor, BoardSize);
	const FVector2D ToPixels = UWorldMapPanelWidget::AnchorToCanvasPosition(*ToAnchor, BoardSize);

	const FVector2D Chord = ToPixels - FromPixels;
	const double Length = Chord.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// 메달리온 밑에서 선이 삐져나오지 않게 양 끝을 비우되, 가까운 두 나라에서 선이 통째로 사라지지 않게
	// 현 길이의 30% 로 묶는다.
	const double Inset = FMath::Min(static_cast<double>(EndpointInsetPx), Length * 0.3);
	const FVector2D Direction = Chord / Length;
	OutStart = FromPixels + Direction * Inset;
	OutEnd = ToPixels - Direction * Inset;
	return true;
}

void UTradeRouteWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float LoopSeconds = FMath::Max(DashLoopSeconds, 0.1f);
	DashElapsed = FMath::Fmod(DashElapsed + InDeltaTime, LoopSeconds);

	// 대시 위상은 페인트에서만 읽는 상태라, 무효화를 안 걸면 글로벌 무효화가 켜진 순간 대시가 얼어붙는다
	// (선례: UGestureHintFxWidget::SetMotion). 그릴 선이 있을 때만 건다.
	if (Routes.Num() + IdleRoutes.Num() > 0)
	{
		Invalidate(EInvalidateWidget::Paint);
	}

	// 틱은 이 위젯이 그려지는 동안만 돈다(접힌·닫힌 패널은 Slate 가 틱하지 않는다).
	if (ProductionRoutes.Num() > 0)
	{
		if (FMotionGate::IsReduced())
		{
			ProductionFlowElapsed = 0.0f;
		}
		else
		{
			const float ProductionLoop = FMath::Max(ProductionLoopSeconds, 0.1f);
			ProductionFlowElapsed = FMath::Fmod(ProductionFlowElapsed + InDeltaTime, ProductionLoop);
			Invalidate(EInvalidateWidget::Paint);
		}
	}

	// 판 사각형을 못 받으면 레이어가 조용히 안 보인다 ― 그리기는 const 라 여기서 한 번만 짖는다.
	// ⚠ 선 개수로 게이트하지 않는다. 선이 0개인 상태가 바로 가장 헷갈리는 상태다.
	if (!bWarnedDegenerateSize)
	{
		const FVector2D LocalSize = MyGeometry.GetLocalSize();
		if (LocalSize.X < 1.0 || LocalSize.Y < 1.0)
		{
			bWarnedDegenerateSize = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[TradeRoute] 항로 레이어의 크기가 0 이라 선을 그릴 수 없습니다. 판(MapBaseImage)과 같은 사각형에 풀스트레치로 놓였는지 확인하세요."));
		}
	}
}

int32 UTradeRouteWidget::PaintProductionRoutes(FSlateWindowElementList& OutDrawElements, int32 BaseLayer,
	const FGeometry& AllottedGeometry, const FVector2D& BoardSize) const
{
	if (ProductionRoutes.Num() == 0 && ProductionHubAnchors.Num() == 0)
	{
		return BaseLayer;
	}

	FLinearColor DotColor = ProductionRouteColor;
	DotColor.A = ProductionRouteAlpha;
	const float DotRadius = ProductionDotThickness * 0.5f;
	const double DotLength = static_cast<double>(ProductionDotLength);
	const double DotPeriod = DotLength + static_cast<double>(ProductionDotGap);
	const double FlowPhase = static_cast<double>(ProductionFlowPerLoopPx)
		* static_cast<double>(ProductionFlowElapsed / FMath::Max(ProductionLoopSeconds, 0.1f));

	for (const FProductionRoute& Route : ProductionRoutes)
	{
		const FVector2D StartPoint = UWorldMapPanelWidget::AnchorToCanvasPosition(Route.FromAnchor, BoardSize);
		const FVector2D EndPoint = UWorldMapPanelWidget::AnchorToCanvasPosition(Route.HubAnchor, BoardSize);
		const TArray<FVector2D> Curve = BuildLiftedCurvePoints(StartPoint, EndPoint, static_cast<double>(ProductionRouteLift));
		TradeRouteEmitDots(OutDrawElements, BaseLayer, AllottedGeometry, Curve, DotColor, DotRadius,
			DotLength, DotPeriod, FlowPhase);
	}

	// 무역항 점은 항로 점 위 ― 같은 층의 둥근 상자는 배치 순서가 보장되지 않는다.
	FLinearColor HubColor = ProductionHubColor;
	HubColor.A = ProductionHubAlpha;
	for (const FVector2D& HubAnchor : ProductionHubAnchors)
	{
		TradeRouteDrawDisc(OutDrawElements, BaseLayer + 1, AllottedGeometry,
			UWorldMapPanelWidget::AnchorToCanvasPosition(HubAnchor, BoardSize), ProductionHubRadius, HubColor);
	}

	return BaseLayer + 2;
}

void UTradeRouteWidget::ReportNoRoutes(int32 OrderCount, const FString& CandidateDetail)
{
	if (bWarnedNoRoutes || OrderCount <= 0)
	{
		return;
	}
	bWarnedNoRoutes = true;

	UE_LOG(LogTemp, Warning,
		TEXT("[TradeRoute] 주문 %d건인데 항로가 0개입니다. 후보 수를 보고 원인을 고르세요 ")
		TEXT("(생산 0 = 그 산업 지원국에 가동 라인이 없음 / 도착 0 = 출발지 말고 갈 곳이 없거나 시장 셀 미정의·잠김).%s"),
		OrderCount, *CandidateDetail);
}

int32 UTradeRouteWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D BoardSize = AllottedGeometry.GetLocalSize();
	if (BoardSize.X < 1.0 || BoardSize.Y < 1.0)
	{
		return MaxLayer;
	}

	// 생산 항로는 배경 정보라 주문 항로보다 아래 층. 없으면 주문 항로 층 번호는 예전과 같다.
	const int32 DrawLayer = PaintProductionRoutes(OutDrawElements, MaxLayer + 1, AllottedGeometry, BoardSize);
	const FPaintGeometry PaintGeo = AllottedGeometry.ToPaintGeometry();
	const double BowRatio = static_cast<double>(RouteBowRatio);

	// 비활성 먼저 ― 파란 선이 항상 위에 온다.
	FLinearColor IdleColor = IdleRouteColor;
	IdleColor.A = IdleRouteAlpha;
	for (const FTradeRoute& Route : IdleRoutes)
	{
		// 이미 파란 선이 말하고 있는 쌍에 점선을 겹치면 블루가 희석된다.
		if (ContainsPair(Routes, Route.From, Route.To))
		{
			continue;
		}

		FVector2D StartPoint;
		FVector2D EndPoint;
		if (!ResolveEndpoints(Route, BoardSize, StartPoint, EndPoint))
		{
			continue;
		}

		const TArray<FVector2D> Curve = BuildCurvePoints(StartPoint, EndPoint, BowRatio);
		TradeRouteEmitDashes(OutDrawElements, DrawLayer, PaintGeo, Curve, IdleColor, IdleThickness,
			static_cast<double>(IdleDashLength), static_cast<double>(IdleDashGap), 0.0);
	}

	FLinearColor GlowColor = ActiveRouteColor;
	GlowColor.A = ActiveGlowAlpha;

	const double DashPeriod = static_cast<double>(ActiveDashLength) + static_cast<double>(ActiveDashGap);
	const double DashPhase = DashPeriod * static_cast<double>(DashElapsed / FMath::Max(DashLoopSeconds, 0.1f));

	for (const FTradeRoute& Route : Routes)
	{
		FVector2D StartPoint;
		FVector2D EndPoint;
		if (!ResolveEndpoints(Route, BoardSize, StartPoint, EndPoint))
		{
			continue;
		}

		const TArray<FVector2D> Curve = BuildCurvePoints(StartPoint, EndPoint, BowRatio);
		if (Curve.Num() < 2)
		{
			continue;
		}

		// 연속 글로우 → 그 위에 흐르는 대시. 글로우가 없으면 경로가 끊겨 보이고,
		// 대시가 없으면 어느 방향으로 흐르는지 알 수 없다.
		FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, PaintGeo, Curve,
			ESlateDrawEffect::None, GlowColor, true, ActiveThickness * GlowThicknessScale);

		TradeRouteEmitDashes(OutDrawElements, DrawLayer + 1, PaintGeo, Curve, ActiveRouteColor, ActiveThickness,
			static_cast<double>(ActiveDashLength), static_cast<double>(ActiveDashGap), DashPhase);
	}

	return DrawLayer + 1;
}
