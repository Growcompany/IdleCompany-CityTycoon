// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/AnimatedActivatableWidget.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
#include "Utils/FWidgetAnimationUtils.h"

namespace
{
	struct FAppearPopSettings
	{
		float Duration;
		float StartScale;
		float RiseDistance;
	};

	constexpr FAppearPopSettings GetPopSettings(EAppearPopStyle Style)
	{
		return Style == EAppearPopStyle::Compact
			? FAppearPopSettings{0.18f, 0.96f, 12.f}
			: FAppearPopSettings{0.24f, 0.92f, 24.f};
	}
}

void UAnimatedActivatableWidget::NativeOnActivated()
{
	FinishAppearTween();
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
	FinishAppearTween();
	if (UWorld* OwningWorld = GetWorld())
	{
		OwningWorld->GetTimerManager().ClearTimer(CloseFailsafeHandle);
	}

	Super::NativeOnDeactivated();
}

void UAnimatedActivatableWidget::NativeDestruct()
{
	FinishAppearTween();
	Super::NativeDestruct();
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

void UAnimatedActivatableWidget::PlayOverlayAppear()
{
	StartAppearTween(true);
}

void UAnimatedActivatableWidget::StartAppearTween(bool bFadeIn)
{
	FinishAppearTween();
	if (bInstantAppear || FMotionGate::IsReduced())
	{
		return;
	}

	const bool bWantsPop = AppearFrom == EAppearSlideFrom::Bottom && !AppearPopTargetName.IsNone();
	UWidget* PopWidget = bWantsPop ? GetWidgetFromName(AppearPopTargetName) : nullptr;
	if (bWantsPop && !PopWidget)
	{
		// 이름이 UMG 자동 생성분(SizeBox_128 등)이라 WBP 를 손대면 조용히 어긋난다. 그대로 두면
		// 딤까지 같이 밀려 화면 가장자리에 띠가 생기는데, 증상만으로는 원인을 못 찾는다.
		UE_LOG(LogTemp, Warning, TEXT("[Appear] %s: 팝 대상 '%s' 를 WBP 에서 못 찾음 — 위젯 전체 슬라이드로 대체됨."),
			*GetClass()->GetName(), *AppearPopTargetName.ToString());
	}
	bPopping = PopWidget != nullptr;
	UWidget* TweenWidget = bPopping ? PopWidget : GetSlideWidget();
	AppearTweenTarget = TweenWidget;
	AppearOriginalTransform = TweenWidget->GetRenderTransform();
	AppearOriginalPivot = TweenWidget->GetRenderTransformPivot();
	bAppearing = true;
	bFading = bFadeIn;
	AppearElapsed = 0.f;
	AppearOriginalOpacity = GetRenderOpacity();
	if (bFading)
	{
		SetRenderOpacity(0.f);
	}

	FWidgetTransform InitialTransform = AppearOriginalTransform;
	if (bPopping)
	{
		const FAppearPopSettings PopSettings = GetPopSettings(AppearPopStyle);
		TweenWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		InitialTransform.Scale *= PopSettings.StartScale;
		InitialTransform.Translation += FVector2D(0.f, PopSettings.RiseDistance);
	}
	else
	{
		InitialTransform.Translation += GetAppearOffset();
	}
	TweenWidget->SetRenderTransform(InitialTransform);
}

void UAnimatedActivatableWidget::FinishAppearTween()
{
	if (bAppearing)
	{
		if (UWidget* TweenWidget = AppearTweenTarget.Get())
		{
			TweenWidget->SetRenderTransform(AppearOriginalTransform);
			TweenWidget->SetRenderTransformPivot(AppearOriginalPivot);
		}
		if (bFading)
		{
			SetRenderOpacity(AppearOriginalOpacity);
		}
	}
	bAppearing = false;
	bPopping = false;
	bFading = false;
	AppearTweenTarget.Reset();
}

void UAnimatedActivatableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAppearing)
	{
		return;
	}
	if (FMotionGate::IsReduced() || !AppearTweenTarget.IsValid())
	{
		FinishAppearTween();
		return;
	}

	AppearElapsed += InDeltaTime;
	const FAppearPopSettings PopSettings = GetPopSettings(AppearPopStyle);
	const float Duration = bPopping ? PopSettings.Duration : AppearDuration;
	const float T = Duration > 0.f ? FMath::Clamp(AppearElapsed / Duration, 0.f, 1.f) : 1.f;
	const float Eased = 1.f - FMath::Pow(1.f - T, 3.f); // CubicOut
	FWidgetTransform CurrentTransform = AppearOriginalTransform;
	if (bPopping)
	{
		// 오버슈트 이즈(EaseOutBack)는 쓰지 않는다 — 스케일이 1.0 을 넘는 순간 ClipToBounds 부모가
		// 내용판 테두리를 깎고 래스터가 거칠어진다. 커지는 느낌은 시작 스케일로만 낸다.
		CurrentTransform.Scale *= FMath::Lerp(PopSettings.StartScale, 1.f, Eased);
		CurrentTransform.Translation += FVector2D(0.f, PopSettings.RiseDistance * (1.f - Eased));
	}
	else
	{
		CurrentTransform.Translation += GetAppearOffset() * (1.f - Eased);
	}
	AppearTweenTarget->SetRenderTransform(CurrentTransform);
	if (bFading)
	{
		SetRenderOpacity(AppearOriginalOpacity * Eased);
	}

	if (T >= 1.f)
	{
		FinishAppearTween();
	}
}

void UAnimatedActivatableWidget::CloseWithAnimation()
{
	if (bIsClosing)
	{
		return;
	}
	bIsClosing = true;
	FinishAppearTween();

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
