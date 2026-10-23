#include "GameMode/CGGameModeBase.h"

#include "Camera/PlayerCameraManager.h"
#include "Core/CGGameInstance.h"
#include "EngineUtils.h"
#include "Entity/Ambient/CityOpeningSequence.h"
#include "TimeCycle/TimeCycleManager.h"
#include "Entity/Factory/BrickFactory.h"
#include "Global/CGDevSettings.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/PanelIntroSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"
#include "UI/HUD/CityOpeningOverlayWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/UIBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Enum/WidgetType.h"
#include "MoviePlayer.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
    const FName CityOpeningSeenKey(TEXT("CityOpening"));
}

bool ACGGameModeBase::TryPrepareCityOpening()
{
    return PrepareCityOpeningPresentation(false);
}

#if !UE_BUILD_SHIPPING
bool ACGGameModeBase::PreviewCityOpening()
{
    return PrepareCityOpeningPresentation(true);
}
#endif

bool ACGGameModeBase::PrepareCityOpeningPresentation(bool bPreview)
{
    if (bCityOpeningActive || !GetWorld() || !GetGameInstance())
    {
        if (bPreview)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CityOpening] 미리보기 거부: Active=%d World=%s GameInstance=%s"),
                bCityOpeningActive, *GetNameSafe(GetWorld()), *GetNameSafe(GetGameInstance()));
        }
        return false;
    }
    UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>();
    UPanelIntroSubsystem* Intros = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>();
    UTableManagerSubsystem* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    AMainMapPlayerController* Controller = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
    APlayerCamera* PlayerCamera = Controller ? Cast<APlayerCamera>(Controller->GetPawn()) : nullptr;
    if (!Tables || !UIManager || !PlayerCamera || !Controller)
    {
        if (bPreview)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CityOpening] 미리보기 거부: Tables=%s UIManager=%s Camera=%s Controller=%s"),
                *GetNameSafe(Tables), *GetNameSafe(UIManager), *GetNameSafe(PlayerCamera), *GetNameSafe(Controller));
        }
        return false;
    }

    if (bPreview)
    {
        const UCGGameInstance* Instance = Cast<UCGGameInstance>(GetGameInstance());
        UUIBase* UIBase = UIManager->GetUIBase();
        UWorldPartitionSubsystem* Partition = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
        TArray<UUserWidget*> Dialogs;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Dialogs, UConfirmCancelWidget::StaticClass(), true);
        const bool bDialogOpen = Dialogs.ContainsByPredicate([](const UUserWidget* Widget)
        {
            return Widget && Widget->IsInViewport();
        });
        const bool bMoviePlaying = IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying();
        const bool bStreamingReady = !Partition || Partition->IsAllStreamingCompleted();
        const int32 MainCount = UIBase ? UIBase->GetMainStackCount() : -1;
        const int32 PromptCount = UIBase ? UIBase->GetPromptStackCount() : -1;
        const int32 BottomCount = UIBase ? UIBase->GetBottomStackCount() : -1;
        // 라이브 도시의 기본 레이어가 열린 상태에서만 입력을 가져온다.
        if (!Instance || Instance->GetCurrentMapType() != ECurrentMapType::MainMap
            || Instance->IsVisitMode() || Instance->IsLevelTransitionActive() || VisitOverlay
            || bMoviePlaying || !bStreamingReady
            || !UIBase || !UIBase->IsInViewport() || !UIBase->IsVisible()
            || !UIManager->GetInGameLayer() || MainCount != 1
            || PromptCount != 0 || BottomCount > 1
            || bDialogOpen || Controller->GetCurrentInputMode() != EInputMode::Normal)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CityOpening] 미리보기 거부: MainMap=%d Visit=%d Transition=%d VisitOverlay=%d Movie=%d StreamingReady=%d UIInViewport=%d UIVisible=%d InGameLayer=%d Main=%d(필요 1) Prompt=%d(필요 0) Bottom=%d(최대 1) Dialog=%d InputMode=%s(필요 Normal)"),
                Instance && Instance->GetCurrentMapType() == ECurrentMapType::MainMap,
                Instance && Instance->IsVisitMode(), Instance && Instance->IsLevelTransitionActive(),
                VisitOverlay != nullptr, bMoviePlaying, bStreamingReady,
                UIBase && UIBase->IsInViewport(), UIBase && UIBase->IsVisible(),
                UIManager->GetInGameLayer() != nullptr, MainCount, PromptCount, BottomCount, bDialogOpen,
                *StaticEnum<EInputMode>()->GetNameStringByValue(static_cast<int64>(Controller->GetCurrentInputMode())));
            return false;
        }
    }
    else
    {
        if (!Missions || Missions->GetActiveMissionID() != TEXT("M1_CollectBricks") || Missions->IsTutorialCompleted()
            || !Intros || Intros->HasSeen(CityOpeningSeenKey)) return false;

    // 개발 미션 점프는 재현 지점에 즉시 진입한다.
#if !UE_BUILD_SHIPPING
        if (UCGDevSettings::GetEffectiveStartMode() != ECGStartMode::FullTutorial
            || !GetDefault<UCGDevSettings>()->DevStartMissionID.IsNone()) return false;
#endif
    }

    const TSubclassOf<UUserWidget> OverlayClass = Tables->GetWidgetClass(EWidgetType::CityOpeningOverlay);
    if (!OverlayClass || !OverlayClass->IsChildOf(UCityOpeningOverlayWidget::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CityOpening] DT_WidgetClass.CityOpeningOverlay 미등록 — 오프닝 생략."));
        return false;
    }
    CityOpeningOverlay = CreateWidget<UCityOpeningOverlayWidget>(Controller, OverlayClass);
    if (!CityOpeningOverlay) return false;

    if (!PrepareCityOpeningViews())
    {
        CityOpeningOverlay = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("[CityOpening] 재생 생략 — 고정 연출 액터·공장·시각 준비를 확인하세요."));
        return false;
    }
    CityOpeningPreparedAt = FPlatformTime::Seconds();
    CityOpeningStartedAt = 0.0;
    CityOpeningSection = INDEX_NONE;
    bCityOpeningLayerReady = false;
    bCityOpeningFramingReady = false;
    bCityOpeningPreview = bPreview;
    bCityOpeningActive = true;
    SetActorTickEnabled(true);

    CityOpeningOverlay->OnSkip.AddUObject(this, &ACGGameModeBase::FinishCityOpening);
    CityOpeningOverlay->AddToViewport(10000);
    UIManager->OnLevelLayerReady.AddUObject(this, &ACGGameModeBase::HandleCityOpeningLayerReady);
    if (!bPreview) UIManager->ClearGuideNotification();
    Controller->GoToUIMode();
    if (bPreview) HandleCityOpeningLayerReady();
    return true;
}

