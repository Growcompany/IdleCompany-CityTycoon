#if WITH_EDITOR

#include "UI/Panel/EquipmentGachaPanelWidget.h"
#include "UI/Panel/ShopPanelWidget.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Gacha/GachaPanelFrameWidget.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

// Exercises registered widgets on the live prompt stack. Pull confirmations are never accepted.
struct FEquipmentGachaPanelValidation
{
    TWeakObjectPtr<UWorld> PlayWorld;
    TWeakObjectPtr<UUIBase> UIBase;
    TWeakObjectPtr<UEquipmentGachaPanelWidget> Panel;
    TWeakObjectPtr<UShopPanelWidget> Shop;
    TWeakObjectPtr<UEmployeeEquipmentWorkspaceWidget> Work;
    TUniquePtr<FWidgetRenderer> Renderer;
    TStrongObjectPtr<UTextureRenderTarget2D> Target;
    TSharedPtr<SOverlay> Surface;
    TArray<TSharedPtr<FJsonValue>> Checks, Captures;
    FString Directory;
    FGuid SourceItem;
    FName SavedBanner;
    int64 Revision = 0;
    int32 Stage = 0, CaptureIndex = 0;
    float Remaining = .65f;
    bool bPassed = true;
    bool bCaptureNotice = false;

    void Check(const FString& Name, bool bValue)
    {
        auto Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Name); Entry->SetBoolField(TEXT("passed"), bValue);
        const auto* Controller = PlayWorld.IsValid() ? PlayWorld->GetFirstPlayerController() : nullptr;
        const auto* MainController = Cast<AMainMapPlayerController>(Controller);
        Entry->SetStringField(TEXT("controller_class"), Controller ? Controller->GetClass()->GetPathName() : TEXT("none"));
        Entry->SetNumberField(TEXT("input_mode"), MainController ? static_cast<int32>(MainController->GetCurrentInputMode()) : INDEX_NONE);
        Checks.Add(MakeShared<FJsonValueObject>(Entry)); bPassed &= bValue;
    }
    void Advance(int32 Next, float Delay = .65f) { Stage = Next; Remaining = Delay; }
    int32 Width() const { return CaptureIndex == 0 ? 2304 : (CaptureIndex == 1 ? 2560 : 3120); }
    bool HasUIInput() const
    {
        const auto* Controller = PlayWorld.IsValid() ? Cast<AMainMapPlayerController>(PlayWorld->GetFirstPlayerController()) : nullptr;
        return Controller && Controller->GetCurrentInputMode() == EInputMode::UI;
    }
    UCloseButtonWidget* CloseButton() const { return Panel.IsValid() ? Cast<UCloseButtonWidget>(Panel->GetCloseButtonWidget()) : nullptr; }
    // 운영 경로와 같게 PushPromptClass 초기화 람다에서 Open 한다
    bool Push(const FString& Prefix)
    {
        auto* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        bool bOpened = false;
        Panel = Cast<UEquipmentGachaPanelWidget>(UIBase->PushPromptClass(Tables->GetWidgetClass(EWidgetType::EquipmentGachaPanel).Get(),
            [&bOpened](UCommonActivatableWidget& Pushed) { if (auto* Gacha = Cast<UEquipmentGachaPanelWidget>(&Pushed)) { Gacha->Open(); bOpened = true; } }));
        Check(Prefix + TEXT("registered_equipment_gacha_panel_created"), Panel.IsValid());
        Check(Prefix + TEXT("open_from_init_lambda_configures_gacha_host"), Panel.IsValid() && bOpened
            && Panel->EquipmentWorkspace && Panel->EquipmentWorkspace->IsGachaHost());
        return Panel.IsValid();
    }
    bool ResolveWork()
    {
        auto* Host = Panel.IsValid() ? Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("EquipmentGachaHost"))) : nullptr;
        Work = Host && Host->GetChildrenCount() == 1 ? Cast<UEmployeeEquipmentWorkspaceWidget>(Host->GetChildAt(0)) : nullptr;
        Check(TEXT("single_registered_equipment_workspace"), Work.IsValid() && Work->IsGachaHost() && Panel->EquipmentWorkspace.Get() == Work.Get());
        return Work.IsValid();
    }
    bool Start(UWorld* InWorld)
    {
        PlayWorld = InWorld;
        // 같은 폴더에 위젯 등록 보고서가 함께 쌓이므로 실행마다 하위 폴더로 나눈다
        Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentGachaPanel"), FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
        auto* UI = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
        auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
        UIBase = UI ? UI->GetUIBase() : nullptr;
        Check(TEXT("world_with_empty_prompt_stack"), InWorld && UIBase.IsValid() && Tables && Equipment && UIBase->GetPromptStackCount() == 0);
        if (!UIBase.IsValid() || !Tables || !Equipment || UIBase->GetPromptStackCount() != 0) return false;
        Revision = Equipment->GetState().Revision;
        Check(TEXT("mainmap_gameplay_controller_required_not_simulate"), Cast<AMainMapPlayerController>(InWorld->GetFirstPlayerController()) != nullptr);
        if (!Cast<AMainMapPlayerController>(InWorld->GetFirstPlayerController())) return false;
        Renderer = MakeUnique<FWidgetRenderer>(true);
        return Push(FString());
    }
    void Capture()
    {
        Surface = SNew(SOverlay); Surface->AddSlot()[Panel->TakeWidget()];
        if (bCaptureNotice && Work->PullNotice.IsValid()) Surface->AddSlot()[Work->PullNotice->TakeWidget()];
        Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(Width(), 1440), TF_Bilinear, false));
        Remaining = .6f;
    }
    void SaveCapture()
    {
        FImage Image; TArray64<uint8> Bytes;
        const FString Name = FString::Printf(TEXT("%s_%dx1440.png"), bCaptureNotice ? TEXT("shortage") : TEXT("equipment"), Width());
        bool bSaved = Target.IsValid() && FImageUtils::GetRenderTargetImage(Target.Get(), Image) && Image.Format == ERawImageFormat::BGRA8;
        if (bSaved) { Image.GammaSpace = EGammaSpace::sRGB; bSaved = FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, Name)); }
        Check(Name + TEXT("_saved"), bSaved);
        auto Record = MakeShared<FJsonObject>(); Record->SetStringField(TEXT("png"), Name);
        Record->SetNumberField(TEXT("width"), Width()); Record->SetNumberField(TEXT("height"), 1440);
        TArray<UWidget*> Widgets = { Panel->GetCloseButtonWidget(), Work->PullOneButton.Get(), Work->PullTenButton.Get(), Work->PullThirtyButton.Get() };
        if (bCaptureNotice && Work->PullNotice.IsValid())
        {
            Widgets.Add(Work->PullNotice->GetConfirmButtonWidget());
            Widgets.Add(Work->PullNotice->GetCancelButtonWidget());
        }
        TArray<TSharedPtr<FJsonValue>> Bounds;
        const auto& Root = Panel->GetCachedGeometry();
        for (UWidget* Widget : Widgets)
        {
            if (!Widget) { Check(Name + TEXT("_required_widget"), false); continue; }
            const auto& Geometry = Widget->GetCachedGeometry();
            const FVector2D Begin = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
            const FVector2D End = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
            auto Bound = MakeShared<FJsonObject>(); Bound->SetStringField(TEXT("widget"), Widget->GetName());
            Bound->SetNumberField(TEXT("x"), Begin.X); Bound->SetNumberField(TEXT("y"), Begin.Y);
            Bound->SetNumberField(TEXT("width"), End.X-Begin.X); Bound->SetNumberField(TEXT("height"), End.Y-Begin.Y);
            const bool bInside = Begin.X >= -.5f && Begin.Y >= -.5f && End.X <= Width()+.5f && End.Y <= 1440.5f && End.X > Begin.X && End.Y > Begin.Y;
            Bound->SetBoolField(TEXT("within_canvas"), bInside); Bounds.Add(MakeShared<FJsonValueObject>(Bound));
            Check(Name + TEXT("_") + Widget->GetName() + TEXT("_within_canvas"), bInside);
        }
        Record->SetArrayField(TEXT("geometry"), Bounds); Captures.Add(MakeShared<FJsonValueObject>(Record));
        const TCHAR* ColumnNames[] = {TEXT("EquipmentGachaRail"), TEXT("EquipmentGachaHero"), TEXT("EquipmentGachaInfo")};
        for (int32 Column = 0; Column < 3; ++Column)
            Check(Name + FString::Printf(TEXT("_column_%d_exists"), Column), Work->GetWidgetFromName(ColumnNames[Column]) != nullptr);
        Surface.Reset(); Target.Reset();
    }
    bool Tick(float Delta)
    {
        if (!PlayWorld.IsValid() || !UIBase.IsValid()) return false;
        if (Surface && Target.IsValid()) Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(Width(),1440), Delta);
        Remaining -= Delta; if (Remaining > 0.f) return true;
        switch (Stage)
        {
        case 0:
        {
            if (!ResolveWork()) return false;
            // 제목은 틀이 공통 헤더로 넘긴다 ― 패널 WBP 의 GachaFrame Title 저작값이 화면 글자까지 갔는지 본다
            auto* Header = Panel->GachaFrame ? Cast<UUserWidget>(Panel->GachaFrame->GetWidgetFromName(TEXT("PanelHeader"))) : nullptr;
            auto* Title = Header ? Cast<UTextBlock>(Header->GetWidgetFromName(TEXT("TitleText"))) : nullptr;
            Check(TEXT("gacha_frame_title_is_equipment_gacha"), Title && Title->GetText().ToString() == TEXT("장비 뽑기"));
            Check(TEXT("equipment_gacha_hides_entire_management_toolbar"), Work->WorkspaceToolbar
                && Work->WorkspaceToolbar->GetVisibility() == ESlateVisibility::Collapsed
                && Work->GetCurrentView() == EEquipmentWorkspaceView::Gacha);
            Check(TEXT("gacha_shop_link_bound"), Work->GachaShopButton && Work->GachaShopButton->OnClicked().IsBound());
            if (!Work->GachaShopButton) return false;
            Check(TEXT("equipment_has_no_starter_claim_button"), Work->GetWidgetFromName(TEXT("GachaStarterButton")) == nullptr
                && Work->GetWidgetFromName(TEXT("StarterButton")) == nullptr);
            UIconWithButtonWidget* PullButtons[] = {Work->PullOneButton, Work->PullTenButton, Work->PullThirtyButton};
            const int32 PullCounts[] = {1, 10, 30};
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(PullCounts); ++Index)
            {
                const auto* Label = PullButtons[Index] ? Cast<UTextBlock>(PullButtons[Index]->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
                const auto* Cost = PullButtons[Index] ? Cast<UTextBlock>(PullButtons[Index]->GetWidgetFromName(TEXT("CountText"))) : nullptr;
                FEmployeeEquipmentPullQuote CostQuote;
                Work->EquipmentManager->QuotePull(Work->CurrentBannerId, PullCounts[Index], CostQuote);
                const FString ExpectedCost = CostQuote.AvailableTickets >= PullCounts[Index]
                    ? FString::Printf(TEXT("×%d"), PullCounts[Index]) : FString::Printf(TEXT("%d/%d"), CostQuote.AvailableTickets, PullCounts[Index]);
                Check(FString::Printf(TEXT("pull_%d_label"), PullCounts[Index]), Label && Label->GetText().ToString() == TEXT("뽑기"));
                // 1·10·30 이 서로 다르게 읽히는 축은 이제 비용 칸이다
                Check(FString::Printf(TEXT("pull_%d_keeps_distinct_cost"), PullCounts[Index]), Cost && Cost->GetText().ToString() == ExpectedCost);
            }
            Capture(); Stage = 1; break;
        }
        case 1:
            SaveCapture();
            if (++CaptureIndex < 3) Capture();
            else
            {
                for (int32 Count : {1,10,30})
                {
                    FEmployeeEquipmentPullQuote DisplayQuote;
                    Work->EquipmentManager->QuotePull(Work->CurrentBannerId, Count, DisplayQuote);
                    DisplayQuote.bCanPull = false;
                    DisplayQuote.ErrorCode = TEXT("InsufficientTickets");
                    DisplayQuote.AvailableTickets = 0;
                    DisplayQuote.MissingTickets = Count;
                    Work->ShowPullQuote(DisplayQuote, Count);
                    Check(FString::Printf(TEXT("pull_%d_shortage_uses_common_notice"), Count), Work->PullNotice.IsValid()
                        && Work->PullNotice->IsInViewport() && !Work->Dialog && Work->RequestedPullCount == Count);
                    auto* Notice = Work->PullNotice.Get();
                    const auto* Message = Notice ? Cast<UTextBlock>(Notice->GetWidgetFromName(TEXT("MessageText"))) : nullptr;
                    Check(FString::Printf(TEXT("pull_%d_notice_shows_requested_count"), Count), Message
                        && Message->GetText().ToString().Contains(FString::Printf(TEXT("%d회"), Count)));
                    Work->ShowPull(Count == 1 ? 30 : 1);
                    Check(FString::Printf(TEXT("pull_%d_notice_blocks_repeat_click"), Count), Work->PullNotice.Get() == Notice && Work->RequestedPullCount == Count);
                    if (Count == 10 && Notice)
                    {
                        if (auto* Cancel = Cast<UButtonWidget>(Notice->GetCancelButtonWidget())) Cancel->OnClicked().Broadcast();
                    }
                    else if (UCloseButtonWidget* Close = CloseButton()) Close->OnCloseClicked.Broadcast();
                    Check(FString::Printf(TEXT("pull_%d_notice_closes_without_closing_panel"), Count), !Work->PullNotice.IsValid()
                        && Panel->IsActivated() && UIBase->GetActivePromptWidget() == Panel.Get());
                }
                Work->HandleTicketSelected(nullptr, 2); SavedBanner = Work->CurrentBannerId;
                FEmployeeEquipmentPullQuote Quote;
                Work->EquipmentManager->QuotePull(SavedBanner, 10, Quote);
                Quote.bCanPull = false; Quote.ErrorCode = TEXT("InsufficientTickets");
                Quote.AvailableTickets = 0; Quote.MissingTickets = 10;
                Work->ShowPullQuote(Quote, 10);
                bCaptureNotice = true; CaptureIndex = 0; Capture(); Stage = 8;
            }
            break;
        case 8:
            SaveCapture();
            if (++CaptureIndex < 3) Capture();
            else
            {
                auto* Notice = Work->PullNotice.Get();
                auto* Confirm = Notice ? Cast<UButtonWidget>(Notice->GetConfirmButtonWidget()) : nullptr;
                Check(TEXT("shortage_confirm_routes_to_shop"), Confirm != nullptr);
                if (!Confirm) return false;
                Confirm->OnClicked().Broadcast(); Advance(2);
            }
            break;
        case 2:
            Shop = Cast<UShopPanelWidget>(UIBase->GetActivePromptWidget());
            Check(TEXT("equipment_shop_uses_real_prompt_stack"), Shop.IsValid() && UIBase->GetPromptStackCount() == 2 && HasUIInput());
            if (!Shop.IsValid()) return false;
            Shop->DeactivateWidget(); Advance(3); break;
        case 3:
        {
            Check(TEXT("shop_return_preserves_ticket_banner_count"), UIBase->GetActivePromptWidget() == Panel.Get()
                && Work->GetCurrentView() == EEquipmentWorkspaceView::Gacha
                && Work->CurrentTicketType == EItemType::EmployeeEquipmentTicketPremium && Work->CurrentBannerId == SavedBanner
                && Work->RequestedPullCount == 10 && HasUIInput());
            const auto& Instances = PlayWorld->GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>()->GetState().Instances;
            if (Instances.IsEmpty())
            {
                Check(TEXT("owned_equipment_available_for_read_only_reveal"), false);
                Panel->DeactivateWidget(); Advance(5); break;
            }
            SourceItem = Instances.CreateConstIterator().Key();
            Work->LastResults = { SourceItem };
            FGachaRevealAwardContext Award; Award.bOwnedBefore.Init(true, Work->LastResults.Num());
            Work->ShowResultReveal(Award, Work->CurrentTicketType, Work->CurrentEmployeeId);
            Check(TEXT("existing_owned_item_uses_production_reveal_lifecycle"), Work->RevealPresentation && Work->RevealPresentation->IsInViewport()
                && Work->RevealPresentation->OnPresentationClosed.IsBound());
            // 공개 연출은 1회 SSS 기준 약 7.4초(던지기~단독 장면~정리 + [확인] 0.5초) ― 실제 박자로 끝까지 기다린다
            Advance(4, 9.f); break;
        }
        case 4:
        {
            auto* Reveal = Work->RevealPresentation.Get();
            auto* Confirm = Reveal ? Cast<UButtonWidget>(Reveal->GetConfirmButtonWidget()) : nullptr;
            Check(TEXT("equipment_reveal_confirm_becomes_visible"), Confirm && Confirm->IsVisible());
            if (!Confirm) return false;
            Confirm->OnClicked().Broadcast();
            Advance(9); break;
        }
        case 9:
        {
            Check(TEXT("reveal_confirm_returns_directly_to_gacha_without_results_dialog"), !Work->RevealPresentation && !Work->Dialog
                && !Work->PullNotice.IsValid() && Work->GetCurrentView() == EEquipmentWorkspaceView::Gacha
                && Panel->IsActivated() && UIBase->GetActivePromptWidget() == Panel.Get() && HasUIInput());
            UCloseButtonWidget* Close = CloseButton();
            Check(TEXT("frame_close_button_bound"), Close && Close->OnCloseClicked.IsBound());
            if (!Close) { Work->DismissDialog(); Panel->DeactivateWidget(); Advance(5); break; }
            Close->OnCloseClicked.Broadcast(); Advance(5); break;
        }
        case 5:
            Check(TEXT("equipment_gacha_close_restores_normal_input"), UIBase->GetPromptStackCount() == 0 && !HasUIInput());
            if (!Push(TEXT("reopen_"))) return false;
            Advance(6); break;
        case 6:
        {
            if (!ResolveWork()) return false;
            Check(TEXT("fresh_reopen_lands_on_gacha_with_toolbar_hidden"), UIBase->GetActivePromptWidget() == Panel.Get()
                && Work->GetCurrentView() == EEquipmentWorkspaceView::Gacha
                && Work->WorkspaceToolbar && Work->WorkspaceToolbar->GetVisibility() == ESlateVisibility::Collapsed);
            if (UCloseButtonWidget* Close = CloseButton()) Close->OnCloseClicked.Broadcast();
            else Panel->DeactivateWidget();
            Advance(7); break;
        }
        case 7:
            Check(TEXT("final_empty_stack_and_normal_input"), UIBase->GetPromptStackCount()==0 && !HasUIInput()); return false;
        default: return false;
        }
        return true;
    }
    void Finish()
    {
        for (UCommonActivatableWidget* Widget : {static_cast<UCommonActivatableWidget*>(Shop.Get()), static_cast<UCommonActivatableWidget*>(Panel.Get())})
            if (Widget && Widget->IsActivated()) Widget->DeactivateWidget();
        if (PlayWorld.IsValid())
        {
            auto* Equipment = PlayWorld->GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
            Check(TEXT("equipment_revision_unchanged_no_transactions"), Equipment && Equipment->GetState().Revision==Revision);
        }
        auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("finished"), true);
        Report->SetBoolField(TEXT("passed"), bPassed && Stage==7); Report->SetNumberField(TEXT("last_stage"),Stage);
        Report->SetArrayField(TEXT("checks"),Checks); Report->SetArrayField(TEXT("captures"),Captures);
        FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(Directory,TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Surface.Reset(); Target.Reset(); Renderer.Reset();
        UE_LOG(LogTemp,Display,TEXT("Equipment gacha panel %s: %s"),bPassed && Stage==7 ? TEXT("PASS") : TEXT("FAIL"),*Directory);
    }
};

static TSharedPtr<FEquipmentGachaPanelValidation> GEquipmentGachaPanelValidation;
static FAutoConsoleCommandWithWorld GValidateEquipmentGachaPanel(TEXT("EquipmentUI.ValidateEquipmentGachaPanel"),
    TEXT("Validate the registered equipment gacha panel in MainMap PIE or a -game standalone. Empty prompt stack and owned equipment required; no transactions."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld)
    {
        if (GEquipmentGachaPanelValidation) return;
        auto IsPlayWorld = [](EWorldType::Type Type) { return Type==EWorldType::PIE || Type==EWorldType::Game; };
        UWorld* PlayWorld = CommandWorld && IsPlayWorld(CommandWorld->WorldType) ? CommandWorld : nullptr;
        if (!PlayWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (IsPlayWorld(Context.WorldType) && Context.World()) { PlayWorld=Context.World(); break; }
        auto Run=MakeShared<FEquipmentGachaPanelValidation>();
        if (!Run->Start(PlayWorld)) { Run->Finish(); return; }
        GEquipmentGachaPanelValidation=Run;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta)
        {
            if (Run->Tick(Delta)) return true;
            Run->Finish(); GEquipmentGachaPanelValidation.Reset(); return false;
        }));
    }));

#endif
