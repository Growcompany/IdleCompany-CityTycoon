#include "Misc/AutomationTest.h"
#include "ICommonInputModule.h"
#include "CommonInputSettings.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/Element/LiveOps/QuestRowWidget.h"
#include "UI/Element/LiveOps/MailRowWidget.h"
#include "UI/Element/LiveOps/NoticeRowWidget.h"
#include "UI/Element/LiveOps/EventBannerWidget.h"
#include "Blueprint/UserWidget.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ListView.h"
#include "Components/WidgetSwitcher.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

// 익명 ns 금지 — 유니티 TU 병합 시 재정의 충돌 (LiveOpsMenuTileRenderValidation.cpp 와 같은 규약).
// 그 파일과 심볼이 겹치지 않도록 네임스페이스를 따로 둔다.
namespace LiveOpsPanelLayout
{
	static const TCHAR* QuestPanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_QuestPanel.UI_QuestPanel_C");
	static const TCHAR* AttendancePanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_AttendancePanel.UI_AttendancePanel_C");
	static const TCHAR* MailboxPanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_MailboxPanel.UI_MailboxPanel_C");
	static const TCHAR* NewsPanelPath = TEXT("/Game/CompanyGrowth/UI/Panels/MainMap/UI_NewsPanel.UI_NewsPanel_C");

	// 캔버스 기준 해상도 (UI_STYLE_CATALOG.md §1 "2560x1440, DPI 1.0")
	static constexpr int32 ViewportWidth = 2560, ViewportHeight = 1440;

	// 셸 계약 (스펙 §7.3 + liveops_ui.modal_shell) — 하한이자 상한이다
	static constexpr double ShellWidth = 2160.0;
	static constexpr double ShellHeight = 1180.0;
	static constexpr double ShellTolerance = 1.0;

	// 출석은 아담한 판이다(설계 v5 §11) — 오른쪽 판 없이 격자 아래 한 줄. 판·격자·아래 줄 = AttendanceRender 와 같은 계약
	static constexpr double AttendanceShellWidth = 1580.0, AttendanceShellHeight = 1380.0;
	static constexpr double AttendanceGridWidth = 1500.0, AttendanceGridHeight = 972.0;
	static constexpr double AttendanceBottomRowWidth = 1500.0, AttendanceBottomRowHeight = 176.0;
	static constexpr double AttendanceBottomRowGap = 24.0;
	// 셸/격자 실측은 레이아웃 패스 반올림으로 1px 안팎 흔들린다 (R5)
	static constexpr double GridTolerance = 2.0;

	// 스트레스 픽스처 행 수 — 어느 화면도 셸 안에 다 안 들어가는 양이어야 스크롤 계약을 실제로 민다
	static constexpr int32 QuestRowCount = 30;
	static constexpr int32 LifetimeRowCount = 30;
	static constexpr int32 MailRowCount = 30;
	static constexpr int32 NoticeRowCount = 20;
	static constexpr int32 BannerRowCount = 12;
	static constexpr int32 MilestoneRowCount = 20;

	struct FLayoutBox
	{
		bool bFound = false;
		double Top = 0.0, Left = 0.0, Width = 0.0, Height = 0.0;
		double Bottom() const { return Top + Height; }
	};

