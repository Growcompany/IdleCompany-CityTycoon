#include "UI/Panel/EmployeeEquipmentPanelWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Manager/TableManagerSubsystem.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/TileView.h"

void UEmployeeEquipmentPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr) PanelCloseButton->OnCloseClicked.AddDynamic(this, &ThisClass::HandleClose);
	SetupPartRail();
	EnsureWorkspace();
}
void UEmployeeEquipmentPanelWidget::NativeDestruct()
{
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr) PanelCloseButton->OnCloseClicked.RemoveAll(this);
	if (PartGroup)
	{
		// 선택 필수인 채로 비우면 엔진이 남은 버튼을 재선택한다(CLAUDE.md 탭 그룹 규칙)
		PartGroup->OnButtonBaseClicked.RemoveAll(this);
		PartGroup->SetSelectionRequired(false);
		PartGroup->RemoveAll();
		PartGroup = nullptr;
	}
	if (Workspace) Workspace->DismissDialog();
	Super::NativeDestruct();
}
bool UEmployeeEquipmentPanelWidget::HasOwnList() const
{
	return InventoryView && PartFilterAll && PartFilter0 && PartFilter1 && PartFilter2 && PartFilter3 && PartFilter4 && PartFilter5;
}

void UEmployeeEquipmentPanelWidget::EnsureWorkspace()
{
	// 창이 제 목록을 들고 있으면 워크스페이스는 필요 없다 — 만들면 가챠·직원판까지 딸려 온다.
	if (HasOwnList() || !WorkspaceHost) return;
	if (!Workspace)
	{
		const auto* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		const auto Class = Tables ? Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentWorkspace) : nullptr;
		if (!Class) return;
		Workspace = CreateWidget<UEmployeeEquipmentWorkspaceWidget>(GetOwningPlayer(), Class);
	}
	if (Workspace && !Workspace->GetParent())
	{
		UOverlaySlot* HostSlot = WorkspaceHost->AddChildToOverlay(Workspace);
		HostSlot->SetHorizontalAlignment(HAlign_Fill); HostSlot->SetVerticalAlignment(VAlign_Fill);
	}
}
void UEmployeeEquipmentPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	EnsureWorkspace();
	if (Workspace) { if (Workspace->IsReturningFromShop()) Workspace->Resume(); else Workspace->Configure(ContextEmployeeId, InitialView); }
	RefreshList();
	if (auto* PC = Cast<AMainMapPlayerController>(GetOwningPlayer())) PC->GoToUIMode();
}
void UEmployeeEquipmentPanelWidget::NativeOnDeactivated()
{
	if (Workspace) { Workspace->DismissDialog(); Workspace->SuspendPreview(); }
	// 닫기 버튼이든 뒤로가기든 스택 pop 이든 여기로 모인다 — 띄운 쪽은 이 한 신호만 들으면 된다.
	OnPanelClosed.ExecuteIfBound();
	if (auto* PC = Cast<AMainMapPlayerController>(GetOwningPlayer())) if (PC->GetCurrentInputMode() == EInputMode::UI) PC->GoToNormalMode();
	Super::NativeOnDeactivated();
}
void UEmployeeEquipmentPanelWidget::OpenWithContext(int32 EmployeeId, EEquipmentWorkspaceView View)
{
	ContextEmployeeId = EmployeeId; InitialView = View;
	SetupPartRail();
	EnsureWorkspace();
	if (Workspace) Workspace->Configure(EmployeeId, View);
	RefreshList();
}
void UEmployeeEquipmentPanelWidget::HandleClose() { CloseWithAnimation(); }

