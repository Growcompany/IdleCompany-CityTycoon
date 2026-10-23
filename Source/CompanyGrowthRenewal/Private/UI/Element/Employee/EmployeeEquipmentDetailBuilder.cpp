#include "UI/Element/Employee/EmployeeEquipmentDetailBuilder.h"
#include "Manager/EmployeeEquipmentManagerSubsystem.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/CompanyType.h"
#include "Engine/GameInstance.h"
#include "Utils/EmployeeEquipmentEffectEvaluator.h"
#include "UI/Element/Employee/EmployeeEquipmentCardWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "Manager/UIManagerSubsystem.h"
#include "Global/KoreanJosa.h"

#define LOCTEXT_NAMESPACE "EmployeeEquipmentDetail"

// 익명 네임스페이스에 두면 유니티 블롭이 합쳐질 때 같은 이름의 다른 정의와 부딪힌다
// (워크스페이스에 바이트 단위로 같은 사본이 있었다) — 공용으로 뺀다.
FString CGEquipmentDetail::Number(float Value, int32 MinDigits)
{
	FNumberFormattingOptions Format;
	Format.MinimumFractionalDigits = MinDigits;
	Format.MaximumFractionalDigits = 2;
	return FText::AsNumber(Value, &Format).ToString();
}

int32 CGEquipmentDetail::FractionDigits(float Value)
{
	if (!FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value * 10.f) / 10.f, KINDA_SMALL_NUMBER)) return 2;
	if (!FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value), KINDA_SMALL_NUMBER)) return 1;
	return 0;
}

float CGEquipmentDetail::DisplayValue(const FEmployeeEquipmentEffectRow& Effect, float RawValue)
{
	switch (Effect.ValueDisplay)
	{
	case EEquipmentEffectValueDisplay::Decrease:
		return -RawValue;
	case EEquipmentEffectValueDisplay::SpeedToInterval:
		return -100.f * RawValue / (100.f + RawValue);
	case EEquipmentEffectValueDisplay::Increase:
	default:
		return RawValue;
	}
}

FString CGEquipmentDetail::FormatEffectValue(const FEmployeeEquipmentEffectRow& Effect, float RawValue, int32 MinDigits)
{
	const float Value = DisplayValue(Effect, RawValue);
	const float Rounded = FMath::RoundToFloat(Value * 100.f) / 100.f;
	const FString Prefix = Rounded > 0.f ? TEXT("+") : Rounded < 0.f ? TEXT("-") : TEXT("");
	return Prefix + Number(FMath::Abs(Rounded), MinDigits) + Effect.Unit.ToString();
}

namespace
{
	using CGEquipmentDetail::Number;
	using CGEquipmentDetail::FractionDigits;

	FText NameWithEnhance(const FText& DisplayName, int32 EnhanceLevel)
	{
		return EnhanceLevel > 0
			? FText::Format(LOCTEXT("NameEnhanced", "{0} +{1}"), DisplayName, FText::AsNumber(EnhanceLevel))
			: DisplayName;
	}
}

