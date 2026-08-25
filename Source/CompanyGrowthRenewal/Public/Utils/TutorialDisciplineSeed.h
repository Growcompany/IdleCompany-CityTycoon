#pragma once

#include "CoreMinimal.h"
#include "Enum/ProductionDiscipline.h"

/**
 * 튜토리얼 첫 채용(M4)에서 강제할 주 직능 목록 = 기획 + 개발 고정.
 *
 * 튜토리얼 판인 탭탭코인(Project001)은 활성 직능이 기획/개발 2칸뿐이라(Weight_Graphics=0),
 * 2명이 두 칸을 1:1로 맡으면 어느 칸도 비지 않는다. 업무집중도가 각자를 자기 칸으로 몰아주는 것과 짝이다.
 * 구 설계는 두 번째를 기획/그래픽 랜덤으로 굴렸는데, 그래픽이 뽑히면 기획 칸을 아무도 주력으로 안 맡았다.
 *
 * 난수를 인자로 받아 결정적 — 호출부가 RandRange 를 담당한다.
 */
namespace TutorialDisciplineSeed
{
	/** @param OrderRoll 짝수=개발 먼저, 홀수=기획 먼저 */
	inline TArray<EProductionDiscipline> BuildSeedList(int32 OrderRoll)
	{
		TArray<EProductionDiscipline> Result;
		Result.Reserve(2);

		// 첫 카드가 늘 개발이면 멀티뽑기 연출의 기대감이 죽는다
		if (FMath::Abs(OrderRoll) % 2 == 0)
		{
			Result.Add(EProductionDiscipline::Dev);
			Result.Add(EProductionDiscipline::Plan);
		}
		else
		{
			Result.Add(EProductionDiscipline::Plan);
			Result.Add(EProductionDiscipline::Dev);
		}
		return Result;
	}
}
