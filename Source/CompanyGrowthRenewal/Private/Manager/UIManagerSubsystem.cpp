// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/UIManagerSubsystem.h"
#include "GameMode/CGGameModeBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Enum/WidgetType.h"
#include "UI/UISoundTags.h"
#include "Core/CGGameInstance.h"
#include "UI/UIBase.h"
#include "UI/Panel/LoadingWidget.h"
#include "UI/Panel/InGameLayerWidget.h"
#include "UI/Panel/OfficeLayerWidget.h"
#include "UI/Panel/OfficeMainWidget.h"
#include "UI/Panel/LegacyWorldMapLayerWidget.h"
#include "UI/Element/Notifications/NotificationContainerWidget.h"
#include "UI/Element/Notifications/NotificationElementWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Panel/RewardRevealPresentationWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Enum/ResourceType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/CloudErrorTextTable.h"
#include "Table/ResourceInfo.h"
#include "Engine/Texture2D.h"
#include "UI/Panel/HQLevelUpCelebrationWidget.h"
#include "Manager/SideContentSubsystem.h"
#include "Manager/AttendanceSubsystem.h"
#include "Manager/LiveOpsSubsystem.h"
#include "Manager/GameClockSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Data/WorldMapTypes.h"
#include "UI/Panel/WorldMapPanelWidget.h"

void UUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 1) TableManagerSubsystem 꺼내기
    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] TableManagerSubsystem Error"));
        return;
    }

    // 2) 다음 틱에 Base UI 생성
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateLambda([this, TableMgr]()
                {
                    // 3) UiBase 클래스 얻기
                    TSubclassOf<UUserWidget> BaseClass = TableMgr->GetWidgetClass(EWidgetType::UIBase);
                    if (!BaseClass)
                    {
                        UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] BaseClass is null! DataTable 확인 필요"));
                        return;
                    }

                    // 4) PlayerController 얻기
                    APlayerController* PC = GetWorld()->GetFirstPlayerController();
                    if (!PC)
                    {
                        UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] PlayerController is null!"));
                        return;
                    }

                    // 5) 위젯 생성 시도
                    UIBaseInstance = CreateWidget<UUIBase>(PC, BaseClass);
                    if (!IsValid(UIBaseInstance))
                    {
                        UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] CreateWidget<UUIBase> 실패. ")
                            TEXT("BaseClass=%s, IsChildOf<UUIBase>=%d"),
                            *BaseClass.Get()->GetName(),
                            BaseClass->IsChildOf(UUIBase::StaticClass()));
                        return;
                    }

                    // 이제 안전하게 뷰포트에 추가
                    UIBaseInstance->AddToViewport();
                    // ★ 뷰포트에 올리는 것만으로는 CommonUI 액션 라우터가 이 트리를 쓰지 않는다.
                    //   라우터는 부모 없는 활성화 위젯마다 루트 노드를 만들어 두지만, `ActiveRootNode` 는
                    //   그 루트가 **활성일 때만** 설정된다(ProcessRebuiltWidgets). 활성화하지 않으면
                    //   ESC·안드로이드 뒤로가기 바인딩이 만들어져 있어도 ProcessInput 이 노드를 못 찾아
                    //   Unhandled 로 떨어진다(2026-09-26 실측).
                    UIBaseInstance->ActivateWidget();

                    // 초기화 완료 및 델리게이트 실행
                    bIsInitialized = true;
                    OnInitializeSuccess.Broadcast();
                    OnInitializeSuccess.Clear(); 
                    UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] UIBase Initialized"));
                })
        );

    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[UUIManagerSubsystem] GetWorld() Error"));
    }

    // ResourceItemManager 구독 (리소스 변경 시 UI 업데이트)
    if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
    {
        ResourceManagerHandle = ResourceMgr->OnResourceChanged.AddUObject(
            this, &UUIManagerSubsystem::HandleResourceManagerChanged
        );
        UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] Subscribed to ResourceItemManager"));

        // 구독 직후 현재 리소스 값들을 UI에 전달 (초기 동기화)
        for (int32 i = 1; i <= 3; ++i)
        {
            EResourceType Type = static_cast<EResourceType>(i);
            int64 CurrentAmount = ResourceMgr->GetResourceAmount(Type);
            OnUIResourceChanged.Broadcast(Type, CurrentAmount);
            UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] Initial sync: Type=%d, Amount=%lld"), (int32)Type, CurrentAmount);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ResourceItemManager not found"));
    }
}

void UUIManagerSubsystem::Deinitialize()
{
    ClearBottomNotification();
    // ResourceItemManager 구독 해제
    if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
    {
        ResourceMgr->OnResourceChanged.Remove(ResourceManagerHandle);
        UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] Unsubscribed from ResourceItemManager"));
    }

    // 알림 컨테이너 정리
    if (NotificationContainer && NotificationContainer->IsInViewport())
    {
        NotificationContainer->RemoveFromParent();
        NotificationContainer = nullptr;
    }

    if (UIBaseInstance && UIBaseInstance->IsInViewport())
    {
        UIBaseInstance->RemoveFromParent();
        UIBaseInstance = nullptr;
    }
    Super::Deinitialize();
}

UUIBase* UUIManagerSubsystem::GetUIBase() const
{
    return UIBaseInstance;
}

