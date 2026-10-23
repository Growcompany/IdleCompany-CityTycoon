#include "Misc/AutomationTest.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RetainerBox.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Gacha/GachaRevealFXLayer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Utils/GachaSparkSim.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

struct FGachaRevealFXLayerTestAccessor
{
	static FGachaSparkSim& Sim(UGachaRevealFXLayer& L) { return L.Sim; }
	static const TArray<TObjectPtr<UImage>>& Main(UGachaRevealFXLayer& L) { return L.MainImages; }
	static const TArray<TObjectPtr<UImage>>& Core(UGachaRevealFXLayer& L) { return L.CoreImages; }
	static const TArray<TObjectPtr<UImage>>& SlitImages(UGachaRevealFXLayer& L) { return L.SlitImages; }
	static const TArray<TObjectPtr<UImage>>& Screen(UGachaRevealFXLayer& L) { return L.ScreenImages; }
	static URetainerBox* Retainer(UGachaRevealFXLayer& L) { return L.FXRetainer; }
	static UCanvasPanel* Accum(UGachaRevealFXLayer& L) { return L.FXAccum; }
	static URetainerBox* CoverageRetainer(UGachaRevealFXLayer& L) { return L.CoverageRetainer; }
	static UCanvasPanel* CoverageAccum(UGachaRevealFXLayer& L) { return L.CoverageAccum; }
	static UTextureRenderTarget2D* CoverageTarget(UGachaRevealFXLayer& L) { return L.CoverageTarget; }
	static const TArray<TObjectPtr<UImage>>& CoverageMain(UGachaRevealFXLayer& L) { return L.CoverageMainImages; }
	static const TArray<TObjectPtr<UImage>>& CoverageCore(UGachaRevealFXLayer& L) { return L.CoverageCoreImages; }
	static const TArray<TObjectPtr<UImage>>& CoverageSlit(UGachaRevealFXLayer& L) { return L.CoverageSlitImages; }
	static const TArray<TObjectPtr<UTexture2D>>& Textures(UGachaRevealFXLayer& L) { return L.Textures; }
	static UMaterialInterface* AddMaterial(UGachaRevealFXLayer& L) { return L.AddMaterial; }
	static UMaterialInterface* OverMaterial(UGachaRevealFXLayer& L) { return L.OverMaterial; }
	static UMaterialInterface* ScreenMulMaterial(UGachaRevealFXLayer& L) { return L.ScreenMulMaterial; }
	static UMaterialInterface* RingMaterial(UGachaRevealFXLayer& L) { return L.RingMaterial; }
	static UMaterialInterface* CoverageMaterial(UGachaRevealFXLayer& L) { return L.CoverageMaterial; }
	static UMaterialInterface* CoverageRingMaterial(UGachaRevealFXLayer& L) { return L.CoverageRingMaterial; }
	static UMaterialInterface* ColorCompositeMaterial(UGachaRevealFXLayer& L) { return L.ColorCompositeMaterial; }
	static UMaterialInterface* CoverageCompositeMaterial(UGachaRevealFXLayer& L) { return L.CoverageCompositeMaterial; }
	static void ReconfigureComposite(UGachaRevealFXLayer& L, UMaterialInterface* Color, UMaterialInterface* Carrier)
	{
		L.ColorCompositeMaterial = Color;
		L.CoverageCompositeMaterial = Carrier;
		L.ConfigureCompositeMaterials();
	}
	static const TArray<TObjectPtr<UMaterialInstanceDynamic>>& ScreenMulMIDs(UGachaRevealFXLayer& L) { return L.ScreenMulMIDs; }
};

// 익명 namespace 금지 ― 유니티 TU 병합 시 같은 이름이 겹친다
namespace CGRGachaFXTests
{
	using FAccess = FGachaRevealFXLayerTestAccessor;

	// UWidget::IsVisible() 는 TakeWidget 전엔 늘 false ― 위젯 코드가 저장한 값을 Slate 와 같은 기준으로 본다
	static bool Shown(const UWidget* Widget)
	{
		if (!Widget)
		{
			return false;
		}
		const ESlateVisibility Visibility = Widget->GetVisibility();
		return Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden;
	}

	static int32 CountShown(const TArray<TObjectPtr<UImage>>& Images)
	{
		int32 Count = 0;
		for (const UImage* Image : Images)
		{
			Count += Shown(Image) ? 1 : 0;
		}
		return Count;
	}

	// 보이는 이미지가 전부 머티리얼 브러시인지(빈 브러시 = 흰 판)
	static bool AllShownHaveMaterial(const TArray<TObjectPtr<UImage>>& Images)
	{
		for (const UImage* Image : Images)
		{
			if (Shown(Image) && !Cast<UMaterialInterface>(Image->GetBrush().GetResourceObject()))
			{
				return false;
			}
		}
		return true;
	}

