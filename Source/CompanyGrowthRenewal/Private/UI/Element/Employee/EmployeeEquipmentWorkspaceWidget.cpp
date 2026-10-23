#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/ResourceItemManager.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "Components/ComboBoxString.h"
#include "Components/TileView.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "UI/Gacha/GachaCaptureStage.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Panel/GachaRevealPresentationWidget.h"
#include "UI/Panel/EquipmentGachaPanelWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UIBase.h"

#define LOCTEXT_NAMESPACE "EmployeeEquipmentUI"

namespace
{
	// 부위 칸 글리프. 이름은 EEmployeeEquipmentSlot 순서(Head..Back)를 그대로 따른다.
	// ⚠ 경로를 C++ 문자열로 조립하므로 쿠커가 못 따라간다 — 상위 UIIcon 폴더가 재귀 등록돼 있어 덮인다.
	UTexture2D* GetEquipmentPartIcon(EEmployeeEquipmentSlot Slot)
	{
		static const TCHAR* Names[] = {TEXT("Head"), TEXT("Eyewear"), TEXT("Outfit"), TEXT("Hands"), TEXT("Feet"), TEXT("Back")};
		const int32 Index = static_cast<int32>(Slot);
		if (Index < 0 || Index >= UE_ARRAY_COUNT(Names)) return nullptr;
		const FString Path = FString::Printf(
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/EquipmentPart/T_UIIcon_EquipPart_%s.T_UIIcon_EquipPart_%s"),
			Names[Index], Names[Index]);
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous();
	}

	FText Text(const FString& Value) { return FText::FromString(Value); }
	// 서식은 CGEquipmentDetail 하나뿐이다 — 여기 사본이 있으면 유니티 블롭에서 재정의로 터진다.
	using CGEquipmentDetail::FractionDigits;
	using CGEquipmentDetail::DisplayValue;
	using CGEquipmentDetail::FormatEffectValue;
	FString EffectLines(const FEquipmentEffectLoadout& Effects, const UTableManagerSubsystem* Tables, bool bCompact = false)
	{
		TArray<FName> Keys;
		Effects.BasicValues.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		FString Out;
		for (FName Key : Keys)
		{
			const FEmployeeEquipmentEffectRow* Row = Tables->FindEmployeeEquipmentEffect(Key);
			if (Row) Out += FString::Printf(TEXT("%s %s\n"), *Row->DisplayName.ToString(), *FormatEffectValue(*Row, Effects.BasicValues[Key]));
		}
		Effects.UniqueEffects.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		for (FName Key : Keys)
		{
			const FEmployeeEquipmentEffectRow& Row = Effects.UniqueEffects[Key];
			Out += Row.DisplayName.ToString() + (bCompact ? TEXT("\n") : TEXT("\n") + Row.Description.ToString() + TEXT("\n\n"));
		}
		return Out.TrimEnd();
	}
}

void UEmployeeEquipmentWorkspaceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bPresentationInitialized = false;
	EquipmentManager = GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
	TableManager = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (EquipmentManager) EquipmentManager->OnEquipmentChanged.AddUObject(this, &ThisClass::HandleEquipmentChanged);
	if (auto* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>()) Items->OnItemChanged.AddUObject(this, &ThisClass::HandleTicketItemsChanged);
	TicketButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TicketButtonGroup->SetSelectionRequired(true);
	UGachaBannerButton* TicketButtons[] = {TicketNormalButton, TicketAdvancedButton, TicketPremiumButton};
	for (auto* Button : TicketButtons) if (Button) { Button->SetIsSelectable(true); TicketButtonGroup->AddWidget(Button); }
	TicketButtonGroup->OnButtonBaseClicked.AddDynamic(this, &ThisClass::HandleTicketSelected);
	TicketButtonGroup->SelectButtonAtIndex(CurrentTicketType == EItemType::EmployeeEquipmentTicketPremium ? 2 : CurrentTicketType == EItemType::EmployeeEquipmentTicketAdvanced ? 1 : 0);
	if (UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>())
		Employees->OnEmployeePortraitUpdated.AddUObject(this, &ThisClass::HandlePortraitChanged);
	if (InventoryView)
	{
		InventoryView->OnItemClicked().AddUObject(this, &ThisClass::HandleItemClicked);
		InventoryView->OnEntryWidgetGenerated().AddUObject(this, &ThisClass::HandleInventoryEntryGenerated);
	}
	if (EmployeeCombo) EmployeeCombo->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleEmployeeSelected);
	if (SlotFilterCombo) SlotFilterCombo->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleSlotSelected);
	if (GradeFilterCombo) GradeFilterCombo->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGradeSelected);
	if (SlotFilterCombo) SlotFilterCombo->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GenerateEquipmentFilterLabel);
	if (GradeFilterCombo) GradeFilterCombo->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GenerateEquipmentFilterLabel);
	const TPair<UButtonWidget*, void (ThisClass::*)()> Actions[] = {
		{GachaButton, &ThisClass::HandleGacha},
		{ShopButton, &ThisClass::HandleShop},
		{GachaShopButton, &ThisClass::HandleShop},
		{BulkButton, &ThisClass::ShowBulk}, {CurrentItemButton, &ThisClass::HandleCurrentItem},
		{EffectsButton, &ThisClass::ShowEffects}, {EnhanceButton, &ThisClass::HandleEquippedEnhance},
		{PullOneButton, &ThisClass::HandlePullOne}, {PullTenButton, &ThisClass::HandlePullTen},
		{PullThirtyButton, &ThisClass::HandlePullThirty},
		{InspectorEquipButton, &ThisClass::HandleInspectorEquip}, {InspectorEnhanceButton, &ThisClass::HandleInspectorEnhance},
		{InspectorDetailButton, &ThisClass::HandleInspectorDetail},
		{SortCycleButton, &ThisClass::OnSortCycleClicked},
		{OddsButton, &ThisClass::ShowOdds}};
	for (const auto& Action : Actions) if (Action.Key) Action.Key->OnClicked().AddUObject(this, Action.Value);
	UButtonWidget* Slots[] = {SlotButton0, SlotButton1, SlotButton2, SlotButton3, SlotButton4, SlotButton5};
	SlotButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	SlotButtonGroup->SetSelectionRequired(false);
	for (UButtonWidget* Button : Slots)
	{
		if (!Button) continue;
		Button->SetIsSelectable(true);
		Button->SetIsToggleable(false);
		SlotButtonGroup->AddWidget(Button);
	}
	SlotButtonGroup->OnButtonBaseClicked.AddDynamic(this, &ThisClass::HandleSlotButtonSelected);
	if (bAllSlots) SlotButtonGroup->DeselectAll();
	else SlotButtonGroup->SelectButtonAtIndex(static_cast<int32>(CurrentSlot));
	SlotIconMaterials.SetNum(UE_ARRAY_COUNT(Slots));

	// 드로어 부위 레일 — 전체가 0, 부위가 1..6. 선택 필수라 '아무것도 안 고른' 상태가 없다.
	UButtonWidget* PartRail[] = {PartFilterAll, PartFilter0, PartFilter1, PartFilter2, PartFilter3, PartFilter4, PartFilter5};
	// 그룹 인덱스가 곧 부위다(0=전체, 1..6=슬롯). 하나라도 비면 인덱스가 밀려 엉뚱한 부위가 걸리므로
	// 레일은 7칸이 다 있을 때만 세운다 — WBP 저작 전에는 드롭다운이 그대로 필터 역할을 한다.
	bool bRailComplete = true;
	for (UButtonWidget* Button : PartRail) if (!Button) bRailComplete = false;
	if (bRailComplete)
	{
		PartFilterGroup = NewObject<UCommonButtonGroupBase>(this);
		PartFilterGroup->SetSelectionRequired(true);
		for (UButtonWidget* Button : PartRail)
		{
			Button->SetIsSelectable(true);
			Button->SetIsToggleable(false);
			PartFilterGroup->AddWidget(Button);
		}
		PartFilterGroup->OnButtonBaseClicked.AddDynamic(this, &ThisClass::HandlePartFilterSelected);
	}

	SetupFilters();
	UpdateSortLabel();
	Refresh();
}

void UEmployeeEquipmentWorkspaceWidget::OnSortCycleClicked()
{
	switch (SortMode)
	{
	case EEquipmentSortMode::Grade:   SortMode = EEquipmentSortMode::Enhance; break;
	case EEquipmentSortMode::Enhance: SortMode = EEquipmentSortMode::Name;    break;
	default:                          SortMode = EEquipmentSortMode::Grade;   break;
	}
	UpdateSortLabel();
	RefreshInventory();
}

void UEmployeeEquipmentWorkspaceWidget::UpdateSortLabel()
{
	if (!SortCycleButton) return;
	FText Label;
	switch (SortMode)
	{
	case EEquipmentSortMode::Enhance: Label = LOCTEXT("SortEnhance", "강화순"); break;
	case EEquipmentSortMode::Name:    Label = LOCTEXT("SortName", "이름순");    break;
	default:                          Label = LOCTEXT("SortGrade", "등급순");   break;
	}
	// ▾(U+25BE)는 NEXON cmap 에 없어 빈 사각형이 된다 — 수록된 하향 삼각은 ▼/▽ 뿐이다.
	SortCycleButton->SetButtonText(FText::Format(LOCTEXT("SortLabel", "{0} ▼"), Label));
}

