#include "Utils/EmployeeEquipmentAppearanceResolver.h"

namespace
{
bool IsAllowedPart(EEmployeeEquipmentSlot EquipmentSlot, EFunnyPartSlot PartSlot)
{
    switch (EquipmentSlot)
    {
    case EEmployeeEquipmentSlot::Head: return PartSlot == EFunnyPartSlot::Headwear;
    case EEmployeeEquipmentSlot::Eyewear: return PartSlot == EFunnyPartSlot::Glasses;
    case EEmployeeEquipmentSlot::Outfit: return PartSlot == EFunnyPartSlot::Outerwear || PartSlot == EFunnyPartSlot::Pants;
    case EEmployeeEquipmentSlot::Hands: return PartSlot == EFunnyPartSlot::Glove;
    case EEmployeeEquipmentSlot::Feet: return PartSlot == EFunnyPartSlot::Shoe;
    case EEmployeeEquipmentSlot::Back: return PartSlot == EFunnyPartSlot::Back;
    default: return false;
    }
}
}

FString FEmployeeEquipmentAppearanceResolver::BuildKey(const FEmployeeEquipmentState& State,
    const TMap<FName, FEmployeeEquipmentDefinitionRow>& Definitions,
    const TMap<FName, FEmployeeEquipmentAppearanceRow>& Appearances, int32 EmployeeId)
{
    FString Key = TEXT("fx3;");
    const FEmployeeEquipmentLoadout* Loadout = State.Loadouts.Find(EmployeeId);
    const FEmployeeEquipmentAppearanceSelections* Selections = State.AppearanceSelections.Find(EmployeeId);
    for (int32 Index = 0; Index < static_cast<int32>(EEmployeeEquipmentSlot::Count); ++Index)
    {
        const EEmployeeEquipmentSlot EquipmentSlot = static_cast<EEmployeeEquipmentSlot>(Index);
        const FEquipmentAppearanceSelection* Selection = Selections ? Selections->Slots.Find(EquipmentSlot) : nullptr;
        const EEquipmentAppearanceMode Mode = Selection ? Selection->Mode : EEquipmentAppearanceMode::UseEquipment;
        FName AppearanceId = Selection ? Selection->AppearanceId : NAME_None;
        if (Mode == EEquipmentAppearanceMode::UseEquipment)
        {
            AppearanceId = NAME_None;
            const FGuid* InstanceId = Loadout ? Loadout->Slots.Find(EquipmentSlot) : nullptr;
            const FEmployeeEquipmentInstance* Instance = InstanceId ? State.Instances.Find(*InstanceId) : nullptr;
            const FEmployeeEquipmentDefinitionRow* Definition = Instance ? Definitions.Find(Instance->DefinitionId) : nullptr;
            if (Definition) { AppearanceId = Definition->AppearanceId; }
        }
        if (Mode == EEquipmentAppearanceMode::Hidden) { AppearanceId = NAME_None; }
        const FEmployeeEquipmentAppearanceRow* Appearance = AppearanceId.IsNone() ? nullptr : Appearances.Find(AppearanceId);
        const int32 VisualGrade = Appearance ? static_cast<int32>(Appearance->VisualGrade) : INDEX_NONE;
        Key += FString::Printf(TEXT("%d:%s:%d;"), static_cast<int32>(Mode), *AppearanceId.ToString(), VisualGrade);
    }
    return Key;
}

bool FEmployeeEquipmentAppearanceResolver::Resolve(const TArray<FEquipmentAppearanceInput>& Inputs,
    FResolvedEquipmentAppearance& OutAppearance, FString& OutError)
{
    OutAppearance = {};
    OutError.Reset();
    FResolvedEquipmentAppearance Candidate;
    TSet<EEmployeeEquipmentSlot> SeenSlots;
    TSet<EFunnyPartSlot> SeenParts;
    for (const FEquipmentAppearanceInput& Input : Inputs)
    {
        if (Input.EquipmentSlot >= EEmployeeEquipmentSlot::Count || SeenSlots.Contains(Input.EquipmentSlot))
        {
            OutError = TEXT("DuplicateEquipmentSlot");
            return false;
        }
        SeenSlots.Add(Input.EquipmentSlot);
        if (Input.Mode == EEquipmentAppearanceMode::Hidden)
        {
            switch (Input.EquipmentSlot)
            {
            case EEmployeeEquipmentSlot::Head: Candidate.HiddenParts.Add(EFunnyPartSlot::Headwear); break;
            case EEmployeeEquipmentSlot::Eyewear: Candidate.HiddenParts.Add(EFunnyPartSlot::Glasses); break;
            case EEmployeeEquipmentSlot::Back: Candidate.HiddenParts.Add(EFunnyPartSlot::Back); break;
            default: OutError = TEXT("SlotCannotBeHidden"); return false;
            }
            continue;
        }
        const FEmployeeEquipmentAppearanceRow* Appearance = Input.Equipped;
        if (Input.Mode == EEquipmentAppearanceMode::Override)
        {
            if (!Input.bOverrideUnlocked || !Input.Override)
            {
                OutError = TEXT("AppearanceNotUnlocked");
                return false;
            }
            Appearance = Input.Override;
        }
        else if (Input.Mode != EEquipmentAppearanceMode::UseEquipment)
        {
            OutError = TEXT("InvalidAppearanceMode");
            return false;
        }
        if (!Appearance) { continue; }
        if (Appearance->VisualGrade >= EEmployeeEquipmentGrade::Count)
        {
            OutError = TEXT("InvalidAppearanceGrade");
            return false;
        }
        if (Appearance->EquipmentSlot != Input.EquipmentSlot || Appearance->PartOverrides.IsEmpty()
            || (Appearance->bHideHair && Input.EquipmentSlot != EEmployeeEquipmentSlot::Head))
        {
            OutError = TEXT("InvalidAppearanceSlot");
            return false;
        }
        for (const FEquipmentPartOverride& Part : Appearance->PartOverrides)
        {
            if (!IsAllowedPart(Input.EquipmentSlot, Part.PartSlot) || Part.PartRowId.IsNone()
                || SeenParts.Contains(Part.PartSlot))
            {
                OutError = TEXT("InvalidAppearancePart");
                return false;
            }
            SeenParts.Add(Part.PartSlot);
            Candidate.Parts.Add(Part);
            Candidate.VisualGrades.Add(Part.PartSlot, Appearance->VisualGrade);
        }
        if (Appearance->bHideHair) { Candidate.HiddenParts.Add(EFunnyPartSlot::Hair); }
    }
    OutAppearance = MoveTemp(Candidate);
    return true;
}
