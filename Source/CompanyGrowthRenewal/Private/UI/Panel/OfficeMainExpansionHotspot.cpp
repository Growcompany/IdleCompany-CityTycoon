#include "UI/Panel/OfficeMainWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Manager/TableManagerSubsystem.h"
#include "GameMode/OfficeGameMode.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Player/OfficeCameraPawn.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Office/OfficeExpansionContentWidget.h"
#include "UI/Element/Effects/SelectionChevronWidget.h"
#include "UI/UIBase.h"

void UOfficeMainWidget::BindExpansionHotspots()
{
    struct FBinding { const TCHAR* Name; UWidget* Widget; };
    const FBinding Bindings[] = {
        {TEXT("ExpansionLayer"), ExpansionLayer}, {TEXT("ExpandAnchorX"), ExpandAnchorX},
        {TEXT("ExpandAnchorY"), ExpandAnchorY}, {TEXT("ExpandHotspotX"), ExpandHotspotX},
        {TEXT("ExpandHotspotY"), ExpandHotspotY}, {TEXT("ExpandCostX"), ExpandCostX},
        {TEXT("ExpandCostY"), ExpandCostY}};
    for (const FBinding& Binding : Bindings)
        if (!Binding.Widget) UE_LOG(LogTemp, Error, TEXT("[OfficeMain] Missing expansion binding: %s (UI_OfficeMapMain)"), Binding.Name);

    const TCHAR* ChevronNames[] = {TEXT("ExpandChevronX"), TEXT("ExpandChevronY")};
    const TCHAR* InkNames[] = {TEXT("ExpandChevronInkX"), TEXT("ExpandChevronInkY")};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        ExpansionChevrons[Index] = GetWidgetFromName(ChevronNames[Index]);
        USelectionChevronWidget* Ink = Cast<USelectionChevronWidget>(GetWidgetFromName(InkNames[Index]));
        if (!ExpansionChevrons[Index].IsValid() || !Ink)
            UE_LOG(LogTemp, Error, TEXT("[OfficeMain] Missing expansion chevron: %s / %s"), ChevronNames[Index], InkNames[Index]);
        if (Ink)
        {
            const FLinearColor MainColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("DED7C4")));
            FLinearColor EchoColor = MainColor;
            EchoColor.A = 0.45f;
            Ink->SetChevronColors(MainColor, EchoColor);
        }
    }

    if (ExpandHotspotX) ExpandHotspotX->OnClicked().AddUObject(this, &ThisClass::OpenExpansionConfirm, EOfficeExpandDirection::Left);
    if (ExpandHotspotY) ExpandHotspotY->OnClicked().AddUObject(this, &ThisClass::OpenExpansionConfirm, EOfficeExpandDirection::Right);
    if (UGameInstance* GameInstance = GetGameInstance())
        if (UResourceItemManager* Resources = GameInstance->GetSubsystem<UResourceItemManager>())
            Resources->OnResourceChanged.AddUObject(this, &ThisClass::OnExpansionResourceChanged);
    CloseExpansionConfirm();
}

void UOfficeMainWidget::UnbindExpansionHotspots()
{
    CloseExpansionConfirm();
    if (ExpandHotspotX) ExpandHotspotX->OnClicked().RemoveAll(this);
    if (ExpandHotspotY) ExpandHotspotY->OnClicked().RemoveAll(this);
    if (UGameInstance* GameInstance = GetGameInstance())
        if (UResourceItemManager* Resources = GameInstance->GetSubsystem<UResourceItemManager>())
            Resources->OnResourceChanged.RemoveAll(this);
    if (AOfficeInterior* Interior = ExpansionInterior.Get())
    {
        Interior->OnFootprintChanged.Remove(ExpansionFootprintHandle);
        Interior->SetExpansionPreviewVisible(false);
    }
    ExpansionInterior.Reset();
    LastPreviewInterior.Reset();
    for (TWeakObjectPtr<UWidget>& Chevron : ExpansionChevrons) Chevron.Reset();
    ExpansionFootprintHandle.Reset();
    bExpansionPreviewShown = false;
    bExpansionAffordInitialized = false;
}

void UOfficeMainWidget::OnExpansionResourceChanged(EResourceType Type, int64 /*NewValue*/)
{
    if (Type == EResourceType::Diamond) RefreshExpansionValues();
}

