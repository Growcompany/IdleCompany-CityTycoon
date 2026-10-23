#include "UI/Element/Effects/ScoreOrbContainerWidget.h"
#include "UI/Element/Effects/StageGoalTabWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Enum/WidgetType.h"
#include "Framework/Application/SlateApplication.h"
#include "Global/GlobalUtilFunctions.h"
#include "HAL/IConsoleManager.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

// 수치는 승인 목업(docs/05_UI/mockups/OfficeDevCompleteFx_2026-10-14)의 1024×640 화면 단위다.
// 실제 캔버스에서는 높이 비율(DevFxScale)로 키우고, 위치는 화면 비율로 놓는다.
namespace
{
	constexpr float DevFxRefHeight = 640.f;
	constexpr float DevFxLifetime = 1.6f;
	constexpr float DevFxDimRamp = .18f;
	constexpr float DevFxDimFade = .22f;
	constexpr float DevFxDimTail = .35f;
	constexpr float DevFxDimStrength = .32f;
	constexpr float DevFxDimEdge = 1.3f;
	constexpr float DevFxDimRelief = .4f;
	constexpr int32 DevFxMaxStreaks = 96;
	constexpr int32 DevFxEmbersPerBurst = 12;
	constexpr float DevFxLaunch = .14f;
	constexpr float DevFxDrag = 7.f;

	struct FDevFxBurst { float Time; FVector2D Center; int32 Count; double Seed; };
	// 첫 발사 꼬리가 타이머 0 에서 바로 시작하도록 목업 시각에 발사 시간(0.06)을 더했다
	const FDevFxBurst DevFxBursts[] = {
		{ .14f, FVector2D(.20f, .40f), 27, 11.0 },
		{ .30f, FVector2D(.81f, .33f), 24, 23.0 },
		{ .44f, FVector2D(.63f, .66f), 22, 37.0 }
	};
	constexpr int32 DevFxBurstCount = 3;
	static_assert(UE_ARRAY_COUNT(DevFxBursts) == DevFxBurstCount, "burst table size");
	FLinearColor DevFxColor(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }
	const FLinearColor DevFxGlow[] = { DevFxColor(TEXT("#FFC24A")), DevFxColor(TEXT("#5DB8FF")), DevFxColor(TEXT("#FFC24A")) };
	const FLinearColor DevFxHot[] = { DevFxColor(TEXT("#FFF6D8")), DevFxColor(TEXT("#EEF8FF")), DevFxColor(TEXT("#FFF6D8")) };

	double DevFxRandom(double Seed)
	{
		const double Value = FMath::Sin(Seed * 127.1 + 311.7) * 43758.5453;
		return Value - FMath::FloorToDouble(Value);
	}
	float DevFxOut3(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Pow(1.f - T, 3.f); }
	float DevFxOut2(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Square(1.f - T); }
	FLinearColor DevFxAlpha(FLinearColor Color, float Alpha) { Color.A = FMath::Clamp(Alpha, 0.f, 1.f); return Color; }