// 게임시작 후 맨 처음 켜지는 UI는 초기화가 잘 되있는 상태인지 체크하고 켜야됨.
void UUIManagerSubsystem::CreateInGameLayerWidget()
{
    if (!bTitleSubscribed)
    {
        if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
        {
            SaveMgr->OnCompanyTitleChanged.AddDynamic(this, &UUIManagerSubsystem::HandleCompanyTitleChanged);
            TitlePromotionFrom = SaveMgr->GetCompanyTitle();
            bTitleSubscribed = true;
        }
    }

    if (bIsInitialized && UIBaseInstance)
    {
        ShowMainMapUI();
        UE_LOG(LogTemp, Log, TEXT("[UUIManagerSubsystem] CreateInGameLayerWidget-> InternalCreateInGameLayerWidget()"));
    }
    else
    {
        OnInitializeSuccess.AddUObject(this, &UUIManagerSubsystem::ShowMainMapUI);
        UE_LOG(LogTemp, Log, TEXT("[UUIManagerSubsystem] CreateInGameLayerWidget - > Bind Delegate()"));
    }
}

void UUIManagerSubsystem::CreateOfficeLayerWidget()
{
    if (bIsInitialized && UIBaseInstance)
    {
        ShowOfficeUI();
        UE_LOG(LogTemp, Log,
            TEXT("[UIManagerSubsystem] CreateOfficeLayerWidget - UIBase ready, showing Office UI immediately"));
    }
    else
    {
        OnInitializeSuccess.RemoveAll(this);
        OnInitializeSuccess.AddUObject(this, &UUIManagerSubsystem::ShowOfficeUI);
        UE_LOG(LogTemp, Log,
            TEXT("[UIManagerSubsystem] CreateOfficeLayerWidget - UIBase pending, queued Office UI creation"));
    }
}

UInGameLayerWidget* UUIManagerSubsystem::GetInGameLayer() const
{
    return UIInGameMain;
}

UOfficeLayerWidget* UUIManagerSubsystem::GetOfficeLayer() const
{
    return UIOfficeLayer;
}

UOfficeMainWidget* UUIManagerSubsystem::GetOfficeMain() const
{
    return UIOfficeMain;
}

void UUIManagerSubsystem::ClearAllUI()
{
    ClearBottomNotification();
    if (!bIsInitialized || !UIBaseInstance) return;

    // 1. 기존 UIBase 완전 제거
    if (UIBaseInstance && UIBaseInstance->IsInViewport())
    {
        UIBaseInstance->RemoveFromParent();
        UE_LOG(LogTemp, Warning, TEXT("UIBaseInstance removed from viewport"));
    }

    // 2. 모든 참조 초기화
    UIBaseInstance = nullptr;
    UIInGameMain = nullptr;
    UIOfficeLayer = nullptr;
    UIOfficeMain = nullptr;
    UILegacyWorldMapLayer = nullptr;

    // 3. 새로운 UIBase 생성
    if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
    {
        TSubclassOf<UUserWidget> BaseClass = TableMgr->GetWidgetClass(EWidgetType::UIBase);
        if (BaseClass)
        {
            APlayerController* PC = GetWorld()->GetFirstPlayerController();
            if (PC)
            {
                UIBaseInstance = CreateWidget<UUIBase>(PC, BaseClass);
                if (UIBaseInstance)
                {
                    UIBaseInstance->AddToViewport();
                    // 위와 같은 이유 ― 루트가 활성이어야 액션 라우터가 바인딩을 조회한다.
                    UIBaseInstance->ActivateWidget();
                    UE_LOG(LogTemp, Warning, TEXT("New UIBaseInstance created and added to viewport"));
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ClearAllUI"));
}

void UUIManagerSubsystem::ShowMainMapUI()
{
    // 방문 모드에서는 일반 MainMap UI를 표시하지 않음
    if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
    {
        if (GI->IsVisitMode())
        {
            UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowMainMapUI 스킵 (방문 모드)"));
            return;
        }
    }

    ClearAllUI();

    // 다음 틱에 UI 생성 (UIBase 완전 초기화 후)
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
            {
                // InGameMain 레이어
                if (TSubclassOf<UUserWidget> LayerClass = TableMgr->GetWidgetClass(EWidgetType::InGameMain))
                {
                    UIInGameMain = Cast<UInGameLayerWidget>(UIBaseInstance->PushMenuClass(LayerClass.Get()));
                }

                // BuildOpen 하단 버튼
                if (TSubclassOf<UUserWidget> BottomClass = TableMgr->GetWidgetClass(EWidgetType::BuildOpen))
                {
                    UIBaseInstance->PushBottomClass(BottomClass.Get());
                }

				if (USideContentSubsystem* Side = GetGameInstance()->GetSubsystem<USideContentSubsystem>())
				{
					if (TSubclassOf<UUserWidget> SkylineClass = TableMgr->GetWidgetClass(EWidgetType::SkylineScreen))
					{
						if (Side->ConsumeSkylineLobbyReturn())
						{
							if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance())) GI->SetOfficeMode(EOfficeMode::Normal);
							UIBaseInstance->PushMenuClass(SkylineClass.Get());
						}
					}
				}
            }
            UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowMainMapUI"));

            // 레이어 생성 완료 — 미션 트래커/가이드는 이 신호 뒤에 빌드 (next-tick 경쟁/깜빡임 제거)
            OnLevelLayerReady.Broadcast();

            // 오프라인 정산 리포트가 보류 중이면 지금(UIBase 준비 완료) 표시 시도
            TryShowOfflineReport();

            // 출석·새 공지 자동 창 ― 정산 창·튜토리얼이 끝나 화면이 한가해지면 연다
            StartEntryPopups();

            TryOpenPendingWorldMap();
        });
}

void UUIManagerSubsystem::RequestWorldMapOnCityReturn(ECountryType FocusCountry)
{
    bPendingWorldMapOpen = true;
    PendingWorldMapCountry = FocusCountry;
}

