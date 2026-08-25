#include "UI/Element/Office/ReviewReactionToastWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "CommonTextBlock.h"

void UReviewReactionToastWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 레일 계약: 입력 통과. WBP 편집에 지지 않게 C++ 에서 못박는다.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);
}

void UReviewReactionToastWidget::SetupReaction(const FLaunchReaction& Reaction)
{
	const bool bCritic = (Reaction.Kind == FName(TEXT("Critic")));
	if (bCritic)
	{
		SetCriticData(Reaction.Source, Reaction.Score, Reaction.Comment);
	}
	else
	{
		SetSnsData(Reaction.Source, Reaction.Comment);
	}

	if (KindText)
	{
		KindText->SetText(bCritic ? NSLOCTEXT("LaunchReaction", "KindCritic", "비평가") : NSLOCTEXT("LaunchReaction", "KindSns", "SNS"));
	}

	const FLinearColor Accent = (Reaction.Band == FName(TEXT("High"))) ? DarkHigh : (Reaction.Band == FName(TEXT("Mid"))) ? DarkMid : DarkLow;
	if (AccentBar) { AccentBar->SetColorAndOpacity(Accent); }
	if (NameText) { NameText->SetColorAndOpacity(FSlateColor(Accent)); }
	if (ScoreBadge) { ScoreBadge->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.08f)); }   // 다크 칩: 배지는 무채 글래스, 색은 이름이 맡는다

	Clock = 0.f;
	bRemoveRequested = false;
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderOpacity(0.f);
	SetRenderTranslation(FVector2D(SlideDistance, 0.f));
}

void UReviewReactionToastWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Clock < 0.f)
	{
		return;
	}
	Clock += InDeltaTime;

	if (Clock < SlideInSeconds)
	{
		const float P = Clock / SlideInSeconds;
		const float E = 1.f - FMath::Pow(1.f - P, 3.f);
		SetRenderOpacity(P);
		SetRenderTranslation(FVector2D((1.f - E) * SlideDistance, 0.f));
		return;
	}

	const float HoldEnd = SlideInSeconds + HoldSeconds;
	if (Clock < HoldEnd)
	{
		SetRenderOpacity(1.f);
		SetRenderTranslation(FVector2D::ZeroVector);
		return;
	}

	const float FadeEnd = HoldEnd + FadeOutSeconds;
	if (Clock < FadeEnd)
	{
		const float P = (Clock - HoldEnd) / FadeOutSeconds;
		SetRenderOpacity(1.f - P);
		SetRenderTranslation(FVector2D(P * SlideDistance * 0.6f, 0.f));
		return;
	}

	if (!bRemoveRequested)
	{
		// 만료 요청은 1회 — Clock 정지로 다음 틱부터 조기 반환(제거가 다음 프레임에 와도 재발화 없음)
		bRemoveRequested = true;
		Clock = -1.f;
		OnRailRemoveRequested.ExecuteIfBound();
	}
}
