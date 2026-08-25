// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonTextBlock.h"

void UConfirmCancelWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (TitleText)
    {
        TitleText->SetText(DefaultTitle);
    }

    // 버튼 텍스트/스타일은 런타임에도 기본값 적용 (SetConfirmButtonText로 덮어쓸 수 있음)
    if (ConfirmButton)
    {
        ConfirmButton->SetButtonText(DefaultConfirmText);
        if (ConfirmButtonStyle)
        {
            ConfirmButton->SetStyle(ConfirmButtonStyle);
        }
    }
    if (CancelButton)
    {
        CancelButton->SetButtonText(DefaultCancelText);
        if (CancelButtonStyle)
        {
            CancelButton->SetStyle(CancelButtonStyle);
        }
    }
}

void UConfirmCancelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 프롬프트 스택 풀 재사용 시 NativeConstruct 재진입 — 클릭 핸들러 누적 가드 (누적되면 1클릭에 OnConfirm N회 발화)
    if (ConfirmButton)
    {
        ConfirmButton->OnClicked().RemoveAll(this);
        ConfirmButton->OnClicked().AddUObject(this, &UConfirmCancelWidget::OnConfirmButtonClicked);
    }

    if (CancelButton)
    {
        CancelButton->OnClicked().RemoveAll(this);
        CancelButton->OnClicked().AddUObject(this, &UConfirmCancelWidget::OnCancelButtonClicked);
    }
}

void UConfirmCancelWidget::SetMessage(const FText& Message)
{
    if (MessageText)
    {
        MessageText->SetText(Message);
    }
}

void UConfirmCancelWidget::SetTitle(const FText& Title)
{
    if (TitleText)
    {
        TitleText->SetText(Title);
    }
}

void UConfirmCancelWidget::RestoreDefaultTexts()
{
    if (TitleText)
    {
        TitleText->SetText(DefaultTitle);
    }
    if (ConfirmButton)
    {
        ConfirmButton->SetButtonText(DefaultConfirmText);
    }
    if (CancelButton)
    {
        CancelButton->SetButtonText(DefaultCancelText);
    }
}

void UConfirmCancelWidget::SetConfirmOnly(bool bConfirmOnly)
{
    if (CancelButton)
    {
        CancelButton->SetVisibility(bConfirmOnly ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
}

void UConfirmCancelWidget::SetCancelOnly(bool bCancelOnly)
{
    if (ConfirmButton)
    {
        ConfirmButton->SetVisibility(bCancelOnly ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
}

void UConfirmCancelWidget::SetConfirmButtonText(const FText& Text)
{
    if (ConfirmButton)
    {
        ConfirmButton->SetButtonText(Text);
    }
}

void UConfirmCancelWidget::SetCancelButtonText(const FText& Text)
{
    if (CancelButton)
    {
        CancelButton->SetButtonText(Text);
    }
}

void UConfirmCancelWidget::HideMessageText()
{
    if (MessageText)
    {
        MessageText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

UWidget* UConfirmCancelWidget::GetConfirmButtonWidget() const
{
    return ConfirmButton;
}

UWidget* UConfirmCancelWidget::GetCancelButtonWidget() const
{
    return CancelButton;
}

void UConfirmCancelWidget::OnConfirmButtonClicked()
{
    // 닫기 먼저 → 액션 나중. 반대 순서면 콜백이 새 프롬프트를 푸시할 때 스택이 이 위젯을 이미 비활성화해
    // DeactivateWidget 이 무효 → 스택에 잔류 → 위 모달이 닫히는 순간 되살아남 (2026-07-08 운영종료 중복 모달).
    if (bAutoRemove)
    {
        // PushPromptClass 로 올라간 CommonUI 활성화 스택에서 pop. RemoveFromParent 는 스택이 추적 못 해 안 닫힘.
        DeactivateWidget();
    }
    OnConfirm.Broadcast();
}

void UConfirmCancelWidget::OnCancelButtonClicked()
{
    if (bAutoRemove)
    {
        DeactivateWidget();
    }
    OnCancel.Broadcast();
}
