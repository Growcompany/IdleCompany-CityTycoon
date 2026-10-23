#if WITH_EDITOR

#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Panel/EquipmentGachaPanelWidget.h"
#include "UI/Panel/ShopPanelWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "UI/Element/Gacha/GachaRevealBoxWidget.h"
#include "UI/Element/Gacha/GachaRevealCardWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Table/BuildingSkinData.h"
#include "Table/EmployeeEquipmentTable.h"
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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

// Display-only results are constructed from the actual catalog; they are never granted.
struct FEquipmentThirtyRevealValidation
{
    TWeakObjectPtr<UWorld> PlayWorld;
    TWeakObjectPtr<UUIBase> UIBase;
    TWeakObjectPtr<UEquipmentGachaPanelWidget> Recruitment;
    TWeakObjectPtr<UEmployeeEquipmentWorkspaceWidget> Work;
    TWeakObjectPtr<UShopPanelWidget> Shop;
    TStrongObjectPtr<UGachaRevealPresentationWidget> Reveal;
    TUniquePtr<FWidgetRenderer> Renderer;
    TStrongObjectPtr<UTextureRenderTarget2D> Target;
    TSharedPtr<SOverlay> Surface;
    TArray<FEmployeeEquipmentInstance> Results;
    TArray<TSharedPtr<FJsonValue>> Checks, Captures;
    TArray<TSharedPtr<FJsonValue>> LiveTrace;
    TWeakObjectPtr<UUserWidget> RenderRoot;
    FString Directory, Scenario;
    FName BannerBefore;
    int64 RevisionBefore = 0;
    int32 Stage = 0, WidthIndex = 0, FrameIndex = 0, GradeCaptureIndex = 0;
    float Wait = .7f;
    bool bPassed = true;
    float LiveElapsed = 0.f;
    int32 PreviousLiveBeat = INDEX_NONE, PreviousLiveCount = 0;
    bool bLiveOrdered = true, bLivePartialSeen = false, bNativeTickPassed = false;

