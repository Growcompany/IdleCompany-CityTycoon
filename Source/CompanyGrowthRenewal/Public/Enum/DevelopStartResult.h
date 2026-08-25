// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DevelopStartResult.generated.h"

// 자체개발 착수 결과. 실패 사유를 호출자(UI)가 알림 문구로 옮길 수 있게 분해해 둔 것 —
// 이전엔 void 반환이라 호출자가 Lifecycle 전이 여부로 성공을 추정할 수밖에 없었고, 그래서 조용히 실패했다.
UENUM(BlueprintType)
enum class EDevelopStartResult : uint8
{
	Success,
	NoIndustry,			// 산업 미확인 (Game 산업 빌딩에서 진입하지 않음)
	NoProjectRow		// DT_Project_* 에 해당 행이 없음
};
