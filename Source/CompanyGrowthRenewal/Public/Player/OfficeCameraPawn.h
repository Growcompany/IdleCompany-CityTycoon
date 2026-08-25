// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/PlayerCamera.h"
#include "Office/DecorationTypes.h"
#include "Table/WorkstationCardTable.h"
#include "Table/DecorationCardTable.h"
#include "OfficeCameraPawn.generated.h"

/**
 * OfficeMap 전용 카메라 Pawn
 *
 * 특징:
 * - PlayerCamera를 상속받아 동일한 구조 사용
 * - 회전(Spin) 기능 비활성화
 * - WASD/드래그로 이동, 마우스 휠로 줌
 * - 줌 범위를 좁게 제한 (오피스 환경에 맞춤)
 * - PlacementHandler, InteractableInputHandler 비활성화 (오피스에서 불필요)
 * - 벽 장식 배치 시 카메라 회전 기능 추가
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeCameraPawn : public APlayerCamera
{
	GENERATED_BODY()

public:
	AOfficeCameraPawn();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
	// 디버그/튜닝: 줌연동 피치·FOV 밴드를 런타임 교체 후 즉시 반영 (치트 CamPreset/CamBand)
	void ApplyCameraBand(float InPitch, float OutPitch, float InFOV, float OutFOV);

	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	bool ApplyInitialOfficeFraming(bool bForce = false);

	// ========== 벽 편집 모드 ==========

	/**
	 * 벽 편집 모드 진입 (카메라 회전 제한 해제)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	void EnterWallEditMode();

	/**
	 * 선택한 벽으로 카메라 회전
	 * @param WallSide 왼쪽 또는 오른쪽 벽
	 * @param FocusLocation 카메라가 바라볼 위치 (기본값: 벽 중앙)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	void FocusOnWall(EWallSide WallSide, const FVector& FocusLocation = FVector::ZeroVector);

	/**
	 * 벽 편집 모드 종료 (카메라를 원래 각도로 복귀)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	void ExitWallEditMode();

	/**
	 * 현재 벽 편집 모드인지 확인
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	bool IsInWallEditMode() const { return bIsInWallEditMode; }

	// ========== 직원 포커스 ==========

	/**
	 * 직원에게 카메라 포커스 (각도 유지, 확대 및 이동만)
	 * @param Employee 포커스할 직원 Actor
	 * @param DesiredDistance 직원까지의 목표 거리 (기본 500)
	 * @param bEnableFollow true면 직원을 계속 따라감
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	void FocusOnEmployee(class AOfficeworker* Employee, float DesiredDistance = 500.f, bool bEnableFollow = true);

	/**
	 * 직원 추적 중지
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	void StopFollowingEmployee();

	/**
	 * 현재 직원을 추적 중인지 확인
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	bool IsFollowingEmployee() const { return bIsFollowingEmployee; }

	// ========== 위치 포커스 ==========

	/**
	 * 특정 위치로 카메라 포커스 (각도 유지, 확대 및 이동만)
	 * @param Location 포커스할 월드 위치
	 * @param DesiredDistance 목표 거리 (기본 600)
	 */
	virtual void FocusOnLocation(const FVector& Location, float DesiredDistance = 600.f) override;

	// ========== 업무공간 배치 ==========

	/**
	 * 업무공간 배치 모드 시작
	 * @param WorkstationInfo 배치할 업무공간 정보
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Workstation")
	void BeginWorkstationPlacement(const FWorkstationCardTable& WorkstationInfo);

	/**
	 * 업무공간 배치 모드 종료
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Workstation")
	void EndWorkstationPlacement();

	/**
	 * 현재 업무공간 배치 모드인지 확인
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Workstation")
	bool IsInWorkstationPlacementMode() const { return bIsInWorkstationPlacementMode; }

	/**
	 * 현재 배치 중인 업무공간 정보 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Workstation")
	const FWorkstationCardTable& GetCurrentWorkstationInfo() const { return CurrentWorkstationInfo; }

	// ========== 장식품 배치 ==========

	/**
	 * 장식품 배치 모드 시작 (바닥/Grid 배치)
	 * @param DecorationInfo 배치할 장식품 정보
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Decoration")
	void BeginDecorationPlacement(const FDecorationCardTable& DecorationInfo);

	/**
	 * 벽 장식품 배치 모드 시작
	 * @param DecorationInfo 배치할 장식품 정보
	 * @param WallSide 배치할 벽 (Left 또는 Right)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Decoration")
	void BeginWallDecorationPlacement(const FDecorationCardTable& DecorationInfo, EWallSide WallSide);

	/**
	 * 장식품 배치 모드 종료
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Decoration")
	void EndDecorationPlacement();

	/**
	 * 현재 장식품 배치 모드인지 확인
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Decoration")
	bool IsInDecorationPlacementMode() const { return bIsInDecorationPlacementMode; }

	/**
	 * 현재 배치 중인 장식품 정보 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Decoration")
	const FDecorationCardTable& GetCurrentDecorationInfo() const { return CurrentDecorationInfo; }

private:
	void HandleGameDataLoaded();

	UPROPERTY(EditDefaultsOnly, Category = "Office Camera|Initial Framing", meta = (ClampMin = "1.0"))
	float InitialFacadeVisibleDepthCm = 400.f;

	bool bInitialOfficeFramingApplied = false;

	// 회전 각도 제한 (135도 기준 ±45도 → 90도 ~ 180도)
	void ClampRotation();
	float MinYawRotation = 90.0f;   // 135도에서 왼쪽으로 45도
	float MaxYawRotation = 180.0f;  // 135도에서 오른쪽으로 45도

	// ========== 벽 편집 모드 상태 ==========

	// 벽 편집 모드 활성화 여부
	bool bIsInWallEditMode = false;

	// 벽 편집 모드 진입 전 원래 줌 값 (복귀용)
	float OriginalZoomValue = 0.f;

	// 기본 회전 각도 (벽 모드 종료 시 복귀할 각도)
	UPROPERTY(EditAnywhere, Category = "Office Camera|Wall Edit")
	float DefaultYawRotation = 150.0f;

	// 벽 편집 모드 진입 시 줌 거리 (값이 클수록 카메라가 멀어짐)
	UPROPERTY(EditAnywhere, Category = "Office Camera|Wall Edit")
	float WallEditZoomDistance = 2000.f;

	// 카메라 회전 보간 속도
	UPROPERTY(EditAnywhere, Category = "Office Camera|Wall Edit")
	float RotationInterpSpeed = 5.0f;

	// 현재 목표 회전 각도 (보간용)
	FRotator TargetRotation;

	// 회전 보간 중인지 여부
	bool bIsRotating = false;

	// 줌 보간 중인지 여부
	bool bIsZooming = false;

	// 카메라 회전 보간 업데이트
	void UpdateCameraRotation(float DeltaTime);

	// 카메라 줌 보간 업데이트
	void UpdateCameraZoom(float DeltaTime);

	// ========== 직원 추적 상태 ==========

	// 추적 중인 직원
	UPROPERTY()
	AOfficeworker* FollowingEmployee = nullptr;

	// 직원 추적 중인지 여부
	bool bIsFollowingEmployee = false;

	// 추적 시 유지할 거리
	float FollowDistance = 500.f;

	// 직원 추적 업데이트
	void UpdateEmployeeFollow(float DeltaTime);

	// 타겟 위치가 화면 중앙에 오도록 Pawn 위치 계산
	FVector CalculatePawnLocationForTarget(const FVector& TargetLocation) const;

	// ========== 업무공간 배치 상태 ==========

	// 업무공간 배치 모드 활성화 여부
	bool bIsInWorkstationPlacementMode = false;

	// 현재 배치 중인 업무공간 정보
	UPROPERTY()
	FWorkstationCardTable CurrentWorkstationInfo;

	// ========== 장식품 배치 상태 ==========

	// 장식품 배치 모드 활성화 여부
	bool bIsInDecorationPlacementMode = false;

	// 현재 배치 중인 장식품 정보
	UPROPERTY()
	FDecorationCardTable CurrentDecorationInfo;
};
