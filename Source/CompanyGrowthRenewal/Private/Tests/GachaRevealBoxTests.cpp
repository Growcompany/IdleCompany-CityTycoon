#include "Misc/AutomationTest.h"
#include "Components/Image.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ScopeExit.h"
#include "Tests/GachaRevealTestTables.h"
#include "UI/Element/Gacha/GachaRevealBoxWidget.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

struct FGachaRevealBoxTestAccessor
{
	static void UseTables(UGachaRevealBoxWidget& W, const UTableManagerSubsystem& Tables) { W.TablesOverride = &Tables; }
	static UImage* Closed(UGachaRevealBoxWidget& W) { return W.ClosedImage; }
	static UImage* Body(UGachaRevealBoxWidget& W) { return W.BodyImage; }
	static UImage* Glow(UGachaRevealBoxWidget& W) { return W.GlowImage; }
	static UImage* FlapBack(UGachaRevealBoxWidget& W) { return W.FlapBackImage; }
	static UImage* FlapFront(UGachaRevealBoxWidget& W) { return W.FlapFrontImage; }
	static UMaterialInstanceDynamic* GlowMaterial(UGachaRevealBoxWidget& W) { return W.GlowMID; }
};

// 익명 ns 금지 ― 유니티 TU 병합 시 다른 테스트의 같은 이름과 충돌
namespace CGRGachaRevealBoxTests
{
	using FAccess = FGachaRevealBoxTestAccessor;

