#include "UI/Element/Gacha/GachaRevealFXLayer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RetainerBox.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GachaRevealFXAssets
{
	enum ETexture : int32
	{
		TexSparkOuter,
		TexSparkCore,
		TexGlow,
		TexBlade,
		TexSlitCore,
		TexSlitOuter055,
		TexSlitOuter085,
		TexSlitOuter100,
		TexSlitLeak,
		TexBeam,
		TexCurtainLid,
		TexCurtainSpot,
		TexPillar,
		TextureCount,
	};

	// import_assets.py fx_texture_jobs 와 같은 이름(apply_pending check 가 대조)
	static const TCHAR* const TextureNames[TextureCount] = {
		TEXT("T_GachaFX_SparkOuter"), TEXT("T_GachaFX_SparkCore"), TEXT("T_GachaFX_Glow"), TEXT("T_GachaFX_Blade"),
		TEXT("T_GachaFX_SlitCore"), TEXT("T_GachaFX_SlitOuter_055"), TEXT("T_GachaFX_SlitOuter_085"), TEXT("T_GachaFX_SlitOuter_100"),
		TEXT("T_GachaFX_SlitLeak"), TEXT("T_GachaFX_Beam"), TEXT("T_GachaFX_CurtainLid"), TEXT("T_GachaFX_CurtainSpot"),
		TEXT("T_GachaFX_Pillar"),
	};

	static const TCHAR* const TextureDir = TEXT("/Game/CompanyGrowth/UI/Textures/GachaBox/FX");
	static const TCHAR* const MaterialDir = TEXT("/Game/CompanyGrowth/UI/Materials/Gacha");
	static const TCHAR* const AddMaterialName = TEXT("M_UI_GachaFXAdd");
	static const TCHAR* const OverMaterialName = TEXT("M_UI_GachaFXOver");
	static const TCHAR* const ScreenMulMaterialName = TEXT("M_UI_GachaFXScreenMul");
	static const TCHAR* const RingMaterialName = TEXT("M_UI_GachaFXRing");
	static const TCHAR* const CoverageMaterialName = TEXT("M_UI_GachaFXCoverage");
	static const TCHAR* const CoverageRingMaterialName = TEXT("M_UI_GachaFXCoverageRing");
	static const TCHAR* const ColorCompositeMaterialName = TEXT("M_UI_GachaFXComposite");
	static const TCHAR* const CoverageCompositeMaterialName = TEXT("M_UI_GachaFXCoverageCarrier");

	static FSoftObjectPath AssetPath(const TCHAR* Dir, const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Dir, Name, Name));
	}

	// 시안 drawSlit 세기 .55·.85·1 에 맞춰 구운 바깥 획
	static int32 SlitOuterFor(float Intensity, float& OutBakedIntensity, GachaFXMath::FStrokeSprite& OutSprite)
	{
		if (Intensity <= .7f)
		{
			OutBakedIntensity = .55f;
			OutSprite = GachaFXMath::SlitOuter055;
			return TexSlitOuter055;
		}
		if (Intensity <= .925f)
		{
			OutBakedIntensity = .85f;
			OutSprite = GachaFXMath::SlitOuter085;
			return TexSlitOuter085;
		}
		OutBakedIntensity = 1.f;
		OutSprite = GachaFXMath::SlitOuter100;
		return TexSlitOuter100;
	}

	static FMargin StrokeMargin(const GachaFXMath::FStrokeSprite& Sprite)
	{
		return FMargin(Sprite.StretchL / Sprite.TexW, 0.f, Sprite.StretchR / Sprite.TexW, 0.f);
	}

	// 흰 심·칼끝은 누적 캔버스 안에서 바깥 획보다 뒤에 그린다.
	constexpr int32 ParticleZOrder = 0;
	constexpr int32 AddOnTopZOrder = 1;
	constexpr int32 CurtainZOrder = 10;
	constexpr int32 UnderZOrder = -10;
	constexpr float LidCurtainPeakAt = .22f;
	constexpr float SpotCurtainPeakAt = .2f;
	constexpr float BeamPeakAt = .25f;
	constexpr float BeamHeight = 1200.f;
	constexpr float CurtainW = 3120.f;
	constexpr float CurtainH = 1440.f;
	constexpr float PillarW = 760.f;
	constexpr float PillarH = 1440.f;
	constexpr int32 PillarZOrder = 0;
	constexpr float MaxFrameSec = .05f;
}