bool ACGGameModeBase::PrepareCityOpeningViews()
{
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    APlayerCamera* PlayerCamera = Controller ? Cast<APlayerCamera>(Controller->GetPawn()) : nullptr;
    if (!PlayerCamera) return false;
    ACityOpeningSequence* Sequence = nullptr;
    for (TActorIterator<ACityOpeningSequence> It(GetWorld()); It; ++It)
    {
        if (Sequence)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CityOpening] 연출 액터가 중복되어 재생할 수 없습니다."));
            return false;
        }
        Sequence = *It;
    }
    ATimeCycleManager* Clock = nullptr;
    for (TActorIterator<ATimeCycleManager> It(GetWorld()); It; ++It) { Clock = *It; break; }
    if (!Sequence || !Sequence->IsAuthored() || !Clock
        || (Clock->HasVisualTimeOverride() && !bCityOpeningClockOverride)) return false;
    ABrickFactory* Factory = nullptr;
    for (TActorIterator<ABrickFactory> It(GetWorld()); It; ++It) { Factory = *It; break; }
    if (!Factory) return false;
    const float FocusDistance = Factory->GetCameraFocusDistanceCm();
    if (FocusDistance <= 0.f || !PlayerCamera->SnapFocusOnActor(Factory, .5f, FocusDistance)) return false;
    bCityOpeningViewReady = false;
    FMinimalViewInfo Landing;
    PlayerCamera->CalcCamera(0.f, Landing);
    if (!Sequence->Start(Landing)) return false;
    CityOpeningSequence = Sequence;
    CityOpeningClock = Clock;
    FTimeCycleCode Dusk;
    Dusk.Hours = 19;
    bCityOpeningClockOverride = Clock->SetVisualTimeOverride(Dusk);
    CityOpeningView = Sequence->Evaluate(0.f);
    bCityOpeningViewReady = true;
    if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
    UE_LOG(LogTemp, Log, TEXT("[CityOpening] 고정 연출: %s -> Factory=%s"), *Sequence->GetName(), *Landing.Location.ToString());
    return true;
}