void UOfficeMainWidget::OnExpansionFootprintChanged(FIntPoint /*TileCount*/)
{
    CloseExpansionConfirm();
    RefreshExpansionValues();
    UpdateExpansionHotspots();
}

void UOfficeMainWidget::RefreshExpansionValues()
{
    const AOfficeInterior* Interior = ExpansionInterior.Get();
    const UResourceItemManager* Resources = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr;
    if (!Interior || !Resources) return;
    const int64 Balance = Resources->GetResourceAmount(EResourceType::Diamond);
    UResourceWidget* Chips[] = {ExpandCostX, ExpandCostY};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const EOfficeExpandDirection Direction = Index == 0 ? EOfficeExpandDirection::Left : EOfficeExpandDirection::Right;
        const int64 Cost = Interior->GetExpandCost(Direction);
        const bool bCanAfford = Balance >= Cost;
        if (UResourceWidget* Chip = Chips[Index])
        {
            Chip->TextColor = FLinearColor::FromSRGBColor(FColor::FromHex(bCanAfford ? TEXT("3D9BE0") : TEXT("97A3B6")));
            Chip->SetResourceType(EResourceType::Diamond, false);
            Chip->SetValueSilent(Cost);
            if (bExpansionAffordInitialized && !bExpansionCanAfford[Index] && bCanAfford
                && IsActivated() && bExpansionPreviewShown && Interior->HasExpansionCell(Direction)) Chip->PlayBump();
        }
        bExpansionCanAfford[Index] = bCanAfford;
    }
    bExpansionAffordInitialized = true;
}