void UGachaRevealFXLayer::Init(UCanvasPanel* InFXCanvas, UCanvasPanel* InUnderCanvas, UCanvasPanel* InPillarCanvas)
{
	using namespace GachaRevealFXAssets;

	Release();
	if (!InFXCanvas)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealFX] Init: FXCanvas 없음 ― 효과를 그리지 않는다"));
		return;
	}
	FXCanvas = InFXCanvas;
	UnderCanvas = InUnderCanvas ? InUnderCanvas : InFXCanvas;
	PillarCanvas = InPillarCanvas;
	SetSeed(static_cast<int32>(FPlatformTime::Cycles()));
	UWidgetTree* Tree = InFXCanvas->GetTypedOuter<UWidgetTree>();
	CoverageRetainer = Tree ? Tree->ConstructWidget<URetainerBox>(URetainerBox::StaticClass()) : NewObject<URetainerBox>(InFXCanvas);
	CoverageAccum = Tree ? Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()) : NewObject<UCanvasPanel>(CoverageRetainer);
	CoverageRetainer->SetRetainRendering(true);
	CoverageRetainer->SetRenderingPhase(0, 1);
	CoverageRetainer->SetVisibility(ESlateVisibility::Collapsed);
	CoverageAccum->SetVisibility(ESlateVisibility::HitTestInvisible);
	CoverageRetainer->SetContent(CoverageAccum);
	if (UCanvasPanelSlot* CoverageSlot = InFXCanvas->AddChildToCanvas(CoverageRetainer))
	{
		CoverageSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CoverageSlot->SetOffsets(FMargin(0.f));
		CoverageSlot->SetAlignment(FVector2D::ZeroVector);
		CoverageSlot->SetAutoSize(false);
		CoverageSlot->SetZOrder(ParticleZOrder);
	}
	FXRetainer = Tree ? Tree->ConstructWidget<URetainerBox>(URetainerBox::StaticClass()) : NewObject<URetainerBox>(InFXCanvas);
	FXAccum = Tree ? Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()) : NewObject<UCanvasPanel>(FXRetainer);
	FXRetainer->SetRetainRendering(true);
	FXRetainer->SetRenderingPhase(0, 1);
	FXRetainer->SetTextureParameter(TEXT("Tex"));
	FXRetainer->SetVisibility(ESlateVisibility::Collapsed);
	FXAccum->SetVisibility(ESlateVisibility::HitTestInvisible);
	FXRetainer->SetContent(FXAccum);
	if (UCanvasPanelSlot* RetainerSlot = InFXCanvas->AddChildToCanvas(FXRetainer))
	{
		RetainerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		RetainerSlot->SetOffsets(FMargin(0.f));
		RetainerSlot->SetAlignment(FVector2D::ZeroVector);
		RetainerSlot->SetAutoSize(false);
		RetainerSlot->SetZOrder(ParticleZOrder + 1);
	}

	// 문자열 경로 → 쿡 등록 폴더(DefaultGame.ini: UI/Textures/GachaBox, UI/Materials)
	bool bAllAssets = true;
	auto LoadMaterial = [&bAllAssets](const TCHAR* Name) -> UMaterialInterface*
	{
		UMaterialInterface* Loaded = TSoftObjectPtr<UMaterialInterface>(AssetPath(MaterialDir, Name)).LoadSynchronous();
		if (!Loaded)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaRevealFX] 머티리얼 없음 %s/%s ― 그 효과는 그리지 않는다"), MaterialDir, Name);
			bAllAssets = false;
		}
		return Loaded;
	};
	AddMaterial = LoadMaterial(AddMaterialName);
	OverMaterial = LoadMaterial(OverMaterialName);
	ScreenMulMaterial = LoadMaterial(ScreenMulMaterialName);
	RingMaterial = LoadMaterial(RingMaterialName);
	CoverageMaterial = LoadMaterial(CoverageMaterialName);
	CoverageRingMaterial = LoadMaterial(CoverageRingMaterialName);
	ColorCompositeMaterial = LoadMaterial(ColorCompositeMaterialName);
	CoverageCompositeMaterial = LoadMaterial(CoverageCompositeMaterialName);
	ConfigureCompositeMaterials();

	Textures.SetNum(TextureCount);
	AddMIDs.SetNum(TextureCount);
	OverMIDs.SetNum(TextureCount);
	CoverageMIDs.SetNum(TextureCount);
	for (int32 Id = 0; Id < TextureCount; ++Id)
	{
		Textures[Id] = TSoftObjectPtr<UTexture2D>(AssetPath(TextureDir, TextureNames[Id])).LoadSynchronous();
		if (!Textures[Id])
		{
			UE_LOG(LogTemp, Warning, TEXT("[GachaRevealFX] 텍스처 없음 %s/%s ― 그 효과는 그리지 않는다"), TextureDir, TextureNames[Id]);
			bAllAssets = false;
		}
	}

	// 열 때 한 번에 만든다 ― 클라이맥스(SSS 단독 장면 불꽃 110개)에 위젯을 만들면 그 프레임이 끊긴다
	const int32 Capacity = FGachaSparkSim::Capacity;
	MainImages.SetNum(Capacity);
	CoreImages.SetNum(Capacity);
	RingMIDs.SetNum(Capacity);
	CoverageMainImages.SetNum(Capacity);
	CoverageCoreImages.SetNum(Capacity);
	CoverageRingMIDs.SetNum(Capacity);
	ShownKinds.Init(EGachaFXParticleKind::None, Capacity);
	ShownSerials.Init(0, Capacity);
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		MainImages[Index] = NewImage(FXAccum, ParticleZOrder);
		CoreImages[Index] = NewImage(FXAccum, AddOnTopZOrder);
		CoverageMainImages[Index] = NewImage(CoverageAccum, ParticleZOrder);
		CoverageCoreImages[Index] = NewImage(CoverageAccum, AddOnTopZOrder);
	}
	// 0 바깥 획·2 새는 빛, 1 흰 심·3 칼끝은 같은 누적 RT 에 더한다.
	SlitImages.SetNum(4);
	CoverageSlitImages.SetNum(4);
	for (int32 Index = 0; Index < SlitImages.Num(); ++Index)
	{
		SlitImages[Index] = NewImage(FXAccum, Index % 2 == 0 ? ParticleZOrder : AddOnTopZOrder);
		CoverageSlitImages[Index] = NewImage(CoverageAccum, Index % 2 == 0 ? ParticleZOrder : AddOnTopZOrder);
	}
	// 곱하기 장이 먼저 그려져야 screen = dst·(1−k) + k
	ScreenImages.SetNum(6);
	ScreenImages[0] = NewImage(InFXCanvas, CurtainZOrder);
	ScreenImages[1] = NewImage(InFXCanvas, CurtainZOrder + 1);
	UCanvasPanel* BeamCanvas = UnderCanvas.Get();
	ScreenImages[2] = NewImage(BeamCanvas, UnderZOrder);
	ScreenImages[3] = NewImage(BeamCanvas, UnderZOrder + 1);
	for (int32 Index : { 0, 1 })
	{
		if (UCanvasPanelSlot* CurtainSlot = Cast<UCanvasPanelSlot>(ScreenImages[Index]->Slot))
		{
			// 무대 폭이 3120 보다 좁으면 가운데 맞춰 양옆이 잘린다(굽기 fx.json curtain 규칙)
			CurtainSlot->SetAnchors(FAnchors(.5f, 0.f));
			CurtainSlot->SetAlignment(FVector2D(.5f, 0.f));
			CurtainSlot->SetPosition(FVector2D::ZeroVector);
			CurtainSlot->SetSize(FVector2D(CurtainW, CurtainH));
		}
	}
	UCanvasPanel* SpotCanvas = PillarCanvas.Get();
	ScreenImages[4] = NewImage(SpotCanvas, PillarZOrder);
	ScreenImages[5] = NewImage(SpotCanvas, PillarZOrder + 1);
	for (int32 Index : { 2, 3, 4, 5 })
	{
		if (ScreenImages[Index])
		{
			ScreenImages[Index]->SetRenderTransformPivot(FVector2D(.5f, 1.f));
		}
	}
	ScreenMulMIDs.SetNum(3);
	if (ScreenMulMaterial)
	{
		for (int32 Index = 0; Index < ScreenMulMIDs.Num(); ++Index)
		{
			ScreenMulMIDs[Index] = UMaterialInstanceDynamic::Create(ScreenMulMaterial, this);
		}
	}
	bReady = true;
	if (!bAllAssets)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealFX] 자산이 빠져 일부 효과가 안 그려진다(fx_textures·fx_materials 단계 확인)"));
	}
}

