// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/StatRowWidget.h"
#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Components/Image.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/BorderSlot.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

const FLinearColor UStatRowWidget::ColorSuccess = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);   // 초록
const FLinearColor UStatRowWidget::ColorFail = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f);      // 빨강

void UStatRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyDefaultValues();
}

void UStatRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyDefaultValues();
}

void UStatRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 카운트업 애니메이션 업데이트
	if (CountUpAnimation.IsPlaying())
	{
		float CurrentDisplayValue = CountUpAnimation.Tick(InDeltaTime);
		UpdateAnimatedDisplay(CurrentDisplayValue);
	}

	// 스케일 펀치 애니메이션 업데이트
	if (ScalePunchAnimation.IsPlaying())
	{
		float CurrentScale = ScalePunchAnimation.Tick(InDeltaTime);

		// CurrentStatValueText 또는 StatValueText에 스케일 적용
		UWidget* TargetWidget = CurrentStatValueText ? static_cast<UWidget*>(CurrentStatValueText) : static_cast<UWidget*>(StatValueText);
		if (TargetWidget)
		{
			TargetWidget->SetRenderScale(FVector2D(CurrentScale, CurrentScale));
		}
	}
}

void UStatRowWidget::SetStatInfo(const FString& InStatName, const FString& InStatValue)
{
	SetStatName(InStatName);
	SetStatValue(InStatValue);
}

void UStatRowWidget::SetStatName(const FString& InStatName)
{
	if (StatNameText)
	{
		StatNameText->SetText(FText::FromString(InStatName));
	}
}

void UStatRowWidget::SetStatValue(const FString& InStatValue)
{
	if (StatValueText)
	{
		StatValueText->SetText(FText::FromString(InStatValue));
	}
}

void UStatRowWidget::ApplyDefaultValues()
{
	if (StatNameText && !DefaultStatName.IsEmpty())
	{
		StatNameText->SetText(FText::FromString(DefaultStatName));
	}

	if (StatValueText && !DefaultStatValue.IsEmpty())
	{
		StatValueText->SetText(FText::FromString(DefaultStatValue));
	}

	if (FontSize > 0)
	{
		SetFontSize(FontSize);
	}

	if (bOverrideNameColor)
	{
		SetNameColor(FSlateColor(NameColor));
	}

	if (bOverrideValueColor)
	{
		SetValueColor(FSlateColor(ValueColor));
	}

	if (UIE_Separator)
	{
		UIE_Separator->SetVisibility(bShowSeparator
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);

		if (bShowSeparator)
		{
			ApplySeparatorSlotProperties();
		}
	}

	ApplyIconProperties();
}

void UStatRowWidget::ApplyIconProperties()
{
	if (!StatIcon) return;

	// 텍스처: IconTexture 가 비어있으면 아이콘 자체를 Collapsed (자리 차지 X)
	if (!IconTexture.IsNull())
	{
		if (UTexture2D* Tex = IconTexture.LoadSynchronous())
		{
			StatIcon->SetBrushFromTexture(Tex);
			StatIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			StatIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		StatIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 사이즈: (0,0) 이면 WBP 기본 brush 사이즈 유지
	if (IconSize.X > 0.0f && IconSize.Y > 0.0f)
	{
		FSlateBrush Brush = StatIcon->GetBrush();
		Brush.ImageSize = IconSize;
		StatIcon->SetBrush(Brush);
	}

	// 패딩: StatIcon 슬롯의 종류에 따라 분기 (4종 컨테이너 지원)
	UPanelSlot* IconSlot = StatIcon->Slot;
	if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(IconSlot)) { HSlot->SetPadding(IconPadding); }
	else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(IconSlot)) { VSlot->SetPadding(IconPadding); }
	else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(IconSlot))         { OSlot->SetPadding(IconPadding); }
	else if (UBorderSlot* BSlot = Cast<UBorderSlot>(IconSlot))           { BSlot->SetPadding(IconPadding); }
}

