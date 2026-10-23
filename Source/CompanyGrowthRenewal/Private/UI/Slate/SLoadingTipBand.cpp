#include "UI/Slate/SLoadingTipBand.h"

#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

namespace
{
	/** 가로 알파 램프가 가능한 사각형. 텍스처 없이 정점 알파만으로 양끝 소멸을 만든다 */
	void AddRamp(float Left, float Right, float Top, float Bottom,
		const FSlateRenderTransform& Transform, FColor LeftColor, FColor RightColor,
		TArray<FSlateVertex>& Vertices, TArray<SlateIndex>& Indices)
	{
		if (Right <= Left || Bottom <= Top || Vertices.Num() + 4 > 65535)
		{
			return;
		}
		const SlateIndex Base = static_cast<SlateIndex>(Vertices.Num());
		const FVector2f Zero = FVector2f::ZeroVector;
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Left, Top), Zero, LeftColor));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Right, Top), Zero, RightColor));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Right, Bottom), Zero, RightColor));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Left, Bottom), Zero, LeftColor));
		Indices.Add(Base); Indices.Add(Base + 1); Indices.Add(Base + 2);
		Indices.Add(Base); Indices.Add(Base + 2); Indices.Add(Base + 3);
	}

	/** 중앙은 평평하고 양끝만 소멸하는 띠 한 줄 */
	void AddFadingBar(float Left, float Right, float Top, float Bottom, float FadeWidth,
		const FSlateRenderTransform& Transform, FColor Core,
		TArray<FSlateVertex>& Vertices, TArray<SlateIndex>& Indices)
	{
		const FColor Edge(Core.R, Core.G, Core.B, 0);
		const float Fade = FMath::Min(FadeWidth, (Right - Left) * .5f);
		AddRamp(Left, Left + Fade, Top, Bottom, Transform, Edge, Core, Vertices, Indices);
		AddRamp(Left + Fade, Right - Fade, Top, Bottom, Transform, Core, Core, Vertices, Indices);
		AddRamp(Right - Fade, Right, Top, Bottom, Transform, Core, Edge, Vertices, Indices);
	}

	/** 한 줄이 원칙. 넘치면 2줄까지만 접는다 ― 3줄째가 생기면 밴드가 화면 하단을 통째로 먹는다 */
	TArray<FString> WrapBody(const FString& Body, float MaxWidth, const FSlateFontInfo& Font,
		const TSharedRef<FSlateFontMeasure>& Measure)
	{
		TArray<FString> Lines;
		if (Body.IsEmpty())
		{
			return Lines;
		}
		if (MaxWidth <= 0.f || Measure->Measure(Body, Font).X <= MaxWidth)
		{
			Lines.Add(Body);
			return Lines;
		}
		TArray<FString> Words;
		Body.ParseIntoArray(Words, TEXT(" "), true);
		FString Current;
		for (const FString& Word : Words)
		{
			const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
			if (!Current.IsEmpty() && Measure->Measure(Candidate, Font).X > MaxWidth)
			{
				Lines.Add(Current);
				Current = Word;
			}
			else
			{
				Current = Candidate;
			}
		}
		if (!Current.IsEmpty())
		{
			Lines.Add(Current);
		}
		while (Lines.Num() > 2)
		{
			Lines[1] = Lines[1] + TEXT(" ") + Lines[2];
			Lines.RemoveAt(2);
		}
		return Lines;
	}

	FColor Scaled(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color).CopyWithNewOpacity(Color.A * Alpha).ToFColor(true);
	}
}

