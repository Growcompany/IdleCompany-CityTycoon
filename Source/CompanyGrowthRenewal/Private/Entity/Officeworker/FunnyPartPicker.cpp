#include "Entity/Officeworker/FunnyPartPicker.h"

namespace
{
	// 슬롯별 솔트. 기존 FunnyOfficeworker 의 SaltSkin/SaltCloth 와 겹치지 않는 값을 쓴다.
	constexpr uint32 SaltBody    = 0x7FEB352Du;
	constexpr uint32 SaltOuter   = 0x846CA68Bu;
	constexpr uint32 SaltPants   = 0xD168AAADu;
	constexpr uint32 SaltShoe    = 0xAF251AF3u;
	constexpr uint32 SaltHairPk  = 0xB55A4F09u;
	constexpr uint32 SaltBrow    = 0x1B03738Cu;
	constexpr uint32 SaltGlasses = 0x4DD81482u;
	constexpr uint32 SaltMustache = 0x5BD1E995u;
	constexpr uint32 SaltGlove    = 0x2545F491u;

	// 얼굴 판 개인 차이·깜빡임 박자. 위 슬롯 솔트·FunnyOfficeworker 솔트와 겹치지 않는 값.
	constexpr uint32 SaltFaceEyeScale    = 0x94D049BBu;
	constexpr uint32 SaltFaceEyeOffset   = 0xBF58476Du;
	constexpr uint32 SaltFaceMouthOffset = 0x632BE59Bu;
	constexpr uint32 SaltFaceBlink       = 0xE7037ED1u;
	constexpr uint32 SaltFaceStyle       = 0xA54FF53Au;

	// 해시 하위 24비트 → [0,1). float 가수부 안이라 플랫폼과 무관하게 같은 값이 나온다.
	float FunnyFaceUnit(int32 EmployeeID, uint32 Salt)
	{
		return static_cast<float>(FFunnyPartPicker::Hash(EmployeeID, Salt) & 0x00FFFFFFu) / 16777216.f;
	}
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
	case EFunnyPartSlot::Mustache:  return SaltMustache;
	case EFunnyPartSlot::Glove:     return SaltGlove;
	case EFunnyPartSlot::Headwear:  return 0x9E3779B9u;
	case EFunnyPartSlot::Back:      return 0xA24BAED5u;
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

FFunnyFaceVariation FFunnyPartPicker::PickFaceVariation(int32 EmployeeID)
{
	FFunnyFaceVariation Out;
	Out.EyeScale     = FMath::Lerp(0.92f, 1.08f, FunnyFaceUnit(EmployeeID, SaltFaceEyeScale));
	Out.EyeOffsetY   = FMath::Lerp(-0.02f, 0.02f, FunnyFaceUnit(EmployeeID, SaltFaceEyeOffset));
	Out.MouthOffsetY = FMath::Lerp(-0.015f, 0.02f, FunnyFaceUnit(EmployeeID, SaltFaceMouthOffset));
	return Out;
}

int32 FFunnyPartPicker::FaceBlinkSeed(int32 EmployeeID)
{
	return static_cast<int32>(Hash(EmployeeID, SaltFaceBlink));
}

int32 FFunnyPartPicker::PickFaceStyle(int32 EmployeeID, int32 StyleCount)
{
	if (StyleCount <= 0) { return INDEX_NONE; }
	const uint32 Ordinal = static_cast<uint32>(EmployeeID) - 1u;
	const uint32 Count = static_cast<uint32>(StyleCount);
	const uint32 Block = Ordinal / Count;
	const uint32 Offset = Ordinal % Count;
	uint32 ShuffleState = Hash(static_cast<int32>(Block), SaltFaceStyle);
	TArray<int32> Styles;
	Styles.Reserve(StyleCount);
	for (int32 Index = 0; Index < StyleCount; ++Index) { Styles.Add(Index); }
	for (int32 Last = StyleCount - 1; Last > 0; --Last)
	{
		ShuffleState = ShuffleState * 196314165u + 907633515u;
		Styles.Swap(Last, static_cast<int32>(ShuffleState % static_cast<uint32>(Last + 1)));
	}
	return Styles[static_cast<int32>(Offset)];
}
