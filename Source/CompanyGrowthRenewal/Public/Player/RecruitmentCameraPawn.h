// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RecruitmentCameraPawn.generated.h"

class UCameraComponent;

/**
 * 채용 맵 전용 고정 카메라 Pawn
 * - 약간 낮은 앵글에서 문을 바라보는 고정 카메라
 * - 입력 처리 없음 (카메라 이동/회전 불가)
 * - PlayerStart 위치에 스폰됨
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ARecruitmentCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARecruitmentCameraPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	// 카메라 피치 각도 (양수 = 위를 올려봄, 기본 10도)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraPitch = 0.0f;

	// FOV (시야각)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraFOV = 70.0f;

protected:
	virtual void BeginPlay() override;
};