void UEmployeeEquipmentWorkspaceWidget::NativeDestruct()
{
	SuspendPreview();
	DrawerPortraitTarget.Reset();
	ClearInspectionSelection(false);
	DismissDialog();
	if (EquipmentManager) EquipmentManager->OnEquipmentChanged.RemoveAll(this);
	if (auto* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>()) Items->OnItemChanged.RemoveAll(this);
	if (TicketButtonGroup) TicketButtonGroup->OnButtonBaseClicked.RemoveAll(this);
	if (SlotButtonGroup)
	{
		SlotButtonGroup->OnButtonBaseClicked.RemoveAll(this);
		SlotButtonGroup->RemoveAll();
		SlotButtonGroup = nullptr;
	}
	if (UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>()) Employees->OnEmployeePortraitUpdated.RemoveAll(this);
	PendingSelectionPulseId.Invalidate();
	if (InventoryView)
	{
		InventoryView->OnItemClicked().RemoveAll(this);
		InventoryView->OnEntryWidgetGenerated().RemoveAll(this);
	}
	if (EmployeeCombo) EmployeeCombo->OnSelectionChanged.RemoveAll(this);
	DismissDetailWindow();
	if (PartFilterGroup)
	{
		PartFilterGroup->OnButtonBaseClicked.RemoveAll(this);
		PartFilterGroup->SetSelectionRequired(false);
		PartFilterGroup->RemoveAll();
		PartFilterGroup = nullptr;
	}
	if (SlotFilterCombo) SlotFilterCombo->OnSelectionChanged.RemoveAll(this);
	if (GradeFilterCombo) GradeFilterCombo->OnSelectionChanged.RemoveAll(this);
	if (SlotFilterCombo) SlotFilterCombo->OnGenerateWidgetEvent.Unbind();
	if (GradeFilterCombo) GradeFilterCombo->OnGenerateWidgetEvent.Unbind();
	UButtonWidget* Buttons[] = {GachaButton, ShopButton, GachaShopButton, BulkButton, CurrentItemButton,
		EffectsButton, EnhanceButton, PullOneButton, PullTenButton, PullThirtyButton, OddsButton,
		InspectorEquipButton, InspectorEnhanceButton, InspectorDetailButton,
		SlotButton0, SlotButton1, SlotButton2, SlotButton3, SlotButton4, SlotButton5};
	for (UButtonWidget* Button : Buttons) if (Button) Button->OnClicked().RemoveAll(this);
	UButtonWidget* Slots[] = {SlotButton0, SlotButton1, SlotButton2, SlotButton3, SlotButton4, SlotButton5};
	for (UButtonWidget* Button : Slots)
		if (auto* IconButton = Cast<UIconWithButtonWidget>(Button))
		{
			IconButton->SetIconResource(nullptr);
			CGEquipmentIcon::Apply(Cast<UImage>(IconButton->GetWidgetFromName(TEXT("IconImage"))), nullptr);
			CGEquipmentIcon::ApplyGradeFrame(Cast<UImage>(IconButton->GetWidgetFromName(TEXT("GradeFrame"))), nullptr, FLinearColor::Transparent);
		}
	SlotIconMaterials.Reset();
	CGEquipmentIcon::Apply(GachaImage, nullptr);
	GachaIconMaterial = nullptr;
	CGEquipmentIcon::Apply(InspectorImage, nullptr);
	CGEquipmentIcon::ApplyGradeFrame(InspectorGradeFrame, nullptr, FLinearColor::Transparent);
	InspectorIconMaterial = nullptr;
	Super::NativeDestruct();
}

void UEmployeeEquipmentWorkspaceWidget::Configure(int32 EmployeeId, EEquipmentWorkspaceView View)
{
	if (CurrentEmployeeId != EmployeeId || CurrentView != View)
	{
		ClearInspectionSelection();
		DismissDialog();
	}
	CurrentEmployeeId = EmployeeId;
	CurrentView = View;
	EmployeeReturnView = EEquipmentWorkspaceView::Employee;
	bPreviewEnabled = true;
	bReturningFromShop = false;
	EquipmentManager = GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
	TableManager = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	SetupFilters();
	Refresh();
}

void UEmployeeEquipmentWorkspaceWidget::ConfigureForEmployee(int32 EmployeeId, EEquipmentWorkspaceView View)
{
	bGachaHost = false;
	bEmployeeContextLocked = true;
	Configure(EmployeeId, View);
}

void UEmployeeEquipmentWorkspaceWidget::SetDrawerPresentation(UImage* PortraitTarget)
{
	DrawerPortraitTarget = PortraitTarget;
	// 상세 창은 뷰포트가 아니라 호스트 트리에 붙인다 — 캡처 하네스는 호스트 위젯을 오프스크린
	// 렌더하므로 뷰포트에 붙이면 안 찍히고, z-order 도 직원창 레이어 밖으로 새어 나간다.
	DetailWindowHost = nullptr;
	if (PortraitTarget)
	{
		if (const UUserWidget* Host = PortraitTarget->GetTypedOuter<UUserWidget>())
		{
			DetailWindowHost = Cast<UPanelWidget>(Host->GetRootWidget());
		}
	}
	if (!DetailWindowHost.IsValid()) DismissDetailWindow();
}

UImage* UEmployeeEquipmentWorkspaceWidget::GetPreviewImage() const
{
	return DrawerPortraitTarget.IsValid() ? DrawerPortraitTarget.Get() : EmployeePortrait.Get();
}

UWidget* UEmployeeEquipmentWorkspaceWidget::GenerateEquipmentFilterLabel(FString Option)
{
	if (!WidgetTree || !SlotFilterCombo) return nullptr;
	UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
	FSlateFontInfo LabelFont = SlotFilterCombo->GetFont();
	// 드로어 칩이 크림이던 시절의 차콜 26 분기를 제거했다 — 칩이 다크 면으로 바뀌면서 글자가 묻혔다.
	// 두 프레젠테이션 모두 카탈로그 §11 의 34 + 콤보 인스턴스가 들고 있는 밝은 잉크를 쓴다.
	LabelFont.Size = 34;
	Label->SetFont(LabelFont);
	Label->SetColorAndOpacity(SlotFilterCombo->GetForegroundColor());
	Label->SetText(FText::FromString(Option));
	return Label;
}