	// 입자마다 위젯/드로우콜을 만들지 않고 색상 정점 묶음으로 모은다.
	struct FDevFxBatch
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		FSlateRenderTransform Transform;
		explicit FDevFxBatch(const FGeometry& Geo, int32 ExpectedQuads = 64) : Transform(Geo.ToPaintGeometry().GetAccumulatedRenderTransform())
		{
			Vertices.Reserve(ExpectedQuads * 4);
			Indices.Reserve(ExpectedQuads * 6);
		}
		SlateIndex Vertex(FVector2D Point, const FLinearColor& Color, FVector2f UV = FVector2f(.5f, .5f))
		{
			const SlateIndex Index = static_cast<SlateIndex>(Vertices.Num());
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Point),
				FVector4f(UV.X, UV.Y, 1.f, 1.f), UV, Color.ToFColor(true)));
			return Index;
		}
		bool Room(int32 Count) const { return Vertices.Num() + Count <= 60000; }
		void Quad(FVector2D A, FVector2D B, FVector2D C, FVector2D D, const FLinearColor& CA, const FLinearColor& CB,
			const FLinearColor& CC, const FLinearColor& CD)
		{
			if (!Room(4)) return;
			const SlateIndex Base = Vertex(A, CA);
			Vertex(B, CB); Vertex(C, CC); Vertex(D, CD);
			for (int32 Index : { 0, 1, 2, 0, 2, 3 }) Indices.Add(Base + Index);
		}
		// 칼끝: 앞은 뾰족하고 16% 뒤가 가장 넓으며 꼬리로 사라지는 연 모양
		void Blade(FVector2D Tip, float Angle, float Length, float Width, const FLinearColor& Glow, float Alpha)
		{
			if (Alpha <= .002f || Length <= .5f) return;
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle)), Normal(-Dir.Y, Dir.X);
			const FVector2D Tail = Tip - Dir * Length, Mid = Tip - Dir * Length * .16f;
			const float GlowW = Width * 2.8f * .5f, CoreW = Width * .5f;
			Quad(Tip + Dir * GlowW, Mid + Normal * GlowW, Tail, Mid - Normal * GlowW,
				DevFxAlpha(Glow, .6f * Alpha), DevFxAlpha(Glow, .46f * Alpha), DevFxAlpha(Glow, 0.f), DevFxAlpha(Glow, .46f * Alpha));
			Quad(Tip + Dir * CoreW, Mid + Normal * CoreW, Tail, Mid - Normal * CoreW,
				DevFxAlpha(FLinearColor::White, Alpha), DevFxAlpha(FLinearColor::White, .86f * Alpha),
				DevFxAlpha(FLinearColor::White, 0.f), DevFxAlpha(FLinearColor::White, .86f * Alpha));
		}
		// 네 갈래 반짝임 — 마름모 두 장을 십자로 겹친다
		void Glint(FVector2D Center, float Radius, const FLinearColor& Color)
		{
			if (Radius <= .4f || Color.A <= .01f) return;
			const float Long = Radius * 1.9f, Thin = Radius * .35f;
			Quad(Center + FVector2D(0, -Long), Center + FVector2D(Thin, 0), Center + FVector2D(0, Long), Center + FVector2D(-Thin, 0), Color, Color, Color, Color);
			Quad(Center + FVector2D(-Long, 0), Center + FVector2D(0, -Thin), Center + FVector2D(Long, 0), Center + FVector2D(0, Thin), Color, Color, Color, Color);
		}
		// 충격파: 테두리 선이 아니라 가운데만 진하고 안팎으로 사라지는 번진 띠
		void Ring(FVector2D Center, float Radius, float Width, const FLinearColor& Color)
		{
			constexpr int32 Segments = 40;
			if (!Room(Segments * 8)) return;
			const FLinearColor Clear = DevFxAlpha(Color, 0.f);
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				const float A0 = Index * 2.f * PI / Segments, A1 = (Index + 1) * 2.f * PI / Segments;
				const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
				const float In = FMath::Max(0.f, Radius - Width * .5f), Out = Radius + Width * .5f;
				Quad(Center + D0 * In, Center + D1 * In, Center + D1 * Radius, Center + D0 * Radius, Clear, Clear, Color, Color);
				Quad(Center + D0 * Radius, Center + D1 * Radius, Center + D1 * Out, Center + D0 * Out, Color, Color, Clear, Clear);
			}
		}
		void Light(FVector2D Center, float Radius, const FLinearColor& Color)
		{
			if (!Room(4) || Radius <= .5f || Color.A <= .002f) return;
			const SlateIndex Base = Vertex(Center + FVector2D(-Radius, -Radius), Color, FVector2f(0.f, 0.f));
			Vertex(Center + FVector2D(Radius, -Radius), Color, FVector2f(1.f, 0.f));
			Vertex(Center + FVector2D(Radius, Radius), Color, FVector2f(1.f, 1.f));
			Vertex(Center + FVector2D(-Radius, Radius), Color, FVector2f(0.f, 1.f));
			for (int32 Index : { 0, 1, 2, 0, 2, 3 }) Indices.Add(Base + Index);
		}
		void Submit(FSlateWindowElementList& Elements, int32 Layer, UMaterialInterface* Material)
		{
			if (Vertices.IsEmpty()) return;
			FSlateBrush Brush;
			Brush.SetResourceObject(Material);
			const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(
				Material ? Brush : *FCoreStyle::Get().GetBrush("WhiteBrush"));
			FSlateDrawElement::MakeCustomVerts(Elements, Layer, Handle, Vertices, Indices, nullptr, 0, 0);
		}
	};
}

