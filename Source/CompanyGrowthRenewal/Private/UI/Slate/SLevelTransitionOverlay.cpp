#include "UI/Slate/SLevelTransitionOverlay.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "UI/Slate/SLoadingTipBand.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	struct FPaintVertex
	{
		FVector2D Position;
		FVector2D UV;
	};

	float CubicOut(float Amount)
	{
		const float Remaining = 1.f - FMath::Clamp(Amount, 0.f, 1.f);
		return 1.f - Remaining * Remaining * Remaining;
	}

	float EdgeAt(float Progress, float Delay, float Range, float Inset, bool bOpening)
	{
		const float Amount = FMath::Clamp((Progress - Delay) / (1.f - Delay), 0.f, 1.f);
		return (Range + Inset) * CubicOut(Amount) - (bOpening ? 0.f : Inset);
	}

	void ClipAgainstLine(TArray<FPaintVertex>& Polygon, float A, float B, float C)
	{
		if (Polygon.IsEmpty())
		{
			return;
		}

		TArray<FPaintVertex, TInlineAllocator<8>> Clipped;
		Clipped.Reserve(Polygon.Num() + 1);
		FPaintVertex Previous = Polygon.Last();
		float PreviousDistance = A * Previous.Position.X + B * Previous.Position.Y + C;
		for (const FPaintVertex& Current : Polygon)
		{
			const float CurrentDistance = A * Current.Position.X + B * Current.Position.Y + C;
			if ((PreviousDistance >= 0.f) != (CurrentDistance >= 0.f))
			{
				const float T = PreviousDistance / (PreviousDistance - CurrentDistance);
				Clipped.Add({ FMath::Lerp(Previous.Position, Current.Position, T),
					FMath::Lerp(Previous.UV, Current.UV, T) });
			}
			if (CurrentDistance >= 0.f)
			{
				Clipped.Add(Current);
			}
			Previous = Current;
			PreviousDistance = CurrentDistance;
		}
		Polygon.Reset(Clipped.Num());
		Polygon.Append(Clipped);
	}

	void ClipToViewport(TArray<FPaintVertex>& Polygon, float Width, float Height)
	{
		ClipAgainstLine(Polygon, 1.f, 0.f, 0.f);
		ClipAgainstLine(Polygon, -1.f, 0.f, Width);
		ClipAgainstLine(Polygon, 0.f, 1.f, 0.f);
		ClipAgainstLine(Polygon, 0.f, -1.f, Height);
	}

	void ClipToPane(TArray<FPaintVertex>& Polygon, float Width, float Height, float Slant,
		float Range, float Inset, float Edge, bool bOpening)
	{
		if (bOpening)
		{
			ClipAgainstLine(Polygon, 1.f, Slant / Height, -Edge);
			ClipAgainstLine(Polygon, -1.f, 0.f, Range + Inset);
		}
		else
		{
			ClipAgainstLine(Polygon, 1.f, 0.f, Inset);
			ClipAgainstLine(Polygon, -1.f, -Slant / Height, Edge);
		}
		ClipToViewport(Polygon, Width, Height);
	}

	TArray<FPaintVertex> MakePane(float Height, float Slant, float Range, float Inset,
		float Edge, bool bOpening)
	{
		TArray<FPaintVertex> Polygon;
		Polygon.Reserve(4);
		if (bOpening)
		{
			Polygon.Add({ FVector2D(Edge, 0.f), FVector2D::ZeroVector });
			Polygon.Add({ FVector2D(Range + Inset, 0.f), FVector2D::ZeroVector });
			Polygon.Add({ FVector2D(Range + Inset, Height), FVector2D::ZeroVector });
			Polygon.Add({ FVector2D(Edge - Slant, Height), FVector2D::ZeroVector });
		}
		else
		{
			Polygon.Add({ FVector2D(-Inset, 0.f), FVector2D::ZeroVector });
			Polygon.Add({ FVector2D(Edge, 0.f), FVector2D::ZeroVector });
			Polygon.Add({ FVector2D(Edge - Slant, Height), FVector2D::ZeroVector });
			Polygon.Add({ FVector2D(-Inset, Height), FVector2D::ZeroVector });
		}
		return Polygon;
	}

	void AddPolygon(const TArray<FPaintVertex>& Polygon, const FSlateRenderTransform& Transform,
		float Width, bool bMirror, FColor Color, TArray<FSlateVertex>& Vertices,
		TArray<SlateIndex>& Indices)
	{
		if (Polygon.Num() < 3 || Vertices.Num() + Polygon.Num() > 65535)
		{
			return;
		}
		const SlateIndex Base = static_cast<SlateIndex>(Vertices.Num());
		for (const FPaintVertex& Point : Polygon)
		{
			const float X = bMirror ? Width - Point.Position.X : Point.Position.X;
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform,
				FVector2f(X, Point.Position.Y), FVector2f(Point.UV), Color));
		}
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			Indices.Add(Base);
			Indices.Add(Base + (bMirror ? Index + 1 : Index));
			Indices.Add(Base + (bMirror ? Index : Index + 1));
		}
	}

	void Submit(FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FSlateResourceHandle& Handle, const TArray<FSlateVertex>& Vertices,
		const TArray<SlateIndex>& Indices)
	{
		if (!Vertices.IsEmpty())
		{
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle,
				Vertices, Indices, nullptr, 0, 0);
		}
	}
}