void UGachaRevealFXLayer::Release()
{
	ApplyShake(FVector2D::ZeroVector);
	for (TArray<TObjectPtr<UImage>>* Group : { &MainImages, &CoreImages, &SlitImages, &CoverageMainImages, &CoverageCoreImages, &CoverageSlitImages, &ScreenImages })
	{
		for (UImage* Image : *Group)
		{
			if (Image)
			{
				Image->RemoveFromParent();
			}
		}
		Group->Reset();
	}
	if (FXRetainer) { FXRetainer->RemoveFromParent(); }
	if (CoverageRetainer) { CoverageRetainer->RemoveFromParent(); }
	FXRetainer = nullptr;
	FXAccum = nullptr;
	CoverageRetainer = nullptr;
	CoverageAccum = nullptr;
	ColorCompositeMID = nullptr;
	CoverageCompositeMID = nullptr;
	CoverageTarget = nullptr;
	Textures.Reset();
	AddMIDs.Reset();
	OverMIDs.Reset();
	CoverageMIDs.Reset();
	RingMIDs.Reset();
	CoverageRingMIDs.Reset();
	ScreenMulMIDs.Reset();
	AddMaterial = nullptr;
	OverMaterial = nullptr;
	ScreenMulMaterial = nullptr;
	RingMaterial = nullptr;
	CoverageMaterial = nullptr;
	CoverageRingMaterial = nullptr;
	ColorCompositeMaterial = nullptr;
	CoverageCompositeMaterial = nullptr;
	ShownKinds.Reset();
	ShownSerials.Reset();
	FXCanvas.Reset();
	UnderCanvas.Reset();
	PillarCanvas.Reset();
	Sim.Clear();
	ShakeState.Amp = 0.f;
	Slit = FSlitState();
	CurtainState = FTimedScreen();
	BeamState = FTimedScreen();
	PillarState = FPillarState();
	SlitOuterTexture = INDEX_NONE;
	bReady = false;
}

UImage* UGachaRevealFXLayer::NewImage(UCanvasPanel* Canvas, int32 ZOrder)
{
	if (!Canvas)
	{
		return nullptr;
	}
	UWidgetTree* Tree = Canvas->GetTypedOuter<UWidgetTree>();
	UImage* Image = Tree ? Tree->ConstructWidget<UImage>(UImage::StaticClass()) : NewObject<UImage>(Canvas);
	Image->SetVisibility(ESlateVisibility::Collapsed);
	Image->SetRenderTransformPivot(FVector2D(.5f, .5f));
	if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Image))
	{
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetZOrder(ZOrder);
	}
	return Image;
}

UMaterialInstanceDynamic* UGachaRevealFXLayer::AddMaterialFor(int32 TextureId)
{
	if (!AddMIDs.IsValidIndex(TextureId) || !Textures[TextureId] || !AddMaterial)
	{
		return nullptr;
	}
	if (!AddMIDs[TextureId])
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AddMaterial, this);
		MID->SetTextureParameterValue(TEXT("Tex"), Textures[TextureId]);
		AddMIDs[TextureId] = MID;
	}
	return AddMIDs[TextureId];
}

UMaterialInstanceDynamic* UGachaRevealFXLayer::OverMaterialFor(int32 TextureId)
{
	using namespace GachaRevealFXAssets;

	if (!OverMIDs.IsValidIndex(TextureId) || !Textures[TextureId] || !OverMaterial)
	{
		return nullptr;
	}
	if (!OverMIDs[TextureId])
	{
		UMaterialInstanceDynamic* OverMID = UMaterialInstanceDynamic::Create(OverMaterial, this);
		OverMID->SetTextureParameterValue(TEXT("Tex"), Textures[TextureId]);
		// 그라데이션 그림(빛·새는 빛)은 RGB 가 알파와 같이 검게 간다(시안 그대로) ― 흰 획은 알파만 모양(밉·가장자리에서 검정과 섞인 RGB 가 밝은 바탕에 검은 테를 만든다)
		OverMID->SetScalarParameterValue(TEXT("TexRGB"), TextureId == TexGlow || TextureId == TexSlitLeak ? 1.f : 0.f);
		OverMIDs[TextureId] = OverMID;
	}
	return OverMIDs[TextureId];
}

UMaterialInstanceDynamic* UGachaRevealFXLayer::CoverageMaterialFor(int32 TextureId)
{
	if (!CoverageMIDs.IsValidIndex(TextureId) || !Textures[TextureId] || !CoverageMaterial)
	{
		return nullptr;
	}
	if (!CoverageMIDs[TextureId])
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(CoverageMaterial, this);
		MID->SetTextureParameterValue(TEXT("Tex"), Textures[TextureId]);
		CoverageMIDs[TextureId] = MID;
	}
	return CoverageMIDs[TextureId];
}

