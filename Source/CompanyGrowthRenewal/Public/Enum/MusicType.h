#pragma once

#include "CoreMinimal.h"
#include "MusicType.generated.h"

UENUM(BlueprintType)
enum class EMusicType : uint8
{
	None UMETA(DisplayName = "None"),
	Main UMETA(DisplayName = "Main Map"),
	Upgrade UMETA(DisplayName = "Upgrade"),
	WorldMap UMETA(DisplayName = "World Map"),
	OfficeGame UMETA(DisplayName = "Office - Game"),
	OfficeElectronics UMETA(DisplayName = "Office - Electronics"),
	OfficeFinance UMETA(DisplayName = "Office - Finance"),
	OfficeIT UMETA(DisplayName = "Office - IT"),
	OfficeSemiconductor UMETA(DisplayName = "Office - Semiconductor"),
	OfficeAutomobile UMETA(DisplayName = "Office - Automobile")
};

inline FString EnumToString(EMusicType Value)
{
	const UEnum* EnumPtr = StaticEnum<EMusicType>();
	if (!EnumPtr)
	{
		return FString("Invalid");
	}
	return EnumPtr->GetNameStringByIndex(static_cast<uint8>(Value));
}
