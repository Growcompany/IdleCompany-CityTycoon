#include "Data/TierLayout.h"
#include "Data/ProjectBoardData.h"

void FTierLayout::FillDefault(TArray<FTierRange>& OutRanges)
{
	for (int32 T = 1; T <= TierConstants::MAX_TIER; ++T)
	{
		const int32 StartIndex = (T - 1) * TierConstants::PROJECTS_PER_TIER + 1;
		OutRanges.Add(FTierRange{ T, StartIndex, TierConstants::PROJECTS_PER_TIER, TierConstants::CLEAR_TO_UNLOCK, StartIndex });
	}
}

TArray<FTierRange>& FTierLayout::Ranges()
{
	static TArray<FTierRange> R;
	if (R.Num() == 0) { FillDefault(R); }
	return R;
}

void FTierLayout::ResetToDefault()
{
	TArray<FTierRange>& R = Ranges();
	R.Reset();
	FillDefault(R);
}

void FTierLayout::SetLayout(const TArray<FTierRange>& InRanges)
{
	if (InRanges.Num() == 0) { ResetToDefault(); return; }
	TArray<FTierRange>& R = Ranges();
	R = InRanges;
	R.Sort([](const FTierRange& A, const FTierRange& B) { return A.Tier < B.Tier; });
}

const FTierRange* FTierLayout::Find(int32 Tier)
{
	for (const FTierRange& Rg : Ranges())
	{
		if (Rg.Tier == Tier) { return &Rg; }
	}
	return nullptr;
}

int32 FTierLayout::MaxTier()
{
	return Ranges().Last().Tier;
}

int32 FTierLayout::ProjectsPerTier(int32 Tier)
{
	const FTierRange* R = Find(Tier);
	return R ? R->Count : 0;
}

int32 FTierLayout::ClearToUnlock(int32 Tier)
{
	const FTierRange* R = Find(Tier);
	return R ? R->ClearToUnlock : TierConstants::CLEAR_TO_UNLOCK;
}

void FTierLayout::GetRange(int32 Tier, int32& OutStart, int32& OutEnd)
{
	const FTierRange* R = Find(Tier);
	if (!R) { OutStart = 0; OutEnd = -1; return; }
	OutStart = R->StartIndex;
	OutEnd = R->StartIndex + R->Count - 1;
}

int32 FTierLayout::GetTierForProject(int32 ProjectIndex)
{
	for (const FTierRange& Rg : Ranges())
	{
		if (ProjectIndex >= Rg.StartIndex && ProjectIndex < Rg.StartIndex + Rg.Count) { return Rg.Tier; }
	}
	return 0;
}

int32 FTierLayout::RevenueScaleIndex(int32 ProjectIndex)
{
	const FTierRange* R = Find(GetTierForProject(ProjectIndex));
	if (!R) { return ProjectIndex; }   // 레이아웃 밖(합성 프로젝트 등) = 현행 그대로
	return R->RevenueAnchor + (ProjectIndex - R->StartIndex);
}