void UUIManagerSubsystem::TryOpenPendingWorldMap()
{
    if (!bPendingWorldMapOpen)
    {
        return;
    }
    // 한 번만 시도한다 ― 남겨 두면 다음 도시 진입마다 세계지도가 다시 열린다.
    bPendingWorldMapOpen = false;
    const ECountryType FocusCountry = PendingWorldMapCountry;
    PendingWorldMapCountry = ECountryType::None;

    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
    if (!UIBaseInstance || !TableMgr || UIBaseInstance->GetPromptStackCount() > 0)
    {
        return;
    }
    // 도시 버튼과 같은 해금 조건 ― 잠겨 있으면 도시 화면에 머문다.
    if (SaveMgr && SaveMgr->GetHQLevel() < WorldMapConstants::WorldMapRequiredHQLevel)
    {
        return;
    }
    const TSubclassOf<UUserWidget> MapClass = TableMgr->GetWidgetClass(EWidgetType::WorldMapPanel);
    UWorldMapPanelWidget* Map = MapClass
        ? Cast<UWorldMapPanelWidget>(UIBaseInstance->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(MapClass)))
        : nullptr;
    if (!Map)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] 공장 이동 요청 - 세계지도 패널을 열지 못함"));
        return;
    }
    if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
    {
        PC->GoToUIMode();
    }
    if (FocusCountry != ECountryType::None)
    {
        Map->FocusOnCountry(FocusCountry, true);
    }
}

void UUIManagerSubsystem::StartEntryPopups()
{
    // 게임 인스턴스 타이머라 맵을 오가도 한 번만 돈다 ― 사무실에선 한가함 판정이 거절하고, 도시로 돌아오면 그때 연다
    FTimerManager& Timers = GetGameInstance()->GetTimerManager();
    if (!Timers.IsTimerActive(EntryPopupTimer))
    {
        Timers.SetTimer(EntryPopupTimer, FTimerDelegate::CreateUObject(this, &UUIManagerSubsystem::TickEntryPopups), 1.f, true);
    }
}

bool UUIManagerSubsystem::IsCityScreenIdle() const
{
    const UCGGameInstance* Instance = Cast<UCGGameInstance>(GetGameInstance());
    UWorld* CityWorld = GetWorld();
    if (!Instance || !CityWorld || Instance->GetCurrentMapType() != ECurrentMapType::MainMap
        || Instance->IsVisitMode() || Instance->IsLevelTransitionActive())
    {
        return false;
    }
    if (!UIBaseInstance || !UIBaseInstance->IsInViewport() || !UIInGameMain
        || UIBaseInstance->GetMainStackCount() != 1 || UIBaseInstance->GetPromptStackCount() != 0
        || UIBaseInstance->GetBottomStackCount() > 1)
    {
        return false;
    }
    const AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(CityWorld->GetFirstPlayerController());
    if (!PC || PC->GetCurrentInputMode() != EInputMode::Normal)
    {
        return false;
    }
    if (const ACGGameModeBase* CityMode = CityWorld->GetAuthGameMode<ACGGameModeBase>(); CityMode && CityMode->IsCityOpeningActive())
    {
        return false;
    }
    TArray<UUserWidget*> Dialogs;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(CityWorld, Dialogs, UConfirmCancelWidget::StaticClass(), true);
    return !Dialogs.ContainsByPredicate([](const UUserWidget* Dialog) { return Dialog && Dialog->IsInViewport(); });
}

void UUIManagerSubsystem::TickEntryPopups()
{
    UGameInstance* GI = GetGameInstance();
    const UMissionManagerSubsystem* Missions = GI->GetSubsystem<UMissionManagerSubsystem>();
    if (Missions && Missions->HasActiveMission())
    {
        bTutorialSeenThisSession = true;
    }
    const USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>();
    // 튜토리얼이 끝나고 마지막 보상 창·오프라인 정산 창도 지나간 뒤에만
    const bool bIdle = Missions && Missions->IsTutorialCompleted() && !Missions->IsRewardSplashShown()
        && !(SaveMgr && SaveMgr->HasPendingOfflineReport()) && IsCityScreenIdle();
    const double Now = FPlatformTime::Seconds();
    if (!bIdle)
    {
        EntryPopupIdleSince = -1.0;
        EntryPopupToastWaitSince = -1.0;
        return;
    }
    if (NotificationContainer && NotificationContainer->HasActiveNotifications())
    {
        if (EntryPopupToastWaitSince < 0.0)
        {
            EntryPopupToastWaitSince = Now;
        }
        if (Now - EntryPopupToastWaitSince < FEntryPopupRules::ToastWaitMaxSeconds)
        {
            EntryPopupIdleSince = -1.0;
            return;
        }
    }
    if (EntryPopupIdleSince < 0.0)
    {
        EntryPopupIdleSince = Now;
    }
    if (Now - EntryPopupIdleSince < FEntryPopupRules::SettleSeconds)
    {
        return;
    }

    const UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>();
    const FString Today = (Clock && Clock->IsServerTimeValid()) ? Clock->GetDayKey() : FString();
    const UAttendanceSubsystem* Attendance = GI->GetSubsystem<UAttendanceSubsystem>();
    const ULiveOpsSubsystem* LiveOps = GI->GetSubsystem<ULiveOpsSubsystem>();
    const EEntryPopup Next = FEntryPopupRules::ResolveNext(Attendance && Attendance->CanClaimToday(),
        LiveOps && LiveOps->HasUnreadNotice(), Today, EntryPopupSave, bTutorialSeenThisSession);
    if (Next != EEntryPopup::None && OpenEntryPopup(Next))
    {
        FEntryPopupRules::MarkShown(Next, Today, EntryPopupSave);
        EntryPopupIdleSince = -1.0;
        EntryPopupToastWaitSince = -1.0;
        // 다시 켰을 때 같은 날 같은 창을 또 밀지 않게 기록을 남긴다
        if (USaveLoadManager* EntrySave = GI->GetSubsystem<USaveLoadManager>())
        {
            EntrySave->RequestDeferredSave();
        }
    }
}

