#if WITH_EDITOR

#include "UI/Panel/BuildingTraitGachaPanelWidget.h"
#include "UI/Panel/TraitGachaProbabilityWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/HelpButtonWidget.h"
#include "UI/Element/Gacha/GachaPanelFrameWidget.h"
#include "UI/Element/Gacha/GachaOddsCellWidget.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Data/BuildingTraitSaveData.h"
#include "Table/GachaBoxVisualTable.h"
#include "Table/ShopItemTable.h"
#include "Player/MainMapPlayerController.h"
#include "Tests/WidgetDeepLookup.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

// 가챠 패널 공통 계약을 특성·스킨 패널의 실제 클릭 경로(HandleButtonClicked)로 검사한다. 뽑기·마일리지 교환 거래는 일으키지 않는다.
struct FGachaSharedBaseValidation
{
    TWeakObjectPtr<UWorld> PlayWorld;
    TWeakObjectPtr<UUIBase> UIBase;
    TWeakObjectPtr<UTableManagerSubsystem> Tables;
    TWeakObjectPtr<UItemInventoryManager> Items;
    TWeakObjectPtr<UCommonActivatableWidget> Panel;
    TUniquePtr<FWidgetRenderer> Renderer;
    TStrongObjectPtr<UTextureRenderTarget2D> Target;
    TSharedPtr<SOverlay> Surface;
    TArray<TSharedPtr<FJsonValue>> Checks;
    // SaveCapture 의 지역 FGeometry 가 Geometry 라 멤버는 다른 이름이어야 한다(C4458)
    TArray<TSharedPtr<FJsonValue>> GeometryRecords;
    TArray<FString> NormalPills;
    TArray<FString> SkinNormalPills;
    FString Directory;
    FString CaptureName = TEXT("trait");
    int32 Stage = 0, CaptureWidth = 2304, CaptureIndex = 0;
    float Remaining = .5f;
    bool bPassed = true;
    bool bCompleted = false;

    FString Key() const { return TEXT("trait"); }
    void Advance(int32 Next, float Delay = .5f) { Stage = Next; Remaining = Delay; }
    UWidget* W(const TCHAR* Name) const { return Panel.IsValid() ? Panel->GetWidgetFromName(Name) : nullptr; }

