#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Panel/BuildingManagePanelWidget.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Building/TraitSlotWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Enum/BuildingTraitCategory.h"
#include "Enum/BuildingTraitTarget.h"
#include "Enum/WidgetType.h"
#include "AssetCompilingManager.h"
#include "Blueprint/UserWidget.h"
#include "CommonAnimatedSwitcher.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// 익명 ns 금지 ― 유니티 TU 병합 시 재정의 충돌 (CodexRailRenderValidation.cpp 와 같은 규약)
namespace TraitBandRender
{
	// 16:10 갤탭 S8(2304) 과 19.5:9 긴 폰(3120). 기준 2560 은 이 구간의 중간이라 판정에 안 쓴다
	static constexpr int32 CaptureWidths[] = { 2304, 3120 };
	static constexpr int32 CaptureHeight = 1440;
	// WBP SlotBodyBox 저작값 ― 잠금·빈칸·장착이 같은 자리·같은 크기여야 한다 (레퍼런스 §1.1-21)
	static constexpr double SocketSize = 156.0;
	// 빈칸 ＋ 시안 규격 ― 링 70, 막대 30×5, 둘의 중심이 같다
	static constexpr double EmptyRingSize = 70.0;
	static constexpr double PlusBarLength = 30.0;
	static constexpr double PlusBarThickness = 5.0;
	// 가격 알약 = 시안 높이 56 + 테두리 3·턱 4 를 판 안에 그린 위젯 높이
	static constexpr double PricePillHeight = 64.0;
	static constexpr double EdgeTolerance = 1.0;
	static constexpr double SizeTolerance = 0.5;

	struct FLayoutBox
	{
		bool bFound = false;
		double Left = 0.0, Top = 0.0, Width = 0.0, Height = 0.0;
		double Right() const { return Left + Width; }
		double Bottom() const { return Top + Height; }
	};

	enum class ESlotFixture : uint8 { Equipped, Empty, EmptyTarget, Locked };

	struct FSlotFixture
	{
		ESlotFixture Kind = ESlotFixture::Empty;
		FName TraitID = NAME_None;
		int32 Cost = 0;
	};

	struct FScenario
	{
		FString Label;
		TArray<FSlotFixture> Slots;
	};

