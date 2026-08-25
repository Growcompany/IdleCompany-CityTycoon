// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Notifications/NotificationContainerWidget.h"
#include "UI/Element/Notifications/NotificationElementWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UNotificationContainerWidget::UNotificationContainerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNotificationContainerWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

namespace
{
	// 이 시간 안에 같은 문구가 다시 오면 중복으로 보고 새 줄을 만들지 않는다
	constexpr double DuplicateNotificationWindow = 1.5;
}

void UNotificationContainerWidget::AddNotification(const FText& Message, float Duration, FLinearColor TextColor)
{
	AddNotificationInternal(Message, nullptr, Duration, TextColor, EWidgetType::NotificationElement);
}

void UNotificationContainerWidget::AddIconNotification(
	const FText& Message,
	UTexture2D* Icon,
	float Duration,
	FLinearColor TextColor)
{
	AddNotificationInternal(Message, Icon, Duration, TextColor, EWidgetType::FundsToast);
}

void UNotificationContainerWidget::AddNotificationInternal(
	const FText& Message,
	UTexture2D* Icon,
	float Duration,
	FLinearColor TextColor,
	EWidgetType WidgetType)
{
	if (!VBox_Notifications)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	// 직전 알림과 같은 문구면 그 엘리먼트의 표시 시간만 되돌려 스택이 쌓이는 것을 막는다
	UNotificationElementWidget* LastWidget = LastNotificationWidget.Get();
	if (IsValid(LastWidget) && ActiveNotifications.Contains(LastWidget)
		&& LastNotificationMessage.EqualTo(Message)
		&& LastNotificationWidgetType == WidgetType
		&& LastNotificationIcon.Get() == Icon
		&& (Now - LastNotificationTime) < DuplicateNotificationWindow)
	{
		LastNotificationTime = Now;
		LastWidget->RefreshDisplay(Duration, TextColor);
		return;
	}

	// TableManager에서 NotificationElement 클래스 가져오기
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("NotificationContainerWidget: TableManagerSubsystem not found!"));
		return;
	}

	TSubclassOf<UUserWidget> ElementClass = TableMgr->GetWidgetClass(WidgetType);
	if (!ElementClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("NotificationContainerWidget: Widget type %d not found in DataTable!"),
			static_cast<int32>(WidgetType));
		return;
	}

	// 새 알림 위젯 생성
	UNotificationElementWidget* NewNotification = CreateWidget<UNotificationElementWidget>(this, ElementClass);
	if (!NewNotification)
	{
		UE_LOG(LogTemp, Warning, TEXT("NotificationContainerWidget: Failed to create notification widget!"));
		return;
	}

	// 종료 콜백 바인딩
	NewNotification->OnNotificationFinished.BindUObject(this, &UNotificationContainerWidget::OnNotificationFinished);

	// VBox 상단(인덱스 0)에 삽입하여 새 알림이 위에 표시되도록
	TArray<UWidget*> ExistingChildren;
	for (int32 i = 0; i < VBox_Notifications->GetChildrenCount(); i++)
	{
		ExistingChildren.Add(VBox_Notifications->GetChildAt(i));
	}
	VBox_Notifications->ClearChildren();
	VBox_Notifications->AddChild(NewNotification);
	for (UWidget* Child : ExistingChildren)
	{
		VBox_Notifications->AddChild(Child);
	}

	// 슬롯 설정 (간격 적용)
	if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(NewNotification->Slot))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, NotificationSpacing));
	}

	// 활성 알림 목록에 추가 (맨 앞에 추가하여 순서 유지)
	ActiveNotifications.Insert(NewNotification, 0);

	// 최대 개수 초과 시 오래된 알림 제거
	RemoveOldestNotifications();

	// 알림 초기화 및 애니메이션 시작
	if (WidgetType == EWidgetType::FundsToast)
	{
		NewNotification->ShowIconMessage(Message, Icon, Duration, TextColor);
	}
	else
	{
		NewNotification->ShowMessage(Message, Duration, TextColor);
	}

	LastNotificationMessage = Message;
	LastNotificationTime = Now;
	LastNotificationWidget = NewNotification;
	LastNotificationWidgetType = WidgetType;
	LastNotificationIcon = Icon;
}

