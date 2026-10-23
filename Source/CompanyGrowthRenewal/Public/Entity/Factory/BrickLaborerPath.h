#pragma once

#include "CoreMinimal.h"

// 문 소켓의 +X가 안쪽이다. 문 앞 직선 사이에서만 외부 경로를 바꾼다.
struct FBrickLaborerPath
{
	static constexpr int32 CurveSamples = 64;
	FVector InsideFrom = FVector::ZeroVector;
	FVector InsideTo = FVector::ZeroVector;
	FVector InwardFrom = FVector::ForwardVector;
	FVector InwardTo = FVector::ForwardVector;
	FVector CurveP0 = FVector::ZeroVector;
	FVector CurveP1 = FVector::ZeroVector;
	FVector CurveP2 = FVector::ZeroVector;
	FVector CurveP3 = FVector::ZeroVector;
	FVector SecondP1 = FVector::ZeroVector;
	FVector SecondP2 = FVector::ZeroVector;
	FVector SecondP3 = FVector::ZeroVector;
	float ExitLength = 0.f;
	float CurveLength = 0.f;
	float EntryLength = 0.f;
	float EntryDepth = 0.f;
	float CurveDistances[CurveSamples + 1] = {};

	void BuildBounded(const FTransform& FromDoor, const FTransform& ToDoor,
		float EnterDepth, float ApproachDistance, int32 LaneIndex,
		const FTransform& Footprint, const FVector2D& HalfExtent, float Clearance, const FBox2D& BodyBounds)
	{
		SideSegments.Reset();
		InwardFrom = FromDoor.GetUnitAxis(EAxis::X).GetSafeNormal2D();
		InwardTo = ToDoor.GetUnitAxis(EAxis::X).GetSafeNormal2D();
		if (InwardFrom.IsNearlyZero()) InwardFrom = FVector::ForwardVector;
		if (InwardTo.IsNearlyZero()) InwardTo = InwardFrom;
		const FVector2D SafeHalfExtent(
			FMath::Max(0.f, HalfExtent.X - FMath::Max(0.f, Clearance)),
			FMath::Max(0.f, HalfExtent.Y - FMath::Max(0.f, Clearance)));
		const FVector LocalFromInward = Footprint.InverseTransformVectorNoScale(InwardFrom);
		const FVector LocalToInward = Footprint.InverseTransformVectorNoScale(InwardTo);
		const bool bHasBody = BodyBounds.Min.X <= BodyBounds.Max.X && BodyBounds.Min.Y <= BodyBounds.Max.Y;
		if (!bHasBody && (LocalFromInward.X < 0.99f || LocalToInward.X < 0.99f))
		{
			BuildWithoutBody(FromDoor, ToDoor, EnterDepth, ApproachDistance, Footprint, SafeHalfExtent);
			return;
		}
		if (LocalFromInward.X < 0.99f || LocalToInward.X < 0.99f)
		{
			BuildSideRoute(FromDoor, ToDoor, EnterDepth, ApproachDistance, LaneIndex,
				Footprint, SafeHalfExtent, Clearance, BodyBounds);
			return;
		}
		const float ExteriorFrontX = bHasBody
			? static_cast<float>(BodyBounds.Min.X) - Clearance : TNumericLimits<float>::Max();
		const float FromInsideLimit = RayDistanceToRectangle(FromDoor.GetLocation(), InwardFrom, Footprint, SafeHalfExtent);
		const float ToInsideLimit = RayDistanceToRectangle(ToDoor.GetLocation(), InwardTo, Footprint, SafeHalfExtent);
		const float FromOutsideLimit = RayDistanceToRectangle(FromDoor.GetLocation(), -InwardFrom, Footprint, SafeHalfExtent);
		const float ToOutsideLimit = RayDistanceToRectangle(ToDoor.GetLocation(), -InwardTo, Footprint, SafeHalfExtent);
		// 건물과 적재물 앞을 완전히 벗어난 뒤 곡선을 시작한다.
		const float FromApproach = FMath::Min(FMath::Max(FMath::Max(1.f, ApproachDistance),
			DistanceToFront(FromDoor.GetLocation(), -InwardFrom, Footprint, ExteriorFrontX)), FromOutsideLimit);
		const float ToApproach = FMath::Min(FMath::Max(FMath::Max(1.f, ApproachDistance),
			DistanceToFront(ToDoor.GetLocation(), -InwardTo, Footprint, ExteriorFrontX)), ToOutsideLimit);
		const float PlotBowBudget = FMath::Max(0.f,
			FMath::Min(FromOutsideLimit - FromApproach, ToOutsideLimit - ToApproach));
		const float BowBudget = bHasBody
			? FMath::Min(PlotBowBudget, PreferredExcursion(BodyBounds, Clearance)) : PlotBowBudget;

		// 전면의 좁은 X 여유를 5개 경로에 직접 나눠 쓴다. 좌우 경로는 측면 여유로도 벌린다.
		static constexpr float DepthFraction[5] = { 0.30f, 1.f, 0.72f, 0.72f, 0.85f };
		static constexpr float MidFraction[5] = { 0.5f, 0.5f, 0.22f, 0.78f, 0.58f };
		const int32 Lane = FMath::Max(0, LaneIndex) % 5;
		// 정원 100명 안에서 같은 기본 경로의 20명에게 고유한 깊이·합류 위치를 준다.
		const int32 Personality = ((FMath::Max(0, LaneIndex) / 5) % 20) * 7 % 20;
		const float BaseBow = BowBudget * DepthFraction[Lane];
		const float VariationWidth = FMath::Min(BowBudget * 0.9f, 150.f);
		float Bow = BaseBow;
		if (Personality > 0 && VariationWidth > KINDA_SMALL_NUMBER)
		{
			const float MinimumBow = FMath::Clamp(BaseBow - VariationWidth * 0.5f,
				BowBudget * 0.1f, BowBudget - VariationWidth);
			const float DepthPhase = FMath::Fmod((BaseBow - MinimumBow) / VariationWidth
				+ static_cast<float>(Personality) / 20.f, 1.f);
			Bow = MinimumBow + DepthPhase * VariationWidth;
		}
		const float MergeFraction = MidFraction[Lane]
			+ static_cast<float>(Personality / 4 - 2) * 0.025f;
		InsideFrom = FromDoor.GetLocation() + InwardFrom * FMath::Min(FMath::Max(0.f, EnterDepth), FromInsideLimit);
		EntryDepth = FMath::Min(FMath::Max(0.f, EnterDepth), ToInsideLimit);
		InsideTo = ToDoor.GetLocation() + InwardTo * EntryDepth;
		CurveP0 = FromDoor.GetLocation() - InwardFrom * FromApproach;
		SecondP3 = ToDoor.GetLocation() - InwardTo * ToApproach;
		CurveP1 = CurveP0 - InwardFrom * Bow;
		SecondP2 = SecondP3 - InwardTo * Bow;
		CurveP3 = FMath::Lerp(CurveP1, SecondP2, MergeFraction);

		const FVector LocalFrom = Footprint.InverseTransformPositionNoScale(CurveP0);
		const FVector LocalTo = Footprint.InverseTransformPositionNoScale(SecondP3);
		FVector LocalMiddle = Footprint.InverseTransformPositionNoScale(CurveP3);
		const float TravelSign = LocalTo.Y >= LocalFrom.Y ? 1.f : -1.f;
		const double HalfSpan = FMath::Abs(LocalTo.Y - LocalFrom.Y) * 0.5;
		double Handle = FMath::Min(HalfSpan * 0.4,
			FMath::Min(SafeHalfExtent.Y - LocalMiddle.Y, SafeHalfExtent.Y + LocalMiddle.Y));
		if (Lane == 2 || Lane == 3)
		{
			const double SideLimit = Lane == 2
				? (TravelSign > 0.f ? -SafeHalfExtent.Y : SafeHalfExtent.Y)
				: (TravelSign > 0.f ? SafeHalfExtent.Y : -SafeHalfExtent.Y);
			Handle = FMath::Min(FMath::Abs(SideLimit - LocalMiddle.Y),
				FMath::Min(SafeHalfExtent.Y - LocalMiddle.Y, SafeHalfExtent.Y + LocalMiddle.Y));
			if (bHasBody)
			{
				const FVector2D BodySize = BodyBounds.GetSize();
				const double LateralBudget = FMath::Max(static_cast<double>(Clearance) * 2.5,
					FMath::Max(BodySize.X, BodySize.Y) * 0.35);
				Handle = FMath::Min(Handle, LateralBudget);
			}
		}
		Handle = FMath::Max(0.0, Handle) * (1.0 - static_cast<double>(Personality % 5) * 0.015);
		FVector LocalBefore = LocalMiddle;
		FVector LocalAfter = LocalMiddle;
		LocalBefore.Y -= TravelSign * Handle;
		LocalAfter.Y += TravelSign * Handle;
		CurveP2 = Footprint.TransformPositionNoScale(LocalBefore);
		SecondP1 = Footprint.TransformPositionNoScale(LocalAfter);
		// 로컬 XY만 조정하고 높이는 문 소켓을 잇는 동일한 보행면으로 둔다.
		CurveP2.Z = CurveP3.Z;
		SecondP1.Z = CurveP3.Z;
		BuildDistances();
	}

