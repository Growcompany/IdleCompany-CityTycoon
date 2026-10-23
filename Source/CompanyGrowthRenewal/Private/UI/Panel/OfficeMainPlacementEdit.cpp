#include "UI/Panel/OfficeMainWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture2D.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Player/OfficeCameraPawn.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"

void UOfficeMainWidget::BuildPlacementEditControls()
{
	if (PlacementEditCanvas || !WidgetTree) return;
	UOverlay* RootOverlay = Cast<UOverlay>(GetRootWidget());
	const auto* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (!RootOverlay || !Tables) return;
	const TSubclassOf<UUserWidget> PlacementClass = Tables->GetWidgetClass(EWidgetType::BuildPlacementPanel);
	PlacementEditPanel = PlacementClass ? CreateWidget<UBuildPlacementPanelWidget>(this, PlacementClass) : nullptr;
	if (!PlacementEditPanel) return;
	// BottomStack에 push하면 OfficeMain이 비활성화되어 진행 중인 편집 세션이 종료된다.
	PlacementEditPanel->SetOfficeEditMode(Cast<AOfficeCameraPawn>(GetOwningPlayerPawn()));
	UOverlaySlot* PlacementPanelSlot = RootOverlay->AddChildToOverlay(PlacementEditPanel);
	PlacementPanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PlacementPanelSlot->SetVerticalAlignment(VAlign_Fill);
	PlacementEditPanel->ActivateWidget();

	PlacementEditCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("OfficePlacementEditCanvas"));
	PlacementEditCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlaySlot* EditOverlaySlot = RootOverlay->AddChildToOverlay(PlacementEditCanvas);
	EditOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
	EditOverlaySlot->SetVerticalAlignment(VAlign_Fill);

	PlacementEditHint = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("OfficePlacementEditHint"));
	// 버튼 WBP가 소유하는 폰트를 재사용해 런타임 기본 Roboto 폴백을 피한다.
	if (const UCommonTextBlock* Label = DecorationOpenButton
		? Cast<UCommonTextBlock>(DecorationOpenButton->GetWidgetFromName(TEXT("ButtonText"))) : nullptr)
	{
		FSlateFontInfo HintFont = Label->GetFont();
		HintFont.Size = 28;
		HintFont.OutlineSettings.OutlineSize = 2;
		HintFont.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.04f, 0.05f, 1.f);
		PlacementEditHint->SetFont(HintFont);
	}
	PlacementEditHint->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PlacementEditHint->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* HintSlot = PlacementEditCanvas->AddChildToCanvas(PlacementEditHint);
	HintSlot->SetAnchors(FAnchors(0.5f, 1.f));
	HintSlot->SetAlignment(FVector2D(0.5f, 1.f));
	HintSlot->SetPosition(FVector2D(0.f, -200.f));
	HintSlot->SetAutoSize(true);
	RefreshPlacementEditControls();
}

void UOfficeMainWidget::BindPlacementEditControls()
{
	UnbindPlacementEditControls();
	PlacementEditPawn = GetOwningPlayer() ? Cast<AOfficeCameraPawn>(GetOwningPlayer()->GetPawn()) : nullptr;
	if (AOfficeCameraPawn* Camera = PlacementEditPawn.Get())
		Camera->OnOfficePlacementEditChanged.AddDynamic(this, &ThisClass::RefreshPlacementEditControls);
	if (PlacementEditDoneButton) PlacementEditDoneButton->OnClicked().AddUObject(this, &ThisClass::HandlePlacementEditToggle);
	if (PlacementEditPanel) PlacementEditPanel->OnOfficePlacementFeedback.AddUObject(this, &ThisClass::HandlePlacementEditFeedback);
	RefreshPlacementEditControls();
	BeginPendingPlacementEdit();
}

void UOfficeMainWidget::RequestPlacementEdit(AActor* Target)
{
	bPendingPlacementEdit = true;
	PendingPlacementEditTarget = Target;
	if (IsActivated()) BeginPendingPlacementEdit();
}

