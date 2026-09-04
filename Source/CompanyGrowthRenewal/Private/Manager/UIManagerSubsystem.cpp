// Fill out your copyright notice in the Description page of Project Settings.


#include "Manager/UIManagerSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
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
#include "UI/Panel/WorldMapLayerWidget.h"
#include "UI/Element/Notifications/NotificationContainerWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Panel/RewardRevealPresentationWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Enum/ResourceType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ResourceInfo.h"
#include "Engine/Texture2D.h"

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
    UIWorldMapLayer = nullptr;

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
            }
            UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowMainMapUI"));

            // 레이어 생성 완료 — 미션 트래커/가이드는 이 신호 뒤에 빌드 (next-tick 경쟁/깜빡임 제거)
            OnLevelLayerReady.Broadcast();

            // 오프라인 정산 리포트가 보류 중이면 지금(UIBase 준비 완료) 표시 시도
            TryShowOfflineReport();
        });
}

// 정산 모달 표시 단일 관문. 로그인·복귀 경로 합류점
void UUIManagerSubsystem::TryShowOfflineReport()
{
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
        // DT_WidgetClass 미등록이면 스킵 (보류 유지, 등록 후 자동 표시)
        UE_LOG(LogTemp, Warning,
            TEXT("[UIManagerSubsystem] OfflineReportModal 클래스 없음 — DT_WidgetClass 행 확인. 이번 표시 스킵."));
        return;
    }

    // 모달이 NativeConstruct에서 구독 → Consume 1회 발화 → SetReportData 수신. push(구독) 먼저, Consume 나중
    UIBaseInstance->PushPromptClass(ModalClass.Get());
    SaveLoadMgr->ConsumePendingOfflineReport();

    if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->GoToUIMode();
    }
}

void UUIManagerSubsystem::ShowOfficeUI()
{
    ClearAllUI();

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
		if (TSubclassOf<UUserWidget> LayerClass = TableMgr->GetWidgetClass(EWidgetType::WorldMapLayer))
		{
			UIWorldMapLayer = Cast<UWorldMapLayerWidget>(UIBaseInstance->PushMenuClass(LayerClass.Get()));
			UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ShowWorldMapUI - WorldMapLayer created"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[UIManagerSubsystem] ShowWorldMapUI - WorldMapLayer Widget class not found in DataTable"));
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
        NSLOCTEXT("UIManager", "NotEnoughResource", "{0}이(가) {1} 부족합니다"),
        ResInfo.DisplayName, MissingText));
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
    FSimpleDelegate OnCancel)
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

    UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ConfirmDialog shown: %s"), *Title.ToString());

    return CurrentDialog;
}

UConfirmCancelWidget* UUIManagerSubsystem::ShowAlertDialog(
    const FText& Title,
    const FText& Message,
    FSimpleDelegate OnConfirm)
{
    UConfirmCancelWidget* Dialog = ShowConfirmDialog(Title, Message, OnConfirm);
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

UConfirmCancelWidget* UUIManagerSubsystem::CreateConfirmDialog(const FText& Title, const FText& Message)
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

    UE_LOG(LogTemp, Log, TEXT("[UIManagerSubsystem] ConfirmDialog created: %s"), *Title.ToString());

    return CurrentDialog;
}

UConfirmCancelWidget* UUIManagerSubsystem::CreateAlertDialog(const FText& Title, const FText& Message)
{
    UConfirmCancelWidget* Dialog = CreateConfirmDialog(Title, Message);
    if (Dialog)
    {
        Dialog->SetConfirmOnly(true);
    }
    return Dialog;
}