	float Length() const { return ExitLength + CurveLength + EntryLength; }

	FVector PositionAtDistance(float Distance) const
	{
		const float Clamped = FMath::Clamp(Distance, 0.f, Length());
		if (Clamped <= ExitLength)
		{
			return FMath::Lerp(InsideFrom, CurveP0, Clamped / FMath::Max(ExitLength, KINDA_SMALL_NUMBER));
		}
		if (Clamped < ExitLength + CurveLength)
		{
			return SideSegments.IsEmpty() ? EvaluateCurve(CurveAlphaAtDistance(Clamped - ExitLength))
				: EvaluateSideCurve(Clamped - ExitLength, false);
		}
		return FMath::Lerp(SideSegments.IsEmpty() ? SecondP3 : SideSegments.Last().P3, InsideTo,
			(Clamped - ExitLength - CurveLength) / FMath::Max(EntryLength, KINDA_SMALL_NUMBER));
	}

	FVector DirectionAtDistance(float Distance) const
	{
		if (Distance <= ExitLength) return -InwardFrom;
		if (Distance >= ExitLength + CurveLength) return InwardTo;
		if (!SideSegments.IsEmpty()) return EvaluateSideCurve(Distance - ExitLength, true);
		const float Alpha = CurveAlphaAtDistance(Distance - ExitLength);
		const float T = Alpha <= 0.5f ? Alpha * 2.f : (Alpha - 0.5f) * 2.f;
		const FVector P0 = Alpha <= 0.5f ? CurveP0 : CurveP3;
		const FVector P1 = Alpha <= 0.5f ? CurveP1 : SecondP1;
		const FVector P2 = Alpha <= 0.5f ? CurveP2 : SecondP2;
		const FVector P3 = Alpha <= 0.5f ? CurveP3 : SecondP3;
		const float U = 1.f - T;
		return ((3.f * U * U) * (P1 - P0)
			+ (6.f * U * T) * (P2 - P1)
			+ (3.f * T * T) * (P3 - P2)).GetSafeNormal2D();
	}

private:
	struct FCubicSegment
	{
		FVector P0, P1, P2, P3;
		float StartDistance = 0.f;
		float Samples[17] = {};
	};
	TArray<FCubicSegment> SideSegments;