class SDevelopmentEffectsLayer final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDevelopmentEffectsLayer) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UScoreOrbContainerWidget>, EffectOwner)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		EffectOwner = Args._EffectOwner;
		ForceVolatile(true);
	}
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const UScoreOrbContainerWidget* Owner = EffectOwner.Get();
		return Owner && Owner->bDevelopmentPresentation
			? Owner->PaintDevelopmentEffects(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle)
			: LayerId;
	}

private:
	TWeakObjectPtr<UScoreOrbContainerWidget> EffectOwner;
};

TSharedRef<SWidget> UScoreOrbContainerWidget::RebuildWidget()
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SDevelopmentEffectsLayer)
			.EffectOwner(TWeakObjectPtr<UScoreOrbContainerWidget>(this))
			.Visibility(EVisibility::HitTestInvisible)
		]
		+ SOverlay::Slot()
		[
			Super::RebuildWidget()
		];
}

void UScoreOrbContainerWidget::SetDevelopmentPresentationEnabled(bool bEnabled)
{
	if (bDevelopmentPresentation == bEnabled) return;
	bDevelopmentPresentation = bEnabled;
	ResetDevelopmentEffects();
	if (!bEnabled) return;
	TSoftObjectPtr<UMaterialInterface> LightAsset(FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_DevelopmentLight.M_UI_DevelopmentLight")));
	if (UMaterialInterface* LightParent = LightAsset.LoadSynchronous())
	{
		DevelopmentLightMaterial = UMaterialInstanceDynamic::Create(LightParent, this);
	}
	else UE_LOG(LogTemp, Warning, TEXT("[OfficeDevVFX] Missing M_UI_DevelopmentLight"));
}

void UScoreOrbContainerWidget::SetDevelopmentStripRect(FVector2D MinLocal, FVector2D MaxLocal)
{
	DevelopmentStripMin = MinLocal;
	DevelopmentStripMax = MaxLocal;
}

void UScoreOrbContainerWidget::PlayDevelopmentGoalTab(int32 CellIndex, FVector2D CellBottomLeftLocal, float CellWidth)
{
	if (!bDevelopmentPresentation || !OrbCanvas || CellWidth < 4.f) return;
	UGameInstance* GameInstance = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> TabClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::StageGoalTab) : nullptr;
	if (!TabClass || !TabClass->IsChildOf(UStageGoalTabWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeDevVFX] Missing StageGoalTab class in DT_WidgetClass"));
		return;
	}
	if (TObjectPtr<UStageGoalTabWidget>* Existing = ActiveGoalTabs.Find(CellIndex))
	{
		if (*Existing) (*Existing)->RemoveFromParent();
		ActiveGoalTabs.Remove(CellIndex);
	}
	UStageGoalTabWidget* Tab = CreateWidget<UStageGoalTabWidget>(this, TabClass);
	UCanvasPanelSlot* TabSlot = Tab ? OrbCanvas->AddChildToCanvas(Tab) : nullptr;
	if (!TabSlot) return;
	// 칸 가운데에 맞추고, 판 곡선·그림자만큼 좌우로 넓히며, 위는 상단 바 아래 가장자리 밑으로 살짝 숨긴다
	const FVector2D TabPadding = Tab->GetPanelPadding();
	const float BodyWidth = Tab->GetBodyWidthFor(CellWidth);
	const float CellCenterX = CellBottomLeftLocal.X + CellWidth * .5f;
	TabSlot->SetAutoSize(false);
	TabSlot->SetPosition(FVector2D(CellCenterX - BodyWidth * .5f - TabPadding.X, CellBottomLeftLocal.Y - TabPadding.Y));
	TabSlot->SetSize(FVector2D(BodyWidth + TabPadding.X * 2.f, Tab->GetDesignHeight()));
	TabSlot->SetZOrder(40);
	ActiveGoalTabs.Add(CellIndex, Tab);
	Tab->Play(USettingsManagerSubsystem::IsReduceMotion(this));
}

