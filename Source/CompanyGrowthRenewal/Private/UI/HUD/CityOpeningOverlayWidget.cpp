#include "UI/HUD/CityOpeningOverlayWidget.h"

#include "CommonTextBlock.h"
#include "GameMode/CityOpeningCamera.h"
#include "UI/Element/Buttons/ButtonWidget.h"

UCityOpeningOverlayWidget::UCityOpeningOverlayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CityCaption(NSLOCTEXT("CityOpening", "CityCaption", "이 도시에 당신의 회사를 세워나가세요."))
	, FactoryCaption(NSLOCTEXT("CityOpening", "FactoryCaption", "첫 건물을 지을 벽돌부터 모아주세요."))
{
}

void UCityOpeningOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (SkipButton)
	{
		SkipButton->OnClicked().AddUObject(this, &UCityOpeningOverlayWidget::HandleSkipClicked);
	}
	SetOpeningProgress(0.0f);
}

void UCityOpeningOverlayWidget::NativeDestruct()
{
	if (SkipButton)
	{
		SkipButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UCityOpeningOverlayWidget::SetOpeningProgress(float ElapsedSeconds)
{
	if (!CaptionText)
	{
		return;
	}

	const float Seconds = FMath::Max(0.0f, ElapsedSeconds);
	CaptionText->SetText(Seconds < CityOpeningCamera::FactoryCut ? CityCaption : FactoryCaption);
	constexpr float FadeSeconds = .25f;
	SetRenderOpacity(FMath::Clamp((CityOpeningCamera::Duration - Seconds) / FadeSeconds, 0.0f, 1.0f));
}

void UCityOpeningOverlayWidget::HandleSkipClicked()
{
	OnSkip.Broadcast();
}
