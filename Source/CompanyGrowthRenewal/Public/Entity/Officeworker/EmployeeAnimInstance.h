// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Enum/EmployeeState.h"
#include "EmployeeAnimInstance.generated.h"

class AOfficeworker;
class UEmployeeBehaviorComponent;

/**
 * 직원 캐릭터 애니메이션 인스턴스
 * EmployeeBehaviorComponent의 상태를 ABP에 전달
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 현재 행동 패턴 모드
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	EEmployeeBehaviorMode CurrentBehaviorMode = EEmployeeBehaviorMode::Idle;

	// 현재 직원 상태
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	EEmployeeState EmployeeState = EEmployeeState::Wander;

	// Dance 상태에서 재생할 댄스 클립 인덱스 (AnimBP의 Blend Poses by Int 핀 선택용)
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	int32 DanceIndex = 0;

	// 앉은 자세 변주 인덱스 (Sitting 상태에서 Blend Poses by Int 핀 선택). 0=기본 1=태평 2=꾸벅
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	int32 SeatedPoseIndex = 0;

	// 앉음/서있음 상태 (true: 앉음, false: 서있음)
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	bool bIsSeated = false;

	// 걷는 중 여부
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	bool bWalking = false;

	// 이동 속도
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	float Speed = 0.0f;

	// 전 직원이 같은 프레임에 같은 동작을 시작해 군무처럼 보인다. 직원마다 고정 위상/속도를 준다.
	// EmployeeID 해시라 결정적이다 - 세이브/재입장에도 같은 직원은 같은 값을 받는다(신발 선택과 같은 축).
	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	float AnimPhase01 = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Employee")
	float AnimRateJitter = 1.0f;

	// 작업 장비 소켓을 직원 메시의 컴포넌트 공간으로 변환한 손목 목표
	UPROPERTY(BlueprintReadOnly, Category = "Typing")
	FVector TypingLeftHandLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Typing")
	FVector TypingRightHandLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Typing")
	FRotator TypingLeftHandRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Typing")
	FRotator TypingRightHandRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Typing")
	float TypingIKAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Typing")
	float TypingMouseBlend = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Typing", meta = (ClampMin = "0.0"))
	float TypingIKBlendSpeed = 10.0f;

	// 반응 연출이 몸만 잠깐 달리게 할 때(잡혀서 자리로 복귀) 행동 상태 대신 이 속도로 이동 애님을 고른다. 음수 = 쓰지 않음.
	float ReactionRunSpeed = -1.0f;

protected:
	// 애니메이션 업데이트 (매 프레임 호출)
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void UpdateTypingHandTargets(float DeltaSeconds);

	// 캐시된 Owner 참조
	UPROPERTY()
	TWeakObjectPtr<AOfficeworker> CachedOwner;

	// 캐시된 BehaviorComponent 참조
	UPROPERTY()
	TWeakObjectPtr<UEmployeeBehaviorComponent> CachedBehaviorComponent;

	bool bAnimPhaseInitialized = false;
};