    int32 Width() const { return WidthIndex == 0 ? 2304 : 3120; }
    void Check(const FString& Name, bool bValue)
    {
        auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("name"), Name); Entry->SetBoolField(TEXT("passed"), bValue);
        Checks.Add(MakeShared<FJsonValueObject>(Entry)); bPassed &= bValue;
    }
    bool UIInput() const
    {
        const auto* Controller = PlayWorld.IsValid() ? Cast<AMainMapPlayerController>(PlayWorld->GetFirstPlayerController()) : nullptr;
        return Controller && Controller->GetCurrentInputMode() == EInputMode::UI;
    }
    int32 RevealedCount() const
    {
        int32 Count = 0;
        for (const auto& Motion : Reveal->CardMotion) Count += Motion.bFaceUp && !Motion.bHidden ? 1 : 0;
        return Count;
    }
    // 같은 객체를 다시 쓰려면 닫기 경로 없이 실행기·상태를 비운다(화면에서는 떼지 않는다)
    void Restart()
    {
        Reveal->Runner.Reset();
        Reveal->ResetPresentation();
    }
    void Setup(int32 Count, bool bReduced = false)
    {
        TArray<FEmployeeEquipmentInstance> Subset; Subset.Append(Results.GetData(), Count);
        Restart();
        // 캡처 폭을 무대 폭으로 ― 렌더러가 한 번 그리기 전에는 지오메트리가 앞 폭에 머문다
        Reveal->LastGeometryWidth = static_cast<float>(Width());
        // 사용자 설정을 건드리지 않고 연출 감소 경로만 고른다
        Reveal->ReduceMotionOverride = bReduced;
        Reveal->SetupPullEquipment(Subset, EItemType::EmployeeEquipmentTicketPremium);
    }
    // 실제 박자로 원하는 박자까지 민다 ― 실행기는 한 번에 0.1초만 쓰므로 1/60 씩
    bool AdvanceTo(EGachaRevealBeat Beat, float ExtraSec = 0.f)
    {
        for (int32 Step = 0; Step < 60 * 30 && !Reveal->IsOnBeat(Beat); ++Step) Reveal->DebugAdvanceTime(1.f / 60.f);
        if (!Reveal->IsOnBeat(Beat)) return false;
        Reveal->DebugAdvanceTime(ExtraSec);
        return true;
    }
    bool Start(UWorld* InWorld)
    {
        PlayWorld = InWorld;
        Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EquipmentThirtyPull"));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
        auto* UI = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
        auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        auto* Equipment = Instance ? Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
        UIBase = UI ? UI->GetUIBase() : nullptr;
        Check(TEXT("actual_play_world_and_empty_prompt_stack"), UIBase.IsValid() && Tables && Equipment && UIBase->GetPromptStackCount() == 0);
        if (!UIBase.IsValid() || !Tables || !Equipment || UIBase->GetPromptStackCount() != 0) return false;
        RevisionBefore = Equipment->GetState().Revision;
        Check(TEXT("mainmap_gameplay_controller_required_not_simulate"), Cast<AMainMapPlayerController>(InWorld->GetFirstPlayerController()) != nullptr);
        if (!Cast<AMainMapPlayerController>(InWorld->GetFirstPlayerController())) return false;
        TArray<FName> DefinitionIds; Tables->GetEmployeeEquipmentDefinitions().GetKeys(DefinitionIds); DefinitionIds.Sort(FNameLexicalLess());
        if (DefinitionIds.IsEmpty()) return false;
        for (int32 Index = 0; Index < 30; ++Index)
        {
            auto& Result = Results.AddDefaulted_GetRef(); Result.InstanceId = FGuid::NewGuid(); Result.DefinitionId = DefinitionIds[Index % DefinitionIds.Num()];
        }
        Recruitment = Cast<UEquipmentGachaPanelWidget>(UIBase->PushPromptClass(Tables->GetWidgetClass(EWidgetType::EquipmentGachaPanel).Get(),
            [](UCommonActivatableWidget& Pushed) { if (auto* Gacha = Cast<UEquipmentGachaPanelWidget>(&Pushed)) Gacha->Open(); }));
        Renderer = MakeUnique<FWidgetRenderer>(true);
        return Recruitment.IsValid();
    }
    void BeginCapture(UUserWidget* Widget, const FString& Name)
    {
        RenderRoot = Widget; Scenario = Name;
        Surface = SNew(SOverlay); Surface->AddSlot()[Widget->TakeWidget()];
        Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(Width(), 1440), TF_Bilinear, false));
        Wait = .5f;
    }
    TSharedPtr<FJsonObject> Bounds(UWidget* Widget, const FGeometry& Root) const
    {
        auto Value = MakeShared<FJsonObject>();
        if (!Widget) { Value->SetBoolField(TEXT("valid"), false); return Value; }
        const auto& Geometry = Widget->GetCachedGeometry();
        const FVector2D Position = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
        const FVector2D End = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
        Value->SetBoolField(TEXT("valid"), true); Value->SetStringField(TEXT("name"), Widget->GetName());
        Value->SetNumberField(TEXT("x"), Position.X); Value->SetNumberField(TEXT("y"), Position.Y);
        Value->SetNumberField(TEXT("width"), End.X - Position.X); Value->SetNumberField(TEXT("height"), End.Y - Position.Y);
        Value->SetBoolField(TEXT("minimum_154"), End.X - Position.X >= 153.5f && End.Y - Position.Y >= 153.5f);
        Value->SetBoolField(TEXT("within_canvas"), Position.X >= -.5f && Position.Y >= -.5f && End.X <= Width() + .5f && End.Y <= 1440.5f);
        return Value;
    }
    static bool Separated(const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B, double Tolerance = .5)
    {
        return A->GetBoolField(TEXT("valid")) && B->GetBoolField(TEXT("valid")) && (
            A->GetNumberField(TEXT("x")) + A->GetNumberField(TEXT("width")) <= B->GetNumberField(TEXT("x")) + Tolerance
            || B->GetNumberField(TEXT("x")) + B->GetNumberField(TEXT("width")) <= A->GetNumberField(TEXT("x")) + Tolerance
            || A->GetNumberField(TEXT("y")) + A->GetNumberField(TEXT("height")) <= B->GetNumberField(TEXT("y")) + Tolerance
            || B->GetNumberField(TEXT("y")) + B->GetNumberField(TEXT("height")) <= A->GetNumberField(TEXT("y")) + Tolerance);
    }
    void SaveCapture(bool bButtons)
    {
        FImage Image; TArray64<uint8> Bytes;
        bool bSaved = Target.IsValid() && FImageUtils::GetRenderTargetImage(Target.Get(), Image) && Image.Format == ERawImageFormat::BGRA8;
        const FString Filename = FString::Printf(TEXT("%s_%dx1440.png"), *Scenario, Width());
        if (bSaved) { Image.GammaSpace = EGammaSpace::sRGB; bSaved = FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, Filename)); }
        Check(Filename + TEXT("_saved"), bSaved);
        auto Record = MakeShared<FJsonObject>(); Record->SetStringField(TEXT("scenario"), Scenario); Record->SetStringField(TEXT("png"), Filename);
        Record->SetNumberField(TEXT("width"), Width()); Record->SetNumberField(TEXT("height"), 1440);
        TArray<TSharedPtr<FJsonValue>> Geometry;
        const auto& Root = RenderRoot->GetCachedGeometry();
        if (bButtons)
        {
            TArray<TSharedPtr<FJsonObject>> ButtonBounds;
            for (UWidget* Button : {static_cast<UWidget*>(Work->PullOneButton.Get()), static_cast<UWidget*>(Work->PullTenButton.Get()), static_cast<UWidget*>(Work->PullThirtyButton.Get())})
            {
                const auto Bound = Bounds(Button, Root); Geometry.Add(MakeShared<FJsonValueObject>(Bound)); ButtonBounds.Add(Bound);
                // 시안 CTA 높이는 116px; 기존 버튼의 터치 영역은 너비 154px·높이 116px 이상이어야 한다.
                Check(Filename + TEXT("_") + (Button ? Button->GetName() : TEXT("missing")), Button && Bound->GetBoolField(TEXT("within_canvas"))
                    && Bound->GetNumberField(TEXT("width")) >= 153.5 && Bound->GetNumberField(TEXT("height")) >= 115.5);
            }
            bool bButtonsSeparated = true;
            for (int32 I = 0; I < ButtonBounds.Num(); ++I)
                for (int32 J = I + 1; J < ButtonBounds.Num(); ++J)
                    bButtonsSeparated &= Separated(ButtonBounds[I], ButtonBounds[J], 0.);
            Check(Filename + TEXT("_pull_buttons_do_not_overlap"), bButtonsSeparated);
        }
        else
        {
            Record->SetNumberField(TEXT("card_count"), Reveal->DealtCards.Num()); Record->SetNumberField(TEXT("revealed_count"), RevealedCount());
            Record->SetNumberField(TEXT("beat_index"), Reveal->GetRunner().GetBeatIndex());
            Record->SetNumberField(TEXT("clock_ms"), Reveal->GetRunner().GetClockMs());
            const auto ConfirmBounds = Bounds(Reveal->ConfirmButton, Root);
            TArray<TSharedPtr<FJsonObject>> SortedCardBounds;
            for (UGachaRevealCardWidget* Card : Reveal->DealtCards)
            {
                const auto Bound = Bounds(Card, Root); Geometry.Add(MakeShared<FJsonValueObject>(Bound));
                if (!Scenario.Contains(TEXT("final")) || !Card) continue;
                // 결과 카드는 탭 대상(Task 11 툴팁) ― 30회 정리 칸도 2304 폭에서 189 라 터치 154 를 지킨다
                Check(Filename + TEXT("_") + Card->GetName(), Bound->GetBoolField(TEXT("within_canvas")) && Bound->GetBoolField(TEXT("minimum_154")));
                SortedCardBounds.Add(Bound);
                Check(Filename + TEXT("_") + Card->GetName() + TEXT("_does_not_overlap_confirm"), Separated(Bound, ConfirmBounds));
            }
            // 지급 순서와 정리 칸의 화면 순서는 다르므로 화면의 행·열 순서에서 인접 간격을 잰다.
            SortedCardBounds.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
            {
                const double AY = A->GetNumberField(TEXT("y")), BY = B->GetNumberField(TEXT("y"));
                return AY == BY ? A->GetNumberField(TEXT("x")) < B->GetNumberField(TEXT("x")) : AY < BY;
            });
            for (int32 I = 1; I < SortedCardBounds.Num(); ++I)
            {
                const auto& Previous = SortedCardBounds[I - 1];
                const auto& Current = SortedCardBounds[I];
                if (FMath::Abs(Previous->GetNumberField(TEXT("y")) - Current->GetNumberField(TEXT("y"))) >= 1.) continue;
                const double Gap = Current->GetNumberField(TEXT("x")) - Previous->GetNumberField(TEXT("x")) - Previous->GetNumberField(TEXT("width"));
                Check(Filename + TEXT("_") + Current->GetStringField(TEXT("name")) + TEXT("_no_oversized_empty_card_cell"), Gap >= -1. && Gap <= 32.);
            }
            Geometry.Add(MakeShared<FJsonValueObject>(ConfirmBounds));
            // [확인] = 시안 크기 360×118(10c 트리 ConfirmBox) ― 높이가 154 미만이라 옛 minimum_154 대신 저작 크기가 눌리지 않았는지 본다
            if (Scenario.Contains(TEXT("final"))) Check(Filename + TEXT("_confirm_keeps_mockup_size"), ConfirmBounds->GetBoolField(TEXT("valid"))
                && ConfirmBounds->GetBoolField(TEXT("within_canvas"))
                && ConfirmBounds->GetNumberField(TEXT("width")) >= 359.5 && ConfirmBounds->GetNumberField(TEXT("height")) >= 117.5);
        }
        Record->SetArrayField(TEXT("geometry"), Geometry); Captures.Add(MakeShared<FJsonValueObject>(Record));
        Surface.Reset(); Target.Reset();
    }
    // 공개 단계 표본: 던지기 첫 틱 · 뚜껑 · 뿌리기 끝 · 뒤집기 중 · 결과(+0.6초)
    void PrepareFrame()
    {
        static const TCHAR* Names[] = {TEXT("thirty_throw"), TEXT("thirty_lid"), TEXT("thirty_dealt"), TEXT("thirty_cascade"), TEXT("thirty_final")};
        static const EGachaRevealBeat Beats[] = {EGachaRevealBeat::Throw, EGachaRevealBeat::Lid, EGachaRevealBeat::Cascade, EGachaRevealBeat::Cascade, EGachaRevealBeat::Result};
        static const float Extra[] = {0.f, .1f, 0.f, .5f, .6f};
        Setup(30);
        const bool bReached = AdvanceTo(Beats[FrameIndex], Extra[FrameIndex]);
        const int32 Count = RevealedCount();
        const bool bExpected = FrameIndex <= 2 ? Count == 0 : (FrameIndex == 3 ? Count > 0 && Count < 30 : Count == 30);
        Check(FString::Printf(TEXT("width_%d_phase_%d_reached_and_revealed_count"), Width(), FrameIndex), bReached && Reveal->DealtCards.Num() == 30 && bExpected);
        BeginCapture(Reveal.Get(), Names[FrameIndex]);
    }
    void CheckOtherCountsAndReuse()
    {
        for (int32 Count : {1, 10, 30})
        {
            Setup(Count); Reveal->DebugSkipToResult(); Reveal->DebugAdvanceTime(.6f);
            Check(FString::Printf(TEXT("equipment_%d_same_object_reconfigure_and_skip"), Count), Reveal->DealtCards.Num() == Count
                && RevealedCount() == Count && Reveal->IsOnBeat(EGachaRevealBeat::Result)
                && Reveal->ConfirmButton && Reveal->ConfirmButton->GetVisibility() == ESlateVisibility::Visible);
        }
        Setup(30, true); Reveal->DebugAdvanceTime(1.f / 60.f);
        Check(TEXT("reduced_motion_thirty_first_tick_result_all_cards"), Reveal->IsOnBeat(EGachaRevealBeat::Result) && RevealedCount() == 30
            && Reveal->BoxWidget && Reveal->BoxWidget->GetVisibility() == ESlateVisibility::Collapsed);
        Reveal->DebugAdvanceTime(.55f);
        Check(TEXT("reduced_motion_confirm_after_half_second"), Reveal->ConfirmButton && Reveal->ConfirmButton->GetVisibility() == ESlateVisibility::Visible);
        auto* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        const auto Traits = Tables->GetAllBuildingTraits();
        const auto Skins = Tables->GetBuildingSkinsByRarity(ELootBoxRarity::Common);
        Check(TEXT("trait_and_skin_regression_catalog_fixtures_available"), !Traits.IsEmpty() && !Skins.IsEmpty());
        if (!Traits.IsEmpty())
        {
            TArray<FBuildingTraitGachaResult> TraitResults;
            for (int32 Index = 0; Index < 10; ++Index) { auto& Item = TraitResults.AddDefaulted_GetRef(); Item.ResultTraitID = Traits[Index % Traits.Num()].TraitID; Item.Rarity = Traits[Index % Traits.Num()].Rarity; }
            Restart(); Reveal->ReduceMotionOverride = false; Reveal->SetupPullTrait(TraitResults, false); Reveal->DebugSkipToResult(); Reveal->DebugAdvanceTime(.6f);
            Check(TEXT("trait_ten_uses_same_reveal_cards"), !Reveal->bEquipmentReveal && Reveal->Kind == EGachaRevealKind::Trait && Reveal->DealtCards.Num() == 10 && RevealedCount() == 10);
        }
        if (!Skins.IsEmpty())
        {
            TArray<FBuildingSkinGachaResult> SkinResults;
            for (int32 Index = 0; Index < 10; ++Index) { auto& Item = SkinResults.AddDefaulted_GetRef(); Item.ResultSkinID = Skins[Index % Skins.Num()].SkinID; Item.Rarity = Skins[Index % Skins.Num()].Rarity; }
            Restart(); Reveal->ReduceMotionOverride = false; Reveal->SetupPullSkin(SkinResults, false); Reveal->DebugSkipToResult(); Reveal->DebugAdvanceTime(.6f);
            Check(TEXT("skin_ten_uses_same_reveal_cards"), !Reveal->bEquipmentReveal && Reveal->Kind == EGachaRevealKind::Skin && Reveal->DealtCards.Num() == 10 && RevealedCount() == 10);
        }
        Setup(30);
        Check(TEXT("fresh_equipment_reopen_has_no_trait_skin_or_old_reveal_flags"), Reveal->bEquipmentReveal && Reveal->DealtCards.Num() == 30 && RevealedCount() == 0);
    }
    void CheckEquipmentGradeEffects()
    {
        const auto OriginalResults = Results;
        auto* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        for (int32 GradeIndex = 0; GradeIndex < static_cast<int32>(EEmployeeEquipmentGrade::Count); ++GradeIndex)
        {
            const auto Grade = static_cast<EEmployeeEquipmentGrade>(GradeIndex);
            const FEmployeeEquipmentDefinitionRow* Definition = nullptr;
            for (const auto& Pair : Tables->GetEmployeeEquipmentDefinitions())
                if (Pair.Value.Grade == Grade) { Definition = &Pair.Value; break; }
            Check(FString::Printf(TEXT("grade_%d_display_catalog_available"), GradeIndex), Definition != nullptr);
            if (!Definition) continue;
            Results.SetNum(2);
            for (auto& Result : Results) { Result.DefinitionId = Definition->DefinitionId; Result.InstanceId = FGuid::NewGuid(); }
            Setup(2);
            const UGachaRevealCardWidget* Card = Reveal->DealtCards.IsValidIndex(0) ? Reveal->DealtCards[0].Get() : nullptr;
            Check(FString::Printf(TEXT("grade_%d_entry_and_card_use_definition_grade"), GradeIndex), Reveal->Entries.Num() == 2
                && Reveal->Entries[0].Grade == GradeIndex && Card && Card->GetData().Grade == GradeIndex && Card->GetData().Name.EqualTo(Definition->DisplayName));
            // S 이상은 뒤집지 않고 도장·단독 장면으로, 그 아래는 한 번에 뒤집기로 공개된다(설계서 §4-6)
            AdvanceTo(EGachaRevealBeat::Result, .6f);
            Check(FString::Printf(TEXT("grade_%d_reaches_result_with_both_cards_face_up"), GradeIndex), Reveal->IsOnBeat(EGachaRevealBeat::Result) && RevealedCount() == 2);
        }
        Results = OriginalResults;
        Setup(30);
    }
    bool PrepareGradeCapture()
    {
        auto* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
        const auto Grade = static_cast<EEmployeeEquipmentGrade>(GradeCaptureIndex);
        for (const auto& Pair : Tables->GetEmployeeEquipmentDefinitions())
        {
            if (Pair.Value.Grade != Grade) continue;
            FEmployeeEquipmentInstance Result; Result.InstanceId = FGuid::NewGuid(); Result.DefinitionId = Pair.Key;
            const auto OriginalResults = Results; Results = {Result, Result}; Setup(2); Results = OriginalResults;
            Reveal->DebugSkipToResult(); Reveal->DebugAdvanceTime(.6f);
            BeginCapture(Reveal.Get(), FString::Printf(TEXT("grade_%d_settled_final"), GradeCaptureIndex));
            return true;
        }
        Check(FString::Printf(TEXT("grade_%d_capture_catalog_available"), GradeCaptureIndex), false);
        return false;
    }
    void RecordLiveFrame()
    {
        const int32 Count = RevealedCount();
        const int32 Beat = Reveal->GetRunner().GetBeatIndex();
        bLiveOrdered &= Beat >= PreviousLiveBeat && Count >= PreviousLiveCount;
        bLivePartialSeen |= Count > 0 && Count < 30;
        auto Frame = MakeShared<FJsonObject>(); Frame->SetNumberField(TEXT("elapsed"), LiveElapsed);
        Frame->SetNumberField(TEXT("beat_index"), Beat); Frame->SetNumberField(TEXT("revealed_count"), Count);
        LiveTrace.Add(MakeShared<FJsonValueObject>(Frame)); PreviousLiveBeat = Beat; PreviousLiveCount = Count;
    }
    bool Tick(float DeltaSeconds)
    {
        if (!PlayWorld.IsValid() || !UIBase.IsValid()) return false;
        if (Surface.IsValid() && Target.IsValid()) Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(Width(), 1440), 0.f);
        Wait -= DeltaSeconds; if (Wait > 0.f) return true;
        switch (Stage)
        {
        case 0:
        {
            auto* Host = Recruitment.IsValid() ? Cast<UPanelWidget>(Recruitment->GetWidgetFromName(TEXT("EquipmentGachaHost"))) : nullptr;
            Work = Host && Host->GetChildrenCount() ? Cast<UEmployeeEquipmentWorkspaceWidget>(Host->GetChildAt(0)) : nullptr;
            Check(TEXT("recruitment_gacha_workspace_bound"), Work.IsValid() && Work->IsGachaHost()); if (!Work.IsValid()) return false;
            Work->HandleTicketSelected(nullptr, 2);
            FEmployeeEquipmentPullQuote DisplayQuote;
            Work->EquipmentManager->QuotePull(Work->CurrentBannerId, 30, DisplayQuote);
            DisplayQuote.bCanPull = false;
            DisplayQuote.ErrorCode = TEXT("InsufficientTickets");
            DisplayQuote.AvailableTickets = 0;
            DisplayQuote.MissingTickets = 30;
            Work->ShowPullQuote(DisplayQuote, 30);
            BannerBefore = Work->CurrentBannerId;
            Check(TEXT("synthetic_thirty_shortage_preserves_refill_count"), Work->RequestedPullCount == 30 && Work->PullNotice.IsValid());
            Work->HandleShop(); Stage = 1; Wait = .7f; break;
        }
        case 1:
        {
            Shop = Cast<UShopPanelWidget>(UIBase->GetActivePromptWidget());
            const auto* Info = Shop.IsValid() ? Cast<UTextBlock>(Shop->GetWidgetFromName(TEXT("InfoText"))) : nullptr;
            Check(TEXT("existing_shop_retains_thirty_ticket_refill_request"), Shop.IsValid() && UIBase->GetPromptStackCount() == 2 && UIInput()
                && Info && Info->GetText().ToString().Contains(TEXT("필요 30장")));
            if (!Shop.IsValid()) return false;
            Shop->DeactivateWidget(); Stage = 2; Wait = .7f; break;
        }
        case 2:
            Check(TEXT("shop_return_preserves_thirty_banner_tier_and_ui_input"), UIBase->GetActivePromptWidget() == Recruitment.Get() && UIBase->GetPromptStackCount() == 1
                && Work->RequestedPullCount == 30 && Work->CurrentBannerId == BannerBefore && Work->CurrentTicketType == EItemType::EmployeeEquipmentTicketPremium && UIInput());
            BeginCapture(Recruitment.Get(), TEXT("gacha_three_quantities")); Stage = 3; break;
        case 3:
            SaveCapture(true);
            if (++WidthIndex < 2) BeginCapture(Recruitment.Get(), TEXT("gacha_three_quantities"));
            else { Recruitment->DeactivateWidget(); Stage = 4; Wait = .7f; WidthIndex = 0; }
            break;
        case 4:
        {
            Check(TEXT("shop_test_closes_prompt_stack"), UIBase->GetPromptStackCount() == 0);
            auto* Tables = PlayWorld->GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
            const auto Class = Tables->GetWidgetClass(EWidgetType::GachaRevealPresentation);
            Reveal.Reset(Class ? CreateWidget<UGachaRevealPresentationWidget>(PlayWorld->GetFirstPlayerController(), Class) : nullptr);
            Check(TEXT("shared_reveal_registered"), Reveal.IsValid()); if (!Reveal.IsValid()) return false;
            // 캡처 동안은 시험이 시계를 쥔다 ― 뷰포트 NativeTick 과 겹치지 않게 화면에는 8단계(실시간 관찰)에서만 붙인다
            Reveal->SetDisableExitFade(true);
            Setup(30);
            Check(TEXT("thirty_reveal_keeps_all_results"), Reveal->DealtCards.Num() == 30 && Reveal->Entries.Num() == 30);
            PrepareFrame(); Stage = 5; break;
        }
        case 5:
            SaveCapture(false);
            if (++FrameIndex < 5) PrepareFrame();
            else if (++WidthIndex < 2) { FrameIndex = 0; PrepareFrame(); }
            else { WidthIndex = 0; CheckOtherCountsAndReuse(); CheckEquipmentGradeEffects(); Setup(30); Reveal->DebugSkipToResult(); Reveal->DebugAdvanceTime(.6f); BeginCapture(Reveal.Get(), TEXT("thirty_skip_final")); Stage = 6; }
            break;
        case 6:
            SaveCapture(false); Setup(30, true); Reveal->DebugAdvanceTime(.6f); BeginCapture(Reveal.Get(), TEXT("thirty_reduced_final")); Stage = 7; break;
        case 7:
            SaveCapture(false); GradeCaptureIndex = 0;
            if (!PrepareGradeCapture()) return false;
            Stage = 9; break;
        case 9:
            SaveCapture(false);
            if (++GradeCaptureIndex < static_cast<int32>(EEmployeeEquipmentGrade::Count))
            {
                if (!PrepareGradeCapture()) return false;
                break;
            }
            Surface.Reset(); Target.Reset(); RenderRoot.Reset();
            Setup(30); Reveal->AddToViewport(1000);
            RecordLiveFrame(); Stage = 8; Wait = 0.f; break;
        case 8:
        {
            // 게임 뷰포트의 NativeTick 만 연출을 민다 ― 이 단계는 관찰만 한다(30회 SSS 포함 약 9.7초)
            LiveElapsed += DeltaSeconds; RecordLiveFrame();
            const bool bCompleted = Reveal->IsOnBeat(EGachaRevealBeat::Result) && Reveal->GetRunner().IsConfirmEnabled() && RevealedCount() == 30;
            if (!bCompleted && LiveElapsed < 14.f) break;
            bool bSettled = Reveal->ConfirmButton && Reveal->ConfirmButton->GetVisibility() == ESlateVisibility::Visible;
            for (UGachaRevealCardWidget* Card : Reveal->DealtCards) bSettled &= Card && Card->GetRenderOpacity() >= .99f;
            bNativeTickPassed = bCompleted && bLiveOrdered && bLivePartialSeen && bSettled && Reveal->DealtCards.Num() == 30;
            Check(TEXT("native_tick_thirty_reaches_ordered_complete_result_before_timeout"), bNativeTickPassed);
            if (Reveal->ConfirmButton) Reveal->ConfirmButton->OnClicked().Broadcast();
            Stage = 10; Wait = .35f; break;
        }
        case 10:
            Check(TEXT("confirm_dismisses_reveal"), !Reveal->IsInViewport() && !Reveal->IsPresentationOpen());
            return false;
        default: return false;
        }
        return true;
    }
    void Finish()
    {
        if (Reveal.IsValid() && Reveal->IsInViewport()) { Reveal->DeactivateWidget(); Reveal->RemoveFromParent(); }
        if (Shop.IsValid() && Shop->IsActivated()) Shop->DeactivateWidget();
        if (Recruitment.IsValid() && Recruitment->IsActivated()) Recruitment->DeactivateWidget();
        if (PlayWorld.IsValid())
        {
            auto* Equipment = PlayWorld->GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
            Check(TEXT("display_validation_never_grants_or_spends_equipment"), Equipment && Equipment->GetState().Revision == RevisionBefore);
        }
        auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("passed"), bPassed && Stage == 10 && bNativeTickPassed); Report->SetBoolField(TEXT("finished"), true);
        Report->SetStringField(TEXT("scope"), TEXT("Actual PIE widgets and shop stack, synthetic display-only results, beat-runner driven landscape captures followed by a viewport-only NativeTick thirty-card run. No purchase, draw or save. Result card tooltips belong to Task 11."));
        Report->SetArrayField(TEXT("checks"), Checks); Report->SetArrayField(TEXT("captures"), Captures);
        Report->SetArrayField(TEXT("live_trace"), LiveTrace); Report->SetNumberField(TEXT("live_elapsed"), LiveElapsed); Report->SetBoolField(TEXT("native_tick_passed"), bNativeTickPassed);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("reveal.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Surface.Reset(); Target.Reset(); Renderer.Reset(); Reveal.Reset();
        UE_LOG(LogTemp, Display, TEXT("Equipment thirty reveal validation %s"), bPassed && Stage == 10 && bNativeTickPassed ? TEXT("PASS") : TEXT("FAIL"));
    }
};

static TSharedPtr<FEquipmentThirtyRevealValidation> GEquipmentThirtyRevealValidation;
static FAutoConsoleCommandWithWorld GValidateEquipmentThirtyReveal(TEXT("EquipmentUI.ValidateThirtyReveal"), TEXT("Capture actual thirty-card reveal phases and validate 1/10/30 controls plus shop return, without transactions."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld)
    {
        if (GEquipmentThirtyRevealValidation) return;
        UWorld* PlayWorld = CommandWorld && CommandWorld->WorldType == EWorldType::PIE ? CommandWorld : nullptr;
        if (!PlayWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); break; }
        auto Run = MakeShared<FEquipmentThirtyRevealValidation>();
        if (!Run->Start(PlayWorld)) { Run->Finish(); return; }
        GEquipmentThirtyRevealValidation = Run;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float DeltaSeconds)
        {
            if (Run->Tick(DeltaSeconds)) return true;
            Run->Finish(); GEquipmentThirtyRevealValidation.Reset(); return false;
        }));
    }));

#endif
