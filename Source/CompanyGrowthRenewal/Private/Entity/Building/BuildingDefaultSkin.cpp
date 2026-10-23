// Fill out your copyright notice in the Description page of Project Settings.

#include "Entity/Building/BuildingDefaultSkin.h"

namespace CGBuildingDefaultSkin
{
	ELootBoxRarity BandForFootprint(int32 WidthCells, int32 DepthCells)
	{
		if (WidthCells <= 0 || DepthCells <= 0)
		{
			return ELootBoxRarity::Common;
		}

		const int64 CellCount = static_cast<int64>(WidthCells) * static_cast<int64>(DepthCells);
		switch (CellCount)
		{
		case 2: return ELootBoxRarity::Unusual;
		case 4: return ELootBoxRarity::Rare;
		case 9: return ELootBoxRarity::Epic;
		default: return ELootBoxRarity::Common;
		}
	}

	int32 FindDefaultSkinID(FName BuildingRowName)
	{
		const int32* SkinID = GetTable().Find(BuildingRowName);
		return SkinID ? *SkinID : INDEX_NONE;
	}

	const TMap<FName, int32>& GetTable()
	{
		static const TMap<FName, int32> Table = {
			{ FName(TEXT("B1")), 102 },
			{ FName(TEXT("B2")), 101 },
			{ FName(TEXT("B4")), 103 },
			{ FName(TEXT("B5")), 102 },
			{ FName(TEXT("B6")), 104 },
			{ FName(TEXT("B8")), 101 },
			{ FName(TEXT("B11")), 100 },
			{ FName(TEXT("B13")), 102 },
			{ FName(TEXT("B22")), 104 },
			{ FName(TEXT("B24")), 103 },
			{ FName(TEXT("B38")), 101 },
			{ FName(TEXT("B3")), 203 },
			{ FName(TEXT("B7")), 207 },
			{ FName(TEXT("B9")), 200 },
			{ FName(TEXT("B12")), 202 },
			{ FName(TEXT("B14")), 205 },
			{ FName(TEXT("B15")), 204 },
			{ FName(TEXT("B16")), 206 },
			{ FName(TEXT("B17")), 206 },
			{ FName(TEXT("B19")), 201 },
			{ FName(TEXT("B21")), 205 },
			{ FName(TEXT("B10")), 307 },
			{ FName(TEXT("B18")), 306 },
			{ FName(TEXT("B20")), 304 },
			{ FName(TEXT("B23")), 306 },
			{ FName(TEXT("B25")), 304 },
			{ FName(TEXT("B26")), 301 },
			{ FName(TEXT("B27")), 307 },
			{ FName(TEXT("B28")), 305 },
			{ FName(TEXT("B29")), 304 },
			{ FName(TEXT("B30")), 302 },
			{ FName(TEXT("B31")), 300 },
			{ FName(TEXT("B32")), 305 },
			{ FName(TEXT("B33")), 303 },
			{ FName(TEXT("B34")), 306 },
			{ FName(TEXT("B35")), 302 },
			{ FName(TEXT("B36")), 304 },
			{ FName(TEXT("B37")), 301 },
			{ FName(TEXT("B39")), 300 },
			{ FName(TEXT("B40")), 404 },
		};
		return Table;
	}
}
