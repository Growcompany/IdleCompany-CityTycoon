#pragma once

#include "CoreMinimal.h"
#include "Enum/ProductionDiscipline.h"

// 미션 복원과 랜덤 채용 경로도 DT에 지정한 첫 채용 구성을 따른다.
namespace TutorialDisciplineSeed
{
	inline TArray<EProductionDiscipline> BuildSeedList(EProductionDiscipline Required, int32 Count)
	{
		TArray<EProductionDiscipline> Result;
		if (Required != EProductionDiscipline::Count && Count > 0) Result.Init(Required, Count);
		return Result;
	}
}