void UOfficeMainWidget::BeginPendingPlacementEdit()
{
	if (!bPendingPlacementEdit) return;
	bPendingPlacementEdit = false;
	if (PendingPlacementEditTarget.IsStale()) return;
	AActor* Target = PendingPlacementEditTarget.Get();
	PendingPlacementEditTarget.Reset();
	if (AOfficeCameraPawn* Camera = PlacementEditPawn.Get())
	{
		if (Camera->BeginOfficePlacementEdit() && Target && !Camera->SelectOfficeEditActor(Target))
		{
			Camera->EndOfficePlacementEdit();
		}
	}
}

void UOfficeMainWidget::UnbindPlacementEditControls()
{
	if (AOfficeCameraPawn* Camera = PlacementEditPawn.Get())
	{
		Camera->EndOfficePlacementEdit();
		Camera->OnOfficePlacementEditChanged.RemoveAll(this);
	}
	PlacementEditPawn.Reset();
	if (PlacementEditDoneButton) PlacementEditDoneButton->OnClicked().RemoveAll(this);
	if (PlacementEditPanel) PlacementEditPanel->OnOfficePlacementFeedback.RemoveAll(this);
}

void UOfficeMainWidget::RefreshPlacementEditControls()
{
	const AOfficeCameraPawn* Camera = PlacementEditPawn.Get();
	const bool bEditing = Camera && Camera->IsOfficePlacementEditing();
	const UOfficeStageProgressManager* StageMgr = GetWorld()
		? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	if (UWidget* ContentRail = GetWidgetFromName(TEXT("OfficeContentRail")))
	{
		const bool bShowContent = !bEditing && StageMgr && StageMgr->GetLifecycle() == EProjectLifecycle::Idle;
		ContentRail->SetVisibility(bShowContent ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StageMgr) RefreshOpCardVisibility(StageMgr->GetLifecycle());
	const bool bSelected = bEditing && Camera->GetOfficeEditSelection();
	if (PlacementEditDoneButton) PlacementEditDoneButton->SetVisibility(bEditing ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (PlacementEditPanel)
	{
		PlacementEditPanel->RefreshOfficeEditSelection();
		PlacementEditPanel->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (PlacementEditHint)
	{
		PlacementEditHint->SetText(FText::FromString(bSelected
			? TEXT("선택한 물건을 끌어서 이동한 뒤 확정하세요")
			: TEXT("이동할 책상이나 장식을 선택하세요")));
		PlacementEditHint->SetVisibility(bEditing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bEditing != bPlacementEditSuppressedHud)
	{
		if (bEditing)
		{
			if (OfficeDockTray) PlacementEditPreviousDockVisibility = OfficeDockTray->GetVisibility();
			if (RightIdleZone) bPlacementEditPreviousRightEnabled = RightIdleZone->GetIsEnabled();
			if (EquipmentManualButton) bPlacementEditPreviousEquipmentEnabled = EquipmentManualButton->GetIsEnabled();
		}
		if (OfficeDockTray) OfficeDockTray->SetVisibility(bEditing ? ESlateVisibility::Collapsed : PlacementEditPreviousDockVisibility);
		if (RightIdleZone) RightIdleZone->SetIsEnabled(bEditing ? false : bPlacementEditPreviousRightEnabled);
		if (EquipmentManualButton) EquipmentManualButton->SetIsEnabled(bEditing ? false : bPlacementEditPreviousEquipmentEnabled);
		bPlacementEditSuppressedHud = bEditing;
	}
}

void UOfficeMainWidget::HandlePlacementEditToggle()
{
	if (AOfficeCameraPawn* Camera = PlacementEditPawn.Get())
	{
		if (Camera->IsOfficePlacementEditing()) Camera->EndOfficePlacementEdit();
		else Camera->BeginOfficePlacementEdit();
	}
}

void UOfficeMainWidget::HandlePlacementEditFeedback(const FText& Message)
{
	if (PlacementEditHint) PlacementEditHint->SetText(Message);
}