int32 PaintLoadingTipBand(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FLoadingTipBandStyle& Style, const FString& Body,
	const UFont* BodyFont, const UFont* LabelFont, const FSlateBrush* IconBrush, float Alpha)
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Width = static_cast<float>(Size.X);
	const float Height = static_cast<float>(Size.Y);
	if (Body.IsEmpty() || !BodyFont || Width <= 0.f || Height <= 0.f || Alpha <= 0.f)
	{
		return LayerId;
	}

	const float BodySize = FMath::Max(8.f, FMath::RoundToFloat(Height * Style.BodySizeRatio));
	const float LabelSize = FMath::Max(8.f, FMath::RoundToFloat(Height * Style.LabelSizeRatio));
	const FSlateFontInfo BodyFontInfo(BodyFont, BodySize);
	const FSlateFontInfo LabelFontInfo(LabelFont ? LabelFont : BodyFont, LabelSize);
	const TSharedRef<FSlateFontMeasure> Measure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	const float BandLeft = Width * Style.BandInsetX;
	const float BandRight = Width * (1.f - Style.BandInsetX);
	const FString Label(TEXT("TIP!"));
	const float LabelWidth = Measure->Measure(Label, LabelFontInfo).X;
	const float LabelGap = Height * .0139f;
	const bool bHasIcon = IconBrush && IconBrush->GetResourceObject() != nullptr;
	const float IconSize = bHasIcon ? BodySize * Style.IconScale : 0.f;
	const float IconGap = bHasIcon ? Height * .0097f : 0.f;

	const float TextLimit = (BandRight - BandLeft) * .88f - IconSize - IconGap - LabelWidth - LabelGap;
	const TArray<FString> Lines = WrapBody(Body, TextLimit, BodyFontInfo, Measure);
	if (Lines.IsEmpty())
	{
		return LayerId;
	}

	const float LabelHeight = static_cast<float>(Measure->GetMaxCharacterHeight(LabelFontInfo));
	const float BodyHeight = static_cast<float>(Measure->GetMaxCharacterHeight(BodyFontInfo));
	const float LineGap = Height * .0062f;
	const float PadV = Height * Style.BandPadV;
	const float ContentHeight = BodyHeight * static_cast<float>(Lines.Num())
		+ LineGap * static_cast<float>(FMath::Max(0, Lines.Num() - 1));
	const float BandBottom = Height * Style.BandBottom;
	const float BandTop = BandBottom - (ContentHeight + PadV * 2.f);

	const FSlateRenderTransform Transform = AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();
	const float FadeWidth = (BandRight - BandLeft) * Style.FadeRatio;
	const float LineThickness = FMath::Max(1.f, FMath::RoundToFloat(Height * .0021f));
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
	AddFadingBar(BandLeft, BandRight, BandTop, BandBottom, FadeWidth, Transform,
		Scaled(Style.BandTint, Alpha), Vertices, Indices);
	AddFadingBar(BandLeft, BandRight, BandTop - LineThickness, BandTop, FadeWidth, Transform,
		Scaled(Style.HairlineTint, Alpha), Vertices, Indices);
	AddFadingBar(BandLeft, BandRight, BandBottom, BandBottom + LineThickness, FadeWidth, Transform,
		Scaled(Style.HairlineTint, Alpha), Vertices, Indices);
	if (!Vertices.IsEmpty())
	{
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, FSlateResourceHandle(),
			Vertices, Indices, nullptr, 0, 0);
	}

	// 크기가 다른 두 글꼴은 윗변이 아니라 베이스라인을 맞춘다 ― 윗변을 맞추면 라벨이 떠 보인다
	const float BaselineDrop = static_cast<float>(Measure->GetBaseline(BodyFontInfo))
		- static_cast<float>(Measure->GetBaseline(LabelFontInfo));
	const FLinearColor LabelTint = FLinearColor(Style.LabelTint).CopyWithNewOpacity(Style.LabelTint.A * Alpha);
	const FLinearColor BodyTint = FLinearColor(Style.BodyTint).CopyWithNewOpacity(Style.BodyTint.A * Alpha);

	float TextY = BandTop + PadV;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const float LineWidth = Measure->Measure(Lines[Index], BodyFontInfo).X;
		// 아이콘+라벨은 첫 줄에만 붙는다 ― 접힌 둘째 줄은 본문만 가운데 정렬
		const float Lead = Index == 0 ? IconSize + IconGap + LabelWidth + LabelGap : 0.f;
		const float StartX = (Width - (Lead + LineWidth)) * .5f;
		if (Index == 0)
		{
			if (bHasIcon)
			{
				const float IconY = TextY + (BodyHeight - IconSize) * .5f;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
					AllottedGeometry.ToPaintGeometry(FVector2f(IconSize, IconSize),
						FSlateLayoutTransform(FVector2f(StartX, IconY))),
					IconBrush, ESlateDrawEffect::None,
					FLinearColor(Style.IconTint).CopyWithNewOpacity(Style.IconTint.A * Alpha));
			}
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
				AllottedGeometry.ToPaintGeometry(FVector2f(LabelWidth, LabelHeight),
					FSlateLayoutTransform(FVector2f(StartX + IconSize + IconGap, TextY + BaselineDrop))),
				Label, LabelFontInfo, ESlateDrawEffect::None, LabelTint);
		}
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2f(LineWidth, BodyHeight),
				FSlateLayoutTransform(FVector2f(StartX + Lead, TextY))),
			Lines[Index], BodyFontInfo, ESlateDrawEffect::None, BodyTint);
		TextY += BodyHeight + LineGap;
	}
	return LayerId + 2;
}

void SLoadingTipBand::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	Body = InArgs._Body;
	BodyFont = InArgs._BodyFont;
	LabelFont = InArgs._LabelFont;
	SetIcon(InArgs._IconTexture);
	SetCanTick(false);
}

void SLoadingTipBand::SetIcon(UTexture2D* InIcon)
{
	bHasIcon = InIcon != nullptr;
	if (bHasIcon)
	{
		IconBrush.SetResourceObject(InIcon);
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	else
	{
		IconBrush = FSlateBrush();
	}
}

FVector2D SLoadingTipBand::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// 전폭 밴드라 부모가 주는 크기를 그대로 쓴다
	return FVector2D::ZeroVector;
}

int32 SLoadingTipBand::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return PaintLoadingTipBand(AllottedGeometry, OutDrawElements, LayerId, Style, Body,
		BodyFont, LabelFont, bHasIcon ? &IconBrush : nullptr, 1.f);
}
