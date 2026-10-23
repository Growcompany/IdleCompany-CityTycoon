// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "CommonTextBlock.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

UConfirmCancelWidget::UConfirmCancelWidget()
{
    AppearPopTargetName = TEXT("SizeBox_128");
    AppearPopStyle = EAppearPopStyle::Compact;
}

void UConfirmCancelWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (PanelHeader)
    {
        // 비어 있으면 건드리지 않는다 — 무조건 덮으면 중첩 UIE_PanelHeader 인스턴스에서 저작한
        // 테마·패턴·사이드 그라데이션이 지워져 팝업 헤더가 기본 모습으로 돌아간다.
        if (!ThemeKey.IsNone() || !HeaderPatternKey.IsNone() || !SideGradientKey.IsNone())
        {
            PanelHeader->SetAppearance(ThemeKey, HeaderPatternKey, SideGradientKey);
        }
        // 이 대화상자에는 닫기 버튼을 두지 않는다(정책이라 항상 적용).
        PanelHeader->SetCloseButtonVisible(false);
    }
}

void UConfirmCancelWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    SetTitle(DefaultTitle);

    SetTitleIconType(EConfirmDialogIcon::None);

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
    // 다른 화면의 프레임으로 내장됐을 때는 소유 화면의 연출을 따른다.
    if (GetTypedOuter<UUserWidget>())
    {
        bInstantAppear = true;
    }
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
    if (PanelHeader)
    {
        PanelHeader->SetTitle(Title);
    }
    if (TitleText)
    {
        TitleText->SetText(Title);
    }
    // PanelHeader 로 이행 중이라 둘 다 Optional 이지만, 하나는 반드시 있어야 한다 —
    // 둘 다 미배선인 WBP 는 컴파일 에러 없이 통과하고 제목 없는 확인 모달이 열린다.
    if (!PanelHeader && !TitleText && !Title.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ConfirmCancel] %s 에 PanelHeader/TitleText 가 둘 다 없어 제목 '%s' 이 표시되지 않는다"),
            *GetClass()->GetName(), *Title.ToString());
    }
}

void UConfirmCancelWidget::SetTitleIconType(EConfirmDialogIcon Type)
{
    if (!TitleIcon && !PanelHeader)
    {
        return;
    }

    const TCHAR* IconPath = nullptr;
    switch (Type)
    {
    case EConfirmDialogIcon::Warning:
        IconPath = TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Confirm/T_UIIcon_Confirm_Warning.T_UIIcon_Confirm_Warning");
        break;
    case EConfirmDialogIcon::Delete:
        IconPath = TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Confirm/T_UIIcon_Confirm_Delete.T_UIIcon_Confirm_Delete");
        break;
    case EConfirmDialogIcon::Info:
        IconPath = TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Notify/T_UIIcon_Notify_Info.T_UIIcon_Notify_Info");
        break;
    case EConfirmDialogIcon::None:
    default:
        break;
    }

    UTexture2D* IconTexture = IconPath ? TSoftObjectPtr<UTexture2D>(FSoftObjectPath(IconPath)).LoadSynchronous() : nullptr;
    if (PanelHeader)
    {
        PanelHeader->SetHeaderIcon(IconTexture);
    }
    if (!TitleIcon) { return; }
    if (!IconTexture)
    {
        TitleIcon->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // ImageSize/TintColor 는 WBP 저작값 ― ResourceObject 만 갈아끼운다
    CGIconBrush::ApplyResource(TitleIcon, IconTexture);
    TitleIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UConfirmCancelWidget::RestoreDefaultTexts()
{
    SetTitleIconType(EConfirmDialogIcon::None);
    SetTitle(DefaultTitle);
    if (ConfirmButton)
    {
        ConfirmButton->SetButtonText(DefaultConfirmText);
        // 소비자가 진행 중 중복 입력을 막으려고 내려둔 Enabled 가 되돌려지지 않으면
        // 다음 모달이 확인 버튼이 죽은 채로 열린다(리빌 중 back/ESC 로 닫은 경우).
        ConfirmButton->SetIsEnabled(true);
    }
    if (CancelButton)
    {
        CancelButton->SetButtonText(DefaultCancelText);
        CancelButton->SetIsEnabled(true);
    }
    SetIsEnabled(true);
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
