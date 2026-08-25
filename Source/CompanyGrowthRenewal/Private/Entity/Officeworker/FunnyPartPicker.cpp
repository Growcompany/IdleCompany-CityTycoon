#include "Entity/Officeworker/FunnyPartPicker.h"

namespace
{
	// 슬롯별 솔트. 기존 StickOfficeworker 의 SaltSkin/SaltCloth 와 겹치지 않는 값을 쓴다.
	constexpr uint32 SaltBody    = 0x7FEB352Du;
	constexpr uint32 SaltOuter   = 0x846CA68Bu;
	constexpr uint32 SaltPants   = 0xD168AAADu;
	constexpr uint32 SaltShoe    = 0xAF251AF3u;
	constexpr uint32 SaltHairPk  = 0xB55A4F09u;
	constexpr uint32 SaltBrow    = 0x1B03738Cu;
	constexpr uint32 SaltGlasses = 0x4DD81482u;
}

uint32 FFunnyPartPicker::Hash(int32 EmployeeID, uint32 Salt)
{
	uint32 H = static_cast<uint32>(EmployeeID) ^ (Salt * 2654435761u);
	H ^= H >> 16; H *= 2246822519u;
	H ^= H >> 13; H *= 3266489917u;
	H ^= H >> 16;
	return H;
}

uint32 FFunnyPartPicker::SaltFor(EFunnyPartSlot PartSlot)
{
	switch (PartSlot)
	{
	case EFunnyPartSlot::Body:      return SaltBody;
	case EFunnyPartSlot::Outerwear: return SaltOuter;
	case EFunnyPartSlot::Pants:     return SaltPants;
	case EFunnyPartSlot::Shoe:      return SaltShoe;
	case EFunnyPartSlot::Hair:      return SaltHairPk;
	case EFunnyPartSlot::Eyebrow:   return SaltBrow;
	case EFunnyPartSlot::Glasses:   return SaltGlasses;
	default:                        return SaltBody;
	}
}

int32 FFunnyPartPicker::PickWeightedIndex(int32 EmployeeID, EFunnyPartSlot PartSlot, const TArray<int32>& Weights)
{
	int32 Total = 0;
	for (const int32 W : Weights)
	{
		Total += FMath::Max(0, W);
	}
	if (Total <= 0)
	{
		return INDEX_NONE;
	}

	const uint32 Roll = Hash(EmployeeID, SaltFor(PartSlot)) % static_cast<uint32>(Total);

	int32 Acc = 0;
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		Acc += FMath::Max(0, Weights[i]);
		if (static_cast<int32>(Roll) < Acc)
		{
			return i;
		}
	}
	return Weights.Num() - 1;   // 부동 오차 없는 정수 누적이라 도달 불가. 방어용.
}

bool FFunnyPartPicker::IsGenderMatch(EFunnyPartGender RowGender, EEmployeeGender Employee)
{
	if (RowGender == EFunnyPartGender::Any)
	{
		return true;
	}
	return (RowGender == EFunnyPartGender::Male) == (Employee == EEmployeeGender::Male);
}

bool FFunnyPartPicker::ExpressionOverridesBrow(EWorkerFaceExpression Expression)
{
	return Expression != EWorkerFaceExpression::Neutral;
}
