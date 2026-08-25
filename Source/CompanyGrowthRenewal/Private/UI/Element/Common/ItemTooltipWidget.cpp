// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/ItemTooltipWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "TimerManager.h"

void UItemTooltipWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
	}
	Super::NativeDestruct();
}

void UItemTooltipWidget::ApplyData(const FText& InName, UTexture2D* InIcon, const FText& InDesc, int32 InBasePrice)
{
	if (NameText)
	{
		NameText->SetText(InName);
	}
	if (EntityImage && InIcon)
	{
		EntityImage->SetBrushFromTexture(InIcon);
	}
	if (DescText)
	{
		DescText->SetText(InDesc);
		DescText->SetVisibility(InDesc.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (PriceText)
	{
		if (InBasePrice > 0)
		{
			PriceText->SetText(FText::FromString(FString::Printf(TEXT("기준가 %s"),
				*UGlobalUtilFunctions::FormatExactNumber(InBasePrice).ToString())));
			PriceText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			PriceText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UItemTooltipWidget::ShowAt(FVector2D AbsoluteScreenPos, const FText& InName, UTexture2D* InIcon, const FText& InDesc, int32 InBasePrice, float DurationSec, ETooltipAnchor AnchorMode, int32 ZOrder)
{
	ApplyData(InName, InIcon, InDesc, InBasePrice);

	if (!IsInViewport())
	{
		// 기본 ZOrder 101 = ProductionStartPopup(100) 짝. 보상 리빌(10000)/가챠 리빌(1000) 등 더 높은
		// 오버레이 위에서는 호출자가 host+1 ZOrder 를 넘겨 그 위로 띄운다 (안 그러면 오버레이 뒤에 가려짐).
		AddToViewport(ZOrder);
	}

	// AbsoluteToLocal 로 변환 — SafeZone/DPI/중첩 자동 보정
	const FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D Anchor = ViewportGeo.AbsoluteToLocal(AbsoluteScreenPos);

	ForceLayoutPrepass();
	const FVector2D TooltipSize = GetDesiredSize();
	const FVector2D ViewportSize = ViewportGeo.GetLocalSize();

	// 배치: BelowAnchor = 앵커(클릭 지점) 그대로 아래로 드롭(X 센터링 안 함) / RightOfAnchor = 오른쪽 세로중앙.
	// 화면 밖이면 반대로 플립, 마지막에 가장자리 여백 클램프.
	const float EdgeMargin = 12.0f;
	const float Gap = 10.0f;
	FVector2D Pos;

	if (AnchorMode == ETooltipAnchor::BelowAnchor)
	{
		// 클릭 지점에서 아래로(X는 클릭 X 그대로, 가운데 정렬 X).
		// 아래 공간이 모자라고 위 공간이 더 넉넉할 때만 위로 플립 — 그래야 아래 가장자리 클램프가 플립을 무효화하지 않음.
		Pos.X = Anchor.X;
		const float SpaceBelow = (ViewportSize.Y - EdgeMargin) - (Anchor.Y + Gap);
		const float SpaceAbove = (Anchor.Y - Gap) - EdgeMargin;
		const bool bFlipUp = (TooltipSize.Y > SpaceBelow) && (SpaceAbove > SpaceBelow);
		Pos.Y = bFlipUp ? (Anchor.Y - Gap - TooltipSize.Y) : (Anchor.Y + Gap);
	}
	else
	{
		// 오른쪽 세로중앙 (오른쪽 넘치면 왼쪽으로 플립)
		Pos.X = Anchor.X + Gap;
		if (Pos.X + TooltipSize.X > ViewportSize.X - EdgeMargin)
		{
			Pos.X = Anchor.X - Gap - TooltipSize.X;
		}
		Pos.Y = Anchor.Y - TooltipSize.Y * 0.5f;
	}

	// 가장자리 여백 클램프 (0 이 아니라 EdgeMargin 으로 -> 화면 끝에 딱 붙지 않음)
	Pos.X = FMath::Clamp(Pos.X, EdgeMargin, FMath::Max(EdgeMargin, ViewportSize.X - TooltipSize.X - EdgeMargin));
	Pos.Y = FMath::Clamp(Pos.Y, EdgeMargin, FMath::Max(EdgeMargin, ViewportSize.Y - TooltipSize.Y - EdgeMargin));

	SetPositionInViewport(Pos, /*bRemoveDPIScale=*/false);

	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (ShowAnim)
	{
		PlayAnimation(ShowAnim);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
		if (DurationSec > 0.f)
		{
			World->GetTimerManager().SetTimer(DismissTimerHandle, this, &UItemTooltipWidget::HandleAutoDismiss, DurationSec, false);
		}
	}
}

void UItemTooltipWidget::HandleAutoDismiss()
{
	Hide();
}

void UItemTooltipWidget::Hide()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DismissTimerHandle);
	}

	if (HideAnim)
	{
		PlayAnimation(HideAnim);
		// post-anim Collapsed: 다음 ShowAt 가 ClearTimer 로 즉시 취소할 수 있도록 멤버 핸들 재사용
		const float AnimDur = HideAnim->GetEndTime();
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UItemTooltipWidget> WeakThis(this);
			World->GetTimerManager().SetTimer(DismissTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (UItemTooltipWidget* Strong = WeakThis.Get())
				{
					Strong->SetVisibility(ESlateVisibility::Collapsed);
				}
			}), AnimDur, false);
		}
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
