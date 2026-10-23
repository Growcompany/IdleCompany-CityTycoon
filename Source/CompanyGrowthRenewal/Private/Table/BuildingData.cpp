#include "Table/BuildingData.h"

#include "Engine/World.h"

float ResolvePlotBaseCellSize(const UWorld* World)
{
	// PIE 접두사(UEDPIE_0_)가 붙어도 Contains 가 흡수한다.
	const bool bRiverwalk = World && World->GetMapName().Contains(TEXT("TheRiverwalkCity"));
	return bRiverwalk ? PlotBaseCellSizeRiverwalk : PlotBaseCellSizeCompact;
}
