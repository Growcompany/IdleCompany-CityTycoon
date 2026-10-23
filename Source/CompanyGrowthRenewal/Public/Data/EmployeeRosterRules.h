// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// 로스터 띠의 빈자리("채용") 타일 수 — 정원을 넘긴 보유(이벤트 지급 등)에서도 음수가 되지 않는다.
struct FEmployeeRosterRules
{
	static int32 HireSlotCount(int32 Owned, int32 Capacity)
	{
		return FMath::Max(0, Capacity - FMath::Max(0, Owned));
	}
};