void UScoreOrbContainerWidget::ClearGoalTabs()
{
	for (const TPair<int32, TObjectPtr<UStageGoalTabWidget>>& Pair : ActiveGoalTabs)
	{
		if (Pair.Value) Pair.Value->RemoveFromParent();
	}
	ActiveGoalTabs.Reset();
}

void UScoreOrbContainerWidget::PlayDevelopmentFireworks(float Intensity)
{
	if (!bDevelopmentPresentation) return;
	DevelopmentStreaks.Reset();
	DevelopmentEmbers.Reset();
	DevelopmentFireworkElapsed = 0.f;
	DevelopmentDimFade = -1.f;
	// 모션 줄이기에서는 막만 깔고 움직이는 불꽃은 만들지 않는다
	if (USettingsManagerSubsystem::IsReduceMotion(this)) return;
	const float Density = FMath::Clamp(Intensity, .8f, 1.22f);
	for (int32 Burst = 0; Burst < DevFxBurstCount; ++Burst)
	{
		const FDevFxBurst& Info = DevFxBursts[Burst];
		const int32 Count = FMath::Min(FMath::RoundToInt(Info.Count * Density), DevFxMaxStreaks / DevFxBurstCount);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Seed = Info.Seed + Index * 7.31;
			FDevelopmentStreak Streak;
			Streak.Kind = Index % 5 == 0 ? 2 : (Index % 3 == 2 ? 0 : 1);
			Streak.Angle = Index * 2.f * PI / Count + (DevFxRandom(Seed) - .5) * .42;
			Streak.Speed = Streak.Kind == 2 ? 1150.f + DevFxRandom(Seed + 1.0) * 250.f
				: Streak.Kind == 1 ? 520.f + DevFxRandom(Seed + 1.0) * 520.f : 300.f + DevFxRandom(Seed + 1.0) * 300.f;
			Streak.Lifetime = Streak.Kind == 2 ? .5f
				: Streak.Kind == 1 ? .38f + DevFxRandom(Seed + 2.0) * .2f : .3f + DevFxRandom(Seed + 2.0) * .15f;
			Streak.Width = Streak.Kind == 2 ? 3.2f : Streak.Kind == 1 ? 2.4f : 1.5f;
			Streak.BurstIndex = Burst;
			Streak.bGlint = Streak.Kind > 0 && Index % 2 == 0;
			DevelopmentStreaks.Add(Streak);
		}
		for (int32 Index = 0; Index < DevFxEmbersPerBurst; ++Index)
		{
			const double Seed = Info.Seed * 3.0 + Index * 5.17;
			FDevelopmentEmber Ember;
			Ember.Start = .14f + DevFxRandom(Seed) * .1f;
			Ember.Lifetime = .7f + DevFxRandom(Seed + 1.0) * .45f;
			Ember.Angle = DevFxRandom(Seed + 2.0) * 2.f * PI;
			Ember.Radius = 40.f + DevFxRandom(Seed + 3.0) * 55.f;
			Ember.Drift = DevFxRandom(Seed + 4.0) - .5f;
			Ember.Phase = static_cast<float>(Seed);
			Ember.BurstIndex = Burst;
			DevelopmentEmbers.Add(Ember);
		}
	}
}

void UScoreOrbContainerWidget::FadeDevelopmentDim()
{
	if (DevelopmentFireworkElapsed >= 0.f && DevelopmentDimFade < 0.f) DevelopmentDimFade = 0.f;
}

float UScoreOrbContainerWidget::GetDevelopmentDimAlpha() const
{
	if (DevelopmentFireworkElapsed < 0.f) return 0.f;
	float Alpha = DevFxOut3(DevelopmentFireworkElapsed / DevFxDimRamp)
		* FMath::Clamp((DevFxLifetime - DevelopmentFireworkElapsed) / DevFxDimTail, 0.f, 1.f);
	if (DevelopmentDimFade >= 0.f) Alpha *= 1.f - DevFxOut2(DevelopmentDimFade / DevFxDimFade);
	return Alpha;
}