	static FString RenderDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("TraitBandValidation"));
	}

	static FLayoutBox MeasureGeometry(const UWidget* Widget, FVector2D Origin)
	{
		FLayoutBox Out;
		if (Widget == nullptr || Widget->GetVisibility() == ESlateVisibility::Collapsed)
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

	static TSharedPtr<FJsonObject> BoxJson(const FLayoutBox& Box)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("found"), Box.bFound);
		Out->SetNumberField(TEXT("left"), Box.Left);
		Out->SetNumberField(TEXT("top"), Box.Top);
		Out->SetNumberField(TEXT("width"), Box.Width);
		Out->SetNumberField(TEXT("height"), Box.Height);
		return Out;
	}

	static bool DrawToImage(const TSharedRef<SWidget>& Widget, FIntPoint Size, FImage& OutImage)
	{
		FWidgetRenderer Renderer(true);
		// raw target(false) ― sRGB 타깃은 Slate 셰이더 감마와 겹쳐 화면이 뿌옇게 찍힌다
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, false));
		if (!Target.IsValid())
		{
			return false;
		}
		const FVector2D DrawSize(Size.X, Size.Y);
		// 첫 패스는 레이아웃만 세운다 ― 지오메트리가 잡힌 다음 패스라야 자식이 제자리 크기를 보고한다.
		// 시간은 흘리지 않는다 ― 대상 링 펄스가 선택 순간(최고점)으로 찍힌다
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			// 대상 건물 없이 띄운 패널은 틱에서 스스로 닫히며 라이브 카메라의 건물 이동까지 끝낸다 ― 패널 자신의 틱만 끈다.
			// UUserWidget::Invalidate 가 틱 여부를 다시 계산하므로 패스마다 끈다
			Widget->SetCanTick(false);
			Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static bool SaveCrop(const FImage& Image, const FLayoutBox& Box, int32 Pad, const FString& Path)
	{
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(Box.Left) - Pad, 0, Image.SizeX - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Box.Top) - Pad, 0, Image.SizeY - 1);
		const int32 X1 = FMath::Clamp(FMath::CeilToInt(Box.Right()) + Pad, X0 + 1, Image.SizeX);
		const int32 Y1 = FMath::Clamp(FMath::CeilToInt(Box.Bottom()) + Pad, Y0 + 1, Image.SizeY);
		FImage Crop(X1 - X0, Y1 - Y0, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const int64 RowBytes = int64(Crop.SizeX) * 4;
		for (int32 Y = 0; Y < Crop.SizeY; ++Y)
		{
			FMemory::Memcpy(Crop.RawData.GetData() + int64(Y) * RowBytes,
				Image.RawData.GetData() + (int64(Y0 + Y) * Image.SizeX + X0) * 4, RowBytes);
		}
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Crop) && FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	// 활성화되면 닫힐 때 NativeOnDeactivated 가 라이브 컨트롤러의 입력 모드를 바꾼다 ― 검증이 세션을 건드리면 안 된다
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

	static bool PickTrait(const UTableManagerSubsystem& Tables, EBuildingTraitCategory Category, bool bHighest, FBuildingTraitTableRow& OutRow)
	{
		const TArray<FBuildingTraitTableRow> Rows = Tables.GetBuildingTraitsByCategory(Category);
		if (Rows.Num() == 0)
		{
			return false;
		}
		OutRow = Rows[0];
		for (const FBuildingTraitTableRow& Row : Rows)
		{
			if (bHighest ? Row.Rarity > OutRow.Rarity : Row.Rarity < OutRow.Rarity)
			{
				OutRow = Row;
			}
		}
		return true;
	}

	static void ApplyFixture(UTraitSlotWidget& SlotWidget, const FSlotFixture& Fixture)
	{
		switch (Fixture.Kind)
		{
		case ESlotFixture::Equipped:    SlotWidget.SetEquipped(Fixture.TraitID); break;
		case ESlotFixture::Locked:      SlotWidget.SetLocked(Fixture.Cost); break;
		default:                        SlotWidget.SetEmpty(); break;
		}
		SlotWidget.SetSelected(Fixture.Kind == ESlotFixture::EmptyTarget);
	}

	static bool IsShown(const UUserWidget& Owner, const TCHAR* Name)
	{
		const UWidget* Widget = Owner.GetWidgetFromName(FName(Name));
		return Widget != nullptr && Widget->GetVisibility() != ESlateVisibility::Collapsed
			&& Widget->GetVisibility() != ESlateVisibility::Hidden;
	}
}

namespace TraitBandRender
{
	// PIE 우선 2패스 ― 둘 다 떠 있을 때 어느 쪽을 잡을지 확정한다. 로딩 맵은 준비 전으로 본다
	static UGameInstance* FindReadyGameInstance()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (Context.WorldType != Wanted || World == nullptr || !World->HasBegunPlay()
					|| World->GetMapName().Contains(TEXT("Loading")))
				{
					continue;
				}
				// 패널 NativeConstruct 가 메인맵 컨트롤러의 폰을 바로 꺼낸다 ― 없으면 기다린다(널 접근 방지)
				UCGGameInstance* GameInstance = Cast<UCGGameInstance>(World->GetGameInstance());
				const AMainMapPlayerController* Controller = GameInstance != nullptr
					? Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController()) : nullptr;
				if (Controller != nullptr && Controller->GetPawn() != nullptr
					&& GameInstance->GetSubsystem<UTableManagerSubsystem>() != nullptr)
				{
					return GameInstance;
				}
			}
		}
		return nullptr;
	}

	static void RunBandRender(FAutomationTestBase& T, UGameInstance* GameInstance);

	// -ExecCmds 는 맵 로드 전에 발사된다 ― 월드가 서고 몇 초 지난 뒤에 그린다 (최대 120초 대기)
	class FTraitBandRenderCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FTraitBandRenderCommand(FAutomationTestBase* InTest)
			: Test(InTest), StartTime(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			UGameInstance* GameInstance = FindReadyGameInstance();
			if (GameInstance == nullptr)
			{
				if (Now - StartTime < 120.0)
				{
					return false;
				}
				Test->AddError(TEXT("120초 안에 플레이 월드가 서지 않았습니다 ― -game 스탠드얼론이나 PIE 에서 실행해야 합니다"));
				return true;
			}
			if (ReadySince < 0.0)
			{
				ReadySince = Now;
			}
			if (Now - ReadySince < 5.0)
			{
				return false;
			}
			FAssetCompilingManager::Get().FinishAllCompilation();
			if (GShaderCompilingManager != nullptr)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			FlushRenderingCommands();
			RunBandRender(*Test, GameInstance);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartTime;
		double ReadySince = -1.0;
	};
}

