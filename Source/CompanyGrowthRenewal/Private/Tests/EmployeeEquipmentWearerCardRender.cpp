#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"

#include "CommonInputSettings.h"
#include "CommonTextBlock.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ICommonInputModule.h"
#include "ImageUtils.h"
#include "LevelEditorViewport.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌
namespace EquipmentWearerCard
{
	static const TCHAR* CardPath = TEXT("/Game/CompanyGrowth/UI/Elements/Employee/UIE_EmployeeEquipmentCard.UIE_EmployeeEquipmentCard_C");
	static const TCHAR* IconPath = TEXT("/Game/CompanyGrowth/UI/Textures/Equipment/Parts/T_SK_Backpack_001.T_SK_Backpack_001");
	static constexpr double CardSize = 256.0;
	static constexpr double Gap = 24.0;
	// 목업 시안 B 의 띠 = 카드 높이의 약 22% ― 너무 얇으면 안 읽히고 두꺼우면 그림을 가린다
	static constexpr double BandMinRatio = 0.16, BandMaxRatio = 0.28;
	// 띠 글자와 강화 숫자·자물쇠 사이 최소 틈 ― 띄어쓰기(약 10)보다 넓어야 「+15 착용 중」이 한 문장으로 안 읽힌다
	static constexpr double LabelClearance = 12.0;
	// 「+N」과 자물쇠가 둘 다 있으면 띠 글자는 그 사이 가운데 ― 양쪽 틈 차이 한도
	static constexpr double GapBalance = 6.0;

	struct FBox2DRect
	{
		bool bVisible = false;
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		double Width() const { return Max.X - Min.X; }
		double Height() const { return Max.Y - Min.Y; }
	};

	// 글자는 슬롯이 아니라 잉크 폭(desired)으로 잰다 ― 고정 슬롯 폭으로 재면 빈 자리까지 겹침으로 읽힌다.
	// 그리기 배율(RenderScale)까지 반영하려고 칠하는 공간 기하로 재고, 접힌 부모 안의 위젯은 안 보임으로 친다
	static FBox2DRect Measure(const UWidget* Widget, const FVector2D& Origin, bool bUseDesired)
	{
		FBox2DRect Out;
		for (const UWidget* Node = Widget; Node; Node = Node->GetParent())
		{
			if (Node->GetVisibility() == ESlateVisibility::Collapsed || Node->GetVisibility() == ESlateVisibility::Hidden)
			{
				return Out;
			}
		}
		if (!Widget)
		{
			return Out;
		}
		const FGeometry Geometry = Widget->GetPaintSpaceGeometry();
		const FVector2D LocalSize = bUseDesired ? Widget->GetDesiredSize() : FVector2D(Geometry.GetLocalSize());
		Out.bVisible = true;
		Out.Min = FVector2D(Geometry.LocalToAbsolute(FVector2D::ZeroVector)) - Origin;
		Out.Max = FVector2D(Geometry.LocalToAbsolute(LocalSize)) - Origin;
		return Out;
	}

	// 가로나 세로 중 한쪽으로라도 떨어져 있으면 안 겹친다 ― 큰 쪽 틈을 돌려준다
	static double Separation(const FBox2DRect& A, const FBox2DRect& B)
	{
		const double Horizontal = FMath::Max(B.Min.X - A.Max.X, A.Min.X - B.Max.X);
		const double Vertical = FMath::Max(B.Min.Y - A.Max.Y, A.Min.Y - B.Max.Y);
		return FMath::Max(Horizontal, Vertical);
	}