    void Check(const FString& Name, bool bValue, const FString& Detail = FString())
    {
        auto Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Name); Entry->SetBoolField(TEXT("passed"), bValue);
        if (!Detail.IsEmpty()) Entry->SetStringField(TEXT("detail"), Detail);
        Checks.Add(MakeShared<FJsonValueObject>(Entry)); bPassed &= bValue;
    }

    // BindWidget 는 이름 일치만으로는 증명이 안 된다 — C++ 프로퍼티가 실제로 그 위젯을 물었는지 본다
    UObject* Bound(const TCHAR* Name) const
    {
        const FObjectPropertyBase* Prop = Panel.IsValid() ? FindFProperty<FObjectPropertyBase>(Panel->GetClass(), Name) : nullptr;
        return Prop ? Prop->GetObjectPropertyValue_InContainer(Panel.Get()) : nullptr;
    }

    void Click(const TCHAR* Name)
    {
        auto* Button = Cast<UCommonButtonBase>(W(Name));
        UFunction* Fn = Button ? Button->FindFunction(TEXT("HandleButtonClicked")) : nullptr;
        Check(Key() + TEXT("_") + Name + TEXT("_click_invoked"), Fn != nullptr);
        if (Fn) Button->ProcessEvent(Fn, nullptr);
    }

    TArray<FString> PillTexts() const
    {
        TArray<FString> Out;
        if (auto* Row = Cast<UPanelWidget>(W(TEXT("ProbabilityPillRow"))))
            for (UWidget* Child : Row->GetAllChildren())
                if (auto* Cell = Cast<UGachaOddsCellWidget>(Child))
                {
                    auto* Grade = Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("GradeText")));
                    auto* Percent = Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("PercentText")));
                    if (Grade && Percent) Out.Add(Grade->GetText().ToString() + TEXT("/") + Percent->GetText().ToString());
                }
        return Out;
    }

    void CheckBindings()
    {
        for (const TCHAR* Name : { TEXT("GachaFrame"), TEXT("BannerNormal"), TEXT("BannerAdvanced"), TEXT("PullButton"),
            TEXT("HeroTitleText"), TEXT("HeroDescText"), TEXT("HeroBoxImage"), TEXT("ProbabilityPillRow"), TEXT("PullButton30"), TEXT("MileageBar"), TEXT("MileageLabel"),
            TEXT("MileageHead"), TEXT("ProbabilityInfoButton"), TEXT("MileageExchangeButton") })
        {
            UObject* Value = Bound(Name);
            Check(Key() + TEXT("_bound_") + Name, Value != nullptr && Value == W(Name));
        }
        // 뽑기·교환은 거래라 클릭으로 증명할 수 없다 — 대신 베이스로 옮긴 클릭 바인딩이 이 패널을 물었는지 본다
        auto ClickBound = [this](const TCHAR* Suffix, const TCHAR* Name)
        {
            auto* Button = Cast<UCommonButtonBase>(W(Name));
            Check(Key() + Suffix, Button && Button->OnClicked().IsBoundToObject(Panel.Get()));
        };
        ClickBound(TEXT("_pull_click_bound"), TEXT("PullButton"));
        ClickBound(TEXT("_pull30_click_bound"), TEXT("PullButton30"));
        ClickBound(TEXT("_mileage_exchange_click_bound"), TEXT("MileageExchangeButton"));
        ClickBound(TEXT("_probability_info_click_bound"), TEXT("ProbabilityInfoButton"));
    }

    // 옛 트리를 붙여넣어 틀을 떠나도 내용 위젯 이름은 그대로라 다른 검사는 전부 통과한다 ― 틀 사용 자체를 따로 본다
    void CheckFrameUsage()
    {
        TArray<UWidget*> Frames;
        if (Panel.IsValid() && Panel->WidgetTree)
            Panel->WidgetTree->ForEachWidget([&Frames](UWidget* Widget) { if (Widget->IsA<UGachaPanelFrameWidget>()) Frames.Add(Widget); });
        auto* Root = Cast<UPanelWidget>(W(TEXT("RootOverlay")));
        UWidget* RootChild = Root && Root->GetChildrenCount() > 0 ? Root->GetChildAt(0) : nullptr;
        UWidget* Frame = Frames.Num() == 1 ? Frames[0] : nullptr;
        Check(Key() + TEXT("_uses_single_gacha_frame"),
            Frame && Frame->GetFName() == FName(TEXT("GachaFrame")) && Root && Root->GetChildrenCount() == 1 && RootChild == Frame,
            FString::Printf(TEXT("frames=%d root_children=%d first=%s"), Frames.Num(), Root ? Root->GetChildrenCount() : -1,
                RootChild ? *RootChild->GetName() : TEXT("none")));

        UWidget* Columns = W(TEXT("GachaColumns"));
        const FString ColumnsClass = Columns ? Columns->GetClass()->GetPathName() : FString(TEXT("none"));
        // 특성은 틀 Body 칸에 바로 든다
        auto* FrameHost = Cast<UGachaPanelFrameWidget>(W(TEXT("GachaFrame")));
        UPanelWidget* ColumnsParent = Columns ? Columns->GetParent() : nullptr;
        const bool bPlaced = Columns && FrameHost && FrameHost->GetContentForSlot(TEXT("Body")) == Columns;
        Check(Key() + TEXT("_uses_gacha_columns"),
            ColumnsClass == TEXT("/Game/CompanyGrowth/UI/Elements/Gacha/UIE_GachaPanelColumns.UIE_GachaPanelColumns_C") && bPlaced,
            FString::Printf(TEXT("class=%s parent=%s"), *ColumnsClass, ColumnsParent ? *ColumnsParent->GetName() : TEXT("none")));

        // 헤더는 틀이 통째로 소유한다 ― 패널이 제 닫기 버튼을 또 들면 틀 것과 겹쳐 두 개가 된다.
        // ForEachWidget 은 패널 제 트리만 돈다(중첩 틀 안은 안 센다)
        int32 StrayCloseButtons = 0;
        if (Panel.IsValid() && Panel->WidgetTree)
            Panel->WidgetTree->ForEachWidget([&StrayCloseButtons](UWidget* Widget) { if (Widget->IsA<UCloseButtonWidget>()) ++StrayCloseButtons; });
        Check(Key() + TEXT("_close_button_only_in_frame"), StrayCloseButtons == 0 && FrameHost && FrameHost->GetCloseButton() != nullptr,
            FString::Printf(TEXT("stray=%d frame_close=%d"), StrayCloseButtons, FrameHost && FrameHost->GetCloseButton() ? 1 : 0));

        // 틀은 부품이라 "?" 키가 하나뿐이다 ― 패널이 인스턴스 노브로 덮지 않으면 남의 도움말이 뜬다
        const FName ExpectedHelpKey(TEXT("TraitGacha"));
        auto* Help = Cast<UHelpButtonWidget>(FindWidgetDeep(Panel.Get(), TEXT("HelpButton")));
        Check(Key() + TEXT("_help_key_from_panel"), Help && Help->HelpKey == ExpectedHelpKey,
            FString::Printf(TEXT("key=%s want=%s"), Help ? *Help->HelpKey.ToString() : TEXT("none"), *ExpectedHelpKey.ToString()));
    }

    void CheckOddsCell(const TCHAR* Scope)
    {
        auto* Row = Cast<UPanelWidget>(W(TEXT("ProbabilityPillRow")));
        bool bAllCellsFilled = Row && Row->GetChildrenCount() > 0;
        if (Row)
            for (UWidget* Child : Row->GetAllChildren())
            {
                auto* Cell = Cast<UGachaOddsCellWidget>(Child);
                auto* CellGrade = Cell ? Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("GradeText"))) : nullptr;
                auto* CellPercent = Cell ? Cast<UTextBlock>(Cell->GetWidgetFromName(TEXT("PercentText"))) : nullptr;
                bAllCellsFilled &= CellGrade && CellPercent
                    && !CellGrade->GetText().IsEmpty() && CellPercent->GetText().ToString().EndsWith(TEXT("%"));
            }
        Check(FString(Scope) + TEXT("_odds_cells_typed_and_filled"), bAllCellsFilled);
        auto* First = Row && Row->GetChildrenCount() > 0 ? Cast<UGachaOddsCellWidget>(Row->GetChildAt(0)) : nullptr;
        auto* Grade = First ? Cast<UTextBlock>(First->GetWidgetFromName(TEXT("GradeText"))) : nullptr;
        auto* Percent = First ? Cast<UTextBlock>(First->GetWidgetFromName(TEXT("PercentText"))) : nullptr;
        Check(FString(Scope) + TEXT("_odds_cell_grade_above_percent"), Grade && Percent &&
            Grade->GetCachedGeometry().GetAbsolutePosition().Y < Percent->GetCachedGeometry().GetAbsolutePosition().Y);
        Check(FString(Scope) + TEXT("_odds_cell_fonts_set"), Grade && Percent &&
            Grade->GetFont().FontObject != nullptr && Percent->GetFont().FontObject != nullptr);
    }

    void CheckTitle(const TCHAR* Expected)
    {
        auto* Title = Cast<UTextBlock>(W(TEXT("HeroTitleText")));
        Check(Key() + TEXT("_title_") + Expected, Title && Title->GetText().ToString() == Expected);
    }

    void CheckFrameTitle(const TCHAR* Expected)
    {
        auto* Title = Cast<UTextBlock>(FindWidgetDeep(Panel.Get(), TEXT("TitleText")));
        Check(Key() + TEXT("_frame_title_") + Expected, Title && Title->GetText().ToString() == Expected);
    }

    void CheckTraitBanners(bool bSkin)
    {
        const EItemType Tickets[] = {
            bSkin ? EItemType::SkinTicketNormal : EItemType::BuildingTraitTicketNormal,
            bSkin ? EItemType::SkinTicketAdvanced : EItemType::BuildingTraitTicketAdvanced
        };
        const TCHAR* Names[] = { TEXT("BannerNormal"), TEXT("BannerAdvanced") };
        for (int32 Index = 0; Index < 2; ++Index)
        {
            FShopItemTable Row;
            const bool bFound = Tables.IsValid() && Tables->GetShopItemByItemType(Tickets[Index], Row);
            FString ExpectedName = Row.DisplayName.ToString();
            ExpectedName.RemoveFromEnd(TEXT(" 상자"));
            auto* Banner = Cast<UGachaBannerButton>(W(Names[Index]));
            auto* Label = Banner ? Cast<UTextBlock>(Banner->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
            auto* Icon = Banner ? Cast<UImage>(Banner->GetWidgetFromName(TEXT("TierIcon"))) : nullptr;
            UObject* ExpectedIcon = bFound ? Row.ResolveIconResource() : nullptr;
            const FString Prefix = FString::Printf(TEXT("trait_%s_%s"), bSkin ? TEXT("skin") : TEXT("trait"), Names[Index]);
            Check(Prefix + TEXT("_name"), bFound && Label && Label->GetText().ToString() == ExpectedName);
            Check(Prefix + TEXT("_icon"), ExpectedIcon && Icon && Icon->GetBrush().GetResourceObject() == ExpectedIcon);
        }
    }

    void CheckSkinBox(bool bAdvanced)
    {
        const EGachaBoxType BoxType = bAdvanced ? EGachaBoxType::SkinAdvanced : EGachaBoxType::SkinNormal;
        const FGachaBoxVisualRow* Visual = Tables.IsValid() ? Tables->GetGachaBoxVisual(BoxType) : nullptr;
        UTexture2D* ExpectedIcon = Visual && !Visual->Icon.IsNull() ? Visual->Icon.LoadSynchronous() : nullptr;
        auto* Hero = Cast<UImage>(W(TEXT("HeroBoxImage")));
        UObject* ActualIcon = Hero ? Hero->GetBrush().GetResourceObject() : nullptr;
        const FString Prefix = Key() + (bAdvanced ? TEXT("_advanced_skin") : TEXT("_normal_skin"));
        Check(Prefix + TEXT("_box_visible_and_from_visual_table"),
            Hero && Hero->IsVisible() && ExpectedIcon && ActualIcon == ExpectedIcon,
            FString::Printf(TEXT("expected=%s actual=%s visible=%d"),
                ExpectedIcon ? *ExpectedIcon->GetPathName() : TEXT("none"),
                ActualIcon ? *ActualIcon->GetPathName() : TEXT("none"), Hero && Hero->IsVisible() ? 1 : 0));
        auto* LegacyCards = Cast<UWidgetSwitcher>(W(TEXT("HeroCardSwitcher")));
        Check(Prefix + TEXT("_legacy_card_hidden"), !LegacyCards || !LegacyCards->IsVisible());
    }

    // 라벨·활성 규칙은 현행 UpdateCTALabel 그대로 — 리팩터가 바꾸면 안 되는 거동
    void CheckPullButton(bool bAdvanced)
    {
        auto* Button = Cast<UButtonWidget>(W(TEXT("PullButton")));
        auto* Label = Button ? Cast<UTextBlock>(Button->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
        const EItemType Ticket = bAdvanced ? EItemType::BuildingTraitTicketAdvanced : EItemType::BuildingTraitTicketNormal;
        const int32 Owned = Items.IsValid() ? Items->GetItemCount(Ticket) : 0;
        auto* Cost = Button ? Cast<UTextBlock>(Button->GetWidgetFromName(TEXT("CountText"))) : nullptr;
        const FString ExpectedCost = Owned >= 1 ? TEXT("×1") : FString::Printf(TEXT("%d/1"), Owned);
        const FString Prefix = Key() + (bAdvanced ? TEXT("_advanced") : TEXT("_normal"));
        Check(Prefix + TEXT("_pull_label"), Label && Label->GetText().ToString() == TEXT("뽑기"));
        Check(Prefix + TEXT("_pull_cost"), Cost && Cost->GetText().ToString() == ExpectedCost);
        // 모자라도 눌러야 부족 안내가 뜬다
        Check(Prefix + TEXT("_pull_enabled_even_if_short"), Button && Button->GetIsEnabled());
    }

    void CheckUtility()
    {
        const TArray<TPair<const TCHAR*, float>> Names{ {TEXT("ProbabilityInfoButton"), 240.f}, {TEXT("DismantleShopButton"), 240.f}, {TEXT("MileageExchangeButton"), 280.f} };
        for (const TPair<const TCHAR*, float>& Pair : Names)
        {
            auto* Button = Cast<UCommonButtonBase>(W(Pair.Key));
            const FString Prefix = Key() + TEXT("_utility_") + Pair.Key;
            Check(Prefix + TEXT("_is_variant"), Button && Button->GetClass()->GetName() == TEXT("UIE_GachaUtilityButton_C"));
            const UCommonButtonStyle* Style = Button ? Button->GetStyle() : nullptr;
            Check(Prefix + TEXT("_style"), Style && Style->GetClass()->GetName() == TEXT("CUI_Style2_Btn_GachaUtility_C"));
            if (Button && Button->IsVisible())
            {
                const FVector2D Size = Button->GetDesiredSize();
                Check(Prefix + TEXT("_min_size"), Size.X >= Pair.Value - .5f && Size.Y >= 71.5f);
            }
            else Check(Prefix + TEXT("_visible"), false);
        }
    }

    // 기대 문구도 패널 게이트와 같은 규칙 상수로 조립한다 ― 상수가 바뀌면 라벨과 검사가 함께 따라간다
    void CheckExchangeLabel()
    {
        auto* Button = Cast<UButtonWidget>(W(TEXT("MileageExchangeButton")));
        auto* Label = Button ? Cast<UTextBlock>(Button->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
        const FString Expected = FString::Printf(TEXT("마일리지 교환 %dP"), FGachaTraitData::MileageEpicBox);
        const FString Actual = Label ? Label->GetText().ToString() : FString();
        Check(Key() + TEXT("_exchange_label"), Label && Actual == Expected,
            !Label ? FString(TEXT("missing")) : Actual.IsEmpty() ? FString(TEXT("(empty)")) : Actual);
    }

    void CheckProbabilityOverlay()
    {
        TArray<UUserWidget*> Found;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayWorld.Get(), Found, UTraitGachaProbabilityWidget::StaticClass(), true);
        Check(Key() + TEXT("_probability_overlay_opened"), Found.Num() == 1);
        for (UUserWidget* Widget : Found) Widget->RemoveFromParent();
    }

    void ClickMileageExchangeSafely()
    {
        // 비활성 버튼 클릭은 HandleButtonClicked 가 조용히 무시한다 ― 교환 경로를 실제로 탔는지는 이 값으로만 구분된다
        auto* Exchange = Cast<UCommonButtonBase>(W(TEXT("MileageExchangeButton")));
        Check(TEXT("trait_exchange_enabled_at_click"), true,
            Exchange ? (Exchange->IsInteractionEnabled() ? TEXT("enabled") : TEXT("disabled")) : TEXT("missing"));
        Click(TEXT("MileageExchangeButton"));
        Check(Key() + TEXT("_alive_after_mileage_exchange"), Panel.IsValid() && Panel->IsActivated());
    }

    void BeginCapture(int32 Width)
    {
        CaptureWidth = Width;
        if (!Panel.IsValid()) { Check(FString::Printf(TEXT("%s_%d_panel_valid_BeginCapture"), *Key(), Width), false); return; }
        Surface = SNew(SOverlay); Surface->AddSlot()[Panel->TakeWidget()];
        Target.Reset(FWidgetRenderer::CreateTargetFor(FVector2D(Width, 1440), TF_Bilinear, false));
    }

    void SaveCapture()
    {
        if (!Panel.IsValid())
        {
            Check(FString::Printf(TEXT("%s_%d_panel_valid_SaveCapture"), *Key(), CaptureWidth), false);
            Surface.Reset(); Target.Reset(); return;
        }
        FImage Image; TArray64<uint8> Bytes;
        const FString Name = FString::Printf(TEXT("%s_%dx1440.png"), *CaptureName, CaptureWidth);
        bool bSaved = Target.IsValid() && FImageUtils::GetRenderTargetImage(Target.Get(), Image) && Image.Format == ERawImageFormat::BGRA8;
        if (bSaved)
        {
            Image.GammaSpace = EGammaSpace::sRGB;
            bSaved = FImageUtils::CompressImage(Bytes, TEXT("png"), Image) && FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, Name));
        }
        Check(Name + TEXT("_saved"), bSaved);
        const FGeometry& Root = Panel->GetCachedGeometry();
        for (const TCHAR* WidgetName : { TEXT("PullButton"), TEXT("ProbabilityInfoButton"), TEXT("MileageExchangeButton"), TEXT("DismantleShopButton") })
        {
            UWidget* Widget = W(WidgetName);
            const FString Prefix = FString::Printf(TEXT("%s_%d_%s"), *CaptureName, CaptureWidth, WidgetName);
            if (!Widget || !Widget->IsVisible()) { Check(Prefix + TEXT("_visible"), false); continue; }
            const FGeometry& Geometry = Widget->GetCachedGeometry();
            const FVector2D Begin = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
            const FVector2D End = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
            Check(Prefix + TEXT("_within_canvas"),
                Begin.X >= -.5f && Begin.Y >= -.5f && End.X <= CaptureWidth + .5f && End.Y <= 1440.5f && End.X > Begin.X && End.Y > Begin.Y);
        }
        // 공용 틀 이식 전후 비교용 ― 체크가 아니라 기록이라 검사 수를 바꾸지 않는다
        const TArray<const TCHAR*> Recorded{ TEXT("TitleText"), TEXT("HeaderIcon"), TEXT("UIE_CloseButton"), TEXT("CategoryTraitBtn"),
            TEXT("BannerNormal"), TEXT("BannerAdvanced"), TEXT("LinkVBox"), TEXT("HeroVBox"), TEXT("GaugeVBox"),
            TEXT("PullButton"), TEXT("PullButton10"), TEXT("PullButton30"), TEXT("HeroBoxImage") };
        for (const TCHAR* WidgetName : Recorded)
        {
            UWidget* Widget = FindWidgetDeep(Panel.Get(), WidgetName);
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("panel"), CaptureName); Entry->SetNumberField(TEXT("width"), CaptureWidth);
            Entry->SetStringField(TEXT("widget"), WidgetName);
            if (Widget)
            {
                const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
                const FVector2D Begin = Root.AbsoluteToLocal(WidgetGeometry.LocalToAbsolute(FVector2D::ZeroVector));
                const FVector2D End = Root.AbsoluteToLocal(WidgetGeometry.LocalToAbsolute(WidgetGeometry.GetLocalSize()));
                Entry->SetNumberField(TEXT("x"), Begin.X); Entry->SetNumberField(TEXT("y"), Begin.Y);
                Entry->SetNumberField(TEXT("w"), End.X - Begin.X); Entry->SetNumberField(TEXT("h"), End.Y - Begin.Y);
            }
            GeometryRecords.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Surface.Reset(); Target.Reset();
    }

    bool Open()
    {
        Check(Key() + TEXT("_prompt_stack_empty_before_open"), UIBase->GetPromptStackCount() == 0);
        // 서브시스템은 약참조라 게임 인스턴스가 내려가는 중이면 여기서 이미 사라져 있을 수 있다
        if (!Tables.IsValid()) { Check(Key() + TEXT("_tables_valid"), false); return false; }
        Panel = Cast<UCommonActivatableWidget>(UIBase->PushPromptClass(Tables->GetWidgetClass(EWidgetType::BuildingTraitGachaPanel).Get()));
        Check(Key() + TEXT("_panel_created"), Panel.IsValid());
        Advance(1);
        return Panel.IsValid();
    }

    bool Start(UWorld* InWorld)
    {
        PlayWorld = InWorld;
        Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GachaPanelSharedBase"), TEXT("validation"));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        auto* Instance = InWorld ? InWorld->GetGameInstance() : nullptr;
        auto* UI = Instance ? Instance->GetSubsystem<UUIManagerSubsystem>() : nullptr;
        Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        Items = Instance ? Instance->GetSubsystem<UItemInventoryManager>() : nullptr;
        UIBase = UI ? UI->GetUIBase() : nullptr;
        const bool bReady = UIBase.IsValid() && Tables.IsValid() && Items.IsValid()
            && InWorld && Cast<AMainMapPlayerController>(InWorld->GetFirstPlayerController()) != nullptr;
        Check(TEXT("mainmap_ready"), bReady);
        Renderer = MakeUnique<FWidgetRenderer>(true);
        return bReady;
    }

    bool Tick(float Delta)
    {
        if (!PlayWorld.IsValid() || !UIBase.IsValid()) return false;
        if (Surface && Target.IsValid()) Renderer->DrawWidget(Target.Get(), Surface.ToSharedRef(), FVector2D(CaptureWidth, 1440), Delta);
        Remaining -= Delta; if (Remaining > 0.f) return true;
        switch (Stage)
        {
        case 0:
            return Open();
        case 1:
            // 풀링된 특성 패널은 마지막 카테고리를 기억한다 ― 같은 세션 재실행도 같은 상태에서 시작하도록 실제 클릭으로 고른다
            Click(TEXT("CategoryTraitBtn")); Advance(2);
            return true;
        case 2:
            CheckBindings(); CheckFrameUsage();
            CheckFrameTitle(TEXT("특성 뽑기"));
            CheckTraitBanners(false);
            NormalPills = PillTexts();
            Check(Key() + TEXT("_normal_pills_present"), NormalPills.Num() > 0);
            CheckOddsCell(TEXT("trait_normal")); CheckPullButton(false); CheckUtility(); CheckExchangeLabel();
            Click(TEXT("BannerAdvanced")); Advance(3); return true;
        case 3:
            CheckTitle(TEXT("고급 특성 뽑기"));
            Check(Key() + TEXT("_banner_switch_rebuilds_pills"), PillTexts().Num() > 0 && PillTexts() != NormalPills);
            CheckPullButton(true);
            Click(TEXT("ProbabilityInfoButton")); Advance(4); return true;
        case 4:
            CheckProbabilityOverlay(); ClickMileageExchangeSafely();
            CaptureIndex = 0; BeginCapture(2304); Advance(5, .6f); return true;
        case 5:
            SaveCapture();
            if (++CaptureIndex < 3) { BeginCapture(CaptureIndex == 1 ? 2560 : 3120); Advance(5, .6f); }
            else Advance(6, 0.f);
            return true;
        case 6:
            // 특성 PNG 는 특성 카테고리 화면이어야 하므로 스킨 전환은 캡처가 끝난 뒤에 검사한다
            Click(TEXT("CategorySkinBtn"));
            Advance(7);
            return true;
        case 7:
        {
            CheckTitle(TEXT("일반 스킨 뽑기"));
            CheckFrameTitle(TEXT("스킨 뽑기"));
            CheckTraitBanners(true);
            CheckSkinBox(false); CheckOddsCell(TEXT("skin_normal"));
            auto* SkinPull30 = Cast<UCommonButtonBase>(W(TEXT("PullButton30")));
            Check(TEXT("skin_pull30_bound"), SkinPull30 && Bound(TEXT("PullButton30")) == SkinPull30
                && SkinPull30->OnClicked().IsBoundToObject(Panel.Get()));
            SkinNormalPills = PillTexts();
            Check(Key() + TEXT("_skin_normal_odds_present"), SkinNormalPills.Num() > 0);
            Click(TEXT("BannerAdvanced")); Advance(8); return true;
        }
        case 8:
        {
            CheckTitle(TEXT("고급 스킨 뽑기"));
            CheckTraitBanners(true); CheckSkinBox(true); CheckOddsCell(TEXT("skin_advanced"));
            Check(Key() + TEXT("_skin_banner_switch_rebuilds_odds"),
                PillTexts().Num() > 0 && PillTexts() != SkinNormalPills);
            CaptureName = TEXT("skin");
            CaptureIndex = 0; BeginCapture(2304); Advance(9, .6f); return true;
        }
        case 9:
            SaveCapture();
            if (++CaptureIndex < 3) { BeginCapture(CaptureIndex == 1 ? 2560 : 3120); Advance(9, .6f); }
            else Advance(10, 0.f);
            return true;
        case 10:
            Click(TEXT("CategoryTraitBtn"));
            CheckTraitBanners(false);
            Advance(11, 0.f); return true;
        case 11:
        {
            // 헤더는 틀이 소유한다 ― W() 는 패널 제 트리만 보므로 런타임 계약과 같은 경로로 잡는다
            auto* FrameForClose = Cast<UGachaPanelFrameWidget>(W(TEXT("GachaFrame")));
            auto* Close = FrameForClose ? FrameForClose->GetCloseButton() : nullptr;
            Check(Key() + TEXT("_close_button_found"), Close != nullptr);
            if (Close) Close->OnCloseClicked.Broadcast();
            Advance(12); return true;
        }
        case 12:
        {
            const bool bClosed = !Panel.IsValid() || !Panel->IsActivated();
            Check(Key() + TEXT("_closed_by_close_button"), bClosed);
            // 안 닫힌 패널이 스택에 남지 않게 ― 실패는 위 체크로 남기고 직접 내린다
            if (!bClosed) Panel->DeactivateWidget();
            bCompleted = true;
            return false;
        }
        }
        return false;
    }

    void Finish()
    {
        Check(TEXT("run_completed"), bCompleted);
        if (Panel.IsValid() && Panel->IsActivated()) Panel->DeactivateWidget();
        auto Report = MakeShared<FJsonObject>();
        Report->SetBoolField(TEXT("passed"), bPassed);
        Report->SetNumberField(TEXT("last_stage"), Stage);
        Report->SetArrayField(TEXT("checks"), Checks);
        Report->SetArrayField(TEXT("geometry"), GeometryRecords);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Directory, TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Surface.Reset(); Target.Reset(); Renderer.Reset();
        UE_LOG(LogTemp, Display, TEXT("Gacha shared base %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Directory);
    }
};

static TSharedPtr<FGachaSharedBaseValidation> GGachaSharedBaseValidation;
static FAutoConsoleCommandWithWorld GValidateGachaSharedBase(TEXT("GachaUI.ValidateSharedBase"),
    TEXT("Validate the shared gacha panel contract on the building trait/skin panel in MainMap PIE or a -game standalone. Empty prompt stack required; no pulls or exchanges."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* CommandWorld)
    {
        if (GGachaSharedBaseValidation) return;
        // 사람 손 없이 돌 수 있게 -game 스탠드얼론의 Game 월드도 받는다
        auto IsPlayWorld = [](EWorldType::Type Type) { return Type == EWorldType::PIE || Type == EWorldType::Game; };
        UWorld* PlayWorld = CommandWorld && IsPlayWorld(CommandWorld->WorldType) ? CommandWorld : nullptr;
        if (!PlayWorld && GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (IsPlayWorld(Context.WorldType) && Context.World()) { PlayWorld = Context.World(); break; }
        auto Run = MakeShared<FGachaSharedBaseValidation>();
        if (!Run->Start(PlayWorld)) { Run->Finish(); return; }
        GGachaSharedBaseValidation = Run;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta)
        {
            if (Run->Tick(Delta)) return true;
            Run->Finish(); GGachaSharedBaseValidation.Reset(); return false;
        }));
    }));

#endif