void UGachaRevealFXLayer::ConfigureCompositeMaterials()
{
	if (!FXRetainer || !CoverageRetainer)
	{
		return;
	}
	// 원본 RT 브러시를 화면에 그리는 기본 경로는 coverage(빨간색)를 노출하므로 먼저 둘 다 닫는다.
	FXRetainer->SetVisibility(ESlateVisibility::Collapsed);
	CoverageRetainer->SetVisibility(ESlateVisibility::Collapsed);
	ColorCompositeMID = nullptr;
	CoverageCompositeMID = nullptr;
	CoverageTarget = nullptr;
	if (!ColorCompositeMaterial || !CoverageCompositeMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealFX] 합성 재질 누락: Composite=%s Carrier=%s ― 두 리테이너를 숨긴다"),
			ColorCompositeMaterial ? TEXT("있음") : TEXT("없음"), CoverageCompositeMaterial ? TEXT("있음") : TEXT("없음"));
		return;
	}
	ColorCompositeMID = UMaterialInstanceDynamic::Create(ColorCompositeMaterial, this);
	CoverageCompositeMID = UMaterialInstanceDynamic::Create(CoverageCompositeMaterial, this);
	if (!ColorCompositeMID || !CoverageCompositeMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaRevealFX] 합성 MID 생성 실패 ― 두 리테이너를 숨긴다"));
		ColorCompositeMID = nullptr;
		CoverageCompositeMID = nullptr;
		return;
	}
	FXRetainer->SetEffectMaterial(ColorCompositeMID);
	CoverageRetainer->SetEffectMaterial(CoverageCompositeMID);
	CoverageRetainer->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGachaRevealFXLayer::ConnectCoverageTarget()
{
	if (CoverageTarget || !CoverageRetainer || !ColorCompositeMID)
	{
		return;
	}
	UMaterialInstanceDynamic* Carrier = CoverageRetainer->GetEffectMaterial();
	UTexture* Texture = Carrier ? Carrier->K2_GetTextureParameterValue(TEXT("Texture")) : nullptr;
	UTextureRenderTarget2D* Target = Cast<UTextureRenderTarget2D>(Texture);
	if (Target)
	{
		CoverageTarget = Target;
		ColorCompositeMID->SetTextureParameterValue(TEXT("CoverageTex"), Target);
		FXRetainer->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UGachaRevealFXLayer::MirrorCoverage(UImage* ColorImage, UImage* CoverageImage, UMaterialInterface* RingCoverage)
{
	if (!ColorImage || !CoverageImage)
	{
		return;
	}
	if (ColorImage->GetVisibility() != ESlateVisibility::HitTestInvisible)
	{
		HideImage(CoverageImage);
		return;
	}
	UMaterialInterface* CoverageBrush = RingCoverage;
	if (!CoverageBrush)
	{
		UMaterialInstanceDynamic* ColorMID = Cast<UMaterialInstanceDynamic>(ColorImage->GetBrush().GetResourceObject());
		UTexture* ColorTexture = ColorMID ? ColorMID->K2_GetTextureParameterValue(TEXT("Tex")) : nullptr;
		for (int32 Id = 0; Id < Textures.Num(); ++Id)
		{
			if (ColorTexture && ColorTexture == Textures[Id])
			{
				CoverageBrush = CoverageMaterialFor(Id);
				break;
			}
		}
	}
	if (!CoverageBrush)
	{
		HideImage(ColorImage);
		HideImage(CoverageImage);
		return;
	}
	const FSlateBrush& ColorBrush = ColorImage->GetBrush();
	if (CoverageImage->GetBrush().GetResourceObject() != CoverageBrush)
	{
		BrushImage(CoverageImage, CoverageBrush, ColorBrush.ImageSize,
			ColorBrush.DrawAs == ESlateBrushDrawType::Box, ColorBrush.Margin);
	}
	const UCanvasPanelSlot* ColorSlot = Cast<UCanvasPanelSlot>(ColorImage->Slot);
	UCanvasPanelSlot* CoverageSlot = Cast<UCanvasPanelSlot>(CoverageImage->Slot);
	if (ColorSlot && CoverageSlot)
	{
		CoverageSlot->SetAnchors(ColorSlot->GetAnchors());
		CoverageSlot->SetOffsets(ColorSlot->GetOffsets());
		CoverageSlot->SetAlignment(ColorSlot->GetAlignment());
		CoverageSlot->SetAutoSize(ColorSlot->GetAutoSize());
	}
	CoverageImage->SetRenderTransform(ColorImage->GetRenderTransform());
	CoverageImage->SetRenderTransformPivot(ColorImage->GetRenderTransformPivot());
	CoverageImage->SetPixelSnapping(ColorImage->GetPixelSnapping());
	CoverageImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, ColorImage->GetColorAndOpacity().A));
	CoverageImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UGachaRevealFXLayer::BrushImage(UImage* Image, UMaterialInterface* Material, const FVector2D& ImageSize, bool bBox, const FMargin& Margin)
{
	if (!Image)
	{
		return false;
	}
	if (!Material)
	{
		// 빈 브러시는 흰 판으로 그려진다
		HideImage(Image);
		return false;
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(Material);
	Brush.ImageSize = ImageSize;
	Brush.DrawAs = bBox ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
	Brush.Margin = bBox ? Margin : FMargin(0.f);
	Image->SetBrush(Brush);
	return true;
}

void UGachaRevealFXLayer::HideImage(UImage* Image)
{
	if (Image && Image->GetVisibility() != ESlateVisibility::Collapsed)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
}

namespace GachaRevealFXDraw
{
	static void Place(UImage* Image, const FVector2D& TopLeft, const FVector2D& Size)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
		{
			CanvasSlot->SetPosition(TopLeft);
			CanvasSlot->SetSize(Size);
		}
	}

	// 굽기 그림을 그 크기 그대로 놓고 렌더 배율(≤1)로 굵기를 맞춘다 ― Box 여백(둥근 끝)이 굵기와 같은 배로 준다
	static void PlaceStroke(UImage* Image, const GachaFXMath::FStrokeLayout& Layout, const FLinearColor& Tint)
	{
		Place(Image, Layout.Center - Layout.LocalSize * .5, Layout.LocalSize);
		Image->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Layout.Scale * Layout.ScaleX, Layout.Scale),
			FVector2D::ZeroVector, Layout.AngleDeg));
		Image->SetColorAndOpacity(Tint);
		if (Image->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	static void Show(UImage* Image, const FLinearColor& Tint)
	{
		Image->SetColorAndOpacity(Tint);
		if (Image->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UGachaRevealFXLayer::ShowParticle(int32 Index)
{
	using namespace GachaRevealFXAssets;

	const FGachaFXParticle& P = Sim.Particles[Index];
	UImage* Main = MainImages[Index];
	UImage* Core = CoreImages[Index];
	if (!Main || !Core)
	{
		return;
	}
	const bool bNew = ShownKinds[Index] != P.Kind || ShownSerials[Index] != P.Serial;
	Main->SetPixelSnapping(P.Kind == EGachaFXParticleKind::Ring ? EWidgetPixelSnapping::Disabled : EWidgetPixelSnapping::Inherit);
	const float T = P.Progress();
	if (P.Kind == EGachaFXParticleKind::Spark)
	{
		if (ShownKinds[Index] != P.Kind)
		{
			const bool bOuter = BrushImage(Main, OverMaterialFor(TexSparkOuter),
				FVector2D(GachaFXMath::SparkOuter.TexW, GachaFXMath::SparkOuter.TexH), true, StrokeMargin(GachaFXMath::SparkOuter));
			const bool bCore = BrushImage(Core, OverMaterialFor(TexSparkCore),
				FVector2D(GachaFXMath::SparkCore.TexW, GachaFXMath::SparkCore.TexH), true, StrokeMargin(GachaFXMath::SparkCore));
			if (!bOuter || !bCore)
			{
				HideImage(Main);
				HideImage(Core);
				return;
			}
		}
		const float Alpha = GachaFXMath::SparkAlpha(T);
		const float LineW = GachaFXMath::SparkLineWidth(P.Width, T);
		const FVector2D Tail = P.Pos - P.Vel * P.TailSec;
		const FVector2D CoreTail = P.Pos - P.Vel * (P.TailSec * GachaFXMath::SparkCoreLengthMul);
		GachaRevealFXDraw::PlaceStroke(Main, GachaFXMath::LayoutStroke(GachaFXMath::SparkOuter, P.Pos, Tail, LineW * GachaFXMath::SparkOuterWidthMul),
			GachaFXMath::OverTint(P.Color, Alpha));
		GachaRevealFXDraw::PlaceStroke(Core, GachaFXMath::LayoutStroke(GachaFXMath::SparkCore, P.Pos, CoreTail, LineW * GachaFXMath::SparkCoreWidthMul),
			GachaFXMath::OverTint(FLinearColor::White, Alpha));
	}
	else if (P.Kind == EGachaFXParticleKind::Ring)
	{
		HideImage(Core);
		if (!RingMIDs[Index] && RingMaterial)
		{
			RingMIDs[Index] = UMaterialInstanceDynamic::Create(RingMaterial, this);
		}
		if (ShownKinds[Index] != P.Kind && !BrushImage(Main, RingMIDs[Index], FVector2D(1.f, 1.f), false, FMargin()))
		{
			return;
		}
		const float Radius = GachaFXMath::RingRadius(P.R0, P.R1, T);
		const float LineW = GachaFXMath::RingWidth(P.Width, T);
		const float Side = 2.f * (Radius + LineW) + 4.f;
		GachaRevealFXDraw::Place(Main, P.Pos - FVector2D(Side, Side) * .5, FVector2D(Side, Side));
		Main->SetRenderTransform(FWidgetTransform());
		UMaterialInstanceDynamic* MID = RingMIDs[Index];
		if (!MID)
		{
			HideImage(Main);
			return;
		}
		MID->SetScalarParameterValue(TEXT("RadiusPx"), Radius);
		MID->SetScalarParameterValue(TEXT("WidthPx"), LineW);
		MID->SetScalarParameterValue(TEXT("SizePx"), Side);
		MID->SetScalarParameterValue(TEXT("Alpha"), GachaFXMath::RingAlpha(T));
		MID->SetVectorParameterValue(TEXT("Color"), P.Color);
		GachaRevealFXDraw::Show(Main, FLinearColor::White);
	}
	else if (P.Kind == EGachaFXParticleKind::Glow)
	{
		HideImage(Core);
		if (ShownKinds[Index] != P.Kind && !BrushImage(Main, OverMaterialFor(TexGlow), FVector2D(1.f, 1.f), false, FMargin()))
		{
			return;
		}
		if (bNew)
		{
			const float Diameter = 2.f * P.Radius / GachaFXMath::GlowTexRadiusRatio;
			GachaRevealFXDraw::Place(Main, P.Pos - FVector2D(Diameter, Diameter) * .5, FVector2D(Diameter, Diameter));
			Main->SetRenderTransform(FWidgetTransform());
		}
		GachaRevealFXDraw::Show(Main, GachaFXMath::OverTint(P.Color, GachaFXMath::GlowAlpha(P.Peak, T)));
	}
	ShownKinds[Index] = P.Kind;
	ShownSerials[Index] = P.Serial;
}

void UGachaRevealFXLayer::UpdateParticles()
{
	for (int32 Index = 0; Index < MainImages.Num(); ++Index)
	{
		if (Sim.Particles[Index].IsActive())
		{
			ShowParticle(Index);
			UMaterialInstanceDynamic* CoverageRing = nullptr;
			if (Sim.Particles[Index].Kind == EGachaFXParticleKind::Ring && RingMIDs[Index] && CoverageRingMaterial)
			{
				if (!CoverageRingMIDs[Index])
				{
					CoverageRingMIDs[Index] = UMaterialInstanceDynamic::Create(CoverageRingMaterial, this);
				}
				CoverageRing = CoverageRingMIDs[Index];
				for (const FName Name : { FName(TEXT("RadiusPx")), FName(TEXT("WidthPx")), FName(TEXT("SizePx")), FName(TEXT("Alpha")) })
				{
					CoverageRing->SetScalarParameterValue(Name, RingMIDs[Index]->K2_GetScalarParameterValue(Name));
				}
			}
			MirrorCoverage(MainImages[Index], CoverageMainImages[Index], CoverageRing);
			MirrorCoverage(CoreImages[Index], CoverageCoreImages[Index]);
			continue;
		}
		if (ShownKinds[Index] != EGachaFXParticleKind::None)
		{
			HideImage(MainImages[Index]);
			HideImage(CoreImages[Index]);
			HideImage(CoverageMainImages[Index]);
			HideImage(CoverageCoreImages[Index]);
			ShownKinds[Index] = EGachaFXParticleKind::None;
		}
	}
}

void UGachaRevealFXLayer::UpdateSlit()
{
	using namespace GachaRevealFXAssets;

	if (SlitImages.Num() != 4)
	{
		return;
	}
	UImage* OuterImage = SlitImages[0];
	UImage* CoreImage = SlitImages[1];
	UImage* LeakImage = SlitImages[2];
	UImage* BladeImage = SlitImages[3];
	if (!Slit.bOn)
	{
		for (UImage* Image : SlitImages)
		{
			HideImage(Image);
		}
		for (UImage* Image : CoverageSlitImages)
		{
			HideImage(Image);
		}
		return;
	}
	const float I = Slit.Intensity;
	const float P = FMath::Clamp(Slit.Progress, 0.f, 1.f);
	const FVector2D Tip = FMath::Lerp(Slit.L, Slit.R, static_cast<double>(P));

	float BakedIntensity = 1.f;
	GachaFXMath::FStrokeSprite OuterSprite = GachaFXMath::SlitOuter100;
	const int32 OuterId = SlitOuterFor(I, BakedIntensity, OuterSprite);
	bool bOuter = true;
	if (OuterId != SlitOuterTexture)
	{
		bOuter = BrushImage(OuterImage, OverMaterialFor(OuterId), FVector2D(OuterSprite.TexW, OuterSprite.TexH), true, StrokeMargin(OuterSprite));
		SlitOuterTexture = bOuter ? OuterId : INDEX_NONE;
	}
	if (bOuter)
	{
		// 구운 바깥 획에 알파 .9·세기·번짐 50·세기가 이미 들어 있다 ― 세 단계 밖의 세기만 비율로 맞춘다
		GachaRevealFXDraw::PlaceStroke(OuterImage, GachaFXMath::LayoutStroke(OuterSprite, Slit.L, Tip, OuterSprite.LineWidth),
			GachaFXMath::OverTint(Slit.Color, I / BakedIntensity));
	}
	if (CoreImage->GetBrush().GetResourceObject() != OverMaterialFor(TexSlitCore))
	{
		BrushImage(CoreImage, OverMaterialFor(TexSlitCore), FVector2D(GachaFXMath::SlitCore.TexW, GachaFXMath::SlitCore.TexH), true,
			StrokeMargin(GachaFXMath::SlitCore));
	}
	if (CoreImage->GetBrush().GetResourceObject())
	{
		GachaRevealFXDraw::PlaceStroke(CoreImage, GachaFXMath::LayoutStroke(GachaFXMath::SlitCore, Slit.L, Tip, GachaFXMath::SlitCore.LineWidth),
			GachaFXMath::OverTint(FLinearColor::White, I));
	}

	// 시안 :619-622 틈 위로 새는 빛 ― 이음매를 아래 변으로 하는 기울인 띠(원장 Ruling: 왼쪽 끝 최대 23% 차 수용)
	const double Dx = Tip.X - Slit.L.X;
	if (I > .6f && P > .05f && FMath::Abs(Dx) > .5)
	{
		if (LeakImage->GetBrush().GetResourceObject() != OverMaterialFor(TexSlitLeak))
		{
			BrushImage(LeakImage, OverMaterialFor(TexSlitLeak), FVector2D(8.f, GachaFXMath::SlitLeakHeight), false, FMargin());
		}
		if (LeakImage->GetBrush().GetResourceObject())
		{
			const FVector2D Mid = (Slit.L + Tip) * .5;
			const float W = static_cast<float>(FMath::Abs(Dx));
			const float Slope = static_cast<float>((Tip.Y - Slit.L.Y) / Dx);
			const float ShearDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(Slope)), -89.f, 89.f);
			GachaRevealFXDraw::Place(LeakImage, FVector2D(Mid.X - W * .5f, Mid.Y - GachaFXMath::SlitLeakHeight), FVector2D(W, GachaFXMath::SlitLeakHeight));
			LeakImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.f, 1.f), FVector2D(0.f, ShearDeg), 0.f));
			GachaRevealFXDraw::Show(LeakImage, GachaFXMath::OverTint(Slit.Color, .22f * (I - .6f) / .4f * 1.4f));
		}
	}
	else
	{
		HideImage(LeakImage);
	}

	if (Slit.bBlade)
	{
		if (BladeImage->GetBrush().GetResourceObject() != OverMaterialFor(TexBlade))
		{
			BrushImage(BladeImage, OverMaterialFor(TexBlade), FVector2D(GachaFXMath::BladeSize, GachaFXMath::BladeSize), false, FMargin());
		}
		if (BladeImage->GetBrush().GetResourceObject())
		{
			const FVector2D Size(GachaFXMath::BladeSize, GachaFXMath::BladeSize);
			GachaRevealFXDraw::Place(BladeImage, Tip - Size * .5, Size);
			GachaRevealFXDraw::Show(BladeImage, GachaFXMath::OverTint(FLinearColor::White, 1.f));
		}
	}
	else
	{
		HideImage(BladeImage);
	}
	for (int32 Index = 0; Index < SlitImages.Num(); ++Index)
	{
		MirrorCoverage(SlitImages[Index], CoverageSlitImages[Index]);
	}
}

