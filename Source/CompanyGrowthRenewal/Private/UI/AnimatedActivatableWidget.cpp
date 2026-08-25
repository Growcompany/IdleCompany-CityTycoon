// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/AnimatedActivatableWidget.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"

void UAnimatedActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	bIsClosing = false;

	// 스택 풀 재사용 시 이전 세션의 Hide 종료값(오프스크린 translation/투명)이 속성에 박제된 채 재활성될 수 있음
	// → '안 보이는데 active'로 입력이 잠기는 사고 방지 위해 등장 전 무조건 원복 (2026-07-24 오피스 입력잠금)
	SetRenderOpacity(1.f);
	SetRenderTranslation(FVector2D::ZeroVector);
	if (AppearSlideTarget)
	{
		AppearSlideTarget->SetRenderTranslation(FVector2D::ZeroVector);
	}

	if (UWorld* OwningWorld = GetWorld())
	{
		OwningWorld->GetTimerManager().ClearTimer(CloseFailsafeHandle);
	}

	// 이전 Hide 애니메이션 델리게이트 정리 (위젯 재활성화 시 누적 방지)
	if (Hide)
	{
		FWidgetAnimationDynamicEvent Delegate;
		Delegate.BindDynamic(this, &UAnimatedActivatableWidget::OnHideAnimFinished);
		UnbindFromAnimationFinished(Hide, Delegate);
	}

	if (bInstantAppear)
	{
		// 슬라이드/자체 애님 없이 스택 페이드만
		bAppearing = false;
		return;
	}

	if (Show)
	{
		PlayAnimation(Show);
	}
	else
	{
		StartAppearTween();
	}
}

void UAnimatedActivatableWidget::NativeOnDeactivated()
{
	// 풀 재사용 대비 잔여 상태 정리 (트윈 중단 + failsafe 해제)
	bAppearing = false;
	if (UWorld* OwningWorld = GetWorld())
	{
		OwningWorld->GetTimerManager().ClearTimer(CloseFailsafeHandle);
	}

	Super::NativeOnDeactivated();
}

UWidget* UAnimatedActivatableWidget::GetSlideWidget() const
{
	return AppearSlideTarget ? AppearSlideTarget.Get() : const_cast<UAnimatedActivatableWidget*>(this);
}

FVector2D UAnimatedActivatableWidget::GetAppearOffset() const
{
	switch (AppearFrom)
	{
	case EAppearSlideFrom::Left:  return FVector2D(-AppearRiseDistance, 0.f);
	case EAppearSlideFrom::Right: return FVector2D(AppearRiseDistance, 0.f);
	default:                      return FVector2D(0.f, AppearRiseDistance);
	}
}

void UAnimatedActivatableWidget::StartAppearTween()
{
	bAppearing = true;
	AppearElapsed = 0.f;
	GetSlideWidget()->SetRenderTranslation(GetAppearOffset());
}

void UAnimatedActivatableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAppearing)
	{
		return;
	}

	AppearElapsed += InDeltaTime;
	const float T = (AppearDuration > 0.f) ? FMath::Clamp(AppearElapsed / AppearDuration, 0.f, 1.f) : 1.f;
	const float Eased = 1.f - FMath::Pow(1.f - T, 3.f); // CubicOut
	UWidget* SlideWidget = GetSlideWidget();
	SlideWidget->SetRenderTranslation(GetAppearOffset() * (1.f - Eased));

	if (T >= 1.f)
	{
		bAppearing = false;
		SlideWidget->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UAnimatedActivatableWidget::CloseWithAnimation()
{
	if (bIsClosing)
	{
		return;
	}
	bIsClosing = true;

	if (Hide)
	{
		// finished 바인딩을 재생보다 먼저 — 즉시 완료(0길이/트랙 소실 애님)가 콜백을 유실하지 않도록
		FWidgetAnimationDynamicEvent Delegate;
		Delegate.BindDynamic(this, &UAnimatedActivatableWidget::OnHideAnimFinished);
		BindToAnimationFinished(Hide, Delegate);

		PlayAnimation(Hide);

		// finished 유실(틱 정지/플레이어 소실) 시에도 반드시 Deactivate — 재활성화 시 bIsClosing 리셋이라 오발동 없음
		if (UWorld* OwningWorld = GetWorld())
		{
			OwningWorld->GetTimerManager().SetTimer(CloseFailsafeHandle,
				FTimerDelegate::CreateUObject(this, &UAnimatedActivatableWidget::OnHideAnimFinished),
				Hide->GetEndTime() + 0.5f, false);
		}
	}
	else
	{
		DeactivateWidget();
	}
}

void UAnimatedActivatableWidget::OnHideAnimFinished()
{
	DeactivateWidget();
}