void UStatRowWidget::SetStatIcon(UTexture2D* InIcon)
{
	// 컨스트럭트/SynchronizeProperties의 ApplyIconProperties가 IconTexture 기준으로 재적용하므로
	// 런타임 변경도 프로퍼티에 반영 — 안 하면 늦은 컨스트럭트가 아이콘을 도로 Collapsed시킴
	IconTexture = InIcon;

	if (!StatIcon) return;
	if (InIcon)
	{
		StatIcon->SetBrushFromTexture(InIcon);
		StatIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		StatIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UStatRowWidget::SetRowPlateVisible(bool bVisible)
{
	if (!RowPlate) return;
	RowPlate->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UStatRowWidget::SetIconSize(FVector2D InSize)
{
	IconSize = InSize;
	if (!StatIcon || InSize.X <= 0.0f || InSize.Y <= 0.0f) return;
	FSlateBrush Brush = StatIcon->GetBrush();
	Brush.ImageSize = InSize;
	StatIcon->SetBrush(Brush);
}

void UStatRowWidget::SetIconPadding(FMargin InPadding)
{
	IconPadding = InPadding;
	if (!StatIcon) return;
	UPanelSlot* IconSlot = StatIcon->Slot;
	if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(IconSlot)) { HSlot->SetPadding(InPadding); }
	else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(IconSlot)) { VSlot->SetPadding(InPadding); }
	else if (UOverlaySlot* OSlot = Cast<UOverlaySlot>(IconSlot))         { OSlot->SetPadding(InPadding); }
	else if (UBorderSlot* BSlot = Cast<UBorderSlot>(IconSlot))           { BSlot->SetPadding(InPadding); }
}

void UStatRowWidget::ApplySeparatorSlotProperties()
{
	if (!UIE_Separator) return;
	UPanelSlot* SeparatorSlot = UIE_Separator->Slot;
	if (!SeparatorSlot) return;

	UClass* SlotClass = SeparatorSlot->GetClass();

	// Padding (FMargin)
	if (FStructProperty* PaddingProp = CastField<FStructProperty>(SlotClass->FindPropertyByName(TEXT("Padding"))))
	{
		*PaddingProp->ContainerPtrToValuePtr<FMargin>(SeparatorSlot) = SeparatorPadding;
	}

	// HorizontalAlignment / VerticalAlignment (TEnumAsByte → FByteProperty)
	if (FByteProperty* HAlignProp = CastField<FByteProperty>(SlotClass->FindPropertyByName(TEXT("HorizontalAlignment"))))
	{
		*HAlignProp->ContainerPtrToValuePtr<uint8>(SeparatorSlot) = static_cast<uint8>(SeparatorHAlign.GetValue());
	}
	if (FByteProperty* VAlignProp = CastField<FByteProperty>(SlotClass->FindPropertyByName(TEXT("VerticalAlignment"))))
	{
		*VAlignProp->ContainerPtrToValuePtr<uint8>(SeparatorSlot) = static_cast<uint8>(SeparatorVAlign.GetValue());
	}

	// 레이아웃 갱신
	SeparatorSlot->SynchronizeProperties();
}

void UStatRowWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyDefaultValues();
}

void UStatRowWidget::SetCurrentProgress(float CurrentValue, float TargetValue)
{
	// 마지막 표시값 갱신 (값 변경 감지용)
	LastDisplayedValue = CurrentValue;
	CachedTargetValue = TargetValue;

	bool bIsSuccess = CurrentValue >= TargetValue;

	if (CurrentStatValueText)
	{
		// 별도 텍스트가 있으면 현재값만 표시
		CurrentStatValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), CurrentValue)));
		if (bColorizeProgressText)
			CurrentStatValueText->SetColorAndOpacity(bIsSuccess ? ColorSuccess : ColorFail);

		// 목표값은 StatValueText에 "/목표값" 형식으로 표시
		if (StatValueText)
		{
			StatValueText->SetText(FText::FromString(FString::Printf(TEXT("/%.0f"), TargetValue)));
		}
	}
	else if (StatValueText)
	{
		// 별도 텍스트가 없으면 "현재/목표" 형식으로 통합 표시
		FString CombinedText = FString::Printf(TEXT("%.0f / %.0f"), CurrentValue, TargetValue);
		StatValueText->SetText(FText::FromString(CombinedText));
		if (bColorizeProgressText)
			StatValueText->SetColorAndOpacity(bIsSuccess ? ColorSuccess : ColorFail);
	}

	UpdateProgressBar(CurrentValue, TargetValue);
}

