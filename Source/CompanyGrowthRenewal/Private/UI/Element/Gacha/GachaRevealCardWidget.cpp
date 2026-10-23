#include "UI/Element/Gacha/GachaRevealCardWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"

// 이름 붙은 namespace ― 익명이면 unity 묶음 뒤쪽 파일의 같은 이름 지역 변수가 C4459(전역 가림)로 빌드를 깬다
namespace GachaRevealCardDetail
{
	const TCHAR* const CardTextureFolder = TEXT("/Game/CompanyGrowth/UI/Textures/GachaBox/Card");
	const TCHAR* const EquipmentEmblemPath = TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_EquipHeader.T_UIIcon_EquipHeader");

	struct FCardClassSpec
	{
		float W;
		float H;
		const TCHAR* Suffix;
		float HotPad;
		FVector2D NewPad;
		FVector2D NewSize;
		FVector2D UpPad;
		FVector2D UpSize;
	};

	// EGachaCardSize 순서. 여백·꼬리표 상자 = bake/out/plate/slices.json _content (apply_pending.py check 가 대조한다)
	const FCardClassSpec ClassSpecs[] = {
		{ 460.f, 460.f, TEXT("Single"), 126.f, FVector2D(16.047, 16.391), FVector2D(102.953, 46.0), FVector2D(16.203, 16.391), FVector2D(59.797, 55.188) },
		{ 290.f, 348.f, TEXT("Ten"), 82.f, FVector2D(11.594, 11.594), FVector2D(64.906, 29.0), FVector2D(11.812, 11.594), FVector2D(37.688, 34.797) },
		{ 224.f, 224.f, TEXT("Thirty"), 65.f, FVector2D(9.672, 9.953), FVector2D(50.141, 22.406), FVector2D(9.703, 9.953), FVector2D(29.109, 26.875) },
		{ 378.f, 378.f, TEXT("Best"), 105.f, FVector2D(14.5, 14.109), FVector2D(84.609, 37.797), FVector2D(14.984, 14.109), FVector2D(49.125, 45.359) },
		{ 560.f, 560.f, TEXT("Spot"), 152.f, FVector2D(19.672, 19.391), FVector2D(125.328, 56.0), FVector2D(19.203, 19.391), FVector2D(72.797, 67.188) },
	};

	const FCardClassSpec& SpecOf(EGachaCardSize Size)
	{
		const int32 Index = FMath::Clamp(static_cast<int32>(Size), 0, static_cast<int32>(UE_ARRAY_COUNT(ClassSpecs)) - 1);
		return ClassSpecs[Index];
	}

	constexpr float FacePad = 10.f;  // 앞·뒷면 그림 = 카드 + 바닥 그림자 여유 10
	constexpr float BadgePad = 2.f;
	constexpr float EmblemRatio = 0.42f;
	constexpr float IconShadowRatio = 0.02f;
	constexpr float TagRight = 0.95f;
	constexpr float TagTop = 0.04f;
	constexpr float TagGap = 0.01f;
	constexpr float FooterSide = 0.06f;
	constexpr float FooterBottom = 0.045f;
	constexpr float FooterGap = 0.005f;
	constexpr float HoldRiseFrom = 0.6f;

	float EaseOut(float T) { return GachaRevealCardLayout::CubicBezier(0.f, 0.f, 0.58f, 1.f, T); }
	float EaseInOut(float T) { return GachaRevealCardLayout::CubicBezier(0.42f, 0.f, 0.58f, 1.f, T); }

	void PlaceAt(UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		if (UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetSize(Size);
		}
	}

	UTexture2D* LoadSurface(const FSoftObjectPath& Path)
	{
		if (Path.IsNull())
		{
			return nullptr;
		}
		UTexture2D* Texture = TSoftObjectPtr<UTexture2D>(Path).LoadSynchronous();
		if (!Texture)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaRevealCard] 텍스처를 불러오지 못함: %s"), *Path.ToString());
		}
		return Texture;
	}

	// 에디터에서 비동기 텍스처 컴파일 중이면 GetSizeX 가 기본 텍스처 32x32 를 준다(Task 4 실측) ― 임포트 크기를 쓴다
	FVector2D TextureSize(const UTexture2D* Texture)
	{
		const FIntPoint Imported = Texture->GetImportedSize();
		if (Imported.X > 0 && Imported.Y > 0)
		{
			return FVector2D(Imported.X, Imported.Y);
		}
		return FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
	}
}

