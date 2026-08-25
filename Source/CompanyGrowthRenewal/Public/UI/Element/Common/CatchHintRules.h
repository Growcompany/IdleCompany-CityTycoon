#pragma once

#include "CoreMinimal.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"

// 캐치 링 옆 라이브 힌트 — 페이즈별 키/라벨과 졸업. 키는 PanelIntroSubsystem 카운트의 세이브 키다.
namespace CatchHintRules
{
	inline const FName KeyDoze(TEXT("CatchDoze"));
	inline const FName KeyBolt(TEXT("CatchBolt"));
	constexpr int32 GraduationCount = 3;
	// 링 중심 기준 손끝 위치 — 몸통 우상단(목업 실측)
	inline const FVector2D HintOffset(86.f, -74.f);

	inline FName ResolveKey(EFatigueSlackPhase Phase)
	{
		switch (Phase)
		{
		case EFatigueSlackPhase::Telegraph:
		case EFatigueSlackPhase::Slumping: return KeyDoze;
		case EFatigueSlackPhase::Bolting:  return KeyBolt;
		default:                           return NAME_None;
		}
	}
	inline FText ResolveLabel(EFatigueSlackPhase Phase)
	{
		switch (Phase)
		{
		case EFatigueSlackPhase::Telegraph:
		case EFatigueSlackPhase::Slumping: return NSLOCTEXT("CatchHint", "Doze", "탭해서 깨우기");
		case EFatigueSlackPhase::Bolting:  return NSLOCTEXT("CatchHint", "Bolt", "탭해서 불러오기");
		default:                           return FText::GetEmpty();
		}
	}
}