	static UMaterialInterface* ParentOf(const UImage* Image)
	{
		const UMaterialInstanceDynamic* MID = Image ? Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject()) : nullptr;
		return MID ? MID->Parent.Get() : nullptr;
	}

	static int32 ZOrderOf(const UWidget* Image)
	{
		const UCanvasPanelSlot* CanvasSlot = Image ? Cast<UCanvasPanelSlot>(Image->Slot) : nullptr;
		return CanvasSlot ? CanvasSlot->GetZOrder() : INDEX_NONE;
	}

	static FGachaFXParticle MakeSpark(const FVector2D& Pos, const FVector2D& Vel, float Life, float Gravity)
	{
		FGachaFXParticle P;
		P.Kind = EGachaFXParticleKind::Spark;
		P.Pos = Pos;
		P.Vel = Vel;
		P.Life = Life;
		P.Gravity = Gravity;
		P.Width = 5.f;
		P.TailSec = .035f;
		return P;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaFXSparkSimTest,
	"CGR.Gacha.FX.SparkSim",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaFXSparkSimTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaFXTests;

	// 시안 frame(): vx *= e^(−3.2dt), vy = vy·e^(−3.2dt) + g·dt, p += v·dt
	FGachaSparkSim Sim(7);
	const int32 Index = Sim.Emit(MakeSpark(FVector2D::ZeroVector, FVector2D(1000.0, 0.0), .2f, 1400.f));
	Sim.Step(.1f, 1.f);
	const FGachaFXParticle& P = Sim.Particles[Index];
	TestTrue(TEXT("dt 0.1 뒤 살아 있음"), P.IsActive());
	TestEqual(TEXT("x = 1000·e^(−0.32)·0.1 = 72.6"), static_cast<float>(P.Pos.X), 72.61f, .5f);
	TestEqual(TEXT("y = (0·e^(−0.32) + 1400·0.1)·0.1 = 14"), static_cast<float>(P.Pos.Y), 14.f, .5f);
	TestEqual(TEXT("t = 1 − life/max = 0.5"), P.Progress(), .5f, 1e-4f);
	Sim.Step(.1f, 1.f);
	TestFalse(TEXT("수명 0.2 뒤 비활성"), Sim.Particles[Index].IsActive());
	TestEqual(TEXT("활성 0"), Sim.NumActive(), 0);

	// 수명만 속도 배수로 준다(시안 life −= dt·speed, 움직임은 dt)
	FGachaSparkSim Fast(7);
	const int32 FastIndex = Fast.Emit(MakeSpark(FVector2D::ZeroVector, FVector2D(1000.0, 0.0), .2f, 0.f));
	Fast.Step(.05f, 2.f);
	TestEqual(TEXT("속도 2: 수명 0.2 − 0.05·2 = 0.1"), Fast.Particles[FastIndex].Life, .1f, 1e-4f);
	TestEqual(TEXT("속도 2: 이동은 dt 그대로 1000·e^(−0.16)·0.05"), static_cast<float>(Fast.Particles[FastIndex].Pos.X),
		1000.f * FMath::Exp(-.16f) * .05f, .01f);

	// 갱신식 = 굽기 fx.json ring_ref_lid(60→900, w14, t .3 → 반경 611.88·굵기 11.48·알파 .5857)
	TestEqual(TEXT("고리 반경 lerp(r0,r1,1−(1−t)^3)"), GachaFXMath::RingRadius(60.f, 900.f, .3f), 611.88f, .01f);
	TestEqual(TEXT("고리 굵기 w·(1−.6t)"), GachaFXMath::RingWidth(14.f, .3f), 11.48f, .001f);
	TestEqual(TEXT("고리 알파 (1−t)^1.5"), GachaFXMath::RingAlpha(.3f), .5857f, .0005f);
	TestEqual(TEXT("불꽃 알파 (1−t)^1.2"), GachaFXMath::SparkAlpha(.5f), .43528f, .0005f);
	TestEqual(TEXT("불꽃 굵기 w·(1−.7t)"), GachaFXMath::SparkLineWidth(10.f, .5f), 6.5f, 1e-4f);
	TestEqual(TEXT("빛 알파 peak·(1−t)^1.6"), GachaFXMath::GlowAlpha(.9f, 0.f), .9f, 1e-5f);
	TestEqual(TEXT("ease-out(.5) = cubic-bezier(0,0,.58,1)"), GachaFXMath::EaseOut(.5f), .68464f, .001f);
	TestEqual(TEXT("막 세기 0.1/1초(22% 정점)"), GachaFXMath::PeakEnvelope(.1f, 1.f, .22f), .72987f, .002f);
	TestEqual(TEXT("막 끝 = 0"), GachaFXMath::PeakEnvelope(1.f, 1.f, .22f), 0.f, 1e-6f);
	TestEqual(TEXT("기둥 scaleY 시작 .2"), GachaFXMath::BeamScaleY(0.f, .9f), .2f, 1e-4f);
	TestEqual(TEXT("시안 Math.round(2.5) = 3"), GachaFXMath::JsRound(2.5f), 3);
	TestEqual(TEXT("sRGB 공간 절반 = linear .2140"), GachaFXMath::ScaleInSRGB(FLinearColor::White, .5f).R, .21404f, .0005f);
	TestTrue(TEXT("누적용 정점색 = 색 그대로 + A 세기"), GachaFXMath::OverTint(FLinearColor(.2f, .4f, .6f), .3f).Equals(FLinearColor(.2f, .4f, .6f, .3f)));

	// 빛 기둥(시안 cutIn #pillar): 켜기 T(380) ease-out, T(400) 뒤 1→.65→1 1.8초(linear), 끄기 T(200) 1→0
	TestEqual(TEXT("기둥 켜기 시작 불투명도 0"), GachaFXMath::PillarOnOpacity(0.f, .38f, .4f), 0.f, 1e-5f);
	TestEqual(TEXT("기둥 켜기 반 = ease-out(.5)"), GachaFXMath::PillarOnOpacity(.19f, .38f, .4f), .68464f, .001f);
	TestEqual(TEXT("기둥 scaleY 시작 .3"), GachaFXMath::PillarScaleY(0.f, .38f), .3f, 1e-5f);
	TestEqual(TEXT("기둥 scaleY 반 = .3 + .7·ease-out(.5)"), GachaFXMath::PillarScaleY(.19f, .38f), .3f + .7f * .68464f, .001f);
	TestEqual(TEXT("기둥 숨쉬기 시작 = 1"), GachaFXMath::PillarOnOpacity(.4f, .38f, .4f), 1.f, 1e-4f);
	TestEqual(TEXT("기둥 숨쉬기 반 주기 = .65"), GachaFXMath::PillarOnOpacity(.4f + .9f, .38f, .4f), .65f, 1e-3f);
	TestEqual(TEXT("기둥 숨쉬기 1/4 = .825"), GachaFXMath::PillarOnOpacity(.4f + .45f, .38f, .4f), .825f, 1e-3f);
	TestEqual(TEXT("기둥 숨쉬기 둘째 주기 반 = .65"), GachaFXMath::PillarOnOpacity(.4f + 1.8f + .9f, .38f, .4f), .65f, 1e-3f);
	TestEqual(TEXT("기둥 끄기 반 = 1 − ease-out(.5)"), GachaFXMath::PillarOffOpacity(.1f, .2f), 1.f - .68464f, .001f);
	TestEqual(TEXT("기둥 즉시(0초) = 바로 다 켬"), GachaFXMath::PillarOnOpacity(0.f, 0.f, 0.f), 1.f, 1e-5f);

	// 바깥 획: 머리 (0,0) → 꼬리 (−144,0), 굵기 26.4 = 구운 그림 그대로(배율 1, 176×31, 가운데 = 선 가운데)
	const GachaFXMath::FStrokeLayout Outer = GachaFXMath::LayoutStroke(GachaFXMath::SparkOuter, FVector2D::ZeroVector, FVector2D(-144.0, 0.0), 26.4f);
	TestEqual(TEXT("바깥 획 배율 1"), Outer.Scale, 1.f, 1e-4f);
	TestTrue(TEXT("바깥 획 크기 176×31"), Outer.LocalSize.Equals(FVector2D(176.0, 31.0), .01));
	TestTrue(TEXT("바깥 획 가운데 (−72, 0)"), Outer.Center.Equals(FVector2D(-72.0, 0.0), .01));
	TestEqual(TEXT("바깥 획 각도 180"), FMath::Abs(Outer.AngleDeg), 180.f, .01f);
	// 고정 끝 조각보다 짧은 틈 빛 바깥 획은 고정 폭 + 가로 축소
	const GachaFXMath::FStrokeLayout Short = GachaFXMath::LayoutStroke(GachaFXMath::SlitOuter100, FVector2D::ZeroVector, FVector2D(40.0, 0.0), 16.f);
	TestTrue(TEXT("짧은 틈 빛 = 고정 폭 242"), Short.LocalSize.Equals(FVector2D(242.0, 160.0), .01));
	TestEqual(TEXT("짧은 틈 빛 가로 축소 200/242"), Short.ScaleX, 200.f / 242.f, 1e-4f);

	// 흔들림: max(지금, px×배수), e^(−9dt), 0.3 이하가 되면 0 으로 되돌리는 프레임 한 번
	FGachaShake Shake;
	FRandomStream Rng(3);
	Shake.Kick(14.f, 1.f);
	Shake.Kick(6.f, 1.f);
	bool bActive = false;
	const FVector2D Offset = Shake.Step(.05f, Rng, bActive);
	TestTrue(TEXT("흔들림 진행 중"), bActive);
	TestEqual(TEXT("세기 14·e^(−.45)"), Shake.Amp, 8.9268f, .001f);
	TestTrue(TEXT("이동 ≤ 세기"), FMath::Abs(Offset.X) <= Shake.Amp + 1e-3 && FMath::Abs(Offset.Y) <= Shake.Amp + 1e-3);
	int32 Frames = 0;
	while (Shake.Amp > 0.f && Frames < 200)
	{
		Shake.Step(.05f, Rng, bActive);
		++Frames;
	}
	TestEqual(TEXT("흔들림 끝 = 0"), Shake.Amp, 0.f);
	Shake.Step(.05f, Rng, bActive);
	TestFalse(TEXT("끝난 뒤엔 대상에 손대지 않음"), bActive);
	FGachaShake Still;
	Still.Kick(30.f, 0.f);
	Still.Step(.05f, Rng, bActive);
	TestFalse(TEXT("배수 0(연출 감소) = 안 흔듦"), bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaFXPoolTest,
	"CGR.Gacha.FX.Pool",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaFXPoolTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaFXTests;

	FGachaSparkSim Sim(11);
	FGachaSparkParams Params;
	Params.Count = 200;
	TestEqual(TEXT("200개 요청"), Sim.Sparks(FVector2D(500.0, 500.0), Params, 1.f), 200);
	TestEqual(TEXT("활성 = 풀 160"), Sim.NumActive(), FGachaSparkSim::Capacity);
	TestTrue(TEXT("활성 ≤ 160"), Sim.NumActive() <= 160);

	// 넘치면 가장 먼저 만든 입자를 덮는다
	FGachaSparkSim Fill(11);
	for (int32 I = 0; I < FGachaSparkSim::Capacity; ++I)
	{
		Fill.Emit(MakeSpark(FVector2D(I, 0.0), FVector2D::ZeroVector, 1.f, 0.f));
	}
	const int32 Reused = Fill.Ring(FVector2D::ZeroVector, 10.f, 20.f, .5f, FLinearColor::White, 4.f);
	TestEqual(TEXT("161번째 = 첫 입자 칸"), Reused, 0);
	TestTrue(TEXT("그 칸은 고리"), Fill.Particles[0].Kind == EGachaFXParticleKind::Ring);
	TestEqual(TEXT("다음 = 둘째 입자 칸"), Fill.Glow(FVector2D::ZeroVector, 50.f, .5f, FLinearColor::White, .9f), 1);
	Fill.Step(.6f, 1.f);
	const int32 Free = Fill.Allocate();
	TestTrue(TEXT("죽은 칸을 먼저 쓴다"), Free == 0 || Free == 1);

	// 불꽃 양 배수(시안 Math.round(n × sparkMul))
	FGachaSparkSim Half(5);
	Params.Count = 16;
	TestEqual(TEXT("16 × .5 = 8"), Half.Sparks(FVector2D::ZeroVector, Params, .5f), 8);
	TestEqual(TEXT("16 × 0 = 0"), Half.Sparks(FVector2D::ZeroVector, Params, 0.f), 0);

	// 효과층 제어기(캔버스 없이도 계산은 돈다)
	TStrongObjectPtr<UGachaRevealFXLayer> Layer(NewObject<UGachaRevealFXLayer>(GetTransientPackage()));
	Params.Count = 200;
	Layer->Sparks(FVector2D::ZeroVector, Params);
	TestTrue(TEXT("효과층 활성 ≤ 160"), Layer->GetActiveParticleCount() <= FGachaSparkSim::Capacity);
	Layer->Tick(.05f);
	Layer->Clear();
	TestEqual(TEXT("Clear 뒤 0"), Layer->GetActiveParticleCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaFXLayerTest,
	"CGR.Gacha.FX.Layer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaFXLayerTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaFXTests;

	TStrongObjectPtr<UCanvasPanel> Canvas(NewObject<UCanvasPanel>(GetTransientPackage()));
	TStrongObjectPtr<UCanvasPanel> Under(NewObject<UCanvasPanel>(GetTransientPackage()));
	TStrongObjectPtr<UCanvasPanel> Spot(NewObject<UCanvasPanel>(GetTransientPackage()));
	TStrongObjectPtr<UImage> Target(NewObject<UImage>(GetTransientPackage()));
	TStrongObjectPtr<UGachaRevealFXLayer> Layer(NewObject<UGachaRevealFXLayer>(GetTransientPackage()));
	UGachaRevealFXLayer& L = *Layer;
	L.Init(Canvas.Get(), Under.Get(), Spot.Get());
	L.SetSeed(1);
	TestTrue(TEXT("준비됨"), L.IsReady());

	// 자산 = fx_textures(13장)·fx_materials(8개) 단계 ― 빠지면 Init 이 Warning 을 남기고 이 단언이 깨진다
	int32 Loaded = 0;
	for (const UTexture2D* Texture : FAccess::Textures(L))
	{
		Loaded += Texture ? 1 : 0;
	}
	TestEqual(TEXT("FX 텍스처 13장"), Loaded, 13);
	TestNotNull(TEXT("M_UI_GachaFXAdd"), FAccess::AddMaterial(L));
	TestNotNull(TEXT("M_UI_GachaFXOver"), FAccess::OverMaterial(L));
	TestNotNull(TEXT("M_UI_GachaFXScreenMul"), FAccess::ScreenMulMaterial(L));
	TestNotNull(TEXT("M_UI_GachaFXRing"), FAccess::RingMaterial(L));
	TestNotNull(TEXT("M_UI_GachaFXCoverage"), FAccess::CoverageMaterial(L));
	TestNotNull(TEXT("M_UI_GachaFXCoverageRing"), FAccess::CoverageRingMaterial(L));
	TestNotNull(TEXT("M_UI_GachaFXComposite"), FAccess::ColorCompositeMaterial(L));
	TestNotNull(TEXT("M_UI_GachaFXCoverageCarrier"), FAccess::CoverageCompositeMaterial(L));
	TestEqual(TEXT("효과 캔버스 = coverage·색 리테이너 + 막 2"), Canvas->GetChildrenCount(), 4);
	TestNotNull(TEXT("FX 누적 리테이너"), FAccess::Retainer(L));
	TestNotNull(TEXT("FX 누적 캔버스"), FAccess::Accum(L));
	TestNotNull(TEXT("Coverage 누적 리테이너"), FAccess::CoverageRetainer(L));
	TestNotNull(TEXT("Coverage 누적 캔버스"), FAccess::CoverageAccum(L));
	TestTrue(TEXT("coverage와 색을 각각 독립 RT에 누적"), FAccess::CoverageRetainer(L)->GetContent() == FAccess::CoverageAccum(L)
		&& FAccess::CoverageRetainer(L) != FAccess::Retainer(L));
	TestTrue(TEXT("리테이너가 누적 캔버스를 단일 자식으로 가짐"), FAccess::Retainer(L)->GetContent() == FAccess::Accum(L));
	TestEqual(TEXT("누적 캔버스 = 입자 160×2 + 틈 빛 4"), FAccess::Accum(L)->GetChildrenCount(), 160 * 2 + 4);
	TestEqual(TEXT("Coverage 캔버스도 동일한 이미지 풀"), FAccess::CoverageAccum(L)->GetChildrenCount(), 160 * 2 + 4);
	TestTrue(TEXT("누적 리테이너는 매 프레임 갱신"), FAccess::Retainer(L)->IsRetainRendering()
		&& FAccess::Retainer(L)->GetPhase() == 0 && FAccess::Retainer(L)->GetPhaseCount() == 1);
	TestTrue(TEXT("Coverage 리테이너도 매 프레임 갱신"), FAccess::CoverageRetainer(L)->IsRetainRendering()
		&& FAccess::CoverageRetainer(L)->GetPhase() == 0 && FAccess::CoverageRetainer(L)->GetPhaseCount() == 1);
	TestTrue(TEXT("Coverage가 색보다 먼저 paint"), ZOrderOf(FAccess::CoverageRetainer(L)) < ZOrderOf(FAccess::Retainer(L)));
	TestTrue(TEXT("리테이너 내부 RT 주입 파라미터"), FAccess::CoverageRetainer(L)->GetTextureParameter() == FName(TEXT("Texture"))
		&& FAccess::Retainer(L)->GetTextureParameter() == FName(TEXT("Tex")));
	TestTrue(TEXT("RT 연결 전 색 리테이너는 표시하지 않음"), FAccess::Retainer(L)->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("입자·틈 이미지는 누적 캔버스 소유"), FAccess::Main(L)[0]->GetParent() == FAccess::Accum(L)
		&& FAccess::Core(L)[0]->GetParent() == FAccess::Accum(L) && FAccess::SlitImages(L)[0]->GetParent() == FAccess::Accum(L));
	TestEqual(TEXT("상자 아래 캔버스 = 빛줄기 2장"), Under->GetChildrenCount(), 2);
	TestEqual(TEXT("단독 장면 캔버스 = 빛 기둥 2장"), Spot->GetChildrenCount(), 2);
	TestTrue(TEXT("흰 심이 바깥 획 위(내부 누적에서 뒤에 그려짐)"), ZOrderOf(FAccess::Core(L)[0]) > ZOrderOf(FAccess::Main(L)[0])
		&& ZOrderOf(FAccess::SlitImages(L)[1]) > ZOrderOf(FAccess::SlitImages(L)[0]) && ZOrderOf(FAccess::SlitImages(L)[3]) > ZOrderOf(FAccess::SlitImages(L)[2]));
	TestEqual(TEXT("처음엔 다 숨김"), CountShown(FAccess::Main(L)) + CountShown(FAccess::Core(L)) + CountShown(FAccess::Screen(L)), 0);

	FGachaSparkParams Params;
	Params.Count = 10;
	Params.Color = FLinearColor(1.f, .5f, .1f);
	L.Sparks(FVector2D(400.0, 300.0), Params);
	L.Ring(FVector2D(400.0, 300.0), 60.f, 900.f, .5f, FLinearColor::White, 14.f);
	L.Glow(FVector2D(400.0, 300.0), 500.f, .6f, FLinearColor::White, .8f);
	L.Tick(1.f / 60.f);
	TestEqual(TEXT("불꽃 10 = 바깥 10 + 심 10, 고리 1, 빛 1"), CountShown(FAccess::Main(L)) + CountShown(FAccess::Core(L)), 22);
	TestEqual(TEXT("Coverage도 같은 입자 수"), CountShown(FAccess::CoverageMain(L)) + CountShown(FAccess::CoverageCore(L)), 22);
	for (int32 Index = 0; Index < FAccess::Main(L).Num(); ++Index)
	{
		const UImage* ColorImage = FAccess::Main(L)[Index];
		const UImage* CoverageImage = FAccess::CoverageMain(L)[Index];
		if (!Shown(ColorImage))
		{
			continue;
		}
		const UCanvasPanelSlot* ColorSlot = Cast<UCanvasPanelSlot>(ColorImage->Slot);
		const UCanvasPanelSlot* CoverageSlot = Cast<UCanvasPanelSlot>(CoverageImage->Slot);
		const bool bMatchingPosition = ColorSlot && CoverageSlot
			&& ColorSlot->GetPosition().Equals(CoverageSlot->GetPosition())
			&& ColorSlot->GetSize().Equals(CoverageSlot->GetSize());
		TestTrue(TEXT("보이는 색 입자와 coverage의 위치·크기·세기 일치"), Shown(CoverageImage) && bMatchingPosition
			&& FMath::IsNearlyEqual(ColorImage->GetColorAndOpacity().A, CoverageImage->GetColorAndOpacity().A));
		const UMaterialInterface* Expected = FAccess::Sim(L).Particles[Index].Kind == EGachaFXParticleKind::Ring
			? FAccess::CoverageRingMaterial(L) : FAccess::CoverageMaterial(L);
		TestTrue(TEXT("coverage 입자는 별도 누적 재질"), ParentOf(CoverageImage) == Expected);
	}
	TestTrue(TEXT("보이는 입자 이미지는 전부 머티리얼 브러시(흰 판 없음)"),
		AllShownHaveMaterial(FAccess::Main(L)) && AllShownHaveMaterial(FAccess::Core(L)));
	TestTrue(TEXT("보이는 coverage 입자도 전부 머티리얼 브러시"),
		AllShownHaveMaterial(FAccess::CoverageMain(L)) && AllShownHaveMaterial(FAccess::CoverageCore(L)));
	// #fx 내부의 색·coverage 는 각각 더한 뒤 색 리테이너에서 한 번 화면에 겹친다.
	int32 OverSparks = 0, OverGlows = 0, Rings = 0;
	for (int32 Index = 0; Index < FAccess::Main(L).Num(); ++Index)
	{
		const UImage* Image = FAccess::Main(L)[Index];
		if (!Shown(Image))
		{
			continue;
		}
		const UMaterialInterface* Parent = ParentOf(Image);
		const bool bRing = FAccess::Sim(L).Particles[Index].Kind == EGachaFXParticleKind::Ring;
		const bool bGlow = FAccess::Sim(L).Particles[Index].Kind == EGachaFXParticleKind::Glow;
		Rings += bRing && Parent == FAccess::RingMaterial(L) ? 1 : 0;
		OverGlows += bGlow && Parent == FAccess::OverMaterial(L) ? 1 : 0;
		OverSparks += !bRing && !bGlow && Parent == FAccess::OverMaterial(L) ? 1 : 0;
		if (!bRing && !bGlow)
		{
			TestTrue(TEXT("바깥 획 정점색 A = 누적 세기"), Image->GetColorAndOpacity().A > 0.f && Image->GetColorAndOpacity().A <= 1.f);
		}
	}
	TestEqual(TEXT("바깥 획 10 = 누적용 머티리얼"), OverSparks, 10);
	TestEqual(TEXT("빛 1 = 누적용 머티리얼"), OverGlows, 1);
	TestEqual(TEXT("고리 1 = 고리 머티리얼"), Rings, 1);
	for (const UImage* Image : FAccess::Core(L))
	{
		if (Shown(Image))
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject());
			TestTrue(TEXT("심 = 누적용 머티리얼 + 흰 심 그림"), MID && MID->Parent == FAccess::OverMaterial(L)
				&& MID->K2_GetTextureParameterValue(TEXT("Tex")) == FAccess::Textures(L)[1]);
			TestTrue(TEXT("심 정점색 = 흰색 + 알파"), Image->GetColorAndOpacity().A > 0.f
				&& FMath::IsNearlyEqual(Image->GetColorAndOpacity().R, Image->GetColorAndOpacity().B));
			break;
		}
	}

	// 수명 0.42 이 다 지나면(속도 1) 불꽃·고리·빛 다 숨김
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		L.Tick(.05f);
	}
	TestEqual(TEXT("수명 뒤 입자 0"), L.GetActiveParticleCount(), 0);
	TestEqual(TEXT("수명 뒤 입자 이미지 0"), CountShown(FAccess::Main(L)) + CountShown(FAccess::Core(L)), 0);
	TestEqual(TEXT("수명 뒤 coverage 입자 이미지 0"), CountShown(FAccess::CoverageMain(L)) + CountShown(FAccess::CoverageCore(L)), 0);

	// 막·빛 기둥 = 곱하기 장 + 더하기 장
	L.Curtain(FLinearColor(1.f, .45f, .02f), .62f, .52f, EGachaCurtainShape::Lid);
	L.Beam(FVector2D(400.0, 900.0), 520.f, FLinearColor::White, 1.f, .9f);
	L.Tick(.1f);
	TestEqual(TEXT("막 2장 + 기둥 2장 보임"), CountShown(FAccess::Screen(L)), 4);
	TestTrue(TEXT("막·기둥 브러시 = 머티리얼"), AllShownHaveMaterial(FAccess::Screen(L)));
	UMaterialInstanceDynamic* CurtainMul = FAccess::ScreenMulMIDs(L).IsValidIndex(0) ? FAccess::ScreenMulMIDs(L)[0].Get() : nullptr;
	TestTrue(TEXT("막 곱하기 장 세기 = .62 × 시안 봉우리"), CurtainMul
		&& FMath::IsNearlyEqual(CurtainMul->K2_GetScalarParameterValue(TEXT("Opacity")), .62f * GachaFXMath::PeakEnvelope(.1f, .52f, .22f), .002f));
	TestTrue(TEXT("기둥 렌더 배율 ≤ 1"), FAccess::Screen(L)[3]->GetRenderTransform().Scale.Y <= 1.0);
	L.Tick(.5f);
	L.Tick(.5f);
	TestEqual(TEXT("막·기둥 끝나면 숨김"), CountShown(FAccess::Screen(L)), 0);

	// 단독 장면 빛 기둥 = 곱하기 장 + 더하기 장(단독 장면 캔버스), 아래 가운데 축, 숨쉬기, 끄기
	L.PillarOn(FVector2D(640.0, 1440.0), FLinearColor(1.f, .7f, .1f), .38f, .4f);
	L.Tick(.19f);
	TestTrue(TEXT("기둥 2장 보임(단독 장면 캔버스)"), Shown(FAccess::Screen(L)[4]) && Shown(FAccess::Screen(L)[5]) && CountShown(FAccess::Screen(L)) == 2);
	TestTrue(TEXT("기둥 브러시 = 곱하기 MID + 더하기 MID"), ParentOf(FAccess::Screen(L)[4]) == FAccess::ScreenMulMaterial(L)
		&& ParentOf(FAccess::Screen(L)[5]) == FAccess::AddMaterial(L));
	TestEqual(TEXT("기둥 scaleY = .3 + .7·ease-out(.5)"), static_cast<float>(FAccess::Screen(L)[4]->GetRenderTransform().Scale.Y), .3f + .7f * .68464f, .002f);
	TestTrue(TEXT("기둥 축 = 아래 가운데"), FAccess::Screen(L)[5]->GetRenderTransformPivot().Equals(FVector2D(.5, 1.0)));
	UMaterialInstanceDynamic* PillarMul = FAccess::ScreenMulMIDs(L).IsValidIndex(2) ? FAccess::ScreenMulMIDs(L)[2].Get() : nullptr;
	TestTrue(TEXT("기둥 곱하기 세기 = ease-out(.5)"), PillarMul
		&& FMath::IsNearlyEqual(PillarMul->K2_GetScalarParameterValue(TEXT("Opacity")), .68464f, .002f)
		&& PillarMul->K2_GetTextureParameterValue(TEXT("Tex")) == FAccess::Textures(L)[12]);
	L.Tick(.21f + .9f);
	TestTrue(TEXT("기둥 숨쉬기 반 주기 = .65"), PillarMul && FMath::IsNearlyEqual(PillarMul->K2_GetScalarParameterValue(TEXT("Opacity")), .65f, .01f));
	L.PillarOff(.2f);
	L.Tick(.1f);
	TestTrue(TEXT("기둥 끄기 = 1 에서 내려옴(1 − ease-out(.5))"), PillarMul
		&& FMath::IsNearlyEqual(PillarMul->K2_GetScalarParameterValue(TEXT("Opacity")), 1.f - .68464f, .01f));
	L.Tick(.15f);
	TestEqual(TEXT("기둥 끄기 끝 → 숨김"), CountShown(FAccess::Screen(L)), 0);

	// 틈 빛: 세기 1·진행 .5 → 바깥·심·새는 빛 + 칼끝
	L.SetSlit(true, FVector2D(100.0, 600.0), FVector2D(620.0, 640.0), .5f, FLinearColor(.4f, .6f, 1.f), 1.f, true);
	L.Tick(1.f / 60.f);
	TestEqual(TEXT("틈 빛 4장"), CountShown(FAccess::SlitImages(L)), 4);
	TestEqual(TEXT("Coverage 틈 빛 4장"), CountShown(FAccess::CoverageSlit(L)), 4);
	TestTrue(TEXT("틈 빛 4장 모두 내부 더하기 누적"),
		ParentOf(FAccess::SlitImages(L)[0]) == FAccess::OverMaterial(L) && ParentOf(FAccess::SlitImages(L)[2]) == FAccess::OverMaterial(L)
		&& ParentOf(FAccess::SlitImages(L)[1]) == FAccess::OverMaterial(L) && ParentOf(FAccess::SlitImages(L)[3]) == FAccess::OverMaterial(L));
	TestTrue(TEXT("새는 빛 = 기울임(shear Y)"), !FMath::IsNearlyZero(FAccess::SlitImages(L)[2]->GetRenderTransform().Shear.Y));
	L.SetSlit(true, FVector2D(100.0, 600.0), FVector2D(620.0, 640.0), .5f, FLinearColor(.4f, .6f, 1.f), .55f, false);
	L.Tick(1.f / 60.f);
	TestEqual(TEXT("세기 .55·칼끝 끔 → 바깥·심만(새는 빛은 세기 .6 초과)"), CountShown(FAccess::SlitImages(L)), 2);
	L.SetSlit(false, FVector2D::ZeroVector, FVector2D::ZeroVector, 0.f, FLinearColor::White, 1.f, false);
	L.Tick(1.f / 60.f);
	TestEqual(TEXT("틈 빛 끔"), CountShown(FAccess::SlitImages(L)), 0);
	TestEqual(TEXT("Coverage 틈 빛 끔"), CountShown(FAccess::CoverageSlit(L)), 0);

	// 흔들림 = 대상 렌더 이동, 끝나면 0, 배수 0 이면 안 흔듦
	L.SetShakeTarget(Target.Get());
	L.Shake(10.f);
	L.Tick(.016f);
	const FVector2D Moved = Target->GetRenderTransform().Translation;
	TestTrue(TEXT("흔들림 이동 ≤ 10"), Moved.GetAbsMax() <= 10.0 + 1e-3);
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		L.Tick(.05f);
	}
	TestTrue(TEXT("흔들림 끝 → 이동 0"), Target->GetRenderTransform().Translation.IsNearlyZero());
	L.SetIntensity(1.f, 0.f, 1.f);
	L.Shake(30.f);
	L.Tick(.016f);
	TestTrue(TEXT("흔들림 배수 0 → 이동 0"), Target->GetRenderTransform().Translation.IsNearlyZero());

	L.SetIntensity(1.f, 1.f, 1.f);
	L.Sparks(FVector2D(400.0, 300.0), Params);
	L.Tick(1.f / 60.f);
	L.Clear();
	TestEqual(TEXT("Clear → 다 숨김"), CountShown(FAccess::Main(L)) + CountShown(FAccess::Core(L)) + CountShown(FAccess::Screen(L))
		+ CountShown(FAccess::SlitImages(L)), 0);
	UMaterialInterface* SavedColorComposite = FAccess::ColorCompositeMaterial(L);
	UMaterialInterface* SavedCoverageComposite = FAccess::CoverageCompositeMaterial(L);
	AddExpectedError(TEXT("합성 재질 누락"), EAutomationExpectedErrorFlags::Contains, 2);
	FAccess::ReconfigureComposite(L, nullptr, SavedCoverageComposite);
	TestTrue(TEXT("색 합성 재질 누락 시 raw coverage·color RT 모두 숨김"),
		FAccess::CoverageRetainer(L)->GetVisibility() == ESlateVisibility::Collapsed
		&& FAccess::Retainer(L)->GetVisibility() == ESlateVisibility::Collapsed
		&& FAccess::CoverageTarget(L) == nullptr);
	FAccess::ReconfigureComposite(L, SavedColorComposite, nullptr);
	TestTrue(TEXT("coverage carrier 재질 누락 시 raw coverage·color RT 모두 숨김"),
		FAccess::CoverageRetainer(L)->GetVisibility() == ESlateVisibility::Collapsed
		&& FAccess::Retainer(L)->GetVisibility() == ESlateVisibility::Collapsed
		&& FAccess::CoverageTarget(L) == nullptr);
	FAccess::ReconfigureComposite(L, SavedColorComposite, SavedCoverageComposite);
	TestTrue(TEXT("합성 재질 복구 시 coverage만 예열하고 색은 RT 연결까지 숨김"),
		FAccess::CoverageRetainer(L)->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& FAccess::Retainer(L)->GetVisibility() == ESlateVisibility::Collapsed);
	L.Release();
	TestFalse(TEXT("Release → 준비 해제"), L.IsReady());
	TestEqual(TEXT("Release → 캔버스 비움"), Canvas->GetChildrenCount() + Under->GetChildrenCount() + Spot->GetChildrenCount(), 0);
	TestNull(TEXT("Release → 누적 리테이너 참조 해제"), FAccess::Retainer(L));
	TestNull(TEXT("Release → 누적 캔버스 참조 해제"), FAccess::Accum(L));
	TestNull(TEXT("Release → coverage 리테이너 참조 해제"), FAccess::CoverageRetainer(L));
	TestNull(TEXT("Release → coverage 캔버스 참조 해제"), FAccess::CoverageAccum(L));
	TestNull(TEXT("Release → coverage RT 참조 해제"), FAccess::CoverageTarget(L));
	return true;
}