	static constexpr EGachaBoxType BakedTypes[] = {
		EGachaBoxType::EquipmentNormal, EGachaBoxType::EquipmentAdvanced, EGachaBoxType::EquipmentPremium,
		EGachaBoxType::TraitNormal, EGachaBoxType::TraitAdvanced,
		EGachaBoxType::SkinNormal, EGachaBoxType::SkinAdvanced };

	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			// Game 월드는 남의 월드 하위 시스템(EntityManager·WindowLightManager)을 깨워 시험에 Warning 을 남긴다 ― 둘 다 안 받는 미리보기 월드
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
			}
		}
		~FTestWorld()
		{
			if (GEngine)
			{
				GEngine->DestroyWorldContext(World);
			}
			World->DestroyWorld(false);
		}
	};

	static FString ResourcePath(const UImage* Image)
	{
		const UObject* Resource = Image ? Image->GetBrush().GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}

	static FString PathOf(const TSoftObjectPtr<UTexture2D>& Texture)
	{
		return Texture.ToSoftObjectPath().ToString();
	}

	// UWidget::IsVisible() 는 Slate 위젯이 지어지기 전(TakeWidget 전)엔 늘 false ― 위젯 코드가 저장한 값을 Slate 와 같은 기준으로 본다
	static bool Shown(const UWidget* Widget)
	{
		if (!Widget)
		{
			return false;
		}
		const ESlateVisibility Visibility = Widget->GetVisibility();
		return Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden;
	}

	// 보임 단언은 보고서 index.json 에 [VIS] 줄을 하나씩 남긴다 ― GREEN 은 테스트 합계가 아니라 이 줄 수·PASS 로 대조한다
	static bool ExpectShown(FAutomationTestBase& Test, const FString& What, bool bHolds)
	{
		Test.AddInfo(FString::Printf(TEXT("[VIS] %s = %s"), *What, bHolds ? TEXT("PASS") : TEXT("FAIL")));
		return Test.TestTrue(*What, bHolds);
	}

	static int32 ShownLayers(UGachaRevealBoxWidget& W)
	{
		int32 Count = 0;
		for (const UImage* Image : { FAccess::Closed(W), FAccess::Body(W), FAccess::Glow(W), FAccess::FlapBack(W), FAccess::FlapFront(W) })
		{
			Count += Shown(Image) ? 1 : 0;
		}
		return Count;
	}

	static bool OnlyClosedShown(UGachaRevealBoxWidget& W)
	{
		return Shown(FAccess::Closed(W)) && ShownLayers(W) == 1;
	}

	static bool LayersShown(UGachaRevealBoxWidget& W)
	{
		return !Shown(FAccess::Closed(W)) && Shown(FAccess::Body(W)) && Shown(FAccess::FlapBack(W)) && Shown(FAccess::FlapFront(W));
	}

	static TSubclassOf<UUserWidget> BoxClass(const UTableManagerSubsystem& Tables)
	{
		// 공개 화면과 같은 경로(TableManager::GetWidgetClass ← DT_WidgetClass 의 WidgetType 행)
		return Tables.GetWidgetClass(EWidgetType::GachaRevealBox);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaBoxWidgetTest,
	"CGR.Gacha.Box.Widget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaBoxWidgetTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaRevealBoxTests;
	using namespace GachaRevealBoxLayout;

	TestEqual(TEXT("프레임 0.5 → 5"), FlapFrameIndex(0.5f), 5);
	TestEqual(TEXT("프레임 0.04 → 0"), FlapFrameIndex(0.04f), 0);
	TestEqual(TEXT("프레임 0.96 → 10"), FlapFrameIndex(0.96f), 10);
	TestEqual(TEXT("프레임 음수 → 0"), FlapFrameIndex(-1.f), 0);
	TestEqual(TEXT("프레임 1 초과 → 10"), FlapFrameIndex(2.f), 10);

	const TStrongObjectPtr<UTableManagerSubsystem> Tables = GachaRevealTestTables::MakeBoxTables();
	const TSubclassOf<UUserWidget> Class = BoxClass(*Tables);
	if (!TestNotNull(TEXT("DT_WidgetClass 에 GachaRevealBox 행(TableManager GetWidgetClass)"), Class.Get()))
	{
		return false;
	}
	if (!TestTrue(TEXT("행 클래스가 UGachaRevealBoxWidget"), Class->IsChildOf(UGachaRevealBoxWidget::StaticClass())))
	{
		return false;
	}

	FTestWorld Scope;
	TStrongObjectPtr<UGachaRevealBoxWidget> Box(CreateWidget<UGachaRevealBoxWidget>(Scope.World, Class));
	if (!TestNotNull(TEXT("상자 생성"), Box.Get()))
	{
		return false;
	}
	UGachaRevealBoxWidget& W = *Box.Get();
	FAccess::UseTables(W, *Tables);
	// 코드가 아직 아무것도 안 정했다 ― WBP 저작 기본값(5층 Collapsed)을 본다. Visible 로 저작되면 빈 브러시가 흰 판으로 그려진다
	ExpectShown(*this, TEXT("SetBoxType 전에는 아무 층도 안 보임"), !W.HasBoxVisual() && ShownLayers(W) == 0);

	for (const EGachaBoxType Type : BakedTypes)
	{
		const FString Label = UEnum::GetValueAsString(Type);
		const FGachaBoxVisualRow* Row = Tables->GetGachaBoxVisual(Type);
		if (!TestNotNull(*(Label + TEXT(" 행")), Row) || Row->FlapFront.Num() != FlapFrameCount || Row->FlapBack.Num() != FlapFrameCount)
		{
			AddError(Label + TEXT(": 행 없음 또는 날개 프레임 수가 11 이 아님"));
			continue;
		}
		W.SetBoxType(Type);
		TestTrue(*(Label + TEXT(" 그림 있음")), W.HasBoxVisual() && W.GetBoxType() == Type);
		TestTrue(*(Label + TEXT(" 층 크기 = LayerSize")), W.GetLayerSize().Equals(Row->LayerSize));
		ExpectShown(*this, Label + TEXT(" 쉬는 동안 닫힌 그림 한 장만"), OnlyClosedShown(W));

		// 4층 = 몸통·안쪽 빛·뒤 날개·앞 날개(+ 쉬는 동안의 닫힌 그림). 경로까지 본다 ― 빈 브러시는 흰 판으로 그려진다
		TestEqual(*(Label + TEXT(" 닫힌 그림")), ResourcePath(FAccess::Closed(W)), PathOf(Row->Closed));
		TestEqual(*(Label + TEXT(" 몸통")), ResourcePath(FAccess::Body(W)), PathOf(Row->Body));
		TestEqual(*(Label + TEXT(" 뒤 날개 0")), ResourcePath(FAccess::FlapBack(W)), PathOf(Row->FlapBack[0]));
		TestEqual(*(Label + TEXT(" 앞 날개 0")), ResourcePath(FAccess::FlapFront(W)), PathOf(Row->FlapFront[0]));
		UMaterialInstanceDynamic* Glow = FAccess::GlowMaterial(W);
		if (TestNotNull(*(Label + TEXT(" 안쪽 빛 머티리얼(M_UI_GachaBoxGlow)")), Glow))
		{
			const UTexture* GlowTexture = Glow->K2_GetTextureParameterValue(TEXT("Glow"));
			TestEqual(*(Label + TEXT(" 안쪽 빛 그림")), GlowTexture ? GlowTexture->GetPathName() : FString(), PathOf(Row->Glow));
			TestTrue(*(Label + TEXT(" 안쪽 빛 머티리얼 부모")), Glow->Parent && Glow->Parent->GetName() == TEXT("M_UI_GachaBoxGlow"));
		}

		W.SetLid(0.5f);
		TestEqual(*(Label + TEXT(" SetLid(0.5) → 프레임 5")), W.GetFlapFrame(), 5);
		TestEqual(*(Label + TEXT(" 앞 날개 프레임 5")), ResourcePath(FAccess::FlapFront(W)), PathOf(Row->FlapFront[5]));
		TestEqual(*(Label + TEXT(" 뒤 날개 프레임 5")), ResourcePath(FAccess::FlapBack(W)), PathOf(Row->FlapBack[5]));
		ExpectShown(*this, Label + TEXT(" 열리는 동안 층 그림(닫힌 그림 숨김)"), LayersShown(W));
		ExpectShown(*this, Label + TEXT(" 빛 세기 0 이면 안쪽 빛 숨김"), !Shown(FAccess::Glow(W)));

		const FVector2D L = W.GetSeamPoint(EGachaSeam::Left);
		const FVector2D R = W.GetSeamPoint(EGachaSeam::Right);
		const FVector2D T = W.GetSeamPoint(EGachaSeam::Top);
		TestTrue(*(Label + TEXT(" 이음매 왼쪽.X < 오른쪽.X")), L.X < R.X);
		TestTrue(*(Label + TEXT(" 위 표시점은 이음매 사이")), T.X > L.X && T.X < R.X);
		TestTrue(*(Label + TEXT(" 기준점 = 비율 × 층 크기")), W.GetSeamPoint(EGachaSeam::Origin).Equals(Row->BoxOrigin * Row->LayerSize, 1e-3));
		TestTrue(*(Label + TEXT(" 렌더 변형 축 = 기준점")), W.GetRenderTransformPivot().Equals(Row->BoxOrigin, 1e-6));

		W.SetLid(1.f);
		TestEqual(*(Label + TEXT(" SetLid(1) → 앞 날개 끝 프레임")), ResourcePath(FAccess::FlapFront(W)), PathOf(Row->FlapFront[10]));
		W.SetLid(2.f);
		TestEqual(*(Label + TEXT(" 1 넘으면 10 에 멈춤")), W.GetFlapFrame(), 10);
		W.SetLid(0.f);
		ExpectShown(*this, Label + TEXT(" SetLid(0) → 다시 닫힌 그림"), OnlyClosedShown(W));

		const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(226, 169, 59));
		W.SetInnerGlow(Gold, 0.6f);
		ExpectShown(*this, Label + TEXT(" 빛을 켜면 층 그림 + 안쪽 빛"), LayersShown(W) && Shown(FAccess::Glow(W)));
		if (Glow)
		{
			TestTrue(*(Label + TEXT(" 빛 색")), Glow->K2_GetVectorParameterValue(TEXT("GlowColor")).Equals(Gold, 1e-4f));
			TestTrue(*(Label + TEXT(" 빛 세기")), FMath::IsNearlyEqual(Glow->K2_GetScalarParameterValue(TEXT("Opacity")), 0.6f));
		}
		W.SetInnerGlow(Gold, 0.f);
		ExpectShown(*this, Label + TEXT(" 빛을 끄고 뚜껑 0 이면 닫힌 그림"), OnlyClosedShown(W));
	}

	// 같은 위젯이 앞 상자를 그리던 상태에서 행 없는 종류로 바뀌면 비워진다(경고는 TableManager 가 1번씩)
	UDataTable* EmptyVisualTable = NewObject<UDataTable>();
	EmptyVisualTable->RowStruct = FGachaBoxVisualRow::StaticStruct();
	W.SetBoxType(EGachaBoxType::EquipmentAdvanced);
	W.SetLid(0.5f);
	FGachaBoxVisualTableTestAccessor::Initialize(*Tables, EmptyVisualTable);
	AddExpectedError(TEXT("GachaBoxVisual"), EAutomationExpectedErrorFlags::Contains, 1);
	for (const EGachaBoxType Type : { EGachaBoxType::SkinNormal })
	{
		const FString Label = UEnum::GetValueAsString(Type);
		W.SetBoxType(Type);
		TestFalse(*(Label + TEXT(" 행 없음 → 그림 없음")), W.HasBoxVisual());
		// 바로 앞이 층 그림(뚜껑 0.5)이라 숨김이 실제로 일어나야 통과한다
		const int32 AfterSwitch = ShownLayers(W);
		ExpectShown(*this, FString::Printf(TEXT("%s 보이는 층 0(지금 %d)"), *Label, AfterSwitch), AfterSwitch == 0);
		TestTrue(*(Label + TEXT(" 이음매·크기 0")), W.GetSeamPoint(EGachaSeam::Left).IsZero() && W.GetLayerSize().IsZero());
		W.SetLid(0.5f);
		W.SetInnerGlow(FLinearColor::White, 1.f);
		const int32 AfterLidGlow = ShownLayers(W);
		ExpectShown(*this, FString::Printf(TEXT("%s 빈 상자에 뚜껑·빛을 줘도 아무것도 안 그림(지금 %d)"), *Label, AfterLidGlow), AfterLidGlow == 0);
		TestTrue(*(Label + TEXT(" 앞 상자 그림 안 남음")), ResourcePath(FAccess::Body(W)).IsEmpty() && ResourcePath(FAccess::FlapFront(W)).IsEmpty());
	}
	return true;
}

