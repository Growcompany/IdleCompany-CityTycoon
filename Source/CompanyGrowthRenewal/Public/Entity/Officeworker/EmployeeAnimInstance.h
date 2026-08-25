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

protected:
	// 애니메이션 업데이트 (매 프레임 호출)
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	// 캐시된 Owner 참조
	UPROPERTY()
	TWeakObjectPtr<AOfficeworker> CachedOwner;

	// 캐시된 BehaviorComponent 참조
	UPROPERTY()
	TWeakObjectPtr<UEmployeeBehaviorComponent> CachedBehaviorComponent;
};
