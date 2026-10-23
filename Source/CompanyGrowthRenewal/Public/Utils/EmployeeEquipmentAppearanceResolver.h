#pragma once

#include "CoreMinimal.h"
#include "Table/EmployeeEquipmentTable.h"
#include "Data/EmployeeEquipmentData.h"

struct FEquipmentAppearanceInput
{
    EEmployeeEquipmentSlot EquipmentSlot = EEmployeeEquipmentSlot::Head;
    EEquipmentAppearanceMode Mode = EEquipmentAppearanceMode::UseEquipment;
    const FEmployeeEquipmentAppearanceRow* Equipped = nullptr;
    const FEmployeeEquipmentAppearanceRow* Override = nullptr;
    bool bOverrideUnlocked = false;
};

struct FResolvedEquipmentAppearance
{
    TArray<FEquipmentPartOverride> Parts;
    TSet<EFunnyPartSlot> HiddenParts;
    TMap<EFunnyPartSlot, EEmployeeEquipmentGrade> VisualGrades;
};

struct COMPANYGROWTHRENEWAL_API FEmployeeEquipmentAppearanceResolver
{
	static FString BuildKey(const FEmployeeEquipmentState& State,
		const TMap<FName, FEmployeeEquipmentDefinitionRow>& Definitions,
        const TMap<FName, FEmployeeEquipmentAppearanceRow>& Appearances, int32 EmployeeId);
    // 전체 입력을 검증한 뒤 결과를 공개해 부분 적용을 막는다.
    static bool Resolve(const TArray<FEquipmentAppearanceInput>& Inputs,
        FResolvedEquipmentAppearance& OutAppearance, FString& OutError);
};