bool CGEquipmentDetail::BuildContent(
	const UEmployeeEquipmentManagerSubsystem& Equipment,
	const UTableManagerSubsystem& Tables,
	UEmployeeManager* Employees,
	int32 EmployeeId,
	const FGuid& InstanceId,
	FEquipmentDetailContent& Out)
{
	const FEmployeeEquipmentInstance* Item = Equipment.FindInstance(InstanceId);
	const FEmployeeEquipmentDefinitionRow* Def = Item ? Tables.FindEmployeeEquipmentDefinition(Item->DefinitionId) : nullptr;
	if (!Def) return false;

	const FEmployeeEquipmentGradeRow* Grade = Tables.FindEmployeeEquipmentGrade(Def->Grade);
	const FEmployeeEquipmentSlotRow* Part = Tables.FindEmployeeEquipmentSlot(Def->EquipmentSlot);
	const int32 HolderId = Equipment.GetInstanceEmployee(InstanceId);
	const bool bEquippedHere = HolderId == EmployeeId && HolderId >= 0;
	const bool bHasEmployee = Employees && Employees->GetEmployeeData(EmployeeId) != nullptr;
	bool bManufacturingEmployee = false;
	if (bHasEmployee)
	{
		const FEmployeeInstance* EmployeeData = Employees->GetEmployeeData(EmployeeId);
		if (EmployeeData && EmployeeData->bIsAssigned && EmployeeData->AssignedBuildingIndex >= 0)
		{
			if (UGameInstance* GameInstance = Employees->GetGameInstance())
			{
				if (USaveLoadManager* Saves = GameInstance->GetSubsystem<USaveLoadManager>())
				{
					bManufacturingEmployee = IsManufacturingType(Saves->GetBuildingCompanyType(EmployeeData->AssignedBuildingIndex));
				}
			}
		}
	}

	Out = FEquipmentDetailContent();
	Out.Name = NameWithEnhance(Def->DisplayName, Item->EnhanceLevel);
	Out.GradeName = Grade ? FText::Format(LOCTEXT("GradeSuffix", "{0} 등급"), Grade->DisplayName) : FText();
	Out.PartName = Part ? Part->DisplayName : FText();
	Out.EnhanceText = Item->EnhanceLevel > 0
		? FText::Format(LOCTEXT("EnhanceLabel", "강화 +{0}"), FText::AsNumber(Item->EnhanceLevel)) : FText();
	Out.State = bEquippedHere
		? LOCTEXT("StateEquipped", "현재 장착 중")
		: HolderId >= 0
			? Employees && Employees->GetEmployeeData(HolderId)
				? FText::FromString(Employees->GetEmployeeFullName(HolderId) + TEXT(" 착용 중"))
				: LOCTEXT("StateEquippedElsewhere", "다른 직원이 장착 중")
			: LOCTEXT("StateUnequipped", "미장착");
	Out.bHeldByOther = HolderId >= 0 && !bEquippedHere;
	if (!Def->Icon.IsNull()) Out.Icon = Def->Icon.LoadSynchronous();
	// 등급 테두리 재질은 행이 있어야 켜진다 ― 색만 넘기면 ApplyGradeFrame 이 통째로 접는다.
	Out.GradeRow = Grade;
	Out.VisualGradeRow = Tables.FindEmployeeEquipmentGrade(CGEquipmentIcon::VisualGrade(&Tables, *Def));
	Out.GradeColor = Grade ? Grade->Color : FLinearColor::Transparent;
	Out.Stars = static_cast<int32>(Def->Grade) + 1;

	// 교체 대상 = 이 부위에 지금 낀 것. 이미 그걸 고른 경우엔 비교할 상대가 없다.
	const FEmployeeEquipmentInstance* Equipped = Equipment.GetEquippedInstance(EmployeeId, Def->EquipmentSlot);
	const FEmployeeEquipmentDefinitionRow* EquippedDef =
		Equipped ? Tables.FindEmployeeEquipmentDefinition(Equipped->DefinitionId) : nullptr;
	Out.bShowComparison = bHasEmployee && !bEquippedHere;
	Out.bShowSwap = Out.bShowComparison;
	if (Out.bShowComparison)
	{
		Out.CurrentLabel = FText::Format(LOCTEXT("CurrentSlot", "현재 {0}"), Part ? Part->DisplayName : FText());
		Out.CurrentName = EquippedDef
			? NameWithEnhance(EquippedDef->DisplayName, Equipped->EnhanceLevel)
			: LOCTEXT("EmptySlot", "빈 슬롯");
		if (EquippedDef)
		{
			if (!EquippedDef->Icon.IsNull()) Out.CurrentIcon = EquippedDef->Icon.LoadSynchronous();
			Out.CurrentVisualGradeRow = Tables.FindEmployeeEquipmentGrade(CGEquipmentIcon::VisualGrade(&Tables, *EquippedDef));
			if (const FEmployeeEquipmentGradeRow* EquippedGrade = Tables.FindEmployeeEquipmentGrade(EquippedDef->Grade))
				Out.CurrentGradeColor = EquippedGrade->Color;
		}
		Out.AfterLabel = LOCTEXT("After", "변경 후");
		Out.AfterName = Out.Name;
	}

	const FEmployeeEquipmentEconomyRow* Economy = Tables.GetEmployeeEquipmentEconomy();
	Out.BodyLabel = LOCTEXT("EquipChanges", "장착 시 변화");
	Out.ItemNoEffectsText = LOCTEXT("NoEffects", "기본 수치 효과 없음");
	Out.NoChangeText = LOCTEXT("NoChange", "장비 합산 변화 없음");
	Out.Notice = bHasEmployee ? FText() : LOCTEXT("NoEmployee", "장착할 직원이 없습니다");
	const auto AppendNotice = [&Out](const FText& Notice)
	{
		if (Notice.IsEmpty()) return;
		Out.Notice = Out.Notice.IsEmpty() ? Notice
			: FText::Format(LOCTEXT("NoticeLines", "{0}\n{1}"), Out.Notice, Notice);
	};
	bool bItemEffectsValid = false;
	if (Economy)
	{
		const FEquipmentEffectLoadout ItemEffects = FEmployeeEquipmentEffectEvaluator::BuildItemEffects(
			*Def, Item->EnhanceLevel, Tables.GetEmployeeEquipmentEffects(), *Economy);
		bItemEffectsValid = ItemEffects.bValid;
		if (!bItemEffectsValid)
		{
			Out.ItemNoEffectsText = LOCTEXT("InvalidItemEffects", "장비 효과를 확인할 수 없습니다");
			AppendNotice(LOCTEXT("InvalidItemNotice", "선택한 장비 데이터가 유효하지 않아 효과가 적용되지 않습니다."));
		}
		if (bItemEffectsValid)
		{
			TArray<FName> Keys;
			ItemEffects.BasicValues.GetKeys(Keys);
			Keys.Sort(FNameLexicalLess());
			for (FName Key : Keys)
			{
				const FEmployeeEquipmentEffectRow* Effect = Tables.FindEmployeeEquipmentEffect(Key);
				if (!Effect) continue;
				FEquipmentDeltaRow& DeltaRow = Out.ItemEffectRows.AddDefaulted_GetRef();
				DeltaRow.Name = Effect->DisplayName;
				DeltaRow.After = FText::FromString(FormatEffectValue(*Effect, ItemEffects.BasicValues[Key]));
			}
			FString UniqueDescriptions;
			for (FName EffectId : Def->UniqueEffectIds)
			{
				const FEmployeeEquipmentEffectRow* Effect = ItemEffects.UniqueEffects.Find(EffectId);
				if (!Effect) continue;
				if (!UniqueDescriptions.IsEmpty()) UniqueDescriptions += TEXT("\n\n");
				if (!Effect->DisplayName.IsEmpty()) UniqueDescriptions += Effect->DisplayName.ToString();
				const FText& Description = bManufacturingEmployee ? Effect->ManufacturingDescription : Effect->Description;
				if (bManufacturingEmployee && Description.IsEmpty())
				{
				UE_LOG(LogTemp, Warning, TEXT("[EmployeeEquipmentDetail] Missing manufacturing description: %s"), *EffectId.ToString());
				}
				if (!Description.IsEmpty())
				{
					if (!Effect->DisplayName.IsEmpty()) UniqueDescriptions += TEXT("\n");
					UniqueDescriptions += Description.ToString();
				}
			}
			Out.ItemUniqueEffects = FText::FromString(UniqueDescriptions);
		}
		if (bEquippedHere && bItemEffectsValid)
		{
			const FEquipmentEffectLoadout Current = FEmployeeEquipmentEffectEvaluator::BuildLoadout(
				Equipment.GetState(), EmployeeId, Tables.GetEmployeeEquipmentDefinitions(),
				Tables.GetEmployeeEquipmentEffects(), *Economy);
			if (!Current.bValid)
				AppendNotice(LOCTEXT("InvalidCurrentLoadout", "현재 장착 구성에 유효하지 않은 장비가 있어 장비 효과가 적용되지 않습니다."));
		}
		else if (Out.bShowComparison)
		{
			FEquipmentEffectLoadout Before = FEmployeeEquipmentEffectEvaluator::BuildLoadout(
				Equipment.GetState(), EmployeeId, Tables.GetEmployeeEquipmentDefinitions(),
				Tables.GetEmployeeEquipmentEffects(), *Economy);
			FEquipmentEffectLoadout After = FEmployeeEquipmentEffectEvaluator::BuildLoadout(
				Equipment.GetState(), EmployeeId, Tables.GetEmployeeEquipmentDefinitions(),
				Tables.GetEmployeeEquipmentEffects(), *Economy, Def->EquipmentSlot, InstanceId);
			// 실제 업무는 유효하지 않은 구성의 효과 전체를 적용하지 않는다.
			if (!Before.bValid)
			{
				Before.BasicValues.Reset();
				Before.UniqueEffects.Reset();
			}
			if (!After.bValid)
			{
				After.BasicValues.Reset();
				After.UniqueEffects.Reset();
				if (bItemEffectsValid) Out.NoChangeText = LOCTEXT("InvalidLoadoutEffects", "적용되는 장비 효과 없음");
				AppendNotice(LOCTEXT("InvalidAfterLoadout", "변경 후 장착 구성에 유효하지 않은 장비가 있어 장비 효과가 적용되지 않습니다."));
			}
			else if (!Before.bValid)
			{
				AppendNotice(LOCTEXT("RestoredLoadout", "현재 장비 효과는 적용되지 않으며, 교체하면 표시된 효과가 적용됩니다."));
			}

			TSet<FName> KeySet;
			for (const auto& Pair : Before.BasicValues) KeySet.Add(Pair.Key);
			for (const auto& Pair : After.BasicValues) KeySet.Add(Pair.Key);
			TArray<FName> Keys = KeySet.Array();
			Keys.Sort(FNameLexicalLess());
			for (FName Key : Keys)
			{
				const FEmployeeEquipmentEffectRow* Effect = Tables.FindEmployeeEquipmentEffect(Key);
				if (!Effect) continue;
				const float BeforeValue = Before.BasicValues.FindRef(Key);
				const float AfterValue = After.BasicValues.FindRef(Key);
				if (FMath::IsNearlyEqual(BeforeValue, AfterValue)) continue;
				const int32 Digits = FMath::Max(FractionDigits(DisplayValue(*Effect, BeforeValue)), FractionDigits(DisplayValue(*Effect, AfterValue)));
				FEquipmentDeltaRow& DeltaRow = Out.Rows.AddDefaulted_GetRef();
				DeltaRow.Name = Effect->DisplayName;
				DeltaRow.Before = FText::FromString(FormatEffectValue(*Effect, BeforeValue, Digits));
				DeltaRow.After = FText::FromString(FormatEffectValue(*Effect, AfterValue, Digits));
				DeltaRow.bUp = AfterValue > BeforeValue;
			}
			for (const auto& Pair : Before.UniqueEffects)
				if (!After.UniqueEffects.Contains(Pair.Key)) Out.Uniques.Add({Pair.Value.DisplayName, false});
			for (const auto& Pair : After.UniqueEffects)
				if (!Before.UniqueEffects.Contains(Pair.Key)) Out.Uniques.Add({Pair.Value.DisplayName, true});
		}
	}
	else
	{
		Out.ItemNoEffectsText = LOCTEXT("InvalidItemEffects", "장비 효과를 확인할 수 없습니다");
		AppendNotice(LOCTEXT("MissingEquipmentEconomy", "장비 효과 정보를 불러올 수 없습니다."));
	}

	const bool bMaxEnhance = Economy && Item->EnhanceLevel >= Economy->MaxEnhanceLevel;
	Out.Primary = bEquippedHere
		? LOCTEXT("Unequip", "해제")
		: Out.bHeldByOther ? LOCTEXT("TakeOver", "가져와 장착")
		: Equipped ? LOCTEXT("Replace", "교체") : LOCTEXT("Equip", "장착");
	Out.Secondary = bMaxEnhance ? LOCTEXT("MaxEnhance", "최대") : LOCTEXT("Enhance", "강화");
	Out.bPrimaryEnabled = bHasEmployee && (bEquippedHere || bItemEffectsValid);
	Out.bSecondaryEnabled = bHasEmployee && bItemEffectsValid && !bMaxEnhance;
	return true;
}