namespace GachaRevealCardLayout
{
	FVector2D DesignSize(EGachaCardSize Size)
	{
		const GachaRevealCardDetail::FCardClassSpec& S = GachaRevealCardDetail::SpecOf(Size);
		return FVector2D(S.W, S.H);
	}

	const TCHAR* ClassSuffix(EGachaCardSize Size)
	{
		return GachaRevealCardDetail::SpecOf(Size).Suffix;
	}

	bool UsesSingleRules(EGachaCardSize Size)
	{
		return Size == EGachaCardSize::Single || Size == EGachaCardSize::Best || Size == EGachaCardSize::Spot;
	}

	FSoftObjectPath SurfacePath(ECardPart Part, EGachaRevealKind Kind, int32 Grade, EGachaCardSize Size)
	{
		const TCHAR* KindName = Kind == EGachaRevealKind::Equipment ? TEXT("Equip") : TEXT("Trait");
		const TCHAR* Suffix = ClassSuffix(Size);
		const bool bGradeOk = Grade >= 0 && Grade <= GachaRevealModel::MaxGrade;
		FString Name;
		switch (Part)
		{
		case ECardPart::Face:
			Name = bGradeOk ? FString::Printf(TEXT("Face_%s_G%d_%s"), KindName, Grade, Suffix) : FString();
			break;
		case ECardPart::Badge:
			Name = bGradeOk ? FString::Printf(TEXT("Badge_%s_G%d_%s"), KindName, Grade, Suffix) : FString();
			break;
		case ECardPart::BackHot:
			Name = bGradeOk && Grade >= GachaRevealModel::HighGrade
				? FString::Printf(TEXT("BackHot_%s_G%d_%s"), KindName, Grade, Suffix) : FString();
			break;
		case ECardPart::Back:
			Name = FString::Printf(TEXT("Back_%s"), Suffix);
			break;
		case ECardPart::New:
			Name = FString::Printf(TEXT("New_%s"), Suffix);
			break;
		case ECardPart::Up:
			Name = FString::Printf(TEXT("Up_%s"), Suffix);
			break;
		}
		if (Name.IsEmpty())
		{
			return FSoftObjectPath();
		}
		return FSoftObjectPath(FString::Printf(TEXT("%s/T_GachaCard_%s.T_GachaCard_%s"), GachaRevealCardDetail::CardTextureFolder, *Name, *Name));
	}

	FSoftObjectPath EmblemPath(EGachaRevealKind Kind)
	{
		using namespace GachaRevealCardDetail;
		switch (Kind)
		{
		case EGachaRevealKind::Trait:
			return FSoftObjectPath(FString::Printf(TEXT("%s/T_GachaCard_Emblem_Trait.T_GachaCard_Emblem_Trait"), CardTextureFolder));
		case EGachaRevealKind::Skin:
			return FSoftObjectPath(FString::Printf(TEXT("%s/T_GachaCard_Emblem_Skin.T_GachaCard_Emblem_Skin"), CardTextureFolder));
		default:
			return FSoftObjectPath(EquipmentEmblemPath);
		}
	}

	FVector2D SurfacePad(ECardPart Part, EGachaCardSize Size)
	{
		using namespace GachaRevealCardDetail;
		const FCardClassSpec& S = SpecOf(Size);
		switch (Part)
		{
		case ECardPart::BackHot: return FVector2D(S.HotPad, S.HotPad);
		case ECardPart::Badge: return FVector2D(BadgePad, BadgePad);
		case ECardPart::New: return S.NewPad;
		case ECardPart::Up: return S.UpPad;
		default: return FVector2D(FacePad, FacePad);
		}
	}

	FVector2D TagSize(ECardPart Part, EGachaCardSize Size)
	{
		const GachaRevealCardDetail::FCardClassSpec& S = GachaRevealCardDetail::SpecOf(Size);
		return Part == ECardPart::Up ? S.UpSize : S.NewSize;
	}