bool UUIManagerSubsystem::OpenEntryPopup(EEntryPopup Popup)
{
    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    const EWidgetType Type = Popup == EEntryPopup::Notice ? EWidgetType::NewsPanel : EWidgetType::AttendancePanel;
    TSubclassOf<UUserWidget> PanelClass = TableMgr ? TableMgr->GetWidgetClass(Type) : nullptr;
    if (!PanelClass || !UIBaseInstance)
    {
        return false;
    }
    // 소식 창은 이벤트 탭이 기본 ― 공지 탭으로 열어야 새 공지가 읽힌다
    if (Popup == EEntryPopup::Notice)
    {
        SetPendingPanelTab(TEXT("Notice"));
    }
    if (UIBaseInstance->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(PanelClass)))
    {
        // 메뉴에서 열 때와 같은 입력 상태 ― 창이 떠 있는 동안 도시 끌기·건물 탭이 새지 않고, 닫히면 창이 Normal 로 되돌린다
        if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
        {
            PC->GoToUIMode();
        }
        UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] 자동 창 열기: %s"), Popup == EEntryPopup::Notice ? TEXT("공지") : TEXT("출석"));
        return true;
    }
    SetPendingPanelTab(NAME_None);
    return false;
}

// 정산 모달 표시 단일 관문. 로그인·복귀 경로 합류점
void UUIManagerSubsystem::TryShowOfflineReport()
{
    if (const ACGGameModeBase* CityMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameModeBase>() : nullptr)
    {
        if (CityMode->IsCityOpeningActive()) return;
    }
    // UI 미준비면 보류 유지 → 이후 ShowMainMap/CalculateOfflineGains가 flush
    if (!UIBaseInstance)
    {
        return;
    }

    USaveLoadManager* SaveLoadMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
    if (!SaveLoadMgr || !SaveLoadMgr->HasPendingOfflineReport())
    {
        return;
    }

    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr)
    {
        return;
    }

    TSubclassOf<UUserWidget> ModalClass = TableMgr->GetWidgetClass(EWidgetType::OfflineReportModal);
    if (!ModalClass)
    {
        // 표시할 수 없는 리포트의 pending 을 붙들면 MainMap 진입 안전망(InGameLayerWidget::NativeConstruct)이
        // 영구히 양보해 금고 회수 경로가 0 이 된다 — 세이브에 없는 런타임 플래그라 재시작으로도 안 풀린다.
        // 요약은 못 보여주더라도 돈은 넘긴다.
        UE_LOG(LogTemp, Warning,
            TEXT("[UIManagerSubsystem] OfflineReportModal 클래스 없음 — DT_WidgetClass 행 확인. 표시 생략, 금고만 조용히 회수."));

        // 띄울 UI 가 없는 분기라 연출 경로(InGameLayer::ExecuteAllVaultCollection)가 아니라 매니저 직접 호출
        if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
        {
            if (OpMgr->HasAnyStoredRevenue())
            {
                OpMgr->CollectAllStoredRevenue();
            }
        }

        // 모달을 push 하지 않았으므로 구독자 0 — Consume 의 broadcast 는 무동작이고 pending 상태만 정리된다
        SaveLoadMgr->ConsumePendingOfflineReport();
        return;
    }

    // 모달이 NativeConstruct에서 구독 → Consume 1회 발화 → SetReportData 수신. push(구독) 먼저, Consume 나중
    if (!UIBaseInstance->PushPromptClass(ModalClass.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] OfflineReportModal push 실패 — pending 리포트 유지."));
        return;
    }
    SaveLoadMgr->ConsumePendingOfflineReport();

    if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->GoToUIMode();
    }
}

void UUIManagerSubsystem::ShowOfficeUI()
{
    ClearAllUI();
	if (const UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		if (GI->GetOfficeMode() == EOfficeMode::Skyline)
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				if (UIBaseInstance)
				{
					if (TSubclassOf<UUserWidget> SkylineClass = TableMgr->GetWidgetClass(EWidgetType::SkylineScreen))
					{
						UIBaseInstance->PushMenuClass(SkylineClass.Get());
					}
				}
			}
			return;
		}
	}

    if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
    {
        // Office 레이어
        if (TSubclassOf<UUserWidget> LayerClass = TableMgr->GetWidgetClass(EWidgetType::OfficeLayer))
        {
            UIOfficeLayer = Cast<UOfficeLayerWidget>(UIBaseInstance->PushMenuClass(LayerClass.Get()));
            UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowOfficeUI - OfficeLayer created"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] ShowOfficeUI - OfficeLayer Widget class not found in DataTable"));
        }

        // OfficeMain을 Bottom으로
        if (TSubclassOf<UUserWidget> OfficeMain = TableMgr->GetWidgetClass(EWidgetType::OfficeMain))
        {
            UIOfficeMain = Cast<UOfficeMainWidget>(UIBaseInstance->PushBottomClass(OfficeMain.Get()));
            if (!UIOfficeMain)
            {
                // 캐스트 실패 = WBP 가 UOfficeMainWidget 로 reparent 안 됨 → 미션트래커가 뷰포트 폴백으로 강등됨
                UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowOfficeUI - OfficeMain 푸시됐지만 UOfficeMainWidget 캐스트 실패"));
            }
            UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowOfficeUI - OfficeMain created"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] ShowOfficeUI - OfficeMain Widget class not found in DataTable"));
        }
    }

    // 레이어/OfficeMain 생성 완료 — 미션 위젯 빌드 신호. next-tick 으로 통일(StartPlay 동기 구간 종료 후 발화).
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimerForNextTick([this]()
        {
            OnLevelLayerReady.Broadcast();
        });
    }
}