#if WITH_EDITOR

// 시안 대조용 캡처 ― Tools/GachaBoxReveal/capture_compare.py box prep 이 쓴 fixture.json 대로 상자를 그린다.
// ⚠ 실제 RHI 가 있는 에디터에서만 판정 가능(-nullrhi = 전부 0, 메모리 reference_nullrhi_widgetrenderer_zero) ― NonNullRHI 로 -NullRHI 실행에서 빠진다
namespace CGRGachaRevealBoxCapture
{
	static FString Directory()
	{
		return FPaths::ProjectSavedDir() / TEXT("GachaBoxReveal/box_compare");
	}

	struct FCase
	{
		FString Id;
		EGachaBoxType BoxType = EGachaBoxType::EquipmentNormal;
		float Lid = 0.f;
		FLinearColor GlowColor = FLinearColor::White;
		float GlowOpacity = 0.f;
		FVector2D Layer;
		FIntPoint Crop;
		FVector2D Offset;
	};

	static bool ParseCase(const TSharedPtr<FJsonObject>& J, FCase& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Layer = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* CropBox = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Offset = nullptr;
		if (!J || !J->TryGetArrayField(TEXT("layer"), Layer) || !J->TryGetArrayField(TEXT("crop"), CropBox)
			|| !J->TryGetArrayField(TEXT("offset"), Offset) || Layer->Num() != 2 || CropBox->Num() != 4 || Offset->Num() != 2)
		{
			return false;
		}
		const int64 TypeValue = StaticEnum<EGachaBoxType>()->GetValueByNameString(J->GetStringField(TEXT("box")));
		if (TypeValue == INDEX_NONE)
		{
			return false;
		}
		Out.Id = J->GetStringField(TEXT("id"));
		Out.BoxType = static_cast<EGachaBoxType>(TypeValue);
		Out.Lid = static_cast<float>(J->GetNumberField(TEXT("lid")));
		const FString Glow = J->GetStringField(TEXT("glow"));
		Out.GlowColor = Glow.IsEmpty() ? FLinearColor::White : FLinearColor::FromSRGBColor(FColor::FromHex(Glow));
		Out.GlowOpacity = static_cast<float>(J->GetNumberField(TEXT("glow_opacity")));
		Out.Layer = FVector2D((*Layer)[0]->AsNumber(), (*Layer)[1]->AsNumber());
		Out.Crop = FIntPoint(static_cast<int32>((*CropBox)[2]->AsNumber()), static_cast<int32>((*CropBox)[3]->AsNumber()));
		Out.Offset = FVector2D((*Offset)[0]->AsNumber(), (*Offset)[1]->AsNumber());
		return true;
	}

