// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "GameplayTagContainer.h"
#include "Enum/NotificationType.h"
#include "UIManagerSubsystem.generated.h"

class UUserWidget;
class UUIBase;
class ULoadingWidget;
class UInGameLayerWidget;
class UOfficeLayerWidget;
class UOfficeMainWidget;
class UWorldMapLayerWidget;
class UNotificationContainerWidget;
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
    UWorldMapLayerWidget* UIWorldMapLayer = nullptr;

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
    // 두 곳에서 호출: ShowMainMapUI 끝(로그인) + CalculateOfflineGains 끝(포그라운드 복귀, UI 이미 준비).
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

    // 거부 알림 — 같은 문구는 쿨다운 안에 1회만. 홀드(10Hz) 연타와 Pressed+Clicked 이중 발화를 함께 막는다.
    // Type 은 "~부족합니다/~할 수 없습니다" 류 거부면 Failed, "이미 ~입니다" 류 상태 안내면 Warning.
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowRejectNotification(const FText& Message, float CooldownSec = 0.4f, ENotificationType Type = ENotificationType::Failed);

    // 자원 부족 거부 — 호출 시점의 실제 부족분으로 문구를 만든다. DT_Resource 행이 없으면 알림 생략.
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void NotifyInsufficientResource(EResourceType Type, int64 Need);

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
        FSimpleDelegate OnCancel = FSimpleDelegate()
    );

    // 확인만 있는 대화상자 (C++ 전용 - 콜백 포함)
    UConfirmCancelWidget* ShowAlertDialog(
        const FText& Title,
        const FText& Message,
        FSimpleDelegate OnConfirm = FSimpleDelegate()
    );

    // 색상 지정 알림 표시 (등급 등 커스텀 색상)
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowColoredNotification(const FText& Message, float Duration, FLinearColor TextColor);

    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ShowFundsToast(int64 Delta, float Duration = 2.0f);

    void ShowRewardToast(const TArray<FMissionReward>& Rewards, const FText& Title, bool bShowTitle);

    // 모든 알림 제거
    UFUNCTION(BlueprintCallable, Category = "UI|Notification")
    void ClearAllNotifications();

    // 현재 열린 대화상자 닫기
    UFUNCTION(BlueprintCallable, Category = "UI|Dialog")
    void CloseCurrentDialog();

    // 대화상자 위젯 직접 반환 (블루프린트에서 델리게이트 직접 바인딩용)
    UFUNCTION(BlueprintCallable, Category = "UI|Dialog")
    UConfirmCancelWidget* CreateConfirmDialog(const FText& Title, const FText& Message);

    // Alert 위젯 직접 반환 (블루프린트용)
    UFUNCTION(BlueprintCallable, Category = "UI|Dialog")
    UConfirmCancelWidget* CreateAlertDialog(const FText& Title, const FText& Message);

private:
    // 다음 틱으로 초기화 지연용 타이머 핸들
    FTimerHandle InitTimerHandle;
    // Base UI 준비 여부
    bool bIsInitialized = false;

    // ResourceItemManager 구독 핸들
    FDelegateHandle ResourceManagerHandle;

    // ResourceItemManager의 리소스 변경을 받아서 위젯들에게 전달
    void HandleResourceManagerChanged(EResourceType Type, int64 NewValue);

    // 거부 알림 문구별 마지막 표시 시각 (FPlatformTime::Seconds 기준 — 레벨 전환에도 리셋되지 않는다)
    TMap<FString, double> RejectNotifyTimes;

    // 알림 컨테이너 위젯
    UPROPERTY()
    UNotificationContainerWidget* NotificationContainer = nullptr;

    // 현재 열린 대화상자
    UPROPERTY()
    UConfirmCancelWidget* CurrentDialog = nullptr;

    // 알림 컨테이너 생성 또는 반환
    UNotificationContainerWidget* GetOrCreateNotificationContainer();

    // 타입별 마지막 알림음 재생 시각 — 같은 타입 연타가 소리로 겹치는 것 차단
    TMap<ENotificationType, double> LastNotificationSoundTime;
};