void UOfficeMainWidget::UpdateExpansionHotspots(bool bFromTick)
{
    // ConfirmCancel은 비활성화 뒤 확인 콜백을 발행하므로 닫힘 정리는 다음 Tick에서 한다.
    if (bFromTick && bExpansionConfirmOpen && !bPushingExpansionConfirm)
    {
        UConfirmCancelWidget* Dialog = ExpansionDialog.Get();
        if (!Dialog || (bExpansionConfirmWasActivated && !Dialog->IsActivated())) CloseExpansionConfirm();
        else if (Dialog->IsActivated()) bExpansionConfirmWasActivated = true;
    }
    AOfficeGameMode* Mode = GetWorld() ? Cast<AOfficeGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
    AOfficeInterior* Interior = Mode ? Mode->GetOfficeInterior() : nullptr;
    if (Interior != ExpansionInterior.Get())
    {
        CloseExpansionConfirm();
        if (AOfficeInterior* Previous = ExpansionInterior.Get())
        {
            Previous->OnFootprintChanged.Remove(ExpansionFootprintHandle);
            Previous->SetExpansionPreviewVisible(false);
        }
        ExpansionInterior = Interior;
        if (Interior)
        {
            ExpansionFootprintHandle = Interior->OnFootprintChanged.AddUObject(this, &ThisClass::OnExpansionFootprintChanged);
            RefreshExpansionValues();
        }
    }
    const UOfficeStageProgressManager* Stage = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
    const AOfficeCameraPawn* CameraPawn = Cast<AOfficeCameraPawn>(GetOwningPlayerPawn());
    const bool bOwnPrompt = bExpansionConfirmOpen && (bPushingExpansionConfirm || ExpansionDialog.IsValid());
    const bool bShow = (IsActivated() || bOwnPrompt) && Interior && Stage && !IsFocusLockedPhase(Stage->GetLifecycle())
        && !(CameraPawn && (CameraPawn->IsOfficePlacementEditing() || CameraPawn->IsInWorkstationPlacementMode()
            || CameraPawn->IsInDecorationPlacementMode() || CameraPawn->IsInWallEditMode()));
    if (Interior && (bExpansionPreviewShown != bShow || Interior != LastPreviewInterior.Get()))
    {
        Interior->SetExpansionPreviewVisible(bShow);
        bExpansionPreviewShown = bShow;
        LastPreviewInterior = Interior;
    }
    if (ExpansionLayer) ExpansionLayer->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!bShow || !ExpansionLayer || !GetOwningPlayer())
    {
        CloseExpansionConfirm();
        return;
    }
    const FGeometry CanvasGeometry = ExpansionLayer->GetCachedGeometry();
    const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();
    if (CanvasSize.X <= 0.0 || CanvasSize.Y <= 0.0) return;
    const FBox2D ViewportBounds(FVector2D::ZeroVector, CanvasSize);
    FBox2D ReservedIdleArea(ForceInit);
    if (RightIdleZone && RightIdleZone->IsVisible())
    {
        const FGeometry IdleGeometry = RightIdleZone->GetCachedGeometry();
        if (!IdleGeometry.GetLocalSize().IsNearlyZero())
        {
            ReservedIdleArea = FBox2D(
                CanvasGeometry.AbsoluteToLocal(IdleGeometry.LocalToAbsolute(FVector2D::ZeroVector)),
                CanvasGeometry.AbsoluteToLocal(IdleGeometry.LocalToAbsolute(IdleGeometry.GetLocalSize())));
        }
    }
    UWidget* Anchors[] = {ExpandAnchorX, ExpandAnchorY};
    UIconWithButtonWidget* Buttons[] = {ExpandHotspotX, ExpandHotspotY};
    UResourceWidget* Chips[] = {ExpandCostX, ExpandCostY};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        UWidget* AnchorWidget = Anchors[Index];
        const EOfficeExpandDirection Direction = Index == 0 ? EOfficeExpandDirection::Left : EOfficeExpandDirection::Right;
        if (!AnchorWidget) continue;
        const FVector AnchorWorld = Interior->GetExpansionAnchorWorld(Direction);
        FVector2D ViewportPosition;
        const bool bProjected = Buttons[Index] && Chips[Index] && Interior->HasExpansionCell(Direction)
            && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),
                AnchorWorld, ViewportPosition, true);
        if (!bProjected)
        {
            AnchorWidget->SetVisibility(ESlateVisibility::Collapsed);
            if (bExpansionConfirmOpen && ExpansionDirection == Direction) CloseExpansionConfirm();
            continue;
        }
        const FVector2D LocalPosition = CanvasGeometry.AbsoluteToLocal(
            UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(ViewportPosition));
        const FVector2D PlateSize = Buttons[Index]->GetCachedGeometry().GetLocalSize().IsNearlyZero()
            ? Buttons[Index]->GetDesiredSize() : Buttons[Index]->GetCachedGeometry().GetLocalSize();
        const FVector2D AnchorSize = AnchorWidget->GetCachedGeometry().GetLocalSize().IsNearlyZero()
            ? AnchorWidget->GetDesiredSize() : AnchorWidget->GetCachedGeometry().GetLocalSize();
        const FVector2D AnchorTopLeft = LocalPosition - FVector2D(AnchorSize.X * 0.5, PlateSize.Y * 0.5);
        const FBox2D AnchorBounds(AnchorTopLeft, AnchorTopLeft + AnchorSize);
        const bool bAxisVisible = ViewportBounds.IsInsideOrOn(LocalPosition)
            && !(ReservedIdleArea.bIsValid && ReservedIdleArea.Intersect(AnchorBounds));
        AnchorWidget->SetVisibility(bAxisVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        if (!bAxisVisible)
        {
            if (bExpansionConfirmOpen && ExpansionDirection == Direction) CloseExpansionConfirm();
            continue;
        }
        if (UCanvasPanelSlot* AnchorSlot = Cast<UCanvasPanelSlot>(AnchorWidget->Slot))
            AnchorSlot->SetPosition(LocalPosition - FVector2D(0.0, PlateSize.Y * 0.5));
        if (UWidget* Chevron = ExpansionChevrons[Index].Get())
        {
            FVector2D OutwardPosition;
            if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),
                AnchorWorld + Interior->GetExpansionDirectionWorld(Direction) * Interior->GetTileSize(),
                OutwardPosition, true))
            {
                const FVector2D OutwardLocal = CanvasGeometry.AbsoluteToLocal(
                    UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(OutwardPosition));
                const FVector2D ProjectedDirection = OutwardLocal - LocalPosition;
                if (!ProjectedDirection.IsNearlyZero())
                    Chevron->SetRenderTransformAngle(static_cast<float>(
                        FMath::RadiansToDegrees(FMath::Atan2(ProjectedDirection.Y, ProjectedDirection.X)) - 90.0));
            }
        }
    }
}