void UScoreOrbContainerWidget::ResetDevelopmentEffects()
{
	ClearGoalTabs();
	DevelopmentStreaks.Reset();
	DevelopmentEmbers.Reset();
	DevelopmentFireworkElapsed = -1.f;
	DevelopmentDimFade = -1.f;
	bDevelopmentPaused = false;
	for (FOrbAnimData& Orb : ActiveOrbs) Orb.DevelopmentTrail.Reset();
}

void UScoreOrbContainerWidget::TickDevelopmentEffects(float DeltaTime)
{
	if (USettingsManagerSubsystem::IsReduceMotion(this))
	{
		DevelopmentStreaks.Reset();
		DevelopmentEmbers.Reset();
		for (FOrbAnimData& Orb : ActiveOrbs) Orb.DevelopmentTrail.Reset();
	}
	for (auto It = ActiveGoalTabs.CreateIterator(); It; ++It)
	{
		if (!It->Value || It->Value->IsFinished())
		{
			if (It->Value) It->Value->RemoveFromParent();
			It.RemoveCurrent();
		}
	}
	if (DevelopmentFireworkElapsed >= 0.f)
	{
		DevelopmentFireworkElapsed += DeltaTime;
		if (DevelopmentDimFade >= 0.f) DevelopmentDimFade += DeltaTime;
		if (DevelopmentFireworkElapsed >= DevFxLifetime)
		{
			DevelopmentStreaks.Reset();
			DevelopmentEmbers.Reset();
			DevelopmentFireworkElapsed = -1.f;
			DevelopmentDimFade = -1.f;
		}
	}
}

void UScoreOrbContainerWidget::NativeDestruct()
{
	ResetDevelopmentEffects();
	ActiveOrbs.Reset();
	ActiveNumbers.Reset();
	ActiveConfetti.Reset();
	ActiveStamps.Reset();
	if (OrbCanvas) OrbCanvas->ClearChildren();
	Super::NativeDestruct();
}

