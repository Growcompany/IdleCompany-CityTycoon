#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Element/Buttons/MenuSheetTileWidget.h"
#include "Components/Image.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Table/MenuUnlockData.h"
#include "Tests/LiveOpsRenderTestUtils.h"
#include "Widgets/Layout/SBox.h"
#endif

// BindWidget 필수라 WBP 없이 CreateWidget 할 수 없다 ― 리플렉션으로 계약만 본다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileBindingContractTest, "CGR.LiveOps.MenuSheetTile.MedallionBindingContract",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileBindingContractTest::RunTest(const FString& Parameters)
{
	const UClass* C = UMenuSheetTileWidget::StaticClass();
	TestTrue(TEXT("UMenuTileWidget 파생"), C->IsChildOf(UMenuTileWidget::StaticClass()));
	const FProperty* P = C->FindPropertyByName(TEXT("MedallionMaterial"));
	TestTrue(TEXT("MedallionMaterial 이 인스턴스 편집 가능"), P && P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
	const FObjectProperty* ImageProperty = FindFProperty<FObjectProperty>(C, TEXT("MedallionImage"));
	TestTrue(TEXT("MedallionImage 가 UImage 객체 속성"),
		ImageProperty && ImageProperty->PropertyClass && ImageProperty->PropertyClass->IsChildOf(UImage::StaticClass()));
#if WITH_EDITORONLY_DATA
	// Optional 이면 WBP 에 받침이 빠져도 조용히 null 로 넘어간다 ― 필수 바인딩이어야 컴파일이 막는다
	TestTrue(TEXT("MedallionImage 에 BindWidget 메타"), ImageProperty && ImageProperty->HasMetaData(TEXT("BindWidget")));
	TestFalse(TEXT("MedallionImage 가 BindWidgetOptional 이 아니다"), ImageProperty && ImageProperty->HasMetaData(TEXT("BindWidgetOptional")));
#endif
	// 잠금 칸 회색 글리프 ― WBP 에서 빠지면 잠금 면이 아이콘을 통째로 가린다
	const FObjectProperty* LockedIconProperty = FindFProperty<FObjectProperty>(C, TEXT("LockedIconImage"));
	TestTrue(TEXT("LockedIconImage 가 UImage 객체 속성"),
		LockedIconProperty && LockedIconProperty->PropertyClass && LockedIconProperty->PropertyClass->IsChildOf(UImage::StaticClass()));
#if WITH_EDITORONLY_DATA
	TestTrue(TEXT("LockedIconImage 에 BindWidget 메타(필수)"), LockedIconProperty && LockedIconProperty->HasMetaData(TEXT("BindWidget"))
		&& !LockedIconProperty->HasMetaData(TEXT("BindWidgetOptional")));
#endif
	return true;
}

#if WITH_EDITOR

namespace MenuSheetTileRender
{
	using LiveOpsRenderTestUtils::FWidgetBox;

	static const TCHAR* SquareClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Button/UIE_MenuSheetTile.UIE_MenuSheetTile_C");
	static const TCHAR* WideClassPath = TEXT("/Game/CompanyGrowth/UI/Elements/Button/UIE_MenuSheetTile_Wide.UIE_MenuSheetTile_Wide_C");
	// 라벨의 원천 = 시트에 저작된 타일 인스턴스 (시험이 문구를 따로 들고 있지 않는다)
	static const TCHAR* MenuPanelClassPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_MenuPanel.UI_MenuPanel_C");
	static const TCHAR* MenuUnlockPath = TEXT("/Game/CompanyGrowth/Table/Menu/DT_MenuUnlock.DT_MenuUnlock");
	static const TCHAR* NewsMedallionPath = TEXT("/Game/CompanyGrowth/UI/Materials/MenuSheet/MI_MenuSheet_Medallion_News.MI_MenuSheet_Medallion_News");
	static const TCHAR* CaptureIconPath = TEXT("/Game/CompanyGrowth/UI/Textures/ButtonIcon/Menu/T_MenuIcon_Codex.T_MenuIcon_Codex");

	// Task 4 배치 폭: 정사각 = (728 − 64 − 홈판 여백 32 − 간격 40) / 3, 가로 = (664 − 32 − 20) / 2
	static const FIntPoint SquareSize(197, 192);
	static const FIntPoint WideSize(306, 132);
	// 면 테 두께(MI_MenuSheet_TileFace*) ― 「안쪽 폭」 은 위젯 폭에서 양쪽 테를 뺀 값
	static constexpr double RimPx = 4.0;
	// 이름 자리 좌우 숨통 = 안쪽 폭 − 16 (테 안쪽 8 씩)
	static constexpr double LabelSideBreath = 8.0;
	static constexpr double SquareMedallion = 104.0, WideMedallion = 88.0, MedallionTolerance = 4.0;
	// 받침과 이름 사이 최소 간격 ― 이보다 붙으면 받침 테와 글자가 한 덩어리로 읽힌다
	static constexpr double MedallionLabelGap = 8.0;
	static constexpr double LockReasonGap = 4.0;
	static constexpr double BoxTolerance = 0.5;
	static constexpr int32 InkMinPixels = 40;
	// 알림 점 = 목업 .dot 30 + 검은 테 4 → 보이는 지름 ≈ 38, 오른쪽·위 모서리 밖으로 4~8 걸친다(T4 홈판 여백 16 안)
	static constexpr double BadgeGlyph = 38.0, BadgeGlyphTolerance = 4.0;
	static constexpr double BadgeOverhangMin = 4.0, BadgeOverhangMax = 8.0;
	// 타일보다 큰 렌더 타깃 ― 타일 밖으로 걸친 부분이 타깃 가장자리에서 잘리지 않게
	static constexpr int32 FramePad = 16;
	static constexpr int32 HiddenInkMaxPixels = 3;
	// 자물쇠 상자 왼쪽 위 (타일 기준). 정사각 = 목업 .lockb 보이는 금빛 자리(2026-10-21 실측 −7, −9) · 가로 = Task 3 룰링(받침 오른쪽 아래 옆)
	static const FVector2D SquareLockTopLeft(127.5, 71.0);
	static const FVector2D WideLockTopLeft(104.0, 74.0);
	// 잠금 회색 글리프 ― 흰 글리프 속(휘도 >= 230) 이 잠금 시 0.40~0.75 배·무채색, 회색 칸 수 <= 속 × 1.6 (회색 사각이면 실패)
	static constexpr double GlyphCoreLuma = 230.0;
	static constexpr int32 GlyphMinCore = 50;
	static constexpr double GlyphRatioMin = 0.40, GlyphRatioMax = 0.75, GlyphMaxSaturation = 0.08, GlyphMaxGreyToCore = 1.6;

	static const TCHAR* SheetTileNames[] = {
		TEXT("AttendanceTile"), TEXT("QuestTile"), TEXT("EventTile"),
		TEXT("MailboxTile"), TEXT("AchievementTile"), TEXT("CodexTile"),
		TEXT("NoticeTile"), TEXT("RankingTile"), TEXT("ProfileTile"),
		TEXT("SkylineTile"), TEXT("FusionTile") };
	// 「도전」 묶음만 가로 타일에 앉는다 (설계서 목표 6)
	static bool IsWideTile(const FString& Name) { return Name == TEXT("SkylineTile") || Name == TEXT("FusionTile"); }

	static FString CaptureDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("MenuSheetRedesign/tiles"));
	}

	// 라벨 잉크 = 흰 계열(INK #ECEEF0), 잠금 사유 잉크 = 금빛 뮤트(#C9B27A, 휘도 179 라 흰 잉크 기준에 안 걸린다)
	static bool IsWhiteInk(const FColor& P)
	{
		const int32 Spread = FMath::Max3(P.R, P.G, P.B) - FMath::Min3(P.R, P.G, P.B);
		return P.A >= LiveOpsRenderTestUtils::InkOpaqueAlpha && LiveOpsRenderTestUtils::Luma(P.R, P.G, P.B) >= 180.0 && Spread <= 40;
	}

	static bool IsGoldInk(const FColor& P)
	{
		return P.A >= LiveOpsRenderTestUtils::InkOpaqueAlpha && P.R >= 140 && P.R >= P.G && P.R - P.B >= 45;
	}

	struct FInkScan
	{
		int32 Count = 0;
		int32 MinX = TNumericLimits<int32>::Max(), MaxX = TNumericLimits<int32>::Lowest();
	};

	// Exclude 상자 안은 세지 않는다 ― 자물쇠의 밝은 고리가 이름 자리 모서리에 걸칠 수 있다
	template <typename PredicateType>
	static FInkScan ScanInk(const FImage& Image, const FWidgetBox& Zone, PredicateType Predicate, const FWidgetBox* Exclude = nullptr)
	{
		FInkScan Out;
		const int32 Left = FMath::FloorToInt(Zone.Left), Top = FMath::FloorToInt(Zone.Top);
		const int32 Right = FMath::CeilToInt(Zone.Right()), Bottom = FMath::CeilToInt(Zone.Bottom());
		for (int32 Y = Top; Y < Bottom; ++Y)
		{
			for (int32 X = Left; X < Right; ++X)
			{
				if (Exclude && X >= Exclude->Left && X < Exclude->Right() && Y >= Exclude->Top && Y < Exclude->Bottom())
				{
					continue;
				}
				const FColor* Pixel = LiveOpsRenderTestUtils::PixelAt(Image, X, Y);
				if (Pixel && Predicate(*Pixel))
				{
					++Out.Count;
					Out.MinX = FMath::Min(Out.MinX, X);
					Out.MaxX = FMath::Max(Out.MaxX, X);
				}
			}
		}
		return Out;
	}

	static FWidgetBox Inflate(const FWidgetBox& Box, double Pad)
	{
		FWidgetBox Out = Box;
		Out.Left -= Pad; Out.Top -= Pad; Out.Width += 2.0 * Pad; Out.Height += 2.0 * Pad;
		return Out;
	}

	static bool Overlaps(const FWidgetBox& A, const FWidgetBox& B)
	{
		return A.Left < B.Right() - BoxTolerance && B.Left < A.Right() - BoxTolerance
			&& A.Top < B.Bottom() - BoxTolerance && B.Top < A.Bottom() - BoxTolerance;
	}

	static bool HasArea(const FWidgetBox& Box) { return Box.bFound && Box.Width > 1.0 && Box.Height > 1.0; }

	static FString BoxText(const FWidgetBox& Box)
	{
		return FString::Printf(TEXT("(%.1f,%.1f %.1fx%.1f)"), Box.Left, Box.Top, Box.Width, Box.Height);
	}

	// 타일 하나를 실제 배치 폭으로 띄워 두는 리그 ― Slate 참조를 들고 있어야 재빌드(=PreConstruct 재실행)가 안 일어난다
	struct FTileRig
	{
		TStrongObjectPtr<UMenuSheetTileWidget> Tile;
		TSharedPtr<SWidget> Slate;
		FIntPoint Size = FIntPoint::ZeroValue;

		bool Draw(FImage& Out) const
		{
			return Slate.IsValid() && LiveOpsRenderTestUtils::DrawToImage(Slate.ToSharedRef(), Size, 1.f, Out);
		}

		FWidgetBox Box(const TCHAR* Name) const
		{
			const FVector2D Origin = FVector2D(Tile->GetCachedGeometry().GetAbsolutePosition());
			return LiveOpsRenderTestUtils::MeasureWidget(*Tile.Get(), Name, Origin);
		}

		double DesiredWidth(const TCHAR* Name) const
		{
			const UWidget* Widget = Tile->GetWidgetFromName(FName(Name));
			return Widget ? Widget->GetDesiredSize().X : -1.0;
		}
	};

	static bool MakeRig(FAutomationTestBase& Test, const TCHAR* ClassPath, FIntPoint Size, FTileRig& Out,
		UMaterialInterface* Medallion = nullptr, UTexture2D* Icon = nullptr)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		UClass* TileClass = LoadClass<UMenuSheetTileWidget>(nullptr, ClassPath);
		if (World == nullptr || TileClass == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("타일 WBP 를 못 읽었다 (world %d): %s"), World != nullptr, ClassPath));
			return false;
		}
		Out.Tile.Reset(CreateWidget<UMenuSheetTileWidget>(World, TileClass));
		if (!Out.Tile.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("타일 인스턴스 생성 실패: %s"), ClassPath));
			return false;
		}
		if (Medallion)
		{
			// MedallionMaterial 은 인스턴스 편집 속성(헤더 게터만 공개) ― 시트 인스턴스가 하는 일을 리플렉션으로 흉내 낸다
			if (FObjectProperty* Property = FindFProperty<FObjectProperty>(TileClass, TEXT("MedallionMaterial")))
			{
				Property->SetObjectPropertyValue_InContainer(Out.Tile.Get(), Medallion);
			}
		}
		if (Icon)
		{
			// 시트 인스턴스의 IconTexture 처럼 구성 전에 넣는다 ― 구성 뒤 SetIcon 이면 잠금 회색 글리프가 텍스처를 못 받는다
			if (FObjectProperty* Property = FindFProperty<FObjectProperty>(TileClass, TEXT("IconTexture")))
			{
				Property->SetObjectPropertyValue_InContainer(Out.Tile.Get(), Icon);
			}
			else
			{
				Test.AddError(TEXT("IconTexture 속성을 못 찾았다 ― 리그가 아이콘을 구성 전에 넣지 못한다"));
			}
		}
		Out.Slate = Out.Tile->TakeWidget();
		Out.Size = Size;
		return true;
	}

	static bool ReadSheetLabels(FAutomationTestBase& Test, TArray<TPair<FString, FText>>& Out)
	{
		const UWidgetBlueprintGeneratedClass* PanelClass = LoadObject<UWidgetBlueprintGeneratedClass>(nullptr, MenuPanelClassPath);
		const UWidgetTree* Tree = PanelClass ? PanelClass->GetWidgetTreeArchetype() : nullptr;
		if (Tree == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("시트 위젯 트리를 못 읽었다: %s"), MenuPanelClassPath));
			return false;
		}
		for (const TCHAR* Name : SheetTileNames)
		{
			const UMenuTileWidget* SheetTile = Cast<UMenuTileWidget>(Tree->FindWidget(FName(Name)));
			if (SheetTile == nullptr || SheetTile->GetTileLabel().IsEmpty())
			{
				Test.AddError(FString::Printf(TEXT("시트 타일 라벨이 없다: %s"), Name));
				continue;
			}
			Out.Emplace(Name, SheetTile->GetTileLabel());
		}
		return Out.Num() == UE_ARRAY_COUNT(SheetTileNames);
	}

	static bool ReadLockReasons(FAutomationTestBase& Test, TArray<FText>& Out)
	{
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, MenuUnlockPath);
		if (Table == nullptr)
		{
			Test.AddError(FString::Printf(TEXT("해금 표를 못 읽었다: %s"), MenuUnlockPath));
			return false;
		}
		TArray<FMenuUnlockData*> Rows;
		Table->GetAllRows<FMenuUnlockData>(TEXT("MenuSheetTileTests"), Rows);
		for (const FMenuUnlockData* Row : Rows)
		{
			if (Row && !Row->LockReasonText.IsEmpty())
			{
				Out.Add(Row->LockReasonText);
			}
		}
		// 사유가 하나도 없으면 아래 단언이 전부 공허하게 통과한다
		if (Out.IsEmpty())
		{
			Test.AddError(TEXT("DT_MenuUnlock 에 잠금 사유가 한 줄도 없다"));
			return false;
		}
		return true;
	}

	using LiveOpsRenderTestUtils::IsBadgeRed;

	// 알림 점은 Show 때 0.7 배에서 튀어 오른다 ― 큰 델타로 한 번 그려 등장 연출을 끝낸 뒤 잰다
	static bool DrawFramedSettled(const FTileRig& Rig, FImage& Out)
	{
		const FIntPoint FrameSize = Rig.Size + FIntPoint(2 * FramePad, 2 * FramePad);
		const TSharedRef<SWidget> Framed = SNew(SBox).Padding(FMargin(FramePad))
			[
				SNew(SBox).WidthOverride(Rig.Size.X).HeightOverride(Rig.Size.Y)[Rig.Slate.ToSharedRef()]
			];
		return LiveOpsRenderTestUtils::SettleDraw(Framed, FrameSize, 1.f)
			&& LiveOpsRenderTestUtils::DrawToImage(Framed, FrameSize, 1.f, Out);
	}

	static void SaveCapture(const FImage& Image, const TCHAR* Name)
	{
		const FString Directory = CaptureDirectory();
		IFileManager::Get().MakeDirectory(*Directory, true);
		LiveOpsRenderTestUtils::SaveTilePNG(Image, Directory / Name);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileMedallionBrushAppliedTest, "CGR.LiveOps.MenuSheetTile.MedallionBrushApplied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileMedallionBrushAppliedTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	UMaterialInterface* News = LoadObject<UMaterialInterface>(nullptr, NewsMedallionPath);
	if (News == nullptr)
	{
		AddError(FString::Printf(TEXT("받침 MI 를 못 읽었다: %s"), NewsMedallionPath));
		return false;
	}
	struct FClassCase { const TCHAR* Path; FIntPoint Size; };
	const FClassCase Classes[] = { { SquareClassPath, SquareSize }, { WideClassPath, WideSize } };
	for (const FClassCase& Case : Classes)
	{
		const TCHAR* ClassPath = Case.Path;
		FTileRig Rig;
		if (!MakeRig(*this, ClassPath, Case.Size, Rig, News))
		{
			continue;
		}
		const UImage* Medallion = Cast<UImage>(Rig.Tile->GetWidgetFromName(TEXT("MedallionImage")));
		const UObject* Resource = Medallion ? Medallion->GetBrush().GetResourceObject() : nullptr;
		TestTrue(FString::Printf(TEXT("%s 인스턴스 받침 재질 = News (게터 %s)"), ClassPath,
			Rig.Tile->GetMedallionMaterial() ? *Rig.Tile->GetMedallionMaterial()->GetName() : TEXT("null")),
			Rig.Tile->GetMedallionMaterial() == News);
		TestTrue(FString::Printf(TEXT("%s 구성 후 MedallionImage 브러시 = News (실제 %s)"), ClassPath,
			Resource ? *Resource->GetName() : TEXT("null")), Resource == News);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileSquareLabelFitsTest, "CGR.LiveOps.MenuSheetTile.Layout.SquareLabelFits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileSquareLabelFitsTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	TArray<TPair<FString, FText>> Labels;
	FTileRig Rig;
	if (!ReadSheetLabels(*this, Labels) || !MakeRig(*this, SquareClassPath, SquareSize, Rig))
	{
		return false;
	}
	const double Usable = SquareSize.X - 2.0 * (RimPx + LabelSideBreath);
	for (const TPair<FString, FText>& Label : Labels)
	{
		Rig.Tile->SetButtonText(Label.Value);
		FImage Image;
		if (!Rig.Draw(Image))
		{
			AddError(TEXT("정사각 타일 렌더 실패"));
			return false;
		}
		const double Desired = Rig.DesiredWidth(TEXT("ButtonText"));
		const FWidgetBox LabelBox = Rig.Box(TEXT("LabelTextScale"));
		const FInkScan Ink = ScanInk(Image, Inflate(LabelBox, 2.0), IsWhiteInk);
		const bool bWide = IsWideTile(Label.Key);
		UE_LOG(LogTemp, Display, TEXT("MenuSheetTile square label %s '%s' desired=%.1f usable=%.1f box=%s ink=%d x%d..%d"),
			*Label.Key, *Label.Value.ToString(), Desired, Usable, *BoxText(LabelBox), Ink.Count, Ink.MinX, Ink.MaxX);
		TestTrue(FString::Printf(TEXT("%s 라벨 상자 측정"), *Label.Key), HasArea(LabelBox) && Desired > 1.0);
		TestTrue(FString::Printf(TEXT("%s 라벨 잉크가 보인다 (%d)"), *Label.Key, Ink.Count), Ink.Count >= InkMinPixels);
		// 정사각 칸에 앉는 9개는 줄이지 않고 그대로 들어가야 한다. 「도전」 2개는 가로 타일 몫이라 여기선 축소 안전망(ScaleBox)이 받는지만 본다
		if (!bWide)
		{
			TestTrue(FString::Printf(TEXT("%s desired 폭 %.1f <= 안쪽 폭 − 16 = %.1f"), *Label.Key, Desired, Usable), Desired <= Usable);
		}
		TestTrue(FString::Printf(TEXT("%s 그려진 잉크 x %d..%d 가 이름 자리 %.0f..%.0f 안"), *Label.Key, Ink.MinX, Ink.MaxX,
			RimPx + LabelSideBreath, SquareSize.X - RimPx - LabelSideBreath),
			Ink.Count > 0 && Ink.MinX >= RimPx + LabelSideBreath - 1.0 && Ink.MaxX + 1 <= SquareSize.X - RimPx - LabelSideBreath + 1.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileWideLabelFitsTest, "CGR.LiveOps.MenuSheetTile.Layout.WideLabelFits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileWideLabelFitsTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	TArray<TPair<FString, FText>> Labels;
	FTileRig Rig;
	if (!ReadSheetLabels(*this, Labels) || !MakeRig(*this, WideClassPath, WideSize, Rig))
	{
		return false;
	}
	const double RightLimit = WideSize.X - RimPx - LabelSideBreath;
	int32 Checked = 0;
	for (const TPair<FString, FText>& Label : Labels)
	{
		if (!IsWideTile(Label.Key))
		{
			continue;
		}
		++Checked;
		Rig.Tile->SetButtonText(Label.Value);
		FImage Image;
		if (!Rig.Draw(Image))
		{
			AddError(TEXT("가로 타일 렌더 실패"));
			return false;
		}
		const double Desired = Rig.DesiredWidth(TEXT("ButtonText"));
		const FWidgetBox LabelBox = Rig.Box(TEXT("LabelTextScale"));
		const FWidgetBox Medallion = Rig.Box(TEXT("MedallionImage"));
		const double Available = RightLimit - LabelBox.Left;
		const FInkScan Ink = ScanInk(Image, Inflate(LabelBox, 2.0), IsWhiteInk);
		UE_LOG(LogTemp, Display, TEXT("MenuSheetTile wide label %s '%s' desired=%.1f available=%.1f box=%s medallion=%s ink=%d x%d..%d"),
			*Label.Key, *Label.Value.ToString(), Desired, Available, *BoxText(LabelBox), *BoxText(Medallion), Ink.Count, Ink.MinX, Ink.MaxX);
		TestTrue(FString::Printf(TEXT("%s 라벨·받침 상자 측정"), *Label.Key), HasArea(LabelBox) && HasArea(Medallion) && Desired > 1.0);
		TestTrue(FString::Printf(TEXT("%s 이름이 받침 오른쪽 %.0f 이상 떨어져 시작 (%.1f vs 받침 끝 %.1f)"), *Label.Key,
			MedallionLabelGap, LabelBox.Left, Medallion.Right()), LabelBox.Left >= Medallion.Right() + MedallionLabelGap - BoxTolerance);
		TestTrue(FString::Printf(TEXT("%s desired 폭 %.1f <= 받침 오른쪽 가용 폭 %.1f"), *Label.Key, Desired, Available),
			Desired <= Available);
		TestTrue(FString::Printf(TEXT("%s 그려진 잉크 끝 %d <= %.0f"), *Label.Key, Ink.MaxX + 1, RightLimit),
			Ink.Count >= InkMinPixels && Ink.MaxX + 1 <= RightLimit + 1.0);
	}
	TestEqual(TEXT("가로 타일 라벨 수"), Checked, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileAllLockReasonsFitTest, "CGR.LiveOps.MenuSheetTile.Layout.AllLockReasonsFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileAllLockReasonsFitTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	TArray<FText> Reasons;
	FTileRig Square, Wide;
	if (!ReadLockReasons(*this, Reasons) || !MakeRig(*this, SquareClassPath, SquareSize, Square)
		|| !MakeRig(*this, WideClassPath, WideSize, Wide))
	{
		return false;
	}
	for (const FText& Reason : Reasons)
	{
		FImage Image;
		Square.Tile->SetLocked(true, Reason);
		const bool bSquareDrawn = Square.Draw(Image);
		const double SquareUsable = SquareSize.X - 2.0 * (RimPx + LabelSideBreath);
		const double SquareDesired = Square.DesiredWidth(TEXT("LockReasonText"));
		const FWidgetBox SquareReason = Square.Box(TEXT("LockReasonText"));
		TestTrue(FString::Printf(TEXT("정사각 '%s' 사유 측정"), *Reason.ToString()), bSquareDrawn && HasArea(SquareReason));
		TestTrue(FString::Printf(TEXT("정사각 '%s' 사유 폭 %.1f <= 이름 자리 %.1f"), *Reason.ToString(), SquareDesired, SquareUsable),
			SquareDesired > 1.0 && SquareDesired <= SquareUsable);

		Wide.Tile->SetLocked(true, Reason);
		const bool bWideDrawn = Wide.Draw(Image);
		const double WideDesired = Wide.DesiredWidth(TEXT("LockReasonText"));
		const FWidgetBox WideReason = Wide.Box(TEXT("LockReasonText"));
		const FWidgetBox Lock = Wide.Box(TEXT("LockIcon"));
		const double WideAvailable = WideSize.X - RimPx - LabelSideBreath - WideReason.Left;
		UE_LOG(LogTemp, Display, TEXT("MenuSheetTile reason '%s' square=%.1f/%.1f wide=%.1f/%.1f wide_reason=%s lock=%s"),
			*Reason.ToString(), SquareDesired, SquareUsable, WideDesired, WideAvailable, *BoxText(WideReason), *BoxText(Lock));
		TestTrue(FString::Printf(TEXT("가로 '%s' 사유·자물쇠 측정"), *Reason.ToString()), bWideDrawn && HasArea(WideReason) && HasArea(Lock));
		TestTrue(FString::Printf(TEXT("가로 '%s' 사유가 자물쇠 오른쪽에서 시작 (%.1f >= %.1f)"), *Reason.ToString(),
			WideReason.Left, Lock.Right() + LockReasonGap), WideReason.Left >= Lock.Right() + LockReasonGap - BoxTolerance);
		TestTrue(FString::Printf(TEXT("가로 '%s' 사유 폭 %.1f <= 가용 %.1f"), *Reason.ToString(), WideDesired, WideAvailable),
			WideDesired > 1.0 && WideDesired <= WideAvailable);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileLockedWideLayoutTest, "CGR.LiveOps.MenuSheetTile.Layout.LockedWideTileLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileLockedWideLayoutTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	FTileRig Rig;
	if (!MakeRig(*this, WideClassPath, WideSize, Rig))
	{
		return false;
	}
	// 초반 기본 화면 = 가로 두 칸 모두 이 문구로 잠겨 있다 (DT_MenuUnlock Skyline/Fusion)
	Rig.Tile->SetLocked(true, FText::FromString(TEXT("본사 Lv.2")));
	FImage Image;
	TestTrue(TEXT("잠금 가로 타일 렌더"), Rig.Draw(Image));
	const FWidgetBox Lock = Rig.Box(TEXT("LockIcon"));
	const FWidgetBox Reason = Rig.Box(TEXT("LockReasonText"));
	const FWidgetBox Medallion = Rig.Box(TEXT("MedallionImage"));
	const FWidgetBox Locked = Rig.Box(TEXT("LockedMedallion"));
	UE_LOG(LogTemp, Display, TEXT("MenuSheetTile locked wide lock=%s reason=%s medallion=%s locked_medallion=%s"),
		*BoxText(Lock), *BoxText(Reason), *BoxText(Medallion), *BoxText(Locked));
	// 크기 0 이면 아래 비겹침 단언이 공허하게 통과한다 ― 먼저 막는다
	TestTrue(TEXT("자물쇠·사유·받침 상자에 면적이 있다"), HasArea(Lock) && HasArea(Reason) && HasArea(Medallion));
	TestFalse(TEXT("자물쇠 ∩ 사유 = 없음"), Overlaps(Lock, Reason));
	TestFalse(TEXT("자물쇠 ∩ 받침 = 없음"), Overlaps(Lock, Medallion));
	TestFalse(TEXT("사유 ∩ 받침 = 없음"), Overlaps(Reason, Medallion));
	for (const FWidgetBox* Box : { &Lock, &Reason, &Medallion })
	{
		TestTrue(FString::Printf(TEXT("%s 가 타일 %dx%d 안"), *BoxText(*Box), WideSize.X, WideSize.Y),
			Box->Left >= -BoxTolerance && Box->Top >= -BoxTolerance
			&& Box->Right() <= WideSize.X + BoxTolerance && Box->Bottom() <= WideSize.Y + BoxTolerance);
	}
	TestTrue(TEXT("어두운 받침이 받침 자리를 그대로 덮는다"), HasArea(Locked)
		&& FMath::Abs(Locked.Left - Medallion.Left) <= 1.0 && FMath::Abs(Locked.Top - Medallion.Top) <= 1.0
		&& FMath::Abs(Locked.Width - Medallion.Width) <= 1.0 && FMath::Abs(Locked.Height - Medallion.Height) <= 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileLockHidesLabelTest, "CGR.LiveOps.MenuSheetTile.Layout.LockHidesLabel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileLockHidesLabelTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	struct FCase { const TCHAR* ClassPath; FIntPoint Size; const TCHAR* Label; const TCHAR* Prefix; };
	const FCase Cases[] = {
		{ SquareClassPath, SquareSize, TEXT("도감"), TEXT("square") },
		{ WideClassPath, WideSize, TEXT("스카이라인"), TEXT("wide") } };
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, CaptureIconPath);
	for (const FCase& Case : Cases)
	{
		FTileRig Rig;
		if (!MakeRig(*this, Case.ClassPath, Case.Size, Rig, nullptr, Icon))
		{
			continue;
		}
		Rig.Tile->SetButtonText(FText::FromString(Case.Label));
		FImage Open, Closed;
		const bool bOpen = Rig.Draw(Open);
		const FWidgetBox NameZone = Inflate(Rig.Box(TEXT("LabelTextScale")), 2.0);
		Rig.Tile->SetLocked(true, FText::FromString(TEXT("본사 Lv.2")));
		const bool bClosed = Rig.Draw(Closed);
		const FWidgetBox Lock = Rig.Box(TEXT("LockIcon"));
		SaveCapture(Open, *FString::Printf(TEXT("%s_normal.png"), Case.Prefix));
		SaveCapture(Closed, *FString::Printf(TEXT("%s_locked.png"), Case.Prefix));

		const FInkScan OpenLabel = ScanInk(Open, NameZone, IsWhiteInk);
		const FInkScan ClosedLabel = ScanInk(Closed, NameZone, IsWhiteInk, &Lock);
		const FInkScan ClosedReason = ScanInk(Closed, NameZone, IsGoldInk, &Lock);
		UE_LOG(LogTemp, Display, TEXT("MenuSheetTile %s name zone=%s label ink open=%d locked=%d reason ink=%d lock=%s"),
			Case.Prefix, *BoxText(NameZone), OpenLabel.Count, ClosedLabel.Count, ClosedReason.Count, *BoxText(Lock));
		TestTrue(FString::Printf(TEXT("%s 렌더 2회"), Case.Prefix), bOpen && bClosed && HasArea(NameZone));
		TestTrue(FString::Printf(TEXT("%s 평상시 이름 잉크 %d >= %d"), Case.Prefix, OpenLabel.Count, InkMinPixels),
			OpenLabel.Count >= InkMinPixels);
		TestTrue(FString::Printf(TEXT("%s 잠금 시 이름 잉크 %d <= %d"), Case.Prefix, ClosedLabel.Count, HiddenInkMaxPixels),
			ClosedLabel.Count <= HiddenInkMaxPixels);
		TestTrue(FString::Printf(TEXT("%s 잠금 시 사유 금빛 잉크 %d >= %d"), Case.Prefix, ClosedReason.Count, InkMinPixels),
			ClosedReason.Count >= InkMinPixels);

		// 자물쇠는 잠금 레이어(아이콘 위)에 있어 아이콘 모서리에 걸쳐도 보인다 ― 타일 안 · 아이콘 가운데는 비우고 · 사유 밖 · 정해진 자리
		const FWidgetBox IconBox = Rig.Box(TEXT("IconImage"));
		const FWidgetBox ReasonBox = Rig.Box(TEXT("LockReasonText"));
		const FVector2D IconCenter(IconBox.Left + 0.5 * IconBox.Width, IconBox.Top + 0.5 * IconBox.Height);
		const FVector2D ExpectedLock = Case.ClassPath == SquareClassPath ? SquareLockTopLeft : WideLockTopLeft;
		TestTrue(FString::Printf(TEXT("%s 자물쇠 자리 (%.1f,%.1f) == (%.1f,%.1f) ±1"), Case.Prefix, Lock.Left, Lock.Top,
			ExpectedLock.X, ExpectedLock.Y), HasArea(Lock) && FMath::Abs(Lock.Left - ExpectedLock.X) <= 1.0
			&& FMath::Abs(Lock.Top - ExpectedLock.Y) <= 1.0);
		TestTrue(FString::Printf(TEXT("%s 자물쇠 %s 가 타일 %dx%d 안"), Case.Prefix, *BoxText(Lock), Case.Size.X, Case.Size.Y),
			HasArea(Lock) && Lock.Left >= -BoxTolerance && Lock.Top >= -BoxTolerance
			&& Lock.Right() <= Case.Size.X - RimPx + BoxTolerance && Lock.Bottom() <= Case.Size.Y + BoxTolerance);
		TestTrue(FString::Printf(TEXT("%s 자물쇠 %s 가 아이콘 가운데 (%.1f,%.1f) 를 가리지 않는다"), Case.Prefix, *BoxText(Lock),
			IconCenter.X, IconCenter.Y), HasArea(IconBox) && !(IconCenter.X >= Lock.Left && IconCenter.X <= Lock.Right()
			&& IconCenter.Y >= Lock.Top && IconCenter.Y <= Lock.Bottom()));
		TestTrue(FString::Printf(TEXT("%s 자물쇠 %s ∩ 사유 %s = 없음"), Case.Prefix, *BoxText(Lock), *BoxText(ReasonBox)),
			HasArea(ReasonBox) && !Overlaps(Lock, ReasonBox));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileMedallionSizeTest, "CGR.LiveOps.MenuSheetTile.Layout.MedallionSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileMedallionSizeTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	struct FCase { const TCHAR* ClassPath; FIntPoint Size; double Expected; const TCHAR* Prefix; };
	const FCase Cases[] = {
		{ SquareClassPath, SquareSize, SquareMedallion, TEXT("square") },
		{ WideClassPath, WideSize, WideMedallion, TEXT("wide") } };
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, CaptureIconPath);
	for (const FCase& Case : Cases)
	{
		FTileRig Rig;
		if (!MakeRig(*this, Case.ClassPath, Case.Size, Rig, nullptr, Icon))
		{
			continue;
		}
		Rig.Tile->SetButtonText(FText::FromString(TEXT("도감")));
		FImage Image;
		const bool bDrawn = Rig.Draw(Image);
		const FWidgetBox Medallion = Rig.Box(TEXT("MedallionImage"));
		const FWidgetBox IconBox = Rig.Box(TEXT("IconImage"));
		UE_LOG(LogTemp, Display, TEXT("MenuSheetTile %s medallion=%s icon=%s"), Case.Prefix,
			*BoxText(Medallion), *BoxText(IconBox));
		TestTrue(FString::Printf(TEXT("%s 렌더"), Case.Prefix), bDrawn);
		TestTrue(FString::Printf(TEXT("%s 받침 %.1fx%.1f ≈ %.0f (±%.0f)"), Case.Prefix, Medallion.Width, Medallion.Height,
			Case.Expected, MedallionTolerance), HasArea(Medallion)
			&& FMath::Abs(Medallion.Width - Case.Expected) <= MedallionTolerance
			&& FMath::Abs(Medallion.Height - Case.Expected) <= MedallionTolerance);
		const FVector2D MedallionCenter(Medallion.Left + 0.5 * Medallion.Width, Medallion.Top + 0.5 * Medallion.Height);
		const FVector2D IconCenter(IconBox.Left + 0.5 * IconBox.Width, IconBox.Top + 0.5 * IconBox.Height);
		TestTrue(FString::Printf(TEXT("%s 아이콘이 받침 가운데 (%.1f,%.1f vs %.1f,%.1f)"), Case.Prefix,
			IconCenter.X, IconCenter.Y, MedallionCenter.X, MedallionCenter.Y),
			HasArea(IconBox) && FVector2D::Distance(IconCenter, MedallionCenter) <= 1.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileBadgeOverhangTest, "CGR.LiveOps.MenuSheetTile.Layout.BadgeOverhang",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileBadgeOverhangTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	struct FCase { const TCHAR* ClassPath; FIntPoint Size; const TCHAR* Prefix; };
	const FCase Cases[] = { { SquareClassPath, SquareSize, TEXT("square") }, { WideClassPath, WideSize, TEXT("wide") } };
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, CaptureIconPath);
	for (const FCase& Case : Cases)
	{
		FTileRig Rig;
		if (!MakeRig(*this, Case.ClassPath, Case.Size, Rig, nullptr, Icon))
		{
			continue;
		}
		Rig.Tile->SetButtonText(FText::FromString(TEXT("도감")));
		Rig.Tile->ShowBadge();
		FImage Image;
		const bool bDrawn = DrawFramedSettled(Rig, Image);
		SaveCapture(Image, *FString::Printf(TEXT("%s_badge.png"), Case.Prefix));

		// 빨간 글리프의 실제 픽셀 범위를 타일 원점(FramePad) 기준으로 잰다 ― 위젯 상자는 투명 여백을 포함한다
		int32 MinX = TNumericLimits<int32>::Max(), MaxX = TNumericLimits<int32>::Lowest();
		int32 MinY = TNumericLimits<int32>::Max(), MaxY = TNumericLimits<int32>::Lowest(), Count = 0;
		for (int32 Y = 0; bDrawn && Y < Image.SizeY; ++Y)
		{
			for (int32 X = 0; X < Image.SizeX; ++X)
			{
				const FColor* Pixel = LiveOpsRenderTestUtils::PixelAt(Image, X, Y);
				if (Pixel && IsBadgeRed(*Pixel))
				{
					++Count;
					MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X);
					MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y);
				}
			}
		}
		// 빨간 칸이 0 이면 Min/Max 가 초기값이라 빼기가 넘친다 ― 칸이 있을 때만 계산한다
		const double GlyphW = Count > 0 ? static_cast<double>(MaxX) - MinX + 1.0 : 0.0;
		const double GlyphH = Count > 0 ? static_cast<double>(MaxY) - MinY + 1.0 : 0.0;
		const double RightOverhang = Count > 0 ? (static_cast<double>(MaxX) + 1.0 - FramePad) - Case.Size.X : 0.0;
		const double TopOverhang = Count > 0 ? static_cast<double>(FramePad) - MinY : 0.0;
		UE_LOG(LogTemp, Display, TEXT("MenuSheetTile %s badge glyph %.0fx%.0f (red px %d) overhang right=%.0f top=%.0f"),
			Case.Prefix, GlyphW, GlyphH, Count, RightOverhang, TopOverhang);
		TestTrue(FString::Printf(TEXT("%s 틀 렌더 + 빨간 점 픽셀 %d"), Case.Prefix, Count), bDrawn && Count > 0);
		TestTrue(FString::Printf(TEXT("%s 보이는 점 %.0fx%.0f ≈ %.0f (±%.0f)"), Case.Prefix, GlyphW, GlyphH, BadgeGlyph,
			BadgeGlyphTolerance), Count > 0 && FMath::Abs(GlyphW - BadgeGlyph) <= BadgeGlyphTolerance
			&& FMath::Abs(GlyphH - BadgeGlyph) <= BadgeGlyphTolerance);
		TestTrue(FString::Printf(TEXT("%s 점이 오른쪽 모서리 밖으로 %.0f (%.0f~%.0f)"), Case.Prefix, RightOverhang,
			BadgeOverhangMin, BadgeOverhangMax), Count > 0 && RightOverhang >= BadgeOverhangMin && RightOverhang <= BadgeOverhangMax);
		TestTrue(FString::Printf(TEXT("%s 점이 위 모서리 밖으로 %.0f (%.0f~%.0f)"), Case.Prefix, TopOverhang,
			BadgeOverhangMin, BadgeOverhangMax), Count > 0 && TopOverhang >= BadgeOverhangMin && TopOverhang <= BadgeOverhangMax);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSheetTileLockedIconGlyphTest, "CGR.LiveOps.MenuSheetTile.Layout.LockedIconGlyph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMenuSheetTileLockedIconGlyphTest::RunTest(const FString& Parameters)
{
	using namespace MenuSheetTileRender;
	struct FCase { const TCHAR* ClassPath; FIntPoint Size; const TCHAR* Prefix; };
	const FCase Cases[] = { { SquareClassPath, SquareSize, TEXT("square") }, { WideClassPath, WideSize, TEXT("wide") } };
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, CaptureIconPath);
	if (!TestNotNull(TEXT("시험 아이콘 텍스처"), Icon))
	{
		return false;
	}
	for (const FCase& Case : Cases)
	{
		FTileRig Rig;
		if (!MakeRig(*this, Case.ClassPath, Case.Size, Rig, nullptr, Icon))
		{
			continue;
		}
		Rig.Tile->SetButtonText(FText::FromString(TEXT("도감")));
		const UImage* Glyph = Cast<UImage>(Rig.Tile->GetWidgetFromName(TEXT("LockedIconImage")));
		TestTrue(FString::Printf(TEXT("%s 잠금 글리프 리소스 = 아이콘 텍스처"), Case.Prefix),
			Glyph != nullptr && Glyph->GetBrush().GetResourceObject() == Icon);

		FImage Open, Closed;
		const bool bOpen = Rig.Draw(Open);
		const FWidgetBox IconBox = Rig.Box(TEXT("IconImage"));
		Rig.Tile->SetLocked(true, FText::FromString(TEXT("본사 Lv.2")));
		const bool bClosed = Rig.Draw(Closed);
		const FWidgetBox Lock = Rig.Box(TEXT("LockIcon"));
		SaveCapture(Closed, *FString::Printf(TEXT("%s_locked_glyph.png"), Case.Prefix));

		// 자물쇠가 걸친 칸은 금빛이라 뺀다 ― 글리프 속만 잰다
		auto UnderLock = [&Lock](int32 X, int32 Y)
		{
			return X >= Lock.Left && X < Lock.Right() && Y >= Lock.Top && Y < Lock.Bottom();
		};
		int32 Core = 0, Grey = 0;
		double OpenLuma = 0.0, ClosedLuma = 0.0, Saturation = 0.0;
		for (int32 Y = FMath::FloorToInt(IconBox.Top); bOpen && bClosed && Y < FMath::CeilToInt(IconBox.Bottom()); ++Y)
		{
			for (int32 X = FMath::FloorToInt(IconBox.Left); X < FMath::CeilToInt(IconBox.Right()); ++X)
			{
				const FColor* A = LiveOpsRenderTestUtils::PixelAt(Open, X, Y);
				const FColor* B = LiveOpsRenderTestUtils::PixelAt(Closed, X, Y);
				if (A == nullptr || B == nullptr || UnderLock(X, Y))
				{
					continue;
				}
				const double High = FMath::Max3(B->R, B->G, B->B), Low = FMath::Min3(B->R, B->G, B->B);
				const double ClosedPixel = LiveOpsRenderTestUtils::Luma(B->R, B->G, B->B);
				if (B->A >= 250 && ClosedPixel >= 110.0 && ClosedPixel <= 200.0 && High - Low <= GlyphMaxSaturation * 255.0)
				{
					++Grey;
				}
				if (A->A >= 250 && LiveOpsRenderTestUtils::Luma(A->R, A->G, A->B) >= GlyphCoreLuma)
				{
					++Core;
					OpenLuma += LiveOpsRenderTestUtils::Luma(A->R, A->G, A->B);
					ClosedLuma += ClosedPixel;
					Saturation += High > 0.0 ? (High - Low) / High : 0.0;
				}
			}
		}
		const double Ratio = Core > 0 && OpenLuma > 0.0 ? ClosedLuma / OpenLuma : 0.0;
		const double MeanSaturation = Core > 0 ? Saturation / Core : 1.0;
		UE_LOG(LogTemp, Display, TEXT("MenuSheetTile %s locked glyph core=%d grey=%d ratio=%.3f sat=%.3f lock=%s"),
			Case.Prefix, Core, Grey, Ratio, MeanSaturation, *BoxText(Lock));
		TestTrue(FString::Printf(TEXT("%s 흰 글리프 속 %d px 가 잠금 시 휘도 비 %.2f (%.2f~%.2f) · 채도 %.3f (<= %.2f)"), Case.Prefix,
			Core, Ratio, GlyphRatioMin, GlyphRatioMax, MeanSaturation, GlyphMaxSaturation),
			Core >= GlyphMinCore && Ratio >= GlyphRatioMin && Ratio <= GlyphRatioMax && MeanSaturation <= GlyphMaxSaturation);
		TestTrue(FString::Printf(TEXT("%s 회색 칸 %d <= 글리프 속 %d x %.1f (글리프 모양을 따른다 ― 회색 사각이면 실패)"), Case.Prefix,
			Grey, Core, GlyphMaxGreyToCore), Core > 0 && Grey > 0 && Grey <= GlyphMaxGreyToCore * Core);
	}
	return true;
}

#endif // WITH_EDITOR

#endif