void UGachaRevealFXLayer::UpdateScreen(FTimedScreen& State, int32 MulIndex, float DeltaSeconds, bool bBeam)
{
	using namespace GachaRevealFXAssets;

	UImage* MulImage = ScreenImages.IsValidIndex(MulIndex) ? ScreenImages[MulIndex].Get() : nullptr;
	UImage* AddImage = ScreenImages.IsValidIndex(MulIndex + 1) ? ScreenImages[MulIndex + 1].Get() : nullptr;
	if (!MulImage || !AddImage)
	{
		return;
	}
	if (!State.bActive)
	{
		HideImage(MulImage);
		HideImage(AddImage);
		return;
	}
	State.Elapsed += DeltaSeconds;
	if (State.Elapsed >= State.Duration)
	{
		State.bActive = false;
		HideImage(MulImage);
		HideImage(AddImage);
		return;
	}
	const float Opacity = State.Peak * GachaFXMath::PeakEnvelope(State.Elapsed, State.Duration, State.PeakAt);
	if (bBeam)
	{
		const FVector2D Size(State.Width, BeamHeight);
		const FWidgetTransform Grow(FVector2D::ZeroVector, FVector2D(1.f, GachaFXMath::BeamScaleY(State.Elapsed, State.Duration)), FVector2D::ZeroVector, 0.f);
		for (UImage* Image : { MulImage, AddImage })
		{
			GachaRevealFXDraw::Place(Image, State.Base - FVector2D(State.Width * .5f, BeamHeight), Size);
			Image->SetRenderTransform(Grow);
		}
	}
	ShowScreenPair(MulImage, AddImage, ScreenMulMIDs[MulIndex / 2], State.Color, Opacity);
}