	FGachaRevealRect IconBox(EGachaCardSize Size)
	{
		const GachaRevealCardDetail::FCardClassSpec& S = GachaRevealCardDetail::SpecOf(Size);
		// CSS %: left·width = 카드 폭 기준, top·height = 카드 높이 기준
		if (Size == EGachaCardSize::Single)
		{
			return { 0.22f * S.W, 0.22f * S.H, 0.56f * S.W, 0.56f * S.H };
		}
		if (UsesSingleRules(Size))
		{
			return { 0.10f * S.W, 0.10f * S.H, 0.80f * S.W, 0.80f * S.H };
		}
		if (Size == EGachaCardSize::Ten)
		{
			return { 0.18f * S.W, 0.06f * S.H, 0.64f * S.W, 0.58f * S.H };
		}
		return { 0.14f * S.W, 0.08f * S.H, 0.72f * S.W, 0.72f * S.H };
	}

	FGachaRevealRect FitContain(const FGachaRevealRect& Box, const FVector2D& Texture)
	{
		if (Texture.X <= 0.0 || Texture.Y <= 0.0)
		{
			return Box;
		}
		const float Scale = FMath::Min(Box.W / static_cast<float>(Texture.X), Box.H / static_cast<float>(Texture.Y));
		const float W = static_cast<float>(Texture.X) * Scale;
		const float H = static_cast<float>(Texture.Y) * Scale;
		return { Box.X + (Box.W - W) * 0.5f, Box.Y + (Box.H - H) * 0.5f, W, H };
	}

	float FlipScaleX(float Alpha)
	{
		return FMath::Abs(FMath::Cos(PI * FMath::Clamp(Alpha, 0.f, 1.f)));
	}

	float BreatheOpacity(float Seconds)
	{
		const float Period = GachaRevealTiming::HoldBreathePeriod / 1000.f;
		const float T = FMath::Fmod(FMath::Max(Seconds, 0.f), Period) / Period;
		// 키프레임 0%·100% = .45, 50% = 1, 구간마다 ease-in-out
		return T < 0.5f
			? 0.45f + 0.55f * GachaRevealCardDetail::EaseInOut(T / 0.5f)
			: 1.f - 0.55f * GachaRevealCardDetail::EaseInOut((T - 0.5f) / 0.5f);
	}

	float CubicBezier(float X1, float Y1, float X2, float Y2, float T)
	{
		const float X = FMath::Clamp(T, 0.f, 1.f);
		auto Sample = [](float A1, float A2, float U)
		{
			const float V = 1.f - U;
			return 3.f * V * V * U * A1 + 3.f * V * U * U * A2 + U * U * U;
		};
		// 제어점 x 가 [0,1] 이면 x(u) 는 단조 → 이분법으로 u 를 찾는다
		float Lo = 0.f;
		float Hi = 1.f;
		float U = X;
		for (int32 Step = 0; Step < 30; ++Step)
		{
			const float XU = Sample(X1, X2, U);
			if (FMath::Abs(XU - X) < 1e-6f)
			{
				break;
			}
			(XU < X ? Lo : Hi) = U;
			U = 0.5f * (Lo + Hi);
		}
		return Sample(Y1, Y2, U);
	}
}

void UGachaRevealCardWidget::Configure(const FGachaRevealCardData& InData, EGachaCardSize InSize)
{
	if (!DesignBox || !FlipRoot || !FaceRoot || !BackRoot)
	{
		UE_LOG(LogTemp, Error, TEXT("[GachaRevealCard] 위젯 트리가 없다 ― WBP(UIE_GachaRevealCard)로 만들었는지 확인"));
		return;
	}
	Data = InData;
	Data.Grade = FMath::Clamp(InData.Grade, 0, GachaRevealModel::MaxGrade);
	CardSize = InSize;
	bConfigured = true;

	const FVector2D Design = GachaRevealCardLayout::DesignSize(CardSize);
	DesignBox->SetWidthOverride(Design.X);
	DesignBox->SetHeightOverride(Design.Y);

	ApplySurfaces();
	ApplyIcon();
	ApplyTags();
	ApplyFooter();
	ApplyOrbAndFlashColor();

	bHold = false;
	HoldSeconds = 0.f;
	HoldRiseDuration = 0.f;
	FlashSeconds = -1.f;
	OrbSeconds = 0.f;
	BackHotImage->SetVisibility(ESlateVisibility::Collapsed);
	FlashImage->SetVisibility(ESlateVisibility::Collapsed);
	// 시안 .cd-in 기본 = rotateY(180deg) ― 뿌려질 때는 뒷면
	SetFaceUp(false);
}