void CGEquipmentDetail::ShowTransferConfirm(UGameInstance& GameInstance, int32 FromEmployeeId, int32 ToEmployeeId,
	const FEmployeeEquipmentDefinitionRow& Definition, FSimpleDelegate OnConfirm)
{
	UEmployeeManager* Employees = GameInstance.GetSubsystem<UEmployeeManager>();
	UUIManagerSubsystem* UIMgr = GameInstance.GetSubsystem<UUIManagerSubsystem>();
	const UTableManagerSubsystem* Tables = GameInstance.GetSubsystem<UTableManagerSubsystem>();
	const FEmployeeEquipmentSlotRow* Part = Tables ? Tables->FindEmployeeEquipmentSlot(Definition.EquipmentSlot) : nullptr;
	if (!Employees || !UIMgr || !Part)
	{
		OnConfirm.ExecuteIfBound();
		return;
	}
	const FString From = Employees->GetEmployeeFullName(FromEmployeeId);
	const FString To = Employees->GetEmployeeFullName(ToEmployeeId);
	const FText Message = FText::Format(
		LOCTEXT("TransferAsk", "{0} 끼고 있는 장비입니다.\n{1}에게 옮겨 장착할까요?\n{2}의 {3} 칸은 비게 됩니다."),
		FText::FromString(KoreanJosa::Attach(From, TEXT("가"), TEXT("이"))), FText::FromString(To),
		FText::FromString(From), Part->DisplayName);
	if (UConfirmCancelWidget* Dialog = UIMgr->ShowConfirmDialog(LOCTEXT("TransferTitle", "장비 가져오기"), Message, OnConfirm))
	{
		Dialog->SetConfirmButtonText(LOCTEXT("TransferConfirm", "옮겨 장착"));
		Dialog->SetCancelButtonText(LOCTEXT("TransferCancel", "취소"));
	}
	else
	{
		OnConfirm.ExecuteIfBound();
	}
}

