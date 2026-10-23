#pragma once

#include "CoreMinimal.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"

// 디오라마 시험·렌더 탐침이 함께 쓰는 대표 사양(시안 PRESET/MPRESET 단계와 같은 값).
namespace OverseasDioramaTestSpecs
{
	inline FDioramaMineUnit Unit(EResourceType Resource, EDioramaSlotState State, float Fill)
	{
		FDioramaMineUnit U;
		U.Resource = Resource;
		U.State = State;
		U.Fill01 = Fill;
		return U;
	}

	inline FOverseasDioramaSpec Factory(ECountryType Country, ECompanyType Industry, TArray<EDioramaSlotState> Slots,
		int32 Automation, int32 Quality, int32 Storage, int32 Rush)
	{
		FOverseasDioramaSpec Spec;
		Spec.Country = Country;
		Spec.Factory.bPresent = true;
		Spec.Factory.Industry = Industry;
		Spec.Factory.Slots = MoveTemp(Slots);
		Spec.Factory.AutomationTier = Automation;
		Spec.Factory.QualityTier = Quality;
		Spec.Factory.StorageTier = Storage;
		Spec.Factory.RushTier = Rush;
		return Spec;
	}

	inline void AddMine(FOverseasDioramaSpec& Spec, TArray<FDioramaMineUnit> Units, int32 Rate, int32 Storage, int32 Offline, bool bAuto)
	{
		Spec.Mine.bPresent = true;
		Spec.Mine.Units = MoveTemp(Units);
		Spec.Mine.RateTier = Rate;
		Spec.Mine.StorageTier = Storage;
		Spec.Mine.OfflineTier = Offline;
		Spec.Mine.bAutoClaim = bAuto;
	}

	inline FOverseasDioramaSpec Korea4()
	{
		using S = EDioramaSlotState;
		return Factory(ECountryType::Korea, ECompanyType::Electronics, { S::Running, S::Completed, S::Empty, S::Running }, 2, 1, 1, 1);
	}

	inline FOverseasDioramaSpec Japan6()
	{
		using S = EDioramaSlotState;
		return Factory(ECountryType::Japan, ECompanyType::Electronics, { S::Completed, S::Running, S::Running, S::Completed, S::Running, S::Running }, 3, 2, 2, 2);
	}

	inline FOverseasDioramaSpec Usa3()
	{
		using S = EDioramaSlotState;
		FOverseasDioramaSpec Spec = Factory(ECountryType::USA, ECompanyType::Automobile, { S::Running, S::Running, S::Running, S::Empty }, 1, 1, 0, 0);
		AddMine(Spec, { Unit(EResourceType::Oil, S::Running, 0.5f), Unit(EResourceType::Oil, S::Empty, 0.f) }, 2, 1, 1, false);
		Spec.Port.bPresent = true;
		Spec.Port.CraneCount = 2;
		return Spec;
	}

	inline FOverseasDioramaSpec SaudiRefinery()
	{
		using S = EDioramaSlotState;
		FOverseasDioramaSpec Spec;
		Spec.Country = ECountryType::Saudi;
		TArray<FDioramaMineUnit> Units;
		for (int32 i = 0; i < 5; ++i)
		{
			Units.Add(Unit(EResourceType::Oil, i < 4 ? S::Running : S::Empty, 0.6f));
		}
		AddMine(Spec, MoveTemp(Units), 3, 3, 3, true);
		return Spec;
	}

	inline FOverseasDioramaSpec SingaporePort()
	{
		FOverseasDioramaSpec Spec;
		Spec.Country = ECountryType::Singapore;
		Spec.Port.bPresent = true;
		Spec.Port.CraneCount = 3;
		return Spec;
	}

	inline FOverseasDioramaSpec BrazilMine()
	{
		using S = EDioramaSlotState;
		FOverseasDioramaSpec Spec;
		Spec.Country = ECountryType::Brazil;
		AddMine(Spec, { Unit(EResourceType::IronOre, S::Running, 0.4f), Unit(EResourceType::Aluminum, S::Completed, 1.f),
			Unit(EResourceType::IronOre, S::Empty, 0.f) }, 2, 1, 1, false);
		return Spec;
	}
}