void UGachaRevealCardWidget::ApplySurfaces()
{
	using namespace GachaRevealCardLayout;
	using namespace GachaRevealCardDetail;
	const FVector2D Design = DesignSize(CardSize);

	auto Surface = [this](UImage* Image, ECardPart Part)
	{
		UTexture2D* Texture = LoadSurface(SurfacePath(Part, Data.Kind, Data.Grade, CardSize));
		if (!Texture)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return false;
		}
		Image->SetBrushFromTexture(Texture, false);
		PlaceAt(Image, -SurfacePad(Part, CardSize), TextureSize(Texture));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return true;
	};
	Surface(BackImage, ECardPart::Back);
	Surface(FaceImage, ECardPart::Face);
	Surface(BadgeImage, ECardPart::Badge);
	bHotSurfaceLoaded = Data.Grade >= GachaRevealModel::HighGrade && Surface(BackHotImage, ECardPart::BackHot);

	// 시안 .emb = 카드 42%×42% 비율 무시 채움, 가운데
	if (UTexture2D* Emblem = LoadSurface(EmblemPath(Data.Kind)))
	{
		BackEmblem->SetBrushFromTexture(Emblem, false);
		const FVector2D EmblemSize = Design * EmblemRatio;
		PlaceAt(BackEmblem, (Design - EmblemSize) * 0.5, EmblemSize);
		BackEmblem->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		BackEmblem->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGachaRevealCardWidget::ApplyIcon()
{
    using namespace GachaRevealCardDetail;
    IconImage->SetBrush(FSlateBrush());
    IconShadow->SetBrush(FSlateBrush());
    IconImage->SetVisibility(ESlateVisibility::Collapsed);
    IconShadow->SetVisibility(ESlateVisibility::Collapsed);
    if (Data.Icon.IsNull() || !Data.IconUVRegion.IsSet()) return;
    UTexture2D* Icon = Data.Icon.LoadSynchronous();
    if (!Icon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GachaRevealCard] 아이콘을 불러오지 못함: %s"), *Data.Icon.ToString());
        return;
    }
    const FBox2d UV = Data.IconUVRegion.GetValue();
    if (!FMath::IsFinite(UV.Min.X) || !FMath::IsFinite(UV.Min.Y) ||
        !FMath::IsFinite(UV.Max.X) || !FMath::IsFinite(UV.Max.Y) ||
        UV.Min.X < 0.0 || UV.Min.Y < 0.0 || UV.Max.X > 1.0 || UV.Max.Y > 1.0 ||
        UV.Max.X <= UV.Min.X || UV.Max.Y <= UV.Min.Y) return;
    const FVector2D SourceSize = TextureSize(Icon);
    const FVector2D CroppedSize((UV.Max.X - UV.Min.X) * SourceSize.X, (UV.Max.Y - UV.Min.Y) * SourceSize.Y);
    const FGachaRevealRect IconBox = GachaRevealCardLayout::IconBox(CardSize);
    const float SquareSide = FMath::Min(IconBox.W, IconBox.H);
    const FGachaRevealRect SquareBox{
        IconBox.X + (IconBox.W - SquareSide) * 0.5f,
        IconBox.Y + (IconBox.H - SquareSide) * 0.5f,
        SquareSide, SquareSide };
    const FGachaRevealRect Fit = GachaRevealCardLayout::FitContain(SquareBox, CroppedSize);
    const float W = static_cast<float>(GachaRevealCardLayout::DesignSize(CardSize).X);
    IconImage->SetBrushFromTexture(Icon, false);
    IconShadow->SetBrushFromTexture(Icon, false);
    FSlateBrush IconBrush = IconImage->GetBrush();
    IconBrush.SetUVRegion(UV);
    IconImage->SetBrush(IconBrush);
    FSlateBrush ShadowBrush = IconShadow->GetBrush();
    ShadowBrush.SetUVRegion(UV);
    IconShadow->SetBrush(ShadowBrush);
    PlaceAt(IconImage, FVector2D(Fit.X, Fit.Y), FVector2D(Fit.W, Fit.H));
    // filter: drop-shadow(0 .02w 0 rgba(0,0,0,.12)) ― 색·알파는 WBP 의 IconShadow 가 가진다
    PlaceAt(IconShadow, FVector2D(Fit.X, Fit.Y + IconShadowRatio * W), FVector2D(Fit.W, Fit.H));
    IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    IconShadow->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGachaRevealCardWidget::ApplyTags()
{
	using namespace GachaRevealCardLayout;
	using namespace GachaRevealCardDetail;
	const float W = static_cast<float>(DesignSize(CardSize).X);
	const float Right = TagRight * W;
	float TagY = TagTop * W;

	// 시안 .tg = 오른쪽 위 세로 묶음(NEW → ▲ → +nP), 오른쪽 맞춤, 간격 .01w
	auto ImageTag = [&](UImage* Image, ECardPart Part, bool bShow)
	{
		UTexture2D* Texture = bShow ? LoadSurface(SurfacePath(Part, Data.Kind, Data.Grade, CardSize)) : nullptr;
		if (!Texture)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		const FVector2D Element = TagSize(Part, CardSize);
		Image->SetBrushFromTexture(Texture, false);
		PlaceAt(Image, FVector2D(Right - Element.X, TagY) - SurfacePad(Part, CardSize), TextureSize(Texture));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		TagY += static_cast<float>(Element.Y) + TagGap * W;
	};
	ImageTag(NewTag, ECardPart::New, Data.bNew);
	ImageTag(UpTag, ECardPart::Up, Data.Kind == EGachaRevealKind::Equipment && Data.bRecommended);

	const bool bDup = Data.Kind == EGachaRevealKind::Skin && Data.MileageGained > 0;
	if (!bDup)
	{
		DupText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const float Px = DupRatio * W;
	DupText->SetText(FText::FromString(FString::Printf(TEXT("+%dP"), Data.MileageGained)));
	SetTextPx(DupText, Px);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DupText->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		CanvasSlot->SetAutoSize(true);
		// CSS line-height:1 칸 안에 슬레이트 줄 상자를 가운데로
		CanvasSlot->SetPosition(FVector2D(Right, TagY - (PretendardLineBox - 1.f) * 0.5f * Px));
	}
	DupText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGachaRevealCardWidget::ApplyFooter()
{
	using namespace GachaRevealCardLayout;
	using namespace GachaRevealCardDetail;
	NameText->SetVisibility(ESlateVisibility::Collapsed);
	ValueText->SetVisibility(ESlateVisibility::Collapsed);
	ValueNameText->SetVisibility(ESlateVisibility::Collapsed);
	if (UsesSingleRules(CardSize))
	{
		return; // .cd.sz1 .ft{display:none}
	}

	const FVector2D Design = DesignSize(CardSize);
	const float W = static_cast<float>(Design.X);
	const bool bThirty = CardSize == EGachaCardSize::Thirty;

	struct FLine
	{
		UTextBlock* Block;
		FText Value;
		float Px;
		float LineBox;
	};
	TArray<FLine, TInlineAllocator<2>> Lines;
	if (!bThirty && !Data.Name.IsEmpty())
	{
		Lines.Add({ NameText, Data.Name, NameRatio * W, NexonLineBox });
	}
	if (!Data.EffectLine.IsEmpty())
	{
		Lines.Add({ ValueText, Data.EffectLine, (bThirty ? ValueRatioThirty : ValueRatio) * W, PretendardLineBox });
		ValueText->SetColorAndOpacity(bThirty ? ValueInkThirty : ValueInkTen);
	}
	else if (bThirty && !Data.Name.IsEmpty())
	{
		Lines.Add({ ValueNameText, Data.Name, ValueRatioThirty * W, NexonLineBox });
	}

	// .ft = 아래 .045w 에서 위로 쌓는 세로 묶음(줄 높이 1.25, 간격 .005w), 좌우 .06w
	float LineBottom = static_cast<float>(Design.Y) - FooterBottom * W;
	for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
	{
		const FLine& Line = Lines[Index];
		const float CssBox = FooterLineHeight * Line.Px;
		const float CssTop = LineBottom - CssBox;
		Line.Block->SetText(Line.Value);
		SetTextPx(Line.Block, Line.Px);
		PlaceAt(Line.Block,
			FVector2D(FooterSide * W, CssTop + (CssBox - Line.LineBox * Line.Px) * 0.5f),
			FVector2D((1.f - 2.f * FooterSide) * W, Line.LineBox * Line.Px));
		Line.Block->SetVisibility(ESlateVisibility::HitTestInvisible);
		LineBottom = CssTop - FooterGap * W;
	}
}

void UGachaRevealCardWidget::ApplyOrbAndFlashColor()
{
	const FVector2D Design = GachaRevealCardLayout::DesignSize(CardSize);
	GachaRevealCardDetail::PlaceAt(OrbImage, FVector2D::ZeroVector, Design);
	GachaRevealCardDetail::PlaceAt(FlashImage, FVector2D::ZeroVector, Design);

	// 머티리얼은 시안처럼 sRGB 값으로 섞고 마지막에 선형으로 바꾼다 ― 그래서 sRGB 값을 그대로 넘긴다
	const FColor Fx = GradeFx().ToFColorSRGB();
	const bool bOrb = Data.Grade >= GachaRevealModel::HighGrade;
	OrbImage->SetVisibility(bOrb ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bOrb)
	{
		if (UMaterialInstanceDynamic* Orb = OrbMaterial())
		{
			// color-mix(in srgb, gf 60%, #fff)
			const FLinearColor Band((0.6f * Fx.R + 0.4f * 255.f) / 255.f, (0.6f * Fx.G + 0.4f * 255.f) / 255.f,
				(0.6f * Fx.B + 0.4f * 255.f) / 255.f, 1.f);
			Orb->SetVectorParameterValue(TEXT("BandColor"), Band);
			Orb->SetScalarParameterValue(TEXT("Aspect"), static_cast<float>(Design.Y / Design.X));
			Orb->SetScalarParameterValue(TEXT("Angle"), 0.f);
		}
	}
	if (UMaterialInstanceDynamic* Flash = FlashMaterial())
	{
		Flash->SetVectorParameterValue(TEXT("FlashColor"), FLinearColor(Fx.R / 255.f, Fx.G / 255.f, Fx.B / 255.f, 1.f));
		Flash->SetScalarParameterValue(TEXT("Aspect"), static_cast<float>(Design.Y / Design.X));
		Flash->SetScalarParameterValue(TEXT("Opacity"), 0.f);
	}
}

FLinearColor UGachaRevealCardWidget::GradeFx() const
{
	const TArray<FLinearColor>& Palette = Data.Kind == EGachaRevealKind::Equipment ? EquipmentFxColors : TraitFxColors;
	if (!Palette.IsValidIndex(Data.Grade))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealCard] 등급 색 없음(kind=%d grade=%d) ― WBP 팔레트 저작 확인"),
			static_cast<int32>(Data.Kind), Data.Grade);
		return FLinearColor::White;
	}
	return Palette[Data.Grade];
}

