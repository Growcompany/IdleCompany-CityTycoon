#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"

// 회사 표시 이름 = 업종 이름 + 「회사」 + 같은 업종 안에서 지은 순서(첫 회사도 1). 저장 필드 없이 지금 남은 회사로 센다
struct FCompanyDisplayNameRules
{
	struct FCompanyEntry
	{
		int32 BuildingIndex = INDEX_NONE;
		ECompanyType CompanyType = ECompanyType::None;
	};

	// BuildingIndex 는 지을 때마다 커지므로 같은 업종에서 그 이하인 회사 수가 곧 순번이다 ― 철거하면 뒤 번호가 당겨진다
	static bool FindOrdinal(TConstArrayView<FCompanyEntry> Companies, int32 BuildingIndex,
		ECompanyType& OutType, int32& OutOrdinal)
	{
		OutType = ECompanyType::None;
		for (const FCompanyEntry& Entry : Companies)
		{
			if (Entry.BuildingIndex == BuildingIndex)
			{
				OutType = Entry.CompanyType;
				break;
			}
		}
		OutOrdinal = 0;
		if (OutType == ECompanyType::None) { return false; }
		for (const FCompanyEntry& Entry : Companies)
		{
			OutOrdinal += (Entry.CompanyType == OutType && Entry.BuildingIndex <= BuildingIndex) ? 1 : 0;
		}
		return true;
	}

	static FText Format(const FText& IndustryName, int32 Ordinal)
	{
		return FText::Format(NSLOCTEXT("Company", "IndustryOrdinalName", "{0}회사 {1}"), IndustryName, FText::AsNumber(Ordinal));
	}
};
