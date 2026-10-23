#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Gacha/GachaOddsCellWidget.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Panel/ShopPanelWidget.h"
#include "UI/UIBase.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "CommonTextBlock.h"

#define LOCTEXT_NAMESPACE "EmployeeEquipmentUI"

namespace
{
    constexpr EItemType TicketTypes[] = {EItemType::EmployeeEquipmentTicketNormal,
        EItemType::EmployeeEquipmentTicketAdvanced, EItemType::EmployeeEquipmentTicketPremium};
}

void UEmployeeEquipmentWorkspaceWidget::Resume()
{
    bReturningFromGacha = false;
    bReturningFromShop = false;
    bPreviewEnabled = true;
    SetupFilters();
    Refresh();
}

UWidget* UEmployeeEquipmentWorkspaceWidget::GetPullOneButtonWidget() const
{
    return CurrentView == EEquipmentWorkspaceView::Gacha ? PullOneButton.Get() : nullptr;
}

UWidget* UEmployeeEquipmentWorkspaceWidget::GetNormalTicketButtonWidget() const
{
    return CurrentView == EEquipmentWorkspaceView::Gacha ? TicketNormalButton.Get() : nullptr;
}

void UEmployeeEquipmentWorkspaceWidget::SetupBanners()
{
    CurrentBannerId = NAME_None;
    if (!TableManager) return;
    int32 MatchingBanners = 0;
    for (const auto& Pair : TableManager->GetEmployeeEquipmentBanners())
    {
        if (Pair.Value.TicketType != CurrentTicketType) continue;
        CurrentBannerId = Pair.Key;
        ++MatchingBanners;
    }
    if (MatchingBanners != 1)
    {
        CurrentBannerId = NAME_None;
        UE_LOG(LogTemp, Warning, TEXT("Equipment ticket requires exactly one banner: ticket=%d count=%d"),
            static_cast<int32>(CurrentTicketType), MatchingBanners);
    }
    RefreshTickets();
}

void UEmployeeEquipmentWorkspaceWidget::RefreshTickets()
{
    const auto* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
    UGachaBannerButton* Buttons[] = {TicketNormalButton, TicketAdvancedButton, TicketPremiumButton};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TicketTypes); ++Index)
    {
        if (!Buttons[Index]) continue;
        Buttons[Index]->SetButtonText(GetItemTypeDisplayName(TicketTypes[Index]));
        Buttons[Index]->SetOwnedCount(Items ? Items->GetItemCount(TicketTypes[Index]) : 0, true);
        bool bFound = false;
        UObject* Resource = TableManager ? TableManager->GetItemIconResource(TicketTypes[Index], bFound) : nullptr;
        CGEquipmentIcon::Apply(Cast<UImage>(Buttons[Index]->GetWidgetFromName(TEXT("TierIcon"))), Resource);
    }
}

void UEmployeeEquipmentWorkspaceWidget::HandleTicketSelected(UCommonButtonBase* Button, int32 Index)
{
    if (bUpdating || Index < 0 || Index >= UE_ARRAY_COUNT(TicketTypes)) return;
    CurrentTicketType = TicketTypes[Index];
    if (TicketButtonGroup) TicketButtonGroup->SelectButtonAtIndex(Index);
    DismissDialog();
    SetupBanners();
    RefreshGacha();
}

void UEmployeeEquipmentWorkspaceWidget::HandleTicketItemsChanged(EItemType Item, int32 Count, int32 Delta)
{
    if (Item == EItemType::EmployeeEquipmentCore) { Refresh(); return; }
    for (EItemType Ticket : TicketTypes) if (Item == Ticket)
    {
        RefreshTickets();
        if (CurrentView == EEquipmentWorkspaceView::Gacha) RefreshGacha();
        break;
    }
}

