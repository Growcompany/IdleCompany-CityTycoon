// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Buttons/ButtonWidget.h"
#include "Engine/Texture2D.h"
#include "Components/PanelSlot.h"
#include "Components/BorderSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"

#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Manager/UIManagerSubsystem.h"

void UButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsValid(ButtonText))
	{
		ButtonText->SetText(Text);

		// 텍스트 크기가 설정되어 있으면 적용
		if (TextSize > 0.0f)
		{
			SetTextSize(TextSize);
		}

		// 텍스트 패딩 적용
		SetTextPadding(TextPadding);

		// 텍스트 색상 적용
		ButtonText->SetColorAndOpacity(TextColor);

		// 텍스트 아웃라인 적용
		ApplyTextOutline();
	}
}

void UButtonWidget::SetButtonText(const FText& NewText)
{
	Text = NewText;

	if (IsValid(ButtonText))
	{
		ButtonText->SetText(NewText);
	}
}

void UButtonWidget::SetTextSize(float NewSize)
{
	TextSize = NewSize;

	if (IsValid(ButtonText) && NewSize > 0.0f)
	{
		// 현재 폰트 정보 가져오기
		FSlateFontInfo FontInfo = ButtonText->GetFont();
		FontInfo.Size = NewSize;
		ButtonText->SetFont(FontInfo);
	}
}

void UButtonWidget::SetTextPadding(FMargin NewPadding)
{
	TextPadding = NewPadding;

	if (IsValid(ButtonText))
	{
		if (UPanelSlot* PanelSlot = ButtonText->Slot)
		{
			// OverlaySlot인 경우
			if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
			{
				OverlaySlot->SetPadding(NewPadding);
			}
			// BorderSlot인 경우
			else if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(PanelSlot))
			{
				BorderSlot->SetPadding(NewPadding);
			}
			// HorizontalBoxSlot인 경우
			else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
			{
				HBoxSlot->SetPadding(NewPadding);
			}
			// VerticalBoxSlot인 경우
			else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
			{
				VBoxSlot->SetPadding(NewPadding);
			}
		}
	}
}

void UButtonWidget::SetTextColor(FSlateColor NewColor)
{
	TextColor = NewColor;

	if (IsValid(ButtonText))
	{
		ButtonText->SetColorAndOpacity(NewColor);
	}
}

void UButtonWidget::NativeOnClicked()
{
	// Super 를 부르지 않으면 OnClicked 브로드캐스트가 일어나지 않는다 — 별도 게이트 없이 동작만 막힌다.
	if (HasRejectReason())
	{
		PlayRejectFeedback();
		return;
	}

	Super::NativeOnClicked();

	// Interface의 기본 구현 사용
	IButtonSoundInterface::PlayClickSound(this);

	PressPunchElapsed = 0.f;
}

void UButtonWidget::NativeOnHovered()
{
	Super::NativeOnHovered();

	// Interface의 기본 구현 사용
	IButtonSoundInterface::PlayHoverSound(this);
}

void UButtonWidget::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	OnIsSelectedChanged.Broadcast(true);
}

void UButtonWidget::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	OnIsSelectedChanged.Broadcast(false);
}

void UButtonWidget::SetTextOutlineEnabled(bool bEnabled, float Size, FLinearColor Color)
{
	bEnableTextOutline = bEnabled;
	OutlineSize = Size;
	OutlineColor = Color;

	ApplyTextOutline();
}

void UButtonWidget::UpdateTextOutline(float Size, FLinearColor Color)
{
	OutlineSize = Size;
	OutlineColor = Color;

	if (bEnableTextOutline)
	{
		ApplyTextOutline();
	}
}

void UButtonWidget::SetLetterSpacing(int32 NewSpacing)
{
	if (!IsValid(ButtonText)) return;
	FSlateFontInfo Font = ButtonText->GetFont();
	Font.LetterSpacing = NewSpacing;
	ButtonText->SetFont(Font);
}

void UButtonWidget::ApplyTextOutline()
{
	if (!IsValid(ButtonText))
	{
		return;
	}

	// 현재 폰트 정보 가져오기
	FSlateFontInfo FontInfo = ButtonText->GetFont();

	if (bEnableTextOutline)
	{
		// 아웃라인 활성화
		FontInfo.OutlineSettings.OutlineSize = FMath::Max(0.0f, OutlineSize);
		FontInfo.OutlineSettings.OutlineColor = OutlineColor;
		FontInfo.OutlineSettings.bSeparateFillAlpha = false;
	}
	else
	{
		// 아웃라인 비활성화
		FontInfo.OutlineSettings.OutlineSize = 0;
		FontInfo.OutlineSettings.OutlineColor = FLinearColor::Transparent;
	}

	ButtonText->SetFont(FontInfo);
}

void UButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 셰이크가 도는 동안은 펀치/펄스를 재우지 않으면 같은 RenderTransform 을 두 곳이 덮어쓴다
	if (RejectShakeElapsed >= 0.f)
	{
		UpdateRejectShake(InDeltaTime);
		return;
	}

	UpdatePressPunch(InDeltaTime);
	UpdatePulse(InDeltaTime);
}

void UButtonWidget::SetRejectReason(const FText& Reason)
{
	RejectReason = Reason;
	bRejectInsufficient = false;
}

void UButtonWidget::SetRejectInsufficient(EResourceType Type, int64 Need)
{
	RejectReason = FText::GetEmpty();
	bRejectInsufficient = true;
	RejectResourceType = Type;
	RejectResourceNeed = Need;
}

void UButtonWidget::ClearRejectReason()
{
	RejectReason = FText::GetEmpty();
	bRejectInsufficient = false;
}

bool UButtonWidget::HasRejectReason() const
{
	return bRejectInsufficient || !RejectReason.IsEmpty();
}

void UButtonWidget::PlayRejectFeedback()
{
	RejectShakeElapsed = 0.f;

	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!UIMgr)
	{
		return;
	}

	// 알림 쿨다운에 걸려 토스트가 생략돼도 셰이크는 매번 나간다 — 연타에도 입력이 먹힌 티는 남는다.
	if (bRejectInsufficient)
	{
		UIMgr->NotifyInsufficientResource(RejectResourceType, RejectResourceNeed);
	}
	else
	{
		UIMgr->ShowRejectNotification(RejectReason);
	}
}

void UButtonWidget::UpdateRejectShake(float DeltaTime)
{
	RejectShakeElapsed += DeltaTime;
	const float A = FMath::Clamp(RejectShakeElapsed / RejectShakeDuration, 0.f, 1.f);
	if (A >= 1.f)
	{
		RejectShakeElapsed = -1.f;
		SetRenderTransform(FWidgetTransform());
		return;
	}

	// 좌우 3주기 감쇠 진동 — 끝에서 진폭 0 이라 원위치로 부드럽게 수렴
	const float Offset = FMath::Sin(A * 3.f * 2.f * PI) * RejectShakeAmplitude * (1.f - A);

	FWidgetTransform Xform;
	Xform.Translation = FVector2D(Offset, 0.f);
	SetRenderTransform(Xform);
}

void UButtonWidget::SetPulseEnabled(bool bEnabled)
{
	if (bPulseEnabled == bEnabled) return;

	bPulseEnabled = bEnabled;
	PulseElapsed = 0.f;

	if (!bEnabled && PressPunchElapsed < 0.f)
	{
		SetRenderTransform(FWidgetTransform());
	}
}

void UButtonWidget::UpdatePulse(float DeltaTime)
{
	if (!bPulseEnabled || PressPunchElapsed >= 0.f) return;

	PulseElapsed += DeltaTime;
	const float Phase = FMath::Fmod(PulseElapsed, PulsePeriod) / PulsePeriod;
	// 1 → 1+Amp → 1 코사인 브리딩 (1 아래로 수축하지 않음)
	const float Scale = 1.f + PulseScaleAmp * 0.5f * (1.f - FMath::Cos(Phase * 2.f * PI));

	FWidgetTransform Xform;
	Xform.Scale = FVector2D(Scale, Scale);
	SetRenderTransform(Xform);
}

void UButtonWidget::UpdatePressPunch(float DeltaTime)
{
	if (PressPunchElapsed < 0.f) return;

	PressPunchElapsed += DeltaTime;
	const float A = FMath::Clamp(PressPunchElapsed / PressPunchDuration, 0.f, 1.f);
	if (A >= 1.f)
	{
		PressPunchElapsed = -1.f;
		SetRenderTransform(FWidgetTransform());
		return;
	}

	// EaseOutCubic 으로 눌림에서 원래 크기로 복원
	const float T = 1.f - A;
	const float Eased = 1.f - T * T * T;
	const float Scale = FMath::Lerp(PressPunchMinScale, 1.0f, Eased);

	FWidgetTransform Xform;
	Xform.Scale = FVector2D(Scale, Scale);
	SetRenderTransform(Xform);
}