void ACGGameModeBase::HandleCityOpeningLayerReady()
{
    if (!bCityOpeningActive) return;
    UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    if (!UIManager || !UIManager->GetUIBase()) return;
    if (!CityOpeningHiddenUI.IsValid())
    {
        CityOpeningHiddenUI = UIManager->GetUIBase();
        CityOpeningPreviousVisibility = CityOpeningHiddenUI->GetVisibility();
        CityOpeningHiddenUI->SetVisibility(ESlateVisibility::Hidden);
    }
    if (bCityOpeningPreview)
    {
        // 가이드/멘토 등 UIBase 바깥 뷰포트 위젯도 현재 표시 상태를 보존한다.
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), true);
        for (UUserWidget* Widget : Widgets)
        {
            if (!Widget || Widget->GetWorld() != GetWorld() || !Widget->IsInViewport()
                || Widget == CityOpeningOverlay.Get() || Widget == CityOpeningHiddenUI.Get()) continue;
            if (CityOpeningHiddenWidgets.ContainsByPredicate([Widget](const TWeakObjectPtr<UUserWidget>& Hidden)
            {
                return Hidden.Get() == Widget;
            })) continue;
            CityOpeningHiddenWidgets.Add(Widget);
            CityOpeningPreviousWidgetVisibilities.Add(Widget->GetVisibility());
            Widget->SetVisibility(ESlateVisibility::Hidden);
        }
    }
    if (AMainMapPlayerController* Controller = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        Controller->GoToUIMode();
        Controller->FlushPressedKeys();
        FInputModeUIOnly OpeningInput;
        OpeningInput.SetWidgetToFocus(CityOpeningOverlay->TakeWidget());
        Controller->SetInputMode(OpeningInput);
    }
    bCityOpeningLayerReady = true;
}

bool ACGGameModeBase::TryGetCityOpeningView(FMinimalViewInfo& OutView) const
{
    if (!bCityOpeningActive || !bCityOpeningViewReady) return false;
    OutView = CityOpeningView;
    return true;
}

void ACGGameModeBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bCityOpeningActive) return;
    const double Now = FPlatformTime::Seconds();
    if (CityOpeningStartedAt == 0.0)
    {
        const UCGGameInstance* Instance = Cast<UCGGameInstance>(GetGameInstance());
        UWorldPartitionSubsystem* Partition = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
        if (!bCityOpeningFramingReady && (!Partition || Partition->IsAllStreamingCompleted()))
        {
            // 전경의 스트리밍 소스가 로드한 도시로 다시 프레이밍한 뒤 로딩 화면을 연다.
            if (PrepareCityOpeningViews())
            {
                bCityOpeningFramingReady = true;
                return;
            }
        }
        const bool bLoading = (Instance && Instance->IsLevelTransitionActive())
            || (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
            || (Partition && !Partition->IsAllStreamingCompleted());
        if (!bCityOpeningLayerReady || !bCityOpeningFramingReady || bLoading)
        {
            // 레벨 전환이 자원 준비·상한을 관리하는 동안에는 오프닝을 생략하지 않는다.
            if (Now - CityOpeningPreparedAt > 30.0 && !(Instance && Instance->IsLevelTransitionActive()))
            {
                UE_LOG(LogTemp, Warning, TEXT("[CityOpening] 준비 대기 상한 — 기존 튜토리얼로 복귀합니다."));
                const bool bWasPreview = bCityOpeningPreview;
                ClearCityOpeningPresentation();
                if (!bWasPreview)
                {
                    if (UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>()) Missions->TryStartMissionChain();
                    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UIManager->TryShowOfflineReport();
                }
            }
            return;
        }
        CityOpeningStartedAt = Now;
        UE_LOG(LogTemp, Log, TEXT("[CityOpening] %.1f초 연출 시작"), CityOpeningCamera::Duration);
    }

    const float Elapsed = static_cast<float>(Now - CityOpeningStartedAt);
    const int32 Section = CityOpeningCamera::Section(Elapsed);
    if (!CityOpeningSequence.IsValid() || !CityOpeningClock.IsValid())
    {
        const bool bWasPreview = bCityOpeningPreview;
        ClearCityOpeningPresentation();
        if (!bWasPreview)
        {
            if (UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>()) Missions->TryStartMissionChain();
            if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UIManager->TryShowOfflineReport();
        }
        return;
    }
    const bool bCameraCut = Section != CityOpeningSection;
    if (bCameraCut)
    {
        FTimeCycleCode ShotTime;
        ShotTime.Hours = Section == 0 ? 19 : 10;
        CityOpeningClock->SetVisualTimeOverride(ShotTime);
    }
    CityOpeningView = CityOpeningSequence->Evaluate(Elapsed);
    CityOpeningSection = Section;
    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        if (Controller->PlayerCameraManager)
        {
            if (bCameraCut) Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
            Controller->PlayerCameraManager->SetManualCameraFade(CityOpeningCamera::FadeAmount(Elapsed), FLinearColor::Black, false);
        }
    }
    if (CityOpeningOverlay) CityOpeningOverlay->SetOpeningProgress(Elapsed);
    if (Elapsed >= CityOpeningCamera::Duration) FinishCityOpening();
}

