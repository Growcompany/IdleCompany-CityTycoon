#include "Entity/OverseasDiorama/DioramaModules.h"

FColor DioramaModules::Hex(uint32 RGB)
{
	return FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 0xFF);
}

uint32 DioramaPalette::ResourceColor(EResourceType Resource)
{
	switch (Resource)
	{
	case EResourceType::IronOre: return 0x8C5A44;
	case EResourceType::Aluminum: return 0xBFC6CE;
	case EResourceType::Gold: return 0xE3B54A;
	case EResourceType::RareEarth: return 0x8E7BB5;
	case EResourceType::Oil: return 0x2B2F36;
	case EResourceType::Wood: return 0xA8743F;
	default: return 0x8E6A45;
	}
}