void UOfficeMainWidget::OpenExpansionConfirm(EOfficeExpandDirection Direction)
{
    AOfficeInterior* Interior = ExpansionInterior.Get();
    if (!IsActivated() || !bExpansionPreviewShown || bExpansionRequestPending || !Interior || !Interior->HasExpansionCell(Direction)
        || !Interior->CanExpand(Direction)) return;
    if (bExpansionConfirmOpen && (!ExpansionDialog.IsValid()
        || (bExpansionConfirmWasActivated && !ExpansionDialog->IsActivated())))
        CloseExpansionConfirm();
    if (bExpansionConfirmOpen && ExpansionDirection == Direction)
    {
        CloseExpansionConfirm();
        return;
    }
    CloseExpansionConfirm();
    UGameInstance* GameInstance = GetGameInstance();
    UTableManagerSubsystem* Tables = GameInstance ? GameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
    UUIManagerSubsystem* UIManager = GameInstance ? GameInstance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
    const TSubclassOf<UUserWidget> DialogClass = Tables ? Tables->GetWidgetClass(EWidgetType::ConfirmCancel) : nullptr;
    if (!UIBase || !DialogClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OfficeMain] Expansion confirmation unavailable: dialog or resource data missing"));
        return;
    }
    if (!DialogClass->IsChildOf(UConfirmCancelWidget::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("[OfficeMain] ConfirmCancel registration is not a UConfirmCancelWidget"));
        return;
    }

    const bool bHorizontal = Direction == EOfficeExpandDirection::Left;
    const FIntPoint Counts = Interior->GetTileCount();
    const FIntPoint Maximum = Interior->GetMaxTileCount();
    const int32 Current = bHorizontal ? Counts.X : Counts.Y;
    const int64 Cost = Interior->GetExpandCost(Direction);
    if (Cost <= 0) return;
    const FText Title = bHorizontal
        ? NSLOCTEXT("OfficeExpansion", "HorizontalTitle", "가로 공간 확장")
        : NSLOCTEXT("OfficeExpansion", "VerticalTitle", "세로 공간 확장");
    const int32 MaximumCount = bHorizontal ? Maximum.X : Maximum.Y;
    if (!ExpansionConfirmContent)
    {
        UE_LOG(LogTemp, Error, TEXT("[OfficeMain] UI_OfficeMapMain is missing ExpansionConfirmContent"));
        return;
    }

    ExpansionDirection = Direction;
    bExpansionConfirmOpen = true;
    bPushingExpansionConfirm = true;
    bExpansionConfirmWasActivated = false;
    bool bContentReady = false;
    TWeakObjectPtr<UOfficeMainWidget> WeakThis(this);
    const TWeakObjectPtr<AOfficeInterior> QuotedInterior(Interior);
    UConfirmCancelWidget* Dialog = Cast<UConfirmCancelWidget>(UIBase->PushPromptClass(DialogClass.Get(),
        [this, WeakThis, Title, Current, MaximumCount, Direction, QuotedInterior, Counts, Cost, &bContentReady](UCommonActivatableWidget& Instance)
        {
            UConfirmCancelWidget* Confirm = Cast<UConfirmCancelWidget>(&Instance);
            if (!Confirm) return;
            Confirm->OnConfirm.Clear();
            Confirm->OnCancel.Clear();
            Confirm->SetConfirmOnly(false);
            Confirm->SetCancelOnly(false);
            Confirm->RestoreDefaultTexts();
            UOfficeExpansionContentWidget* Content = ExpansionConfirmContent.Get();
            if (!Content || !Confirm->GetWidgetFromName(TEXT("ContentSlot")))
            {
                UE_LOG(LogTemp, Error, TEXT("[OfficeMain] Expansion content or ConfirmCancel ContentSlot is missing"));
                return;
            }
            if (!Content->SetExpansionPreview(Current, Current + 1, MaximumCount, Cost)) return;
            Content->RemoveFromParent();
            Confirm->SetContentForSlot(TEXT("ContentSlot"), Content);
            if (Confirm->GetContentForSlot(TEXT("ContentSlot")) != Content)
            {
                UE_LOG(LogTemp, Error, TEXT("[OfficeMain] Failed to attach expansion content to ConfirmCancel ContentSlot"));
                Content->ClearPreview();
                return;
            }
            Confirm->SetMessage(FText::GetEmpty());
            Confirm->HideMessageText();
            bContentReady = true;
            Confirm->SetTitle(Title);
            Confirm->SetConfirmButtonText(NSLOCTEXT("OfficeExpansion", "ExpandButton", "확장"));
            Confirm->SetCancelButtonText(NSLOCTEXT("OfficeExpansion", "CancelButton", "취소"));
            Confirm->bAutoRemove = true;
            ExpansionDialog = Confirm;
            ExpansionConfirmHandle = Confirm->OnConfirm.AddLambda([WeakThis, Direction, QuotedInterior, Counts, Cost]()
            {
                if (UOfficeMainWidget* Self = WeakThis.Get()) Self->ConfirmExpansion(Direction, QuotedInterior, Counts, Cost);
            });
            ExpansionCancelHandle = Confirm->OnCancel.AddLambda([WeakThis]()
            {
                if (UOfficeMainWidget* Self = WeakThis.Get()) Self->CloseExpansionConfirm();
            });
        }));
    bPushingExpansionConfirm = false;
    if (!bContentReady || !Dialog || Dialog != ExpansionDialog.Get())
    {
        if (Dialog && Dialog != ExpansionDialog.Get())
        {
            if (!bContentReady)
                UE_LOG(LogTemp, Error, TEXT("[OfficeMain] Expansion confirmation unavailable: content or ContentSlot binding missing"));
            UIBase->RemovePromptWidget(Dialog);
        }
        CloseExpansionConfirm();
    }
}

