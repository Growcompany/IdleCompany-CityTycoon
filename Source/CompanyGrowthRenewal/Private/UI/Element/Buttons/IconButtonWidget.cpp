// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Buttons/IconButtonWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "CommonTextBlock.h"

void UIconButtonWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    ApplyIconSettings();

    // Text 프로퍼티를 ButtonText 위젯에 적용
    if (ButtonText && !Text.IsEmpty())
    {
        ButtonText->SetText(Text);
    }
}

void UIconButtonWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 선택 가능하도록 설정 (SetIsSelected가 작동하려면 필요)
    SetIsSelectable(true);

    // 토글 비활성화 (클릭 시 자동 토글 방지)
    SetIsToggleable(false);

    // BindWidget 상태 확인
    UE_LOG(LogTemp, Warning, TEXT("[IconButtonWidget] NativeConstruct - IconImage: %s, ButtonText: %s"),
        IconImage ? TEXT("Valid") : TEXT("NULL"),
        ButtonText ? TEXT("Valid") : TEXT("NULL"));

    ApplyIconSettings();
}

void UIconButtonWidget::ApplyIconSettings()
{
    if (!IconImage) return;

    if (IconTexture)
    {
        IconImage->SetBrushFromTexture(IconTexture);
    }

    if (IconSize.X > 0 && IconSize.Y > 0)
    {
        IconImage->SetDesiredSizeOverride(IconSize);
    }
}

void UIconButtonWidget::SetIcon(UTexture2D* NewIcon)
{
    IconTexture = NewIcon;

    if (IconImage && IconTexture)
    {
        IconImage->SetBrushFromTexture(IconTexture);
        UE_LOG(LogTemp, Log, TEXT("[IconButtonWidget] SetIcon: Texture set successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[IconButtonWidget] SetIcon failed - IconImage: %s, IconTexture: %s"),
            IconImage ? TEXT("Valid") : TEXT("NULL"),
            IconTexture ? TEXT("Valid") : TEXT("NULL"));
    }
}

void UIconButtonWidget::SetIconFromSoft(TSoftObjectPtr<UTexture2D> SoftIcon)
{
    if (SoftIcon.ToSoftObjectPath().IsValid())
    {
        UTexture2D* LoadedTexture = SoftIcon.LoadSynchronous();
        UE_LOG(LogTemp, Log, TEXT("[IconButtonWidget] SetIconFromSoft: Path=%s, Loaded=%s"),
            *SoftIcon.ToSoftObjectPath().ToString(),
            LoadedTexture ? TEXT("Success") : TEXT("Failed"));
        SetIcon(LoadedTexture);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[IconButtonWidget] SetIconFromSoft: Invalid path"));
    }
}

void UIconButtonWidget::SetButtonText(const FText& NewText)
{
    Text = NewText;

    if (ButtonText)
    {
        ButtonText->SetText(NewText);
        ButtonText->SetVisibility(ESlateVisibility::HitTestInvisible);
        UE_LOG(LogTemp, Log, TEXT("[IconButtonWidget] SetButtonText: %s"), *NewText.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[IconButtonWidget] SetButtonText failed - ButtonText is NULL!"));
    }
}

void UIconButtonWidget::SetCount(int32 Count)
{
    CurrentCount = Count;

    // SetButtonText를 사용하여 숫자 설정
    SetButtonText(FText::FromString(FString::FromInt(Count)));

    UE_LOG(LogTemp, Log, TEXT("[IconButtonWidget] SetCount: %d"), Count);
}

void UIconButtonWidget::SetSelected(bool bInSelected)
{
    if (bInSelected)
    {
        // 선택
        SetIsSelected(true);
    }
    else
    {
        // 선택 해제 - ClearSelection 시도
        ClearSelection();
        SetIsSelected(false);
    }

    // 디버깅: 실제 선택 상태 확인
    bool ActualState = GetSelected();
    UE_LOG(LogTemp, Warning, TEXT("[IconButtonWidget] SetSelected(%s) → Actual State: %s"),
        bInSelected ? TEXT("True") : TEXT("False"),
        ActualState ? TEXT("True") : TEXT("False"));
}

void UIconButtonWidget::NativeOnClicked()
{
    Super::NativeOnClicked();

    // Interface의 기본 구현 사용
    IButtonSoundInterface::PlayClickSound(this);
}

void UIconButtonWidget::NativeOnHovered()
{
    Super::NativeOnHovered();

    // Interface의 기본 구현 사용
    IButtonSoundInterface::PlayHoverSound(this);
}