void UUIManagerSubsystem::ShowWorldMapUI()
{
	ClearAllUI();

	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		// WorldMap 레이어 (MainStack)
		if (TSubclassOf<UUserWidget> LayerClass = TableMgr->GetWidgetClass(EWidgetType::LegacyWorldMapLayer))
		{
			UILegacyWorldMapLayer = Cast<ULegacyWorldMapLayerWidget>(UIBaseInstance->PushMenuClass(LayerClass.Get()));
			UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowWorldMapUI - LegacyWorldMapLayer created"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowWorldMapUI - LegacyWorldMapLayer Widget class not found in DataTable"));
		}

		// WorldMap 하단 (BottomStack)
		if (TSubclassOf<UUserWidget> BottomClass = TableMgr->GetWidgetClass(EWidgetType::WorldMapBottom))
		{
			UIBaseInstance->PushBottomClass(BottomClass.Get());
			UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowWorldMapUI - WorldMapBottom created"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowWorldMapUI - WorldMapBottom Widget class not found in DataTable"));
		}
	}
}

void UUIManagerSubsystem::HandleCompanyTitleChanged(ECompanyTitle NewTitle)
{
    // 연쇄 승격은 한 호출 안에서 여러 번 방송된다 - 첫 출발점만 잡고 마지막 도착점으로 갱신한다
    TitlePromotionTo = NewTitle;
    if (bTitlePromotionPending)
    {
        return;
    }
    bTitlePromotionPending = true;
    if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
    {
        World->GetTimerManager().SetTimerForNextTick(this, &UUIManagerSubsystem::ShowPendingTitlePromotion);
    }
    else
    {
        ShowPendingTitlePromotion();
    }
}

void UUIManagerSubsystem::ShowPendingTitlePromotion()
{
    bTitlePromotionPending = false;
    const ECompanyTitle From = TitlePromotionFrom;
    const ECompanyTitle To = TitlePromotionTo;
    TitlePromotionFrom = To;   // 다음 승격의 출발점
    if (From == To)
    {
        return;
    }

    UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    if (!TableMgr || !GetUIBase())
    {
        return;
    }
    TSubclassOf<UUserWidget> CelebClass = TableMgr->GetWidgetClass(EWidgetType::HQLevelUpCelebration);
    if (!CelebClass)
    {
        return;
    }
    UCommonActivatableWidget* Widget = GetUIBase()->PushPromptClass(CelebClass.Get());
    if (UHQLevelUpCelebrationWidget* Celeb = Cast<UHQLevelUpCelebrationWidget>(Widget))
    {
        Celeb->SetTitlePromotion(
            FText::FromString(CompanyTitleToString(From)),
            FText::FromString(CompanyTitleToString(To)),
            static_cast<int32>(From) + 1,
            static_cast<int32>(To) + 1);
    }
}

void UUIManagerSubsystem::HandleResourceManagerChanged(EResourceType Type, int64 NewValue)
{
    // 중앙에서 받아서 위젯들에게 재브로드캐스트
    OnUIResourceChanged.Broadcast(Type, NewValue);

    UE_LOG(LogTemp, Verbose, TEXT("[UIManagerSubsystem] Resource Changed: Type=%d, Value=%lld"),
        (int32)Type, NewValue);
}

namespace
{
    // 같은 타입 알림이 연달아 터질 때 소리가 겹쳐 쌓이는 것 차단
    constexpr double NotificationSoundCooldown = 1.5;

    // Normal 은 정보성이라 존재감만 남기고 뒤로 뺀다 (무음으로 두면 알림 절반이 소리를 잃음)
    constexpr float NormalNotificationVolume = 0.35f;
}

void UUIManagerSubsystem::ShowNotification(const FText& Message, float Duration, ENotificationType Type)
{
    UNotificationContainerWidget* Container = GetOrCreateNotificationContainer();
    if (!Container)
    {
        return;
    }

    Container->AddNotification(Message, Duration, GetNotificationColor(Type));

    USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr;
    if (!SoundMgr)
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    double& LastPlayed = LastNotificationSoundTime.FindOrAdd(Type);
    if (LastPlayed > 0.0 && (Now - LastPlayed) < NotificationSoundCooldown)
    {
        return;
    }
    LastPlayed = Now;

    const FGameplayTag SoundTag = GetNotificationSoundTag(Type);
    if (Type == ENotificationType::Normal)
    {
        SoundMgr->PlayUISoundWithParams(SoundTag, NormalNotificationVolume);
    }
    else
    {
        SoundMgr->PlayUISound(SoundTag);
    }
}

void UUIManagerSubsystem::ShowBottomNotification(const FText& Message, float Duration)
{
    if (Message.IsEmpty()) return;

    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!TableMgr || !PC) return;

    TSubclassOf<UUserWidget> ElementClass = TableMgr->GetWidgetClass(EWidgetType::NotificationElement);
    if (!ElementClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] Bottom notification: NotificationElement class missing."));
        return;
    }

    ClearBottomNotification();
    BottomNotification = CreateWidget<UNotificationElementWidget>(PC, ElementClass);
    if (!BottomNotification) return;

    BottomNotification->OnNotificationFinished.BindUObject(this, &UUIManagerSubsystem::HandleBottomNotificationFinished);
    BottomNotification->SetVisibility(ESlateVisibility::HitTestInvisible);
    BottomNotification->AddToViewport(9999);
    // 하단 도크 위 여백을 확보하고 기존 알림의 내용 크기와 애니메이션을 유지한다.
    BottomNotification->SetPositionInViewport(FVector2D(0.0f, -220.0f), false);
    BottomNotification->SetAnchorsInViewport(FAnchors(0.5f, 1.0f));
    BottomNotification->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
    BottomNotification->ShowMessage(Message, Duration, GetNotificationColor(ENotificationType::Success));
}