#if WITH_EDITOR

// ⚠ 실제 RHI 가 있는 에디터에서만 판정 가능(-nullrhi = 전부 0) ― NonNullRHI 로 -NullRHI 실행에서 빠진다.
// 정해진 장면 12개를 그려 Saved/GachaBoxReveal/fx_compare/game_<id>.png 로 남긴다 → capture_compare.py fx compare 가 굽기 그림·시안 식과 대조.
// 밝은 바탕(흰 카드 #F5F7FA) 장면이 겹치기·더하기를 가른다(검정 바탕에서는 둘이 같다)
namespace CGRGachaFXCapture
{
	static FString Directory()
	{
		return FPaths::ProjectSavedDir() / TEXT("GachaBoxReveal/fx_compare");
	}

	struct FCase
	{
		const TCHAR* Id;
		FIntPoint Size;
		FLinearColor Background;
	};

	static FLinearColor Hex(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	static void EmitSpark(UGachaRevealFXLayer& L, const FVector2D& Pos, const FVector2D& Vel, float Width, const FLinearColor& Color)
	{
		FGachaFXParticle P;
		P.Kind = EGachaFXParticleKind::Spark;
		P.Pos = Pos;
		P.Vel = Vel;
		P.Life = 1.f;
		P.Width = Width;
		P.TailSec = .035f;
		P.Color = Color;
		FGachaRevealFXLayerTestAccessor::Sim(L).Emit(P);
	}

	static void Setup(UGachaRevealFXLayer& L, const FString& Id)
	{
		L.Clear();
		L.SetIntensity(1.f, 1.f, 1.f);
		if (Id == TEXT("ring_lid"))
		{
			// 굽기 ring_ref_lid 와 같은 장면: 60→900, w14, 수명 .5 의 t = .3
			L.Ring(FVector2D(625.5, 625.5), 60.f, 900.f, .5f, FLinearColor::White, 14.f);
			for (int32 Step = 0; Step < 3; ++Step)
			{
				L.Tick(.05f);
			}
		}
		else if (Id == TEXT("ring_card"))
		{
			// 같은 고리, 뒤집기 A 카드 색 #4AA2FF 를 흰 카드 위에
			L.Ring(FVector2D(625.5, 625.5), 60.f, 900.f, .5f, Hex(0x4A, 0xA2, 0xFF), 14.f);
			for (int32 Step = 0; Step < 3; ++Step)
			{
				L.Tick(.05f);
			}
		}
		else if (Id == TEXT("glow"))
		{
			L.Glow(FVector2D(256.0, 256.0), 256.f, .6f, FLinearColor::White, .9f);
			L.Tick(0.f);
		}
		else if (Id == TEXT("glow_gold_card"))
		{
			// 단독 장면 SS 빛(세기 .8, #FFB628)이 흰 카드 앞면 위 ― 시안 (253,195,82), 더하기면 흰색으로 날아간다
			L.Glow(FVector2D(256.0, 256.0), 256.f, .7f, Hex(0xFF, 0xB6, 0x28), .8f);
			L.Tick(0.f);
		}
		else if (Id == TEXT("glow_blue_card"))
		{
			// 뒤집기 A 카드 빛(세기 .6, #4AA2FF)
			L.Glow(FVector2D(256.0, 256.0), 256.f, .35f, Hex(0x4A, 0xA2, 0xFF), .6f);
			L.Tick(0.f);
		}
		else if (Id == TEXT("spark") || Id == TEXT("spark_card"))
		{
			// 굵기 12·꼬리 144 = 구운 그림 그대로(배율 1), 왼쪽으로 나는 불꽃이라 회전 0
			EmitSpark(L, FVector2D(400.0, 100.5), FVector2D(-144.0 / .035, 0.0), 12.f,
				Id == TEXT("spark") ? FLinearColor::White : Hex(0xFF, 0xB6, 0x28));
			L.Tick(0.f);
		}
		else if (Id == TEXT("spark_small"))
		{
			// 회전 −35°·배율 .125(굵기 1.5 = 그림 최대 굵기의 1/8, 밉) ― 기준점·3-slice 가 회전·축소에서 맞는지
			const double Ang = FMath::DegreesToRadians(-35.0);
			EmitSpark(L, FVector2D(330.0, 60.0), FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * 2000.0, 1.5f, FLinearColor::White);
			L.Tick(0.f);
		}
		else if (Id == TEXT("slit"))
		{
			L.SetSlit(true, FVector2D(80.0, 320.0), FVector2D(600.0, 320.0), 1.f, FLinearColor::White, 1.f, false);
			L.Tick(0.f);
		}
		else if (Id == TEXT("slit_short"))
		{
			// 칼끝 40px ― 고정 끝 조각보다 짧아 고정 폭 + 가로 축소(ScaleX 200/242)
			L.SetSlit(true, FVector2D(80.0, 320.0), FVector2D(600.0, 320.0), 40.f / 520.f, FLinearColor::White, 1.f, false);
			L.Tick(0.f);
		}
		else if (Id == TEXT("pillar"))
		{
			// 회색 바탕 위 #FFB628 빛 기둥, 켜기 T(380) 의 반(불투명도·scaleY = ease-out(.5)), 아래 가운데 축
			L.PillarOn(FVector2D(380.0, 1440.0), Hex(0xFF, 0xB6, 0x28), .38f, .4f);
			L.Tick(.19f);
		}
		else if (Id == TEXT("curtain"))
		{
			// screen 대조: 회색 바탕 위 #FFB628 막, 1초 중 0.1초(세기 .62 × 봉우리)
			L.Curtain(FLinearColor::FromSRGBColor(FColor(0xFF, 0xB6, 0x28)), .62f, 1.f, EGachaCurtainShape::Lid);
			L.Tick(.1f);
		}
	}

	static bool Draw(UGachaRevealFXLayer& Layer, const TSharedRef<SWidget>& CanvasSlate, const FCase& Case, FImage& OutImage)
	{
		TSharedRef<SWidget> Root = SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Case.Background)]
			+ SOverlay::Slot()[SNew(SBox).WidthOverride(Case.Size.X).HeightOverride(Case.Size.Y)[CanvasSlate]];
		FWidgetRenderer Renderer(true);
		// raw target(false) ― sRGB 타깃은 슬레이트 감마와 겹친다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Case.Size.X, Case.Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Root, 1.f, FVector2D(Case.Size.X, Case.Size.Y), 0.f);
			FlushRenderingCommands();
			if (Pass == 0)
			{
				Layer.Tick(0.f); // 첫 paint가 coverage MID.Texture를 채운 뒤 다음 paint의 합성 MID에 연결한다.
			}
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaFXCaptureTest,
	"CGR.Gacha.FX.Capture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FCGRGachaFXCaptureTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaFXCapture;
	using FAccess = FGachaRevealFXLayerTestAccessor;