// ApplicationContextMask ― -game 스탠드얼론에서도 돈다 (CGR.Codex.RailRender 와 같은 이유)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTraitBandRenderTest, "CGR.Trait.BandRender",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTraitBandRenderTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(TraitBandRender::FTraitBandRenderCommand(this));
	return true;
}

void TraitBandRender::RunBandRender(FAutomationTestBase& T, UGameInstance* GameInstance)
{
	UTableManagerSubsystem* Tables = GameInstance->GetSubsystem<UTableManagerSubsystem>();
	if (Tables == nullptr)
	{
		T.AddError(TEXT("TableManager 를 읽지 못했습니다"));
		return;
	}
	const TSubclassOf<UUserWidget> PanelClass = Tables->GetWidgetClass(EWidgetType::BuildingManage);
	if (!PanelClass)
	{
		T.AddError(TEXT("DT_WidgetClass 에 BuildingManage 행이 없습니다"));
		return;
	}

	// 실데이터에서 고른다 ― 이름이 가장 긴 효과 대상(위험·마인드셋)을 한 화면에 몰아 캡션 넘침을 본다
	FBuildingTraitTableRow RevenueTop, RevenueLow, RiskTop, MindsetTop;
	if (!PickTrait(*Tables, EBuildingTraitCategory::Revenue, true, RevenueTop)
		|| !PickTrait(*Tables, EBuildingTraitCategory::Revenue, false, RevenueLow)
		|| !PickTrait(*Tables, EBuildingTraitCategory::Risk, true, RiskTop)
		|| !PickTrait(*Tables, EBuildingTraitCategory::Mindset, true, MindsetTop))
	{
		T.AddError(TEXT("DT_BuildingTrait 에 수익·위험·마인드셋 행이 없습니다 ― 픽스처를 못 만듭니다"));
		return;
	}

	const TArray<FScenario> Scenarios = {
		{ TEXT("mid"),    { { ESlotFixture::Equipped, RevenueTop.TraitID }, { ESlotFixture::EmptyTarget }, { ESlotFixture::Locked, NAME_None, 450 } } },
		{ TEXT("stress"), { { ESlotFixture::Equipped, RevenueTop.TraitID }, { ESlotFixture::Equipped, RiskTop.TraitID }, { ESlotFixture::Equipped, MindsetTop.TraitID } } },
		{ TEXT("small"),  { { ESlotFixture::Equipped, RevenueLow.TraitID }, { ESlotFixture::Locked, NAME_None, 150 }, { ESlotFixture::Locked, NAME_None, 450 } } },
	};

	const FString Directory = RenderDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();

	// 첫 드로우는 재질·폰트 예열 전이라 판정에 쓰지 않는다 ― 장착 상태까지 한 판 그려 버린다
	auto PreparePanel = [&](TStrongObjectPtr<UBuildingManagePanelWidget>& Panel, TSharedPtr<SWidget>& OutSlate, TArray<UTraitSlotWidget*>& OutSlots) -> bool
	{
		Panel.Reset(CreateWidget<UBuildingManagePanelWidget>(GameInstance, PanelClass));
		if (!Panel.IsValid() || !DisableAutoActivate(Panel.Get()))
		{
			return false;
		}
		// UUserWidget 은 Slate 위젯을 약참조로만 든다 ― 여기서 안 붙들면 즉시 해제돼 NativeDestruct 가 탭 바인딩을 끊고,
		// 그릴 때 다시 지어지며 강화 탭으로 초기화된다
		OutSlate = Panel->TakeWidget();
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Panel->GetWidgetFromName(TEXT("ContentSwitcher")));
		if (UCommonAnimatedSwitcher* Animated = Cast<UCommonAnimatedSwitcher>(Switcher))
		{
			Animated->SetDisableTransitionAnimation(true);
		}
		// 탭은 실제 버튼 그룹으로 넘긴다 ― 스위처만 밀면 패널이 0번으로 되돌린다(CGR.Codex.RailRender 와 같은 경로)
		const FObjectPropertyBase* GroupProperty = FindFProperty<FObjectPropertyBase>(Panel->GetClass(), TEXT("TabButtonGroup"));
		UCommonButtonGroupBase* TabGroup = GroupProperty != nullptr
			? Cast<UCommonButtonGroupBase>(GroupProperty->GetObjectPropertyValue_InContainer(Panel.Get())) : nullptr;
		if (TabGroup == nullptr || Switcher == nullptr)
		{
			return false;
		}
		TabGroup->SelectButtonAtIndex(static_cast<int32>(EBuildingManageTab::Trait));
		OutSlots.Reset();
		if (UPanelWidget* Row = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("TraitSlotRow"))))
		{
			for (UWidget* Child : Row->GetAllChildren())
			{
				if (UTraitSlotWidget* SlotWidget = Cast<UTraitSlotWidget>(Child))
				{
					OutSlots.Add(SlotWidget);
				}
			}
		}
		return true;
	};

	// 처음 쓰는 재질·텍스처는 그리기가 컴파일을 요청한다 ― 모든 상황을 한 번씩 그린 뒤 컴파일을 끝내야 본 촬영에 나온다
	for (const FScenario& Scenario : Scenarios)
	{
		TStrongObjectPtr<UBuildingManagePanelWidget> Warmup;
		TSharedPtr<SWidget> WarmupSlate;
		TArray<UTraitSlotWidget*> WarmupSlots;
		if (PreparePanel(Warmup, WarmupSlate, WarmupSlots))
		{
			for (int32 i = 0; i < WarmupSlots.Num() && i < Scenario.Slots.Num(); ++i)
			{
				ApplyFixture(*WarmupSlots[i], Scenario.Slots[i]);
			}
			FImage Discarded;
			DrawToImage(WarmupSlate.ToSharedRef(), FIntPoint(CaptureWidths[0], CaptureHeight), Discarded);
		}
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	FlushRenderingCommands();

	for (const int32 CaptureWidth : CaptureWidths)
	{
		for (const FScenario& Scenario : Scenarios)
		{
			const FString Label = FString::Printf(TEXT("%d %s"), CaptureWidth, *Scenario.Label);
			TStrongObjectPtr<UBuildingManagePanelWidget> Panel;
			TSharedPtr<SWidget> PanelSlate;
			TArray<UTraitSlotWidget*> Slots;
			if (!T.TestTrue(FString::Printf(TEXT("%s: 패널 생성·특성 탭 전환"), *Label), PreparePanel(Panel, PanelSlate, Slots)))
			{
				continue;
			}
			if (!T.TestEqual(FString::Printf(TEXT("%s: 슬롯 수"), *Label), Slots.Num(), Scenario.Slots.Num()))
			{
				continue;
			}
			// 「슬롯 N/3 해금」 문구는 칸 상태가 대신 말한다 ― 트리에 남아 있으면 안 된다
			T.TestTrue(FString::Printf(TEXT("%s: TraitSlotCountText 제거됨"), *Label),
				Panel->GetWidgetFromName(TEXT("TraitSlotCountText")) == nullptr);

			for (int32 i = 0; i < Slots.Num(); ++i)
			{
				ApplyFixture(*Slots[i], Scenario.Slots[i]);
			}

			// 비활성 페이지는 arrange 가 안 돼 측정이 0 이 된다 ― 그리기 전후로 특성 탭을 못 박는다
			const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Panel->GetWidgetFromName(TEXT("ContentSwitcher")));
			const int32 TraitTabIndex = static_cast<int32>(EBuildingManageTab::Trait);
			T.TestEqual(FString::Printf(TEXT("%s: 그리기 전 활성 탭"), *Label), Switcher ? Switcher->GetActiveWidgetIndex() : -1, TraitTabIndex);
			FImage Rendered;
			const FString FileName = FString::Printf(TEXT("trait_band_%d_%s.png"), CaptureWidth, *Scenario.Label);
			const bool bCaptured = DrawToImage(PanelSlate.ToSharedRef(), FIntPoint(CaptureWidth, CaptureHeight), Rendered);
			T.TestEqual(FString::Printf(TEXT("%s: 그린 뒤 활성 탭"), *Label), Switcher ? Switcher->GetActiveWidgetIndex() : -1, TraitTabIndex);
			T.TestTrue(FString::Printf(TEXT("%s: 캡처"), *Label), bCaptured);
			T.TestFalse(FString::Printf(TEXT("%s: 그리는 동안 패널이 스스로 닫히지 않음"), *Label), Panel->IsCloseRequested());

			const FVector2D Origin = Panel->GetRootWidget() != nullptr
				? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
			const FLayoutBox Band = MeasureGeometry(Panel->GetWidgetFromName(TEXT("EquippedBandBorder")), Origin);
			const FLayoutBox Row = MeasureGeometry(Panel->GetWidgetFromName(TEXT("TraitSlotRow")), Origin);
			// 측정이 실패해도 무엇이 그려졌는지 남긴다
			if (bCaptured)
			{
				TArray64<uint8> Bytes;
				FImageUtils::CompressImage(Bytes, TEXT("png"), Rendered);
				FFileHelper::SaveArrayToFile(Bytes, *(Directory / FileName));
			}
			TSharedPtr<FJsonObject> ScenarioJson = MakeShared<FJsonObject>();
			ScenarioJson->SetStringField(TEXT("image"), FileName);
			ScenarioJson->SetObjectField(TEXT("band"), BoxJson(Band));
			ScenarioJson->SetObjectField(TEXT("row"), BoxJson(Row));
			for (const TCHAR* Ancestor : { TEXT("TraitBorder"), TEXT("ContentSwitcher"), TEXT("RightColumn") })
			{
				ScenarioJson->SetObjectField(Ancestor, BoxJson(MeasureGeometry(Panel->GetWidgetFromName(FName(Ancestor)), Origin)));
			}
			if (Switcher != nullptr)
			{
				ScenarioJson->SetNumberField(TEXT("switcher_index"), Switcher->GetActiveWidgetIndex());
			}
			Report->SetObjectField(Label, ScenarioJson);
			// 이름이 사라지면 단락으로 초록이 되지 않게 존재와 0 아닌 크기부터 단언한다
			if (!T.TestTrue(FString::Printf(TEXT("%s: 밴드·슬롯 행 측정 (%.0fx%.0f / %.0fx%.0f)"), *Label, Band.Width, Band.Height, Row.Width, Row.Height),
				Band.bFound && Row.bFound && Band.Width > 0.0 && Band.Height > 0.0 && Row.Width > 0.0))
			{
				continue;
			}
			if (bCaptured)
			{
				SaveCrop(Rendered, Band, 40, Directory / FString::Printf(TEXT("trait_band_%d_%s_crop.png"), CaptureWidth, *Scenario.Label));
			}

			// 보유 특성 목록 ― 한 줄 카드가 스크롤바 밑으로 들어가면 오른쪽 카드가 잘린다(줄바꿈 폭 950 > 보이는 폭 912 였다)
			{
				const UScrollBox* Scroll = Cast<UScrollBox>(Panel->GetWidgetFromName(TEXT("CommonHierarchicalScrollBox")));
				const UPanelWidget* Cards = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("TraitCardContainer")));
				const FLayoutBox ScrollBox = MeasureGeometry(Scroll, Origin);
				if (T.TestTrue(FString::Printf(TEXT("%s: 보유 목록 측정"), *Label), Scroll != nullptr && Cards != nullptr && ScrollBox.bFound))
				{
					const FMargin BarPadding = Scroll->GetScrollbarPadding();
					const double ViewRight = ScrollBox.Right() - (Scroll->GetScrollbarThickness().X + BarPadding.Left + BarPadding.Right);
					int32 FirstRowCount = 0;
					double FirstRowTop = TNumericLimits<double>::Max();
					double WidestRight = 0.0;
					for (int32 c = 0; c < Cards->GetChildrenCount(); ++c)
					{
						const FLayoutBox Card = MeasureGeometry(Cards->GetChildAt(c), Origin);
						if (!Card.bFound)
						{
							continue;
						}
						WidestRight = FMath::Max(WidestRight, Card.Right());
						if (Card.Top < FirstRowTop - EdgeTolerance)
						{
							FirstRowTop = Card.Top;
							FirstRowCount = 0;
						}
						FirstRowCount += FMath::Abs(Card.Top - FirstRowTop) <= EdgeTolerance ? 1 : 0;
					}
					const int32 CardCount = Cards->GetChildrenCount();
					T.TestTrue(FString::Printf(TEXT("%s: 보유 카드 %d장 오른쪽 끝 %.1f <= 스크롤바 앞 %.1f"), *Label, CardCount, WidestRight, ViewRight),
						CardCount == 0 || WidestRight <= ViewRight + 0.01);
					T.TestEqual(FString::Printf(TEXT("%s: 보유 카드 첫 줄 장수 (총 %d장)"), *Label, CardCount), FirstRowCount, FMath::Min(CardCount, 5));
					ScenarioJson->SetObjectField(TEXT("owned_scroll"), BoxJson(ScrollBox));
					ScenarioJson->SetNumberField(TEXT("owned_view_right"), ViewRight);
					ScenarioJson->SetNumberField(TEXT("owned_widest_right"), WidestRight);
					ScenarioJson->SetNumberField(TEXT("owned_first_row"), FirstRowCount);
				}
			}
			TArray<TSharedPtr<FJsonValue>> SlotRecords;

			const double CellWidth = Row.Width / Slots.Num();
			for (int32 i = 0; i < Slots.Num(); ++i)
			{
				UTraitSlotWidget& SlotWidget = *Slots[i];
				const FSlotFixture& Fixture = Scenario.Slots[i];
				const FString SlotLabel = FString::Printf(TEXT("%s slot%d"), *Label, i);
				const double CellLeft = Row.Left + CellWidth * i;
				const double CellRight = CellLeft + CellWidth;

				const FLayoutBox Socket = MeasureGeometry(SlotWidget.GetWidgetFromName(TEXT("SlotBodyBox")), Origin);
				T.TestTrue(FString::Printf(TEXT("%s: 소켓 %.1fx%.1f == %.0f"), *SlotLabel, Socket.Width, Socket.Height, SocketSize),
					Socket.bFound && FMath::Abs(Socket.Width - SocketSize) <= SizeTolerance && FMath::Abs(Socket.Height - SocketSize) <= SizeTolerance);

				// 상태별로 보여야 할 층 ― 셋이 같은 소켓 자리를 나눠 쓴다
				const bool bEquipped = Fixture.Kind == ESlotFixture::Equipped;
				const bool bLocked = Fixture.Kind == ESlotFixture::Locked;
				const bool bEmpty = !bEquipped && !bLocked;
				T.TestTrue(FString::Printf(TEXT("%s: 장착 면 표시 (기대 %d)"), *SlotLabel, int32(bEquipped)), IsShown(SlotWidget, TEXT("EquippedFace")) == (bEquipped));
				T.TestTrue(FString::Printf(TEXT("%s: 빈 링 표시 (기대 %d)"), *SlotLabel, int32(bEmpty)), IsShown(SlotWidget, TEXT("EmptyRing")) == (bEmpty));
				T.TestTrue(FString::Printf(TEXT("%s: 자물쇠 표시 (기대 %d)"), *SlotLabel, int32(bLocked)), IsShown(SlotWidget, TEXT("LockOverlay")) == (bLocked));
				T.TestTrue(FString::Printf(TEXT("%s: 가격 칩 표시 (기대 %d)"), *SlotLabel, int32(bLocked)), IsShown(SlotWidget, TEXT("DiamondCostWidget")) == (bLocked));
				T.TestTrue(FString::Printf(TEXT("%s: 대상 링 표시 (기대 %d)"), *SlotLabel, int32(Fixture.Kind == ESlotFixture::EmptyTarget)), IsShown(SlotWidget, TEXT("SelectedBorder")) == (Fixture.Kind == ESlotFixture::EmptyTarget));
				T.TestTrue(FString::Printf(TEXT("%s: ＋ 표시 (기대 %d)"), *SlotLabel, int32(bEmpty)), IsShown(SlotWidget, TEXT("EmptyPlus")) == (bEmpty));
				T.TestTrue(FString::Printf(TEXT("%s: 자물쇠 그림자 표시 (기대 %d)"), *SlotLabel, int32(bLocked)), IsShown(SlotWidget, TEXT("LockShadow")) == (bLocked));

				if (bEmpty)
				{
					// 글자 ＋ 는 글꼴 기준선 때문에 링 중심보다 위로 뜨고 작았다 ― 막대 두 개의 크기와 중심을 링에 대어 잰다
					const FLayoutBox Ring = MeasureGeometry(SlotWidget.GetWidgetFromName(TEXT("EmptyRing")), Origin);
					const FLayoutBox BarH = MeasureGeometry(SlotWidget.GetWidgetFromName(TEXT("PlusBarH")), Origin);
					const FLayoutBox BarV = MeasureGeometry(SlotWidget.GetWidgetFromName(TEXT("PlusBarV")), Origin);
					auto CenterOf = [](const FLayoutBox& Box) { return FVector2D(Box.Left + Box.Width * 0.5, Box.Top + Box.Height * 0.5); };
					auto IsSize = [](const FLayoutBox& Box, double W, double H)
					{
						return Box.bFound && FMath::Abs(Box.Width - W) <= SizeTolerance && FMath::Abs(Box.Height - H) <= SizeTolerance;
					};
					T.TestTrue(FString::Printf(TEXT("%s: 빈 링 %.1fx%.1f == %.0f"), *SlotLabel, Ring.Width, Ring.Height, EmptyRingSize),
						IsSize(Ring, EmptyRingSize, EmptyRingSize));
					T.TestTrue(FString::Printf(TEXT("%s: ＋ 가로 막대 %.1fx%.1f == %.0fx%.0f"), *SlotLabel, BarH.Width, BarH.Height, PlusBarLength, PlusBarThickness),
						IsSize(BarH, PlusBarLength, PlusBarThickness));
					T.TestTrue(FString::Printf(TEXT("%s: ＋ 세로 막대 %.1fx%.1f == %.0fx%.0f"), *SlotLabel, BarV.Width, BarV.Height, PlusBarThickness, PlusBarLength),
						IsSize(BarV, PlusBarThickness, PlusBarLength));
					const FVector2D RingCenter = CenterOf(Ring);
					for (const FLayoutBox* Bar : { &BarH, &BarV })
					{
						const FVector2D Offset = CenterOf(*Bar) - RingCenter;
						T.TestTrue(FString::Printf(TEXT("%s: ＋ 막대 중심 어긋남 (%.2f, %.2f) <= %.1f"), *SlotLabel, Offset.X, Offset.Y, SizeTolerance),
							Bar->bFound && Ring.bFound && FMath::Abs(Offset.X) <= SizeTolerance && FMath::Abs(Offset.Y) <= SizeTolerance);
					}
				}

				// 캡션은 자기 칸(행 1/3) 안에 있어야 한다 ― 칸을 넘으면 옆 슬롯 글자와 겹친다
				TArray<const TCHAR*> CaptionNames;
				if (bEquipped) { CaptionNames = { TEXT("EffectNameText"), TEXT("EffectValueText") }; }
				else if (bLocked) { CaptionNames = { TEXT("DiamondCostWidget") }; }
				else { CaptionNames = { TEXT("SlotCaptionText") }; }

				TSharedPtr<FJsonObject> SlotJson = MakeShared<FJsonObject>();
				SlotJson->SetObjectField(TEXT("socket"), BoxJson(Socket));
				for (const TCHAR* Name : CaptionNames)
				{
					const FLayoutBox Caption = MeasureGeometry(SlotWidget.GetWidgetFromName(FName(Name)), Origin);
					SlotJson->SetObjectField(Name, BoxJson(Caption));
					T.TestTrue(FString::Printf(TEXT("%s: %s 크기 %.0fx%.0f > 0"), *SlotLabel, Name, Caption.Width, Caption.Height),
						Caption.bFound && Caption.Width > 0.0 && Caption.Height > 0.0);
					T.TestTrue(FString::Printf(TEXT("%s: %s [%.0f, %.0f] 이 칸 [%.0f, %.0f] 안"), *SlotLabel, Name, Caption.Left, Caption.Right(), CellLeft, CellRight),
						Caption.Left >= CellLeft - EdgeTolerance && Caption.Right() <= CellRight + EdgeTolerance);
					T.TestTrue(FString::Printf(TEXT("%s: %s 아래 %.0f <= 밴드 아래 %.0f"), *SlotLabel, Name, Caption.Bottom(), Band.Bottom()),
						Caption.Bottom() <= Band.Bottom() + EdgeTolerance);
					T.TestTrue(FString::Printf(TEXT("%s: %s 위 %.0f >= 소켓 아래 %.0f (겹침 0)"), *SlotLabel, Name, Caption.Top, Socket.Bottom()),
						Caption.Top >= Socket.Bottom() - EdgeTolerance);
				}

				if (bLocked)
				{
					// 시안 .price.poor = #FF7A7A. 공용 기본값(원색 빨강)은 다른 화면용이라 이 칩 인스턴스만 바꾼다
					const UResourceWidget* Cost = Cast<UResourceWidget>(SlotWidget.GetWidgetFromName(TEXT("DiamondCostWidget")));
					const FLayoutBox Pill = MeasureGeometry(Cost, Origin);
					T.TestTrue(FString::Printf(TEXT("%s: 가격 알약 높이 %.1f == %.0f"), *SlotLabel, Pill.Height, PricePillHeight),
						Pill.bFound && FMath::Abs(Pill.Height - PricePillHeight) <= SizeTolerance);
					const FLinearColor SoftWarn(FColor(0xFF, 0x7A, 0x7A));
					T.TestTrue(FString::Printf(TEXT("%s: 부족 색 = 부드러운 빨강"), *SlotLabel),
						Cost != nullptr && Cost->CannotAffordColor.Equals(SoftWarn, 0.01f));
					// 숫자를 감싼 ScaleBox 가 폭에 맞춰 늘이고 줄이면(ScaleToFitX) 내용 크기 알약에서 숫자가 작아진다 ― 배율 1, 보석과 8 띄움
					const UWidget* Icon = Cost ? Cost->GetWidgetFromName(TEXT("ResourceImage")) : nullptr;
					const UWidget* Amount = Cost ? Cost->GetWidgetFromName(TEXT("ResourceAmountText")) : nullptr;
					if (T.TestTrue(FString::Printf(TEXT("%s: 가격 보석·숫자 위젯"), *SlotLabel), Icon != nullptr && Amount != nullptr))
					{
						const FGeometry& AmountGeometry = Amount->GetCachedGeometry();
						const double AmountScale = AmountGeometry.GetLocalSize().X > 0.f
							? AmountGeometry.GetAbsoluteSize().X / AmountGeometry.GetLocalSize().X : 0.0;
						T.TestTrue(FString::Printf(TEXT("%s: 가격 숫자 배율 %.3f == 1"), *SlotLabel, AmountScale), FMath::Abs(AmountScale - 1.0) <= 0.01);
						const FLayoutBox IconBox = MeasureGeometry(Icon, Origin);
						const double Gap = FVector2D(AmountGeometry.GetAbsolutePosition()).X - Origin.X - IconBox.Right();
						T.TestTrue(FString::Printf(TEXT("%s: 보석→숫자 간격 %.1f == 8"), *SlotLabel, Gap), IconBox.bFound && FMath::Abs(Gap - 8.0) <= SizeTolerance);
					}
				}

				if (bEquipped)
				{
					FBuildingTraitTableRow Row_;
					if (T.TestTrue(FString::Printf(TEXT("%s: DT 행 조회"), *SlotLabel), Tables->GetBuildingTraitData(Fixture.TraitID, Row_)))
					{
						const EBuildingTraitTarget Target = GetTargetForCategory(Row_.Category);
						const UCommonTextBlock* NameText = Cast<UCommonTextBlock>(SlotWidget.GetWidgetFromName(TEXT("EffectNameText")));
						const UCommonTextBlock* ValueText = Cast<UCommonTextBlock>(SlotWidget.GetWidgetFromName(TEXT("EffectValueText")));
						const FString ExpectedName = GetTraitTargetDisplayName(Target).ToString();
						const FString ExpectedValue = FormatTraitEffectValueText(Target, Row_.BaseEffect).ToString();
						T.TestTrue(FString::Printf(TEXT("%s: 효과 이름 '%s'"), *SlotLabel, *ExpectedName),
							NameText != nullptr && !ExpectedName.IsEmpty() && NameText->GetText().ToString().Equals(ExpectedName, ESearchCase::CaseSensitive));
						T.TestTrue(FString::Printf(TEXT("%s: 효과 수치 '%s'"), *SlotLabel, *ExpectedValue),
							ValueText != nullptr && !ExpectedValue.IsEmpty() && ValueText->GetText().ToString().Equals(ExpectedValue, ESearchCase::CaseSensitive));
						SlotJson->SetStringField(TEXT("trait"), Fixture.TraitID.ToString());
						SlotJson->SetStringField(TEXT("effect"), ExpectedName + TEXT(" ") + ExpectedValue);
					}
				}
				SlotJson->SetNumberField(TEXT("index"), i);
				SlotRecords.Add(MakeShared<FJsonValueObject>(SlotJson));
			}

			ScenarioJson->SetArrayField(TEXT("slots"), SlotRecords);
		}
	}

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(Serialized, *(Directory / TEXT("trait_band_render.json")));
	UE_LOG(LogTemp, Display, TEXT("TraitBandRender: 증거 = %s"), *(Directory / TEXT("trait_band_render.json")));
}

#endif