	void BuildWithoutBody(const FTransform& FromDoor, const FTransform& ToDoor,
		float EnterDepth, float ApproachDistance, const FTransform& Footprint,
		const FVector2D& SafeHalfExtent)
	{
		const float FromInsideLimit = RayDistanceToRectangle(FromDoor.GetLocation(), InwardFrom, Footprint, SafeHalfExtent);
		const float ToInsideLimit = RayDistanceToRectangle(ToDoor.GetLocation(), InwardTo, Footprint, SafeHalfExtent);
		const float FromOutsideLimit = RayDistanceToRectangle(FromDoor.GetLocation(), -InwardFrom, Footprint, SafeHalfExtent);
		const float ToOutsideLimit = RayDistanceToRectangle(ToDoor.GetLocation(), -InwardTo, Footprint, SafeHalfExtent);
		InsideFrom = FromDoor.GetLocation() + InwardFrom * FMath::Min(FMath::Max(0.f, EnterDepth), FromInsideLimit);
		EntryDepth = FMath::Min(FMath::Max(0.f, EnterDepth), ToInsideLimit);
		InsideTo = ToDoor.GetLocation() + InwardTo * EntryDepth;
		CurveP0 = FromDoor.GetLocation() - InwardFrom * FMath::Min(FMath::Max(1.f, ApproachDistance), FromOutsideLimit);
		SecondP3 = ToDoor.GetLocation() - InwardTo * FMath::Min(FMath::Max(1.f, ApproachDistance), ToOutsideLimit);
		ExitLength = static_cast<float>(FVector::Distance(InsideFrom, CurveP0));
		EntryLength = static_cast<float>(FVector::Distance(SecondP3, InsideTo));
		CurveLength = 0.f;
		const float Handle = FMath::Min(100.f, static_cast<float>(FVector::Distance(CurveP0, SecondP3)) * 0.25f);
		const float FromHandle = FMath::Min(Handle,
			RayDistanceToRectangle(CurveP0, -InwardFrom, Footprint, SafeHalfExtent));
		const float ToHandle = FMath::Min(Handle,
			RayDistanceToRectangle(SecondP3, -InwardTo, Footprint, SafeHalfExtent));
		AddSideSegment(CurveP0, CurveP0 - InwardFrom * FromHandle,
			SecondP3 - InwardTo * ToHandle, SecondP3);
	}

