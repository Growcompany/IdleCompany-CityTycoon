#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Components/Widget.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "EngineUtils.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "UI/Element/Common/HelpButtonWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Panel/BuildingManagePanelWidget.h"
#include "UI/Panel/HelpTablePanelWidget.h"
#include "UI/UIBase.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

/**
 * Help.Validate ― 도움말 "?" 전수 검증 (계획 T-A4).
 *
 * PIE 가 아니라 스탠드얼론(-game)에서 돈다. 분류기가 PIE 시작을 막고, -game 도 원격 노드를
 * 띄우므로 사람 손 없이 전 과정을 돌릴 수 있다(reference_ue_remote_python 레시피).
 * FWidgetRenderer 가 실제 RHI 를 요구해 -nullrhi 헤드리스는 불가.
 *
 * 묻는 것은 세 가지다:
 *   1. 죽은 "?" 가 없는가        ― Mode != None
 *   2. 한 번 탭에 답이 나오는가  ― OpenHelp() 한 번 뒤 IsHelpOpen()
 *   3. 토글로 닫히는가           ― 두 번째 OpenHelp() 뒤 !IsHelpOpen()
 */
namespace
{
	struct FHelpPanelSpec
	{
		EWidgetType Type;
		const TCHAR* Label;
	};

	// 문맥 없이도 안전하게 열리는 패널을 앞에, 선택된 엔티티를 요구하는 패널을 뒤에 둔다.
	// 뒤쪽이 터져도 앞쪽 결과가 이미 manifest 에 적혀 있어야 하므로 순서가 증거 보존 순서다.
	const FHelpPanelSpec GPanels[] = {
		{ EWidgetType::AttendancePanel,   TEXT("Attendance")        },
		{ EWidgetType::MailboxPanel,      TEXT("Mailbox")           },
		{ EWidgetType::NewsPanel,         TEXT("News")              },
		{ EWidgetType::QuestPanel,        TEXT("Quest")             },
		{ EWidgetType::CodexPanel,        TEXT("Codex")             },
		{ EWidgetType::ProfileImagePanel, TEXT("ProfileImage")      },
		{ EWidgetType::BuildPanel,        TEXT("BuildModal")        },
		{ EWidgetType::CountryDetail,     TEXT("CountryDetail")     },
		{ EWidgetType::EmployeeWindow,    TEXT("EmployeeWindow")    },
		{ EWidgetType::PitchBoard,        TEXT("PitchBoard")        },
		{ EWidgetType::HQManagePanel,     TEXT("HQManage")          },
		{ EWidgetType::WorkstationInfo,   TEXT("WorkstationInfo")   },
		{ EWidgetType::BuildingManage,    TEXT("BuildingManage")    },
		{ EWidgetType::CityCompanyManage, TEXT("CityCompanyManage") },

		// 패널 헤더의 도움말 버튼 배치를 확인한다.
		{ EWidgetType::TraitGachaProbabilityPanel,    TEXT("TraitGachaProbability")    },
		{ EWidgetType::EmployeeGachaProbabilityPanel, TEXT("EmployeeGachaProbability") },
		{ EWidgetType::FactoryPanel,                  TEXT("Factory")                  },
		{ EWidgetType::ProductSellModal,              TEXT("ProductSell")              },
	};

	const TCHAR* ModeName(EHelpMode Mode)
	{
		switch (Mode)
		{
		case EHelpMode::Prose: return TEXT("Prose");
		case EHelpMode::Table: return TEXT("Table");
		case EHelpMode::Panel: return TEXT("Panel");
		default:               return TEXT("None");
		}
	}

	UWorld* FindHelpValidationWorld()
	{
		const auto Usable = [](UWorld* World)
		{
			return World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
				&& World->GetGameInstance() && World->GetFirstPlayerController();
		};
		if (GEngine)
		{
			// PIE 우선 ― 둘 다 떠 있으면 사람이 보고 있는 쪽이 PIE 다. 단순 OR 은 어느 쪽을 잡을지 불확정
			for (EWorldType::Type Wanted : { EWorldType::PIE, EWorldType::Game })
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == Wanted && Usable(Context.World())) { return Context.World(); }
				}
			}
		}
		return nullptr;
	}
}