int32 UScoreOrbContainerWidget::PaintDevelopmentEffects(const FGeometry& Geometry, FSlateWindowElementList& Elements,
	int32 LayerId, const FWidgetStyle& WidgetStyle) const
{
	const bool bHasTrail = ActiveOrbs.ContainsByPredicate([](const FOrbAnimData& Orb) { return Orb.DevelopmentTrail.Num() > 1; });
	if (DevelopmentFireworkElapsed < 0.f && !bHasTrail) return LayerId;
	const FVector2D Scene = Geometry.GetLocalSize();
	if (Scene.X < 100.f || Scene.Y < 100.f) return LayerId;
	const float ParentAlpha = WidgetStyle.GetColorAndOpacityTint().A;
	const float K = Scene.Y / DevFxRefHeight;

	// 1) 화면 전체 막 — 가장자리는 진하게, 상단 바 쪽은 경계 없이 옅게. 바 아래만 칠하던 사각 막은 결과 창 제목을 가로질렀다
	const float Dim = GetDevelopmentDimAlpha() * ParentAlpha;
	if (Dim > 0.f)
	{
		constexpr int32 Columns = 16, Rows = 10;
		FDevFxBatch DimBatch(Geometry, Columns * Rows);
		const FVector2D Center(Scene.X * .5f, Scene.Y * (330.f / DevFxRefHeight));
		const float StripBottom = DevelopmentStripMax.Y > 1.f ? DevelopmentStripMax.Y : Scene.Y * (124.f / DevFxRefHeight);
		const float ReliefEnd = StripBottom + Scene.Y * (126.f / DevFxRefHeight);
		const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(8, 14, 22));
		const auto DimAt = [&](FVector2D Point)
		{
			const float R = FVector2D::Distance(Point, Center) / Scene.Y;
			const float Edge = FMath::Min(.9f, DevFxDimStrength * DevFxDimEdge);
			const float Vignette = R < .5f ? FMath::Lerp(DevFxDimStrength * .78f, DevFxDimStrength * .92f, R / .5f)
				: FMath::Lerp(DevFxDimStrength * .92f, Edge, FMath::Clamp((R - .5f) / .5f, 0.f, 1.f));
			const float Keep = 1.f - DevFxDimRelief;
			const float Relief = Point.Y <= StripBottom ? Keep
				: FMath::Lerp(Keep, 1.f, FMath::Clamp((Point.Y - StripBottom) / FMath::Max(1.f, ReliefEnd - StripBottom), 0.f, 1.f));
			return DevFxAlpha(Ink, Vignette * Relief * Dim);
		};
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				const FVector2D A(Scene.X * Column / Columns, Scene.Y * Row / Rows);
				const FVector2D C(Scene.X * (Column + 1) / Columns, Scene.Y * (Row + 1) / Rows);
				const FVector2D B(C.X, A.Y), D(A.X, C.Y);
				DimBatch.Quad(A, B, C, D, DimAt(A), DimAt(B), DimAt(C), DimAt(D));
			}
		}
		DimBatch.Submit(Elements, LayerId, nullptr);
	}

	// 2) 날카로운 불꽃 세 번 + 상단 바 광택 — 가산 재질
	if (DevelopmentFireworkElapsed >= 0.f)
	{
		const float Time = DevelopmentFireworkElapsed;
		FDevFxBatch Glow(Geometry, 64), Core(Geometry, DevelopmentStreaks.Num() * 2 + DevelopmentEmbers.Num() * 2 + 32);
		for (int32 Burst = 0; Burst < DevFxBurstCount; ++Burst)
		{
			const FDevFxBurst& Info = DevFxBursts[Burst];
			const FVector2D Center(Info.Center.X * Scene.X, Info.Center.Y * Scene.Y);
			const float Age = Time - Info.Time;
			const FLinearColor& GlowColor = DevFxGlow[Burst];
			const FLinearColor& HotColor = DevFxHot[Burst];
			if (Age >= -DevFxLaunch && Age < 0.f && !DevelopmentStreaks.IsEmpty())
			{
				const float P = DevFxOut2((Age + DevFxLaunch) / DevFxLaunch);
				const FVector2D Start(Center.X + (Burst % 2 ? -26.f : 26.f) * K, Scene.Y + 8.f * K);
				const FVector2D Head = FMath::Lerp(Start, Center, P);
				const FVector2D Delta = Center - Start;
				Core.Blade(Head, FMath::Atan2(Delta.Y, Delta.X), 96.f * K, 3.2f * K, GlowColor, .9f * ParentAlpha);
				Glow.Light(Head, 12.f * K, DevFxAlpha(GlowColor, .6f * ParentAlpha));
			}
			if (Age < 0.f || Age > 1.3f) continue;
			if (Age < .16f)
			{
				const float Fade = 1.f - Age / .16f;
				Glow.Light(Center, (30.f + Age * 240.f) * K, DevFxAlpha(HotColor, Fade * ParentAlpha));
				Glow.Light(Center, 96.f * K, DevFxAlpha(GlowColor, .5f * Fade * ParentAlpha));
			}
			if (Age < .24f)
			{
				const float F = Age / .24f;
				Core.Ring(Center, (12.f + 96.f * DevFxOut3(F)) * K, (10.f * (1.f - F) + 2.f) * K, DevFxAlpha(GlowColor, .18f * (1.f - F) * ParentAlpha));
			}
		}
		for (const FDevelopmentStreak& Streak : DevelopmentStreaks)
		{
			const FDevFxBurst& Info = DevFxBursts[Streak.BurstIndex];
			const float Age = Time - Info.Time;
			if (Age < 0.f || Age > Streak.Lifetime) continue;
			const FVector2D Center(Info.Center.X * Scene.X, Info.Center.Y * Scene.Y);
			const float Reach = Streak.Speed / DevFxDrag * (1.f - FMath::Exp(-DevFxDrag * Age));
			const float Behind = Streak.Speed / DevFxDrag * (1.f - FMath::Exp(-DevFxDrag * FMath::Max(0.f, Age - .06f)));
			const FVector2D Dir(FMath::Cos(Streak.Angle), FMath::Sin(Streak.Angle));
			const FVector2D Head = Center + (Dir * Reach + FVector2D(0.f, 16.f * Age * Age)) * K;
			const float Fade = Age < Streak.Lifetime * .4f ? 1.f : 1.f - (Age - Streak.Lifetime * .4f) / (Streak.Lifetime * .6f);
			Core.Blade(Head, Streak.Angle, ((Reach - Behind) * 1.3f + 4.f) * K, Streak.Width * K, DevFxGlow[Streak.BurstIndex], Fade * ParentAlpha);
			if (Streak.bGlint && Age > .1f && Age < .5f)
			{
				const float Twinkle = FMath::Square(FMath::Sin((Age - .1f) / .4f * PI));
				Core.Glint(Head, 3.f * Twinkle * K, DevFxAlpha(DevFxHot[Streak.BurstIndex], Fade * Twinkle * ParentAlpha));
			}
		}
		for (const FDevelopmentEmber& Ember : DevelopmentEmbers)
		{
			const FDevFxBurst& Info = DevFxBursts[Ember.BurstIndex];
			const float Age = Time - Info.Time - Ember.Start;
			if (Age < 0.f || Age > Ember.Lifetime) continue;
			const FVector2D Center(Info.Center.X * Scene.X, Info.Center.Y * Scene.Y);
			const FVector2D Point = Center + FVector2D(FMath::Cos(Ember.Angle) * Ember.Radius + Ember.Drift * 10.f * Age,
				FMath::Sin(Ember.Angle) * Ember.Radius + 14.f * Age + 40.f * Age * Age) * K;
			const float Twinkle = .45f + .55f * FMath::Pow(FMath::Abs(FMath::Sin(Age * 18.f + Ember.Phase)), 6.f);
			const float Fade = 1.f - Age / Ember.Lifetime;
			const float Half = .9f * K;
			const FLinearColor EmberColor = DevFxAlpha(DevFxHot[Ember.BurstIndex], Fade * Twinkle * ParentAlpha);
			Core.Quad(Point + FVector2D(-Half, -Half), Point + FVector2D(Half, -Half), Point + FVector2D(Half, Half), Point + FVector2D(-Half, Half),
				EmberColor, EmberColor, EmberColor, EmberColor);
			if (Twinkle > .9f) Glow.Light(Point, 5.f * K, DevFxAlpha(DevFxGlow[Ember.BurstIndex], .5f * Fade * ParentAlpha));
		}
		if (DevelopmentLightMaterial) Glow.Submit(Elements, LayerId + 1, DevelopmentLightMaterial);
		Core.Submit(Elements, LayerId + 2, DevelopmentLightMaterial);

		// 개발이 끝나는 순간 상단 바를 한 번 훑는 흰 광택
		const FVector2D StripSize = DevelopmentStripMax - DevelopmentStripMin;
		if (Time < .3f && StripSize.X > 10.f && StripSize.Y > 4.f)
		{
			const float P = DevFxOut2(Time / .3f);
			const float X = DevelopmentStripMin.X - 80.f * K + (StripSize.X + 160.f * K) * P;
			const float HalfBand = 70.f * K;
			const FLinearColor Clear = DevFxAlpha(FLinearColor::White, 0.f);
			const FLinearColor Bright = DevFxAlpha(FLinearColor::White, .4f * (1.f - P * .5f) * ParentAlpha);
			FDevFxBatch Sweep(Geometry, 2);
			const float Top = DevelopmentStripMin.Y, Bottom = DevelopmentStripMax.Y;
			Sweep.Quad(FVector2D(X - HalfBand, Top), FVector2D(X, Top), FVector2D(X, Bottom), FVector2D(X - HalfBand, Bottom), Clear, Bright, Bright, Clear);
			Sweep.Quad(FVector2D(X, Top), FVector2D(X + HalfBand, Top), FVector2D(X + HalfBand, Bottom), FVector2D(X, Bottom), Bright, Clear, Clear, Bright);
			const FGeometry StripGeometry = Geometry.MakeChild(StripSize, FSlateLayoutTransform(DevelopmentStripMin));
			Elements.PushClip(FSlateClippingZone(StripGeometry));
			Sweep.Submit(Elements, LayerId + 3, DevelopmentLightMaterial);
			Elements.PopClip();
		}
	}

	// 3) 기존 orb의 실제 위치 샘플만 그린다. 이 꼬리에는 별도의 점수/착지 이벤트가 없다.
	int32 TrailSegments = 0;
	for (int32 Index = 0; Index < FMath::Min(ActiveOrbs.Num(), 48); ++Index)
	{
		TrailSegments += FMath::Max(0, ActiveOrbs[Index].DevelopmentTrail.Num() - 1);
	}
	FDevFxBatch Trails(Geometry, TrailSegments);
	int32 TrailCount = 0;
	for (const FOrbAnimData& Orb : ActiveOrbs)
	{
		if (++TrailCount > 48) break;
		const FLinearColor Color = UGlobalUtilFunctions::GetStepColor(Orb.StepNumber);
		for (int32 Index = 1; Index < Orb.DevelopmentTrail.Num(); ++Index)
		{
			const float P = static_cast<float>(Index) / Orb.DevelopmentTrail.Num();
			const FVector2D A = Orb.DevelopmentTrail[Index - 1], B = Orb.DevelopmentTrail[Index];
			const FVector2D Normal = FVector2D(-(B - A).GetSafeNormal().Y, (B - A).GetSafeNormal().X);
			const FLinearColor CA = DevFxAlpha(Color, P * .18f * ParentAlpha), CB = DevFxAlpha(Color, P * .7f * ParentAlpha);
			Trails.Quad(A - Normal * P * 4.f, A + Normal * P * 4.f, B + Normal * P * 5.f, B - Normal * P * 5.f, CA, CA, CB, CB);
		}
	}
	Trails.Submit(Elements, LayerId + 4, nullptr);
	return LayerId + 5;
}