void UNotificationContainerWidget::ClearAllNotifications()
{
	// 모든 활성 알림에 대해 ForceHide 호출
	for (UNotificationElementWidget* Notification : ActiveNotifications)
	{
		if (Notification && Notification->IsValidLowLevel())
		{
			Notification->OnNotificationFinished.Unbind();
			Notification->ForceHide();
			Notification->RemoveFromParent();
		}
	}

	ActiveNotifications.Empty();
	LastNotificationMessage = FText::GetEmpty();
	LastNotificationTime = 0.0;
	LastNotificationWidget = nullptr;
	LastNotificationWidgetType = EWidgetType::None;
	LastNotificationIcon = nullptr;
}

void UNotificationContainerWidget::ShowGuideNotification(const FText& Message, FLinearColor TextColor)
{
	if (!VBox_Notifications)
	{
		return;
	}

	// 같은 문구 재호출 = 유지 (폴링 리프레시가 매번 슬라이드-인을 재생하지 않도록)
	if (GuideNotification && GuideMessageCache.EqualTo(Message))
	{
		return;
	}

	if (!GuideNotification)
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		TSubclassOf<UUserWidget> ElementClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::NotificationElement) : nullptr;
		if (!ElementClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("NotificationContainerWidget: NotificationElement 클래스 없음 — 가이드 알림 생략"));
			return;
		}

		GuideNotification = CreateWidget<UNotificationElementWidget>(this, ElementClass);
		if (!GuideNotification)
		{
			return;
		}
		GuideNotification->OnNotificationFinished.BindUObject(this, &UNotificationContainerWidget::OnGuideNotificationFinished);

		VBox_Notifications->AddChild(GuideNotification);
		if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(GuideNotification->Slot))
		{
			VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, NotificationSpacing));
		}
	}

	GuideMessageCache = Message;
	// 사실상 무한 지속 — ClearGuideNotification 의 ForceHide 가 닫을 때까지 유지
	GuideNotification->ShowMessage(Message, 1.0e9f, TextColor);
}

void UNotificationContainerWidget::ClearGuideNotification()
{
	GuideMessageCache = FText::GetEmpty();
	if (GuideNotification)
	{
		// HidingOut 애니 후 OnGuideNotificationFinished 에서 제거/정리
		GuideNotification->ForceHide();
	}
}

void UNotificationContainerWidget::OnGuideNotificationFinished(UNotificationElementWidget* FinishedWidget)
{
	if (FinishedWidget)
	{
		FinishedWidget->OnNotificationFinished.Unbind();
		FinishedWidget->RemoveFromParent();
	}
	if (GuideNotification == FinishedWidget)
	{
		GuideNotification = nullptr;
		GuideMessageCache = FText::GetEmpty();
	}
}

void UNotificationContainerWidget::OnNotificationFinished(UNotificationElementWidget* FinishedWidget)
{
	if (!FinishedWidget)
	{
		return;
	}

	// 델리게이트 언바인딩
	FinishedWidget->OnNotificationFinished.Unbind();

	// 활성 목록에서 제거
	ActiveNotifications.Remove(FinishedWidget);

	// 이 엘리먼트가 dedup 기준이었다면 문구도 함께 비워야 다음 같은 문구가 새 알림으로 뜬다
	if (LastNotificationWidget.Get() == FinishedWidget)
	{
		LastNotificationWidget = nullptr;
		LastNotificationMessage = FText::GetEmpty();
		LastNotificationWidgetType = EWidgetType::None;
		LastNotificationIcon = nullptr;
	}

	// 위젯 제거
	FinishedWidget->RemoveFromParent();
}

void UNotificationContainerWidget::RemoveOldestNotifications()
{
	// 최대 개수를 초과하는 경우 오래된 알림부터 제거
	while (ActiveNotifications.Num() > MaxVisibleNotifications)
	{
		// 가장 오래된 알림은 배열의 마지막 (가장 먼저 추가된 것)
		int32 LastIndex = ActiveNotifications.Num() - 1;
		UNotificationElementWidget* OldestNotification = ActiveNotifications[LastIndex];

		if (OldestNotification && OldestNotification->IsValidLowLevel())
		{
			// 콜백 언바인딩 후 강제 숨기기
			OldestNotification->OnNotificationFinished.Unbind();
			OldestNotification->ForceHide();

			// 배열에서 제거 (ForceHide 후 RemoveFromParent는 해당 위젯 내부에서 처리됨)
			// 하지만 이 경우 콜백이 언바인딩되었으므로 여기서 직접 제거
			OldestNotification->RemoveFromParent();
		}

		ActiveNotifications.RemoveAt(LastIndex);
	}
}
