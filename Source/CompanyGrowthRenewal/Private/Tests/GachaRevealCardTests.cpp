#include "Misc/AutomationTest.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Enum/WidgetType.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Tests/GachaRevealTestTables.h"
#include "UI/Element/Gacha/GachaRevealCardWidget.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FGachaRevealCardTestAccessor
{
	static UCanvasPanel* Face(UGachaRevealCardWidget& W) { return W.FaceRoot; }
	static UCanvasPanel* Back(UGachaRevealCardWidget& W) { return W.BackRoot; }
	static UCanvasPanel* Flip(UGachaRevealCardWidget& W) { return W.FlipRoot; }
	static UImage* NewTag(UGachaRevealCardWidget& W) { return W.NewTag; }
	static UImage* UpTag(UGachaRevealCardWidget& W) { return W.UpTag; }
	static UImage* FaceImage(UGachaRevealCardWidget& W) { return W.FaceImage; }
	static UImage* IconImage(UGachaRevealCardWidget& W) { return W.IconImage; }
	static UImage* IconShadow(UGachaRevealCardWidget& W) { return W.IconShadow; }
	static UImage* BackHot(UGachaRevealCardWidget& W) { return W.BackHotImage; }
	static bool Holding(UGachaRevealCardWidget& W) { return W.bHold; }
	static UImage* Orb(UGachaRevealCardWidget& W) { return W.OrbImage; }
	static UTextBlock* Dup(UGachaRevealCardWidget& W) { return W.DupText; }
	static UTextBlock* Name(UGachaRevealCardWidget& W) { return W.NameText; }
	static UTextBlock* Value(UGachaRevealCardWidget& W) { return W.ValueText; }
};

namespace CGRGachaRevealCardTests
{
	using namespace GachaRevealCardLayout;
	using FAccess = FGachaRevealCardTestAccessor;

	static const TCHAR* IconPath = TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_EquipHeader.T_UIIcon_EquipHeader");

	static constexpr EGachaCardSize AllSizes[] = {
		EGachaCardSize::Single, EGachaCardSize::Ten, EGachaCardSize::Thirty, EGachaCardSize::Best, EGachaCardSize::Spot };

