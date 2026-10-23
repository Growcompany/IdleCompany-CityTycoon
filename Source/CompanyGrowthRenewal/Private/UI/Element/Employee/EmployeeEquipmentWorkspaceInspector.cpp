#include "UI/Element/Employee/EmployeeEquipmentWorkspaceWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Employee/EquipmentDetailWindowWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Gacha/GachaCaptureStage.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/TileView.h"
#include "CommonTextBlock.h"

#define LOCTEXT_NAMESPACE "EmployeeEquipmentUI"

void UEmployeeEquipmentWorkspaceWidget::DismissDetailWindow()
{
	if (!DetailWindow) return;
	DetailWindow->RemoveFromParent();
	DetailWindow = nullptr;
}

void UEmployeeEquipmentWorkspaceWidget::RefreshDetailWindow(const FEquipmentDetailContent& Content)
{
	if (!DetailWindow)
	{
		const auto WindowClass = TableManager ? TableManager->GetWidgetClass(EWidgetType::EquipmentDetailWindow) : nullptr;
		if (!WindowClass) { UE_LOG(LogTemp, Error, TEXT("[EquipmentUI] EquipmentDetailWindow is not registered")); return; }
		DetailWindow = CreateWidget<UEquipmentDetailWindowWidget>(GetOwningPlayer(), WindowClass);
		if (!DetailWindow) return;
	}
	if (!DetailWindow->GetParent())
	{
		if (UPanelWidget* Host = DetailWindowHost.Get())
		{
			// 호스트 루트의 마지막 자식 = 드로어 위. 창은 자기 안에서 우변 도킹을 잡으므로
			// 여기서는 화면 전체를 덮는 좌표계만 만들어 준다.
			UPanelSlot* HostSlot = Host->AddChild(DetailWindow);
			if (UOverlaySlot* AsOverlay = Cast<UOverlaySlot>(HostSlot))
			{
				AsOverlay->SetHorizontalAlignment(HAlign_Fill);
				AsOverlay->SetVerticalAlignment(VAlign_Fill);
			}
			else if (UCanvasPanelSlot* AsCanvas = Cast<UCanvasPanelSlot>(HostSlot))
			{
				// 캔버스 기본 슬롯은 크기 0 이라 창이 통째로 안 보인다 — 전면으로 펼친다.
				AsCanvas->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
				AsCanvas->SetOffsets(FMargin(0.f));
				// 드로어 위로 올린다. 형제의 실제 최대 ZOrder 를 읽어 값을 추측하지 않는다.
				int32 TopOrder = 0;
				for (int32 Index = 0; Index < Host->GetChildrenCount(); ++Index)
				{
					const UWidget* Sibling = Host->GetChildAt(Index);
					if (const UCanvasPanelSlot* SiblingSlot = Sibling ? Cast<UCanvasPanelSlot>(Sibling->Slot) : nullptr)
					{
						TopOrder = FMath::Max(TopOrder, SiblingSlot->GetZOrder());
					}
				}
				AsCanvas->SetZOrder(TopOrder + 1);
			}
			// 창이 안 보일 때 슬롯 종류부터 확인할 수 있게 남긴다 — 이 지점이 두 번 틀렸다.
			UE_LOG(LogTemp, Log, TEXT("[EquipmentUI] detail window host=%s slot=%s"),
				*Host->GetName(), HostSlot ? *HostSlot->GetClass()->GetName() : TEXT("null"));
		}
		// 호스트를 못 찾는 독립 셸에서는 뷰포트로 떨어진다(모달 120 보다 아래).
		else DetailWindow->AddToViewport(115);
	}
	// 창은 매번 다시 묶는다 — 재사용 위젯의 델리게이트는 show 마다 재바인딩한다.
	DetailWindow->OnPrimary.BindUObject(this, &ThisClass::HandleInspectorEquip);
	DetailWindow->OnSecondary.BindUObject(this, &ThisClass::HandleInspectorEnhance);
	DetailWindow->OnClose.BindUObject(this, &ThisClass::CloseInspector);
	DetailWindow->SetDockOffset(DrawerDetailDockOffset);
	DetailWindow->Configure(Content);
}

