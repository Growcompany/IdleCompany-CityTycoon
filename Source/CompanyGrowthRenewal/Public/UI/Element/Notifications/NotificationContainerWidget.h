// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/WidgetType.h"
#include "NotificationContainerWidget.generated.h"

class UVerticalBox;
class UNotificationElementWidget;
class UTexture2D;

/**
 * 알림 컨테이너 위젯
 * - 여러 개의 NotificationElementWidget을 관리하는 컨테이너
 * - 새로운 알림은 상단에 추가되고, 최대 개수 초과 시 오래된 알림 제거
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UNotificationContainerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNotificationContainerWidget(const FObjectInitializer& ObjectInitializer);

	// 새로운 알림 추가
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void AddNotification(const FText& Message, float Duration = 3.0f, FLinearColor TextColor = FLinearColor(1.0f, 0.867f, 0.478f, 1.0f));

	UFUNCTION(BlueprintCallable, Category = "Notification")
	void AddIconNotification(const FText& Message, UTexture2D* Icon, float Duration = 3.0f, FLinearColor TextColor = FLinearColor(1.0f, 0.867f, 0.478f, 1.0f));

	// 모든 알림 제거
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void ClearAllNotifications();

	// 가이드(미션) 상주 알림 — 일반 알림과 별개 1개 슬롯, 시간 만료 없이 Clear 호출까지 유지.
	// 같은 문구로 재호출하면 무시(재슬라이드 방지), 문구가 바뀌면 슬라이드-인 재생.
	UFUNCTION(BlueprintCallable, Category = "Notification")
	void ShowGuideNotification(const FText& Message, FLinearColor TextColor = FLinearColor(1.0f, 0.867f, 0.478f, 1.0f));

	UFUNCTION(BlueprintCallable, Category = "Notification")
	void ClearGuideNotification();

protected:
	// BindWidget 멤버
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* VBox_Notifications;

	// 동시에 표시할 수 있는 최대 알림 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification")
	int32 MaxVisibleNotifications = 5;

	// 알림 간 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification")
	float NotificationSpacing = 5.0f;

	virtual void NativeConstruct() override;

private:
	// 현재 활성화된 알림 위젯 목록
	UPROPERTY()
	TArray<UNotificationElementWidget*> ActiveNotifications;

	// 알림 종료 콜백 (알림이 숨겨진 후 호출)
	void OnNotificationFinished(UNotificationElementWidget* FinishedWidget);

	// 오래된 알림 제거 (최대 개수 초과 시)
	void RemoveOldestNotifications();

	// 가이드 상주 알림 엘리먼트 (ActiveNotifications 와 별도 관리 — 최대 개수/자동 만료 미적용)
	UPROPERTY()
	UNotificationElementWidget* GuideNotification = nullptr;

	// 현재 표시 중인 가이드 문구 (동일 문구 재호출 무시용)
	FText GuideMessageCache;

	// 직전 발행 문구/시각 — 짧은 간격의 같은 문구 재발행을 새 엘리먼트 대신 시간 리셋으로 흡수
	FText LastNotificationMessage;
	double LastNotificationTime = 0.0;

	// 그 문구를 실제로 띄운 엘리먼트. 짧은 알림이 먼저 끝나면 인덱스 0 이 다른 알림으로 바뀌므로 위젯을 직접 잡는다
	TWeakObjectPtr<UNotificationElementWidget> LastNotificationWidget;
	EWidgetType LastNotificationWidgetType = EWidgetType::None;
	TWeakObjectPtr<UTexture2D> LastNotificationIcon;

	void AddNotificationInternal(const FText& Message, UTexture2D* Icon, float Duration, FLinearColor TextColor, EWidgetType WidgetType);
	void OnGuideNotificationFinished(UNotificationElementWidget* FinishedWidget);
};