	static FVector CubicPoint(const FCubicSegment& Segment, float T)
	{
		const float U = 1.f - T;
		return U * U * U * Segment.P0 + 3.f * U * U * T * Segment.P1
			+ 3.f * U * T * T * Segment.P2 + T * T * T * Segment.P3;
	}

	void AddSideSegment(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3)
	{
		FCubicSegment& Segment = SideSegments.AddDefaulted_GetRef();
		Segment.P0 = P0; Segment.P1 = P1; Segment.P2 = P2; Segment.P3 = P3;
		Segment.StartDistance = CurveLength;
		FVector Previous = P0;
		for (int32 Index = 1; Index <= 16; ++Index)
		{
			const FVector Point = CubicPoint(Segment, static_cast<float>(Index) / 16.f);
			Segment.Samples[Index] = Segment.Samples[Index - 1]
				+ static_cast<float>(FVector::Distance(Previous, Point));
			Previous = Point;
		}
		CurveLength += Segment.Samples[16];
	}

	void AddSideLine(const FVector& From, const FVector& To)
	{
		if (From.Equals(To, 0.01f)) return;
		AddSideSegment(From, FMath::Lerp(From, To, 1.f / 3.f),
			FMath::Lerp(From, To, 2.f / 3.f), To);
	}

	FVector EvaluateSideCurve(float Distance, bool bDirection) const
	{
		for (const FCubicSegment& Segment : SideSegments)
		{
			if (Distance > Segment.StartDistance + Segment.Samples[16] && &Segment != &SideSegments.Last()) continue;
			const float LocalDistance = FMath::Clamp(Distance - Segment.StartDistance, 0.f, Segment.Samples[16]);
			float T = 1.f;
			for (int32 Index = 1; Index <= 16; ++Index)
			{
				if (LocalDistance <= Segment.Samples[Index])
				{
					T = (Index - 1 + (LocalDistance - Segment.Samples[Index - 1])
						/ FMath::Max(Segment.Samples[Index] - Segment.Samples[Index - 1], KINDA_SMALL_NUMBER)) / 16.f;
					break;
				}
			}
			if (!bDirection) return CubicPoint(Segment, T);
			const float U = 1.f - T;
			return (3.f * U * U * (Segment.P1 - Segment.P0)
				+ 6.f * U * T * (Segment.P2 - Segment.P1)
				+ 3.f * T * T * (Segment.P3 - Segment.P2)).GetSafeNormal2D();
		}
		return FVector::ZeroVector;
	}