class FHelpButtonValidation : public TSharedFromThis<FHelpButtonValidation>
{
public:
	bool Start(UWorld* InWorld)
	{
		PlayWorld = InWorld;
		Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HelpButtonValidation"));
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);

		UGameInstance* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
		UUIManagerSubsystem* UI = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
		Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		UIBase = UI ? UI->GetUIBase() : nullptr;

		Renderer = MakeUnique<FWidgetRenderer>(true);
		const bool bReady = UIBase.IsValid() && Tables.IsValid();
		Check(TEXT("world_ready"), bReady);
		return bReady;
	}

	bool Tick(float Delta)
	{
		if (!PlayWorld.IsValid() || !UIBase.IsValid()) { Finish(); return false; }
		if (Surface.IsValid() && Target.IsValid())
		{
			Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(CaptureWidth, 1440), Delta);
		}
		Settle -= Delta;
		if (Settle > 0.f) { return true; }

		switch (Stage)
		{
		case EStage::Open:      DoOpen();      break;
		case EStage::TapOnce:   DoTapOnce();   break;
		case EStage::TapAgain:  DoTapAgain();  break;
		case EStage::Close:     DoClose();     break;
		}

		// UE_ARRAY_COUNT 는 size_t ― 캐스트 없이 비교하면 C4018 이고 이 빌드는 경고가 곧 에러다
		if (PanelIndex >= static_cast<int32>(UE_ARRAY_COUNT(GPanels))) { Finish(); return false; }
		return true;
	}