void UOfficeMainWidget::CloseExpansionConfirm()
{
    UConfirmCancelWidget* Dialog = ExpansionDialog.Get();
    ExpansionDialog.Reset();
    bExpansionConfirmOpen = false;
    bExpansionConfirmWasActivated = false;
    if (Dialog)
    {
        if (ExpansionConfirmContent && Dialog->GetContentForSlot(TEXT("ContentSlot")) == ExpansionConfirmContent)
            Dialog->SetContentForSlot(TEXT("ContentSlot"), nullptr);
        if (UWidget* MessageWidget = Dialog->GetWidgetFromName(TEXT("MessageText")))
            MessageWidget->SetVisibility(ESlateVisibility::Visible);
        if (ExpansionConfirmHandle.IsValid()) Dialog->OnConfirm.Remove(ExpansionConfirmHandle);
        if (ExpansionCancelHandle.IsValid()) Dialog->OnCancel.Remove(ExpansionCancelHandle);
    }
    ExpansionConfirmHandle.Reset();
    ExpansionCancelHandle.Reset();
    if (ExpansionConfirmContent) ExpansionConfirmContent->ClearPreview();
    if (!Dialog) return;
    const UUIManagerSubsystem* UIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
    if (UIBase) UIBase->RemovePromptWidget(Dialog);
}

void UOfficeMainWidget::ConfirmExpansion(EOfficeExpandDirection Direction, TWeakObjectPtr<AOfficeInterior> QuotedInterior,
    FIntPoint QuotedFootprint, int64 QuotedCost)
{
    const bool bOwnQuote = bExpansionConfirmOpen && ExpansionDirection == Direction;
    CloseExpansionConfirm();
    UOfficeManager* OfficeManager = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr;
    AOfficeInterior* Interior = ExpansionInterior.Get();
    if (!bOwnQuote || !bExpansionPreviewShown || bExpansionRequestPending || !OfficeManager || !Interior
        || Interior != QuotedInterior.Get()
        || !Interior->CanExpand(Direction) || Interior->GetTileCount() != QuotedFootprint
        || QuotedCost <= 0 || OfficeManager->GetExpandCost(Direction) != QuotedCost) return;
    bExpansionRequestPending = true;
    OfficeManager->RequestExpand(Direction, FOnCloudEconomyOpDone::CreateWeakLambda(this,
        [this, QuotedCost](bool bOk, FName Code)
    {
        bExpansionRequestPending = false;
        if (!bOk)
            if (UUIManagerSubsystem* UIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
                UIManager->NotifyCloudOpFailure(Code, EResourceType::Diamond, QuotedCost);
        RefreshExpansionValues();
    }));
}