	static bool Draw(const TSharedRef<SWidget>& BoxSlate, const FCase& Case, const FLinearColor& Background, FImage& OutImage)
	{
		const FMargin Around(static_cast<float>(Case.Offset.X), static_cast<float>(Case.Offset.Y),
			static_cast<float>(Case.Crop.X - Case.Offset.X - Case.Layer.X), static_cast<float>(Case.Crop.Y - Case.Offset.Y - Case.Layer.Y));
		TSharedRef<SWidget> Root = SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Background)]
			+ SOverlay::Slot()[SNew(SBox).Padding(Around)[BoxSlate]];
		FWidgetRenderer Renderer(true);
		// raw target(false) ― sRGB 타깃은 슬레이트 감마와 겹쳐 뿌옇게 나온다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Case.Crop.X, Case.Crop.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
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
	FCGRGachaBoxCaptureTest,
	"CGR.Gacha.Box.Capture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FCGRGachaBoxCaptureTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaRevealBoxCapture;

	FString FixtureText;
	if (!TestTrue(TEXT("fixture.json 읽기(capture_compare.py box prep 먼저)"),
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
	const TStrongObjectPtr<UTableManagerSubsystem> Tables = GachaRevealTestTables::MakeBoxTables();
	const TSubclassOf<UUserWidget> Class = CGRGachaRevealBoxTests::BoxClass(*Tables);
	if (!TestTrue(TEXT("상자 클래스(DT_WidgetClass)"), Class.Get() != nullptr && Cases.Num() > 0))
	{
		return false;
	}

	CGRGachaRevealBoxTests::FTestWorld Scope;
	struct FBuilt
	{
		TStrongObjectPtr<UGachaRevealBoxWidget> Box;
		TSharedPtr<SWidget> Slate; // 반환값을 쥐고 있어야 그릴 때 다시 지어지지 않는다
	};
	TArray<FBuilt> Built;
	for (const FCase& Case : Cases)
	{
		FBuilt Entry;
		Entry.Box.Reset(CreateWidget<UGachaRevealBoxWidget>(Scope.World, Class));
		if (!TestNotNull(*FString::Printf(TEXT("%s: 상자 생성"), *Case.Id), Entry.Box.Get()))
		{
			return false;
		}
		FGachaRevealBoxTestAccessor::UseTables(*Entry.Box, *Tables);
		Entry.Box->SetBoxType(Case.BoxType);
		Entry.Box->SetLid(Case.Lid);
		Entry.Box->SetInnerGlow(Case.GlowColor, Case.GlowOpacity);
		TestTrue(*FString::Printf(TEXT("%s: 층 크기 = fixture"), *Case.Id), Entry.Box->GetLayerSize().Equals(Case.Layer));
		Entry.Slate = Entry.Box->TakeWidget();
		Built.Add(MoveTemp(Entry));
	}

	// 처음 쓰는 UI 재질·막 들인 텍스처는 첫 그림에서 빠진다 ― 전부 한 번 그린 뒤 컴파일을 끝내고 본 촬영
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		FImage Discarded;
		Draw(Built[Index].Slate.ToSharedRef(), Cases[Index], FLinearColor::Black, Discarded);
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
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
			TestTrue(*FString::Printf(TEXT("%s: 상자가 그려짐(%d px)"), *Case.Id, Painted), Painted > 10000);
			const FString Path = Directory() / FString::Printf(TEXT("game_%s_%s.png"), *Case.Id, bWhite ? TEXT("w") : TEXT("k"));
			TestTrue(*FString::Printf(TEXT("%s: 저장"), *Path), SavePNG(Image, Path));
		}
	}
	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
