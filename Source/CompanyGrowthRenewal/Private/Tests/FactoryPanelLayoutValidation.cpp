#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Element/Building/UpgradeSlot.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

// ⚠⚠ 이 하네스가 판정할 수 있는 것 = 레이아웃(지오메트리)뿐이다 (2026-09-23 실측).
// FWidgetRenderer 는 **머티리얼 브러시를 안 그린다** — 텍스처 브러시만 나온다.
// 실제로 이 패널을 뽑으면 셸(UIE_PanelBorder) · 슬롯 크림 카드(MI_Border_UpgradeSlot_Cream) ·
// 버튼 배경(CUI_Style2_Btn_*) · 헤더 패턴(MI_UI_BrickScatter) 이 전부 검정으로 비고,
// 아이콘/글자만 보인다. 손대지 않은 기존 자산도 똑같이 빠지므로 **PNG 가 검다고 버그로 읽지 말 것.**
// 색·재질·패턴 판정은 PIE 로 해야 한다. 여기 단언은 전부 치수/순서에 대한 것이다.
//
// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌 (LiveOps 검증 파일들과 같은 규약)
namespace FactoryPanelLayout
{
	static const TCHAR* PanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_FactoryPanel.UI_FactoryPanel_C");
	static const TCHAR* SlotPath = TEXT("/Game/CompanyGrowth/UI/Elements/Upgrade/UIE_UpgradeSlotHorizon.UIE_UpgradeSlotHorizon_C");

	// 드로어 폭 계약 — DrawerWidthBox 의 MinDesired == MaxDesired. 하한이자 상한이다.
	// 상한이 없으면 강화 비용/레벨 문자열이 길어질 때마다 패널 전체 폭이 따라 움직인다(사용자 신고).
	static constexpr double DrawerWidth = 1040.0;
	static constexpr double WidthTolerance = 2.0;   // 레이아웃 패스 반올림

	static constexpr int32 ViewportHeight = 1440;
	// 2560 = 디자인 기준, 2304 = 16:10 태블릿(갤탭 S8) 가로 최악. 둘 다에서 같은 폭이어야 한다
	static constexpr int32 DesignWidth = 2560;
	static constexpr int32 NarrowWidth = 2304;

	struct FLayoutBox
	{
		bool bFound = false;
		double Left = 0.0, Top = 0.0, Width = 0.0, Height = 0.0;
		double Right() const { return Left + Width; }
	};

