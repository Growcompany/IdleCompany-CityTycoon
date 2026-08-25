#pragma once

#include "CoreMinimal.h"
#include "OperationState.generated.h"

/**
 * 프로젝트 운영 상태
 * 출시 후 운영 단계에서의 상태 관리
 */
UENUM(BlueprintType)
enum class EOperationState : uint8
{
	None UMETA(DisplayName = "None"),                     // 운영 안함
	WaitingLaunch UMETA(DisplayName = "Waiting Launch"),  // 출시 대기 (QA 완료 후)
	Operating UMETA(DisplayName = "Operating"),           // 운영 중
	Paused UMETA(DisplayName = "Paused"),                 // 피로도 MAX로 일시정지
	Completed UMETA(DisplayName = "Completed")            // 운영 완료
};

/**
 * 운영 상태를 문자열로 변환
 */
inline FString OperationStateToString(EOperationState State)
{
	switch (State)
	{
	case EOperationState::None: return TEXT("운영 안함");
	case EOperationState::WaitingLaunch: return TEXT("출시 대기");
	case EOperationState::Operating: return TEXT("운영 중");
	case EOperationState::Paused: return TEXT("일시정지");
	case EOperationState::Completed: return TEXT("운영 완료");
	default: return TEXT("Unknown");
	}
}