#undef LOCTEXT_NAMESPACE
UEmployeeEquipmentItemObject* CGEquipmentEntry::Make(
	UObject* Outer,
	const UTableManagerSubsystem& Tables,
	const FEmployeeEquipmentDefinitionRow& Definition,
	const FEmployeeEquipmentInstance* Instance)
{
	auto* Entry = NewObject<UEmployeeEquipmentItemObject>(Outer);
	Entry->DefinitionID = Definition.DefinitionId;
	Entry->Grade = Definition.Grade;
	// 표시 등급은 실제 등급과 다를 수 있다(외형 승급) — 둘을 같게 두면 아이콘 틀이 틀린다.
	Entry->VisualGrade = CGEquipmentIcon::VisualGrade(&Tables, Definition);
	if (!Definition.Icon.IsNull()) Entry->Icon = Definition.Icon.LoadSynchronous();
	if (const auto* Grade = Tables.FindEmployeeEquipmentGrade(Definition.Grade))
	{
		Entry->GradeName = Grade->DisplayName;
		Entry->GradeColor = Grade->Color;
	}
	if (Instance)
	{
		Entry->InstanceID = Instance->InstanceId;
		Entry->EnhancementLevel = Instance->EnhanceLevel;
		Entry->bLocked = Instance->bLocked;
	}
	return Entry;
}
