#include "UI/Element/Common/AlertMarkWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Utils/FWidgetAnimationUtils.h"

void UAlertMarkWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyBrush();
}

void UAlertMarkWidget::SetMark(EAlertMarkSize InSize, EAlertMarkColor InColor)
{
	Size = InSize;
	Color = InColor;
	ApplyBrush();
}

void UAlertMarkWidget::SetSize(EAlertMarkSize InSize)
{
	Size = InSize;
	ApplyBrush();
}

void UAlertMarkWidget::SetColor(EAlertMarkColor InColor)
{
	Color = InColor;
	ApplyBrush();
}

void UAlertMarkWidget::Show()
{
	const bool bWasHidden = !IsShown();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (!bWasHidden)
	{
		return;   // 이미 보이는 상태의 재호출 — 5초 주기 재평가가 깜빡임이 되지 않게
	}

	if (FMotionGate::IsReduced())
	{
		SetRenderScale(FVector2D(1.f, 1.f));
		return;
	}
	PopElapsed = 0.f;
	SetRenderScale(FVector2D(PopStartScale, PopStartScale));   // 첫 틱 전 원 크기로 한 프레임 비치는 것 방지
}

void UAlertMarkWidget::Hide()
{
	PopElapsed = -1.f;
	SetRenderScale(FVector2D(1.f, 1.f));   // 다음 Show 가 깨끗한 상태에서 시작한다
	SetVisibility(ESlateVisibility::Collapsed);
}

void UAlertMarkWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PopElapsed < 0.f)
	{
		return;   // 26개 소비처에 상시 계산을 남기지 않는다
	}

	PopElapsed += InDeltaTime;
	const float A = FMath::Clamp(PopElapsed / PopDuration, 0.f, 1.f);
	const float S = PopStartScale + (1.f - PopStartScale) * FWidgetAnimationUtils::EaseOutQuad(A);
	SetRenderScale(FVector2D(S, S));
	if (A >= 1.f)
	{
		PopElapsed = -1.f;
		SetRenderScale(FVector2D(1.f, 1.f));
	}
}

bool UAlertMarkWidget::IsShown() const
{
	return GetVisibility() != ESlateVisibility::Collapsed
		&& GetVisibility() != ESlateVisibility::Hidden;
}

void UAlertMarkWidget::ApplyBrush()
{
	if (!IsValid(DotImage))
	{
		return;
	}

	UTexture2D* Tex = PickTexture();
	if (!IsValid(Tex))
	{
		DotImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	DotImage->SetBrushFromTexture(Tex);
	// Size enum별 설정된 픽셀 크기 강제 — 텍스처 원본이 너무 크더라도 일정 규격
	const FVector2D Target = (Size == EAlertMarkSize::Small) ? SmallSize : LargeSize;
	DotImage->SetDesiredSizeOverride(Target);
	DotImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

UTexture2D* UAlertMarkWidget::PickTexture() const
{
	switch (Size)
	{
	case EAlertMarkSize::Small:
		return (Color == EAlertMarkColor::Red) ? Tex_SmallRed : Tex_SmallGreen;
	case EAlertMarkSize::Large:
		return (Color == EAlertMarkColor::Red) ? Tex_LargeRed : Tex_LargeGreen;
	}
	return nullptr;
}