void FLevelTransitionVisualState::SetSnapshot(const FLevelTransitionSnapshot& InSnapshot)
{
	FScopeLock Lock(&SnapshotLock);
	Snapshot = InSnapshot;
}

FLevelTransitionSnapshot FLevelTransitionVisualState::GetSnapshot() const
{
	FScopeLock Lock(&SnapshotLock);
	return Snapshot;
}

void SLevelTransitionOverlay::Construct(const FArguments& InArgs)
{
	VisualState = InArgs._VisualState;
	TipBody = InArgs._TipText.ToString();
	TipBodyFont = InArgs._TipBodyFont;
	TipLabelFont = InArgs._TipLabelFont;
	ForceVolatile(true);
	if (InArgs._TipIcon)
	{
		TipIconBrush.SetResourceObject(InArgs._TipIcon);
		TipIconBrush.DrawAs = ESlateBrushDrawType::Image;
		bHasTipIcon = true;
	}
	if (InArgs._PatternTexture)
	{
		PatternBrush.SetResourceObject(InArgs._PatternTexture);
		PatternBrush.DrawAs = ESlateBrushDrawType::Image;
		PatternHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(PatternBrush);
	}
}

FVector2D SLevelTransitionOverlay::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector;
}

int32 SLevelTransitionOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Width = Size.X;
	const float Height = Size.Y;
	if (!VisualState.IsValid() || Width <= 0.f || Height <= 0.f)
	{
		return LayerId;
	}

	const FLevelTransitionSnapshot State = VisualState->GetSnapshot();
	const double NowSeconds = FPlatformTime::Seconds();
	const bool bOpening = State.Phase == ELevelTransitionPhase::Opening;
	const float Progress = State.Phase == ELevelTransitionPhase::Holding
		? 1.f : FMath::Clamp(State.Progress, 0.f, 1.f);
	const float Slant = Height * .72f;
	const float Range = Width + Slant;
	const float Inset = Width * .10f;
	const float Delays[3] = { bOpening ? .13f : 0.f, .065f, bOpening ? 0.f : .13f };
	const FColor Colors[3] = { FColor(255, 182, 92), FColor::White, FColor(51, 156, 255) };
	const FSlateRenderTransform Transform = AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();

	float BlueEdge = 0.f;
	for (int32 Layer = 0; Layer < 3; ++Layer)
	{
		const float Edge = EdgeAt(Progress, Delays[Layer], Range, Inset, bOpening);
		TArray<FPaintVertex> Pane = MakePane(Height, Slant, Range, Inset, Edge, bOpening);
		ClipToViewport(Pane, Width, Height);
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		AddPolygon(Pane, Transform, Width, State.bExit, Colors[Layer], Vertices, Indices);
		Submit(OutDrawElements, LayerId + Layer, SolidHandle, Vertices, Indices);
		if (Layer == 2)
		{
			BlueEdge = Edge;
		}
	}

	const bool bBlueVisible = bOpening ? BlueEdge < Width + Slant : BlueEdge > 0.f;
	if (PatternHandle.IsValid() && bBlueVisible)
	{
		const float StampSize = Height * .062f;
		const float TileWidth = StampSize * 1.70f;
		const float TileHeight = StampSize * 1.60f;
		const float Cosine = FMath::Cos(-PI / 12.f);
		const float Sine = FMath::Sin(-PI / 12.f);
		const float ExtentX = (Width * Cosine + Height * FMath::Abs(Sine)) * .5f;
		const float ExtentY = (Width * FMath::Abs(Sine) + Height * Cosine) * .5f;
		const double Elapsed = State.bReduceMotion ? 0.0 : FMath::Max(0.0, NowSeconds - State.PatternStartTime);
		const float OffsetX = static_cast<float>(FMath::Fmod(Elapsed * Height * .05, TileWidth));
		const float OffsetY = -static_cast<float>(FMath::Fmod(Elapsed * Height * .025, TileHeight * 2.f));
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		const FVector2D ScreenCenter(Width * .5f, Height * .5f);
		// 매 프레임 수백 개 스탬프를 찍으므로 버퍼 하나를 재사용한다(클리핑으로 꼭짓점이 늘 수 있어 여유 용량).
		TArray<FPaintVertex> Stamp;
		Stamp.Reserve(8);
		for (int32 Row = FMath::FloorToInt(-ExtentY / TileHeight) - 1;
			Row < FMath::CeilToInt(ExtentY / TileHeight) + 3; ++Row)
		{
			for (int32 Col = FMath::FloorToInt(-ExtentX / TileWidth) - 2;
				Col < FMath::CeilToInt(ExtentX / TileWidth) + 1; ++Col)
			{
				const float X = Col * TileWidth + (Row % 2 == 0 ? 0.f : TileWidth * .5f)
					+ TileWidth * .16f + OffsetX;
				const float Y = Row * TileHeight + TileHeight * .22f + OffsetY;
				const float CenterX = X + StampSize * .5f;
				const float CenterY = Y + StampSize * .5f;
				const float ScreenX = CenterX * Cosine - CenterY * Sine;
				const float ScreenY = CenterX * Sine + CenterY * Cosine;
				const float Distance = FMath::Sqrt(FMath::Square(ScreenX / (Width * .23f))
					+ FMath::Square(ScreenY / (Height * .32f)));
				const float Alpha = .085f * (.45f + .55f * FMath::Clamp(Distance, 0.f, 1.f));
				const FColor Tint = FLinearColor(1.f, 1.f, 1.f, Alpha).ToFColor(true);
				Stamp.Reset();
				const FVector2D Corners[4] = {
					FVector2D(X, Y), FVector2D(X + StampSize, Y),
					FVector2D(X + StampSize, Y + StampSize), FVector2D(X, Y + StampSize)
				};
				const FVector2D UVs[4] = {
					FVector2D(0.f, 0.f), FVector2D(1.f, 0.f),
					FVector2D(1.f, 1.f), FVector2D(0.f, 1.f)
				};
				for (int32 Corner = 0; Corner < 4; ++Corner)
				{
					const float RotatedX = Corners[Corner].X * Cosine - Corners[Corner].Y * Sine;
					const float RotatedY = Corners[Corner].X * Sine + Corners[Corner].Y * Cosine;
					Stamp.Add({ ScreenCenter + FVector2D(RotatedX, RotatedY), UVs[Corner] });
				}
				ClipToPane(Stamp, Width, Height, Slant, Range, Inset, BlueEdge, bOpening);
				AddPolygon(Stamp, Transform, Width, State.bExit, Tint, Vertices, Indices);
			}
		}
		Submit(OutDrawElements, LayerId + 3, PatternHandle, Vertices, Indices);
	}

	if (State.Phase == ELevelTransitionPhase::Holding)
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float X = Width * .5f + (Index - 1) * Height * .065f;
			const FVector2D Start(X + Height * .02f, Height * .46f);
			const FVector2D End(X - Height * .02f, Height * .54f);
			const FVector2D Normal = FVector2D(-(End - Start).Y, (End - Start).X).GetSafeNormal()
				* Height * .006f;
			const float Alpha = State.bReduceMotion ? .75f
				: static_cast<float>(.35 + .65 * (.5 + .5 * FMath::Sin(NowSeconds * 1000.0 / 140.0 - Index * .9)));
			TArray<FPaintVertex> Slash;
			Slash.Reserve(4);
			Slash.Add({ Start - Normal, FVector2D::ZeroVector });
			Slash.Add({ Start + Normal, FVector2D::ZeroVector });
			Slash.Add({ End + Normal, FVector2D::ZeroVector });
			Slash.Add({ End - Normal, FVector2D::ZeroVector });
			ClipToViewport(Slash, Width, Height);
			AddPolygon(Slash, Transform, Width, State.bExit,
				FLinearColor(1.f, 1.f, 1.f, Alpha).ToFColor(true), Vertices, Indices);
		}
		Submit(OutDrawElements, LayerId + 4, SolidHandle, Vertices, Indices);
	}

	if (State.Phase == ELevelTransitionPhase::Holding)
	{
		const float Alpha = State.bReduceMotion ? 1.f
			: FMath::Clamp(static_cast<float>((NowSeconds - State.HoldStartTime) / .35), 0.f, 1.f);
		// 그리는 코드는 UMG 래퍼(UTipBandWidget)와 **같은 함수**다 ― 여기서만 고치면 양쪽이 같이 바뀐다
		return PaintLoadingTipBand(AllottedGeometry, OutDrawElements, LayerId + 5, TipStyle,
			TipBody, TipBodyFont, TipLabelFont, bHasTipIcon ? &TipIconBrush : nullptr, Alpha);
	}

	return LayerId + 5;
}