	void BuildSideRoute(const FTransform& FromDoor, const FTransform& ToDoor,
		float EnterDepth, float ApproachDistance, int32 LaneIndex, const FTransform& Footprint,
		const FVector2D& SafeHalfExtent, float Clearance, const FBox2D& BodyBounds)
	{
		const float FromInsideLimit = RayDistanceToRectangle(FromDoor.GetLocation(), InwardFrom, Footprint, SafeHalfExtent);
		const float ToInsideLimit = RayDistanceToRectangle(ToDoor.GetLocation(), InwardTo, Footprint, SafeHalfExtent);
		const float FromOutsideLimit = RayDistanceToRectangle(FromDoor.GetLocation(), -InwardFrom, Footprint, SafeHalfExtent);
		const float ToOutsideLimit = RayDistanceToRectangle(ToDoor.GetLocation(), -InwardTo, Footprint, SafeHalfExtent);
		InsideFrom = FromDoor.GetLocation() + InwardFrom * FMath::Min(FMath::Max(0.f, EnterDepth), FromInsideLimit);
		EntryDepth = FMath::Min(FMath::Max(0.f, EnterDepth), ToInsideLimit);
		InsideTo = ToDoor.GetLocation() + InwardTo * EntryDepth;
		const FVector LocalFromDoor = Footprint.InverseTransformPositionNoScale(FromDoor.GetLocation());
		const FVector LocalToDoor = Footprint.InverseTransformPositionNoScale(ToDoor.GetLocation());
		const FVector LocalOutFrom = Footprint.InverseTransformVectorNoScale(-InwardFrom);
		const FVector LocalOutTo = Footprint.InverseTransformVectorNoScale(-InwardTo);
		const FBox2D Obstacle(BodyBounds.Min - FVector2D(Clearance, Clearance),
			BodyBounds.Max + FVector2D(Clearance, Clearance));
		auto Face = [](const FVector& Direction) -> int32
		{
			if (FMath::Abs(Direction.X) > FMath::Abs(Direction.Y)) return Direction.X > 0.f ? 1 : 3;
			return Direction.Y > 0.f ? 2 : 0;
		};
		const int32 FromFace = Face(LocalOutFrom);
		const int32 ToFace = Face(LocalOutTo);
		auto DistancePastBody = [&Obstacle](const FVector& Origin, const FVector& Direction, int32 Side) -> float
		{
			const float Coordinate = Side % 2 == 0 ? Origin.Y : Origin.X;
			const float Target = Side == 0 ? Obstacle.Min.Y : Side == 1 ? Obstacle.Max.X
				: Side == 2 ? Obstacle.Max.Y : Obstacle.Min.X;
			const float Component = Side % 2 == 0 ? Direction.Y : Direction.X;
			return FMath::Abs(Component) > KINDA_SMALL_NUMBER
				? FMath::Max(0.f, static_cast<float>((Target - Coordinate) / Component)) : 0.f;
		};
		const float FromApproach = FMath::Min(FromOutsideLimit,
			FMath::Max(FMath::Max(1.f, ApproachDistance), DistancePastBody(LocalFromDoor, LocalOutFrom, FromFace)));
		const float ToApproach = FMath::Min(ToOutsideLimit,
			FMath::Max(FMath::Max(1.f, ApproachDistance), DistancePastBody(LocalToDoor, LocalOutTo, ToFace)));
		const FVector LocalP0 = LocalFromDoor + LocalOutFrom * FromApproach;
		const FVector LocalP3 = LocalToDoor + LocalOutTo * ToApproach;
		CurveP0 = Footprint.TransformPositionNoScale(LocalP0);
		SecondP3 = Footprint.TransformPositionNoScale(LocalP3);
		ExitLength = static_cast<float>(FVector::Distance(InsideFrom, CurveP0));
		EntryLength = static_cast<float>(FVector::Distance(SecondP3, InsideTo));

		const int32 Lane = FMath::Max(0, LaneIndex) % 5;
		const int32 Personality = ((FMath::Max(0, LaneIndex) / 5) % 20) * 7 % 20;
		auto OutsideBudget = [&SafeHalfExtent](const FVector& Point, int32 Side) -> float
		{
			switch (Side)
			{
			case 0: return static_cast<float>(Point.Y + SafeHalfExtent.Y);
			case 1: return static_cast<float>(SafeHalfExtent.X - Point.X);
			case 2: return static_cast<float>(SafeHalfExtent.Y - Point.Y);
			default: return static_cast<float>(Point.X + SafeHalfExtent.X);
			}
		};
		const float BodyOutsideBudget = FMath::Min(
			FMath::Min(Obstacle.Min.X + SafeHalfExtent.X, SafeHalfExtent.X - Obstacle.Max.X),
			FMath::Min(Obstacle.Min.Y + SafeHalfExtent.Y, SafeHalfExtent.Y - Obstacle.Max.Y));
		const float PlotBowBudget = FMath::Max(0.f, FMath::Min(BodyOutsideBudget,
			FMath::Min(OutsideBudget(LocalP0, FromFace), OutsideBudget(LocalP3, ToFace))));
		const float BowBudget = FMath::Min(PlotBowBudget, PreferredExcursion(BodyBounds, Clearance));
		const float MinimumMargin = FMath::Min(35.f, BowBudget);
		static constexpr float DepthFraction[5] = { 0.20f, 0.95f, 0.55f, 0.75f, 0.65f };
		const float BaseMargin = MinimumMargin + (BowBudget - MinimumMargin) * DepthFraction[Lane];
		const float VariationWidth = FMath::Min(150.f, BowBudget - MinimumMargin);
		float Margin = BaseMargin;
		if (Personality > 0 && VariationWidth > KINDA_SMALL_NUMBER)
		{
			const float MinimumBow = FMath::Clamp(BaseMargin - VariationWidth * 0.5f,
				MinimumMargin, BowBudget - VariationWidth);
			const float Phase = FMath::Fmod((BaseMargin - MinimumBow) / VariationWidth
				+ static_cast<float>(Personality) / 20.f, 1.f);
			Margin = MinimumBow + Phase * VariationWidth;
		}
		// 문 앞 직선에는 접선 연결을 위한 최소 폭만 남긴다.
		FBox2D Rail(FVector2D(
			FMath::Max(-SafeHalfExtent.X, static_cast<float>(FMath::Min3(Obstacle.Min.X - Margin, LocalP0.X - MinimumMargin, LocalP3.X - MinimumMargin))),
			FMath::Max(-SafeHalfExtent.Y, static_cast<float>(FMath::Min3(Obstacle.Min.Y - Margin, LocalP0.Y - MinimumMargin, LocalP3.Y - MinimumMargin)))),
			FVector2D(
				FMath::Min(SafeHalfExtent.X, static_cast<float>(FMath::Max3(Obstacle.Max.X + Margin, LocalP0.X + MinimumMargin, LocalP3.X + MinimumMargin))),
				FMath::Min(SafeHalfExtent.Y, static_cast<float>(FMath::Max3(Obstacle.Max.Y + Margin, LocalP0.Y + MinimumMargin, LocalP3.Y + MinimumMargin)))));
		const float RailClearance = FMath::Max(0.f, static_cast<float>(FMath::Min(
			FMath::Min(Obstacle.Min.X - Rail.Min.X, Rail.Max.X - Obstacle.Max.X),
			FMath::Min(Obstacle.Min.Y - Rail.Min.Y, Rail.Max.Y - Obstacle.Max.Y))));
		auto OnFace = [&Rail](const FVector& Point, int32 Side) -> FVector
		{
			FVector Result = Point;
			if (Side == 0 || Side == 2) Result.Y = Side == 0 ? Rail.Min.Y : Rail.Max.Y;
			else Result.X = Side == 1 ? Rail.Max.X : Rail.Min.X;
			return Result;
		};
		auto Perimeter = [&Rail](const FVector& Point, int32 Side) -> float
		{
			const float Width = Rail.Max.X - Rail.Min.X;
			const float Height = Rail.Max.Y - Rail.Min.Y;
			switch (Side)
			{
			case 0: return Point.X - Rail.Min.X;
			case 1: return Width + Point.Y - Rail.Min.Y;
			case 2: return Width * 2.f + Height - (Point.X - Rail.Min.X);
			default: return Width * 2.f + Height * 2.f - (Point.Y - Rail.Min.Y);
			}
		};
		const FVector RailFrom = OnFace(LocalP0, FromFace);
		const FVector RailTo = OnFace(LocalP3, ToFace);
		const float Total = 2.f * ((Rail.Max.X - Rail.Min.X) + (Rail.Max.Y - Rail.Min.Y));
		const float Forward = FMath::Fmod(Perimeter(RailTo, ToFace) - Perimeter(RailFrom, FromFace) + Total, Total);
		const bool bForward = Forward < Total * 0.5f || (FMath::IsNearlyEqual(Forward, Total * 0.5f) && Lane % 2 == 0);
		const FVector Corners[4] = {
			FVector(Rail.Max.X, Rail.Min.Y, LocalP0.Z), FVector(Rail.Max.X, Rail.Max.Y, LocalP0.Z),
			FVector(Rail.Min.X, Rail.Max.Y, LocalP0.Z), FVector(Rail.Min.X, Rail.Min.Y, LocalP0.Z) };
		TArray<FVector> Waypoints;
		Waypoints.Add(LocalP0);
		Waypoints.Add(RailFrom);
		float Remaining = bForward ? Forward : Total - Forward;
		float Along = Perimeter(RailFrom, FromFace);
		for (int32 Step = 0; Step < 4 && Remaining > 0.01f; ++Step)
		{
			const int32 CornerIndex = bForward ? FromFace + Step : (FromFace - 1 - Step + 8) % 4;
			const FVector Corner = Corners[CornerIndex % 4];
			const float CornerAlong = bForward
				? FMath::Fmod(Perimeter(Corner, CornerIndex % 4) - Along + Total, Total)
				: FMath::Fmod(Along - Perimeter(Corner, CornerIndex % 4) + Total, Total);
			if (CornerAlong >= Remaining - 0.01f) break;
			Waypoints.Add(Corner);
			Remaining -= CornerAlong;
			Along = Perimeter(Corner, CornerIndex % 4);
		}
		Waypoints.Add(RailTo);
		Waypoints.Add(LocalP3);
		CurveLength = 0.f;
		FVector Previous = Footprint.TransformPositionNoScale(Waypoints[0]);
		for (int32 Index = 1; Index < Waypoints.Num() - 1; ++Index)
		{
			const FVector Corner = Footprint.TransformPositionNoScale(Waypoints[Index]);
			const FVector Next = Footprint.TransformPositionNoScale(Waypoints[Index + 1]);
			const FVector Incoming = (Corner - Previous).GetSafeNormal2D();
			const FVector Outgoing = (Next - Corner).GetSafeNormal2D();
			if (Incoming.IsNearlyZero() || Outgoing.IsNearlyZero() || FVector::DotProduct(Incoming, Outgoing) > 0.999f)
			{
				AddSideLine(Previous, Corner);
				Previous = Corner;
				continue;
			}
			const float Radius = FMath::Min(
				FMath::Min(110.f, RailClearance * 0.75f),
				FMath::Min(static_cast<float>(FVector::Distance(Previous, Corner)) * 0.25f,
					static_cast<float>(FVector::Distance(Corner, Next)) * 0.25f));
			if (Radius <= KINDA_SMALL_NUMBER)
			{
				AddSideLine(Previous, Corner);
				Previous = Corner;
				continue;
			}
			const FVector Before = Corner - Incoming * Radius;
			const FVector After = Corner + Outgoing * Radius;
			AddSideLine(Previous, Before);
			AddSideSegment(Before, FMath::Lerp(Before, Corner, 2.f / 3.f),
				FMath::Lerp(After, Corner, 2.f / 3.f), After);
			Previous = After;
		}
		AddSideLine(Previous, SecondP3);
	}
	static float PreferredExcursion(const FBox2D& BodyBounds, float Clearance)
	{
		const FVector2D BodySize = BodyBounds.GetSize();
		return FMath::Clamp(static_cast<float>(FMath::Max(BodySize.X, BodySize.Y)) * 0.20f,
			Clearance * 2.f, Clearance * 3.f);
	}