	static FString RenderDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("FactoryUI/render"));
	}

	static FLayoutBox MeasureWidget(UUserWidget& Root, const TCHAR* Name, FVector2D Origin)
	{
		FLayoutBox Out;
		UWidget* Widget = Root.GetWidgetFromName(FName(Name));
		if (Widget == nullptr)
		{
			return Out;
		}
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D Position = FVector2D(Geometry.GetAbsolutePosition()) - Origin;
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		Out.bFound = true;
		Out.Left = Position.X;
		Out.Top = Position.Y;
		Out.Width = Size.X;
		Out.Height = Size.Y;
		return Out;
	}

	static bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		// 앞 두 패스는 레이아웃만 세운다 — 스크롤 상자는 지오메트리가 잡힌 다음 패스에서 자식을 배치한다
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		// FWidgetRenderer(true) 는 linear 로 태그된 타겟에 이미 감마 인코딩된 바이트를 써 둔다
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static bool SavePNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactoryPanelLayoutTest, "CGR.Factory.PanelLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFactoryPanelLayoutTest::RunTest(const FString& Parameters)
{
	using namespace FactoryPanelLayout;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World == nullptr)
	{
		AddError(TEXT("에디터 월드가 없습니다 ― 렌더 하네스를 세울 수 없습니다"));
		return false;
	}

	UClass* PanelClass = LoadClass<UUserWidget>(nullptr, PanelPath);
	UClass* SlotClass = LoadClass<UUpgradeSlot>(nullptr, SlotPath);
	if (PanelClass == nullptr || SlotClass == nullptr)
	{
		AddError(TEXT("UI_FactoryPanel / UIE_UpgradeSlotHorizon 클래스를 못 읽었다"));
		return false;
	}

	const FString Directory = RenderDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);

	// 한 번의 측정 = [패널 생성 → 슬롯 픽스처 주입 → 3패스 드로우 → DrawerWidthBox 측정].
	// bStress 가 콘텐츠 길이만 바꾼다 — 그 외 조건은 동일해야 폭 비교가 성립한다.
	auto Measure = [&](int32 ViewportWidth, bool bStress, const FString& PngName,
		double& OutDrawerWidth, double& OutWorstSlotRight, double& OutContainerRight) -> bool
	{
		UUserWidget* Panel = CreateWidget<UUserWidget>(World, PanelClass);
		if (Panel == nullptr)
		{
			AddError(TEXT("UI_FactoryPanel 인스턴스 생성 실패"));
			return false;
		}
		TStrongObjectPtr<UUserWidget> PanelGuard(Panel);
		const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();

		UScrollBox* Container = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("UpgradeSlotContainer")));
		if (Container == nullptr)
		{
			AddError(TEXT("UpgradeSlotContainer 를 못 찾았다"));
			return false;
		}
		Container->ClearChildren();

		TArray<UUpgradeSlot*> Slots;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			UUpgradeSlot* Slot = CreateWidget<UUpgradeSlot>(Panel, SlotClass);
			if (Slot == nullptr)
			{
				AddError(TEXT("UIE_UpgradeSlotHorizon 인스턴스 생성 실패"));
				return false;
			}
			// 최악 = DT 에서 가장 긴 표시명/부제 + 만렙 레벨뱃지 + 소수 3자리 값 3개
			Slot->Description = bStress
				? FText::FromString(TEXT("자동생산용량"))
				: FText::FromString(TEXT("생산량"));
			Slot->SubDescription = bStress
				? FText::FromString(TEXT("누르고 있으면 더 빠르게 생산합니다"))
				: FText::FromString(TEXT("생산합니다"));
			Slot->ValueUnit = TEXT("개");
			Container->AddChild(Slot);
			Slots.Add(Slot);
		}

		// TakeWidget 이후에 채워야 BindWidget 이 잡힌 상태에서 텍스트가 들어간다
		for (UUpgradeSlot* Slot : Slots)
		{
			if (bStress)
			{
				Slot->UpdateInfo(/*Level*/ 9999, /*Current*/ 1234.567f, /*Next*/ 1345.678f,
					/*Cost*/ 9990000000000LL, /*bIsLocked*/ false, /*MaxLevel*/ 10000);
			}
			else
			{
				Slot->UpdateInfo(1, 1.0f, 2.0f, 100LL, false, 10000);
			}
		}

		FImage Image;
		const bool bCaptured = DrawToImage(PanelSlate, FIntPoint(ViewportWidth, ViewportHeight), Image)
			&& SavePNG(Image, Directory / PngName);
		TestTrue(FString::Printf(TEXT("캡처 저장 (%s)"), *PngName), bCaptured);

		const FVector2D Origin = Panel->GetRootWidget()
			? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition())
			: FVector2D::ZeroVector;

		const FLayoutBox Drawer = MeasureWidget(*Panel, TEXT("DrawerWidthBox"), Origin);
		if (!Drawer.bFound)
		{
			AddError(TEXT("DrawerWidthBox 를 못 찾았다 ― 폭 권위자가 사라졌다"));
			return false;
		}
		OutDrawerWidth = Drawer.Width;

		const FGeometry& ContainerGeo = Container->GetCachedGeometry();
		OutContainerRight = (FVector2D(ContainerGeo.GetAbsolutePosition()) - Origin).X
			+ FVector2D(ContainerGeo.GetLocalSize()).X;

		// 슬롯이 컨테이너 밖으로 삐져나오는지 — Slate 는 클리핑이 없어 넘치면 이웃을 침범한다
		OutWorstSlotRight = 0.0;
		for (UUpgradeSlot* Slot : Slots)
		{
			const FGeometry& Geo = Slot->GetCachedGeometry();
			const double Right = (FVector2D(Geo.GetAbsolutePosition()) - Origin).X
				+ FVector2D(Geo.GetLocalSize()).X;
			OutWorstSlotRight = FMath::Max(OutWorstSlotRight, Right);
		}

		// 복귀 주기 안내 띠는 폐기됐다 ― 패널 C++ 와 WBP 에서 함께 제거
		TestFalse(TEXT("OfflineGainStrip 은 트리에 없어야 한다"),
			MeasureWidget(*Panel, TEXT("OfflineGainStrip"), Origin).bFound);

		// 수령 버튼은 폐기됐다 — WBP 에 남아 있으면 "안내인데 누를 수 있는" 모순이 된다
		TestFalse(TEXT("StockCollectButton 은 트리에 없어야 한다"),
			MeasureWidget(*Panel, TEXT("StockCollectButton"), Origin).bFound);
		return true;
	};

	// ── 본 단언: 같은 패널이 콘텐츠 길이에 따라 폭이 달라지면 안 된다 ──
	double LeanWidth = 0.0, LeanSlotRight = 0.0, LeanContainerRight = 0.0;
	double StressWidth = 0.0, StressSlotRight = 0.0, StressContainerRight = 0.0;

	if (!Measure(DesignWidth, false, TEXT("factory_lean_2560.png"), LeanWidth, LeanSlotRight, LeanContainerRight))
	{
		return false;
	}
	if (!Measure(DesignWidth, true, TEXT("factory_stress_2560.png"), StressWidth, StressSlotRight, StressContainerRight))
	{
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("FactoryPanel: lean=%.1f stress=%.1f (기대 %.0f)"),
		LeanWidth, StressWidth, DrawerWidth);

	TestTrue(FString::Printf(TEXT("최소 콘텐츠 드로어 폭 %.1f == %.0f"), LeanWidth, DrawerWidth),
		FMath::Abs(LeanWidth - DrawerWidth) <= WidthTolerance);
	TestTrue(FString::Printf(TEXT("최악 콘텐츠 드로어 폭 %.1f == %.0f"), StressWidth, DrawerWidth),
		FMath::Abs(StressWidth - DrawerWidth) <= WidthTolerance);
	// 이 한 줄이 사용자 신고("비용/레벨이 바뀌면 폭이 달라진다")의 직접 회귀 가드다
	TestTrue(FString::Printf(TEXT("콘텐츠 길이가 폭을 안 바꾼다 (%.1f vs %.1f)"), LeanWidth, StressWidth),
		FMath::Abs(LeanWidth - StressWidth) <= WidthTolerance);

	TestTrue(FString::Printf(TEXT("최악 콘텐츠에서 슬롯이 목록 밖으로 안 나간다 (slot %.1f <= container %.1f)"),
		StressSlotRight, StressContainerRight),
		StressSlotRight <= StressContainerRight + WidthTolerance);

	// 16:10 태블릿에서도 같은 폭이어야 한다 (가로 예산이 256px 좁다)
	double NarrowWidthMeasured = 0.0, NarrowSlotRight = 0.0, NarrowContainerRight = 0.0;
	if (Measure(NarrowWidth, true, TEXT("factory_stress_2304.png"),
		NarrowWidthMeasured, NarrowSlotRight, NarrowContainerRight))
	{
		TestTrue(FString::Printf(TEXT("2304 폭에서도 드로어 %.1f == %.0f"), NarrowWidthMeasured, DrawerWidth),
			FMath::Abs(NarrowWidthMeasured - DrawerWidth) <= WidthTolerance);
		TestTrue(FString::Printf(TEXT("드로어가 2304 가로 예산 안에 든다 (%.0f <= %d)"), DrawerWidth, NarrowWidth),
			DrawerWidth <= static_cast<double>(NarrowWidth));
	}

	UE_LOG(LogTemp, Display, TEXT("FactoryPanel: 렌더 산출물 = %s"), *Directory);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
