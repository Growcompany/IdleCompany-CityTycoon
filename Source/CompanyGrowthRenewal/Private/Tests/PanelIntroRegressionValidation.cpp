#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonActivatableWidget.h"
#include "Enum/WidgetType.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/HUD/MissionGuideOverlayWidget.h"
#include "UI/HUD/PanelIntroOverlayWidget.h"
#include "UI/UIBase.h"

/**
 * Help.IntroCheck ― 최초 진입 코치마크 회귀 (계획 T-A4 잔여분).
 *
 * "?" 에서 코치마크를 떼어낸 뒤, 코치마크의 유일한 소비자는 최초 진입이 됐다.
 * 그 경로는 T3 에서 PlayIntro 로 추출된 이후 한 번도 검증되지 않았고,
 * 이 리포에는 2026-08-08 M13 사고 이력이 있다 ― 인트로가 숨긴 미션 가이드 오버레이가
 * 복구되지 않아 화면이 딤인 채로 남았다.
 *
 * 그래서 묻는 것은 딱 그 계약이다:
 *   1. 안 본 패널은 재생된다
 *   2. 재생 중 미션 오버레이가 숨는다 (딤 2겹 방지)
 *   3. 끝나면 반드시 되돌아온다 ― Visible 이 아니라 HitTestInvisible 로
 *      (Visible 로 되돌리면 입력을 통과시키지 않아 화면을 먹는다)
 *   4. 본 패널은 다시 재생되지 않는다
 */
namespace
{
	UWorld* FindIntroCheckWorld()
	{
		const auto Usable = [](UWorld* InWorld)
		{
			return InWorld && (InWorld->WorldType == EWorldType::PIE || InWorld->WorldType == EWorldType::Game)
				&& InWorld->GetGameInstance() && InWorld->GetFirstPlayerController();
		};
		if (GEngine)
		{
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

	const TCHAR* VisibilityName(ESlateVisibility Visibility)
	{
		switch (Visibility)
		{
		case ESlateVisibility::Visible:             return TEXT("Visible");
		case ESlateVisibility::Collapsed:           return TEXT("Collapsed");
		case ESlateVisibility::Hidden:              return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:    return TEXT("HitTestInvisible");
		default:                                    return TEXT("SelfHitTestInvisible");
		}
	}
}

class FPanelIntroRegression : public TSharedFromThis<FPanelIntroRegression>
{
public:
	bool Start(UWorld* InWorld)
	{
		Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PanelIntroRegression"));
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);

		PlayWorld = InWorld;
		UGameInstance* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
		Intro = Instance ? Instance->GetSubsystem<UPanelIntroSubsystem>() : nullptr;
		UUIManagerSubsystem* UI = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
		UIBase = UI ? UI->GetUIBase() : nullptr;

		const bool bReady = Intro.IsValid() && UIBase.IsValid();
		Check(TEXT("world_ready"), bReady);
		return bReady;
	}

	bool Tick(float Delta)
	{
		Settle -= Delta;
		if (Settle > 0.f) { return true; }

		switch (Stage)
		{
		case 0: DoReset();      Stage = 1; Settle = 0.5f; return true;
		// 패널이 스스로 인트로를 켜기까지 스태거 타이머가 있다 ― 넉넉히 기다린다
		case 1: DoOpenPanel();  Stage = 2; Settle = 3.0f; return true;
		case 2: DoObserve();    Stage = 3; Settle = 0.5f; return true;
		case 3: DoFinish();     Stage = 4; Settle = 0.5f; return true;
		default: DoSeenGate(); Finish(); return false;
		}
	}

private:
	void Flush()
	{
		FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"),
			*FPaths::Combine(Directory, TEXT("result.txt")));
	}

	void Check(const FString& Name, bool bPassed)
	{
		Lines.Add(FString::Printf(TEXT("%s\t%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Name));
		if (!bPassed) { ++Failures; UE_LOG(LogTemp, Error, TEXT("[IntroCheck] FAIL %s"), *Name); }
		Flush();
	}

	UUserWidget* FindMissionOverlay() const
	{
		TArray<UUserWidget*> Found;
		if (UIBase.IsValid())
		{
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
				UIBase.Get(), Found, UMissionGuideOverlayWidget::StaticClass(), false);
		}
		return Found.Num() > 0 ? Found[0] : nullptr;
	}

	void DoReset()
	{
		// ResetAll 은 세이브를 쓴다 ― 이 하네스는 -saveddirsuffix 격리에서만 돌려야 한다
		Intro->ResetAll();
		Check(TEXT("reset_clears_seen"), !Intro->HasSeen(PanelKey));
		// 2026-09-24 부터 패널은 첫 진입에 스스로 안내를 띄우지 않는다(헤더 '?' 담당).
		// 이 하네스가 지키는 계약은 "자동 재생 여부"가 아니라 미션 가이드 복구라서,
		// 아래 DoOpenPanel 이 bIgnoreSeen=true 로 직접 재생해 그 경로를 그대로 탄다.
		Check(TEXT("autoplay_off_for_panel_keys"), !Intro->ShouldPlay(PanelKey));
	}

	void DoOpenPanel()
	{
		UUserWidget* Mission = FindMissionOverlay();
		bHadMissionOverlay = Mission != nullptr;

		// 실제 호출부를 탄다 ― PlayIntro 를 직접 부르면서 OwnerPanel 로 UIBase 를 넘기면
		// 앵커가 하나도 안 잡혀 스텝이 전부 건너뛰어지고, 그러면 복구 단언이 공허하게 통과한다
		UTableManagerSubsystem* TableMgr = PlayWorld.IsValid() && PlayWorld->GetGameInstance()
			? PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		TSubclassOf<UUserWidget> PanelClass = TableMgr
			? TableMgr->GetWidgetClass(EWidgetType::EmployeeWindow) : nullptr;
		Check(TEXT("panel_class_registered"), PanelClass != nullptr);
		if (!PanelClass || !UIBase.IsValid()) { return; }

		Host = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(PanelClass.Get()));
		Check(TEXT("panel_opened"), Host.IsValid());
	}

	void DoObserve()
	{
		// 패널이 더는 스스로 안 띄우므로 여기서 연다. OwnerPanel 은 반드시 Host(실제 패널) —
		// UIBase 를 넘기면 앵커가 하나도 안 잡혀 스텝이 건너뛰어지고 복구 단언이 공허해진다.
		if (Intro.IsValid() && Host.IsValid())
		{
			Intro->PlayIntro(Host.Get(), PanelKey, /*bIgnoreSeen=*/true);
		}

		TArray<UUserWidget*> Found;
		if (UIBase.IsValid())
		{
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
				UIBase.Get(), Found, UPanelIntroOverlayWidget::StaticClass(), false);
		}
		Overlay = Found.Num() > 0 ? Cast<UPanelIntroOverlayWidget>(Found[0]) : nullptr;
		Check(TEXT("intro_plays_when_requested"), Overlay.IsValid());

		UUserWidget* Mission = FindMissionOverlay();
		if (bHadMissionOverlay && Mission)
		{
			// 딤 2겹 방지 ― 인트로가 도는 동안 미션 가이드는 접혀 있어야 한다
			Check(FString::Printf(TEXT("mission_overlay_hidden_during_intro(%s)"),
				VisibilityName(Mission->GetVisibility())),
				Mission->GetVisibility() == ESlateVisibility::Collapsed);
		}
		else
		{
			Lines.Add(TEXT("SKIP\tmission_overlay_absent_in_this_world"));
			Flush();
		}
	}

