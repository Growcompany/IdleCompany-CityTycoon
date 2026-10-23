#include "UI/Panel/OfficeMainWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "Core/CGGameInstance.h"

namespace
{
	TArray<int32> FindManualEmployees(UGameInstance* Instance)
	{
		TArray<int32> Result;
		const auto* Game = Cast<UCGGameInstance>(Instance);
		const auto* Equipment = Instance->GetSubsystem<UEmployeeEquipmentManagerSubsystem>();
		const auto* Tables = Instance->GetSubsystem<UTableManagerSubsystem>();
		const auto* Employees = Instance->GetSubsystem<UEmployeeManager>();
		if (!Game || !Equipment || !Tables || !Employees || !Tables->GetEmployeeEquipmentEconomy()) return Result;
		for (const auto& Employee : Employees->GetAllEmployees())
		{
			if (Employee.AssignedBuildingIndex != Game->GetCurrentManagedBuildingIndex()) continue;
			const auto Effects = FEmployeeEquipmentEffectEvaluator::BuildLoadout(Equipment->GetState(), Employee.EmployeeID, Tables->GetEmployeeEquipmentDefinitions(), Tables->GetEmployeeEquipmentEffects(), *Tables->GetEmployeeEquipmentEconomy());
			if (Effects.UniqueEffects.Contains(TEXT("EQF_MANUAL_PRIORITY"))) Result.Add(Employee.EmployeeID);
		}
		Result.Sort();
		return Result;
	}
}

void UOfficeMainWidget::RefreshEquipmentManualButton()
{
	if (!EquipmentManualButton) return;
	const auto* Stage = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	const bool bVisible = Stage && Stage->GetLifecycle() == EProjectLifecycle::Developing && !FindManualEmployees(GetGameInstance()).IsEmpty();
	EquipmentManualButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bVisible) CloseEquipmentManualDialog();
}

void UOfficeMainWidget::ShowEquipmentManualEmployees()
{
	const auto* Tables = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	auto* Employees = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const TArray<int32> Eligible = FindManualEmployees(GetGameInstance());
	if (!Tables || !Employees || Eligible.IsEmpty()) return;
	if (!EquipmentManualDialog)
	{
		const auto Class = Tables->GetWidgetClass(EWidgetType::EmployeeEquipmentDialog);
		if (!Class) return;
		EquipmentManualDialog = CreateWidget<UEmployeeEquipmentDialogWidget>(GetOwningPlayer(), Class);
		if (!EquipmentManualDialog) return;
		EquipmentManualDialog->AddToViewport(120);
	}
	FEmployeeEquipmentDialogView View;
	View.Title = FText::FromString(TEXT("직접 지시할 직원"));
	View.Notice = FText::FromString(TEXT("지시할 직원을 선택하세요. 개발은 계속 진행됩니다."));
	View.Primary = FText::FromString(TEXT("닫기"));
	EquipmentManualDialog->Configure(View);
	EquipmentManualDialog->OnClose.BindUObject(this, &ThisClass::CloseEquipmentManualDialog);
	EquipmentManualDialog->OnPrimary.BindUObject(this, &ThisClass::CloseEquipmentManualDialog);
	EquipmentManualDialog->OnItemClicked.BindUObject(this, &ThisClass::HandleEquipmentManualEmployee);
	EquipmentManualEntries.Empty();
	TArray<UObject*> List;
	for (int32 Id : Eligible)
	{
		auto* Entry = NewObject<UEmployeeEquipmentItemObject>(this);
		Entry->SelectionValue = Id;
		Entry->ChoiceLabel = FText::FromString(Employees->GetEmployeeFullName(Id) + TEXT("\n직접 지시 가능"));
		Entry->Icon = UEmployeeRosterCardWidget::LoadPortraitTexture(Id, FVector2D(160.f, 200.f));
		EquipmentManualEntries.Add(Entry); List.Add(Entry);
	}
	EquipmentManualDialog->ShowItems(List);
	EquipmentManualDialog->SetKeyboardFocus();
}

void UOfficeMainWidget::HandleEquipmentManualEmployee(UObject* Item)
{
	const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Item);
	const auto* Stage = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!Entry || !Stage || !EquipmentManualDialog) return;
	EquipmentManualEmployeeId = Entry->SelectionValue;
	FEmployeeEquipmentDialogView View;
	View.Title = FText::FromString(TEXT("집중할 직능 선택"));
	View.Notice = FText::FromString(TEXT("직능을 선택하면 직접 지시를 실행합니다."));
	View.Primary = FText::FromString(TEXT("취소"));
	EquipmentManualDialog->Configure(View);
	EquipmentManualDialog->OnItemClicked.BindUObject(this, &ThisClass::HandleEquipmentManualDiscipline);
	EquipmentManualEntries.Empty();
	TArray<UObject*> List;
	for (const auto& Discipline : Stage->GetEquipmentDisciplines())
	{
		const auto* Step = Stage->GetProgressData().Steps.FindByPredicate([&Discipline](const auto& Value) { return Value.DisciplineSlot == Discipline.DisciplineSlot; });
		if (!Step || Step->StepName.IsEmpty()) continue;
		auto* Choice = NewObject<UEmployeeEquipmentItemObject>(this);
		Choice->SelectionValue = Discipline.DisciplineSlot;
		Choice->ChoiceLabel = FText::FromString(FString::Printf(TEXT("%s\n%.0f / %.0f"), *Step->StepName, Discipline.Acquired, Discipline.Target));
		EquipmentManualEntries.Add(Choice); List.Add(Choice);
	}
	EquipmentManualDialog->ShowItems(List);
}

void UOfficeMainWidget::HandleEquipmentManualDiscipline(UObject* Item)
{
	const auto* Entry = Cast<UEmployeeEquipmentItemObject>(Item);
	auto* Stage = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
	if (!Entry || !Stage || !EquipmentManualDialog) return;
	if (Stage->RequestEquipmentManualPriority(EquipmentManualEmployeeId, Entry->SelectionValue)) CloseEquipmentManualDialog();
	else EquipmentManualDialog->SetNotice(FText::FromString(TEXT("지금 지시할 수 없습니다. 재사용 대기와 진행 중인 개발 상태를 확인하세요.")));
}

void UOfficeMainWidget::CloseEquipmentManualDialog()
{
	if (EquipmentManualDialog) { EquipmentManualDialog->RemoveFromParent(); EquipmentManualDialog = nullptr; }
	EquipmentManualEntries.Empty();
}