void UUIManagerSubsystem::ClearBottomNotification()
{
    if (IsValid(BottomNotification))
    {
        BottomNotification->OnNotificationFinished.Unbind();
        BottomNotification->RemoveFromParent();
    }
    BottomNotification = nullptr;
}

void UUIManagerSubsystem::HandleBottomNotificationFinished(UNotificationElementWidget* FinishedWidget)
{
    if (BottomNotification == FinishedWidget)
    {
        ClearBottomNotification();
    }
}

void UUIManagerSubsystem::ShowRejectNotification(const FText& Message, float CooldownSec, ENotificationType Type)
{
    if (Message.IsEmpty())
    {
        return;
    }

    // 문구를 키로 억제 — 서로 다른 거부 사유는 각각 뜨고, 같은 사유의 연타만 삼킨다.
    const FString Key = Message.ToString();
    const double Now = FPlatformTime::Seconds();
    if (const double* Last = RejectNotifyTimes.Find(Key))
    {
        if ((Now - *Last) < CooldownSec)
        {
            return;
        }
    }
    RejectNotifyTimes.Add(Key, Now);

    ShowNotification(Message, 2.0f, Type);
}

void UUIManagerSubsystem::NotifyInsufficientResource(EResourceType Type, int64 Need)
{
    UGameInstance* GI = GetGameInstance();
    UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    UResourceItemManager* ResourceMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
    if (!TableMgr || !ResourceMgr)
    {
        return;
    }

    // 자원명은 DT_Resource 단일 진실 — 행이 없으면 코드 폴백 없이 알림을 생략한다.
    bool bNameOk = false;
    const FResourceInfo ResInfo = TableMgr->GetResourceInfo(Type, bNameOk);
    if (!bNameOk || ResInfo.DisplayName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] DT_Resource DisplayName 없음(Type=%d) - 부족 알림 생략"),
            static_cast<int32>(Type));
        return;
    }

    const int64 Missing = FMath::Max<int64>(0, Need - ResourceMgr->GetResourceAmount(Type));
    const FText MissingText = UGlobalUtilFunctions::AbbreviateNumber(
        Missing, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil);

    ShowRejectNotification(FText::Format(
        NSLOCTEXT("UIManager", "NotEnoughResource", "{0}|hpp(이,가) {1} 부족합니다"),
        ResInfo.DisplayName, MissingText));
}

bool UUIManagerSubsystem::TryNotifyCloudBlock()
{
    UGameInstance* GI = GetGameInstance();
    UCloudEconomySubsystem* Cloud = GI ? GI->GetSubsystem<UCloudEconomySubsystem>() : nullptr;
    const ECloudBlockState Block = Cloud ? Cloud->GetBlockState() : ECloudBlockState::None;
    if (Block == ECloudBlockState::None)
    {
        return false;
    }

    // 잠긴 동안엔 요청이 아예 나가지 않았다 — "부족합니다" 든 코드 문구든 전부 거짓 원인이다(§7.2)
    const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    const FText BlockCodeText = TableMgr ? TableMgr->GetCloudErrorText(Cloud->GetLastBlockCode()) : FText::GetEmpty();
    ShowRejectNotification(UCloudEconomySubsystem::BlockStateMessage(Block, BlockCodeText));
    return true;
}

void UUIManagerSubsystem::NotifyCloudOpFailure(FName Code, EResourceType ShortfallResource, int64 ShortfallNeed)
{
    if (TryNotifyCloudBlock())
    {
        return;
    }

    if (UCloudEconomySubsystem::ShouldKeepShortfallText(Code))
    {
        NotifyInsufficientResource(ShortfallResource, ShortfallNeed);
        return;
    }

    NotifyCloudOpFailureWithText(Code, FText::GetEmpty());
}

void UUIManagerSubsystem::NotifyCloudOpFailureWithText(FName Code, const FText& ShortfallText)
{
    if (TryNotifyCloudBlock())
    {
        return;
    }

    if (UCloudEconomySubsystem::ShouldKeepShortfallText(Code) && !ShortfallText.IsEmpty())
    {
        ShowRejectNotification(ShortfallText);
        return;
    }

    // DT 미등록 코드는 DEFAULT 행, DT 부재는 LastResort — 어느 쪽도 빈 문구가 아니다
    const UTableManagerSubsystem* TableMgr = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>()
        : nullptr;
    ShowRejectNotification(TableMgr
        ? TableMgr->GetCloudErrorText(Code)
        : FCloudErrorTextRules::LastResort());
}

void UUIManagerSubsystem::ShowColoredNotification(const FText& Message, float Duration, FLinearColor TextColor)
{
    if (UNotificationContainerWidget* Container = GetOrCreateNotificationContainer())
    {
        // 무음 유지 — 색만 받고 타입이 없어 어떤 알림음이 맞는지 판정할 근거가 없다.
        // 소리가 필요한 호출부는 ShowNotification(타입) 으로 옮길 것.
        Container->AddNotification(Message, Duration, TextColor);
    }
}

