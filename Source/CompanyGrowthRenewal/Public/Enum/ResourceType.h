#pragma once


#include "CoreMinimal.h"
#include "ResourceType.generated.h"

UENUM()
enum class EResourceType : uint8
{
	None UMETA(DisplayName = "None"),
	Brick UMETA(DisplayName = "Brick"),
	Money UMETA(DisplayName = "Money"),
	Diamond UMETA(DisplayName = "Diamond"),
	Employee UMETA(DisplayName = "Employee"),
	Building UMETA(DisplayName = "Building"),
	MarketCap UMETA(DisplayName = "MarketCap"),
	Box UMETA(DisplayName = "Box"),
	// ── 채광 원자재 (WorldMap Mining System) ──
	IronOre UMETA(DisplayName = "IronOre"),
	Aluminum UMETA(DisplayName = "Aluminum"),
	Copper UMETA(DisplayName = "Copper"),
	Oil UMETA(DisplayName = "Oil"),
	RareEarth UMETA(DisplayName = "RareEarth"),
	Gold UMETA(DisplayName = "Gold"),
	Silicon UMETA(DisplayName = "Silicon"),
	Wood UMETA(DisplayName = "Wood"),
	Lithium UMETA(DisplayName = "Lithium"),
	DiamondOre UMETA(DisplayName = "DiamondOre"),
	Count UMETA(Hidden)
};

inline FString EnumToString(EResourceType Value)
{
	const UEnum* EnumPtr = StaticEnum<EResourceType>();
	if (!EnumPtr)
	{
		return FString("Invalid");
	}
	return EnumPtr->GetNameStringByIndex(static_cast<uint8>(Value));
}
