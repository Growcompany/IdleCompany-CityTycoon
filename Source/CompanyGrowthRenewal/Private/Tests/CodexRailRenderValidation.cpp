#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Core/CGGameInstance.h"
#include "UI/Panel/CodexPanelWidget.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Data/GameSaveData.h"
#include "Data/ShippedRecord.h"
#include "Enum/CompanyType.h"
#include "Enum/WidgetType.h"
#include "Blueprint/UserWidget.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Groups/CommonButtonGroupBase.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 재정의 충돌 (LiveOpsPanelLayoutValidation.cpp 과 같은 규약)
namespace CodexRailRender
{
	// 16:10 갤탭 S8(2304) 과 19.5:9 긴 폰(3120) 의 가상 폭. 디자인 기준 2560 은 이 구간의 중간이라 판정에 안 쓴다
	static constexpr int32 CaptureWidths[] = { 2304, 3120 };
	static constexpr int32 CaptureHeight = 1440;

	// 미진출 포함 6행이 권위 ― 보유 수로 줄어들면 잠긴 산업이 사라진 것이다 (레퍼런스 §1.1-21)
	static constexpr int32 ExpectedRailTabs = 6;
	// 픽스처가 보유로 만드는 산업 = 게임(0) · IT(3). R2 이후 상호작용 여부로는 못 가르므로
	// 인덱스로 가른다 ― 이 값은 리포트 표기용이고 단언은 인덱스 쪽이 한다.
	static constexpr int32 ExpectedOwnedTabs = 2;

	// 레이아웃 패스 반올림으로 1px 안팎 흔들린다
	static constexpr double EdgeTolerance = 1.0;
	// 잠금 탭과 보유 탭은 같은 SizeBox 를 쓰므로 픽셀 단위로 같아야 한다
	static constexpr double SizeTolerance = 0.5;

	struct FLayoutBox
	{
		bool bFound = false;
		double Left = 0.0, Top = 0.0, Width = 0.0, Height = 0.0;
		double Right() const { return Left + Width; }
		double Bottom() const { return Top + Height; }
	};

	static FString RenderDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CodexUIValidation"));
	}

	static FLayoutBox MeasureGeometry(const UWidget* Widget, FVector2D Origin)
	{
		FLayoutBox Out;
		if (Widget == nullptr)
		{
			return Out;
		}
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D Position = FVector2D(Geometry.GetAbsolutePosition()) - Origin;
		const FVector2D Extent = FVector2D(Geometry.GetLocalSize());
		Out.bFound = true;
		Out.Left = Position.X;
		Out.Top = Position.Y;
		Out.Width = Extent.X;
		Out.Height = Extent.Y;
		return Out;
	}

	static FLayoutBox MeasureNamed(UUserWidget& Panel, const TCHAR* Name, FVector2D Origin)
	{
		return MeasureGeometry(Panel.GetWidgetFromName(FName(Name)), Origin);
	}

	static TSharedPtr<FJsonObject> BoxJson(const FLayoutBox& Box)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("found"), Box.bFound);
		Out->SetNumberField(TEXT("left"), Box.Left);
		Out->SetNumberField(TEXT("top"), Box.Top);
		Out->SetNumberField(TEXT("width"), Box.Width);
		Out->SetNumberField(TEXT("height"), Box.Height);
		Out->SetNumberField(TEXT("right"), Box.Right());
		Out->SetNumberField(TEXT("bottom"), Box.Bottom());
		return Out;
	}

	static bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);
		// raw target(false) ― true target 은 bForceLinearGamma=false 로 하드웨어 sRGB 쓰기를 켜서 Slate 셰이더 감마와 겹친다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		// 첫 패스는 레이아웃만 세운다 ― 지오메트리가 잡힌 다음 패스라야 자식이 제자리 크기를 보고한다
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, Pass == 0 ? 1.f : 0.f);
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

	// 활성화되면 닫힐 때 NativeOnDeactivated 가 라이브 PIE 컨트롤러의 입력 모드를 Normal 로 되돌린다 ― 검증이 세션을 건드리면 안 된다
	static bool DisableAutoActivate(UUserWidget* Widget)
	{
		FBoolProperty* AutoActivateProperty = FindFProperty<FBoolProperty>(Widget->GetClass(), TEXT("bAutoActivate"));
		if (AutoActivateProperty == nullptr)
		{
			return false;
		}
		AutoActivateProperty->SetPropertyValue_InContainer(Widget, false);
		return true;
	}

	static void AddOwnedBuilding(FGameSaveData& Save, int32 BuildingIndex, ECompanyType Industry, int32 Tier)
	{
		FBuildingEntitySaveData& Owned = Save.Buildings.AddDefaulted_GetRef();
		Owned.BuildingIndex = BuildingIndex;
		Owned.BuildingData.CompanyType = Industry;
		Save.OfficeDataMap.FindOrAdd(BuildingIndex).TierProgress.CurrentTier = Tier;
	}

	static void AddShipped(FGameSaveData& Save, ECompanyType Industry, const TCHAR* Name, int64 Revenue, int32 Review)
	{
		FShippedProjectRecord& Record = Save.ShippedProjects.AddDefaulted_GetRef();
		Record.Industry = Industry;
		Record.ProjectName = Name;
		Record.CumulativeRevenue = Revenue;
		Record.ReviewScore = Review;
	}

	// 보유 2(게임 2채 · IT 1채) + 미진출 4. 잠금 탭이 보유 탭과 같은 크기인지 보려면 두 상태가 한 화면에 있어야 한다.
	// 긴 산업명(반도체)을 잠금 쪽에 두어 라벨 길이가 크기를 흔드는지도 같은 판에서 걸린다.
	static FGameSaveData BuildFixtureSave()
	{
		FGameSaveData Fixture;
		AddOwnedBuilding(Fixture, 1, ECompanyType::Game, 3);
		AddOwnedBuilding(Fixture, 2, ECompanyType::Game, 5);
		AddOwnedBuilding(Fixture, 3, ECompanyType::IT, 1);
		Fixture.OfficeDataMap.FindOrAdd(1).TierProgress.ClearedProjects = { 21, 22 };
		Fixture.OfficeDataMap.FindOrAdd(2).TierProgress.ClearedProjects = { 22, 41 };

		AddShipped(Fixture, ECompanyType::Game, TEXT("워존"), 120000000, 31);
		AddShipped(Fixture, ECompanyType::Game, TEXT("섀도우 에이전트"), 64000000, 30);
		AddShipped(Fixture, ECompanyType::Game, TEXT("던전크롤러"), 39000000, 28);
		AddShipped(Fixture, ECompanyType::Game, TEXT("이름이 아주 길어서 한 줄을 넘기는 출시작 제목"), 12000000, 19);
		AddShipped(Fixture, ECompanyType::IT, TEXT("사내 메신저"), 31000000, 22);
		return Fixture;
	}
}