UMaterialInstanceDynamic* UGachaRevealCardWidget::OrbMaterial()
{
	if (!OrbMID && OrbImage)
	{
		OrbMID = OrbImage->GetDynamicMaterial();
	}
	return OrbMID;
}

UMaterialInstanceDynamic* UGachaRevealCardWidget::FlashMaterial()
{
	if (!FlashMID && FlashImage)
	{
		FlashMID = FlashImage->GetDynamicMaterial();
	}
	return FlashMID;
}

void UGachaRevealCardWidget::ShowSide(bool bFace)
{
	bFaceShown = bFace;
	FaceRoot->SetVisibility(bFace ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	BackRoot->SetVisibility(bFace ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	// 시안은 도장 때 .hold 를 뺀다(src.html stampGroup) ― 앞면이 되면 숨쉬기를 끝낸다
	if (bFace && bHold)
	{
		SetHoldGlow(false);
	}
}

void UGachaRevealCardWidget::SetTextPx(UTextBlock* Text, float Px)
{
	FSlateFontInfo Font = Text->GetFont();
	// 외곽선은 WBP 가 가진다 ― 저작값을 글자 크기 비율로만 늘인다(저작값 0 이면 0)
	const FAuthoredFont Authored = AuthoredFonts.FindOrAdd(Text, FAuthoredFont{ Font.Size, Font.OutlineSettings.OutlineSize });
	Font.Size = Px * GachaRevealCardLayout::PxToPt;
	Font.OutlineSettings.OutlineSize = Authored.Size > 0.f
		? FMath::RoundToInt(static_cast<float>(Authored.Outline) * Font.Size / Authored.Size)
		: Authored.Outline;
	Text->SetFont(Font);
}

void UGachaRevealCardWidget::SetFaceUp(bool bFaceUp)
{
	if (!FlipRoot)
	{
		return;
	}
	ShowSide(bFaceUp);
	FlipRoot->SetRenderScale(FVector2D(1.f, 1.f));
}

void UGachaRevealCardWidget::SetFlip(float Alpha)
{
	if (!FlipRoot)
	{
		return;
	}
	const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
	ShowSide(Clamped >= 0.5f);
	FlipRoot->SetRenderScale(FVector2D(GachaRevealCardLayout::FlipScaleX(Clamped), 1.f));
}

void UGachaRevealCardWidget::SetHoldGlow(bool bOn)
{
	bHold = bOn && bConfigured && bHotSurfaceLoaded && !bFaceShown && BackHotImage;
	HoldSeconds = 0.f;
	HoldRiseDuration = 0.f;
	if (!BackHotImage)
	{
		return;
	}
	BackHotImage->SetVisibility(bHold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	BackHotImage->SetRenderOpacity(GachaRevealCardLayout::BreatheOpacity(0.f));
}

void UGachaRevealCardWidget::PlayHoldRise(float DurationMs)
{
	if (!bHold || DurationMs <= 0.f)
	{
		return;
	}
	HoldRiseSeconds = 0.f;
	HoldRiseDuration = DurationMs / 1000.f;
	BackHotImage->SetRenderOpacity(GachaRevealCardDetail::HoldRiseFrom);
}

void UGachaRevealCardWidget::PlayFlash(float PeakAlpha)
{
	UMaterialInstanceDynamic* Flash = FlashMaterial();
	if (!Flash)
	{
		return;
	}
	FlashPeak = FMath::Clamp(PeakAlpha, 0.f, 1.f);
	FlashSeconds = 0.f;
	Flash->SetScalarParameterValue(TEXT("Opacity"), FlashPeak);
	FlashImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

UWidget* UGachaRevealCardWidget::GetVisualRoot() const
{
	return VisualRoot;
}

void UGachaRevealCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace GachaRevealCardDetail;
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bHold && BackHotImage)
	{
		HoldSeconds += InDeltaTime;
		float Opacity = GachaRevealCardLayout::BreatheOpacity(HoldSeconds);
		if (HoldRiseDuration > 0.f)
		{
			HoldRiseSeconds += InDeltaTime;
			if (HoldRiseSeconds < HoldRiseDuration)
			{
				Opacity = FMath::Lerp(HoldRiseFrom, 1.f, EaseOut(HoldRiseSeconds / HoldRiseDuration));
			}
			else
			{
				HoldRiseDuration = 0.f;
			}
		}
		BackHotImage->SetRenderOpacity(Opacity);
	}

	if (FlashSeconds >= 0.f)
	{
		FlashSeconds += InDeltaTime;
		const float T = FlashSeconds / (GachaRevealTiming::CardFlash / 1000.f);
		UMaterialInstanceDynamic* Flash = FlashMaterial();
		if (T >= 1.f || !Flash)
		{
			FlashSeconds = -1.f;
			if (Flash)
			{
				Flash->SetScalarParameterValue(TEXT("Opacity"), 0.f);
			}
			FlashImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Flash->SetScalarParameterValue(TEXT("Opacity"), FlashPeak * (1.f - EaseOut(T)));
		}
	}

	if (bConfigured && Data.Grade >= GachaRevealModel::HighGrade && FaceRoot && FaceRoot->IsVisible())
	{
		OrbSeconds += InDeltaTime;
		const float PeriodMs = Data.Grade >= GachaRevealModel::MaxGrade ? GachaRevealTiming::OrbPeriodSSS : GachaRevealTiming::OrbPeriod;
		if (UMaterialInstanceDynamic* Orb = OrbMaterial())
		{
			Orb->SetScalarParameterValue(TEXT("Angle"), FMath::Frac(OrbSeconds / (PeriodMs / 1000.f)));
		}
	}
}
