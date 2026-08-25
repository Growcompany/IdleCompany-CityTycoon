// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LootBoxOrbitPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class ALootBoxActor;
class UInputMappingContext;

/**
 * 룩박스를 중심으로 회전하는 카메라 Pawn
 * - 마우스/터치 드래그로 좌우 회전
 * - 룩박스 액터를 바라보는 고정 거리 유지
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ALootBoxOrbitPawn : public APawn
{
	GENERATED_BODY()

public:
	ALootBoxOrbitPawn();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ===== 컴포넌트 =====

	// 회전 중심점 (룩박스 위치)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* OrbitCenter;

	// 카메라 암 (거리 조절용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* SpringArm;

	// 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* Camera;

	// ===== 카메라 설정 =====

	// 카메라 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraDistance = 800.0f;

	// 카메라 높이 (룩박스 중심에서 위로 얼마나 올라갈지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraHeight = 285.0f;

	// 회전 감도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float RotationSpeed = 1.0f;

	// 현재 회전 각도 (Yaw)
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	float CurrentYaw = 0.0f;

	// ===== 룩박스 참조 =====

	// 현재 표시 중인 룩박스 액터
	UPROPERTY(BlueprintReadWrite, Category = "LootBox")
	ALootBoxActor* CurrentLootBox = nullptr;

	// ===== Enhanced Input =====

	// LootBox 상호작용용 Input Mapping Context
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* IMC_LootBox = nullptr;


public:

	// ===== 블루프린트 함수 =====

	// 특정 룩박스를 표시하도록 설정
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void SetLootBox(ALootBoxActor* NewLootBox);

	// 룩박스를 특정 등급/타입으로 변경
	UFUNCTION(BlueprintCallable, Category = "LootBox")
	void ChangeLootBoxAppearance(ELootBoxRarity Rarity, ELootBoxType Type);
};