void UGachaRevealFXLayer::ShowScreenPair(UImage* MulImage, UImage* AddImage, UMaterialInstanceDynamic* MulMID, const FLinearColor& Color, float Opacity)
{
	// 곱하기·더하기 중 하나라도 브러시가 없으면 둘 다 숨긴다(반쪽 = 어두워짐만 남음)
	if (!MulMID || !MulImage->GetBrush().GetResourceObject() || !AddImage->GetBrush().GetResourceObject())
	{
		HideImage(MulImage);
		HideImage(AddImage);
		return;
	}
	MulMID->SetVectorParameterValue(TEXT("Color"), Color);
	MulMID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	GachaRevealFXDraw::Show(MulImage, FLinearColor::White);
	GachaRevealFXDraw::Show(AddImage, GachaFXMath::ScaleInSRGB(Color, Opacity));
}

void UGachaRevealFXLayer::UpdatePillar(float DeltaSeconds)
{
	using namespace GachaRevealFXAssets;

	UImage* MulImage = ScreenImages.IsValidIndex(4) ? ScreenImages[4].Get() : nullptr;
	UImage* AddImage = ScreenImages.IsValidIndex(5) ? ScreenImages[5].Get() : nullptr;
	if (!MulImage || !AddImage)
	{
		return;
	}
	FPillarState& State = PillarState;
	if (!State.bActive)
	{
		HideImage(MulImage);
		HideImage(AddImage);
		return;
	}
	State.Elapsed += DeltaSeconds;
	float Opacity = 1.f;
	float ScaleY = 1.f;
	if (State.bFading)
	{
		if (State.FadeSec <= 0.f || State.Elapsed >= State.FadeSec)
		{
			State.bActive = false;
			HideImage(MulImage);
			HideImage(AddImage);
			return;
		}
		Opacity = GachaFXMath::PillarOffOpacity(State.Elapsed, State.FadeSec);
	}
	else
	{
		Opacity = GachaFXMath::PillarOnOpacity(State.Elapsed, State.RiseSec, State.DelaySec);
		ScaleY = GachaFXMath::PillarScaleY(State.Elapsed, State.RiseSec);
	}
	const FWidgetTransform Grow(FVector2D::ZeroVector, FVector2D(1.f, ScaleY), FVector2D::ZeroVector, 0.f);
	for (UImage* Image : { MulImage, AddImage })
	{
		GachaRevealFXDraw::Place(Image, State.Base - FVector2D(PillarW * .5f, PillarH), FVector2D(PillarW, PillarH));
		Image->SetRenderTransform(Grow);
	}
	ShowScreenPair(MulImage, AddImage, ScreenMulMIDs.IsValidIndex(2) ? ScreenMulMIDs[2].Get() : nullptr, State.Color, Opacity);
}