// ApplicationContextMask ― EditorContext 만 주면 -game 스탠드얼론에서 필터링돼 아예 안 돌고,
// 그러면 사람이 Alt+P 로 PIE 를 띄워줘야만 검증이 된다 (GachaUI.ValidateSharedBase 와 같은 이유)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexRailRenderTest, "CGR.Codex.RailRender",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCodexRailRenderTest::RunTest(const FString& Parameters)
{
	using namespace CodexRailRender;

	// 사람 손 없이 돌 수 있게 -game 스탠드얼론의 Game 월드도 받는다.
	// 단순 OR 이 아니라 PIE 우선 2패스 ― 둘 다 떠 있을 때 어느 쪽을 잡을지 확정한다
	// (EmployeeEquipmentUIValidation.cpp:2634 와 같은 패턴)
	UWorld* PlayWorld = nullptr;
	if (GEngine != nullptr)
	{
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == Wanted && Context.World() != nullptr)
				{
					PlayWorld = Context.World();
					break;
				}
			}
			if (PlayWorld != nullptr)
			{
				break;
			}
		}
	}
	UGameInstance* GameInstance = PlayWorld != nullptr ? PlayWorld->GetGameInstance() : nullptr;
	if (GameInstance == nullptr)
	{
		// 레일은 SaveLoadManager · TableManager 를 읽어 채워지므로 게임 인스턴스 없이는 6행이 서지 않는다.
		// 빈 에디터(커맨드렛)에서는 여기서 멈추는 게 맞다 ― 플레이 월드를 띄우고 돌릴 것.
		AddError(TEXT("플레이 월드가 없습니다 ― -game 스탠드얼론으로 띄우거나 PIE 를 시작한 뒤 실행해야 합니다"));
		return false;
	}

	UTableManagerSubsystem* TableManager = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	USaveLoadManager* SaveManager = GameInstance->GetSubsystem<USaveLoadManager>();
	USaveGame_GameData* SaveObject = SaveManager != nullptr ? SaveManager->GetCurrentSaveData() : nullptr;
	if (TableManager == nullptr || SaveObject == nullptr)
	{
		AddError(TEXT("TableManager 또는 세이브 데이터를 읽지 못했습니다"));
		return false;
	}

	const TSubclassOf<UUserWidget> PanelClass = TableManager->GetWidgetClass(EWidgetType::CodexPanel);
	if (!PanelClass)
	{
		AddError(TEXT("DT_WidgetClass 에 CodexPanel 행이 없습니다"));
		return false;
	}

	// 라이브 세이브를 픽스처로 잠시 덮는다. 같은 함수 안에서 되돌리므로 틱·지연 저장 타이머가 끼어들 틈이 없고
	// 디스크 저장(SaveGameData)은 한 번도 부르지 않는다.
	const FGameSaveData OriginalSave = SaveObject->GameData;
	UCGGameInstance* CGInstance = Cast<UCGGameInstance>(GameInstance);
	const ECompanyType OriginalBuildingIndustry = CGInstance != nullptr
		? CGInstance->GetCurrentBuildingCompanyType() : ECompanyType::None;
	ON_SCOPE_EXIT
	{
		SaveObject->GameData = OriginalSave;
		if (CGInstance != nullptr)
		{
			CGInstance->SetCurrentBuildingCompanyType(OriginalBuildingIndustry);
		}
	};
	SaveObject->GameData = BuildFixtureSave();
	// 초기 선택을 레일 폴백(첫 보유 산업)으로 고정한다 ― 현재 건물 산업이 남아 있으면 실행마다 첫 탭이 달라진다
	if (CGInstance != nullptr)
	{
		CGInstance->SetCurrentBuildingCompanyType(ECompanyType::None);
	}

	const FString Directory = RenderDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetNumberField(TEXT("expected_rail_tabs"), ExpectedRailTabs);
	Report->SetNumberField(TEXT("expected_owned_tabs"), ExpectedOwnedTabs);

	// 에디터를 갓 띄운 세션의 첫 드로우는 머티리얼·폰트 워밍업 전이라 판정에 쓰지 않는다 ― 버리는 한 판을 먼저 그린다
	{
		TStrongObjectPtr<UCodexPanelWidget> WarmupPanel(CreateWidget<UCodexPanelWidget>(GameInstance, PanelClass));
		if (WarmupPanel.IsValid() && DisableAutoActivate(WarmupPanel.Get()))
		{
			FImage Discarded;
			DrawToImage(WarmupPanel->TakeWidget(), FIntPoint(CaptureWidths[0], CaptureHeight), Discarded);
		}
	}

	for (const int32 CaptureWidth : CaptureWidths)
	{
		const FString WidthLabel = FString::Printf(TEXT("%dx%d"), CaptureWidth, CaptureHeight);
		// UCodexPanelWidget 으로 받는다 ― DT_WidgetClass 행이 엉뚱한 WBP 를 가리키면 여기서 null 로 드러난다
		TStrongObjectPtr<UCodexPanelWidget> Panel(CreateWidget<UCodexPanelWidget>(GameInstance, PanelClass));
		if (!TestTrue(FString::Printf(TEXT("%s: 도감 패널 인스턴스 생성"), *WidthLabel), Panel.IsValid()))
		{
			continue;
		}
		if (!TestTrue(FString::Printf(TEXT("%s: bAutoActivate 를 끄고 구성"), *WidthLabel), DisableAutoActivate(Panel.Get())))
		{
			continue;
		}

		// TakeWidget 이 NativeConstruct 를 돌리며 레일을 세운다 ― 픽스처는 이미 위에서 깔아 뒀다
		const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UPanelWidget* RailBox = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("IndustryRailBox")));
		if (!TestTrue(FString::Printf(TEXT("%s: IndustryRailBox 가 트리에 있다"), *WidthLabel), RailBox != nullptr))
		{
			continue;
		}

		UCommonButtonGroupBase* TabGroup = nullptr;
		if (const FObjectPropertyBase* GroupProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("TabGroup")))
		{
			TabGroup = Cast<UCommonButtonGroupBase>(GroupProperty->GetObjectPropertyValue_InContainer(Panel.Get()));
		}
		UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Panel->GetWidgetFromName(TEXT("CodexSwitcher")));
		// FWidgetRenderer 는 활성 타이머를 돌리지 않는다 ― 전이가 남으면 Slate 인덱스가 이전 탭에 머문 채 찍힌다
		if (UCommonAnimatedSwitcher* AnimatedSwitcher = Cast<UCommonAnimatedSwitcher>(Switcher))
		{
			AnimatedSwitcher->SetDisableTransitionAnimation(true);
		}

		TSharedPtr<FJsonObject> WidthJson = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> PageRecords;

		for (int32 PageIndex = 0; PageIndex < 2; ++PageIndex)
		{
			// 본문 탭은 실제 버튼 그룹으로 넘긴다 ― 스위처만 밀면 탭 배선이 죽어도 초록이 된다
			if (TabGroup != nullptr)
			{
				TabGroup->SelectButtonAtIndex(PageIndex);
			}
			else if (Switcher != nullptr)
			{
				Switcher->SetActiveWidgetIndex(PageIndex);
			}

			const FString PageLabel = FString::Printf(TEXT("%s tab%d"), *WidthLabel, PageIndex);
			// 비활성 페이지는 arrange 가 안 돼 측정 단언이 공허하게 통과한다 ― 캡처 전에 활성 인덱스를 못 박는다
			if (Switcher != nullptr)
			{
				TestEqual(FString::Printf(TEXT("%s: 캡처 직전 활성 탭"), *PageLabel), Switcher->GetActiveWidgetIndex(), PageIndex);
			}
			FImage Rendered;
			const FString FileName = FString::Printf(TEXT("codex_rail_%d_tab%d.png"), CaptureWidth, PageIndex);
			const bool bCaptured = DrawToImage(PanelSlate, FIntPoint(CaptureWidth, CaptureHeight), Rendered)
				&& SavePNG(Rendered, Directory / FileName);
			TestTrue(FString::Printf(TEXT("%s: 캡처 저장 (%s)"), *PageLabel, *FileName), bCaptured);

			const FVector2D Origin = Panel->GetRootWidget() != nullptr
				? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
			const FLayoutBox Inner = MeasureNamed(*Panel, TEXT("PanelContent"), Origin);
			const FLayoutBox Rail = MeasureNamed(*Panel, TEXT("RailSize"), Origin);
			const FLayoutBox Body = MeasureNamed(*Panel, TEXT("ContentVBox"), Origin);

			TSharedPtr<FJsonObject> PageJson = MakeShared<FJsonObject>();
			PageJson->SetNumberField(TEXT("tab"), PageIndex);
			PageJson->SetStringField(TEXT("image"), FileName);
			PageJson->SetBoolField(TEXT("captured"), bCaptured);
			PageJson->SetNumberField(TEXT("active_tab"), Switcher != nullptr ? Switcher->GetActiveWidgetIndex() : -1);
			PageJson->SetObjectField(TEXT("PanelContent"), BoxJson(Inner));
			PageJson->SetObjectField(TEXT("RailSize"), BoxJson(Rail));
			PageJson->SetObjectField(TEXT("ContentVBox"), BoxJson(Body));
			if (Inner.bFound && Inner.Width > 0.0)
			{
				PageJson->SetNumberField(TEXT("rail_share_percent"), Rail.Width / Inner.Width * 100.0);
			}

			// 이름이 사라지면 단락으로 초록이 되지 않게 "존재"부터 단언한다
			TestTrue(FString::Printf(TEXT("%s: PanelContent 측정"), *PageLabel), Inner.bFound);
			TestTrue(FString::Printf(TEXT("%s: RailSize 측정"), *PageLabel), Rail.bFound);
			TestTrue(FString::Printf(TEXT("%s: ContentVBox 측정"), *PageLabel), Body.bFound);

			if (Rail.bFound && Body.bFound)
			{
				TestTrue(FString::Printf(TEXT("%s: 레일 우측 %.0f <= 본문 좌측 %.0f (겹침 0)"), *PageLabel, Rail.Right(), Body.Left),
					Rail.Right() <= Body.Left + EdgeTolerance);
			}
			if (Body.bFound && Inner.bFound)
			{
				TestTrue(FString::Printf(TEXT("%s: 본문 우측 %.0f <= 패널 안쪽 %.0f (가로 넘침 0)"), *PageLabel, Body.Right(), Inner.Right()),
					Body.Right() <= Inner.Right() + EdgeTolerance);
			}
			if (Rail.bFound && Inner.bFound)
			{
				TestTrue(FString::Printf(TEXT("%s: 레일 좌측 %.0f >= 패널 안쪽 %.0f"), *PageLabel, Rail.Left, Inner.Left),
					Rail.Left >= Inner.Left - EdgeTolerance);
			}

			PageRecords.Add(MakeShared<FJsonValueObject>(PageJson));
		}

		// 패널이 캔버스보다 넓으면 그게 곧 가로 스크롤이다 ― 좁은 16:10 에서 먼저 터진다
		const FVector2D Desired = Panel->GetDesiredSize();
		WidthJson->SetNumberField(TEXT("desired_width"), Desired.X);
		WidthJson->SetNumberField(TEXT("desired_height"), Desired.Y);
		TestTrue(FString::Printf(TEXT("%s: 패널 폭 %.0f <= 캔버스 %d"), *WidthLabel, Desired.X, CaptureWidth),
			Desired.X <= static_cast<double>(CaptureWidth) + EdgeTolerance);

		const FVector2D RailOrigin = Panel->GetRootWidget() != nullptr
			? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
		const TArray<UWidget*> RailTabs = RailBox->GetAllChildren();
		TestEqual(FString::Printf(TEXT("%s: 레일 탭 수"), *WidthLabel), RailTabs.Num(), ExpectedRailTabs);

		// R2 — 상호작용 가능 여부로는 보유/미진출을 더 이상 가를 수 없다(둘 다 열린다).
		// 보유는 픽스처가 아는 인덱스로 가른다: ECompanyType 선언 순서에서 게임=0, IT=3.
		int32 InteractiveCount = 0, RenderedCount = 0;
		FLayoutBox OwnedSample, LockedSample;
		TArray<TSharedPtr<FJsonValue>> TabRecords;
		for (int32 TabIndex = 0; TabIndex < RailTabs.Num(); ++TabIndex)
		{
			const UCommonButtonBase* TabButton = Cast<UCommonButtonBase>(RailTabs[TabIndex]);
			const FLayoutBox TabBox = MeasureGeometry(RailTabs[TabIndex], RailOrigin);
			const bool bInteractive = TabButton != nullptr && TabButton->IsInteractionEnabled();
			const bool bFixtureOwned = (TabIndex == 0 || TabIndex == 3);
			if (TabBox.bFound && TabBox.Width > 0.0 && TabBox.Height > 0.0)
			{
				++RenderedCount;
			}
			if (bInteractive) { ++InteractiveCount; }
			if (bFixtureOwned) { if (!OwnedSample.bFound) { OwnedSample = TabBox; } }
			else               { if (!LockedSample.bFound) { LockedSample = TabBox; } }

			TSharedPtr<FJsonObject> TabJson = BoxJson(TabBox);
			TabJson->SetNumberField(TEXT("index"), TabIndex);
			TabJson->SetBoolField(TEXT("interactive"), bInteractive);
			TabJson->SetBoolField(TEXT("fixture_owned"), bFixtureOwned);
			TabRecords.Add(MakeShared<FJsonValueObject>(TabJson));
		}
		WidthJson->SetArrayField(TEXT("rail_tabs"), TabRecords);
		WidthJson->SetArrayField(TEXT("pages"), PageRecords);
		WidthJson->SetNumberField(TEXT("interactive_tabs"), InteractiveCount);

		TestEqual(FString::Printf(TEXT("%s: 크기가 잡힌 레일 탭 수"), *WidthLabel), RenderedCount, ExpectedRailTabs);
		// R2 수용 기준 1 ― 죽은 탭 0. bIsSelectable 만 켜고 IsInteractionEnabled 를 안 켜면 여기서 걸린다.
		TestEqual(FString::Printf(TEXT("%s: 눌리는 탭 수(미진출 포함 전부)"), *WidthLabel),
			InteractiveCount, ExpectedRailTabs);

		// 미진출은 자리와 크기를 그대로 두고 덮기만 한다 (레퍼런스 §1.1-21)
		if (OwnedSample.bFound && LockedSample.bFound)
		{
			TestTrue(FString::Printf(TEXT("%s: 미진출 탭 %.1fx%.1f == 보유 탭 %.1fx%.1f"), *WidthLabel,
				LockedSample.Width, LockedSample.Height, OwnedSample.Width, OwnedSample.Height),
				FMath::Abs(LockedSample.Width - OwnedSample.Width) <= SizeTolerance
				&& FMath::Abs(LockedSample.Height - OwnedSample.Height) <= SizeTolerance);
		}
		else
		{
			AddError(FString::Printf(TEXT("%s: 보유/미진출 탭이 한 화면에 함께 없습니다 ― 크기 비교를 못 했습니다"), *WidthLabel));
		}

		Report->SetObjectField(WidthLabel, WidthJson);
	}

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(Serialized, *(Directory / TEXT("codex_rail_render.json")));

	UE_LOG(LogTemp, Display, TEXT("CodexRailRender: 증거 = %s"), *(Directory / TEXT("codex_rail_render.json")));
	return true;
}

#endif