namespace CGRFundsToastPresentation
{
    void Resolve(
        const FResourceInfo& MoneyInfo,
        bool bResourceInfoOk,
        UTexture2D*& OutMoneyIcon,
        FLinearColor& OutMoneyColor)
    {
        OutMoneyIcon = nullptr;
        OutMoneyColor = FLinearColor::White;

        if (!bResourceInfoOk)
        {
            return;
        }

        OutMoneyColor = MoneyInfo.UIColor;
        if (MoneyInfo.Icon.IsNull())
        {
            return;
        }

        OutMoneyIcon = MoneyInfo.Icon.LoadSynchronous();
    }
}

void UUIManagerSubsystem::ShowFundsToast(int64 Delta, float Duration)
{
    UNotificationContainerWidget* Container = GetOrCreateNotificationContainer();
    if (!Container)
    {
        return;
    }

    UTexture2D* MoneyIcon = nullptr;
    FLinearColor MoneyColor = FLinearColor::White;

    UGameInstance* GI = GetGameInstance();
    UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    if (!TableMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowFundsToast: TableManagerSubsystem not found; using numeric-only fallback."));
    }
    else
    {
        bool bResourceInfoOk = false;
        const FResourceInfo MoneyInfo = TableMgr->GetResourceInfo(EResourceType::Money, bResourceInfoOk);
        CGRFundsToastPresentation::Resolve(MoneyInfo, bResourceInfoOk, MoneyIcon, MoneyColor);
        if (!bResourceInfoOk)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowFundsToast: Money row missing in DT_Resource; using numeric-only fallback."));
        }
        else if (MoneyInfo.Icon.IsNull())
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowFundsToast: Money icon is unset in DT_Resource; using numeric-only fallback."));
        }
        else
        {
            if (!MoneyIcon)
            {
                UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowFundsToast: Money icon failed to load; using numeric-only fallback."));
            }
        }
    }

    Container->AddIconNotification(
        UGlobalUtilFunctions::FormatFundsAmount(Delta, true),
        MoneyIcon,
        Duration,
        MoneyColor);
}

void UUIManagerSubsystem::ShowRewardToast(const TArray<FMissionReward>& Rewards, const FText& Title, bool bShowTitle)
{
    if (Rewards.IsEmpty())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UTableManagerSubsystem* TableMgr = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    if (!TableMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowRewardToast: TableManagerSubsystem not found."));
        return;
    }

    TSubclassOf<UUserWidget> RewardToastClass = TableMgr->GetWidgetClass(EWidgetType::RewardToast);
    if (!RewardToastClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowRewardToast: RewardToast widget class not found in DataTable."));
        return;
    }

    APlayerController* LocalPC = GameInstance->GetFirstLocalPlayerController();
    if (!LocalPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowRewardToast: First local PlayerController not found."));
        return;
    }

    URewardRevealPresentationWidget* RewardToast = CreateWidget<URewardRevealPresentationWidget>(LocalPC, RewardToastClass);
    if (!RewardToast)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowRewardToast: Failed to create RewardToast widget."));
        return;
    }

    RewardToast->AddToViewport(10000);
    RewardToast->SetupRewards(Rewards, Title, /*bInToastMode=*/true, bShowTitle);
    LastRewardToast = RewardToast;
}

bool UUIManagerSubsystem::IsRewardToastShown() const
{
    const URewardRevealPresentationWidget* Toast = LastRewardToast.Get();
    return Toast && Toast->IsInViewport();
}

void UUIManagerSubsystem::ShowGuideNotification(const FText& Message)
{
    if (UNotificationContainerWidget* Container = GetOrCreateNotificationContainer())
    {
        Container->ShowGuideNotification(Message);
    }
}

void UUIManagerSubsystem::ClearGuideNotification()
{
    if (IsValid(NotificationContainer) && NotificationContainer->IsInViewport())
    {
        NotificationContainer->ClearGuideNotification();
    }
}

void UUIManagerSubsystem::ClearAllNotifications()
{
    ClearBottomNotification();
    if (IsValid(NotificationContainer) && NotificationContainer->IsInViewport())
    {
        NotificationContainer->ClearAllNotifications();
    }
}

FLinearColor UUIManagerSubsystem::GetNotificationColor(ENotificationType Type)
{
    switch (Type)
    {
    case ENotificationType::Success:
        return FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);  // 초록색
    case ENotificationType::Warning:
        return FLinearColor(1.0f, 0.722f, 0.196f, 1.0f);  // FFB832FF
    case ENotificationType::Failed:
        return FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);  // 빨간색
    case ENotificationType::Normal:
    default:
        return FLinearColor::White;  // 흰색
    }
}

FGameplayTag UUIManagerSubsystem::GetNotificationSoundTag(ENotificationType Type)
{
    switch (Type)
    {
    case ENotificationType::Success:
        return CGUISoundTags::NotificationSuccess;
    case ENotificationType::Warning:
        return CGUISoundTags::NotificationWarning;
    case ENotificationType::Failed:
        return CGUISoundTags::NotificationError;
    case ENotificationType::Normal:
    default:
        return CGUISoundTags::NotificationInfo;
    }
}

UNotificationContainerWidget* UUIManagerSubsystem::GetOrCreateNotificationContainer()
{
    // 이미 존재하고 유효하면 반환
    if (IsValid(NotificationContainer) && NotificationContainer->IsInViewport())
    {
        return NotificationContainer;
    }

    // TableManager에서 위젯 클래스 가져오기
    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] TableManagerSubsystem not found."));
        return nullptr;
    }

    TSubclassOf<UUserWidget> ContainerClass = TableMgr->GetWidgetClass(EWidgetType::NotificationContainer);
    if (!ContainerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] NotificationContainer class not found in DataTable."));
        return nullptr;
    }

    // PlayerController 확인
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] PlayerController is null."));
        return nullptr;
    }

    // 위젯 생성
    NotificationContainer = CreateWidget<UNotificationContainerWidget>(PC, ContainerClass);
    if (!NotificationContainer)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] Failed to create NotificationContainerWidget."));
        return nullptr;
    }

    // 높은 ZOrder로 뷰포트에 추가 (다른 UI 위에 표시)
    // 블루프린트에서 이미 상단 중앙 앵커 설정됨
    NotificationContainer->AddToViewport(9999);

    UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] NotificationContainerWidget created."));

    return NotificationContainer;
}

