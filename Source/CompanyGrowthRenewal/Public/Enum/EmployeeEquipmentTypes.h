#pragma once
#include "CoreMinimal.h"
#include "EmployeeEquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EEmployeeEquipmentSlot : uint8 { Head, Eyewear, Outfit, Hands, Feet, Back, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EEmployeeEquipmentGrade : uint8 { Common, Uncommon, Rare, Epic, Legendary, Ascendant, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EEquipmentAppearanceMode : uint8 { UseEquipment, Override, Hidden };
UENUM(BlueprintType)
enum class EEquipmentCommand : uint8 { Pull, ClaimStarter, SetSlot, SetAppearance, Enhance, Dismantle, SetLock, RecruitGift };
UENUM(BlueprintType)
enum class EEquipmentMaterialMode : uint8 { Core, Duplicate };
UENUM(BlueprintType)
enum class EEquipmentEffectValueDisplay : uint8 { Increase, Decrease, SpeedToInterval };