private:
	enum class EStage : uint8 { Open, TapOnce, TapAgain, Close };

	void Flush()
	{
		// 뒤쪽 패널이 크래시해도 앞쪽 결과가 남아야 한다 ― 매 판정마다 디스크에 흘린다
		FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"),
			*FPaths::Combine(Directory, TEXT("result.txt")));
	}

	void Check(const FString& Name, bool bPassed)
	{
		Lines.Add(FString::Printf(TEXT("%s\t%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Name));
		if (!bPassed) { ++Failures; UE_LOG(LogTemp, Error, TEXT("[HelpValidate] FAIL %s"), *Name); }
		Flush();
	}

	// 판정이 아니라 사실 기록. 합계에 들어가지 않는다
	void Note(const FString& Name)
	{
		Lines.Add(FString::Printf(TEXT("SKIP\t%s"), *Name));
		++Skips;
		UE_LOG(LogTemp, Warning, TEXT("[HelpValidate] SKIP %s"), *Name);
		Flush();
	}

	const FHelpPanelSpec& Spec() const { return GPanels[PanelIndex]; }

	void CollectHelpButtons()
	{
		Buttons.Reset();
		if (!Panel.IsValid() || !Panel->WidgetTree) { return; }
		TArray<UWidget*> Widgets;
		Panel->WidgetTree->GetAllWidgets(Widgets);
		for (int32 Index = 0; Index < Widgets.Num(); ++Index)
		{
			UWidget* Widget = Widgets[Index];
			if (UHelpButtonWidget* Help = Cast<UHelpButtonWidget>(Widget))
			{
				if (Help->HelpKey.IsNone() && Help->GetVisibility() == ESlateVisibility::Collapsed
					&& Help->GetTypedOuter<UPanelHeaderWidget>())
				{
					Lines.Add(FString::Printf(TEXT("INFO\t%s_optional_header_help_disabled(%s)"), Spec().Label, *Help->GetPathName()));
					++Infos;
					Flush();
				}
				else { Buttons.Add(Help); }
			}
			// 엔진의 ForEachWidgetAndDescendants는 중첩 UserWidget 자체를 건너뛴다.
			// 도움말 인스턴스와 그 아래 트리를 모두 수집하고 NamedSlot 재방문은 제외한다.
			if (UUserWidget* Nested = Cast<UUserWidget>(Widget); Nested && Nested->WidgetTree)
			{
				Nested->WidgetTree->ForEachWidget([&Widgets](UWidget* Child) { Widgets.AddUnique(Child); });
			}
		}
	}

	void DoOpen()
	{
		const FString Label = Spec().Label;
		TSubclassOf<UUserWidget> Class = Tables->GetWidgetClass(Spec().Type);
		Check(Label + TEXT("_class_registered"), Class != nullptr);
		if (!Class) { Stage = EStage::Close; Settle = 0.f; return; }

		Panel = Cast<UCommonActivatableWidget>(
			UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Class.Get())));
		Check(Label + TEXT("_panel_created"), Panel.IsValid());
		if (!Panel.IsValid()) { Stage = EStage::Close; Settle = 0.f; return; }

		// 문맥을 요구하는 패널은 문맥을 준다 ― 안 주면 provider 가 표를 못 만들어 Prose 로 내려가고,
		// 그러면 정작 검증하려던 Table 경로를 한 번도 안 밟는다
		if (UBuildingManagePanelWidget* Manage = Cast<UBuildingManagePanelWidget>(Panel.Get()))
		{
			ABuildingBaseActor* First = nullptr;
			for (TActorIterator<ABuildingBaseActor> It(PlayWorld.Get()); It; ++It) { First = *It; break; }
			Check(Label + TEXT("_building_found"), First != nullptr);
			if (First) { Manage->SetTargetBuilding(First); }
		}

		CollectHelpButtons();
		Check(Label + TEXT("_help_button_present"), Buttons.Num() > 0);

		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			UHelpButtonWidget* Help = Buttons[Index].Get();
			if (!Help) { continue; }
			const EHelpMode Mode = Help->GetHelpMode();
			// 죽은 "?" 0 ― 13개 패널에서 조용히 아무 일도 안 하던 부품의 재발 방지 조항
			Check(FString::Printf(TEXT("%s_%d_mode_not_none(key=%s,mode=%s)"),
				*Label, Index, *Help->HelpKey.ToString(), ModeName(Mode)), Mode != EHelpMode::None);
			Check(FString::Printf(TEXT("%s_%d_visible"), *Label, Index),
				Help->GetVisibility() != ESlateVisibility::Collapsed);
		}

		Stage = EStage::TapOnce;
		Settle = 0.35f;
	}

	// 음수 패딩은 desired 에 드러나지 않는다 ― 프로퍼티 리드백 대신 배치된 기하를 묻는 이유
	void CheckExtents(const FString& Label, int32 Index, UHelpButtonWidget* Help)
	{
		const UWidget* Hit = Help->GetWidgetFromName(TEXT("HelpBtn"));
		const UWidget* Glyph = Help->GetWidgetFromName(TEXT("GlyphSize"));
		if (!Hit || !Glyph) { Note(FString::Printf(TEXT("%s_%d_extent_boxes_missing"), *Label, Index)); return; }

		const FVector2D HitSize = Hit->GetCachedGeometry().GetLocalSize();
		const FVector2D GlyphLocal = Glyph->GetCachedGeometry().GetLocalSize();
		// 소비처가 터치 영역을 더 작게 조일 수 있어 절대치 154 가 아니라 글리프와의 차로 본다
		const bool bSplit = GlyphLocal.X > 1.f && HitSize.X >= GlyphLocal.X + 40.f
			&& HitSize.Y >= GlyphLocal.Y + 40.f;
		Check(FString::Printf(TEXT("%s_%d_touch_wider_than_glyph(hit=%.0fx%.0f glyph=%.0fx%.0f)"),
			*Label, Index, HitSize.X, HitSize.Y, GlyphLocal.X, GlyphLocal.Y), bSplit);
	}

	void DoTapOnce()
	{
		const FString Label = Spec().Label;
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			UHelpButtonWidget* Help = Buttons[Index].Get();
			if (!Help) { continue; }
			CheckExtents(Label, Index, Help);
			Help->OpenHelp();
			// 모드는 탭 순간에 다시 정해진다(소유 패널 데이터가 구성 뒤에 들어오므로) ―
			// 탭 전 모드로 판정하면 재판정 결과를 영영 못 본다
			const EHelpMode After = Help->GetHelpMode();
			if (After == EHelpMode::Table)
			{
				TArray<UUserWidget*> OpenTables;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
					UIBase.Get(), OpenTables, UHelpTablePanelWidget::StaticClass(), false);
				UHelpTablePanelWidget* Opened = OpenTables.Num() > 0
					? Cast<UHelpTablePanelWidget>(OpenTables[0]) : nullptr;
				Check(FString::Printf(TEXT("%s_%d_table_opened"), *Label, Index), Opened != nullptr);
				// 창이 떴다는 것과 숫자가 그려졌다는 것은 별개다
				const int32 Rows = Opened ? Opened->GetRenderedRowCount() : 0;
				Check(FString::Printf(TEXT("%s_%d_table_has_rows(%d)"), *Label, Index, Rows), Rows > 0);
				// AddWidget의 전이가 끝나기 전에는 DeactivateWidget이 아무 일도 하지 않는다.
				OpenedHelpTable = Opened;
				continue;
			}
			if (After != EHelpMode::Prose) { continue; }
			// 한 번 탭 = 답 한 장. 두 번째 탭이 필요하면 이 작업의 목적이 무너진 것이다
			Check(FString::Printf(TEXT("%s_%d_opens_on_first_tap"), *Label, Index), Help->IsHelpOpen());
		}
		// 팝오버가 뜬 뒤에 캡처를 시작해야 PNG 에 말풍선이 담긴다
		BeginCapture();
		Stage = EStage::TapAgain;
		Settle = 0.5f;
	}

	void DoTapAgain()
	{
		const FString Label = Spec().Label;
		if (OpenedHelpTable.IsValid())
		{
			if (!bHelpTableCloseRequested)
			{
				Check(Label + TEXT("_table_active_before_close"), OpenedHelpTable->IsActivated()
					&& UIBase->GetActivePromptWidget() == OpenedHelpTable.Get());
				OpenedHelpTable->DeactivateWidget();
				bHelpTableCloseRequested = true;
				Settle = 0.5f;
				return;
			}
			Check(Label + TEXT("_table_closed"), !OpenedHelpTable->IsActivated()
				&& UIBase->GetActivePromptWidget() != OpenedHelpTable.Get());
			OpenedHelpTable.Reset();
			bHelpTableCloseRequested = false;
		}
		// 말풍선이 화면 밖으로 나가지 않는지 ― 눈이 아니라 좌표로 판정한다
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			UHelpButtonWidget* Help = Buttons[Index].Get();
			FVector2D TipPos, TipSize, Viewport;
			if (!Help || !Help->GetPopoverViewportRect(TipPos, TipSize, Viewport)) { continue; }
			const bool bInside = TipSize.X > 1.f && TipSize.Y > 1.f
				&& TipPos.X >= -0.5f && TipPos.Y >= -0.5f
				&& TipPos.X + TipSize.X <= Viewport.X + 0.5f
				&& TipPos.Y + TipSize.Y <= Viewport.Y + 0.5f;
			Check(FString::Printf(TEXT("%s_%d_popover_within_viewport(pos=%.0f,%.0f size=%.0fx%.0f vp=%.0fx%.0f)"),
				*Label, Index, TipPos.X, TipPos.Y, TipSize.X, TipSize.Y, Viewport.X, Viewport.Y), bInside);
		}
		SaveCapture();
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			UHelpButtonWidget* Help = Buttons[Index].Get();
			if (!Help || Help->GetHelpMode() != EHelpMode::Prose) { continue; }
			// "재탭이 닫는다" 는 열려 있다는 전제 위에 선다. 문맥 없이 연 패널이 매 프레임
			// 자기 back 핸들러를 돌면 그 사이 위젯이 재구성돼 팝오버가 이미 사라진다 ―
			// 그건 도움말 결함이 아니므로 실패로 적지 않고 건너뛴 사실을 남긴다
			if (!Help->IsHelpOpen())
			{
				Note(FString::Printf(TEXT("%s_%d_toggle_skipped_popover_gone_before_retap"), *Label, Index));
				continue;
			}
			Help->OpenHelp();
			Check(FString::Printf(TEXT("%s_%d_toggles_closed"), *Label, Index), !Help->IsHelpOpen());
		}
		Stage = EStage::Close;
		Settle = 0.2f;
	}

	void DoClose()
	{
		if (Panel.IsValid()) { Panel->DeactivateWidget(); }
		Panel.Reset();
		Buttons.Reset();
		++PanelIndex;
		Stage = EStage::Open;
		Settle = 0.35f;
	}

	void BeginCapture()
	{
		if (!Panel.IsValid()) { return; }
		Surface = SNew(SOverlay);
		Surface->AddSlot()[Panel->TakeWidget()];
		Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(CaptureWidth, 1440), TF_Bilinear, false));
	}

	void SaveCapture()
	{
		const FString Name = FString::Printf(TEXT("%s_%dx1440.png"), Spec().Label, CaptureWidth);
		FImage Image;
		TArray64<uint8> Bytes;
		bool bSaved = Target.IsValid() && FImageUtils::GetRenderTargetImage(Target.Get(), Image)
			&& Image.Format == ERawImageFormat::BGRA8;
		if (bSaved)
		{
			Image.GammaSpace = EGammaSpace::sRGB;
			bSaved = FImageUtils::CompressImage(Bytes, TEXT("png"), Image)
				&& FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, Name));
		}
		// 반환값이 아니라 산출물을 본다 ― 이 리포에서 "성공 반환 + 실제 실패" 가 반복해서 나왔다
		Check(Name + TEXT("_saved"), bSaved && Bytes.Num() > 0);
		Surface.Reset();
		Target.Reset();
	}

	void Finish()
	{
		Lines.Add(FString::Printf(TEXT("SUMMARY	%d judged, %d failed, %d skipped"), Lines.Num() - Skips - Infos, Failures, Skips));
		FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"),
			*FPaths::Combine(Directory, TEXT("result.txt")));
		UE_LOG(LogTemp, Display, TEXT("[HelpValidate] 완료 ― 실패 %d건. %s"), Failures, *Directory);
	}

	TWeakObjectPtr<UWorld> PlayWorld;
	TWeakObjectPtr<UUIBase> UIBase;
	TWeakObjectPtr<UTableManagerSubsystem> Tables;
	TWeakObjectPtr<UCommonActivatableWidget> Panel;
	TWeakObjectPtr<UHelpTablePanelWidget> OpenedHelpTable;
	TArray<TWeakObjectPtr<UHelpButtonWidget>> Buttons;
	bool bHelpTableCloseRequested = false;

	TUniquePtr<FWidgetRenderer> Renderer;
	TSharedPtr<SOverlay> Surface;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;

	FString Directory;
	TArray<FString> Lines;
	int32 Failures = 0;
	int32 Skips = 0;
	int32 Infos = 0;
	int32 PanelIndex = 0;
	int32 CaptureWidth = 2560;
	float Settle = 0.5f;
	EStage Stage = EStage::Open;
};