bool UScoreOrbContainerWidget::ValidateDevelopmentEffects(FString& OutError) const
{
	if (DevelopmentStreaks.Num() > DevFxMaxStreaks)
	{
		OutError = TEXT("effect capacity exceeded"); return false;
	}
	if (DevelopmentFireworkElapsed < 0.f && (!DevelopmentStreaks.IsEmpty() || !DevelopmentEmbers.IsEmpty()))
	{
		OutError = TEXT("inactive fireworks retain streaks"); return false;
	}
	for (const FDevelopmentStreak& Streak : DevelopmentStreaks)
	{
		if (!FMath::IsFinite(Streak.Angle) || Streak.Speed <= 0.f || Streak.Lifetime <= 0.f
			|| Streak.BurstIndex < 0 || Streak.BurstIndex >= DevFxBurstCount)
		{
			OutError = TEXT("invalid streak"); return false;
		}
	}
	OutError = FString::Printf(TEXT("streaks=%d embers=%d tabs=%d firework=%.2f dim=%.2f paused=%d"),
		DevelopmentStreaks.Num(), DevelopmentEmbers.Num(), ActiveGoalTabs.Num(), DevelopmentFireworkElapsed,
		GetDevelopmentDimAlpha(), bDevelopmentPaused);
	return true;
}

static FAutoConsoleCommand OfficeDevVFXValidate(TEXT("CGR.OfficeDevVFX.Validate"),
	TEXT("Validate live Office effect budgets and finite lifetime state."), FConsoleCommandDelegate::CreateLambda([]()
	{
		for (TObjectIterator<UScoreOrbContainerWidget> It; It; ++It)
		{
			if (It->IsTemplate() || !It->GetWorld() || !It->GetWorld()->IsGameWorld()) continue;
			FString Result;
			const bool bValid = It->ValidateDevelopmentEffects(Result);
			UE_LOG(LogTemp, Display, TEXT("[OfficeDevVFX] %s %s: %s"), bValid ? TEXT("PASS") : TEXT("FAIL"), *It->GetName(), *Result);
		}
	}));
