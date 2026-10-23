// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "GameplayTagContainer.h"
#include "Enum/NotificationType.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Enum/CompanyTitle.h"
#include "Data/EntryPopupRules.h"
#include "Enum/CountryType.h"
#include "UIManagerSubsystem.generated.h"

class UUserWidget;
class UUIBase;
class ULoadingWidget;
class UInGameLayerWidget;
class UOfficeLayerWidget;
class UOfficeMainWidget;
class ULegacyWorldMapLayerWidget;
class UNotificationContainerWidget;
class UNotificationElementWidget;
class UConfirmCancelWidget;
enum class EResourceType : uint8;
struct FMissionReward;

// Base UI 초기화 완료 시점에 호출되는 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnUIInitialized);

// 리소스 변경 시 UI 업데이트 델리게이트 (중앙 관리)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUIResourceChanged, EResourceType, int64);

// 게임플레이 레이어(InGame/Office) 생성이 끝난 시점 — 미션 위젯 빌드를 이 신호에 매달아 next-tick 경쟁 제거
DECLARE_MULTICAST_DELEGATE(FOnLevelLayerReady);

UCLASS()
class COMPANYGROWTHRENEWAL_API UUIManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 런타임에 생성된 UIBase 인스턴스
    UPROPERTY()
    UUIBase* UIBaseInstance = nullptr;

    // 로딩할 때 뜨는 위젯
    UPROPERTY()
    UUserWidget* LoadingWidget = nullptr;

    // 런타임에 생성된 InGame 레이어 인스턴스
    UPROPERTY()
    UInGameLayerWidget* UIInGameMain = nullptr;

    // Office 레이어
    UPROPERTY()
    UOfficeLayerWidget* UIOfficeLayer = nullptr;

    // Office 메인(하단 스택) — 미션 트래커가 좌레일에 배치돼 MissionManager 가 탐색 대상으로 씀
    UPROPERTY()
    UOfficeMainWidget* UIOfficeMain = nullptr;

    // WorldMap 레이어
    UPROPERTY()
    ULegacyWorldMapLayerWidget* UILegacyWorldMapLayer = nullptr;

    // Base UI 초기화 완료 시점 알림 델리게이트
    FOnUIInitialized OnInitializeSuccess;

    // 게임플레이 레이어(InGame/Office) 생성 완료 신호 — MissionManager 가 구독해 트래커/가이드를 그때 빌드
    FOnLevelLayerReady OnLevelLayerReady;

    // 리소스 변경 알림 델리게이트 (위젯들이 구독)
    FOnUIResourceChanged OnUIResourceChanged;

    // Subsystem 초기화
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // Subsystem 해제
    virtual void Deinitialize() override;

    // 인게임 레이어 위젯 생성 요청
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CreateInGameLayerWidget();

    // Office 레이어 위젯 생성 요청
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CreateOfficeLayerWidget();

    UInGameLayerWidget* GetInGameLayer() const;

    UOfficeLayerWidget* GetOfficeLayer() const;

    UOfficeMainWidget* GetOfficeMain() const;

    UUIBase* GetUIBase() const;

    // 오프라인 정산 모달 표시 시도 (멱등) — 보류 리포트 + UI 준비가 모두 성립할 때만 1회 표시.
    // 세 곳에서 호출: ShowMainMapUI 끝(로그인) + CalculateOfflineGains 끝(포그라운드 복귀, UI 이미 준비) + ShowOfflineReport 치트.
    // 어느 쪽이 먼저 성립하든 HasPendingOfflineReport + Consume 1회성으로 중복 표시 없음.
    void TryShowOfflineReport();

    // 다른 Level로 이동시 UI스택 정리
    void ClearAllUI();

    // 실제 InGame 레이어 생성 로직
    void ShowMainMapUI();

    // Office Level의 메인위젯 생성
    void ShowOfficeUI();

    // WorldMap Level의 메인위젯 생성
    void ShowWorldMapUI();

    // 알림 표시 (타입별 색상 자동 적용)
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowNotification(const FText& Message, float Duration = 3.0f, ENotificationType Type = ENotificationType::Normal);

    void ShowBottomNotification(const FText& Message, float Duration = 2.0f);

    // 거부 알림 — 같은 문구는 쿨다운 안에 1회만. 홀드(10Hz) 연타와 Pressed+Clicked 이중 발화를 함께 막는다.
    // Type 은 "~부족합니다/~할 수 없습니다" 류 거부면 Failed, "이미 ~입니다" 류 상태 안내면 Warning.
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowRejectNotification(const FText& Message, float CooldownSec = 0.4f, ENotificationType Type = ENotificationType::Failed);

    // 자원 부족 거부 — 호출 시점의 실제 부족분으로 문구를 만든다. DT_Resource 행이 없으면 알림 생략.
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void NotifyInsufficientResource(EResourceType Type, int64 Need);

    /**
     * 서버 경제 요청 실패 안내 — **코드별로 갈라** 문구를 고르는 단일 지점(1-B-4c).
     * ① 경제가 잠긴 상태면 3상태 안내(동기화/재시도/영구차단)가 이긴다 ― 잠금은 "부족" 이 아니다.
     * ② `INSUFFICIENT_FUNDS`/`INSUFFICIENT_ITEMS` 만 접점 고유 "부족합니다" 문구를 유지한다.
     * ③ 나머지(네트워크·게이트·서버 내부)는 `DT_CloudErrorText` 문구.
     * 이 분기가 없으면 "부족합니다" 가 서버·네트워크 실패까지 덮어 거짓 원인을 안내한다.
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void NotifyCloudOpFailure(FName Code, EResourceType ShortfallResource, int64 ShortfallNeed);

    /** 위와 같지만 부족 문구를 호출부가 준다(아이템·상태 문구 접점). 빈 FText 면 코드 문구로 떨어진다 */
    void NotifyCloudOpFailureWithText(FName Code, const FText& ShortfallText);

    /** 잠금 상태면 3상태 안내를 내고 true. 두 NotifyCloudOpFailure 가 공유하는 선판정 */
    bool TryNotifyCloudBlock();

    // 미션 가이드 상주 알림 — 자동 만료 없음, Clear 호출까지 유지 (스포트라이트 멘토 라인용)
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowGuideNotification(const FText& Message);

    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ClearGuideNotification();

    // 알림 타입에 따른 색상 반환
    static FLinearColor GetNotificationColor(ENotificationType Type);

    // 알림 타입에 따른 UI 사운드 태그 반환 (DT_UISounds 미등록 태그는 무음으로 안전하게 떨어짐)
    static FGameplayTag GetNotificationSoundTag(ENotificationType Type);

    // 확인/취소 대화상자 표시 (C++ 전용 - 콜백 포함)
    UConfirmCancelWidget* ShowConfirmDialog(
        const FText& Title,
        const FText& Message,
        FSimpleDelegate OnConfirm,
        FSimpleDelegate OnCancel = FSimpleDelegate(),
        EConfirmDialogIcon Icon = EConfirmDialogIcon::None
    );

    // 확인만 있는 대화상자 (C++ 전용 - 콜백 포함)
    UConfirmCancelWidget* ShowAlertDialog(
        const FText& Title,
        const FText& Message,
        FSimpleDelegate OnConfirm = FSimpleDelegate(),
        EConfirmDialogIcon Icon = EConfirmDialogIcon::None
    );

    // 색상 지정 알림 표시 (등급 등 커스텀 색상)
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowColoredNotification(const FText& Message, float Duration, FLinearColor TextColor);

    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowFundsToast(int64 Delta, float Duration = 2.0f);

    void ShowRewardToast(const TArray<FMissionReward>& Rewards, const FText& Title, bool bShowTitle);
    // 보상 띠가 화면 가운데를 가로지르는 동안 튜토리얼 말풍선을 접는 데 쓴다
    bool IsRewardToastShown() const;

    // 모든 알림 제거
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ClearAllNotifications();

    // 현재 열린 대화상자 닫기
    UFUNCTION(BlueprintCallable, Category = "UI|Dialog")
    void CloseCurrentDialog();

    // 대화상자 위젯 직접 반환 (블루프린트에서 델리게이트 직접 바인딩용)
    UFUNCTION(BlueprintCallable, Category = "UI|Dialog")
    UConfirmCancelWidget* CreateConfirmDialog(const FText& Title, const FText& Message, EConfirmDialogIcon Icon = EConfirmDialogIcon::None);

    // Alert 위젯 직접 반환 (블루프린트용)
    UFUNCTION(BlueprintCallable, Category = "UI|Dialog")
    UConfirmCancelWidget* CreateAlertDialog(const FText& Title, const FText& Message, EConfirmDialogIcon Icon = EConfirmDialogIcon::None);

    // 메뉴 타일이 "미션 → 업적 탭"처럼 탭 지정으로 창을 열 때 1회 소비되는 힌트 (창의 NativeOnActivated 가 Consume)
    void SetPendingPanelTab(FName InTab) { PendingPanelTab = InTab; }
    FName ConsumePendingPanelTab() { const FName Out = PendingPanelTab; PendingPanelTab = NAME_None; return Out; }

    // 접속·하루 첫 한가한 순간에 출석(받을 것이 있으면) → 새 공지 창을 하루 한 번씩 스스로 연다. 튜토리얼 중엔 열지 않는다
    void StartEntryPopups();
    // 도시 기본 화면만 떠 있고 다른 창·확인창·연출·전환이 없다
    bool IsCityScreenIdle() const;
    const FEntryPopupSaveData& GetEntryPopupData() const { return EntryPopupSave; }
    void SetEntryPopupData(const FEntryPopupSaveData& InData) { EntryPopupSave = InData; }

    // 오피스 제품 선택의 「공장으로 이동」 — 다음 도시 UI 생성 직후 세계지도를 열고 이 나라에 초점을 맞춘다(1회 소비).
    void RequestWorldMapOnCityReturn(ECountryType FocusCountry);