bool UEmployeeEquipmentWorkspaceWidget::IsInspectorActive() const
{
	return InspectorPane && CurrentView == EEquipmentWorkspaceView::Employee;
}

void UEmployeeEquipmentWorkspaceWidget::CloseInspector()
{
	ClearInspectionSelection();
}

bool UEmployeeEquipmentWorkspaceWidget::HandleBack()
{
	if (PullNotice.IsValid() && PullNotice->IsInViewport())
	{
		DismissDialog();
		return true;
	}
	if (Dialog || RevealPresentation)
	{
		HandleDialogBack();
		return true;
	}
	if (IsInspectorActive() && SelectedInstanceId.IsValid())
	{
		CloseInspector();
		return true;
	}
	if (!bGachaHost && bEmployeeContextLocked && (CurrentView == EEquipmentWorkspaceView::Gacha || CurrentView == EEquipmentWorkspaceView::Inventory))
	{
		ConfigureForEmployee(CurrentEmployeeId, EmployeeReturnView);
		return true;
	}
	return false;
}

void UEmployeeEquipmentWorkspaceWidget::ClearInspectionSelection(bool bRestorePreview)
{
	SelectedInstanceId.Invalidate();
	SelectedDefinitionId = NAME_None;
	InspectorFailureNotice = FText();
	if (InventoryView) InventoryView->ClearSelection();
	if (bRestorePreview && PreviewStage && PreviewStage->IsHeldBy(this)) PreviewStage->ClearEquipmentAppearancePreview();
	if (PreviewLabelText) PreviewLabelText->SetVisibility(ESlateVisibility::Collapsed);
	RefreshInspector();
}

void UEmployeeEquipmentWorkspaceWidget::SelectInspectionItem(FGuid InstanceId)
{
	if (!IsInspectorActive() || !EquipmentManager || !TableManager) return;
	const bool bNewSelection = SelectedInstanceId != InstanceId;
	const auto* Item = EquipmentManager->FindInstance(InstanceId);
	const auto* Definition = Item ? TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	if (!Definition) { ClearInspectionSelection(); RefreshInspector(); return; }
	ClearInspectionSelection();
	DismissDialog();
	SelectedInstanceId = InstanceId;
	SelectedDefinitionId = Definition->DefinitionId;
	// 외부 보관함에서 고른 장비는 이전 필터 밖에 있어도 표시한다.
	if (GradeFilter != EEmployeeEquipmentGrade::Count && GradeFilter != Definition->Grade) GradeFilter = EEmployeeEquipmentGrade::Count;
	CurrentSlot = Definition->EquipmentSlot;
	SetupFilters();
	Refresh();
	if (bNewSelection)
	{
		CGEquipmentIcon::TriggerPulse(InspectorIconMaterial);
		if (DetailWindow) DetailWindow->TriggerSelectedIconPulse();
	}
}