namespace
{
	// 부위 글리프. 이름은 EEmployeeEquipmentSlot 순서(Head..Back)를 그대로 따른다.
	UTexture2D* PartIcon(EEmployeeEquipmentSlot Slot)
	{
		static const TCHAR* Names[] = {TEXT("Head"), TEXT("Eyewear"), TEXT("Outfit"), TEXT("Hands"), TEXT("Feet"), TEXT("Back")};
		const int32 Index = static_cast<int32>(Slot);
		if (Index < 0 || Index >= UE_ARRAY_COUNT(Names)) return nullptr;
		const FString Path = FString::Printf(
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/EquipmentPart/T_UIIcon_EquipPart_%s.T_UIIcon_EquipPart_%s"), Names[Index], Names[Index]);
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous();
	}
}

void UEmployeeEquipmentPanelWidget::SetupPartRail()
{
	if (!HasOwnList() || PartGroup) return;
	UButtonWidget* Rail[] = {PartFilterAll, PartFilter0, PartFilter1, PartFilter2, PartFilter3, PartFilter4, PartFilter5};
	const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	PartGroup = NewObject<UCommonButtonGroupBase>(this);
	PartGroup->SetSelectionRequired(true);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rail); ++Index)
	{
		UButtonWidget* Button = Rail[Index];
		Button->SetIsSelectable(true);
		Button->SetIsToggleable(false);
		PartGroup->AddWidget(Button);

		UIconWithButtonWidget* Tab = Cast<UIconWithButtonWidget>(Button);
		if (Index == 0)
		{
			Button->SetButtonText(NSLOCTEXT("EquipmentPanel", "PartAll", "전체"));
			if (Tab) Tab->SetIconVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		// 부위 칸은 글리프만 — 목업 그대로다(라벨은 넣지 않는다).
		const EEmployeeEquipmentSlot PartSlot = static_cast<EEmployeeEquipmentSlot>(Index - 1);
		Button->SetButtonText(FText());
		if (!Tab) continue;
		Tab->SetIcon(PartIcon(PartSlot));
		Tab->SetIconVisibility(ESlateVisibility::HitTestInvisible);
	}
	PartGroup->OnButtonBaseClicked.AddDynamic(this, &ThisClass::HandlePartSelected);
	PartGroup->SelectButtonAtIndex(0, false);
}

void UEmployeeEquipmentPanelWidget::HandlePartSelected(UCommonButtonBase* Button, int32 Index)
{
	if (Index < 0) return;
	bAllSlots = (Index == 0);
	if (!bAllSlots) CurrentSlot = static_cast<EEmployeeEquipmentSlot>(Index - 1);
	RefreshList();
}

void UEmployeeEquipmentPanelWidget::RefreshList()
{
	if (!InventoryView) return;
	auto* Equipment = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEmployeeEquipmentManagerSubsystem>() : nullptr;
	const auto* Tables = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!Equipment || !Tables) return;

	TArray<UEmployeeEquipmentItemObject*> Entries;
	for (const auto& Pair : Equipment->GetState().Instances)
	{
		const auto* Def = Tables->FindEmployeeEquipmentDefinition(Pair.Value.DefinitionId);
		if (!Def) continue;
		if (!bAllSlots && Def->EquipmentSlot != CurrentSlot) continue;
		auto* Entry = NewObject<UEmployeeEquipmentItemObject>(this);
		Entry->InstanceID = Pair.Key;
		Entry->DefinitionID = Pair.Value.DefinitionId;
		Entry->Grade = Def->Grade;
		Entry->VisualGrade = Def->Grade;
		Entry->EnhancementLevel = Pair.Value.EnhanceLevel;
		Entry->bLocked = Pair.Value.bLocked;
		if (const auto* GradeRow = Tables->FindEmployeeEquipmentGrade(Def->Grade))
		{
			Entry->GradeName = GradeRow->DisplayName;
			Entry->GradeColor = GradeRow->Color;
		}
		Entries.Add(Entry);
	}
	// 등급 내림차순 → 강화 내림차순. EEmployeeEquipmentGrade 는 Common(0) → Ascendant(5) 다.
	Entries.Sort([](const UEmployeeEquipmentItemObject& A, const UEmployeeEquipmentItemObject& B)
	{
		if (A.Grade != B.Grade) return A.Grade > B.Grade;
		return A.EnhancementLevel > B.EnhancementLevel;
	});

	TArray<UObject*> List;
	List.Reserve(Entries.Num());
	for (auto* Entry : Entries) List.Add(Entry);
	// 보유분만 — 빈 칸은 두지 않는다.
	InventoryView->SetListItems(List);
}