	static float DistanceToFront(const FVector& Origin, const FVector& Direction,
		const FTransform& Footprint, float ExteriorFrontX)
	{
		const FVector LocalOrigin = Footprint.InverseTransformPositionNoScale(Origin);
		const FVector LocalDirection = Footprint.InverseTransformVectorNoScale(Direction);
		if (LocalDirection.X >= -KINDA_SMALL_NUMBER) return 0.f;
		return static_cast<float>(FMath::Max(0.0, (ExteriorFrontX - LocalOrigin.X) / LocalDirection.X));
	}

	static float RayDistanceToRectangle(const FVector& Origin, const FVector& Direction,
		const FTransform& Footprint, const FVector2D& HalfExtent)
	{
		const FVector LocalOrigin = Footprint.InverseTransformPositionNoScale(Origin);
		const FVector LocalDirection = Footprint.InverseTransformVectorNoScale(Direction);
		if (FMath::Abs(LocalOrigin.X) > HalfExtent.X || FMath::Abs(LocalOrigin.Y) > HalfExtent.Y) return 0.f;
		float Limit = TNumericLimits<float>::Max();
		if (LocalDirection.X > KINDA_SMALL_NUMBER) Limit = FMath::Min(Limit, (HalfExtent.X - LocalOrigin.X) / LocalDirection.X);
		else if (LocalDirection.X < -KINDA_SMALL_NUMBER) Limit = FMath::Min(Limit, (-HalfExtent.X - LocalOrigin.X) / LocalDirection.X);
		if (LocalDirection.Y > KINDA_SMALL_NUMBER) Limit = FMath::Min(Limit, (HalfExtent.Y - LocalOrigin.Y) / LocalDirection.Y);
		else if (LocalDirection.Y < -KINDA_SMALL_NUMBER) Limit = FMath::Min(Limit, (-HalfExtent.Y - LocalOrigin.Y) / LocalDirection.Y);
		return Limit == TNumericLimits<float>::Max() ? 0.f : FMath::Max(0.f, Limit);
	}