	static FString ResourceName(const UImage* Image)
	{
		const UObject* Resource = Image ? Image->GetBrush().GetResourceObject() : nullptr;
		return Resource ? Resource->GetName() : FString();
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

	static FGachaRevealCardData EquipmentSSCard()
	{
		FGachaRevealCardData Data;
		Data.Kind = EGachaRevealKind::Equipment;
		Data.Grade = 4;
		Data.GradeText = FText::FromString(TEXT("SS"));
		Data.Name = FText::FromString(TEXT("시험용 장비 이름"));
		Data.EffectLine = FText::FromString(TEXT("기획 +12%"));
		Data.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(IconPath));
		Data.IconUVRegion = FBox2d(FVector2D(0.25, 0.10), FVector2D(0.75, 0.90));
		Data.bNew = true;
		Data.bRecommended = true;
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealCardLayoutTest,
	"CGR.Gacha.Reveal.Card.Layout",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealCardLayoutTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaRevealCardTests;

	TestTrue(TEXT("10회 묶음 = 290×348"), DesignSize(EGachaCardSize::Ten).Equals(FVector2D(290.0, 348.0)));
	TestTrue(TEXT("30회 묶음 = 224"), DesignSize(EGachaCardSize::Thirty).Equals(FVector2D(224.0, 224.0)));
	TestTrue(TEXT("이번 최고 묶음 = 378"), DesignSize(EGachaCardSize::Best).Equals(FVector2D(378.0, 378.0)));
	TestTrue(TEXT("1회 묶음 = 460"), DesignSize(EGachaCardSize::Single).Equals(FVector2D(460.0, 460.0)));
	TestTrue(TEXT("단독 장면 묶음 = 560"), DesignSize(EGachaCardSize::Spot).Equals(FVector2D(560.0, 560.0)));
	TestTrue(TEXT("Best·Spot·Single 은 sz1 규칙"), UsesSingleRules(EGachaCardSize::Best) && UsesSingleRules(EGachaCardSize::Spot)
		&& UsesSingleRules(EGachaCardSize::Single) && !UsesSingleRules(EGachaCardSize::Ten) && !UsesSingleRules(EGachaCardSize::Thirty));

	TestEqual(TEXT("앞면 경로"), SurfacePath(ECardPart::Face, EGachaRevealKind::Equipment, 4, EGachaCardSize::Ten).ToString(),
		FString(TEXT("/Game/CompanyGrowth/UI/Textures/GachaBox/Card/T_GachaCard_Face_Equip_G4_Ten.T_GachaCard_Face_Equip_G4_Ten")));
	TestEqual(TEXT("스킨은 특성 그림"), SurfacePath(ECardPart::Badge, EGachaRevealKind::Skin, 2, EGachaCardSize::Thirty).GetAssetName(),
		FString(TEXT("T_GachaCard_Badge_Trait_G2_Thirty")));
	TestTrue(TEXT("빛 테는 등급 3 이상만"), SurfacePath(ECardPart::BackHot, EGachaRevealKind::Equipment, 2, EGachaCardSize::Ten).IsNull());
	TestEqual(TEXT("Best 묶음 앞면"), SurfacePath(ECardPart::Face, EGachaRevealKind::Trait, 5, EGachaCardSize::Best).GetAssetName(),
		FString(TEXT("T_GachaCard_Face_Trait_G5_Best")));
	TestEqual(TEXT("Spot 묶음 빛 테"), SurfacePath(ECardPart::BackHot, EGachaRevealKind::Equipment, 3, EGachaCardSize::Spot).GetAssetName(),
		FString(TEXT("T_GachaCard_BackHot_Equip_G3_Spot")));
	TestTrue(TEXT("범위 밖 등급은 빈 경로"), SurfacePath(ECardPart::Face, EGachaRevealKind::Equipment, 6, EGachaCardSize::Ten).IsNull());

	const FGachaRevealRect Thirty = IconBox(EGachaCardSize::Thirty);
	TestTrue(TEXT("30회 아이콘 = 72%"), FMath::IsNearlyEqual(Thirty.W, 0.72f * 224.f) && FMath::IsNearlyEqual(Thirty.X, 0.14f * 224.f));
	const FGachaRevealRect Ten = IconBox(EGachaCardSize::Ten);
	TestTrue(TEXT("10회 아이콘 = 64%×58%"), FMath::IsNearlyEqual(Ten.W, 0.64f * 290.f) && FMath::IsNearlyEqual(Ten.H, 0.58f * 348.f)
		&& FMath::IsNearlyEqual(Ten.Y, 0.06f * 348.f));
	const FGachaRevealRect Spot = IconBox(EGachaCardSize::Spot);
	TestTrue(TEXT("sz1 아이콘 = 80%"), FMath::IsNearlyEqual(Spot.W, 448.f) && FMath::IsNearlyEqual(Spot.X, 56.f));
	const FGachaRevealRect Single = IconBox(EGachaCardSize::Single);
	TestTrue(TEXT("1회 아이콘 = 56% 중앙"), FMath::IsNearlyEqual(Single.W, 257.6f) && FMath::IsNearlyEqual(Single.H, 257.6f)
		&& FMath::IsNearlyEqual(Single.X, 101.2f) && FMath::IsNearlyEqual(Single.Y, 101.2f));

	const FGachaRevealRect Fit = FitContain({ 0.f, 0.f, 100.f, 100.f }, FVector2D(200.0, 100.0));
	TestTrue(TEXT("contain = 가운데 맞춤"), FMath::IsNearlyEqual(Fit.Y, 25.f) && FMath::IsNearlyEqual(Fit.W, 100.f) && FMath::IsNearlyEqual(Fit.H, 50.f));

	TestTrue(TEXT("뒤집기 0.25 → 0.707"), FMath::IsNearlyEqual(FlipScaleX(0.25f), 0.70710678f, 1e-4f));
	TestTrue(TEXT("뒤집기 0.5 → 0"), FMath::IsNearlyZero(FlipScaleX(0.5f), 1e-4f));
	TestTrue(TEXT("뒤집기 배율 ≤ 1"), FlipScaleX(0.f) <= 1.f && FlipScaleX(1.f) <= 1.f);

	TestTrue(TEXT("숨쉬기 시작 .45"), FMath::IsNearlyEqual(BreatheOpacity(0.f), 0.45f, 1e-3f));
	TestTrue(TEXT("숨쉬기 0.45초 = 1"), FMath::IsNearlyEqual(BreatheOpacity(0.45f), 1.f, 1e-3f));
	TestTrue(TEXT("숨쉬기 주기 0.9초"), FMath::IsNearlyEqual(BreatheOpacity(0.9f), 0.45f, 1e-3f));
	TestTrue(TEXT("ease-in-out 가운데 .5"), FMath::IsNearlyEqual(CubicBezier(0.42f, 0.f, 0.58f, 1.f, 0.5f), 0.5f, 1e-4f));
	TestTrue(TEXT("ease-out 은 앞이 빠르다"), CubicBezier(0.f, 0.f, 0.58f, 1.f, 0.3f) > 0.3f);

	// 꼬리표 상자 = 시안 --w 비례(NEW 글자 .1w 높이, ▲ .13w×.12w)
	for (const EGachaCardSize Size : AllSizes)
	{
		const float W = static_cast<float>(DesignSize(Size).X);
		TestTrue(FString::Printf(TEXT("NEW 높이 .1w (%s)"), ClassSuffix(Size)), FMath::IsNearlyEqual(static_cast<float>(TagSize(ECardPart::New, Size).Y), 0.1f * W, 0.05f));
		TestTrue(FString::Printf(TEXT("▲ 폭 .13w (%s)"), ClassSuffix(Size)), FMath::IsNearlyEqual(static_cast<float>(TagSize(ECardPart::Up, Size).X), 0.13f * W, 0.05f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealCardSurfacesTest,
	"CGR.Gacha.Reveal.Card.Surfaces",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealCardSurfacesTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaRevealCardTests;

	TArray<FString> Missing;
	int32 Checked = 0;
	auto Expect = [&](const FSoftObjectPath& Path)
	{
		++Checked;
		if (Path.IsNull() || !FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			Missing.Add(Path.IsNull() ? FString(TEXT("(빈 경로)")) : Path.ToString());
		}
	};
	for (const EGachaCardSize Size : AllSizes)
	{
		Expect(SurfacePath(ECardPart::Back, EGachaRevealKind::Equipment, 0, Size));
		Expect(SurfacePath(ECardPart::New, EGachaRevealKind::Equipment, 0, Size));
		Expect(SurfacePath(ECardPart::Up, EGachaRevealKind::Equipment, 0, Size));
		for (const EGachaRevealKind Kind : { EGachaRevealKind::Equipment, EGachaRevealKind::Trait })
		{
			for (int32 Grade = 0; Grade <= GachaRevealModel::MaxGrade; ++Grade)
			{
				Expect(SurfacePath(ECardPart::Face, Kind, Grade, Size));
				Expect(SurfacePath(ECardPart::Badge, Kind, Grade, Size));
				if (Grade >= GachaRevealModel::HighGrade)
				{
					Expect(SurfacePath(ECardPart::BackHot, Kind, Grade, Size));
				}
			}
		}
	}
	for (const EGachaRevealKind Kind : { EGachaRevealKind::Equipment, EGachaRevealKind::Trait, EGachaRevealKind::Skin })
	{
		Expect(EmblemPath(Kind));
	}
	// 굽기 card/*.png 180장 − 대조용 orb_ref 15장 = 165 + 엠블럼 3
	TestEqual(TEXT("확인한 그림 수"), Checked, 168);
	TestEqual(TEXT("없는 그림"), Missing.Num(), 0);
	for (const FString& Path : Missing)
	{
		AddError(FString::Printf(TEXT("없음: %s"), *Path));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRGachaRevealCardConfigureTest,
	"CGR.Gacha.Reveal.Card.Configure",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealCardConfigureTest::RunTest(const FString& Parameters)
{
	using namespace CGRGachaRevealCardTests;

	const TSubclassOf<UUserWidget> CardClass = GachaRevealTestTables::RegisteredWidgetClass(EWidgetType::GachaRevealCard);
	if (!TestNotNull(TEXT("DT_WidgetClass 에 GachaRevealCard 행"), CardClass.Get()))
	{
		return false;
	}
	if (!TestTrue(TEXT("행 클래스가 UGachaRevealCardWidget"), CardClass->IsChildOf(UGachaRevealCardWidget::StaticClass())))
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

	TStrongObjectPtr<UGachaRevealCardWidget> Card(CreateWidget<UGachaRevealCardWidget>(World, CardClass));
	if (!TestNotNull(TEXT("카드 생성"), Card.Get()))
	{
		return false;
	}
	UGachaRevealCardWidget& W = *Card.Get();

	// 「숨김」 단언은 앞 상태에서 보이던 것을 숨기는지 본다 ― WBP 기본값(Collapsed)과 같은 상태에서 재면 코드가 안 숨겨도 통과한다
	const FGachaRevealCardData Equip = EquipmentSSCard();
	W.Configure(Equip, EGachaCardSize::Ten);
	ExpectShown(*this, TEXT("NEW 보임"), Shown(FAccess::NewTag(W)));
	ExpectShown(*this, TEXT("▲ 보임"), Shown(FAccess::UpTag(W)));
	ExpectShown(*this, TEXT("+nP 숨김(장비)"), !Shown(FAccess::Dup(W)));
	ExpectShown(*this, TEXT("10회 이름 줄 보임"), Shown(FAccess::Name(W)));
	ExpectShown(*this, TEXT("10회 효과 줄 보임"), Shown(FAccess::Value(W)));
	ExpectShown(*this, TEXT("뿌릴 때는 뒷면"), Shown(FAccess::Back(W)) && !Shown(FAccess::Face(W)));
	ExpectShown(*this, TEXT("SS 순환광"), Shown(FAccess::Orb(W)));
	TestEqual(TEXT("10회 앞면 그림"), ResourceName(FAccess::FaceImage(W)), FString(TEXT("T_GachaCard_Face_Equip_G4_Ten")));

	const FBox2f FirstUV = FAccess::IconImage(W)->GetBrush().GetUVRegion();
	TestTrue(TEXT("아이콘·그림자 같은 crop"), FirstUV == FAccess::IconShadow(W)->GetBrush().GetUVRegion());
	TestTrue(TEXT("정규화 crop 좌표"), FMath::IsNearlyEqual(FirstUV.Min.X, 0.25f)
		&& FMath::IsNearlyEqual(FirstUV.Min.Y, 0.10f)
		&& FMath::IsNearlyEqual(FirstUV.Max.X, 0.75f)
		&& FMath::IsNearlyEqual(FirstUV.Max.Y, 0.90f));
	UTexture2D* IconTexture = Equip.Icon.LoadSynchronous();
	const FIntPoint Imported = IconTexture ? IconTexture->GetImportedSize() : FIntPoint::ZeroValue;
	const FVector2D SourceSize = Imported.X > 0 && Imported.Y > 0 ? FVector2D(Imported.X, Imported.Y)
		: FVector2D(IconTexture ? IconTexture->GetSizeX() : 0, IconTexture ? IconTexture->GetSizeY() : 0);
	const FGachaRevealRect TenIconBox = IconBox(EGachaCardSize::Ten);
	const float SquareSide = FMath::Min(TenIconBox.W, TenIconBox.H);
	const FGachaRevealRect SquareBox{ TenIconBox.X + (TenIconBox.W - SquareSide) * 0.5f,
		TenIconBox.Y + (TenIconBox.H - SquareSide) * 0.5f, SquareSide, SquareSide };
	const FGachaRevealRect ExpectedFit = FitContain(SquareBox, FVector2D(SourceSize.X * 0.5, SourceSize.Y * 0.8));
	const UCanvasPanelSlot* IconSlot = Cast<UCanvasPanelSlot>(FAccess::IconImage(W)->Slot);
	TestTrue(TEXT("crop 뒤 가로세로비 contain"), IconSlot && IconSlot->GetSize().Equals(FVector2D(ExpectedFit.W, ExpectedFit.H), 0.05));
	FGachaRevealCardData Reused = Equip;
	Reused.IconUVRegion = FBox2d(FVector2D(0.10, 0.25), FVector2D(0.90, 0.75));
	W.Configure(Reused, EGachaCardSize::Ten);
	const FBox2f NextUV = FAccess::IconImage(W)->GetBrush().GetUVRegion();
	TestTrue(TEXT("재사용 카드 UV 갱신"), NextUV != FirstUV && NextUV == FAccess::IconShadow(W)->GetBrush().GetUVRegion()
		&& FMath::IsNearlyEqual(NextUV.Min.X, 0.10f) && FMath::IsNearlyEqual(NextUV.Min.Y, 0.25f));
	W.Configure(Equip, EGachaCardSize::Thirty);
	ExpectShown(*this, TEXT("30회 이름 줄 숨김(10회에선 보임)"), !Shown(FAccess::Name(W)));
	ExpectShown(*this, TEXT("30회 효과 줄 보임"), Shown(FAccess::Value(W)));

	W.SetFlip(0.25f);
	ExpectShown(*this, TEXT("0.25 = 뒷면"), Shown(FAccess::Back(W)) && !Shown(FAccess::Face(W)));
	TestTrue(TEXT("0.25 가로 배율 ≈ 0.707"),
		FMath::IsNearlyEqual(static_cast<float>(FAccess::Flip(W)->GetRenderTransform().Scale.X), 0.70710678f, 1e-3f));
	W.SetFlip(0.75f);
	ExpectShown(*this, TEXT("0.75 = 앞면"), Shown(FAccess::Face(W)) && !Shown(FAccess::Back(W)));
	W.SetFaceUp(true);
	TestTrue(TEXT("앞면 고정 배율 1"), FMath::IsNearlyEqual(static_cast<float>(FAccess::Flip(W)->GetRenderTransform().Scale.X), 1.f));

	W.Configure(Equip, EGachaCardSize::Best);
	ExpectShown(*this, TEXT("앞면이던 카드를 다시 Configure 하면 뒷면"), Shown(FAccess::Back(W)) && !Shown(FAccess::Face(W)));
	TestEqual(TEXT("Best 묶음 앞면 그림"), ResourceName(FAccess::FaceImage(W)), FString(TEXT("T_GachaCard_Face_Equip_G4_Best")));
	ExpectShown(*this, TEXT("Best 는 아래 글자 없음(sz1, 30회에선 효과 줄 보임)"), !Shown(FAccess::Name(W)) && !Shown(FAccess::Value(W)));
	W.Configure(Equip, EGachaCardSize::Spot);
	TestEqual(TEXT("Spot 묶음 앞면 그림"), ResourceName(FAccess::FaceImage(W)), FString(TEXT("T_GachaCard_Face_Equip_G4_Spot")));

	W.SetHoldGlow(true);
	ExpectShown(*this, TEXT("S 이상 뒷면 빛"), Shown(FAccess::BackHot(W)) && FAccess::Holding(W));
	// 그림을 못 불러오면 빈 브러시(흰 판)가 숨쉰다 ― 보임만이 아니라 실제 그림 이름까지 본다
	TestEqual(TEXT("S 이상 뒷면 빛 그림"), ResourceName(FAccess::BackHot(W)), FString(TEXT("T_GachaCard_BackHot_Equip_G4_Spot")));
	W.SetFaceUp(true);
	ExpectShown(*this, TEXT("앞면이 되면 숨쉬기 멈춤"), !FAccess::Holding(W) && !Shown(FAccess::BackHot(W)));
	W.SetHoldGlow(true);
	ExpectShown(*this, TEXT("앞면일 때는 숨쉬기 안 켬"), !FAccess::Holding(W) && !Shown(FAccess::BackHot(W)));

	FGachaRevealCardData Skin;
	Skin.Kind = EGachaRevealKind::Skin;
	Skin.Grade = 2;
	Skin.Name = FText::FromString(TEXT("시험용 스킨"));
	Skin.bNew = false;
	Skin.bRecommended = true;
	Skin.MileageGained = 5;
	W.Configure(Skin, EGachaCardSize::Thirty);
	ExpectShown(*this, TEXT("bounds 없는 다음 카드 아이콘 숨김"), !Shown(FAccess::IconImage(W)) && !Shown(FAccess::IconShadow(W)));
	TestTrue(TEXT("재사용 카드의 이전 리소스 제거"), !FAccess::IconImage(W)->GetBrush().GetResourceObject()
		&& !FAccess::IconShadow(W)->GetBrush().GetResourceObject());
	ExpectShown(*this, TEXT("스킨 중복 +nP 보임"), Shown(FAccess::Dup(W)));
	TestEqual(TEXT("+nP 글자"), FAccess::Dup(W)->GetText().ToString(), FString(TEXT("+5P")));
	ExpectShown(*this, TEXT("스킨은 ▲ 없음(앞 장비 카드에선 보임)"), !Shown(FAccess::UpTag(W)));
	ExpectShown(*this, TEXT("A 이하 순환광 없음(앞 SS 카드에선 보임)"), !Shown(FAccess::Orb(W)));
	TestEqual(TEXT("스킨 = 특성 앞면"), ResourceName(FAccess::FaceImage(W)), FString(TEXT("T_GachaCard_Face_Trait_G2_Thirty")));
	W.SetHoldGlow(true);
	ExpectShown(*this, TEXT("A 이하 뒷면 빛 없음"), !Shown(FAccess::BackHot(W)));

	// +nP 외곽선 = WBP 저작값 × 글자 크기 비율. 같은 카드를 다시 Configure 해도 쌓이지 않아야 한다
	const int32 OutlineThirty = FAccess::Dup(W)->GetFont().OutlineSettings.OutlineSize;
	W.Configure(Skin, EGachaCardSize::Single);
	const int32 OutlineSingle = FAccess::Dup(W)->GetFont().OutlineSettings.OutlineSize;
	W.Configure(Skin, EGachaCardSize::Thirty);
	TestTrue(TEXT("+nP 외곽선은 큰 묶음에서 굵다"), OutlineThirty > 0 && OutlineSingle > OutlineThirty);
	TestEqual(TEXT("+nP 외곽선 재설정 누적 없음"), FAccess::Dup(W)->GetFont().OutlineSettings.OutlineSize, OutlineThirty);

	// 재사용 카드: +nP 가 보이던 카드를 장비로 다시 채우면 숨긴다(첫 Configure 의 「+nP 숨김」은 WBP 기본값과 같아 못 가른다)
	W.Configure(Equip, EGachaCardSize::Thirty);
	ExpectShown(*this, TEXT("스킨 다음 장비 = +nP 숨김"), !Shown(FAccess::Dup(W)));
	ExpectShown(*this, TEXT("스킨 다음 장비 = ▲ 다시 보임"), Shown(FAccess::UpTag(W)));
	return true;
}

#endif