void UEmployeeEquipmentWorkspaceWidget::RefreshGacha()
{
    const auto* Banner = TableManager->FindEmployeeEquipmentBanner(CurrentBannerId);
    UIconWithButtonWidget* PullButtons[] = {PullOneButton, PullTenButton, PullThirtyButton};
    constexpr int32 PullCounts[] = {1, 10, 30};
    FEmployeeEquipmentPullQuote Quotes[UE_ARRAY_COUNT(PullCounts)];
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(PullCounts); ++Index)
    {
        const bool bValid = EquipmentManager->QuotePull(CurrentBannerId, PullCounts[Index], Quotes[Index]);
        if (PullButtons[Index]) PullButtons[Index]->SetIsEnabled(bValid);
    }
    if (!Banner) return;
    RefreshTickets();
    if (GachaTitleText) GachaTitleText->SetText(Banner->DisplayName);
	bool bHeroIconFound = false;
	CGEquipmentIcon::Apply(GachaImage, TableManager->GetItemIconResource(Banner->TicketType, bHeroIconFound));
	if (GachaOddsRow)
	{
		GachaOddsRow->ClearChildren();
		const TSubclassOf<UUserWidget> CellClass = TableManager->GetWidgetClass(EWidgetType::GachaOddsCell);
		if (!CellClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EquipmentGacha] GachaOddsCell missing from DT_WidgetClass"));
		}
		else
		{
			int32 VisibleIndex = 0;
			for (int32 GradeIndex = 0; GradeIndex < Banner->GradeWeights.Num(); ++GradeIndex)
			{
				const int32 Weight = Banner->GradeWeights[GradeIndex];
				const auto* GradeRow = TableManager->FindEmployeeEquipmentGrade(static_cast<EEmployeeEquipmentGrade>(GradeIndex));
				if (Weight <= 0 || !GradeRow || GradeRow->DisplayName.IsEmpty()) { continue; }
				UGachaOddsCellWidget* Cell = CreateWidget<UGachaOddsCellWidget>(GetOwningPlayer(), CellClass.Get());
				if (!Cell) { continue; }
				Cell->SetOdds(GradeRow->DisplayName, Weight / 100.f, GradeRow->Color, VisibleIndex > 0, true);
				if (UHorizontalBoxSlot* CellSlot = GachaOddsRow->AddChildToHorizontalBox(Cell))
				{
					CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				}
				++VisibleIndex;
			}
		}
	}

    const auto State = EquipmentManager->GetState().BannerStates.FindRef(Banner->PityGroupId);
    const auto RefreshPity = [this](UCommonTextBlock* Label, UProgressBar* Progress,
        EEmployeeEquipmentGrade Grade, int32 Threshold, int32 Since)
    {
        const auto* GradeRow = TableManager->FindEmployeeEquipmentGrade(Grade);
        const bool bShow = GradeRow && !GradeRow->DisplayName.IsEmpty() && Threshold > 0;
        if (Label)
        {
            Label->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
            Label->SetText(bShow ? FText::Format(Grade == EEmployeeEquipmentGrade::Ascendant
                ? LOCTEXT("HighestGradePity", "{0} 보장 · {1}회 남음")
                : LOCTEXT("MinimumGradePity", "{0} 이상 보장 · {1}회 남음"),
                GradeRow->DisplayName, FText::AsNumber(FMath::Max(1, Threshold - Since))) : FText());
        }
        if (Progress)
        {
            Progress->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
            Progress->SetPercent(bShow ? FMath::Clamp(static_cast<float>(Since) / Threshold, 0.f, 1.f) : 0.f);
        }
    };
    RefreshPity(EpicPityText, EpicPityProgress, EEmployeeEquipmentGrade::Epic, Banner->EpicPity, State.SinceEpic);
    RefreshPity(LegendaryPityText, LegendaryPityProgress, EEmployeeEquipmentGrade::Legendary, Banner->LegendaryPity, State.SinceLegendary);
    RefreshPity(AscendantPityText, AscendantPityProgress, EEmployeeEquipmentGrade::Ascendant, Banner->AscendantPity, State.SinceAscendant);
    bool bFound = false;
    UObject* TicketIcon = TableManager->GetItemIconResource(Banner->TicketType, bFound);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(PullCounts); ++Index) if (PullButtons[Index])
    {
        const int32 Count = PullCounts[Index];
        PullButtons[Index]->SetIconResource(TicketIcon);
        // 횟수는 비용 칸(×N 또는 보유/필요)이 말한다 — 문구에 다시 쓰면 좁은 버튼에서 넘친다
        PullButtons[Index]->SetItemCost(Quotes[Index].AvailableTickets, Count);
        PullButtons[Index]->SetButtonText(LOCTEXT("TicketPullCTA", "뽑기"));
    }
    if (GachaCostText) GachaCostText->SetText(FText::Format(LOCTEXT("TicketOnly", "{0} · 보유 {1}개"),
        GetItemTypeDisplayName(Banner->TicketType), FText::AsNumber(Quotes[0].AvailableTickets)));
}