	FVector EvaluateCurve(float Alpha) const
	{
		const bool bFirst = Alpha <= 0.5f;
		const float T = bFirst ? Alpha * 2.f : (Alpha - 0.5f) * 2.f;
		const float U = 1.f - T;
		const FVector P0 = bFirst ? CurveP0 : CurveP3;
		const FVector P1 = bFirst ? CurveP1 : SecondP1;
		const FVector P2 = bFirst ? CurveP2 : SecondP2;
		const FVector P3 = bFirst ? CurveP3 : SecondP3;
		return (U * U * U) * P0 + (3.f * U * U * T) * P1
			+ (3.f * U * T * T) * P2 + (T * T * T) * P3;
	}

	void BuildDistances()
	{
		ExitLength = static_cast<float>(FVector::Distance(InsideFrom, CurveP0));
		EntryLength = static_cast<float>(FVector::Distance(SecondP3, InsideTo));
		CurveDistances[0] = 0.f;
		FVector Previous = CurveP0;
		for (int32 Index = 1; Index <= CurveSamples; ++Index)
		{
			const FVector Point = EvaluateCurve(static_cast<float>(Index) / CurveSamples);
			CurveDistances[Index] = CurveDistances[Index - 1]
				+ static_cast<float>(FVector::Distance(Previous, Point));
			Previous = Point;
		}
		CurveLength = CurveDistances[CurveSamples];
	}

	float CurveAlphaAtDistance(float Distance) const
	{
		for (int32 Index = 1; Index <= CurveSamples; ++Index)
		{
			if (Distance <= CurveDistances[Index])
			{
				const float Fraction = (Distance - CurveDistances[Index - 1])
					/ FMath::Max(CurveDistances[Index] - CurveDistances[Index - 1], KINDA_SMALL_NUMBER);
				return (static_cast<float>(Index - 1) + Fraction) / CurveSamples;
			}
		}
		return 1.f;
	}
};