	const FLinearColor Gray = FLinearColor::FromSRGBColor(FColor(128, 128, 128));
	// 카드 앞면(시안 :169 #F5F7FA)
	const FLinearColor Card = FLinearColor::FromSRGBColor(FColor(0xF5, 0xF7, 0xFA));
	const FCase Cases[] = {
		{ TEXT("ring_lid"), FIntPoint(1251, 1251), FLinearColor::Black },
		{ TEXT("ring_card"), FIntPoint(1251, 1251), Card },
		{ TEXT("glow"), FIntPoint(512, 512), FLinearColor::Black },
		{ TEXT("glow_gold_card"), FIntPoint(512, 512), Card },
		{ TEXT("glow_blue_card"), FIntPoint(512, 512), Card },
		{ TEXT("spark"), FIntPoint(600, 200), FLinearColor::Black },
		{ TEXT("spark_card"), FIntPoint(600, 200), Card },
		{ TEXT("spark_small"), FIntPoint(400, 200), FLinearColor::Black },
		{ TEXT("slit"), FIntPoint(680, 400), FLinearColor::Black },
		{ TEXT("slit_short"), FIntPoint(680, 400), FLinearColor::Black },
		{ TEXT("curtain"), FIntPoint(1000, 1440), Gray },
		{ TEXT("pillar"), FIntPoint(760, 1440), Gray },
	};
	IFileManager::Get().MakeDirectory(*Directory(), true);
	for (const FCase& Case : Cases)
	{
		TStrongObjectPtr<UCanvasPanel> Canvas(NewObject<UCanvasPanel>(GetTransientPackage()));
		TStrongObjectPtr<UGachaRevealFXLayer> Layer(NewObject<UGachaRevealFXLayer>(GetTransientPackage()));
		Layer->Init(Canvas.Get(), nullptr, Canvas.Get());
		if (!TestTrue(*FString::Printf(TEXT("%s: 효과층 준비"), Case.Id), Layer->IsReady()))
		{
			return false;
		}
		const TSharedRef<SWidget> CanvasSlate = Canvas->TakeWidget();
		// 처음 쓰는 UI 재질·막 들인 텍스처는 첫 그림에서 빠진다 ― 한 번 그린 뒤 컴파일을 끝내고 같은 장면을 다시 세워 촬영
		Setup(*Layer, Case.Id);
		FImage Discarded;
		Draw(*Layer, CanvasSlate, Case, Discarded);
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		FlushRenderingCommands();
		Setup(*Layer, Case.Id);
		FImage Image;
		if (!TestTrue(*FString::Printf(TEXT("%s: coverage RT 연결"), Case.Id), FAccess::CoverageTarget(*Layer) != nullptr))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s: 연결 후 색 리테이너 표시"), Case.Id),
			FAccess::Retainer(*Layer)->GetVisibility() == ESlateVisibility::HitTestInvisible);
		if (!TestTrue(*FString::Printf(TEXT("%s: 그리기"), Case.Id), Draw(*Layer, CanvasSlate, Case, Image)))
		{
			continue;
		}
		const FColor Back = Case.Background.ToFColorSRGB();
		int32 Painted = 0;
		for (const FColor& Pixel : Image.AsBGRA8())
		{
			Painted += FMath::Abs(Pixel.R - Back.R) + FMath::Abs(Pixel.G - Back.G) + FMath::Abs(Pixel.B - Back.B) > 12 ? 1 : 0;
		}
		// 축소 불꽃은 몇십 px 뿐이다
		TestTrue(*FString::Printf(TEXT("%s: 그려짐(%d px)"), Case.Id, Painted), Painted > (FCString::Strcmp(Case.Id, TEXT("spark_small")) == 0 ? 20 : 200));
		TArray64<uint8> Bytes;
		const FString Path = Directory() / FString::Printf(TEXT("game_%s.png"), Case.Id);
		TestTrue(*FString::Printf(TEXT("%s: 저장"), *Path), FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path));
		Layer->Release();
	}
	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