UConfirmCancelWidget* UUIManagerSubsystem::ShowConfirmDialog(
    const FText& Title,
    const FText& Message,
    FSimpleDelegate OnConfirm,
    FSimpleDelegate OnCancel,
    EConfirmDialogIcon Icon)
{
    // 기존 대화상자 닫기
    CloseCurrentDialog();

    // TableManager에서 위젯 클래스 가져오기
    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] TableManagerSubsystem not found for dialog."));
        return nullptr;
    }

    TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::ConfirmCancel);
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ConfirmCancel widget class not found in DataTable."));
        return nullptr;
    }

    // PlayerController 확인
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] PlayerController is null for dialog."));
        return nullptr;
    }

    // 위젯 생성
    CurrentDialog = CreateWidget<UConfirmCancelWidget>(PC, WidgetClass);
    if (!CurrentDialog)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] Failed to create ConfirmCancelWidget."));
        return nullptr;
    }

    // 콜백 바인딩 (확인 버튼)
    if (OnConfirm.IsBound())
    {
        CurrentDialog->OnConfirm.AddLambda([this, OnConfirm]()
        {
            OnConfirm.ExecuteIfBound();
            CloseCurrentDialog();
        });
    }
    else
    {
        CurrentDialog->OnConfirm.AddLambda([this]()
        {
            CloseCurrentDialog();
        });
    }

    // 콜백 바인딩 (취소 버튼)
    if (OnCancel.IsBound())
    {
        CurrentDialog->OnCancel.AddLambda([this, OnCancel]()
        {
            OnCancel.ExecuteIfBound();
            CloseCurrentDialog();
        });
    }
    else
    {
        CurrentDialog->OnCancel.AddLambda([this]()
        {
            CloseCurrentDialog();
        });
    }

    // 높은 ZOrder로 뷰포트에 추가
    CurrentDialog->AddToViewport(10000);
    CurrentDialog->SetTitle(Title);
    CurrentDialog->SetMessage(Message);
    CurrentDialog->SetTitleIconType(Icon);
    CurrentDialog->PlayOverlayAppear();

    UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ConfirmDialog shown: %s"), *Title.ToString());

    return CurrentDialog;
}

UConfirmCancelWidget* UUIManagerSubsystem::ShowAlertDialog(
    const FText& Title,
    const FText& Message,
    FSimpleDelegate OnConfirm,
    EConfirmDialogIcon Icon)
{
    UConfirmCancelWidget* Dialog = ShowConfirmDialog(Title, Message, OnConfirm, FSimpleDelegate(), Icon);
    if (Dialog)
    {
        Dialog->SetConfirmOnly(true);
    }
    return Dialog;
}

void UUIManagerSubsystem::CloseCurrentDialog()
{
    if (CurrentDialog && CurrentDialog->IsInViewport())
    {
        CurrentDialog->RemoveFromParent();
        UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] Dialog closed."));
    }
    CurrentDialog = nullptr;
}

UConfirmCancelWidget* UUIManagerSubsystem::CreateConfirmDialog(const FText& Title, const FText& Message, EConfirmDialogIcon Icon)
{
    // 기존 대화상자 닫기
    CloseCurrentDialog();

    // TableManager에서 위젯 클래스 가져오기
    UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
    if (!TableMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] TableManagerSubsystem not found for dialog."));
        return nullptr;
    }

    TSubclassOf<UUserWidget> WidgetClass = TableMgr->GetWidgetClass(EWidgetType::ConfirmCancel);
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ConfirmCancel widget class not found in DataTable."));
        return nullptr;
    }

    // PlayerController 확인
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] PlayerController is null for dialog."));
        return nullptr;
    }

    // 위젯 생성
    CurrentDialog = CreateWidget<UConfirmCancelWidget>(PC, WidgetClass);
    if (!CurrentDialog)
    {
        UE_LOG(LogTemp, Error, TEXT("[UIManagerSubsystem] Failed to create ConfirmCancelWidget."));
        return nullptr;
    }

    // 내용 설정
    CurrentDialog->SetTitle(Title);
    CurrentDialog->SetMessage(Message);

    // 기본 닫기 동작 바인딩 (블루프린트에서 추가 콜백 바인딩 가능)
    CurrentDialog->OnConfirm.AddLambda([this]()
    {
        CloseCurrentDialog();
    });

    CurrentDialog->OnCancel.AddLambda([this]()
    {
        CloseCurrentDialog();
    });

    // 높은 ZOrder로 뷰포트에 추가
    CurrentDialog->AddToViewport(10000);

    // NativePreConstruct 가 아이콘을 다시 숨기므로 뷰포트 추가 뒤에 적용한다
    CurrentDialog->SetTitleIconType(Icon);
    CurrentDialog->PlayOverlayAppear();

    UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ConfirmDialog created: %s"), *Title.ToString());

    return CurrentDialog;
}

UConfirmCancelWidget* UUIManagerSubsystem::CreateAlertDialog(const FText& Title, const FText& Message, EConfirmDialogIcon Icon)
{
    UConfirmCancelWidget* Dialog = CreateConfirmDialog(Title, Message, Icon);
    if (Dialog)
    {
        Dialog->SetConfirmOnly(true);
    }
    return Dialog;
}