	void DoFinish()
	{
		if (UPanelIntroOverlayWidget* Live = Overlay.Get())
		{
			// NativeDestruct 가 예외 경로에서도 복구 신호를 한 번 보장한다 ― 그 계약을 그대로 건드린다
			Live->RemoveFromParent();
		}
		UUserWidget* Mission = FindMissionOverlay();
		if (bHadMissionOverlay && Mission)
		{
			// M13 사고의 정확한 지점: 복구가 Visible 이면 입력을 먹고, Collapsed 면 영영 안 돌아온다
			Check(FString::Printf(TEXT("mission_overlay_restored_hittestinvisible(%s)"),
				VisibilityName(Mission->GetVisibility())),
				Mission->GetVisibility() == ESlateVisibility::HitTestInvisible);
		}
	}

	void DoSeenGate()
	{
		// MarkSeen 은 FinishIntro 에서만 일어난다. 이 하네스는 RemoveFromParent 로 끝내 NativeDestruct
		// 경로를 타므로(복구 신호는 나가지만 MarkSeen 은 안 탄다) 보통 여기로 온다 ―
		// 스텝을 끝까지 눌러 본 게 아니니 "봤다" 로 치지 않는 게 맞다
		if (Intro->HasSeen(PanelKey))
		{
			Check(TEXT("seen_panel_does_not_replay"),
				Intro->PlayIntro(Host.Get(), PanelKey, /*bIgnoreSeen=*/false) == nullptr);
		}
		else
		{
			Lines.Add(TEXT("SKIP\tseen_gate_untested_destruct_path_does_not_mark_seen"));
			Flush();
		}
	}

	void Finish()
	{
		Lines.Add(FString::Printf(TEXT("SUMMARY\t%d judged, %d failed"), Lines.Num(), Failures));
		Flush();
		UE_LOG(LogTemp, Display, TEXT("[IntroCheck] 완료 ― 실패 %d건. %s"), Failures, *Directory);
	}

	TWeakObjectPtr<UWorld> PlayWorld;
	TWeakObjectPtr<UPanelIntroSubsystem> Intro;
	TWeakObjectPtr<UUIBase> UIBase;
	TWeakObjectPtr<UCommonActivatableWidget> Host;
	TWeakObjectPtr<UPanelIntroOverlayWidget> Overlay;

	FString Directory;
	TArray<FString> Lines;
	FName PanelKey = TEXT("EmployeeWindow");
	int32 Failures = 0;
	int32 Stage = 0;
	float Settle = 1.0f;
	bool bHadMissionOverlay = false;
};

static TSharedPtr<FPanelIntroRegression> GIntroRegression;

static FAutoConsoleCommandWithWorldAndArgs GIntroCheck(
	TEXT("Help.IntroCheck"),
	TEXT("Verify the first-entry coachmark path still restores the mission guide overlay."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* CommandWorld)
	{
		if (GIntroRegression) { return; }
		TSharedRef<float> Waited = MakeShared<float>(0.f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Waited](float DeltaSeconds)
		{
			if (GIntroRegression) { return false; }
			*Waited += DeltaSeconds;
			UWorld* InWorld = FindIntroCheckWorld();
			if (!InWorld)
			{
				if (*Waited < 120.f) { return true; }
				UE_LOG(LogTemp, Error, TEXT("[IntroCheck] %.0fs 동안 월드 없음"), *Waited);
				return false;
			}
			TSharedRef<FPanelIntroRegression> Run = MakeShared<FPanelIntroRegression>();
			if (!Run->Start(InWorld))
			{
				if (*Waited < 120.f) { return true; }
				return false;
			}
			GIntroRegression = Run;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta)
			{
				const bool bContinue = Run->Tick(Delta);
				if (!bContinue) { GIntroRegression.Reset(); }
				return bContinue;
			}));
			return false;
		}));
	}));

#endif
