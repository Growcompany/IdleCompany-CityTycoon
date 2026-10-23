#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Enum/WidgetType.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Tests/GachaRevealTestTables.h"
#include "UI/Element/Gacha/GachaRevealCardWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

// 시안 대조용 캡처 ― Tools/GachaBoxReveal/capture_compare.py card prep 이 쓴 fixture.json 대로 카드를 그린다.
// ⚠ 실제 RHI 가 있는 에디터에서만 판정 가능(-nullrhi = 전부 0, 메모리 reference_nullrhi_widgetrenderer_zero) ― NonNullRHI 로 -NullRHI 실행에서 빠진다.
// 익명 ns 금지 ― 유니티 TU 병합 시 다른 캡처 테스트와 충돌
namespace GachaRevealCardRenderCapture
{
	static FString Directory()
	{
		return FPaths::ProjectSavedDir() / TEXT("GachaBoxReveal/card_compare");
	}

	struct FCase
	{
		FString Id;
		FGachaRevealCardData Data;
		EGachaCardSize Size = EGachaCardSize::Ten;
		bool bFaceUp = true;
		bool bHold = false;
		FVector2D Card;
		FIntPoint Crop;
		FVector2D Offset;
	};

	static bool ParseCase(const TSharedPtr<FJsonObject>& J, FCase& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* CardSize = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* CropBox = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Offset = nullptr;
		if (!J || !J->TryGetArrayField(TEXT("card"), CardSize) || !J->TryGetArrayField(TEXT("crop"), CropBox)
			|| !J->TryGetArrayField(TEXT("offset"), Offset) || CardSize->Num() != 2 || CropBox->Num() != 4 || Offset->Num() != 2)
		{
			return false;
		}
		Out.Id = J->GetStringField(TEXT("id"));
		const FString Kind = J->GetStringField(TEXT("kind"));
		Out.Data.Kind = Kind == TEXT("Trait") ? EGachaRevealKind::Trait : Kind == TEXT("Skin") ? EGachaRevealKind::Skin : EGachaRevealKind::Equipment;
		Out.Data.Grade = J->GetIntegerField(TEXT("grade"));
		Out.Data.Name = FText::FromString(J->GetStringField(TEXT("name")));
		Out.Data.EffectLine = FText::FromString(J->GetStringField(TEXT("effect")));
		Out.Data.bNew = J->GetBoolField(TEXT("new"));
		Out.Data.bRecommended = J->GetBoolField(TEXT("up"));
		Out.Data.MileageGained = J->GetIntegerField(TEXT("dup"));
		Out.Data.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(J->GetStringField(TEXT("icon"))));
		if (!Out.Data.Icon.IsNull())
		{
			Out.Data.IconUVRegion = FBox2d(FVector2D::ZeroVector, FVector2D(1.0, 1.0));
		}
		const FString Size = J->GetStringField(TEXT("size"));
		Out.Size = Size == TEXT("Thirty") ? EGachaCardSize::Thirty : Size == TEXT("Best") ? EGachaCardSize::Best
			: Size == TEXT("Single") ? EGachaCardSize::Single : Size == TEXT("Spot") ? EGachaCardSize::Spot : EGachaCardSize::Ten;
		Out.bFaceUp = J->GetBoolField(TEXT("face_up"));
		Out.bHold = J->GetBoolField(TEXT("hold"));
		Out.Card = FVector2D((*CardSize)[0]->AsNumber(), (*CardSize)[1]->AsNumber());
		Out.Crop = FIntPoint(static_cast<int32>((*CropBox)[2]->AsNumber()), static_cast<int32>((*CropBox)[3]->AsNumber()));
		Out.Offset = FVector2D((*Offset)[0]->AsNumber(), (*Offset)[1]->AsNumber());
		return true;
	}

	static bool Draw(const TSharedRef<SWidget>& CardSlate, const FCase& Case, const FLinearColor& Background, FImage& OutImage)
	{
		const FMargin Around(static_cast<float>(Case.Offset.X), static_cast<float>(Case.Offset.Y),
			static_cast<float>(Case.Crop.X - Case.Offset.X - Case.Card.X), static_cast<float>(Case.Crop.Y - Case.Offset.Y - Case.Card.Y));
		TSharedRef<SWidget> Root = SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Background)]
			+ SOverlay::Slot()[SNew(SBox).Padding(Around)[CardSlate]];
		FWidgetRenderer Renderer(true);
		// raw target(false) ― sRGB 타깃은 슬레이트 감마와 겹쳐 뿌옇게 나온다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Case.Crop.X, Case.Crop.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		// 시간 0 으로만 그린다 ― 순환광 각도·숨쉬기 밝기가 시안 정지 화면(애니 0초)과 같게
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Root, 1.f, FVector2D(Case.Crop.X, Case.Crop.Y), 0.f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	// 바탕색이 아닌 픽셀 수 ― 0 이면 「렌더 안 됨」(판정 불가)이지 통과가 아니다
	static int32 CountPainted(const FImage& Image, const FColor& Background)
	{
		int32 Count = 0;
		for (const FColor& Pixel : Image.AsBGRA8())
		{
			if (FMath::Abs(Pixel.R - Background.R) + FMath::Abs(Pixel.G - Background.G) + FMath::Abs(Pixel.B - Background.B) > 12)
			{
				++Count;
			}
		}
		return Count;
	}

	static bool SavePNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealCardCaptureTest,
	"CGR.Gacha.Reveal.Card.Capture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FCGRGachaRevealCardCaptureTest::RunTest(const FString& Parameters)
{
	using namespace GachaRevealCardRenderCapture;

	FString FixtureText;
	if (!TestTrue(TEXT("fixture.json 읽기(capture_compare.py card prep 먼저)"),
		FFileHelper::LoadFileToString(FixtureText, *(Directory() / TEXT("fixture.json")))))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Fixture;
	if (!TestTrue(TEXT("fixture JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FixtureText), Fixture) && Fixture.IsValid()))
	{
		return false;
	}
	TArray<FCase> Cases;
	for (const TSharedPtr<FJsonValue>& Value : Fixture->GetArrayField(TEXT("cases")))
	{
		FCase Case;
		if (TestTrue(TEXT("fixture 항목"), ParseCase(Value->AsObject(), Case)))
		{
			Cases.Add(Case);
		}
	}
	const TSubclassOf<UUserWidget> Class = GachaRevealTestTables::RegisteredWidgetClass(EWidgetType::GachaRevealCard);
	if (!TestTrue(TEXT("카드 클래스(DT_WidgetClass)"), Class != nullptr && Cases.Num() > 0))
	{
		return false;
	}

	// Game 월드는 남의 월드 하위 시스템(EntityManager·WindowLightManager)을 깨워 시험에 Warning 을 남긴다 ― 둘 다 안 받는 미리보기 월드
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (GEngine)
	{
		GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
	}
	ON_SCOPE_EXIT
	{
		if (GEngine)
		{
			GEngine->DestroyWorldContext(World);
		}
		World->DestroyWorld(false);
	};

	struct FBuilt
	{
		TStrongObjectPtr<UGachaRevealCardWidget> Card;
		TSharedPtr<SWidget> Slate; // 반환값을 쥐고 있어야 그릴 때 다시 지어지지 않는다
	};
	TArray<FBuilt> Built;
	for (const FCase& Case : Cases)
	{
		FBuilt Entry;
		Entry.Card.Reset(CreateWidget<UGachaRevealCardWidget>(World, Class));
		if (!TestNotNull(*FString::Printf(TEXT("%s: 카드 생성"), *Case.Id), Entry.Card.Get()))
		{
			return false;
		}
		Entry.Card->Configure(Case.Data, Case.Size);
		Entry.Card->SetFaceUp(Case.bFaceUp);
		Entry.Card->SetHoldGlow(Case.bHold);
		Entry.Slate = Entry.Card->TakeWidget();
		Built.Add(MoveTemp(Entry));
	}

	// 처음 쓰는 UI 재질·폰트는 첫 그림에서 빠진다 ― 전부 한 번 그린 뒤 컴파일을 끝내고 본 촬영
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		FImage Discarded;
		Draw(Built[Index].Slate.ToSharedRef(), Cases[Index], FLinearColor::Black, Discarded);
	}
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	FlushRenderingCommands();

	IFileManager::Get().MakeDirectory(*Directory(), true);
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		const FCase& Case = Cases[Index];
		for (const bool bWhite : { false, true })
		{
			FImage Image;
			const bool bDrawn = Draw(Built[Index].Slate.ToSharedRef(), Case, bWhite ? FLinearColor::White : FLinearColor::Black, Image);
			if (!TestTrue(*FString::Printf(TEXT("%s: 그리기"), *Case.Id), bDrawn))
			{
				continue;
			}
			const int32 Painted = CountPainted(Image, bWhite ? FColor::White : FColor::Black);
			TestTrue(*FString::Printf(TEXT("%s: 카드가 그려짐(%d px)"), *Case.Id, Painted), Painted > 1000);
			const FString Path = Directory() / FString::Printf(TEXT("game_%s_%s.png"), *Case.Id, bWhite ? TEXT("w") : TEXT("k"));
			TestTrue(*FString::Printf(TEXT("%s: 저장"), *Path), SavePNG(Image, Path));
		}
	}
	return true;
}

#endif