void UEmployeeEquipmentWorkspaceWidget::ShowPull(int32 Count)
{
    if (!EquipmentManager || Dialog || RevealPresentation || (PullNotice.IsValid() && PullNotice->IsInViewport())) return;
    FEmployeeEquipmentPullQuote Quote;
    EquipmentManager->QuotePull(CurrentBannerId, Count, Quote);
    ShowPullQuote(Quote, Count);
}

void UEmployeeEquipmentWorkspaceWidget::ShowPullQuote(const FEmployeeEquipmentPullQuote& Quote, int32 Count)
{
    if (!EquipmentManager || Dialog || RevealPresentation || (PullNotice.IsValid() && PullNotice->IsInViewport())) return;
    RequestedPullCount = Count;
    if (Quote.bCanPull)
    {
        PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::Pull);
        PendingRequest.BannerId = Quote.BannerId;
        PendingRequest.Count = Count;
        ExecutePending();
        return;
    }
    const bool bShortage = Quote.ErrorCode == TEXT("InsufficientTickets");
    const bool bFull = Quote.ErrorCode == TEXT("InventoryFull");
    const FText Title = bFull ? LOCTEXT("InventoryFull", "보관함이 가득 찼습니다")
        : bShortage ? LOCTEXT("TicketShortage", "상자가 부족합니다") : LOCTEXT("PullUnavailableTitle", "지금은 이용할 수 없습니다");
    const FText Message = bShortage ? FText::Format(LOCTEXT("TicketShortageQuote", "{0}\n{1}회 뽑기 · 필요 {2}개\n보유 {3}개 · 부족 {4}개"),
        GetItemTypeDisplayName(Quote.TicketType), FText::AsNumber(Count),
        FText::AsNumber(Quote.RequiredTickets), FText::AsNumber(Quote.AvailableTickets),
        FText::AsNumber(Quote.MissingTickets))
        : bFull ? LOCTEXT("ClearStorage", "장비를 정리해 보관함 공간을 확보해 주세요.\n상자는 사용되지 않았습니다.")
        : LOCTEXT("InvalidPullState", "장비 정보를 확인할 수 없습니다.\n잠시 후 다시 열어 주세요.");
    auto* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    if (!UI) return;
    const FSimpleDelegate OnConfirm = bShortage ? FSimpleDelegate::CreateUObject(this, &ThisClass::HandleShop)
        : bFull ? FSimpleDelegate::CreateUObject(this, &ThisClass::HandleStorageForCapacity)
        : FSimpleDelegate::CreateUObject(this, &ThisClass::DismissDialog);
    PullNotice = UI->ShowConfirmDialog(Title, Message, OnConfirm,
        FSimpleDelegate::CreateUObject(this, &ThisClass::DismissDialog));
    if (UConfirmCancelWidget* Notice = PullNotice.Get())
    {
        Notice->SetConfirmButtonText(bShortage ? LOCTEXT("RefillAtShop", "상점으로")
            : bFull ? LOCTEXT("StorageCleanup", "보관함 정리") : LOCTEXT("Confirm", "확인"));
        Notice->SetCancelButtonText(LOCTEXT("Cancel", "취소"));
    }
}

void UEmployeeEquipmentWorkspaceWidget::HandleStorageForCapacity()
{
    DismissDialog();
    CurrentView = EEquipmentWorkspaceView::Inventory;
    Refresh();
}

void UEmployeeEquipmentWorkspaceWidget::HandleShop()
{
    auto* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
    const auto Class = TableManager ? TableManager->GetWidgetClass(EWidgetType::ShopPanel) : nullptr;
    if (!UI || !UI->GetUIBase() || !Class) return;
    DismissDialog();
    bReturningFromShop = true;
    auto* Shop = Cast<UShopPanelWidget>(UI->GetUIBase()->PushPromptClass(Class.Get()));
    if (Shop) Shop->SetEquipmentTicketContext(CurrentTicketType, RequestedPullCount);
    else bReturningFromShop = false;
}

#undef LOCTEXT_NAMESPACE