void ACGGameModeBase::FinishCityOpening()
{
    if (!bCityOpeningActive) return;
    const double Elapsed = CityOpeningStartedAt > 0.0 ? FPlatformTime::Seconds() - CityOpeningStartedAt : 0.0;
    const bool bWasPreview = bCityOpeningPreview;
    ClearCityOpeningPresentation();
    if (!bWasPreview)
    {
        if (UPanelIntroSubsystem* Intros = GetGameInstance()->GetSubsystem<UPanelIntroSubsystem>()) Intros->MarkSeen(CityOpeningSeenKey);
        if (UMissionManagerSubsystem* Missions = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>()) Missions->TryStartMissionChain();
        if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UIManager->TryShowOfflineReport();
    }
    UE_LOG(LogTemp, Log, TEXT("[CityOpening] 종료 %.2fs — 공장 조작 안내로 전환"), Elapsed);
}

void ACGGameModeBase::ClearCityOpeningPresentation()
{
    if (!bCityOpeningActive) return;
    bCityOpeningActive = false;
    bCityOpeningPreview = false;
    bCityOpeningViewReady = false;
    if (ACityOpeningSequence* Sequence = CityOpeningSequence.Get()) Sequence->Stop();
    CityOpeningSequence.Reset();
    if (bCityOpeningClockOverride)
    {
        if (ATimeCycleManager* Clock = CityOpeningClock.Get()) Clock->ClearVisualTimeOverride();
    }
    bCityOpeningClockOverride = false;
    CityOpeningClock.Reset();
    SetActorTickEnabled(false);
    if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
    {
        UIManager->OnLevelLayerReady.RemoveAll(this);
    }
    if (CityOpeningOverlay)
    {
        CityOpeningOverlay->OnSkip.RemoveAll(this);
        CityOpeningOverlay->RemoveFromParent();
        CityOpeningOverlay = nullptr;
    }
    if (CityOpeningHiddenUI.IsValid())
    {
        CityOpeningHiddenUI->SetVisibility(CityOpeningPreviousVisibility);
        CityOpeningHiddenUI.Reset();
    }
    for (int32 Index = 0; Index < CityOpeningHiddenWidgets.Num(); ++Index)
    {
        if (UUserWidget* Widget = CityOpeningHiddenWidgets[Index].Get())
        {
            if (Widget->IsInViewport()) Widget->SetVisibility(CityOpeningPreviousWidgetVisibilities[Index]);
        }
    }
    CityOpeningHiddenWidgets.Reset();
    CityOpeningPreviousWidgetVisibilities.Reset();
    if (AMainMapPlayerController* Controller = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        if (Controller->PlayerCameraManager)
        {
            Controller->PlayerCameraManager->StopCameraFade();
            Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
        }
        Controller->FlushPressedKeys();
        Controller->GoToNormalMode();
        FInputModeGameAndUI PlayingInput;
        PlayingInput.SetHideCursorDuringCapture(false);
        Controller->SetInputMode(PlayingInput);
    }
}

void ACGGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 도중 맵 이탈/PIE 종료는 본 것으로 저장하지 않는다.
    ClearCityOpeningPresentation();
    Super::EndPlay(EndPlayReason);
}
