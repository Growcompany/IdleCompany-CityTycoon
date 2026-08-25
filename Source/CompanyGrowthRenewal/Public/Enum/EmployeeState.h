// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EmployeeState.generated.h"

// 직원 행동 상태
UENUM(BlueprintType)
enum class EEmployeeState : uint8
{
	Typing			UMETA(DisplayName = "Typing"),			// 타이핑 중
	Sitting			UMETA(DisplayName = "Sitting"),			// 앉아있음
	Wander			UMETA(DisplayName = "Wander"),			// 배회 (Walk/Idle)
	CheerSitting	UMETA(DisplayName = "Cheer Sitting"),	// 앉아서 환호 (앉은 상태 유지)
	CheerStandUp	UMETA(DisplayName = "Cheer Stand Up"),	// 일어서며 환호 (앉음→서있음)

	// 전환 상태 (Transition States)
	TypeToSit		UMETA(DisplayName = "Type To Sit"),		// Typing에서 앉기로 전환 중
	SitToStand		UMETA(DisplayName = "Sit To Stand"),	// 앉은 상태에서 일어나는 중
	StandToSit		UMETA(DisplayName = "Stand To Sit"),	// 서있는 상태에서 앉는 중
	Greeting		UMETA(DisplayName = "Greeting"),		// 인사 (서 있는 상태)
	Dance			UMETA(DisplayName = "Dance"),			// 춤 (DanceIndex 로 클립 선택, 서 있는 상태)

	Max				UMETA(Hidden)
};

// 배회 목적지 타입
UENUM(BlueprintType)
enum class EWanderDestination : uint8
{
	RandomPoint		UMETA(DisplayName = "Random Point"),	// 랜덤 위치
	NearCoworker	UMETA(DisplayName = "Near Coworker"),	// 동료 근처
	WindowArea		UMETA(DisplayName = "Window Area"),		// 창가
	CenterArea		UMETA(DisplayName = "Center Area"),		// 중앙

	Max				UMETA(Hidden)
};

// 직원 행동 패턴 모드 (오피스 상태에 따른 행동 결정)
UENUM(BlueprintType)
enum class EEmployeeBehaviorMode : uint8
{
	Idle		UMETA(DisplayName = "Idle"),		// 배회만 (Walk <-> Idle 반복)
	Stage		UMETA(DisplayName = "Stage"),		// 프로젝트 진행 중 (Workstation에서 Typing 유지)
	Operation	UMETA(DisplayName = "Operation"),	// 운영 중 (배회 <-> 업무 반복)

	Max			UMETA(Hidden)
};

// 직원이 업무에서 이탈한 사유. 착석 자세와 레일 알림 문구가 이 하나를 같이 읽어야
// "졸고 있습니다"인데 태평하게 앉아 있는 불일치가 안 난다.
// 체력축(Drowsy/Lazy)과 침착성축(Bolted)은 서로 독립이지만, 표시 계층은 사유별로만 분기하면 되므로 한 enum 에 모은다.
UENUM(BlueprintType)
enum class EWorkerDownReason : uint8
{
	Drowsy	UMETA(DisplayName = "졸음"),	// 자리에서 꾸벅 (SeatedPoseIndex 2)
	Lazy	UMETA(DisplayName = "딴짓"),	// 자리에서 태평 (SeatedPoseIndex 1)
	Bolted	UMETA(DisplayName = "폭주"),	// 자리를 박차고 배회

	Max		UMETA(Hidden)
};

// 피로도 밴드 (Part A — 살아있는 사무실). v1은 3밴드. 임계값은 DA_FatigueConfig 가 정함(여기 enum 은 식별자만).
UENUM(BlueprintType)
enum class EFatigueBand : uint8
{
	Energetic	UMETA(DisplayName = "활기"),	// 쌩쌩, 산출 최고
	Tired		UMETA(DisplayName = "피곤"),	// 표정/자세로 피곤 표시(중간 graded)
	Slacking	UMETA(DisplayName = "딴짓"),	// 자리 이탈/늘어짐 = 터치/드래그 대상

	Max			UMETA(Hidden)
};