	static FString RenderDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("LiveOpsUI/render"));
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

	static TSharedPtr<FJsonObject> BoxJson(const FLayoutBox& Box)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("found"), Box.bFound);
		Out->SetNumberField(TEXT("left"), Box.Left);
		Out->SetNumberField(TEXT("top"), Box.Top);
		Out->SetNumberField(TEXT("width"), Box.Width);
		Out->SetNumberField(TEXT("height"), Box.Height);
		Out->SetNumberField(TEXT("bottom"), Box.Bottom());
		return Out;
	}

	static bool SavePanelPNG(const FImage& Image, const FString& Path)
	{
		TArray64<uint8> Bytes;
		return FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *Path);
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
		// 첫 두 회는 레이아웃만 세운다 — ListView 는 지오메트리가 잡힌 다음 패스에서 엔트리를 만든다
		Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, 0.f);
		FlushRenderingCommands();
		Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, 0.f);
		FlushRenderingCommands();
		Renderer.DrawWidget(Target.Get(), Widget, 1.f, DrawSize, 0.f);
		FlushRenderingCommands();

		if (!FImageUtils::GetRenderTargetImage(Target.Get(), OutImage) || OutImage.Format != ERawImageFormat::BGRA8)
		{
			return false;
		}
		// FWidgetRenderer(true) 는 linear 로 태그된 타겟에 이미 감마 인코딩된 바이트를 써 둔다
		OutImage.GammaSpace = EGammaSpace::sRGB;
		return true;
	}

	static UListView* FindList(UUserWidget& Root, const TCHAR* Name)
	{
		return Cast<UListView>(Root.GetWidgetFromName(FName(Name)));
	}

	// 공용 창 틀 셸 면 — 내부 BindWidget 이라 공개 getter 가 없어 이름으로 찾는다
	static const UBorder* FindShellBorder(const UUserWidget& Panel)
	{
		const UUserWidget* Frame = Cast<UUserWidget>(Panel.GetWidgetFromName(FName(TEXT("ModalFrame"))));
		return Frame ? Cast<UBorder>(Frame->GetWidgetFromName(FName(TEXT("ShellBorder")))) : nullptr;
	}

	static FText SampleTitle(const TCHAR* Prefix, int32 Index)
	{
		return FText::FromString(FString::Printf(TEXT("%s 스트레스 항목 %02d"), Prefix, Index));
	}

	// 이벤트 상세 설명 스트레스 — 스크롤 상자가 없으면 AUTO 자식이 Fill 목록을 0 으로 밀고 판 밖으로 넘친다 (감사 A-3)
	static FText LongDescription(int32 Lines)
	{
		FString Out;
		for (int32 Index = 0; Index < Lines; ++Index)
		{
			Out += FString::Printf(TEXT("이벤트 설명 스트레스 %02d 줄 ― 길어지면 어디까지 밀리는지 본다.\n"), Index);
		}
		return FText::FromString(Out);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsPanelLayoutTest, "CGR.LiveOps.PanelLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FLiveOpsPanelLayoutTest::RunTest(const FString& Parameters)
{
	// 라이브옵스 창은 뒤로가기 처리 대상이라 생성 때 CommonInput 의 뒤로가기 액션을 찾는다 ― 게임 없이 만들면 입력 데이터가 아직 안 읽혀 ensure 가 난다
	ICommonInputModule::GetSettings().LoadData();
	using namespace LiveOpsPanelLayout;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World == nullptr)
	{
		AddError(TEXT("에디터 월드가 없습니다 ― 렌더 하네스를 세울 수 없습니다"));
		return false;
	}

	const FString Directory = RenderDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetNumberField(TEXT("viewport_width"), ViewportWidth);
	Report->SetNumberField(TEXT("viewport_height"), ViewportHeight);

	UE_LOG(LogTemp, Display,
		TEXT("PanelLayout: 셸(AppearSlideTarget)은 MinDesired 와 MaxDesired 가 같은 1180 이어야 한다. "
			 "상한이 없으면 ListView 가 desired 높이로 자라 스크롤 상태가 생기지 않고 창이 화면을 벗어난다"));

	// 아이템은 리스트의 UPROPERTY 가 잡지만, 측정 사이에 GC 가 도는 경우를 대비해 가드도 든다
	TArray<TStrongObjectPtr<UObject>> ItemGuards;

	// 렌더 하네스는 활성 타이머를 돌리지 않아 전이가 끝나지 않는다 — 껐다가 넘겨야 Slate 인덱스가 실제로 옮겨진다
	auto SelectTabForCapture = [&](UUserWidget& Panel, const TCHAR* Label, int32 TabIndex)
	{
		UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Panel.GetWidgetFromName(FName(TEXT("ContentSwitcher"))));
		if (Switcher == nullptr)
		{
			AddError(FString::Printf(TEXT("%s 에 ContentSwitcher 가 없다"), Label));
			return;
		}
		if (UCommonAnimatedSwitcher* AnimatedSwitcher = Cast<UCommonAnimatedSwitcher>(Switcher))
		{
			AnimatedSwitcher->SetDisableTransitionAnimation(true);
		}
		Switcher->SetActiveWidgetIndex(TabIndex);
		// 비활성 페이지는 arrange 가 안 돼 측정 단언이 공허하게 통과한다 — 캡처 전에 활성 인덱스를 못 박는다
		TestEqual(FString::Printf(TEXT("%s 캡처 직전 활성 탭"), Label), Switcher->GetActiveWidgetIndex(), TabIndex);
	};

	// 한 화면 = [패널 인스턴스 만들기 → 픽스처 주입 → 3패스 드로우 → 측정/단언 → PNG] 한 덩어리.
	// 람다로 묶어 5화면이 같은 단언을 쓰게 한다(화면마다 복붙하면 기준이 갈린다).
	auto RunPanel = [&](const TCHAR* ClassPath, const TCHAR* Label, const FString& PngName,
		TFunctionRef<void(UUserWidget&)> Fixture,
		TFunctionRef<void(UUserWidget&, const FLayoutBox&, FVector2D, const TSharedPtr<FJsonObject>&)> AssertFn,
		const FVector2D ExpectShell = FVector2D(ShellWidth, ShellHeight))
	{
		UClass* PanelClass = LoadClass<UUserWidget>(nullptr, ClassPath);
		if (PanelClass == nullptr)
		{
			AddError(FString::Printf(TEXT("%s 클래스를 못 읽었다: %s"), Label, ClassPath));
			return;
		}
		UUserWidget* Panel = CreateWidget<UUserWidget>(World, PanelClass);
		if (Panel == nullptr)
		{
			AddError(FString::Printf(TEXT("%s 인스턴스 생성 실패"), Label));
			return;
		}
		TStrongObjectPtr<UUserWidget> PanelGuard(Panel);

		// NativeConstruct 가 먼저 돌아야 BindWidget 이 채워진다 — 픽스처는 그 다음이다
		const TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
		Fixture(*Panel);

		FImage Image;
		const bool bCaptured = DrawToImage(PanelSlate, FIntPoint(ViewportWidth, ViewportHeight), Image)
			&& SavePanelPNG(Image, Directory / PngName);
		TestTrue(FString::Printf(TEXT("%s 캡처 저장 (%s)"), Label, *PngName), bCaptured);

		const FVector2D Origin = Panel->GetRootWidget()
			? FVector2D(Panel->GetRootWidget()->GetCachedGeometry().GetAbsolutePosition()) : FVector2D::ZeroVector;
		const FLayoutBox Shell = MeasureWidget(*Panel, TEXT("AppearSlideTarget"), Origin);

		TSharedPtr<FJsonObject> PanelJson = MakeShared<FJsonObject>();
		PanelJson->SetBoolField(TEXT("captured"), bCaptured);
		PanelJson->SetObjectField(TEXT("AppearSlideTarget"), BoxJson(Shell));

		// (1) 창 전체가 캔버스 안에 들어간다 — 사용자 신고("화면을 벗어난다")의 직접 단언
		const FVector2D Desired = Panel->GetDesiredSize();
		PanelJson->SetNumberField(TEXT("desired_width"), Desired.X);
		PanelJson->SetNumberField(TEXT("desired_height"), Desired.Y);
		TestTrue(FString::Printf(TEXT("%s desired 높이 %.0f <= %d"), Label, Desired.Y, ViewportHeight),
			Desired.Y <= static_cast<double>(ViewportHeight));
		TestTrue(FString::Printf(TEXT("%s desired 폭 %.0f <= %d"), Label, Desired.X, ViewportWidth),
			Desired.X <= static_cast<double>(ViewportWidth));

		// (2) 셸이 계약 치수에 묶여 있다 (상한이 없으면 여기서 부푼다)
		TestTrue(FString::Printf(TEXT("%s 셸 측정"), Label), Shell.bFound);
		if (Shell.bFound)
		{
			TestTrue(FString::Printf(TEXT("%s 셸 높이 %.0f (기대 %.0f +-%.0f)"), Label, Shell.Height, ExpectShell.Y, ShellTolerance),
				FMath::Abs(Shell.Height - ExpectShell.Y) <= ShellTolerance);
			TestTrue(FString::Printf(TEXT("%s 셸 폭 %.0f (기대 %.0f +-%.0f)"), Label, Shell.Width, ExpectShell.X, ShellTolerance),
				FMath::Abs(Shell.Width - ExpectShell.X) <= ShellTolerance);
		}

		AssertFn(*Panel, Shell, Origin, PanelJson);
		Report->SetObjectField(Label, PanelJson);
	};

	// 목록이 셸 안에 들어가고(= 판 밖으로 안 나감) 셸보다 낮다(= 스크롤이 생긴다)
	auto AssertScrollable = [&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin,
		const TSharedPtr<FJsonObject>& PanelJson, const TCHAR* ListName, const TCHAR* Label)
	{
		const FLayoutBox List = MeasureWidget(Panel, ListName, Origin);
		PanelJson->SetObjectField(ListName, BoxJson(List));
		if (!TestTrue(FString::Printf(TEXT("%s: %s 지오메트리 측정"), Label, ListName), List.bFound) || !Shell.bFound)
		{
			return;
		}
		TestTrue(FString::Printf(TEXT("%s: %s 높이 %.0f < 셸 %.0f (스크롤 가능)"), Label, ListName, List.Height, Shell.Height),
			List.Height < Shell.Height);
		TestTrue(FString::Printf(TEXT("%s: %s 하단 %.0f <= 셸 하단 %.0f (판 밖 없음)"), Label, ListName, List.Bottom(), Shell.Bottom()),
			List.Bottom() <= Shell.Bottom() + ShellTolerance);
	};

	// 푸터 CTA 가 셸 안에 남아 있어야 한다 — 목록이 자라면 제일 먼저 밀려나는 게 이 줄이다
	auto AssertInsideShell = [&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin,
		const TSharedPtr<FJsonObject>& PanelJson, const TCHAR* WidgetName, const TCHAR* Label)
	{
		const FLayoutBox Box = MeasureWidget(Panel, WidgetName, Origin);
		PanelJson->SetObjectField(WidgetName, BoxJson(Box));
		if (!Box.bFound)
		{
			// 조용히 빠지면 "잰 적 없는 계약"이 초록으로 남는다 — v2 에서 사라진 FootSize 가 그렇게 통과했다
			AddError(FString::Printf(TEXT("%s: %s 가 트리에 없습니다 ― 잴 수 없는 계약은 초록이 아닙니다"),
				Label, WidgetName));
			return;
		}
		if (!Shell.bFound)
		{
			return;
		}
		TestTrue(FString::Printf(TEXT("%s: %s 하단 %.0f <= 셸 하단 %.0f"), Label, WidgetName, Box.Bottom(), Shell.Bottom()),
			Box.Bottom() <= Shell.Bottom() + ShellTolerance);
	};

	// ---- 미션 (일일/주간 페이지 + 업적 페이지) ---------------------------------
	RunPanel(QuestPanelPath, TEXT("QuestPanel"), TEXT("panel_quest.png"),
		[&](UUserWidget& Panel)
		{
			auto FillQuest = [&](const TCHAR* ListName, int32 Count, const TCHAR* Prefix, bool bMilestone)
			{
				UListView* List = FindList(Panel, ListName);
				if (List == nullptr)
				{
					AddError(FString::Printf(TEXT("QuestPanel 에 %s 가 없다"), ListName));
					return;
				}
				TArray<UObject*> Items;
				Items.Reserve(Count);
				for (int32 Index = 0; Index < Count; ++Index)
				{
					UQuestRowItem* Item = NewObject<UQuestRowItem>(&Panel);
					Item->Entry.QuestID = FName(*FString::Printf(TEXT("Stress_%s_%02d"), Prefix, Index));
					Item->Entry.Row.Title = SampleTitle(Prefix, Index);
					Item->Entry.Current = 2;
					Item->Entry.Target = 5;
					Item->bMilestone = bMilestone;
					ItemGuards.Emplace(Item);
					Items.Add(Item);
				}
				List->SetListItems(Items);
			};
			FillQuest(TEXT("QuestList"), QuestRowCount, TEXT("일일"), false);
			FillQuest(TEXT("LifetimeList"), LifetimeRowCount, TEXT("업적"), false);
		},
		[&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin, const TSharedPtr<FJsonObject>& PanelJson)
		{
			AssertScrollable(Panel, Shell, Origin, PanelJson, TEXT("QuestList"), TEXT("QuestPanel"));
			AssertInsideShell(Panel, Shell, Origin, PanelJson, TEXT("FooterSize"), TEXT("QuestPanel"));
			AssertInsideShell(Panel, Shell, Origin, PanelJson, TEXT("SummaryBandSize"), TEXT("QuestPanel"));
		});

	// 업적 페이지는 스위처를 넘겨 따로 잰다 — 요약 줄이 없어 예산이 다르다
	RunPanel(QuestPanelPath, TEXT("QuestPanelLifetime"), TEXT("panel_quest_lifetime.png"),
		[&](UUserWidget& Panel)
		{
			SelectTabForCapture(Panel, TEXT("QuestPanelLifetime"), 1);
			UListView* List = FindList(Panel, TEXT("LifetimeList"));
			if (List == nullptr)
			{
				AddError(TEXT("QuestPanel 에 LifetimeList 가 없다"));
				return;
			}
			TArray<UObject*> Items;
			Items.Reserve(LifetimeRowCount);
			for (int32 Index = 0; Index < LifetimeRowCount; ++Index)
			{
				UQuestRowItem* Item = NewObject<UQuestRowItem>(&Panel);
				Item->Entry.QuestID = FName(*FString::Printf(TEXT("Stress_Lifetime_%02d"), Index));
				Item->Entry.Row.Title = SampleTitle(TEXT("업적"), Index);
				Item->Entry.Current = 120;
				Item->Entry.Target = 500;
				ItemGuards.Emplace(Item);
				Items.Add(Item);
			}
			List->SetListItems(Items);
		},
		[&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin, const TSharedPtr<FJsonObject>& PanelJson)
		{
			AssertScrollable(Panel, Shell, Origin, PanelJson, TEXT("LifetimeList"), TEXT("QuestPanelLifetime"));
			AssertInsideShell(Panel, Shell, Origin, PanelJson, TEXT("LifetimeFooterSize"), TEXT("QuestPanelLifetime"));
		});

	// 실데이터 4행(받기·진행 2·완료) — 스트레스 픽스처는 아이콘·보상·상태가 비어 시안과 대조할 수 없다
	RunPanel(QuestPanelPath, TEXT("QuestPanelReal"), TEXT("panel_quest_real.png"),
		[&](UUserWidget& Panel)
		{
			const UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CompanyGrowth/Table/LiveOps/DT_Quest.DT_Quest"));
			UListView* List = FindList(Panel, TEXT("QuestList"));
			if (Table == nullptr || List == nullptr)
			{
				AddError(TEXT("QuestPanelReal: DT_Quest 또는 QuestList 가 없다"));
				return;
			}
			TArray<TPair<FName, const FQuestTable*>> Daily;
			Table->ForeachRow<FQuestTable>(TEXT("QuestPanelReal"), [&Daily](const FName& Key, const FQuestTable& Row)
			{
				if (Row.Period == EQuestPeriod::Daily) Daily.Emplace(Key, &Row);
			});
			Daily.Sort([](const auto& A, const auto& B) { return A.Value->SortOrder < B.Value->SortOrder; });

			const EQuestState States[] = { EQuestState::Claimable, EQuestState::InProgress, EQuestState::InProgress, EQuestState::Claimed };
			const double Ratios[] = { 1.0, 0.6, 0.2, 1.0 };
			TArray<UObject*> Items;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(States) && Index < Daily.Num(); ++Index)
			{
				UQuestRowItem* Item = NewObject<UQuestRowItem>(&Panel);
				Item->Entry.QuestID = Daily[Index].Key;
				Item->Entry.Row = *Daily[Index].Value;
				Item->Entry.State = States[Index];
				Item->Entry.Target = FMath::Max<int64>(1, Item->Entry.Row.Target);
				Item->Entry.Current = static_cast<int64>(FMath::FloorToDouble(Item->Entry.Target * Ratios[Index]));
				ItemGuards.Emplace(Item);
				Items.Add(Item);
			}
			List->SetListItems(Items);
		},
		[&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin, const TSharedPtr<FJsonObject>& PanelJson)
		{
			AssertInsideShell(Panel, Shell, Origin, PanelJson, TEXT("SummaryBandSize"), TEXT("QuestPanelReal"));
		});

	// ---- 우편함 ----------------------------------------------------------------
	RunPanel(MailboxPanelPath, TEXT("MailboxPanel"), TEXT("panel_mailbox.png"),
		[&](UUserWidget& Panel)
		{
			UListView* List = FindList(Panel, TEXT("MailList"));
			if (List == nullptr)
			{
				AddError(TEXT("MailboxPanel 에 MailList 가 없다"));
				return;
			}
			TArray<UObject*> Items;
			Items.Reserve(MailRowCount);
			for (int32 Index = 0; Index < MailRowCount; ++Index)
			{
				UMailRowItem* Item = NewObject<UMailRowItem>(&Panel);
				Item->Entry.Id = FGuid::NewGuid();
				Item->Entry.Title = SampleTitle(TEXT("우편"), Index);
				Item->Entry.Body = FText::FromString(TEXT("스트레스 픽스처 본문입니다."));
				Item->bServerTimeValid = true;
				ItemGuards.Emplace(Item);
				Items.Add(Item);
			}
			List->SetListItems(Items);
		},
		[&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin, const TSharedPtr<FJsonObject>& PanelJson)
		{
			AssertScrollable(Panel, Shell, Origin, PanelJson, TEXT("MailList"), TEXT("MailboxPanel"));
			AssertInsideShell(Panel, Shell, Origin, PanelJson, TEXT("FooterSize"), TEXT("MailboxPanel"));
			// 셸 숨김(bHideShellSurface)은 출석만 켜는 옵션이다 — 기본값 창은 공용 셸 면을 그대로 그린다
			const UBorder* MailShell = FindShellBorder(Panel);
			if (TestNotNull(TEXT("MailboxPanel: ModalFrame ShellBorder"), MailShell))
			{
				TestTrue(FString::Printf(TEXT("MailboxPanel: 공용 셸 면 DrawAs %d (NoDraw 아님)"),
					static_cast<int32>(MailShell->Background.DrawAs)),
					MailShell->Background.DrawAs != ESlateBrushDrawType::NoDrawType);
			}
		});

	// ---- 소식 (이벤트 탭) -------------------------------------------------------
	RunPanel(NewsPanelPath, TEXT("NewsPanelEvent"), TEXT("panel_news_event.png"),
		[&](UUserWidget& Panel)
		{
			SelectTabForCapture(Panel, TEXT("NewsPanelEvent"), 0);
			if (UListView* Banners = FindList(Panel, TEXT("BannerList")))
			{
				TArray<UObject*> Items;
				Items.Reserve(BannerRowCount);
				for (int32 Index = 0; Index < BannerRowCount; ++Index)
				{
					UEventBannerItem* Item = NewObject<UEventBannerItem>(&Panel);
					Item->Def.Id = FString::Printf(TEXT("StressEvent_%02d"), Index);
					Item->Def.Title = SampleTitle(TEXT("이벤트"), Index);
					Item->bServerTimeValid = true;
					ItemGuards.Emplace(Item);
					Items.Add(Item);
				}
				Banners->SetListItems(Items);
			}
			else
			{
				AddError(TEXT("NewsPanel 에 BannerList 가 없다"));
			}
			if (UCommonTextBlock* Desc = Cast<UCommonTextBlock>(Panel.GetWidgetFromName(FName(TEXT("DetailDescText")))))
			{
				Desc->SetText(LongDescription(40));
			}
			if (UListView* Milestones = FindList(Panel, TEXT("MilestoneList")))
			{
				TArray<UObject*> Items;
				Items.Reserve(MilestoneRowCount);
				for (int32 Index = 0; Index < MilestoneRowCount; ++Index)
				{
					UQuestRowItem* Item = NewObject<UQuestRowItem>(&Panel);
					Item->Entry.Row.Title = SampleTitle(TEXT("마일스톤"), Index);
					Item->Entry.Current = 10;
					Item->Entry.Target = 30;
					Item->bMilestone = true;
					Item->MilestoneIndex = Index;
					ItemGuards.Emplace(Item);
					Items.Add(Item);
				}
				Milestones->SetListItems(Items);
			}
		},
		[&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin, const TSharedPtr<FJsonObject>& PanelJson)
		{
			AssertScrollable(Panel, Shell, Origin, PanelJson, TEXT("BannerList"), TEXT("NewsPanelEvent"));
			AssertScrollable(Panel, Shell, Origin, PanelJson, TEXT("MilestoneList"), TEXT("NewsPanelEvent"));
			// 40줄 설명을 넣은 상태의 단언 — 설명이 스크롤 상자에 담기면 밴드와 목록은 제자리에 남는다 (A-3)
			AssertScrollable(Panel, Shell, Origin, PanelJson, TEXT("EventDescScroll"), TEXT("NewsPanelEvent"));
			AssertInsideShell(Panel, Shell, Origin, PanelJson, TEXT("ProgressBandSize"), TEXT("NewsPanelEvent"));
		});

	// ---- 소식 (공지 탭) ---------------------------------------------------------
	RunPanel(NewsPanelPath, TEXT("NewsPanelNotice"), TEXT("panel_news_notice.png"),
		[&](UUserWidget& Panel)
		{
			SelectTabForCapture(Panel, TEXT("NewsPanelNotice"), 1);
			UListView* List = FindList(Panel, TEXT("NoticeList"));
			if (List == nullptr)
			{
				AddError(TEXT("NewsPanel 에 NoticeList 가 없다"));
				return;
			}
			TArray<UObject*> Items;
			Items.Reserve(NoticeRowCount);
			for (int32 Index = 0; Index < NoticeRowCount; ++Index)
			{
				UNoticeItem* Item = NewObject<UNoticeItem>(&Panel);
				Item->Def.Id = FString::Printf(TEXT("StressNotice_%02d"), Index);
				// 제목 2줄 상한을 실제로 밀어 본다 (행 141 은 하한이라 자란다)
				Item->Def.Title = FText::FromString(FString::Printf(
					TEXT("공지 스트레스 항목 %02d ― 제목이 길어 두 줄로 접히는 경우"), Index));
				Item->bRead = (Index % 2) == 0;
				ItemGuards.Emplace(Item);
				Items.Add(Item);
			}
			List->SetListItems(Items);
		},
		[&](UUserWidget& Panel, const FLayoutBox& Shell, FVector2D Origin, const TSharedPtr<FJsonObject>& PanelJson)
		{
			AssertScrollable(Panel, Shell, Origin, PanelJson, TEXT("NoticeList"), TEXT("NewsPanelNotice"));
			AssertInsideShell(Panel, Shell, Origin, PanelJson, TEXT("NoticeBodyScroll"), TEXT("NewsPanelNotice"));
		});

	// ---- 출석 (아담한 판 — 격자 아래 한 줄. 28칸은 C++ 가 채우므로 픽스처 주입 경로가 없다) ----
	// 칸 단위 계약(셀 피치·타일·도장)과 실물 아이콘 3프레임은 CGR.LiveOps.AttendanceRender 가 본다
	RunPanel(AttendancePanelPath, TEXT("AttendancePanel"), TEXT("panel_attendance.png"),
		[](UUserWidget&) {},
		[&](UUserWidget& Panel, const FLayoutBox&, FVector2D, const TSharedPtr<FJsonObject>& PanelJson)
		{
			// 판은 틀 묶음(FrameStack) 그리기 배율(0.82) 안에 있다 — 절대 위치(배율 반영)와 로컬 크기(미반영)를
			// 섞지 않게 틀 묶음 로컬에서 재고, 셸도 틀 묶음 자신으로 본다
			const UWidget* Stack = Panel.GetWidgetFromName(TEXT("FrameStack"));
			TestNotNull(TEXT("AttendancePanel: FrameStack"), Stack);
			auto MeasureInStack = [&Panel, Stack](const TCHAR* Name)
			{
				FLayoutBox Out;
				const UWidget* Widget = Panel.GetWidgetFromName(FName(Name));
				if (Widget == nullptr || Stack == nullptr)
				{
					return Out;
				}
				const FGeometry& Geometry = Widget->GetCachedGeometry();
				const FVector2D Local = Stack->GetCachedGeometry().AbsoluteToLocal(Geometry.GetAbsolutePosition());
				Out.bFound = true;
				Out.Left = Local.X;
				Out.Top = Local.Y;
				Out.Width = Geometry.GetLocalSize().X;
				Out.Height = Geometry.GetLocalSize().Y;
				return Out;
			};
			const FLayoutBox StackShell = MeasureInStack(TEXT("FrameStack"));
			const FLayoutBox Grid = MeasureInStack(TEXT("GridSize"));
			const FLayoutBox BottomRow = MeasureInStack(TEXT("BottomRow"));
			PanelJson->SetObjectField(TEXT("GridSize"), BoxJson(Grid));
			PanelJson->SetObjectField(TEXT("BottomRow"), BoxJson(BottomRow));
			// 이름이 사라지면 단락으로 초록이 되지 않게 "존재"부터 단언한다
			TestTrue(TEXT("AttendancePanel: GridSize 지오메트리 측정"), Grid.bFound);
			TestTrue(TEXT("AttendancePanel: BottomRow 지오메트리 측정"), BottomRow.bFound);
			if (Grid.bFound && StackShell.bFound)
			{
				TestTrue(FString::Printf(TEXT("AttendancePanel: 격자 %.0fx%.0f (기대 %.0fx%.0f +-%.0f)"),
					Grid.Width, Grid.Height, AttendanceGridWidth, AttendanceGridHeight, GridTolerance),
					FMath::Abs(Grid.Width - AttendanceGridWidth) <= GridTolerance
					&& FMath::Abs(Grid.Height - AttendanceGridHeight) <= GridTolerance);
			}
			// 아래 한 줄은 계약 치수다 — 자라면 받기 버튼이 판 아래로 밀린다
			if (BottomRow.bFound && Grid.bFound)
			{
				TestTrue(FString::Printf(TEXT("AttendancePanel: 아래 줄 %.0fx%.0f (기대 %.0fx%.0f +-%.0f)"),
					BottomRow.Width, BottomRow.Height, AttendanceBottomRowWidth, AttendanceBottomRowHeight, GridTolerance),
					FMath::Abs(BottomRow.Width - AttendanceBottomRowWidth) <= GridTolerance
					&& FMath::Abs(BottomRow.Height - AttendanceBottomRowHeight) <= GridTolerance);
				TestTrue(FString::Printf(TEXT("AttendancePanel: 격자-아래 줄 간격 %.0f (기대 %.0f +-%.0f)"),
					BottomRow.Top - Grid.Bottom(), AttendanceBottomRowGap, GridTolerance),
					FMath::Abs(BottomRow.Top - Grid.Bottom() - AttendanceBottomRowGap) <= GridTolerance);
			}
			for (const TCHAR* Name : { TEXT("GridSize"), TEXT("BottomRow"), TEXT("ClaimRowBox") })
			{
				const FLayoutBox Box = MeasureInStack(Name);
				PanelJson->SetObjectField(Name, BoxJson(Box));
				TestTrue(FString::Printf(TEXT("AttendancePanel: %s 하단 %.0f <= 셸 하단 %.0f · 오른쪽 %.0f <= %.0f"),
					Name, Box.Bottom(), StackShell.Bottom(), Box.Left + Box.Width, StackShell.Left + StackShell.Width),
					Box.bFound && StackShell.bFound && Box.Bottom() <= StackShell.Bottom() + ShellTolerance
					&& Box.Left + Box.Width <= StackShell.Left + StackShell.Width + ShellTolerance);
			}
			// 출석은 제 셸 그림(AttShell)을 깐다 — 공용 셸 면은 꺼져 있어야 두 판이 겹쳐 그려지지 않는다
			const UBorder* AttendanceShell = FindShellBorder(Panel);
			if (TestNotNull(TEXT("AttendancePanel: ModalFrame ShellBorder"), AttendanceShell))
			{
				TestTrue(FString::Printf(TEXT("AttendancePanel: 공용 셸 면 DrawAs %d (기대 NoDraw · bHideShellSurface)"),
					static_cast<int32>(AttendanceShell->Background.DrawAs)),
					AttendanceShell->Background.DrawAs == ESlateBrushDrawType::NoDrawType);
			}
		},
		FVector2D(AttendanceShellWidth, AttendanceShellHeight));

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(Serialized, *(Directory / TEXT("panel_layout.json")));

	UE_LOG(LogTemp, Display, TEXT("PanelLayout: 증거 = %s"), *(Directory / TEXT("panel_layout.json")));
	return true;
}

#endif
