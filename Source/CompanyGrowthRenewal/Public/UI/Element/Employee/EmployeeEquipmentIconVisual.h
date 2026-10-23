#pragma once

#include "CoreMinimal.h"
#include "Enum/EmployeeEquipmentTypes.h"

class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;
class UTableManagerSubsystem;
class UIconWithButtonWidget;
struct FEmployeeEquipmentInstance;
struct FEmployeeEquipmentGradeRow;
struct FEmployeeEquipmentDefinitionRow;

enum class EEmployeeEquipmentIconContext : uint8 { Idle, Focused, Reveal };

namespace CGEquipmentIcon
{
	EEmployeeEquipmentGrade VisualGrade(const UTableManagerSubsystem* Tables, const FEmployeeEquipmentDefinitionRow& Definition);
	UObject* Resolve(UObject* Consumer, UTexture2D* Texture, const FEmployeeEquipmentGradeRow* Grade,
		EEmployeeEquipmentIconContext Context, TObjectPtr<UMaterialInstanceDynamic>& Material, bool bTriggerPulse = false);
	void TriggerPulse(UMaterialInstanceDynamic* Material);
	void Apply(UImage* Image, UObject* Resource);
	void ApplyGradeFrame(UImage* Image, const FEmployeeEquipmentGradeRow* Grade, const FLinearColor& Color);
	// bWearsBaseAppearance = 장비는 없지만 기본 파츠가 그 부위를 채우고 있음 (의상·신발·직급 안경).
	void ApplySlotButton(UObject* Consumer, UIconWithButtonWidget* Button, const UTableManagerSubsystem* Tables,
		EEmployeeEquipmentSlot Part, const FEmployeeEquipmentInstance* Item, TObjectPtr<UMaterialInstanceDynamic>& Material,
		bool bWearsBaseAppearance = false);
}