void UStatRowWidget::SetCurrentProgressAnimated(float CurrentValue, float TargetValue)
{
	// 목표값 캐시 (색상 판정용)
	CachedTargetValue = TargetValue;

	// 목표값은 StatValueText에 "/목표값" 형식으로 표시 (애니메이션 없이 즉시)
	if (CurrentStatValueText && StatValueText)
	{
		StatValueText->SetText(FText::FromString(FString::Printf(TEXT("/%.0f"), TargetValue)));
	}

	// 현재 표시 중인 값 결정 (애니메이션 중이면 현재 진행값, 아니면 마지막 표시값)
	float FromValue = CountUpAnimation.IsPlaying()
		? CountUpAnimation.GetCurrentValue()
		: LastDisplayedValue;

	// 값이 동일하면 애니메이션 생략 (다른 Step 점수 변경 시 이 Step은 스킵)
	if (FMath::IsNearlyEqual(FromValue, CurrentValue, 0.5f))
	{
		// 애니메이션은 생략하지만 텍스트는 표시 (0일 때도 "0" 표시)
		UpdateAnimatedDisplay(CurrentValue);
		return;
	}

	// 카운트업 애니메이션 시작 (0.3초 동안)
	CountUpAnimation.Start(FromValue, CurrentValue, 0.3f);

	// 스케일 펀치 애니메이션 시작
	ScalePunchAnimation.Start(1.2f, 0.2f);
}

void UStatRowWidget::UpdateAnimatedDisplay(float CurrentDisplayValue)
{
	// 마지막 표시값 갱신 (값 변경 감지용)
	LastDisplayedValue = CurrentDisplayValue;

	bool bIsSuccess = CurrentDisplayValue >= CachedTargetValue;

	if (CurrentStatValueText)
	{
		// 별도 텍스트가 있으면 현재값만 표시
		CurrentStatValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), CurrentDisplayValue)));
		if (bColorizeProgressText)
			CurrentStatValueText->SetColorAndOpacity(bIsSuccess ? ColorSuccess : ColorFail);
	}
	else if (StatValueText)
	{
		// 별도 텍스트가 없으면 "현재/목표" 형식으로 통합 표시
		FString CombinedText = FString::Printf(TEXT("%.0f / %.0f"), CurrentDisplayValue, CachedTargetValue);
		StatValueText->SetText(FText::FromString(CombinedText));
		if (bColorizeProgressText)
			StatValueText->SetColorAndOpacity(bIsSuccess ? ColorSuccess : ColorFail);
	}

	// ProgressBar도 텍스트와 동기화하여 부드럽게 애니메이션
	UpdateProgressBar(CurrentDisplayValue, CachedTargetValue);
}

void UStatRowWidget::UpdateProgressBar(float CurrentValue, float TargetValue)
{
	if (!ProgressBar)
	{
		return;
	}

	float RawPercent = TargetValue > 0.f ? CurrentValue / TargetValue : 0.f;
	float ClampedPercent = FMath::Clamp(RawPercent, 0.f, 1.f);

	ProgressBar->SetPercent(ClampedPercent);
	ProgressBar->SetFillColorAndOpacity(GetProgressBarColor(RawPercent));
}

FLinearColor UStatRowWidget::GetProgressBarColor(float Percent)
{
	// 목표 달성 → 초록
	if (Percent >= 1.0f)
	{
		return FLinearColor(0.18f, 0.78f, 0.35f, 1.0f);
	}

	// 진행률에 따른 그라데이션 (빨강 → 주황 → 파랑)
	if (Percent < 0.5f)
	{
		float T = FMath::Clamp(Percent / 0.5f, 0.f, 1.f);
		return FMath::Lerp(
			FLinearColor(0.9f, 0.25f, 0.2f, 1.0f),   // 빨강
			FLinearColor(1.0f, 0.6f, 0.1f, 1.0f),     // 주황
			T);
	}

	float T = FMath::Clamp((Percent - 0.5f) / 0.5f, 0.f, 1.f);
	return FMath::Lerp(
		FLinearColor(1.0f, 0.6f, 0.1f, 1.0f),     // 주황
		FLinearColor(0.1f, 0.5f, 1.0f, 1.0f),      // 파랑
		T);
}

void UStatRowWidget::SetValueColor(const FSlateColor& InColor)
{
	if (StatValueText)
	{
		StatValueText->SetColorAndOpacity(InColor);
	}
}

void UStatRowWidget::SetNameColor(const FSlateColor& InColor)
{
	if (StatNameText)
	{
		StatNameText->SetColorAndOpacity(InColor);
	}
}

void UStatRowWidget::SetFontSize(int32 Size)
{
	auto ApplySize = [Size](UCommonTextBlock* Text)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	};
	ApplySize(StatNameText);
	ApplySize(StatValueText);
}