void UGachaRevealFXLayer::Tick(float DeltaSeconds)
{
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, GachaRevealFXAssets::MaxFrameSec);
	Sim.Step(Dt, Speed);
	if (bReady)
	{
		ConnectCoverageTarget();
		UpdateSlit();
		UpdateParticles();
		UpdateScreen(CurtainState, 0, FMath::Max(DeltaSeconds, 0.f), false);
		UpdateScreen(BeamState, 2, FMath::Max(DeltaSeconds, 0.f), true);
		UpdatePillar(FMath::Max(DeltaSeconds, 0.f));
		if (CoverageRetainer) { CoverageRetainer->RequestRender(); }
		if (FXRetainer) { FXRetainer->RequestRender(); }
	}
	bool bShakeActive = false;
	const FVector2D Offset = ShakeState.Step(Dt, Sim.Rng, bShakeActive);
	if (bShakeActive)
	{
		ApplyShake(Offset);
	}
}

void UGachaRevealFXLayer::Clear()
{
	Sim.Clear();
	Slit = FSlitState();
	CurtainState = FTimedScreen();
	BeamState = FTimedScreen();
	PillarState = FPillarState();
	ShakeState.Amp = 0.f;
	ApplyShake(FVector2D::ZeroVector);
	if (bReady)
	{
		UpdateSlit();
		UpdateParticles();
		UpdateScreen(CurtainState, 0, 0.f, false);
		UpdateScreen(BeamState, 2, 0.f, true);
		UpdatePillar(0.f);
		if (CoverageRetainer) { CoverageRetainer->RequestRender(); }
		if (FXRetainer) { FXRetainer->RequestRender(); }
	}
}

void UGachaRevealFXLayer::Sparks(FVector2D Pos, const FGachaSparkParams& Params)
{
	Sim.Sparks(Pos, Params, SparkMul);
}

void UGachaRevealFXLayer::Ring(FVector2D Pos, float R0, float R1, float Life, FLinearColor Color, float Width)
{
	if (Life > 0.f)
	{
		Sim.Ring(Pos, R0, R1, Life, Color, Width);
	}
}

void UGachaRevealFXLayer::Glow(FVector2D Pos, float Radius, float Life, FLinearColor Color, float Peak)
{
	if (Life > 0.f && Radius > 0.f)
	{
		Sim.Glow(Pos, Radius, Life, Color, Peak);
	}
}

