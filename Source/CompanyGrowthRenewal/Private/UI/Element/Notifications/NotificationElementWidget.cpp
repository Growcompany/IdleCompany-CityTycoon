// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Notifications/NotificationElementWidget.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"

UNotificationElementWidget::UNotificationElementWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNotificationElementWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기 상태 설정 (숨김 상태로 시작)
	SetWidgetAlpha(0.0f);
	SetRenderOffset(SlideDistance);
	CurrentAnimState = ENotificationAnimState::Idle;
}

void UNotificationElementWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	switch (CurrentAnimState)
	{
	case ENotificationAnimState::ShowingIn:
		UpdateShowingIn(InDeltaTime);
		break;
	case ENotificationAnimState::Displaying:
		UpdateDisplaying(InDeltaTime);
		break;
	case ENotificationAnimState::HidingOut:
		UpdateHidingOut(InDeltaTime);
		break;
	case ENotificationAnimState::Idle:
	default:
		break;
	}
}

void UNotificationElementWidget::ShowMessage(const FText& Message, float Duration, FLinearColor TextColor)
{
	if (Image_ResourceIcon)
	{
		Image_ResourceIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 메시지 설정
	if (Text_Message)
	{
		Text_Message->SetText(Message);
		Text_Message->SetColorAndOpacity(FSlateColor(TextColor));
	}

	// 표시 시간 설정
	ActualDisplayDuration = Duration > 0.0f ? Duration : DisplayDuration;

	// 애니메이션 시작
	TransitionToState(ENotificationAnimState::ShowingIn);
}

void UNotificationElementWidget::ShowIconMessage(
	const FText& Message,
	UTexture2D* Icon,
	float Duration,
	FLinearColor TextColor)
{
	if (Image_ResourceIcon)
	{
		if (Icon)
		{
			Image_ResourceIcon->SetBrushFromTexture(Icon);
			Image_ResourceIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			Image_ResourceIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Text_Message)
	{
		Text_Message->SetText(Message);
		Text_Message->SetColorAndOpacity(FSlateColor(TextColor));
	}

	ActualDisplayDuration = Duration > 0.0f ? Duration : DisplayDuration;
	TransitionToState(ENotificationAnimState::ShowingIn);
}

void UNotificationElementWidget::RefreshDisplay(float Duration, FLinearColor TextColor)
{
	if (Text_Message)
	{
		Text_Message->SetColorAndOpacity(FSlateColor(TextColor));
	}

	ActualDisplayDuration = Duration > 0.0f ? Duration : DisplayDuration;
	DisplayElapsedTime = 0.0f;

	// 이미 사라지는 중이면 표시 상태로 되돌린다 (틱이 알파/오프셋을 더는 갱신하지 않으므로 여기서 원복)
	if (CurrentAnimState == ENotificationAnimState::HidingOut)
	{
		SetRenderOffset(0.0f);
		SetWidgetAlpha(1.0f);
		TransitionToState(ENotificationAnimState::Displaying);
	}
}

void UNotificationElementWidget::ForceHide()
{
	if (CurrentAnimState == ENotificationAnimState::HidingOut || CurrentAnimState == ENotificationAnimState::Idle)
	{
		return;
	}

	TransitionToState(ENotificationAnimState::HidingOut);
}

void UNotificationElementWidget::UpdateShowingIn(float DeltaTime)
{
	AnimationElapsedTime += DeltaTime;
	float Progress = FMath::Clamp(AnimationElapsedTime / FadeInDuration, 0.0f, 1.0f);
	float EasedProgress = EaseOutQuad(Progress);

	// Y 오프셋: SlideDistance -> 0
	float YOffset = FMath::Lerp(SlideDistance, 0.0f, EasedProgress);
	SetRenderOffset(YOffset);

	// 알파: 0 -> 1
	SetWidgetAlpha(EasedProgress);

	// 애니메이션 완료 체크
	if (Progress >= 1.0f)
	{
		TransitionToState(ENotificationAnimState::Displaying);
	}
}

void UNotificationElementWidget::UpdateDisplaying(float DeltaTime)
{
	DisplayElapsedTime += DeltaTime;

	// 표시 시간 완료 체크
	if (DisplayElapsedTime >= ActualDisplayDuration)
	{
		TransitionToState(ENotificationAnimState::HidingOut);
	}
}

void UNotificationElementWidget::UpdateHidingOut(float DeltaTime)
{
	AnimationElapsedTime += DeltaTime;
	float Progress = FMath::Clamp(AnimationElapsedTime / FadeOutDuration, 0.0f, 1.0f);
	float EasedProgress = EaseInQuad(Progress);

	// Y 오프셋: 0 -> -30 (위로 슬라이드)
	float TargetOffset = -30.0f;
	float YOffset = FMath::Lerp(0.0f, TargetOffset, EasedProgress);
	SetRenderOffset(YOffset);

	// 알파: 1 -> 0
	SetWidgetAlpha(1.0f - EasedProgress);

	// 애니메이션 완료 체크
	if (Progress >= 1.0f)
	{
		TransitionToState(ENotificationAnimState::Idle);

		// 델리게이트 호출
		if (OnNotificationFinished.IsBound())
		{
			OnNotificationFinished.Execute(this);
		}
	}
}

void UNotificationElementWidget::TransitionToState(ENotificationAnimState NewState)
{
	CurrentAnimState = NewState;
	AnimationElapsedTime = 0.0f;

	if (NewState == ENotificationAnimState::Displaying)
	{
		DisplayElapsedTime = 0.0f;
	}
}

void UNotificationElementWidget::SetRenderOffset(float Offset)
{
	// 우측 레일 토스트는 우측에서 슬라이드-인(X축), 전역 배너는 상단 드롭(Y축) — bSlideFromRight 로 축 선택.
	SetRenderTranslation(bSlideFromRight ? FVector2D(Offset, 0.0f) : FVector2D(0.0f, Offset));
}

void UNotificationElementWidget::SetWidgetAlpha(float Alpha)
{
	SetRenderOpacity(Alpha);
}

float UNotificationElementWidget::EaseOutQuad(float t) const
{
	return 1.0f - (1.0f - t) * (1.0f - t);
}

float UNotificationElementWidget::EaseInQuad(float t) const
{
	return t * t;
}