void UEmployeeEquipmentWorkspaceWidget::RefreshPresentation()
{
	const bool bCompact = IsDrawerPresentation();
	if (bPresentationInitialized && bCompactPresentation == bCompact) return;
	bPresentationInitialized = true;
	bCompactPresentation = bCompact;
	const FSlateColor Ink(FLinearColor(.0252f, .0369f, .0612f, 1.f));
	// 툴바 3종(효과·뽑기·상점)은 여기서 빠졌다 — 표현을 WBP 인스턴스가 들고, 드로어/일반 모두
	// 카탈로그 §11 의 같은 규격(높이 154 · 글자 34)을 쓴다. 드로어만 72/26 으로 깎던 분기가 원인이었다.
	UButtonWidget* PresentationButtons[] = {
		InspectorEquipButton, InspectorEnhanceButton, InspectorDetailButton};
	if (NormalPresentationStyles.IsEmpty())
	{
		for (UButtonWidget* Button : PresentationButtons)
		{
			NormalPresentationStyles.Add(Button && Button->GetStyle() ? Button->GetStyle()->GetClass() : nullptr);
			const UCommonTextBlock* Label = Button ? Cast<UCommonTextBlock>(Button->GetWidgetFromName(TEXT("ButtonText"))) : nullptr;
			FSlateFontInfo NormalFont = Label ? Label->GetFont() : FSlateFontInfo();
			if (Button)
			{
				if (Button->GetTextSize() > 0.f) NormalFont.Size = Button->GetTextSize();
				NormalFont.OutlineSettings.OutlineSize = Button->GetTextOutlineEnabled() ? Button->GetTextOutlineSize() : 0.f;
				NormalFont.OutlineSettings.OutlineColor = Button->GetTextOutlineColor();
			}
			NormalPresentationFonts.Add(NormalFont);
			NormalPresentationColors.Add(Button ? Button->GetTextColor() : FSlateColor(FLinearColor::White));
		}
	}
	// 필터 칩 면·글자는 WBP 인스턴스가 들고 있다. SetWidgetStyle 은 이미 만들어진 Slate 콤보를
	// 갱신하지 않아 런타임 주입은 CDO 만 바꾸고 화면은 그대로였다(칩 글자 실종의 1차 원인).
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PresentationButtons); ++Index)
	{
		UButtonWidget* Button = PresentationButtons[Index];
		if (!Button) continue;
		// 0=InspectorEquip 1=InspectorEnhance 2=InspectorDetail — 툴바 3종이 빠지며 한 칸씩 당겨졌다.
		const TSubclassOf<UCommonButtonStyle> CompactStyle = Index == 1 ? DrawerEquipStyle : DrawerUtilityStyle;
		if (bCompact && CompactStyle) Button->SetStyle(CompactStyle);
		else if (!bCompact && NormalPresentationStyles.IsValidIndex(Index)) Button->SetStyle(NormalPresentationStyles[Index]);
		const int32 Width = Index == 0 && !bCompact ? 228 : 0;
		Button->SetMinDimensions(Width, bCompact ? (Index == 0 ? 72 : 96) : 154);
		Button->SetTextSize(bCompact ? (Index == 0 ? 26 : 28) : NormalPresentationFonts[Index].Size);
		// 카탈로그 §6: 컬러 CTA(골드 장착) 라벨만 흰색, 나머지 회색 유틸은 잉크 그대로
		const bool bColoredCta = bCompact && Index == 1;
		Button->SetTextColor(bColoredCta ? FSlateColor(FLinearColor::White) : (bCompact ? Ink : NormalPresentationColors[Index]));
		Button->SetTextOutlineEnabled(false);
	}
	if (InventoryView)
	{
		// 엔진 열 수 = floor((뷰 + 간격) / (엔트리 + 간격)). 드로어 실측 뷰 = 688, 간격 12 →
		// 4열이 되려면 엔트리 ≤ 163 이다(176 이면 3열로 떨어진다 — 실측 2026-09-24). 160 은 터치 하한 141 초과.
		const float EntrySide = bCompact ? 160.f : 256.f;
		InventoryView->SetEntryWidth(EntrySide);
		InventoryView->SetEntryHeight(EntrySide);
	}
	for (const TCHAR* FilterName : {TEXT("SlotFilterComboSize"), TEXT("GradeFilterComboSize")})
		if (USizeBox* FilterSize = Cast<USizeBox>(GetWidgetFromName(FilterName)))
		{
			// 드로어 칩도 터치 하한 141 을 지킨다 — 72 는 폰에서 3.6mm 라 규격 미달이었다.
			// 폭 250: 칩 둘(500) + 간격 24 를 빼고도 개수 라벨이 한 줄로 들어간다(내용 폭 688).
			FilterSize->SetWidthOverride(bCompact ? 250.f : 300.f);
			FilterSize->SetHeightOverride(bCompact ? 141.f : 154.f);
		}
	if (UWidget* InventoryContent = GetWidgetFromName(TEXT("InventoryVBox")))
		if (UOverlaySlot* ContentSlot = Cast<UOverlaySlot>(InventoryContent->Slot)) ContentSlot->SetPadding(FMargin(bCompact ? 0.f : 16.f));
	// 드로어에서도 밝은 내용판을 쓴다 — 이 판이 강화 모달과 같은 면을 만든다(결정 §6.41).
	if (UWidget* InventoryBackground = GetWidgetFromName(TEXT("InventoryDecor")))
		InventoryBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto SetFontSize = [](UCommonTextBlock* Label, int32 Size)
	{
		if (!Label) return;
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = Size;
		Label->SetFont(Font);
	};
	SetFontSize(InventoryStatusText, bCompact ? 28 : 34);
	// 드로어의 개수는 밝은 크림 판 위에 앉는다 — 창과 공유하는 밝은 본문 잉크(#ECEEF0)를 쓰면 판에 묻혀 사라진다.
	if (InventoryStatusText) InventoryStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(0x5A, 0x6B, 0x7D))));
	UOverlay* BottomHost = Cast<UOverlay>(GetWidgetFromName(TEXT("DrawerInspectorHost")));
	UOverlay* NormalHost = Cast<UOverlay>(GetWidgetFromName(TEXT("InventoryColumnRoot")));
	UOverlay* DetailHost = Cast<UOverlay>(GetWidgetFromName(TEXT("InspectorDetailLinkHost")));
	UHorizontalBox* SecondaryActions = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("InspectorSecondaryActions")));
	UVerticalBox* Actions = Cast<UVerticalBox>(GetWidgetFromName(TEXT("InspectorActions")));
	UOverlay* ExpectedHost = bCompact ? BottomHost : NormalHost;
	if (InspectorPane && ExpectedHost && InspectorPane->GetParent() != ExpectedHost)
	{
		InspectorPane->RemoveFromParent();
		UOverlaySlot* PaneSlot = ExpectedHost->AddChildToOverlay(InspectorPane);
		PaneSlot->SetHorizontalAlignment(bCompact ? HAlign_Fill : HAlign_Right);
		PaneSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (USizeBox* InspectorSize = Cast<USizeBox>(InspectorPane))
	{
		if (bCompact) { InspectorSize->ClearWidthOverride(); InspectorSize->SetHeightOverride(300.f); }
		else { InspectorSize->SetWidthOverride(592.f); InspectorSize->ClearHeightOverride(); }
	}
	if (BottomHost && !bCompact) BottomHost->SetVisibility(ESlateVisibility::Collapsed);
	if (InspectorDetailButton && DetailHost && SecondaryActions)
	{
		UPanelWidget* DetailParent = bCompact ? static_cast<UPanelWidget*>(DetailHost) : SecondaryActions;
		if (InspectorDetailButton->GetParent() != DetailParent)
		{
			InspectorDetailButton->RemoveFromParent();
			if (bCompact)
			{
				UOverlaySlot* DetailSlot = DetailHost->AddChildToOverlay(InspectorDetailButton);
				DetailSlot->SetHorizontalAlignment(HAlign_Fill); DetailSlot->SetVerticalAlignment(VAlign_Fill);
			}
			else
			{
				UHorizontalBoxSlot* DetailSlot = SecondaryActions->AddChildToHorizontalBox(InspectorDetailButton);
				DetailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); DetailSlot->SetPadding(FMargin(4.f, 0.f));
			}
		}
		DetailHost->SetVisibility(bCompact ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (InspectorEquipButton && SecondaryActions && Actions)
	{
		UPanelWidget* EquipParent = bCompact ? static_cast<UPanelWidget*>(SecondaryActions) : Actions;
		if (InspectorEquipButton->GetParent() != EquipParent)
		{
			InspectorEquipButton->RemoveFromParent();
			if (bCompact)
			{
				UHorizontalBoxSlot* EquipSlot = SecondaryActions->AddChildToHorizontalBox(InspectorEquipButton);
				EquipSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); EquipSlot->SetPadding(FMargin(4.f, 0.f));
			}
			else Actions->AddChildToVerticalBox(InspectorEquipButton)->SetPadding(FMargin(4.f, 0.f));
		}
	}
	if (UWidget* Header = GetWidgetFromName(TEXT("InspectorHeader"))) Header->SetVisibility(bCompact ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (UWidget* InspectorContentBox = GetWidgetFromName(TEXT("InspectorVBox")))
		if (UOverlaySlot* ContentSlot = Cast<UOverlaySlot>(InspectorContentBox->Slot)) ContentSlot->SetPadding(bCompact ? FMargin(12.f, 6.f) : FMargin(24.f, 20.f));
	auto SetVBoxPadding = [](UWidget* Widget, const FMargin& ContentPadding)
	{
		if (Widget) if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot)) BoxSlot->SetPadding(ContentPadding);
	};
	SetVBoxPadding(PreviewLabelText, FMargin(0.f, 0.f, 0.f, bCompact ? 6.f : 12.f));
	SetVBoxPadding(GetWidgetFromName(TEXT("InspectorItemHeader")), FMargin(0.f, 0.f, 0.f, bCompact ? 6.f : 24.f));
	SetVBoxPadding(InspectorGradeText, FMargin(0.f, bCompact ? 4.f : 12.f, 0.f, 0.f));
	SetVBoxPadding(Actions, FMargin(0.f, bCompact ? 6.f : 20.f, 0.f, 0.f));
	SetVBoxPadding(SecondaryActions, FMargin(0.f, 0.f, 0.f, bCompact ? 0.f : 12.f));
	if (USizeBox* IconSize = Cast<USizeBox>(GetWidgetFromName(TEXT("InspectorIconSize"))))
	{
		IconSize->SetWidthOverride(bCompact ? 88.f : 176.f); IconSize->SetHeightOverride(bCompact ? 88.f : 176.f);
	}
	if (UWidget* Labels = GetWidgetFromName(TEXT("InspectorItemLabels")))
		if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Labels->Slot)) LabelSlot->SetPadding(FMargin(bCompact ? 12.f : 24.f, 0.f, 0.f, 0.f));
	SetFontSize(PreviewLabelText, bCompact ? 22 : 34);
	SetFontSize(InspectorNameText, bCompact ? 32 : 38);
	SetFontSize(InspectorGradeText, bCompact ? 24 : 34);
	SetFontSize(InspectorBodyText, bCompact ? 26 : 34);
	for (const TCHAR* BackgroundName : {TEXT("InspectorBackground"), TEXT("InspectorGrain")})
		if (UWidget* Background = GetWidgetFromName(BackgroundName)) Background->SetVisibility(bCompact ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (UWidget* InsetBackground = GetWidgetFromName(TEXT("InspectorInsetBackground")))
		InsetBackground->SetVisibility(bCompact ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UEmployeeEquipmentWorkspaceWidget::ConfigureGachaHost(int32 EmployeeId)
{
	SetDrawerPresentation(nullptr);
	bGachaHost = true;
	bEmployeeContextLocked = true;
	Configure(EmployeeId, EEquipmentWorkspaceView::Gacha);
}

void UEmployeeEquipmentWorkspaceWidget::OpenEquipmentItem(FGuid InstanceId)
{
	if (!EquipmentManager || !TableManager) return;
	if (IsInspectorActive()) SelectInspectionItem(InstanceId);
	else ShowDetail(InstanceId);
}

void UEmployeeEquipmentWorkspaceWidget::SetupFilters()
{
	if (!TableManager) return;
	TGuardValue<bool> Updating(bUpdating, true);
	EmployeeOptionIds.Empty();
	if (EmployeeCombo) EmployeeCombo->ClearOptions();
	if (UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>())
	{
		TArray<FEmployeeInstance> List = Employees->GetAllEmployees();
		List.Sort([](const auto& A, const auto& B) { return A.EmployeeID < B.EmployeeID; });
		for (const auto& Employee : List)
		{
			EmployeeOptionIds.Add(Employee.EmployeeID);
			if (EmployeeCombo) EmployeeCombo->AddOption(FString::Printf(TEXT("%s · #%d"), *Employees->GetEmployeeFullName(Employee.EmployeeID), Employee.EmployeeID));
		}
	}
	const int32 PreviousEmployeeId = CurrentEmployeeId;
	if (EmployeeOptionIds.IsEmpty()) CurrentEmployeeId = INDEX_NONE;
	else if (!EmployeeOptionIds.Contains(CurrentEmployeeId)) CurrentEmployeeId = bEmployeeContextLocked ? INDEX_NONE : EmployeeOptionIds[0];
	if (PreviousEmployeeId != CurrentEmployeeId) ClearInspectionSelection();
	const int32 SelectedIndex = EmployeeOptionIds.Find(CurrentEmployeeId);
	if (EmployeeCombo && SelectedIndex >= 0) EmployeeCombo->SetSelectedIndex(SelectedIndex);
	// 부위 줄은 목록 판(밝은 크림) 안에 서므로 잉크는 양쪽 다 어둡다 — 선택 신호는 면색(골드)이 지고
	// 잉크는 선택 차콜 / 비선택 중간톤으로만 갈린다. 0 칸만 글자("전체")고 1..6 은 글리프다.
	{
		UButtonWidget* PartRail[] = {PartFilterAll, PartFilter0, PartFilter1, PartFilter2, PartFilter3, PartFilter4, PartFilter5};
		const FLinearColor PartInkPicked = FLinearColor::FromSRGBColor(FColor(0x2C, 0x34, 0x44));
		const FLinearColor PartInkIdle = FLinearColor::FromSRGBColor(FColor(0x7B, 0x87, 0x98));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(PartRail); ++Index)
		{
			UButtonWidget* Button = PartRail[Index];
			if (!Button) continue;
			const bool bPicked = Index == 0 ? bAllSlots : (!bAllSlots && static_cast<int32>(CurrentSlot) == Index - 1);
			const FLinearColor Ink = bPicked ? PartInkPicked : PartInkIdle;
			Button->SetTextColor(FSlateColor(Ink));
			// 탭은 아이콘 위 · 라벨 아래를 같이 보여 준다 — 글리프만으로는 부위를 못 읽는다.
			UIconWithButtonWidget* Chip = Cast<UIconWithButtonWidget>(Button);
			if (Index == 0)
			{
				Button->SetButtonText(LOCTEXT("PartFilterAll", "전체"));
				// [전체] 는 글리프가 없다 — 라벨 하나가 칸을 채우도록 아이콘을 접는다.
				if (Chip) Chip->SetIconVisibility(ESlateVisibility::Collapsed);
				continue;
			}
			const auto* SlotRow = TableManager->FindEmployeeEquipmentSlot(static_cast<EEmployeeEquipmentSlot>(Index - 1));
			Button->SetButtonText(SlotRow ? SlotRow->DisplayName : FText());
			if (!Chip) continue;
			Chip->SetIcon(GetEquipmentPartIcon(static_cast<EEmployeeEquipmentSlot>(Index - 1)));
			Chip->SetIconColor(Ink);
			Chip->SetIconVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	SyncPartFilterSelection();
	if (SlotFilterCombo)
	{
		SlotFilterCombo->ClearOptions();
		SlotFilterCombo->AddOption(TEXT("모든 부위"));
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const auto* Row = TableManager->FindEmployeeEquipmentSlot(static_cast<EEmployeeEquipmentSlot>(Index));
			SlotFilterCombo->AddOption(Row ? Row->DisplayName.ToString() : FString());
		}
		SlotFilterCombo->SetSelectedIndex(bAllSlots ? 0 : static_cast<int32>(CurrentSlot) + 1);
	}
	if (GradeFilterCombo)
	{
		GradeFilterCombo->ClearOptions();
		GradeFilterCombo->AddOption(TEXT("모든 등급"));
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const auto* Row = TableManager->FindEmployeeEquipmentGrade(static_cast<EEmployeeEquipmentGrade>(Index));
			GradeFilterCombo->AddOption(Row ? Row->DisplayName.ToString() : FString());
		}
		GradeFilterCombo->SetSelectedIndex(GradeFilter == EEmployeeEquipmentGrade::Count ? 0 : static_cast<int32>(GradeFilter) + 1);
	}
	SetupBanners();
}

void UEmployeeEquipmentWorkspaceWidget::RefreshPortrait()
{
	UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (EmployeeNameText) EmployeeNameText->SetText(Employees ? Text(Employees->GetEmployeeFullName(CurrentEmployeeId)) : FText());
	UImage* PortraitImage = GetPreviewImage();
	if (!bPreviewEnabled || !Employees || !Employees->GetEmployeeData(CurrentEmployeeId)
		|| CurrentView != EEquipmentWorkspaceView::Employee)
	{
		if (PreviewStage) PreviewStage->ReleaseShared(this);
		PreviewEmployeeId = INDEX_NONE;
		PreviewWarmup = -1.f;
		if (PortraitImage)
		{
			PortraitImage->SetRenderOpacity(1.f);
			PortraitImage->SetBrushFromTexture(UEmployeeRosterCardWidget::LoadPortraitTexture(CurrentEmployeeId, FVector2D(360.f, 450.f)));
		}
		return;
	}
	AcquirePreview();
	if (PortraitImage)
	{
		if (PreviewStage && PreviewStage->IsHeldBy(this) && PreviewMID)
		{
			PortraitImage->SetBrushFromMaterial(PreviewMID);
			PortraitImage->SetRenderOpacity(PreviewWarmup >= 0.f ? FMath::Clamp((PreviewWarmup - .15f) / .3f, 0.f, 1.f) : 1.f);
		}
		else
		{
			PortraitImage->SetBrushFromTexture(UEmployeeRosterCardWidget::LoadPortraitTexture(CurrentEmployeeId, FVector2D(360.f, 450.f)));
			PortraitImage->SetRenderOpacity(1.f);
		}
		PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UEmployeeEquipmentWorkspaceWidget::AcquirePreview()
{
	UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const auto* Employee = Employees ? Employees->GetEmployeeData(CurrentEmployeeId) : nullptr;
	if (!Employee || !EquipmentManager || PreviewStageClass.IsNull()) return;
	UTextureRenderTarget2D* Target = nullptr;
	if (!PreviewStage || !PreviewStage->IsHeldBy(this) || PreviewEmployeeId != CurrentEmployeeId || PreviewRevision != EquipmentManager->GetState().Revision)
	{
		PreviewStage = AGachaCaptureStage::AcquireShared(this, GetWorld(), PreviewStageClass.LoadSynchronous());
		if (PreviewStage) Target = PreviewStage->BeginReveal(*Employee, EStageAnimMode::Idle);
		if (Target)
		{
			if (!PreviewMID && !PreviewMaterial.IsNull()) PreviewMID = UMaterialInstanceDynamic::Create(PreviewMaterial.LoadSynchronous(), this);
			if (PreviewMID) PreviewMID->SetTextureParameterValue(TEXT("RT"), Target);
			PreviewEmployeeId = CurrentEmployeeId; PreviewRevision = EquipmentManager->GetState().Revision; PreviewWarmup = 0.f;
		}
	}
}

void UEmployeeEquipmentWorkspaceWidget::SuspendPreview()
{
	if (PreviewStage && PreviewStage->IsHeldBy(this))
	{
		PreviewStage->ClearEquipmentAppearancePreview();
		PreviewStage->ReleaseShared(this);
	}
	PreviewEmployeeId = INDEX_NONE; PreviewRevision = -1; bPreviewEnabled = false; PreviewWarmup = -1.f;
}

void UEmployeeEquipmentWorkspaceWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (PreviewWarmup >= 0.f)
	{
		PreviewWarmup += DeltaTime;
		if (UImage* PortraitImage = GetPreviewImage()) PortraitImage->SetRenderOpacity(FMath::Clamp((PreviewWarmup - .15f) / .3f, 0.f, 1.f));
		if (PreviewWarmup >= .45f) PreviewWarmup = -1.f;
	}
	UpdateFilterChipMaterialSize();
}

namespace
{
	// 칩 크기는 SizeBox 오버라이드가 권위다 — 캐시 지오메트리는 오프스크린 렌더(FWidgetRenderer)에서
	// 0 이 나와 Wpx/Hpx 가 영영 안 들어간다. 박스가 없을 때만 지오메트리로 떨어진다.
	void ApplyEquipmentChipSize(USizeBox* Box, UImage* Face, UImage* Line, FVector2D& Last)
	{
		if (!Face) return;
		const float BoxW = Box ? Box->GetWidthOverride() : 0.f;
		const float BoxH = Box ? Box->GetHeightOverride() : 0.f;
		FVector2D Size = BoxW > 0.f && BoxH > 0.f ? FVector2D(BoxW, BoxH)
			: Face->GetCachedGeometry().GetLocalSize();
		if (Size.X < 1.f || Size.Y < 1.f || Size.Equals(Last, 0.5f)) return;
		Last = Size;
		static const FName WpxParam(TEXT("Wpx"));
		static const FName HpxParam(TEXT("Hpx"));
		for (UImage* Layer : {Face, Line})
		{
			if (!Layer) continue;
			if (UMaterialInstanceDynamic* MID = Layer->GetDynamicMaterial())
			{
				MID->SetScalarParameterValue(WpxParam, Size.X);
				MID->SetScalarParameterValue(HpxParam, Size.Y);
			}
		}
	}
}

void UEmployeeEquipmentWorkspaceWidget::UpdateFilterChipMaterialSize()
{
	ApplyEquipmentChipSize(Cast<USizeBox>(GetWidgetFromName(TEXT("SlotFilterComboSize"))),
		Cast<UImage>(GetWidgetFromName(TEXT("SlotFilterChipBG"))),
		Cast<UImage>(GetWidgetFromName(TEXT("SlotFilterChipLine"))), LastSlotChipSize);
	ApplyEquipmentChipSize(Cast<USizeBox>(GetWidgetFromName(TEXT("GradeFilterComboSize"))),
		Cast<UImage>(GetWidgetFromName(TEXT("GradeFilterChipBG"))),
		Cast<UImage>(GetWidgetFromName(TEXT("GradeFilterChipLine"))), LastGradeChipSize);
}

bool UEmployeeEquipmentWorkspaceWidget::PreviewDefinition(FName DefinitionId)
{
	const auto* Def = TableManager->FindEmployeeEquipmentDefinition(DefinitionId);
	if (!Def) return false;
	AcquirePreview();
	if (!PreviewStage || !PreviewStage->IsHeldBy(this)) return false;
	if (UImage* PortraitImage = GetPreviewImage())
		if (PreviewMID) PortraitImage->SetBrushFromMaterial(PreviewMID);
	const bool bApplied = PreviewStage->PreviewEquipmentAppearance(Def->EquipmentSlot, EEquipmentAppearanceMode::Override, Def->AppearanceId);
	if (bApplied && Dialog) Dialog->SetPreviewMaterial(PreviewMID);
	return bApplied;
}

void UEmployeeEquipmentWorkspaceWidget::Refresh()
{
	if (!EquipmentManager || !TableManager) return;
	const bool bGacha = CurrentView == EEquipmentWorkspaceView::Gacha;
	const bool bDrawer = DrawerPortraitTarget.IsValid();
	const bool bEmployeeView = CurrentView == EEquipmentWorkspaceView::Employee;
	RefreshPresentation();
	// 드로어(직원창 안)에는 하단 유틸 줄을 두지 않는다 — 레퍼런스의 장비 목록 패널에는 그 줄이 없다.
	if (WorkspaceToolbar) WorkspaceToolbar->SetVisibility(bGacha || bDrawer ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (WorkspaceToolbar && WorkspaceTopUtilityHost && WorkspaceBottomUtilityHost)
	{
		UOverlay* UtilityHost = bEmployeeView ? WorkspaceBottomUtilityHost.Get() : WorkspaceTopUtilityHost.Get();
		if (WorkspaceToolbar->GetParent() != UtilityHost)
		{
			WorkspaceToolbar->RemoveFromParent();
			if (UOverlaySlot* UtilitySlot = UtilityHost->AddChildToOverlay(WorkspaceToolbar))
			{
				UtilitySlot->SetHorizontalAlignment(HAlign_Fill);
				UtilitySlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		WorkspaceTopUtilityHost->SetVisibility(bGacha || bEmployeeView ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		// 드로어에서는 호스트까지 접어야 목록판이 바닥까지 내려온다 — 숨긴 툴바가 자리를 남기면 빈 띠가 생긴다.
		WorkspaceBottomUtilityHost->SetVisibility(!bGacha && !bDrawer && bEmployeeView ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (EmployeeSelectorRow) EmployeeSelectorRow->SetVisibility(bGacha || bEmployeeContextLocked ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (EmployeeCombo) EmployeeCombo->SetVisibility(bGacha || bEmployeeContextLocked ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (EmployeeNameText) EmployeeNameText->SetVisibility(bEmployeeView && bEmployeeContextLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	// 드로어에서는 뽑기·상점이 호스트 헤더로 올라간다 — 목록 하단에 같은 버튼을 두 번 두지 않는다.
	for (UButtonWidget* NavButton : {GachaButton.Get(), ShopButton.Get()})
		if (NavButton) NavButton->SetVisibility(bGacha || bDrawer ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (CollectionPane) CollectionPane->SetVisibility(bGacha ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (GachaPane) GachaPane->SetVisibility(bGacha ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (EmployeePane) EmployeePane->SetVisibility(bEmployeeView && !bDrawer ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (EffectsButton) EffectsButton->SetVisibility(bEmployeeView ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	// 보관함 뷰를 없애면서 일괄 분해가 갈 곳이 사라졌다 — 장비 목록 하단이 그 자리다(결정 §12.7 ①).
	if (BulkButton) BulkButton->SetVisibility(bGacha ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (BalanceText)
	{
		BalanceText->SetVisibility(bGacha || bDrawer ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		const auto* Items = GetGameInstance()->GetSubsystem<UItemInventoryManager>();
		BalanceText->SetText(FText::Format(LOCTEXT("CoreBalance", "강화 코어 {0}개"),
			FText::AsNumber(Items ? Items->GetItemCount(EItemType::EmployeeEquipmentCore) : 0)));
	}
	UButtonWidget* Slots[] = {SlotButton0, SlotButton1, SlotButton2, SlotButton3, SlotButton4, SlotButton5};
	SlotIconMaterials.SetNum(UE_ARRAY_COUNT(Slots));
	if (SlotButtonGroup)
	{
		if (bAllSlots) SlotButtonGroup->DeselectAll();
		else SlotButtonGroup->SelectButtonAtIndex(static_cast<int32>(CurrentSlot));
	}
	UEmployeeManager* SlotEmployees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
	{
		const auto Part = static_cast<EEmployeeEquipmentSlot>(Index);
		const auto* Item = EquipmentManager->GetEquippedInstance(CurrentEmployeeId, Part);
		const bool bWearsBase = !Item && SlotEmployees && SlotEmployees->HasBaseAppearance(CurrentEmployeeId, Part);
		CGEquipmentIcon::ApplySlotButton(this, Cast<UIconWithButtonWidget>(Slots[Index]), TableManager, Part, Item, SlotIconMaterials[Index], bWearsBase);
	}
	const auto* Current = EquipmentManager->GetEquippedInstance(CurrentEmployeeId, CurrentSlot);
	if (CurrentItemButton) { CurrentItemButton->SetButtonText(Current ? LOCTEXT("CurrentItemDetail", "상세") : LOCTEXT("EmptySlot", "미장착")); CurrentItemButton->SetIsEnabled(Current != nullptr); }
	if (EnhanceButton) EnhanceButton->SetIsEnabled(Current != nullptr);
	RefreshPortrait();
	if (bGacha) { RefreshGacha(); RefreshInspector(); } else RefreshInventory();
	OnViewChanged.ExecuteIfBound();
}

UEmployeeEquipmentItemObject* UEmployeeEquipmentWorkspaceWidget::MakeEntry(const FEmployeeEquipmentDefinitionRow& Definition, const FEmployeeEquipmentInstance* Instance)
{
	// 조립은 CGEquipmentEntry 가 한다 — 직원창 칼럼과 한 군데를 쓴다.
	return CGEquipmentEntry::Make(this, *TableManager, Definition, Instance);
}

void UEmployeeEquipmentWorkspaceWidget::RefreshInventory()
{
	if (!InventoryView) return;
	const float Scroll = InventoryView->GetScrollOffset();
	Entries.Empty();
	TArray<UObject*> List;
	const auto Accept = [this](const FEmployeeEquipmentDefinitionRow& Def) {
		return (bAllSlots || Def.EquipmentSlot == CurrentSlot) && (GradeFilter == EEmployeeEquipmentGrade::Count || GradeFilter == Def.Grade);
	};
	// 인벤토리 뷰는 계정 전체를 보는 화면이라 예외로 전부 보여준다 ― 남이 착용 중인 것까지.
	// 목록은 소유한 장비 인스턴스 하나뿐이다 — 외형은 별도 목록이 아니라 상세 창의 행동이 됐다.
	for (const auto& Pair : EquipmentManager->GetState().Instances)
	{
		const auto* Def = TableManager->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
		// [장비] 는 보유 장비 전부를 보여준다 — 남이 착용 중인 것도 목록에는 뜨고 구분은 카드/상세가 한다.
		// 한때 남 착용분을 숨겼더니 OpenAllEquipment 가 부위·등급을 다 풀어도 이 필터만 남아 목록이 비었다.
		if (Def && Accept(*Def))
		{
			UEmployeeEquipmentItemObject* Entry = MakeEntry(*Def, &Pair.Value);
			const int32 Wearer = EquipmentManager->GetInstanceEmployee(Pair.Key);
			Entry->bEquipped = Wearer != INDEX_NONE;
			Entry->bWornByOther = Wearer != INDEX_NONE && CurrentEmployeeId != INDEX_NONE && Wearer != CurrentEmployeeId;
			Entries.Add(Entry);
		}
	}
	// 표시 이름은 항목 객체가 아니라 정의 테이블에 있다 — 이름순에서만 들춘다.
	const auto DisplayName = [this](const UEmployeeEquipmentItemObject& Item) -> FString
	{
		const auto* Def = TableManager->FindEmployeeEquipmentDefinition(Item.DefinitionID);
		return Def ? Def->DisplayName.ToString() : Item.DefinitionID.ToString();
	};
	Entries.Sort([this, &DisplayName](const UEmployeeEquipmentItemObject& A, const UEmployeeEquipmentItemObject& B)
	{
		// 어느 정렬이든 먼저 묶는다 ― 이 직원이 낀 것 → 안 낀 것 → 다른 직원이 낀 것
		const auto Group = [](const UEmployeeEquipmentItemObject& Entry) { return !Entry.bEquipped ? 1 : (Entry.bWornByOther ? 2 : 0); };
		if (Group(A) != Group(B)) return Group(A) < Group(B);
		switch (SortMode)
		{
		case EEquipmentSortMode::Grade:
			// EEmployeeEquipmentGrade 는 Common(0) → Ascendant(5) 라 높은 등급이 큰 값이다.
			if (A.Grade != B.Grade) return A.Grade > B.Grade;
			if (A.EnhancementLevel != B.EnhancementLevel) return A.EnhancementLevel > B.EnhancementLevel;
			break;
		case EEquipmentSortMode::Enhance:
			if (A.EnhancementLevel != B.EnhancementLevel) return A.EnhancementLevel > B.EnhancementLevel;
			if (A.Grade != B.Grade) return A.Grade > B.Grade;
			break;
		case EEquipmentSortMode::Name:
		{
			const FString NameA = DisplayName(A), NameB = DisplayName(B);
			if (NameA != NameB) return NameA < NameB;
			break;
		}
		}
		// 어느 모드든 마지막 갈림은 같아야 목록이 매 갱신마다 흔들리지 않는다.
		return A.DefinitionID == B.DefinitionID
			? A.InstanceID.ToString() < B.InstanceID.ToString()
			: A.DefinitionID.LexicalLess(B.DefinitionID);
	});
	for (const auto& Entry : Entries) List.Add(Entry);
	if (PendingSelectionPulseId.IsValid() && !Entries.ContainsByPredicate([this](const TObjectPtr<UEmployeeEquipmentItemObject>& Entry)
		{ return Entry && Entry->InstanceID == PendingSelectionPulseId; })) PendingSelectionPulseId.Invalidate();
	// 보유분만 띄운다 — 빈 칸을 채워 격자를 직사각으로 맞추던 동작은 폐기(2026-09-25).
	// 없는 장비를 자리로 보여 줘 봐야 "안 채운 칸"과 "깨진 칸"을 구분하는 비용만 든다.
	InventoryView->SetListItems(List);
	InventoryView->SetScrollOffset(Scroll);
	if (IsInspectorActive())
	{
		UObject* SelectedEntry = nullptr;
		for (const auto& Entry : Entries) if (Entry->InstanceID == SelectedInstanceId) { SelectedEntry = Entry; break; }
		if (SelectedEntry) InventoryView->SetSelectedItem(SelectedEntry);
		else ClearInspectionSelection();
	}
	RefreshInspector();
	// 개수는 보유분만 센다 — List 는 빈 칸까지 채워 넣은 뒤라 그걸 세면 늘 격자 칸 수가 나온다.
	if (InventoryStatusText) InventoryStatusText->SetText(Entries.IsEmpty() ? LOCTEXT("NoEquipment", "표시할 장비가 없습니다") : FText::Format(LOCTEXT("Count", "{0}개"), FText::AsNumber(Entries.Num())));
	if (InventoryPartLabel)
	{
		const auto* SlotRow = bAllSlots || !TableManager ? nullptr : TableManager->FindEmployeeEquipmentSlot(CurrentSlot);
		InventoryPartLabel->SetText(bAllSlots ? LOCTEXT("PartLabelAll", "모든 부위") : (SlotRow ? SlotRow->DisplayName : FText()));
	}
}

void UEmployeeEquipmentWorkspaceWidget::ShowDialog(const FEmployeeEquipmentDialogView& View)
{
	if (PreviewStage && PreviewStage->IsHeldBy(this)) PreviewStage->ClearEquipmentAppearancePreview();
	if (!Dialog)
	{
		const auto Class = TableManager->GetWidgetClass(EWidgetType::EmployeeEquipmentDialog);
		if (!Class) { UE_LOG(LogTemp, Error, TEXT("[EquipmentUI] EmployeeEquipmentDialog is not registered")); return; }
		Dialog = CreateWidget<UEmployeeEquipmentDialogWidget>(GetOwningPlayer(), Class);
		if (!Dialog) return;
		Dialog->AddToViewport(120);
	}
	Dialog->OnPrimary.Unbind(); Dialog->OnSecondary.Unbind(); Dialog->OnTertiary.Unbind(); Dialog->OnExtra.Unbind();
	Dialog->OnItemClicked.Unbind(); Dialog->OnSelectionChanged.Unbind();
	Dialog->OnClose.BindUObject(this, &ThisClass::HandleDialogBack);
	Dialog->Configure(View);
	Dialog->SetKeyboardFocus();
	bBulkDialog = false;
}

void UEmployeeEquipmentWorkspaceWidget::DismissDialog()
{
	if (UConfirmCancelWidget* Notice = PullNotice.Get())
	{
		if (Notice->IsInViewport())
		{
			if (auto* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>()) UI->CloseCurrentDialog();
		}
	}
	PullNotice.Reset();
	if (RevealPresentation)
	{
		RevealPresentation->OnPresentationClosed.Unbind();
		if (RevealPresentation->IsActivated()) RevealPresentation->DeactivateWidget();
		RevealPresentation->RemoveFromParent();
		RevealPresentation = nullptr;
	}
	if (Dialog) Dialog->RemoveFromParent();
	if (PreviewStage && PreviewStage->IsHeldBy(this)) PreviewStage->ClearEquipmentAppearancePreview();
	Dialog = nullptr;
	if (PreviewStage && CurrentView != EEquipmentWorkspaceView::Employee) { PreviewStage->ReleaseShared(this); PreviewEmployeeId = INDEX_NONE; PreviewWarmup = -1.f; }
	DialogEntries.Empty();
	bReturnToResults = false;
	bBulkDialog = false;
	BulkSelectionIds.Empty();
	if (IsInspectorActive()) RefreshInspector();
}

void UEmployeeEquipmentWorkspaceWidget::HandleDialogBack()
{
	if (bReturnToResults) { bReturnToResults = false; ShowResults(); }
	else DismissDialog();
}

void UEmployeeEquipmentWorkspaceWidget::FillDefinitionView(FEmployeeEquipmentDialogView& View, const FEmployeeEquipmentDefinitionRow& Definition, int32 Level) const
{
	View.SelectedGrade = Definition.Grade;
	View.SelectedVisualGrade = CGEquipmentIcon::VisualGrade(TableManager, Definition);
	const auto* Grade = TableManager->FindEmployeeEquipmentGrade(Definition.Grade);
	const auto* Part = TableManager->FindEmployeeEquipmentSlot(Definition.EquipmentSlot);
	View.SelectedHeader = Text(FString::Printf(TEXT("%s · %s\n%s +%d"), Grade ? *Grade->DisplayName.ToString() : TEXT(""), Part ? *Part->DisplayName.ToString() : TEXT(""), *Definition.DisplayName.ToString(), Level));
	if (!Definition.Icon.IsNull()) View.SelectedIcon = Definition.Icon.LoadSynchronous();
	if (const auto* Economy = TableManager->GetEmployeeEquipmentEconomy())
	{
		const auto ItemEffects = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(Definition, Level, TableManager->GetEmployeeEquipmentEffects(), *Economy);
		View.Effects = Text(EffectLines(ItemEffects, TableManager));
		// 상세 창은 같은 값을 표로 그린다 — Before 를 비워 절대값 행으로 만든다.
		View.EffectRows.Reset();
		TArray<FName> Keys;
		ItemEffects.BasicValues.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		for (FName Key : Keys)
		{
			const auto* Effect = TableManager->FindEmployeeEquipmentEffect(Key);
			if (!Effect) continue;
			FEquipmentDeltaRow& Row = View.EffectRows.AddDefaulted_GetRef();
			Row.Name = Effect->DisplayName;
			Row.After = Text(FormatEffectValue(*Effect, ItemEffects.BasicValues[Key]));
		}
	}
	View.Description = LOCTEXT("EffectTiming", "장비 효과는 다음 업무부터 적용됩니다. 이미 진행 중인 졸음·무단 이탈은 유지됩니다. 외형만 바꾸면 능력치는 그대로입니다.");
}

bool UEmployeeEquipmentWorkspaceWidget::BuildItemDetailView(FGuid InstanceId, FEmployeeEquipmentDialogView& View, bool bChangesOnly) const
{
	if (!EquipmentManager || !TableManager) return false;
	const auto* Item = EquipmentManager->FindInstance(InstanceId);
	const auto* Def = Item ? TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	if (!Def) return false;
	const int32 Holder = EquipmentManager->GetInstanceEmployee(InstanceId);
	const bool bEquipped = Holder == CurrentEmployeeId && Holder >= 0;
	View = FEmployeeEquipmentDialogView();
	View.Title = LOCTEXT("EquipmentDetail", "장비 상세");
	FillDefinitionView(View, *Def, Item->EnhanceLevel);
	if (bChangesOnly) if (const auto* Economy = TableManager->GetEmployeeEquipmentEconomy())
		View.Effects = Text(EffectLines(FEmployeeEquipmentEffectEvaluator::BuildItemEffects(*Def, Item->EnhanceLevel, TableManager->GetEmployeeEquipmentEffects(), *Economy), TableManager, true));
	UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const FString HolderText = Holder >= 0 && Employees && Employees->GetEmployeeData(Holder) ? Employees->GetEmployeeFullName(Holder) + TEXT(" 착용 중") : TEXT("미장착");
	View.SelectedHeader = Text(View.SelectedHeader.ToString() + TEXT("\n") + HolderText);
	View.bShowCurrent = !bEquipped;
	const auto* Current = EquipmentManager->GetEquippedInstance(CurrentEmployeeId, Def->EquipmentSlot);
	const auto* CurrentDef = Current ? TableManager->FindEmployeeEquipmentDefinition(Current->DefinitionId) : nullptr;
	View.CurrentHeader = CurrentDef ? Text(TEXT("현재 장비\n") + CurrentDef->DisplayName.ToString() + FString::Printf(TEXT(" +%d"), Current->EnhanceLevel)) : LOCTEXT("NotEquipped", "현재 장비\n미장착");
	if (CurrentDef && !CurrentDef->Icon.IsNull()) View.CurrentIcon = CurrentDef->Icon.LoadSynchronous();
	if (CurrentDef)
	{
		View.CurrentGrade = CurrentDef->Grade;
		View.CurrentVisualGrade = CGEquipmentIcon::VisualGrade(TableManager, *CurrentDef);
	}
	FString Delta, Protection;
	if (const auto* Economy = TableManager->GetEmployeeEquipmentEconomy())
	{
		const auto Before = FEmployeeEquipmentEffectEvaluator::BuildLoadout(EquipmentManager->GetState(), CurrentEmployeeId, TableManager->GetEmployeeEquipmentDefinitions(), TableManager->GetEmployeeEquipmentEffects(), *Economy);
		const auto After = FEmployeeEquipmentEffectEvaluator::BuildLoadout(EquipmentManager->GetState(), CurrentEmployeeId, TableManager->GetEmployeeEquipmentDefinitions(), TableManager->GetEmployeeEquipmentEffects(), *Economy, Def->EquipmentSlot, InstanceId);
		TSet<FName> Keys;
		for (const auto& Pair : Before.BasicValues) Keys.Add(Pair.Key);
		for (const auto& Pair : After.BasicValues) Keys.Add(Pair.Key);
		TArray<FName> Sorted = Keys.Array(); Sorted.Sort(FNameLexicalLess());
		for (FName Key : Sorted)
		{
			const auto* Effect = TableManager->FindEmployeeEquipmentEffect(Key);
			if (!Effect || (bChangesOnly && FMath::IsNearlyEqual(Before.BasicValues.FindRef(Key), After.BasicValues.FindRef(Key)))) continue;
			// 표로 그리는 상세 창은 같은 값을 행으로 받는다 — 문자열을 다시 파싱하지 않는다.
			{
				const float BeforeValue = Before.BasicValues.FindRef(Key);
				const float AfterValue = After.BasicValues.FindRef(Key);
				// 이전·이후는 자릿수를 맞춘다 — 0 과 0.05 가 나란히 있으면 소수점이 들쭉날쭉해 비교가 안 읽힌다.
				const int32 Digits = FMath::Max(FractionDigits(DisplayValue(*Effect, BeforeValue)), FractionDigits(DisplayValue(*Effect, AfterValue)));
				FEquipmentDeltaRow& Row = View.DeltaRows.AddDefaulted_GetRef();
				Row.Name = Effect->DisplayName;
				Row.Before = Text(FormatEffectValue(*Effect, BeforeValue, Digits));
				Row.After = Text(FormatEffectValue(*Effect, AfterValue, Digits));
				Row.bUp = AfterValue > BeforeValue;
			}
			if (bChangesOnly && !IsDrawerPresentation()) Delta += FString::Printf(TEXT("%s\n%s → %s\n"), *Effect->DisplayName.ToString(), *FormatEffectValue(*Effect, Before.BasicValues.FindRef(Key)), *FormatEffectValue(*Effect, After.BasicValues.FindRef(Key)));
			else Delta += FString::Printf(TEXT("%s %s → %s\n"), *Effect->DisplayName.ToString(), *FormatEffectValue(*Effect, Before.BasicValues.FindRef(Key)), *FormatEffectValue(*Effect, After.BasicValues.FindRef(Key)));
		}
		for (const auto& Pair : Before.UniqueEffects) if (!After.UniqueEffects.Contains(Pair.Key))
		{
			Protection += TEXT("상실 ") + Pair.Value.DisplayName.ToString() + TEXT(" · ");
			View.UniqueChanges.Add({Pair.Value.DisplayName, false});
		}
		for (const auto& Pair : After.UniqueEffects) if (!Before.UniqueEffects.Contains(Pair.Key))
		{
			Protection += TEXT("획득 ") + Pair.Value.DisplayName.ToString() + TEXT(" · ");
			View.UniqueChanges.Add({Pair.Value.DisplayName, true});
		}
	}
	View.Comparison = bChangesOnly
		? Text(Delta.IsEmpty() ? (Protection.IsEmpty() ? TEXT("장비 합산 변화 없음") : TEXT("기본 수치 변화 없음")) : Delta.TrimEnd())
		: Text(TEXT("장비 합산 (직원 기본·잠재 제외)\n") + Delta);
	if (bChangesOnly)
	{
		Protection.RemoveFromEnd(TEXT(" · "));
		if (!Employees || !Employees->GetEmployeeData(CurrentEmployeeId)) Protection += (Protection.IsEmpty() ? TEXT("") : TEXT("\n")) + FString(TEXT("장착할 직원이 없습니다"));
		View.Notice = Text(Protection);
	}
	else View.Notice = Text(Protection + (CurrentEmployeeId >= 0 && Employees ? TEXT("대상: ") + Employees->GetEmployeeFullName(CurrentEmployeeId) : TEXT("장착할 직원이 없습니다")));
	View.Primary = bEquipped ? LOCTEXT("Enhance", "강화") : Holder >= 0 ? LOCTEXT("Transfer", "가져와 장착") : Current ? LOCTEXT("Replace", "교체하기") : LOCTEXT("Equip", "장착하기");
	View.bPrimaryEnabled = Employees && Employees->GetEmployeeData(CurrentEmployeeId);
	View.Secondary = bEquipped ? LOCTEXT("Unequip", "해제하기") : LOCTEXT("Enhance", "강화");
	View.Tertiary = Item->bLocked ? LOCTEXT("Unlock", "잠금 해제") : LOCTEXT("Lock", "잠금");
	if (Holder < 0 && !Item->bLocked) View.Extra = LOCTEXT("Dismantle", "분해하기");
	return true;
}

void UEmployeeEquipmentWorkspaceWidget::ShowDetail(FGuid InstanceId)
{
	FEmployeeEquipmentDialogView View;
	if (!BuildItemDetailView(InstanceId, View)) { DismissDialog(); return; }
	const auto* Item = EquipmentManager->FindInstance(InstanceId);
	const auto* Def = TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId);
	SelectedInstanceId = InstanceId;
	SelectedDefinitionId = Def->DefinitionId;
	CurrentSlot = Def->EquipmentSlot;
	ShowDialog(View);
	if (!Dialog) return;
	const bool bEquipped = CurrentEmployeeId >= 0 && EquipmentManager->GetInstanceEmployee(InstanceId) == CurrentEmployeeId;
	Dialog->OnPrimary.BindUObject(this, bEquipped ? &ThisClass::HandleEnhance : &ThisClass::EquipSelected);
	Dialog->OnSecondary.BindUObject(this, bEquipped ? &ThisClass::UnequipSelected : &ThisClass::HandleEnhance);
	Dialog->OnTertiary.BindUObject(this, &ThisClass::ToggleSelectedLock);
	Dialog->OnExtra.BindUObject(this, &ThisClass::ShowDismantle);
	PreviewDefinition(Def->DefinitionId);
}

void UEmployeeEquipmentWorkspaceWidget::ShowDefinition(FName DefinitionId)
{
	const auto* Def = TableManager->FindEmployeeEquipmentDefinition(DefinitionId);
	if (!Def) return;
	SelectedDefinitionId = DefinitionId;
	CurrentSlot = Def->EquipmentSlot;
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("AppearanceDetail", "외형 상세");
	View.bShowCurrent = false;
	FillDefinitionView(View, *Def, 0);
	if (const auto* Appearance = TableManager->FindEmployeeEquipmentAppearance(Def->AppearanceId))
	{
		View.SelectedVisualGrade = Appearance->VisualGrade;
		if (!Appearance->Icon.IsNull()) View.SelectedIcon = Appearance->Icon.LoadSynchronous();
	}
	View.Effects = LOCTEXT("AppearanceEffect", "외형만 변경합니다. 현재 장비와 능력치는 유지됩니다.");
	View.Primary = LOCTEXT("ApplyAppearance", "외형 적용");
	View.Secondary = LOCTEXT("UseEquipmentAppearance", "장비 외형 사용");
	View.bPrimaryEnabled = CurrentEmployeeId >= 0 && EquipmentManager->GetState().UnlockedAppearanceIds.Contains(Def->AppearanceId);
	if (const auto* Part = TableManager->FindEmployeeEquipmentSlot(CurrentSlot)) if (Part->bCanHide) View.Tertiary = LOCTEXT("HideAppearance", "표시 숨김");
	ShowDialog(View);
	if (Dialog) { Dialog->OnPrimary.BindUObject(this, &ThisClass::ApplyAppearance); Dialog->OnSecondary.BindUObject(this, &ThisClass::ApplyEquipmentAppearance); Dialog->OnTertiary.BindUObject(this, &ThisClass::HideAppearance); }
	PreviewDefinition(DefinitionId);
}

void UEmployeeEquipmentWorkspaceWidget::EquipSelected()
{
	auto* EmployeesForTarget = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (!EmployeesForTarget || !EmployeesForTarget->GetEmployeeData(CurrentEmployeeId)) return;
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::SetSlot);
	PendingRequest.EmployeeId = CurrentEmployeeId; PendingRequest.EquipmentSlot = CurrentSlot; PendingRequest.InstanceId = SelectedInstanceId;
	PendingRequest.ExpectedSourceEmployeeId = EquipmentManager->GetInstanceEmployee(SelectedInstanceId);
	if (PendingRequest.ExpectedSourceEmployeeId >= 0 && PendingRequest.ExpectedSourceEmployeeId != CurrentEmployeeId)
	{
		FEmployeeEquipmentDialogView View;
		View.Title = LOCTEXT("TransferConfirm", "직원 간 장비 이전"); View.bShowCurrent = false;
		UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
		View.Description = Text(Employees->GetEmployeeFullName(PendingRequest.ExpectedSourceEmployeeId) + TEXT("의 장비를 ") + Employees->GetEmployeeFullName(CurrentEmployeeId) + TEXT("에게 이전합니다. 이전 직원은 이 장비의 모든 효과를 잃습니다."));
		View.Notice = LOCTEXT("TransferLoss", "원래 직원의 장비 슬롯이 비워집니다.");
		View.Primary = LOCTEXT("TransferCommit", "이전 후 장착"); View.Secondary = LOCTEXT("Cancel", "취소");
		ShowDialog(View);
		if (Dialog) { Dialog->OnPrimary.BindUObject(this, &ThisClass::ExecutePending); Dialog->OnSecondary.BindUObject(this, &ThisClass::HandleDialogBack); }
	}
	else ExecutePending();
}

void UEmployeeEquipmentWorkspaceWidget::UnequipSelected()
{
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::SetSlot);
	PendingRequest.EmployeeId = CurrentEmployeeId; PendingRequest.EquipmentSlot = CurrentSlot;
	FEmployeeEquipmentDialogView View;
	View.Title = LOCTEXT("UnequipConfirm", "장비 해제"); View.bShowCurrent = false;
	const auto* Item = EquipmentManager->FindInstance(SelectedInstanceId);
	const auto* Def = Item ? TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	if (Def) FillDefinitionView(View, *Def, Item->EnhanceLevel);
	View.Notice = LOCTEXT("UnequipLoss", "표시된 장비 효과를 모두 잃습니다. 장비는 보관함에 남습니다.");
	View.Primary = LOCTEXT("Unequip", "해제하기"); View.Secondary = LOCTEXT("Cancel", "취소");
	ShowDialog(View);
	if (Dialog) { Dialog->OnPrimary.BindUObject(this, &ThisClass::ExecutePending); Dialog->OnSecondary.BindUObject(this, &ThisClass::HandleDialogBack); }
}

void UEmployeeEquipmentWorkspaceWidget::ToggleSelectedLock()
{
	const auto* Item = EquipmentManager->FindInstance(SelectedInstanceId);
	if (!Item) return;
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::SetLock);
	PendingRequest.InstanceIds = {SelectedInstanceId}; PendingRequest.bLocked = !Item->bLocked;
	ExecutePending();
}

// 외형 고르기 — 옛 외형 목록/페이지를 대신한다. 부위 하나에 대해 해금된 외형만 보여주고,
// 장비 외형 사용 / 표시 숨김 두 모드도 같은 창에서 고른다(옛 2단 모달을 한 장으로).
void UEmployeeEquipmentWorkspaceWidget::ShowAppearancePicker()
{
	if (!EquipmentManager || !TableManager || CurrentEmployeeId < 0) return;
	const auto* Part = TableManager->FindEmployeeEquipmentSlot(CurrentSlot);
	FEmployeeEquipmentDialogView View;
	View.Title = Part ? FText::Format(LOCTEXT("AppearancePicker", "{0} 외형"), Part->DisplayName)
		: LOCTEXT("AppearancePickerAll", "외형");
	View.bShowCurrent = false;
	View.Description = LOCTEXT("AppearanceOnly", "겉모습만 바뀝니다. 장착한 장비와 능력치는 그대로입니다.");
	View.Primary = LOCTEXT("UseEquipmentAppearance", "장비 외형 사용");
	if (Part && Part->bCanHide) View.Secondary = LOCTEXT("HideAppearance", "표시 숨김");
	ShowDialog(View);
	if (!Dialog) return;

	DialogEntries.Empty();
	TArray<UObject*> Choices;
	for (const auto& Pair : TableManager->GetEmployeeEquipmentDefinitions())
	{
		const auto& Def = Pair.Value;
		if (Def.EquipmentSlot != CurrentSlot) continue;
		if (!EquipmentManager->GetState().UnlockedAppearanceIds.Contains(Def.AppearanceId)) continue;
		auto* Entry = NewObject<UEmployeeEquipmentItemObject>(this);
		Entry->DefinitionID = Def.DefinitionId;
		Entry->ChoiceLabel = Def.DisplayName;
		if (const auto* Appearance = TableManager->FindEmployeeEquipmentAppearance(Def.AppearanceId))
			if (!Appearance->Icon.IsNull()) Entry->Icon = Appearance->Icon.LoadSynchronous();
		DialogEntries.Add(Entry);
		Choices.Add(Entry);
	}
	Choices.Sort([](const UObject& A, const UObject& B)
		{ return CastChecked<UEmployeeEquipmentItemObject>(&A)->DefinitionID.LexicalLess(
			CastChecked<UEmployeeEquipmentItemObject>(&B)->DefinitionID); });
	Dialog->ShowItems(Choices);
	Dialog->OnItemClicked.BindUObject(this, &ThisClass::HandleAppearanceChosen);
	Dialog->OnPrimary.BindUObject(this, &ThisClass::ApplyEquipmentAppearance);
	if (Part && Part->bCanHide) Dialog->OnSecondary.BindUObject(this, &ThisClass::HideAppearance);
}

void UEmployeeEquipmentWorkspaceWidget::HandleAppearanceChosen(UObject* Item)
{
	const auto* Choice = Cast<UEmployeeEquipmentItemObject>(Item);
	if (!Choice) return;
	SelectedDefinitionId = Choice->DefinitionID;
	ApplyAppearance();
}

void UEmployeeEquipmentWorkspaceWidget::ApplyAppearance()
{
	const auto* Def = TableManager->FindEmployeeEquipmentDefinition(SelectedDefinitionId);
	if (!Def) return;
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::SetAppearance);
	PendingRequest.EmployeeId = CurrentEmployeeId; PendingRequest.EquipmentSlot = CurrentSlot;
	PendingRequest.AppearanceMode = EEquipmentAppearanceMode::Override; PendingRequest.AppearanceId = Def->AppearanceId;
	ExecutePending();
}

void UEmployeeEquipmentWorkspaceWidget::ApplyEquipmentAppearance()
{
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::SetAppearance);
	PendingRequest.EmployeeId = CurrentEmployeeId; PendingRequest.EquipmentSlot = CurrentSlot;
	PendingRequest.AppearanceMode = EEquipmentAppearanceMode::UseEquipment;
	ExecutePending();
}

void UEmployeeEquipmentWorkspaceWidget::HideAppearance()
{
	PendingRequest = EquipmentManager->MakeRequest(EEquipmentCommand::SetAppearance);
	PendingRequest.EmployeeId = CurrentEmployeeId; PendingRequest.EquipmentSlot = CurrentSlot;
	PendingRequest.AppearanceMode = EEquipmentAppearanceMode::Hidden;
	ExecutePending();
}

void UEmployeeEquipmentWorkspaceWidget::HandleEquipmentChanged() { Refresh(); }
void UEmployeeEquipmentWorkspaceWidget::HandlePortraitChanged(int32 EmployeeId, int64 Revision)
{
	if (EmployeeId != CurrentEmployeeId) return;
	RefreshPortrait();
	if (IsInspectorActive()) RefreshInspector();
}
void UEmployeeEquipmentWorkspaceWidget::HandleStorage()
{
	DismissDialog();
	CurrentView = CurrentView == EEquipmentWorkspaceView::Inventory ? EmployeeReturnView : EEquipmentWorkspaceView::Inventory;
	Refresh();
}
void UEmployeeEquipmentWorkspaceWidget::HandleGacha()
{
	if (bGachaHost) return;
	auto* UI = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	const auto WidgetClass = TableManager ? TableManager->GetWidgetClass(EWidgetType::EquipmentGachaPanel) : nullptr;
	if (!UI || !UI->GetUIBase() || !WidgetClass) return;
	DismissDialog();
	SuspendPreview();
	bReturningFromGacha = true;
	if (!UI->GetUIBase()->PushPromptClass(WidgetClass.Get(), [this](UCommonActivatableWidget& Widget)
	{
		if (auto* Panel = Cast<UEquipmentGachaPanelWidget>(&Widget)) Panel->Open(CurrentEmployeeId);
	})) Resume();
}
void UEmployeeEquipmentWorkspaceWidget::HandleCurrentItem() { if (const auto* Item = EquipmentManager->GetEquippedInstance(CurrentEmployeeId, CurrentSlot)) ShowDetail(Item->InstanceId); }
void UEmployeeEquipmentWorkspaceWidget::HandleEnhance() { if (!SelectedInstanceId.IsValid()) { if (const auto* Item = EquipmentManager->GetEquippedInstance(CurrentEmployeeId, CurrentSlot)) SelectedInstanceId = Item->InstanceId; } ShowEnhance(); }
void UEmployeeEquipmentWorkspaceWidget::HandleEquippedEnhance() { SelectedInstanceId.Invalidate(); HandleEnhance(); }
void UEmployeeEquipmentWorkspaceWidget::OpenAllEquipment()
{
	if (CurrentView != EEquipmentWorkspaceView::Employee) return;
	bAllSlots = true;
	GradeFilter = EEmployeeEquipmentGrade::Count;
	ClearInspectionSelection();
	if (InventoryView) InventoryView->SetScrollOffset(0.f);
	SetupFilters();
	Refresh();
}

void UEmployeeEquipmentWorkspaceWidget::OpenSlot(EEmployeeEquipmentSlot EquipmentSlot)
{
	if (CurrentView != EEquipmentWorkspaceView::Employee) return;
	GradeFilter = EEmployeeEquipmentGrade::Count;
	HandleSlot(static_cast<int32>(EquipmentSlot));
}
void UEmployeeEquipmentWorkspaceWidget::HandleSlot(int32 Index)
{
	if (Index < 0 || Index >= static_cast<int32>(EEmployeeEquipmentSlot::Count)) return;
	ClearInspectionSelection();
	CurrentSlot = static_cast<EEmployeeEquipmentSlot>(Index); bAllSlots = false;
	SetupFilters(); Refresh();
}
void UEmployeeEquipmentWorkspaceWidget::HandlePartFilterSelected(UCommonButtonBase* Button, int32 Index)
{
	if (bUpdating || Index < 0) return;
	// 0 은 전체다 — 부위를 지우되 CurrentSlot 은 남겨 두어 다시 켤 때 같은 자리로 돌아온다.
	if (Index == 0) { ClearInspectionSelection(); bAllSlots = true; SetupFilters(); Refresh(); }
	else HandleSlot(Index - 1);
}

void UEmployeeEquipmentWorkspaceWidget::SyncPartFilterSelection()
{
	if (!PartFilterGroup) return;
	TGuardValue<bool> Updating(bUpdating, true);
	PartFilterGroup->SelectButtonAtIndex(bAllSlots ? 0 : static_cast<int32>(CurrentSlot) + 1, false);
}

void UEmployeeEquipmentWorkspaceWidget::HandleSlotButtonSelected(UCommonButtonBase* Button, int32 Index)
{
	if (bUpdating || !SlotButtonGroup || Index < 0 || Index >= 6) return;
	HandleSlot(Index);
}
void UEmployeeEquipmentWorkspaceWidget::HandlePullOne() { ShowPull(1); }
void UEmployeeEquipmentWorkspaceWidget::HandlePullTen() { ShowPull(10); }
void UEmployeeEquipmentWorkspaceWidget::HandlePullThirty() { ShowPull(30); }

void UEmployeeEquipmentWorkspaceWidget::HandleEmployeeSelected(FString Item, ESelectInfo::Type Type)
{
	if (bUpdating || bEmployeeContextLocked || !EmployeeCombo) return;
	const int32 Index = EmployeeCombo->GetSelectedIndex();
	if (!EmployeeOptionIds.IsValidIndex(Index)) return;
	ClearInspectionSelection();
	CurrentEmployeeId = EmployeeOptionIds[Index]; DismissDialog(); Refresh();
	OnEmployeeSelected.ExecuteIfBound(CurrentEmployeeId);
}

void UEmployeeEquipmentWorkspaceWidget::HandleSlotSelected(FString Item, ESelectInfo::Type Type) { if (bUpdating) return; const int32 Index = SlotFilterCombo->GetSelectedIndex(); bAllSlots = Index <= 0; if (Index > 0) CurrentSlot = static_cast<EEmployeeEquipmentSlot>(Index - 1); ClearInspectionSelection(); Refresh(); }
void UEmployeeEquipmentWorkspaceWidget::HandleGradeSelected(FString Item, ESelectInfo::Type Type) { if (bUpdating) return; const int32 Index = GradeFilterCombo->GetSelectedIndex(); GradeFilter = Index <= 0 ? EEmployeeEquipmentGrade::Count : static_cast<EEmployeeEquipmentGrade>(Index - 1); ClearInspectionSelection(); RefreshInventory(); }
void UEmployeeEquipmentWorkspaceWidget::HandleItemClicked(UObject* Item)
{
	if (const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Item))
	{
		const FGuid ClickedId = Entry->InstanceID;
		const bool bNewSelection = ClickedId.IsValid() && SelectedInstanceId != ClickedId;
		PendingSelectionPulseId = bNewSelection ? ClickedId : FGuid();
		bReturnToResults = false;
		if (IsInspectorActive() && ClickedId.IsValid()) SelectInspectionItem(ClickedId);
		else if (ClickedId.IsValid()) ShowDetail(ClickedId);
		else ShowDefinition(Entry->DefinitionID);
		if (PendingSelectionPulseId.IsValid() && InventoryView)
		{
			for (const TObjectPtr<UEmployeeEquipmentItemObject>& EntryPtr : Entries)
			{
				UEmployeeEquipmentItemObject* CurrentEntry = EntryPtr.Get();
				if (!CurrentEntry || CurrentEntry->InstanceID != ClickedId) continue;
				if (UEmployeeEquipmentCardWidget* Card = InventoryView->GetEntryWidgetFromItem<UEmployeeEquipmentCardWidget>(CurrentEntry))
					TryConsumeSelectionPulse(Card);
				break;
			}
		}
		if (bNewSelection && !IsInspectorActive() && Dialog)
		{
			if (UImage* SelectedIcon = Cast<UImage>(Dialog->GetWidgetFromName(TEXT("SelectedIconImage"))))
				CGEquipmentIcon::TriggerPulse(Cast<UMaterialInstanceDynamic>(SelectedIcon->GetBrush().GetResourceObject()));
		}
	}
}

void UEmployeeEquipmentWorkspaceWidget::HandleInventoryEntryGenerated(UUserWidget& EntryWidget)
{
	TryConsumeSelectionPulse(Cast<UEmployeeEquipmentCardWidget>(&EntryWidget));
}

void UEmployeeEquipmentWorkspaceWidget::TryConsumeSelectionPulse(UEmployeeEquipmentCardWidget* Card)
{
	if (!Card || !PendingSelectionPulseId.IsValid()) return;
	const UEmployeeEquipmentItemObject* Item = Card->GetListItem<UEmployeeEquipmentItemObject>();
	if (!Item || Item->InstanceID != PendingSelectionPulseId) return;
	if (!Entries.ContainsByPredicate([Item](const TObjectPtr<UEmployeeEquipmentItemObject>& Entry)
		{ return Entry.Get() == Item; })) return;
	Card->TriggerIconPulse();
	PendingSelectionPulseId.Invalidate();
}

#undef LOCTEXT_NAMESPACE