void UGachaRevealFXLayer::Curtain(FLinearColor Color, float Peak, float DurationSec, EGachaCurtainShape Shape)
{
	using namespace GachaRevealFXAssets;

	if (DurationSec <= 0.f || !bReady)
	{
		return;
	}
	CurtainState = FTimedScreen();
	CurtainState.bActive = true;
	CurtainState.Duration = DurationSec;
	CurtainState.Peak = Peak;
	CurtainState.PeakAt = Shape == EGachaCurtainShape::Spot ? SpotCurtainPeakAt : LidCurtainPeakAt;
	CurtainState.Color = Color;
	CurtainShape = Shape;
	const int32 TextureId = Shape == EGachaCurtainShape::Spot ? TexCurtainSpot : TexCurtainLid;
	if (ScreenMulMIDs.IsValidIndex(0) && ScreenMulMIDs[0] && Textures[TextureId])
	{
		ScreenMulMIDs[0]->SetTextureParameterValue(TEXT("Tex"), Textures[TextureId]);
	}
	BrushImage(ScreenImages[0], ScreenMulMIDs.IsValidIndex(0) && Textures[TextureId] ? ScreenMulMIDs[0].Get() : nullptr,
		FVector2D(CurtainW, CurtainH), false, FMargin());
	BrushImage(ScreenImages[1], AddMaterialFor(TextureId), FVector2D(CurtainW, CurtainH), false, FMargin());
}

void UGachaRevealFXLayer::Beam(FVector2D Base, float Width, FLinearColor Color, float Peak, float DurationSec)
{
	using namespace GachaRevealFXAssets;

	if (DurationSec <= 0.f || Width <= 0.f || !bReady)
	{
		return;
	}
	BeamState = FTimedScreen();
	BeamState.bActive = true;
	BeamState.Duration = DurationSec;
	BeamState.Peak = Peak;
	BeamState.PeakAt = BeamPeakAt;
	BeamState.Color = Color;
	BeamState.Base = Base;
	BeamState.Width = Width;
	if (ScreenMulMIDs.IsValidIndex(1) && ScreenMulMIDs[1] && Textures[GachaRevealFXAssets::TexBeam])
	{
		ScreenMulMIDs[1]->SetTextureParameterValue(TEXT("Tex"), Textures[GachaRevealFXAssets::TexBeam]);
	}
	BrushImage(ScreenImages[2], ScreenMulMIDs.IsValidIndex(1) && Textures[GachaRevealFXAssets::TexBeam] ? ScreenMulMIDs[1].Get() : nullptr,
		FVector2D(Width, BeamHeight), false, FMargin());
	BrushImage(ScreenImages[3], AddMaterialFor(GachaRevealFXAssets::TexBeam), FVector2D(Width, BeamHeight), false, FMargin());
}

void UGachaRevealFXLayer::PillarOn(FVector2D Base, FLinearColor Color, float RiseSec, float BreathDelaySec)
{
	using namespace GachaRevealFXAssets;

	if (!bReady || !ScreenImages.IsValidIndex(5) || !ScreenImages[4] || !ScreenImages[5])
	{
		return;
	}
	PillarState = FPillarState();
	PillarState.bActive = true;
	PillarState.RiseSec = FMath::Max(RiseSec, 0.f);
	PillarState.DelaySec = FMath::Max(BreathDelaySec, 0.f);
	PillarState.Base = Base;
	PillarState.Color = Color;
	const bool bMul = ScreenMulMIDs.IsValidIndex(2) && ScreenMulMIDs[2] && Textures[TexPillar];
	if (bMul)
	{
		ScreenMulMIDs[2]->SetTextureParameterValue(TEXT("Tex"), Textures[TexPillar]);
	}
	BrushImage(ScreenImages[4], bMul ? ScreenMulMIDs[2].Get() : nullptr, FVector2D(PillarW, PillarH), false, FMargin());
	BrushImage(ScreenImages[5], AddMaterialFor(TexPillar), FVector2D(PillarW, PillarH), false, FMargin());
	UpdatePillar(0.f);
}

void UGachaRevealFXLayer::PillarOff(float FadeSec)
{
	if (!PillarState.bActive)
	{
		return;
	}
	PillarState.bFading = true;
	PillarState.Elapsed = 0.f;
	PillarState.FadeSec = FMath::Max(FadeSec, 0.f);
	UpdatePillar(0.f);
}

void UGachaRevealFXLayer::SetSlit(bool bOn, FVector2D L, FVector2D R, float Progress, FLinearColor Color, float Intensity, bool bBlade)
{
	Slit.bOn = bOn;
	Slit.L = L;
	Slit.R = R;
	Slit.Progress = Progress;
	Slit.Color = Color;
	Slit.Intensity = FMath::Max(Intensity, 0.f);
	Slit.bBlade = bBlade;
}

void UGachaRevealFXLayer::Shake(float Px)
{
	ShakeState.Kick(Px, ShakeMul);
}

void UGachaRevealFXLayer::SetShakeTarget(UWidget* Target)
{
	ApplyShake(FVector2D::ZeroVector);
	ShakeTargets.Reset();
	AddShakeTarget(Target);
}

void UGachaRevealFXLayer::AddShakeTarget(UWidget* Target)
{
	if (Target)
	{
		ShakeTargets.AddUnique(Target);
	}
}

void UGachaRevealFXLayer::SetIntensity(float InSparkMul, float InShakeMul, float InSpeed)
{
	SparkMul = FMath::Max(InSparkMul, 0.f);
	ShakeMul = FMath::Max(InShakeMul, 0.f);
	Speed = FMath::Max(InSpeed, .01f);
	if (ShakeMul == 0.f && ShakeState.Amp != 0.f)
	{
		ShakeState.Amp = 0.f;
		ApplyShake(FVector2D::ZeroVector);
	}
}

void UGachaRevealFXLayer::SetSeed(int32 Seed)
{
	Sim.Rng.Initialize(Seed);
}

void UGachaRevealFXLayer::ApplyShake(const FVector2D& Offset)
{
	for (const TWeakObjectPtr<UWidget>& Target : ShakeTargets)
	{
		if (UWidget* Widget = Target.Get())
		{
			Widget->SetRenderTranslation(Offset);
		}
	}
}