static TSharedPtr<FHelpButtonValidation> GHelpValidation;

static FAutoConsoleCommandWithWorldAndArgs GValidateHelpButtons(
	TEXT("Help.Validate"),
	TEXT("Validate every help \"?\" in a PIE or standalone game world. Writes Saved/HelpButtonValidation/result.txt."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* CommandWorld)
	{
		if (GHelpValidation) { UE_LOG(LogTemp, Warning, TEXT("[HelpValidate] 이미 실행 중")); return; }
		// -ExecCmds 는 LoadingMap 단계에서 발사돼 본 맵이 아직 없다. 준비될 때까지 기다린다
		TSharedRef<float> Waited = MakeShared<float>(0.f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Waited](float DeltaSeconds)
		{
			if (GHelpValidation) { return false; }
			*Waited += DeltaSeconds;
			UWorld* InWorld = FindHelpValidationWorld();
			if (!InWorld)
			{
				if (*Waited < 120.f) { return true; }
				UE_LOG(LogTemp, Error, TEXT("[HelpValidate] %.0fs 동안 월드 없음"), *Waited);
				return false;
			}
			TSharedRef<FHelpButtonValidation> Run = MakeShared<FHelpButtonValidation>();
			if (!Run->Start(InWorld))
			{
				if (*Waited < 120.f) { return true; }
				return false;
			}
			GHelpValidation = Run;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta)
			{
				const bool bContinue = Run->Tick(Delta);
				if (!bContinue) { GHelpValidation.Reset(); }
				return bContinue;
			}));
			return false;
		}));
	}));

#endif
