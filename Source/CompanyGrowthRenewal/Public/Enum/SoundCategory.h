#pragma once

#include "CoreMinimal.h"
#include "SoundCategory.generated.h"

UENUM(BlueprintType)
enum class ESoundCategory : uint8
{
	Master UMETA(DisplayName = "Master Volume"),
	Music UMETA(DisplayName = "Background Music"),
	SFX UMETA(DisplayName = "Sound Effects"),
	UI UMETA(DisplayName = "UI Sounds"),
	Ambient UMETA(DisplayName = "Ambient Sounds")
};

inline FString EnumToString(ESoundCategory Value)
{
	const UEnum* EnumPtr = StaticEnum<ESoundCategory>();
	if (!EnumPtr)
	{
		return FString("Invalid");
	}
	return EnumPtr->GetNameStringByIndex(static_cast<uint8>(Value));
}