private:
    void TryOpenPendingWorldMap();
    bool bPendingWorldMapOpen = false;
    ECountryType PendingWorldMapCountry = ECountryType::None;

    void TickEntryPopups();
    bool OpenEntryPopup(EEntryPopup Popup);
    FTimerHandle EntryPopupTimer;
    FEntryPopupSaveData EntryPopupSave;
    double EntryPopupIdleSince = -1.0;
    double EntryPopupToastWaitSince = -1.0;
    // 이번 세션에 튜토리얼을 하고 있었다 ― 끝난 직후엔 출석 하나만 띄운다
    bool bTutorialSeenThisSession = false;

    TWeakObjectPtr<class URewardRevealPresentationWidget> LastRewardToast;

    // 탭 힌트는 창이 열리며 1회 소비된다 — 남겨두면 다음 오픈이 엉뚱한 탭으로 뜬다
    FName PendingPanelTab = NAME_None;

    // 다음 틱으로 초기화 지연용 타이머 핸들
    FTimerHandle InitTimerHandle;
    // Base UI 준비 여부
    bool bIsInitialized = false;

    // ResourceItemManager 구독 핸들
    FDelegateHandle ResourceManagerHandle;

    // ResourceItemManager의 리소스 변경을 받아서 위젯들에게 전달
    void HandleResourceManagerChanged(EResourceType Type, int64 NewValue);

    // 기업 호칭 승격 - 방송은 있는데 듣는 곳이 0 이라 중소->초일류 5단계가 화면에 안 나왔다.
    // 시총 급증 시 while(TryPromoteTitle()) 이 한 호출 안에서 여러 번 방송하므로 다음 틱에 한 번만 띄운다.
    UFUNCTION()
    void HandleCompanyTitleChanged(ECompanyTitle NewTitle);
    void ShowPendingTitlePromotion();
    bool bTitleSubscribed = false;
    bool bTitlePromotionPending = false;
    ECompanyTitle TitlePromotionFrom = ECompanyTitle::Small;
    ECompanyTitle TitlePromotionTo = ECompanyTitle::Small;
    FTimerHandle TitlePromotionTimerHandle;

    // 거부 알림 문구별 마지막 표시 시각 (FPlatformTime::Seconds 기준 — 레벨 전환에도 리셋되지 않는다)
    TMap<FString, double> RejectNotifyTimes;

    // 알림 컨테이너 위젯
    UPROPERTY()
    UNotificationContainerWidget* NotificationContainer = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UNotificationElementWidget> BottomNotification = nullptr;

    void ClearBottomNotification();
    void HandleBottomNotificationFinished(UNotificationElementWidget* FinishedWidget);

    // 현재 열린 대화상자
    UPROPERTY()
    UConfirmCancelWidget* CurrentDialog = nullptr;

    // 알림 컨테이너 생성 또는 반환
    UNotificationContainerWidget* GetOrCreateNotificationContainer();

    // 타입별 마지막 알림음 재생 시각 — 같은 타입 연타가 소리로 겹치는 것 차단
    TMap<ENotificationType, double> LastNotificationSoundTime;
};
