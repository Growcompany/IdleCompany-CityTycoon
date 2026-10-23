#if WITH_EDITOR
#include "UI/Panel/EmployeeEquipmentPanelWidget.h"
#include "UI/Panel/ShopPanelWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonTextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "TextureCompiler.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"

struct FEquipmentTicketUIValidation
{
    static void Run(UWorld* PlayWorld)
    {
        auto* GI = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
        auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        auto* Equipment = GI ? GI->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
        if (!Tables || !Equipment || !PlayWorld->GetFirstPlayerController()) return;
        const int64 RevisionBefore = Equipment->GetState().Revision;
        auto Report = MakeShared<FJsonObject>();
        bool bPassed = true;
        auto Check = [&Report, &bPassed](const FString& Key, bool bValue) { Report->SetBoolField(Key, bValue); bPassed &= bValue; };
        const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentPanel);
        TStrongObjectPtr<UEmployeeEquipmentPanelWidget> Panel(Class ? CreateWidget<UEmployeeEquipmentPanelWidget>(PlayWorld->GetFirstPlayerController(), Class) : nullptr);
        if (!Panel.IsValid()) return;
        Panel->AddToViewport(240); Panel->ActivateWidget(); Panel->OpenWithContext(INDEX_NONE, EEquipmentWorkspaceView::Gacha);
        auto* Host = Cast<UPanelWidget>(Panel->GetWidgetFromName(TEXT("WorkspaceHost")));
        auto* Work = Host && Host->GetChildrenCount() ? Cast<UEmployeeEquipmentWorkspaceWidget>(Host->GetChildAt(0)) : nullptr;
        if (!Work) { Panel->DeactivateWidget(); Panel->RemoveFromParent(); return; }
        Check(TEXT("gacha_hides_employee_controls"), Work->EmployeeSelectorRow && Work->BalanceText && Work->GachaButton
            && Work->EmployeeSelectorRow->GetVisibility() == ESlateVisibility::Collapsed
            && Work->BalanceText->GetVisibility() == ESlateVisibility::Collapsed
            && Work->GachaButton->GetVisibility() == ESlateVisibility::Collapsed);
        const EItemType Types[] = {EItemType::EmployeeEquipmentTicketNormal, EItemType::EmployeeEquipmentTicketAdvanced, EItemType::EmployeeEquipmentTicketPremium};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            Work->HandleTicketSelected(nullptr, Index);
            int32 MatchingBanners = 0;
            for (const auto& Pair : Work->TableManager->GetEmployeeEquipmentBanners())
                if (Pair.Value.TicketType == Types[Index]) ++MatchingBanners;
            const auto* SelectedBanner = Work->TableManager->FindEmployeeEquipmentBanner(Work->CurrentBannerId);
            Check(FString::Printf(TEXT("tier_%d_one_banner_selected"), Index), Work->CurrentTicketType == Types[Index]
                && MatchingBanners == 1 && SelectedBanner && SelectedBanner->TicketType == Types[Index]);
            const TArray<FName> BannerIds = {Work->CurrentBannerId};
            for (FName BannerId : BannerIds)
            {
                Work->CurrentBannerId = BannerId;
                Work->SetupBanners(); Work->RefreshGacha();
                Check(BannerId.ToString() + TEXT("_pity_rows_bound_and_visible"), Work->EpicPityText && Work->LegendaryPityText && Work->AscendantPityText
                    && Work->EpicPityProgress && Work->LegendaryPityProgress && Work->AscendantPityProgress
                    && !Work->EpicPityText->GetText().IsEmpty() && !Work->LegendaryPityText->GetText().IsEmpty() && !Work->AscendantPityText->GetText().IsEmpty()
                    && Work->EpicPityProgress->IsVisible() && Work->LegendaryPityProgress->IsVisible() && Work->AscendantPityProgress->IsVisible());
                FEmployeeEquipmentPullQuote Quote;
                const bool bQuoted = Equipment->QuotePull(BannerId, 10, Quote);
                FEmployeeEquipmentPullQuote DisplayQuote = Quote;
                DisplayQuote.bCanPull = false;
                DisplayQuote.ErrorCode = TEXT("InsufficientTickets");
                DisplayQuote.AvailableTickets = 0;
                DisplayQuote.MissingTickets = 10;
                Work->ShowPullQuote(DisplayQuote, 10);
                Check(BannerId.ToString() + TEXT("_quote_routes_to_matching_ticket"), bQuoted && Quote.TicketType == Types[Index] && Quote.RequiredTickets == 10 && Work->PullNotice.IsValid() && !Work->Dialog);
                Check(BannerId.ToString() + TEXT("_shortage_preserves_requested_count_without_grant"), Work->RequestedPullCount == 10 && Equipment->GetState().Revision == RevisionBefore);
                Work->DismissDialog();
            }
        }
        for (int32 Count : {1, 10, 30})
        {
            FEmployeeEquipmentPullQuote InvalidQuote;
            InvalidQuote.bCanPull = true;
            InvalidQuote.BannerId = NAME_None;
            InvalidQuote.TicketType = Work->CurrentTicketType;
            InvalidQuote.RequiredTickets = Count;
            InvalidQuote.AvailableTickets = Count;
            Work->ShowPullQuote(InvalidQuote, Count);
            const auto* FailureTitle = Work->Dialog ? Cast<UCommonTextBlock>(Work->Dialog->GetWidgetFromName(TEXT("TitleText"))) : nullptr;
            Check(FString::Printf(TEXT("direct_pull_%d_executes_and_reports_invalid_banner_without_spend"), Count),
                FailureTitle && FailureTitle->GetText().EqualTo(FText::FromString(TEXT("장비를 뽑지 못했습니다")))
                && Work->PendingRequest.OperationId.IsValid() && Work->PendingRequest.Count == Count
                && Work->PendingRequest.BannerId.IsNone() && !Work->RevealPresentation
                && Equipment->GetState().Revision == RevisionBefore);
            const FGuid OperationId = Work->PendingRequest.OperationId;
            const FName ValidBannerId = Work->CurrentBannerId;
            Work->CurrentBannerId = NAME_None;
            const bool bHadDialog = Work->Dialog != nullptr;
            if (bHadDialog) Work->ShowPull(Count);
            Work->CurrentBannerId = ValidBannerId;
            Check(FString::Printf(TEXT("pull_%d_repeated_click_while_dialog_open_keeps_operation"), Count), bHadDialog && Work->PendingRequest.OperationId == OperationId);
            Work->DismissDialog();
        }
        Work->CurrentView = EEquipmentWorkspaceView::Employee;
        Work->Refresh();
        Check(TEXT("collection_restores_employee_controls"), Work->EmployeeSelectorRow && Work->BalanceText && Work->GachaButton
            && Work->EmployeeSelectorRow->IsVisible() && Work->BalanceText->IsVisible() && Work->GachaButton->IsVisible());
        if (Work->SlotButtonGroup && Work->SlotFilterCombo)
        {
            Work->SlotButtonGroup->OnButtonBaseClicked.Broadcast(Work->SlotButton3, 3);
            Work->SlotButtonGroup->OnButtonBaseClicked.Broadcast(Work->SlotButton5, 5);
            int32 SelectedSlots = 0;
            UButtonWidget* SlotButtons[] = {Work->SlotButton0, Work->SlotButton1, Work->SlotButton2, Work->SlotButton3, Work->SlotButton4, Work->SlotButton5};
            bool bIconsMatchEquipment = true;
            for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(SlotButtons); ++SlotIndex)
            {
                auto* Button = Cast<UIconWithButtonWidget>(SlotButtons[SlotIndex]);
                SelectedSlots += Button && Button->GetSelected() ? 1 : 0;
                const auto* Equipped = Equipment->GetEquippedInstance(Work->CurrentEmployeeId, static_cast<EEmployeeEquipmentSlot>(SlotIndex));
                const auto* Definition = Equipped ? Tables->FindEmployeeEquipmentDefinition(Equipped->DefinitionId) : nullptr;
                auto* Icon = Button ? Cast<UImage>(Button->GetWidgetFromName(TEXT("IconImage"))) : nullptr;
                auto* CountLabel = Button ? Cast<UCommonTextBlock>(Button->GetWidgetFromName(TEXT("CountText"))) : nullptr;
                bIconsMatchEquipment &= Icon && CountLabel && (Definition
                    ? Icon->GetBrush().GetResourceObject() != nullptr && Icon->IsVisible() && !CountLabel->GetText().IsEmpty()
                    : Icon->GetBrush().GetResourceObject() == nullptr && Icon->GetVisibility() == ESlateVisibility::Collapsed && !CountLabel->IsVisible());
            }
            Check(TEXT("slot_clicks_are_exclusive_and_filter_collection"), SelectedSlots == 1
                && Work->SlotButtonGroup->GetSelectedButtonIndex() == 5 && !Work->bAllSlots
                && Work->CurrentSlot == static_cast<EEmployeeEquipmentSlot>(5) && Work->SlotFilterCombo->GetSelectedIndex() == 6);
            Check(TEXT("equipped_slots_use_icons_empty_slots_clear_them"), bIconsMatchEquipment);
            Work->SlotFilterCombo->SetSelectedIndex(0);
            Check(TEXT("all_slots_filter_clears_slot_highlight"), Work->bAllSlots && Work->SlotButtonGroup->GetSelectedButtonIndex() == INDEX_NONE);
        }
        else Check(TEXT("slot_group_and_filter_bound"), false);
        Work->CurrentView = EEquipmentWorkspaceView::Gacha;
        Work->Refresh();
        FEmployeeEquipmentPullQuote Full;
        Full.BannerId = Work->CurrentBannerId; Full.TicketType = Work->CurrentTicketType;
        Full.RequiredTickets = 10; Full.AvailableTickets = 10; Full.ErrorCode = TEXT("InventoryFull");
        Work->ShowPullQuote(Full, 10);
        auto* Notice = Work->PullNotice.IsValid() ? Cast<UCommonTextBlock>(Work->PullNotice->GetWidgetFromName(TEXT("MessageText"))) : nullptr;
        Check(TEXT("full_inventory_does_not_offer_ticket_purchase"), Notice && Notice->GetText().ToString().Contains(TEXT("보관함")) && !Notice->GetText().ToString().Contains(TEXT("부족")));
        if (Work->PullNotice.IsValid()) Work->PullNotice->OnConfirm.Broadcast();
        Check(TEXT("full_inventory_primary_opens_inventory"), Work->CurrentView == EEquipmentWorkspaceView::Inventory && !Work->bReturningFromShop);
        Work->CurrentView = EEquipmentWorkspaceView::Gacha;
        Work->RequestedPullCount = 10;
        const FName BannerBefore = Work->CurrentBannerId;
        const int32 EmployeeBefore = Work->CurrentEmployeeId;
        Work->bReturningFromShop = true;
        Panel->DeactivateWidget(); Panel->ActivateWidget();
        Check(TEXT("reactivation_preserves_employee_ticket_banner_count"), Work->CurrentView == EEquipmentWorkspaceView::Gacha
            && Work->CurrentTicketType == Types[2] && Work->CurrentBannerId == BannerBefore && Work->RequestedPullCount == 10 && Work->CurrentEmployeeId == EmployeeBefore);
        Check(TEXT("reactivation_does_not_auto_pull"), Equipment->GetState().Revision == RevisionBefore);
        auto Bindings = GI->GetSubsystem<UItemInventoryManager>()->OnItemChanged;
        Check(TEXT("one_ticket_subscription"), Bindings.RemoveAll(Work) == 1);
        const auto ShopClass = Tables->GetWidgetClass(EWidgetType::ShopPanel);
        TStrongObjectPtr<UShopPanelWidget> Shop(ShopClass ? CreateWidget<UShopPanelWidget>(PlayWorld->GetFirstPlayerController(), ShopClass) : nullptr);
        if (Shop.IsValid())
        {
            Shop->AddToViewport(245); Shop->ActivateWidget();
            Shop->SetEquipmentTicketContext(Types[2], 10);
            Shop->DeactivateWidget(); Shop->ActivateWidget();
            Check(TEXT("ordinary_shop_reentry_clears_refill_context"), Shop->RefillTicketType == EItemType::None && Shop->RefillRequiredCount == 0);
            Shop->DeactivateWidget(); Shop->RemoveFromParent();
        }
        else Check(TEXT("ordinary_shop_reentry_clears_refill_context"), false);
        Panel->DeactivateWidget(); Panel->RemoveFromParent();
        Report->SetBoolField(TEXT("passed"), bPassed);
        Report->SetStringField(TEXT("scope"), TEXT("Registered PIE widgets, readonly quotes, synthetic shortage/full display, and rejected invalid-banner requests. No valid purchase, draw, inventory fixture, or save."));
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentTicketValidation"));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("ui-contract.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("Equipment ticket UI validation %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    }

    static void CaptureOutline(UWorld* PlayWorld)
    {
        auto* GI = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
        const auto* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        const auto* Grade = Tables ? Tables->FindEmployeeEquipmentGrade(EEmployeeEquipmentGrade::Common) : nullptr;
        if (!Grade) return;
        TArray<TStrongObjectPtr<UMaterialInstanceDynamic>> Materials;
        TArray<TSharedPtr<FSlateBrush>> Brushes;
        auto Surface = SNew(SVerticalBox);
        TArray<TSharedPtr<FJsonValue>> RowManifest;
        const EEmployeeEquipmentSlot Parts[] = {EEmployeeEquipmentSlot::Head, EEmployeeEquipmentSlot::Eyewear};
        int32 RowIndex = 0;
        for (EEmployeeEquipmentSlot Part : Parts)
        {
            const FEmployeeEquipmentDefinitionRow* Definition = nullptr;
            for (const auto& Pair : Tables->GetEmployeeEquipmentDefinitions())
                if (Pair.Value.EquipmentSlot == Part && !Pair.Value.Icon.IsNull() && (!Definition || Pair.Key.LexicalLess(Definition->DefinitionId))) Definition = &Pair.Value;
            UTexture2D* Texture = Definition ? Definition->Icon.LoadSynchronous() : nullptr;
            if (!Texture) return;
            UTexture* TexturesToFinish[] = {Texture};
            FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
            Texture->WaitForStreaming();
            TObjectPtr<UMaterialInstanceDynamic> Material;
            UObject* Outlined = CGEquipmentIcon::Resolve(GI, Texture, Grade, EEmployeeEquipmentIconContext::Idle, Material);
            if (!Material) return;
            Materials.Emplace(Material.Get());
            if (FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIFeatureLevel)) Resource->FinishCompilation();
            for (int32 Size : {48, 64, 96})
            {
                auto Row = SNew(SHorizontalBox);
                for (int32 Background = 0; Background < 2; ++Background)
                    for (int32 Mode = 0; Mode < 2; ++Mode)
                    {
                        TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
                        Brush->DrawAs = ESlateBrushDrawType::Image; Brush->ImageSize = FVector2D(Size);
                        Brush->SetResourceObject(Mode ? Outlined : Texture); Brushes.Add(Brush);
                        Row->AddSlot().AutoWidth()[SNew(SBorder).Padding(0.f)
                            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(Background == 0 ? FLinearColor(.86f, .83f, .76f, 1.f) : FLinearColor(.01096f, .016f, .02956f, 1.f))
                            [SNew(SBox).WidthOverride(140.f).HeightOverride(128.f).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SImage).Image(Brush.Get())]]];
                    }
                Surface->AddSlot().AutoHeight()[Row];
                auto Record = MakeShared<FJsonObject>();
                Record->SetStringField(TEXT("definition"), Definition->DefinitionId.ToString());
                Record->SetNumberField(TEXT("size"), Size); Record->SetNumberField(TEXT("row"), RowIndex++);
                RowManifest.Add(MakeShared<FJsonValueObject>(Record));
            }
        }
        FWidgetRenderer Renderer(true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(560, 768), TF_Bilinear, false));
        if (!Target.IsValid()) return;
        FlushRenderingCommands();
        Renderer.DrawWidget(Target.Get(), Surface, FVector2D(560, 768), 0.f);
        FlushRenderingCommands();
        Renderer.DrawWidget(Target.Get(), Surface, FVector2D(560, 768), 0.f);
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentTicketValidation"));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        FImage Image; TArray64<uint8> Bytes;
        bool bSaved = FImageUtils::GetRenderTargetImage(Target.Get(), Image) && Image.Format == ERawImageFormat::BGRA8;
        bool bAllCellsVisible = bSaved;
        TArray<TSharedPtr<FJsonValue>> CellManifest;
        if (bSaved)
        {
            const auto Pixels = Image.AsBGRA8();
            for (int32 Row = 0; Row < 6; ++Row)
                for (int32 Column = 0; Column < 4; ++Column)
                {
                    const FColor Background = Pixels[(Row * 128 + 4) * 560 + Column * 140 + 4];
                    int32 IconPixels = 0;
                    for (int32 Y = Row * 128 + 8; Y < (Row + 1) * 128 - 8; ++Y)
                        for (int32 X = Column * 140 + 8; X < (Column + 1) * 140 - 8; ++X)
                        {
                            const FColor Pixel = Pixels[Y * 560 + X];
                            if (FMath::Abs(int32(Pixel.R) - Background.R) + FMath::Abs(int32(Pixel.G) - Background.G)
                                + FMath::Abs(int32(Pixel.B) - Background.B) > 15) ++IconPixels;
                        }
                    auto Cell = MakeShared<FJsonObject>();
                    Cell->SetNumberField(TEXT("row"), Row); Cell->SetNumberField(TEXT("column"), Column);
                    Cell->SetNumberField(TEXT("icon_pixels"), IconPixels);
                    CellManifest.Add(MakeShared<FJsonValueObject>(Cell));
                    bAllCellsVisible &= IconPixels >= 8;
                }
        }
        if (bSaved) { Image.GammaSpace = EGammaSpace::sRGB; bSaved = FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, TEXT("outline-small.png"))); }
        auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("saved"), bSaved);
        Report->SetBoolField(TEXT("all_24_cells_visible"), bAllCellsVisible);
        Report->SetBoolField(TEXT("passed"), bSaved && bAllCellsVisible);
        Report->SetArrayField(TEXT("cells"), CellManifest);
        Report->SetStringField(TEXT("columns"), TEXT("light source | light outline | dark source | dark outline; source pixels are unmodified"));
        Report->SetArrayField(TEXT("rows"), RowManifest);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("outline-small.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("Equipment small icon comparison %s: %s"), bSaved && bAllCellsVisible ? TEXT("PASS") : TEXT("FAILED: missing pixels or PNG"), *Directory);
    }
};

static FAutoConsoleCommandWithWorld GEquipmentTicketUIValidation(TEXT("EquipmentUI.ValidateTickets"), TEXT("Read-only PIE 3-tier ticket UI / quote / return / inventory-full validation."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld)
    {
        UWorld* PlayWorld = CommandWorld && CommandWorld->WorldType == EWorldType::PIE ? CommandWorld : nullptr;
        if (!PlayWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); break; }
        FEquipmentTicketUIValidation::Run(PlayWorld);
    }));
static FAutoConsoleCommandWithWorld GEquipmentOutlineUIValidation(TEXT("EquipmentUI.CaptureOutline"), TEXT("Read-only native UI material comparison: hat/eyewear at 48/64/96 px on light/dark backgrounds."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld)
    {
        UWorld* PlayWorld = CommandWorld && CommandWorld->WorldType == EWorldType::PIE ? CommandWorld : nullptr;
        if (!PlayWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); break; }
        FEquipmentTicketUIValidation::CaptureOutline(PlayWorld);
    }));
#endif