void UEmployeeEquipmentWorkspaceWidget::RefreshInspector()
{
	const bool bActive = IsInspectorActive();
	if (LegacyEmployeeActions) LegacyEmployeeActions->SetVisibility(bActive ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	FEmployeeEquipmentDialogView View;
	const auto* Item = bActive && EquipmentManager ? EquipmentManager->FindInstance(SelectedInstanceId) : nullptr;
	const auto* Definition = Item && TableManager ? TableManager->FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	const bool bHasSelection = Definition && BuildItemDetailView(SelectedInstanceId, View, true);
	if (InspectorPane) InspectorPane->SetVisibility(bHasSelection ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	// 드로어에서는 독립 창이 상세를 맡으므로 목록 아래 호스트를 쓰지 않는다.
	if (UWidget* DrawerInspector = GetWidgetFromName(TEXT("DrawerInspectorHost")))
		DrawerInspector->SetVisibility(ESlateVisibility::Collapsed);
	if (InspectorContent) InspectorContent->SetVisibility(bHasSelection ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	const auto* Grade = bHasSelection ? TableManager->FindEmployeeEquipmentGrade(Definition->Grade) : nullptr;
	const auto* VisualGrade = bHasSelection ? TableManager->FindEmployeeEquipmentGrade(View.SelectedVisualGrade) : nullptr;
	CGEquipmentIcon::Apply(InspectorImage, CGEquipmentIcon::Resolve(this, bHasSelection ? View.SelectedIcon : nullptr,
		VisualGrade, EEmployeeEquipmentIconContext::Focused, InspectorIconMaterial));
	CGEquipmentIcon::ApplyGradeFrame(InspectorGradeFrame, Grade, Grade ? Grade->Color : FLinearColor::Transparent);
	if (InspectorImage) InspectorImage->SetVisibility(bHasSelection ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (InspectorNameText) InspectorNameText->SetText(bHasSelection ? Definition->DisplayName : FText());
	const auto* Part = bHasSelection ? TableManager->FindEmployeeEquipmentSlot(Definition->EquipmentSlot) : nullptr;
	if (InspectorGradeText)
	{
		FText GradeLine;
		if (bHasSelection) GradeLine = Item->EnhanceLevel > 0
			? FText::Format(LOCTEXT("InspectorGrade", "{0} · {1} · +{2}"), Grade ? Grade->DisplayName : FText(), Part ? Part->DisplayName : FText(), FText::AsNumber(Item->EnhanceLevel))
			: FText::Format(LOCTEXT("InspectorGradeUnenhanced", "{0} · {1}"), Grade ? Grade->DisplayName : FText(), Part ? Part->DisplayName : FText());
		InspectorGradeText->SetText(GradeLine);
	}
	UEmployeeManager* Employees = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeManager>() : nullptr;
	const bool bHasEmployee = Employees && Employees->GetEmployeeData(CurrentEmployeeId);
	const int32 HolderId = bHasSelection ? EquipmentManager->GetInstanceEmployee(SelectedInstanceId) : INDEX_NONE;
	const bool bEquipped = bHasEmployee && HolderId == CurrentEmployeeId;
	if (InspectorBodyText)
	{
		FString Body;
		if (bHasSelection)
		{
			if (IsDrawerPresentation())
			{
				Body = bEquipped ? LOCTEXT("InspectorEquipped", "현재 장착 중").ToString() + TEXT(" · ") + View.Effects.ToString() : View.Comparison.ToString();
				if (!bEquipped && HolderId >= 0 && Employees && Employees->GetEmployeeData(HolderId))
					Body += TEXT("\n") + Employees->GetEmployeeFullName(HolderId) + TEXT(" 착용 중");
			}
			else
			{
				Body = bEquipped ? LOCTEXT("InspectorEquipped", "현재 장착 중").ToString()
					: HolderId >= 0 && Employees && Employees->GetEmployeeData(HolderId) ? Employees->GetEmployeeFullName(HolderId) + TEXT(" 착용 중")
					: LOCTEXT("InspectorUnequipped", "미장착").ToString();
				if (!bEquipped && !View.Comparison.IsEmpty())
					Body += TEXT(" · ") + LOCTEXT("InspectorEquipChanges", "장착 시 변화").ToString() + TEXT("\n") + View.Comparison.ToString();
				else if (!View.Effects.IsEmpty())
					Body += TEXT(" · ") + LOCTEXT("InspectorItemEffects", "장비 효과").ToString() + TEXT("\n") + View.Effects.ToString();
			}
			if (!View.Notice.IsEmpty()) Body += TEXT("\n") + View.Notice.ToString();
			if (!InspectorFailureNotice.IsEmpty()) Body += TEXT("\n") + InspectorFailureNotice.ToString();
		}
		InspectorBodyText->SetText(FText::FromString(Body));
	}
	if (InspectorEquipButton)
	{
		const bool bReplaces = bHasSelection && EquipmentManager->GetEquippedInstance(CurrentEmployeeId, Definition->EquipmentSlot);
		InspectorEquipButton->SetButtonText(bEquipped ? LOCTEXT("InspectorUnequipAction", "해제") : bReplaces ? LOCTEXT("InspectorReplaceAction", "교체") : LOCTEXT("InspectorEquipAction", "장착"));
		InspectorEquipButton->SetIsEnabled(bHasSelection && bHasEmployee && InspectorFailureNotice.IsEmpty());
	}
	const auto* Economy = TableManager ? TableManager->GetEmployeeEquipmentEconomy() : nullptr;
	if (InspectorEnhanceButton)
	{
		const bool bMax = bHasSelection && Economy && Item->EnhanceLevel >= Economy->MaxEnhanceLevel;
		InspectorEnhanceButton->SetButtonText(bMax ? LOCTEXT("InspectorMaxEnhance", "최대") : LOCTEXT("Enhance", "강화"));
		InspectorEnhanceButton->SetIsEnabled(bHasSelection && bHasEmployee && !bMax);
	}
	if (InspectorDetailButton) InspectorDetailButton->SetIsEnabled(bHasSelection);

	if (bHasSelection && IsDrawerPresentation())
	{
		FEquipmentDetailContent Content;
		if (CGEquipmentDetail::BuildContent(*EquipmentManager, *TableManager, Employees,
			CurrentEmployeeId, SelectedInstanceId, Content))
		{
			if (!InspectorFailureNotice.IsEmpty())
			{
				Content.Notice = Content.Notice.IsEmpty() ? InspectorFailureNotice
					: FText::Format(LOCTEXT("DetailFailureNotice", "{0}\n{1}"), Content.Notice, InspectorFailureNotice);
				Content.bPrimaryEnabled = false;
			}
			RefreshDetailWindow(Content);
		}
		else DismissDetailWindow();
	}
	else
	{
		DismissDetailWindow();
	}
	if (PreviewLabelText)
	{
		PreviewLabelText->SetText(LOCTEXT("InspectorCurrentAppearance", "현재 장착 모습"));
		PreviewLabelText->SetVisibility(bActive ? (bHasEmployee ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden) : ESlateVisibility::Collapsed);
	}
	if (bActive && bPreviewEnabled && !Dialog)
	{
		if (bHasSelection && bHasEmployee && !bEquipped)
		{
			const bool bPreviewApplied = PreviewDefinition(Definition->DefinitionId);
			if (PreviewLabelText && bPreviewApplied)
			{
				PreviewLabelText->SetText(LOCTEXT("InspectorPreview", "시착 중 · 장착 전"));
				PreviewLabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		else if (PreviewStage && PreviewStage->IsHeldBy(this)) PreviewStage->ClearEquipmentAppearancePreview();
	}
}

void UEmployeeEquipmentWorkspaceWidget::HandleInspectorEquip()
{
	if (!IsInspectorActive() || !EquipmentManager || !EquipmentManager->FindInstance(SelectedInstanceId)) return;
	UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (!Employees || !Employees->GetEmployeeData(CurrentEmployeeId)) return;
	InspectorFailureNotice = FText();
	if (EquipmentManager->GetInstanceEmployee(SelectedInstanceId) == CurrentEmployeeId) UnequipSelected();
	else EquipSelected();
}

void UEmployeeEquipmentWorkspaceWidget::HandleInspectorEnhance()
{
	UEmployeeManager* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	if (IsInspectorActive() && Employees && Employees->GetEmployeeData(CurrentEmployeeId) && EquipmentManager && EquipmentManager->FindInstance(SelectedInstanceId)) HandleEnhance();
}

void UEmployeeEquipmentWorkspaceWidget::HandleInspectorDetail()
{
	if (IsInspectorActive() && EquipmentManager && EquipmentManager->FindInstance(SelectedInstanceId)) ShowDetail(SelectedInstanceId);
}

#undef LOCTEXT_NAMESPACE
