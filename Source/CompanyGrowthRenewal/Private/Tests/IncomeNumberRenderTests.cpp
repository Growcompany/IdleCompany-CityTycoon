#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Element/Common/CoinFlyoutContainerWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "AssetCompilingManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Internationalization/Internationalization.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "SceneRenderTargetParameters.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace IncomeNumberRenderTests
{
	struct FCase
	{
		int64 Amount;
		const TCHAR* Label;
	};

	static const FCase Cases[] = {
		{ 847, TEXT("+847") },
		{ 9999, TEXT("+9,999") },
		{ 12400, TEXT("+1.24만") },
		{ 124000000, TEXT("+1.24억") },
		{ 1240000000000LL, TEXT("+1.24조") },
		{ 12400000000000000LL, TEXT("+1.24경") },
	};

	static const TCHAR* GlyphName(TCHAR Character)
	{
		switch (Character)
		{
		case TEXT('0'): return TEXT("0");
		case TEXT('1'): return TEXT("1");
		case TEXT('2'): return TEXT("2");
		case TEXT('3'): return TEXT("3");
		case TEXT('4'): return TEXT("4");
		case TEXT('5'): return TEXT("5");
		case TEXT('6'): return TEXT("6");
		case TEXT('7'): return TEXT("7");
		case TEXT('8'): return TEXT("8");
		case TEXT('9'): return TEXT("9");
		case TEXT('+'): return TEXT("Plus");
		case TEXT('.'): return TEXT("Dot");
		case TEXT(','): return TEXT("Comma");
		case TEXT('만'): return TEXT("Man");
		case TEXT('억'): return TEXT("Eok");
		case TEXT('조'): return TEXT("Jo");
		case TEXT('경'): return TEXT("Gyeong");
		default: return nullptr;
		}
	}

	static void Visit(const UWidget* Parent, TFunctionRef<void(const UWidget*)> Callback)
	{
		Callback(Parent);
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Parent))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				Visit(Panel->GetChildAt(ChildIndex), Callback);
			}
		}
	}

	static bool Capture(const TSharedRef<SWidget>& Widget, float Scale, FImage& OutImage)
	{
		const FIntPoint Size(900, 220);
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid()) return false;
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Widget, Scale, FVector2D(Size.X, Size.Y),
				0.0f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage)
			|| OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static int32 CountVisiblePixels(const FImage& Image)
	{
		int32 Count = 0;
		for (const FColor& Pixel : Image.AsBGRA8())
		{
			if (Pixel.A > 32) ++Count;
		}
		return Count;
	}

	static bool HasSceneUniformBuffer()
	{
		bool bAvailable = false;
		ENQUEUE_RENDER_COMMAND(IncomeSceneUniformPrerequisite)([&bAvailable](FRHICommandListImmediate& RHICmdList)
		{
			bAvailable = GetSceneTextureExtracts().GetUniformBuffer() != nullptr;
		});
		FlushRenderingCommands();
		return bAvailable;
	}

	struct FRestoreCulture
	{
		FInternationalization::FCultureStateSnapshot Saved;
		FRestoreCulture() { FInternationalization::Get().BackupCultureState(Saved); }
		~FRestoreCulture() { FInternationalization::Get().RestoreCultureState(Saved); }
	};

	class FIncomeCaptureCommand final : public IAutomationLatentCommand
	{
	public:
		FIncomeCaptureCommand(FAutomationTestBase* InTest, UCoinFlyoutContainerWidget* InContainer,
			UHorizontalBox* InRow, int64 InAmount, FString InDirectory)
			: Test(InTest), Container(InContainer), Row(InRow), Amount(InAmount),
			  Directory(MoveTemp(InDirectory)), StartFrame(GFrameCounter), StartTime(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			if (GFrameCounter < StartFrame + 2)
			{
				if (FPlatformTime::Seconds() - StartTime < 30.0) return false;
				Test->AddError(TEXT("Income capture did not receive two engine frames"));
				return true;
			}
			if (!Container.IsValid() || !Row.IsValid())
			{
				Test->AddError(TEXT("Income row expired before capture"));
				return true;
			}
			FAssetCompilingManager::Get().FinishAllCompilation();
			if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
			FlushRenderingCommands();
			if (!HasSceneUniformBuffer())
			{
				if (!bPrerequisiteLogged)
				{
					Test->AddInfo(TEXT("[IncomePrerequisite] scene texture uniform buffer unavailable; waiting for the editor scene to render"));
					bPrerequisiteLogged = true;
				}
				if (FPlatformTime::Seconds() - StartTime >= 30.0)
				{
					Test->AddError(TEXT("Income capture prerequisite unavailable after 30 seconds: scene texture uniform buffer"));
					return true;
				}
				return false;
			}

			TSharedRef<SWidget> RowFrame = SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					Row->TakeWidget()
				];
			for (float Scale : { 1.0f, 0.75f, 0.5f })
			{
				FImage Image;
				const FString CaseName = FString::Printf(TEXT("%lld at %.2f"), Amount, Scale);
				if (!Test->TestTrue(CaseName + TEXT(" render target readback"),
					Capture(RowFrame, Scale, Image))) continue;
				Test->TestTrue(CaseName + TEXT(" contains visible glyph pixels"),
					CountVisiblePixels(Image) > 50);
				TArray64<uint8> Bytes;
				const FString Path = Directory / FString::Printf(TEXT("income_%lld_%.2f.png"), Amount, Scale);
				Test->TestTrue(CaseName + TEXT(" PNG saved"),
					FImageUtils::CompressImage(Bytes, TEXT("png"), Image)
					&& FFileHelper::SaveArrayToFile(Bytes, *Path));
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TStrongObjectPtr<UCoinFlyoutContainerWidget> Container;
		TStrongObjectPtr<UHorizontalBox> Row;
		int64 Amount;
		FString Directory;
		uint64 StartFrame;
		double StartTime;
		bool bPrerequisiteLogged = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncomeNumberGlyphRenderTest,
	"CGR.UI.IncomeNumber.GlyphComposition",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIncomeNumberGlyphRenderTest::RunTest(const FString& Parameters)
{
	using namespace IncomeNumberRenderTests;

	UWorld* TestWorld = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Editor && Context.World())
			{
				TestWorld = Context.World();
				break;
			}
		}
	}
	if (!TestNotNull(TEXT("Editor world"), TestWorld)) return false;
	const FString CaptureDirectory = FPaths::ProjectSavedDir() / TEXT("IncomeNumberCapture") /
		FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	IFileManager::Get().MakeDirectory(*CaptureDirectory, true);

	for (const FCase& Sample : Cases)
	{
		const FString Expected(Sample.Label);
		const FString Actual = UGlobalUtilFunctions::FormatFundsAmount(Sample.Amount, true).ToString();
		TestEqual(TEXT("FormatFundsAmount retains the exact income label"), Actual, Expected);

		TStrongObjectPtr<UCoinFlyoutContainerWidget> Container(
			CreateWidget<UCoinFlyoutContainerWidget>(TestWorld));
		if (!TestNotNull(TEXT("Native income container"), Container.Get())) return false;
		Container->SpawnIncomeText(FVector2D(400.0f, 100.0f), Sample.Amount,
			nullptr, FLinearColor(0.067f, 0.471f, 0.098f, 1.0f));

		UCanvasPanel* Canvas = Cast<UCanvasPanel>(Container->WidgetTree->RootWidget);
		if (!TestNotNull(TEXT("Income canvas"), Canvas)) return false;
		TestEqual(TEXT("One active income row"), Canvas->GetChildrenCount(), 1);
		if (Canvas->GetChildrenCount() != 1) continue;

		UHorizontalBox* Row = Cast<UHorizontalBox>(Canvas->GetChildAt(0));
		if (!TestNotNull(TEXT("Income row"), Row)) continue;

		TArray<const UImage*> GlyphImages;
		int32 TextBlockCount = 0;
		Visit(Row, [&](const UWidget* Child)
		{
			if (const UImage* Image = Cast<UImage>(Child)) GlyphImages.Add(Image);
			if (Child->IsA<UTextBlock>()) ++TextBlockCount;
		});
		TestEqual(TEXT("Income row has no Slate text outline layers"), TextBlockCount, 0);
		TestEqual(TEXT("Income row contains one image per formatted glyph"), GlyphImages.Num(), Expected.Len());

		for (int32 GlyphIndex = 0; GlyphIndex < FMath::Min(GlyphImages.Num(), Expected.Len()); ++GlyphIndex)
		{
			const TCHAR* Name = GlyphName(Expected[GlyphIndex]);
			if (!TestNotNull(TEXT("Known formatted glyph"), Name)) continue;
			const UImage* Image = GlyphImages[GlyphIndex];
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject());
			if (!TestNotNull(TEXT("Glyph image has a dynamic material"), MID)) continue;
			UTexture* Texture = MID->K2_GetTextureParameterValue(TEXT("Glyph"));
			const FString ExpectedPath = FString::Printf(
				TEXT("/Game/CompanyGrowth/UI/Textures/Effect/IncomeGlyph/48/T_UI_IncomeGlyph_%s.T_UI_IncomeGlyph_%s"),
				Name, Name);
			TestEqual(TEXT("Glyph material uses its matching baked texture"),
				Texture ? Texture->GetPathName() : FString(), ExpectedPath);
			TestTrue(TEXT("Glyph image has a measured draw size"),
				Image->GetBrush().ImageSize.X > 0.0f && Image->GetBrush().ImageSize.Y > 0.0f);
		}

		// Keep the native row alive until the renderer has received normal resource updates.
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FIncomeCaptureCommand>(
			this, Container.Get(), Row, Sample.Amount, CaptureDirectory));
	}
	{
		FRestoreCulture Restore;
		for (const TCHAR* CultureName : { TEXT("fr"), TEXT("ar") })
		{
			if (!TestTrue(TEXT("Smoke culture is available"),
				FInternationalization::Get().SetCurrentCulture(CultureName))) continue;
			TStrongObjectPtr<UCoinFlyoutContainerWidget> Container(
				CreateWidget<UCoinFlyoutContainerWidget>(TestWorld));
			if (!TestNotNull(TEXT("Culture smoke income container"), Container.Get())) continue;
			Container->SpawnIncomeText(FVector2D(400.0f, 100.0f), 9999,
				nullptr, FLinearColor(0.067f, 0.471f, 0.098f, 1.0f));
			UCanvasPanel* Canvas = Cast<UCanvasPanel>(Container->WidgetTree->RootWidget);
			if (!TestNotNull(TEXT("Culture smoke canvas"), Canvas)) continue;
			if (!TestEqual(TEXT("Invariant income glyph row survives localized numeric formatting"),
				Canvas->GetChildrenCount(), 1)) continue;
			TArray<const UImage*> GlyphImages;
			Visit(Canvas->GetChildAt(0), [&](const UWidget* Child)
			{
				if (const UImage* Image = Cast<UImage>(Child)) GlyphImages.Add(Image);
			});
			const FString Expected(TEXT("+9,999"));
			if (!TestEqual(TEXT("Invariant income row keeps ASCII comma glyph count"),
				GlyphImages.Num(), Expected.Len())) continue;
			for (int32 GlyphIndex = 0; GlyphIndex < Expected.Len(); ++GlyphIndex)
			{
				UMaterialInstanceDynamic* MID =
					Cast<UMaterialInstanceDynamic>(GlyphImages[GlyphIndex]->GetBrush().GetResourceObject());
				if (!TestNotNull(TEXT("Localized income glyph material"), MID)) continue;
				const UTexture* Texture = MID->K2_GetTextureParameterValue(TEXT("Glyph"));
				TestEqual(TEXT("Localized number uses invariant glyph textures"),
					Texture ? Texture->GetName() : FString(),
					FString::Printf(TEXT("T_UI_IncomeGlyph_%s"), GlyphName(Expected[GlyphIndex])));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("Income number captures: %s"), *CaptureDirectory));
	return true;
}

#endif
