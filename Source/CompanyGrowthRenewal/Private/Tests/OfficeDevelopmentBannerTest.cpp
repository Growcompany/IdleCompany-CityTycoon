#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Element/Effects/DevelopmentBannerWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeDevelopmentBannerRenderingTest,
	"CompanyGrowth.UI.OfficeDevelopmentEffects.BannerRendering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOfficeDevelopmentBannerRenderingTest::RunTest(const FString& Parameters)
{
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world exists"), EditorWorld)) return false;
	const FString OutputDirectory = FPaths::ProjectSavedDir() / TEXT("OfficeDevVFX/BannerRendering");
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	for (int32 Variant = 0; Variant < 3; ++Variant)
	{
		const bool bStart = Variant == 2;
		const bool bCompletion = !bStart;
		const TCHAR* AssetPath = bStart
			? TEXT("/Game/CompanyGrowth/UI/Elements/Effects/UIE_DevelopmentStart.UIE_DevelopmentStart_C")
			: TEXT("/Game/CompanyGrowth/UI/Elements/Effects/UIE_DevelopmentBanner.UIE_DevelopmentBanner_C");
		UClass* BannerClass = LoadClass<UDevelopmentBannerWidget>(nullptr, AssetPath);
		if (!TestNotNull(TEXT("Authored banner WBP class exists"), BannerClass)) return false;
		TStrongObjectPtr<UDevelopmentBannerWidget> Banner(CreateWidget<UDevelopmentBannerWidget>(EditorWorld, BannerClass));
		if (!TestNotNull(TEXT("Banner WBP constructs"), Banner.Get())) return false;
		TestTrue(TEXT("Presentation animation is bound with authored duration"),
			FMath::IsNearlyEqual(Banner->GetPresentationDuration(), bStart ? .95f : (bCompletion ? 2.2f : 1.34f), .001f));
		for (const FName WidgetName : { FName(TEXT("AnimationRoot")), FName(TEXT("BannerCanvas")),
			FName(TEXT("BackgroundImage")), FName(TEXT("OutlineImage")), FName(TEXT("ShadowImage")),
			FName(TEXT("HighlightImage")), FName(TEXT("LeftStarImage")), FName(TEXT("RightStarImage")),
			FName(TEXT("LeftStarOutlineImage")), FName(TEXT("RightStarOutlineImage")),
			FName(TEXT("TitleText")), FName(TEXT("SubtitleText")) })
		{
			TestNotNull(*FString::Printf(TEXT("Required WBP child %s"), *WidgetName.ToString()), Banner->GetWidgetFromName(WidgetName));
		}
		const FText Title = FText::FromString(bStart ? TEXT("START") : Variant == 1 ? TEXT("문구 변경 가능") : (bCompletion ? TEXT("개발 종료!") : TEXT("분야 목표 달성!")));
		if (!bStart)
		{
			Banner->SetBannerText(Title,
				bCompletion ? FText::FromString(TEXT("목표 5/5 달성")) : FText::FromString(TEXT("5/5")));
		}
		if (Variant == 1)
		{
			Banner->BackgroundBottomColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("#79c8ff")));
			Banner->SetAccentColor(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("#e5f5ff"))));
		}
		Banner->SynchronizeProperties();
		const UCommonTextBlock* TitleBlock = Cast<UCommonTextBlock>(Banner->GetWidgetFromName(TEXT("TitleText")));
		TestTrue(TEXT("Title remains independent editable text after appearance update"), TitleBlock && TitleBlock->GetText().EqualTo(Title));
		if (bStart)
		{
			for (const FName Decoration : { FName(TEXT("LeftStarImage")), FName(TEXT("RightStarImage")), FName(TEXT("SubtitleText")) })
			{
				const UWidget* ChildWidget = Banner->GetWidgetFromName(Decoration);
				TestTrue(TEXT("Start variant keeps unused banner decorations collapsed"), ChildWidget && ChildWidget->GetVisibility() == ESlateVisibility::Collapsed);
			}
			TestTrue(TEXT("Start presentation passes input through"), Banner->GetVisibility() == ESlateVisibility::HitTestInvisible);
		}
		const FVector2D DrawSize = bStart ? FVector2D(1000.f, 563.f) : FVector2D(800.f, 240.f);
		const float DesignWidth = bStart ? 1000.f : (bCompletion ? 350.f : 435.f);
		const float DesignHeight = bStart ? 562.5f : (bCompletion ? 90.f : 70.f);
		const float PreviewScale = bStart ? 1.f : 1.5f;
		Banner->StartPresentation(true);
		TSharedRef<SWidget> BannerPreviewWidget = Banner->TakeWidget();
		if (!bStart)
		{
			BannerPreviewWidget = SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[BannerPreviewWidget];
		}
		TSharedRef<SWidget> Preview = SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[SNew(SBox).WidthOverride(DesignWidth * PreviewScale).HeightOverride(DesignHeight * PreviewScale)
				[BannerPreviewWidget]];
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
		if (!TestTrue(TEXT("Render target exists"), Target.IsValid())) return false;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Preview, 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		FImage Capture;
		if (!TestTrue(TEXT("Authored banner pixels read back"), FImageUtils::GetRenderTargetImage(Target.Get(), Capture))) return false;
		if (!TestTrue(TEXT("Expected capture format"), Capture.Format == ERawImageFormat::BGRA8)) return false;
		Capture.GammaSpace = EGammaSpace::sRGB;
		int32 OpaquePixels = 0;
		for (const FColor& Pixel : Capture.AsBGRA8()) if (Pixel.A > 200) ++OpaquePixels;
		TestTrue(TEXT("Authored WBP visibly renders"), OpaquePixels > 20000);
		TArray64<uint8> Bytes;
		const TCHAR* VariantName[] = { TEXT("white-base"), TEXT("tint-and-text"), TEXT("start") };
		const FString Filename = OutputDirectory / FString::Printf(TEXT("%s.png"), VariantName[Variant]);
		TestTrue(TEXT("Actual WBP frame exported"), FImageUtils::CompressImage(Bytes, TEXT("png"), Capture)
			&& FFileHelper::SaveArrayToFile(Bytes, *Filename));
	}
	return true;
}
#endif