	static TSharedRef<FJsonObject> ToJson(const FBox2DRect& Rect)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("visible"), Rect.bVisible);
		Json->SetArrayField(TEXT("rect"), {
			MakeShared<FJsonValueNumber>(Rect.Min.X), MakeShared<FJsonValueNumber>(Rect.Min.Y),
			MakeShared<FJsonValueNumber>(Rect.Max.X), MakeShared<FJsonValueNumber>(Rect.Max.Y) });
		return Json;
	}

	static int32 ForceViewportDraw()
	{
		int32 Drawn = 0;
		for (FLevelEditorViewportClient* ViewportClient : GEditor ? GEditor->GetLevelViewportClients() : TArray<FLevelEditorViewportClient*>())
		{
			if (ViewportClient && ViewportClient->Viewport)
			{
				ViewportClient->Viewport->Draw(true);
				++Drawn;
			}
		}
		FlushRenderingCommands();
		return Drawn;
	}

	static bool SavePNG(UTextureRenderTarget2D* Target, const FString& Path, FImage& OutImage)
	{
		if (!FImageUtils::GetRenderTargetImage(Target, OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		// FWidgetRenderer(true) 가 감마 바이트를 linear 표식 타깃에 썼다 ― 다시 감마를 걸면 물빠진다
		OutImage.GammaSpace = EGammaSpace::sRGB;
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), OutImage) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentWearerCardRenderTest, "CGR.EquipmentUI.WearerCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEquipmentWearerCardRenderTest::RunTest(const FString& Parameters)
{
	using namespace EquipmentWearerCard;
	ICommonInputModule::GetSettings().LoadData();

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* CardClass = LoadClass<UEmployeeEquipmentCardWidget>(nullptr, CardPath);
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, IconPath);
	if (!TestNotNull(TEXT("에디터 월드"), EditorWorld) || !TestNotNull(TEXT("UIE_EmployeeEquipmentCard"), CardClass))
	{
		return false;
	}
	TestNotNull(TEXT("카드 아이콘 텍스처"), Icon);
	const bool bViewportDrawn = ForceViewportDraw() > 0;
	if (!bViewportDrawn)
	{
		AddWarning(TEXT("그려진 레벨 뷰포트가 없어 머티리얼 면은 사진에 안 나옵니다 ― 기하 판정만 유효합니다"));
	}

	struct FFixture
	{
		const TCHAR* Name;
		bool bEquipped;
		bool bWornByOther;
		int32 Enhancement;
		bool bLocked;
	};
	// 둘째 카드가 스트레스 픽스처 ― 띠 위에 강화 두 자리 숫자와 자물쇠가 함께 올라간다
	const FFixture Fixtures[] = {
		{ TEXT("mine"), true, false, 15, true },
		{ TEXT("other_stress"), true, true, 15, true },
		{ TEXT("other"), true, true, 0, false },
		{ TEXT("free"), false, false, 3, false } };

	TArray<TStrongObjectPtr<UEmployeeEquipmentCardWidget>> Cards;
	TArray<TStrongObjectPtr<UEmployeeEquipmentItemObject>> Items;
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (const FFixture& Fixture : Fixtures)
	{
		UEmployeeEquipmentCardWidget* Card = CreateWidget<UEmployeeEquipmentCardWidget>(EditorWorld, CardClass);
		UEmployeeEquipmentItemObject* Item = NewObject<UEmployeeEquipmentItemObject>();
		Item->InstanceID = FGuid::NewGuid();
		Item->DefinitionID = FName(Fixture.Name);
		// 에디터 월드엔 표 서브시스템이 없어 등급 행 대신 이 색으로 면·테두리를 칠한다 (DT_EmployeeEquipmentGrade 의 B 등급 색)
		Item->Grade = EEmployeeEquipmentGrade::Uncommon;
		Item->VisualGrade = EEmployeeEquipmentGrade::Uncommon;
		Item->GradeColor = FLinearColor(0.24f, 0.68f, 0.38f);
		Item->Icon = Icon;
		Item->bEquipped = Fixture.bEquipped;
		Item->bWornByOther = Fixture.bWornByOther;
		Item->EnhancementLevel = Fixture.Enhancement;
		Item->bLocked = Fixture.bLocked;
		Cards.Emplace(Card);
		Items.Emplace(Item);
		Row->AddSlot().AutoWidth().Padding(Gap, Gap, 0.0, Gap)
		[
			SNew(SBox).WidthOverride(CardSize).HeightOverride(CardSize)[Card->TakeWidget()]
		];
		Card->SetItemData(Item);
	}
	const FVector2D DrawSize(4.0 * CardSize + 5.0 * Gap, CardSize + 2.0 * Gap);
	TSharedRef<SWidget> Surface = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
		.BorderBackgroundColor(FLinearColor(0.855f, 0.879f, 0.913f))
		.Padding(0.f)
		[Row];

	FWidgetRenderer Renderer(true);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
	Renderer.DrawWidget(Target.Get(), Surface, DrawSize, 0.f);
	// 첫 그리기가 셰이더 컴파일을 깨운다 ― 끝난 뒤에 다시 그려야 면이 나온다
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		Renderer.DrawWidget(Target.Get(), Surface, DrawSize, 0.f);
	}
	FlushRenderingCommands();

	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("EquipmentUIValidation/wearer_card"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	FImage Image;
	TestTrue(TEXT("카드 사진 저장"), SavePNG(Target.Get(), Directory / TEXT("wearer_cards.png"), Image));

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("viewport_drawn"), bViewportDrawn);
	TMap<FString, double> LockBottoms;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const FFixture& Fixture = Fixtures[Index];
		UEmployeeEquipmentCardWidget& Card = *Cards[Index].Get();
		const FVector2D Origin(Gap + Index * (CardSize + Gap), Gap);
		const FBox2DRect Band = Measure(Card.GetWidgetFromName(TEXT("WearerBand")), Origin, false);
		const FBox2DRect Label = Measure(Card.GetWidgetFromName(TEXT("WearerText")), Origin, false);
		const FBox2DRect Badge = Measure(Card.GetWidgetFromName(TEXT("EquippedBadge")), Origin, false);
		const FBox2DRect Enhance = Measure(Card.GetWidgetFromName(TEXT("EnhanceText")), Origin, true);
		const FBox2DRect Lock = Measure(Card.GetWidgetFromName(TEXT("LockMark")), Origin, false);
		const FString Tag = Fixture.Name;

		const bool bExpectBand = Fixture.bEquipped && Fixture.bWornByOther;
		const bool bExpectBadge = Fixture.bEquipped && !Fixture.bWornByOther;
		TestEqual(Tag + TEXT(": 아래 띠 표시"), Band.bVisible, bExpectBand);
		TestEqual(Tag + TEXT(": 파란 장착 배지 표시"), Badge.bVisible, bExpectBadge);
		if (bExpectBand)
		{
			const UCommonTextBlock* LabelText = Cast<UCommonTextBlock>(Card.GetWidgetFromName(TEXT("WearerText")));
			TestTrue(Tag + TEXT(": 띠 문구가 비어 있지 않다"), LabelText && !LabelText->GetText().IsEmpty());
			TestTrue(Tag + FString::Printf(TEXT(": 띠가 카드 안에 있다 (%.1f,%.1f)-(%.1f,%.1f)"), Band.Min.X, Band.Min.Y, Band.Max.X, Band.Max.Y),
				Band.Min.X >= -0.5 && Band.Max.X <= CardSize + 0.5 && Band.Max.Y <= CardSize + 0.5);
			const double Ratio = Band.Height() / CardSize;
			TestTrue(Tag + FString::Printf(TEXT(": 띠 높이 비율 %.3f (%.2f~%.2f)"), Ratio, BandMinRatio, BandMaxRatio),
				Ratio >= BandMinRatio && Ratio <= BandMaxRatio);
			TestTrue(Tag + TEXT(": 띠 글자가 띠 안에 있다"), Label.bVisible
				&& Label.Min.X >= Band.Min.X - 0.5 && Label.Max.X <= Band.Max.X + 0.5
				&& Label.Min.Y >= Band.Min.Y - 0.5 && Label.Max.Y <= Band.Max.Y + 0.5);
			// 강화 숫자·자물쇠는 다른 카드와 같은 바닥 줄에 남아 띠 양 끝 빈자리에 앉는다 ― 띠 글자와는 안 겹친다
			for (const TPair<const TCHAR*, const FBox2DRect*>& Mark : { TPair<const TCHAR*, const FBox2DRect*>(TEXT("강화 숫자"), &Enhance),
				TPair<const TCHAR*, const FBox2DRect*>(TEXT("자물쇠"), &Lock) })
			{
				if (!Mark.Value->bVisible) continue;
				const double Clear = Separation(Label, *Mark.Value);
				TestTrue(Tag + FString::Printf(TEXT(": 띠 글자-%s 틈 %.1f (≥%.0f)"), Mark.Key, Clear, LabelClearance), Clear >= LabelClearance);
				const double CenterY = (Mark.Value->Min.Y + Mark.Value->Max.Y) * 0.5;
				TestTrue(Tag + FString::Printf(TEXT(": %s 세로 가운데 %.1f 가 띠 안(%.1f~%.1f)"), Mark.Key, CenterY, Band.Min.Y, Band.Max.Y),
					CenterY >= Band.Min.Y && CenterY <= Band.Max.Y);
			}
			if (Enhance.bVisible && Lock.bVisible)
			{
				const double LeftGap = Label.Min.X - Enhance.Max.X, RightGap = Lock.Min.X - Label.Max.X;
				TestTrue(Tag + FString::Printf(TEXT(": 띠 글자 양쪽 틈 %.1f / %.1f (차이 ≤%.0f)"), LeftGap, RightGap, GapBalance),
					FMath::Abs(LeftGap - RightGap) <= GapBalance);
			}
			// 띠 왼쪽 끝 면을 찍어 실제로 어둡게 그려졌는지 본다 (머티리얼 면이 있는 문맥에서만)
			if (bViewportDrawn && Image.RawData.Num() > 0 && !Enhance.bVisible)
			{
				const int32 X = FMath::RoundToInt32(Origin.X + Band.Min.X + 20.0);
				const int32 Y = FMath::RoundToInt32(Origin.Y + (Band.Min.Y + Band.Max.Y) * 0.5);
				const FColor Pixel = Image.AsBGRA8()[Y * Image.SizeX + X];
				const int32 Luma = (Pixel.R * 299 + Pixel.G * 587 + Pixel.B * 114) / 1000;
				TestTrue(Tag + FString::Printf(TEXT(": 띠 면 밝기 %d (< 110)"), Luma), Luma < 110);
			}
		}
		else
		{
			// 띠가 없으면 강화 숫자·자물쇠는 원래대로 카드 바닥 쪽에 남는다
			for (const FBox2DRect* Mark : { &Enhance, &Lock })
			{
				if (!Mark->bVisible) continue;
				TestTrue(Tag + FString::Printf(TEXT(": 띠 없을 때 표식 아래 %.1f 가 바닥 근처(≥%.0f, ≤%.0f)"), Mark->Max.Y, CardSize - 40.0, CardSize),
					Mark->Max.Y >= CardSize - 40.0 && Mark->Max.Y <= CardSize);
			}
		}
		if (bExpectBadge)
		{
			// 목업 배지 = 카드 폭의 약 31%·높이 17% ― 띠 글자보다 커지면 크기 순서가 뒤집힌다
			TestTrue(Tag + FString::Printf(TEXT(": 장착 배지 %.0fx%.0f 가 폭 34%%·높이 20%% 이하"), Badge.Width(), Badge.Height()),
				Badge.Width() <= CardSize * 0.34 && Badge.Height() <= CardSize * 0.20);
		}
		LockBottoms.Add(Tag, Lock.bVisible ? Lock.Max.Y : -1.0);
		TSharedRef<FJsonObject> CardJson = MakeShared<FJsonObject>();
		CardJson->SetObjectField(TEXT("band"), ToJson(Band));
		CardJson->SetObjectField(TEXT("label"), ToJson(Label));
		CardJson->SetObjectField(TEXT("badge"), ToJson(Badge));
		CardJson->SetObjectField(TEXT("enhance_ink"), ToJson(Enhance));
		CardJson->SetObjectField(TEXT("lock"), ToJson(Lock));
		Report->SetObjectField(Tag, CardJson);
	}
	// 띠가 있어도 바닥 줄 높이는 같다 ― 목록에 섞였을 때 자물쇠가 들쭉날쭉하면 안 된다
	const double MineLock = LockBottoms.FindRef(TEXT("mine")), StressLock = LockBottoms.FindRef(TEXT("other_stress"));
	TestTrue(FString::Printf(TEXT("자물쇠 아래 끝이 띠 유무와 무관하게 같다 (%.1f / %.1f)"), MineLock, StressLock),
		MineLock > 0.0 && FMath::IsNearlyEqual(MineLock, StressLock, 1.0));
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Report, Writer);
	FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("wearer_cards.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	for (TStrongObjectPtr<UEmployeeEquipmentCardWidget>& Card : Cards)
	{
		Card->RemoveFromParent();
	}
	return true;
}

#endif
