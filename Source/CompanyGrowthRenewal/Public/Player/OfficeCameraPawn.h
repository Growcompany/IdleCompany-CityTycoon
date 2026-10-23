// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/PlayerCamera.h"
#include "Office/DecorationTypes.h"
#include "Table/WorkstationCardTable.h"
#include "Table/DecorationCardTable.h"
#include "OfficeCameraPawn.generated.h"

class AOfficeInterior;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOfficePlacementEditChanged);

/**
 * 등반 중 카메라가 얹는 상승·후퇴량. 기존 초기 프레이밍 결과 위에 더해지는 값이다.
 * 사무실은 고정이고 땅이 내려가므로, 쌓인 탑을 보여주려면 카메라가 올라가며 물러나야 한다.
 */
struct COMPANYGROWTHRENEWAL_API FSkylineClimbCameraOffset
{
	static constexpr float LiftPerFloorCm = 70.f;
	static constexpr float PullBackPerFloorCm = 90.f;
	static constexpr float MaxLiftCm = 600.f;
	static constexpr float MaxPullBackCm = 800.f;

	float LiftCm = 0.f;
	float PullBackCm = 0.f;

	static FSkylineClimbCameraOffset Compute(int32 ClearedCount);
};

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
	UFUNCTION(BlueprintCallable) bool BeginOfficePlacementEdit();
	UFUNCTION(BlueprintCallable) void EndOfficePlacementEdit();
	UFUNCTION(BlueprintPure) bool IsOfficePlacementEditing() const { return bOfficePlacementEditing; }
	UFUNCTION(BlueprintPure) AActor* GetOfficeEditSelection() const;
	UFUNCTION(BlueprintCallable) bool SelectOfficeEditActor(AActor* Actor);
	UFUNCTION(BlueprintCallable) bool ConfirmOfficeEditSelection();
	UFUNCTION(BlueprintCallable) void CancelOfficeEditSelection();
	UFUNCTION(BlueprintCallable) void RotateOfficeEditSelection();
	UFUNCTION(BlueprintPure) bool CanRemoveOfficeEditSelection() const;
	UFUNCTION(BlueprintCallable) bool RemoveOfficeEditSelection();
	UPROPERTY(BlueprintAssignable) FOnOfficePlacementEditChanged OnOfficePlacementEditChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
	// 디버그/튜닝: 줌연동 피치·FOV 밴드를 런타임 교체 후 즉시 반영 (치트 CamPreset/CamBand)
	void ApplyCameraBand(float InPitch, float OutPitch, float InFOV, float OutFOV);
	// Office 카메라가 허용하는 yaw 범위. 입장 전 도시 전망의 보수적 방향 검사에서도 재사용한다.
	static FVector2D GetSupportedYawRange();
	static float GetDefaultViewYaw();
	// 기본 Office 리그의 최초 카메라 위치를 바닥 중심 기준으로 계산한다. 월드와 카메라 상태는 변경하지 않는다.
	static bool TryGetInitialPhotoViewOffset(FVector2D FloorSizeCm, float PresentationScale, FVector& OutCameraOffset);
	bool IsInitialOfficeFramingReady() const { return bInitialOfficeFramingApplied && !bInitialOfficeFramingPending; }
	bool TryBuildCameraPoseLog(
		const AOfficeInterior* Interior,
		FIntPoint ViewportSize,
		FString& OutLine) const;

	UFUNCTION(BlueprintCallable, Category = "Office Camera")
	bool ApplyInitialOfficeFraming(bool bForce = false);

	// ========== 스카이라인 등반 ==========

	/** 등반으로 넘은 층 수. 목표값만 세팅하고 Tick 이 보간한다. 0 이면 원위치로 돌아간다. */
	UFUNCTION(BlueprintCallable, Category = "Office Camera|Skyline")
	void SetSkylineClimbFloors(int32 ClearedCount);

	FSkylineClimbCameraOffset GetSkylineClimbOffset() const { return AppliedClimbOffset; }

	// ========== 연출 당김 ==========

	// 개발 완료 환호처럼 잠깐 바라보는 쪽으로 당긴다 — 줌·등반 값은 두고 SocketOffset 에만 더한다. Fraction = 지금 거리 대비 비율, 흔들림 없음.
	void PlayPresentationPullIn(float Fraction, float Duration);
	void ReleasePresentationPullIn(float Duration);

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
	bool bOfficePlacementEditing = false;
	friend struct FOfficeCameraPawnTestAccess;

	void HandleGameDataLoaded();
	bool TryGetInitialFramingViewport(
		float& OutAspectRatio,
		int32& OutViewportWidth,
		int32& OutViewportHeight) const;
	void TryApplyPendingInitialOfficeFraming();
	bool ResolveZoomValueForArmLength(
		float DesiredArmLength,
		float& OutZoomValue,
		float& OutClampedArmLength);
	bool ResolveInitialZoomValueForArmRange(
		float DesiredArmLength,
		float MinimumArmLength,
		float MaximumArmLength,
		float& OutZoomValue,
		float& OutRepresentedArmLength);
	bool TrySettleCalibratedInitialPose(AOfficeInterior* Interior, const FBox2D* FramingBounds = nullptr);
	void ApplyExpansionFramingSlack(AOfficeInterior* Interior);
	bool IsAtAutomaticFramingPose() const;
	FTransform LastAutomaticFramingPose;
	float LastAutomaticFramingZoom = -1.f;
	bool SettleFocusOnLocationImmediately(
		const FVector& Location,
		float DesiredArmLength,
		float MinimumArmLength,
		float MaximumArmLength);
	AOfficeInterior* FindUniqueOfficeInterior(int32& OutInteriorCount) const;
	void BindOfficeBoundaryInterior(AOfficeInterior* Interior);
	void UpdateOfficeBoundaryCenter(AOfficeInterior* Interior);
	void HandleOfficeFootprintChanged(FIntPoint TileCount);

	UPROPERTY(EditDefaultsOnly, Category = "Office Camera|Initial Framing", meta = (ClampMin = "0.0"))
	float InitialFocusBelowFloorCm = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office Camera|Initial Framing", meta = (ClampMin = "1.0"))
	float InitialFacadeVisibleDepthCm = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office Camera|Initial Framing", meta = (ClampMin = "1.0"))
	float InitialMinimumArmLengthCm = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office Camera|Initial Framing", meta = (ClampMin = "1.0"))
	float InitialMaximumArmLengthCm = 5200.f;

	bool bInitialOfficeFramingApplied = false;
	bool bInitialOfficeFramingPending = false;
	// 뷰포트가 유효해진 뒤의 적용 실패 횟수. 영구 실패(OfficeInterior 부재 등)가 Tick 에서
	// 월드 순회 + 경고를 무한 반복하지 않도록 이 예산을 넘기면 포기한다.
	int32 InitialOfficeFramingFailureCount = 0;
	static constexpr int32 MaxInitialOfficeFramingFailures = 10;
	TWeakObjectPtr<AOfficeInterior> BoundOfficeInterior;

	// 회전 각도 제한 (135도 기준 ±45도 → 90도 ~ 180도)
	void ClampRotation();
	float MinYawRotation = 90.0f;   // 135도에서 왼쪽으로 45도
	float MaxYawRotation = 180.0f;  // 135도에서 오른쪽으로 45도

	// ========== 벽 편집 모드 상태 ==========

	// 벽 편집 모드 활성화 여부
	bool bIsInWallEditMode = false;

	// 벽 편집 진입 전 보기(위치·줌·각도) ― 벽 앞으로 옮긴 카메라를 종료 때 그 자리로 돌린다
	FOfficeCameraViewState WallEditReturnView;

	// 기본 회전 각도 (벽 모드 종료 시 복귀할 각도)
	UPROPERTY(EditAnywhere, Category = "Office Camera|Wall Edit")
	float DefaultYawRotation = 135.0f;

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

	// ========== 스카이라인 등반 상태 ==========

	FSkylineClimbCameraOffset TargetClimbOffset;
	FSkylineClimbCameraOffset AppliedClimbOffset;
	float ClimbSocketBaseX = 0.f;
	bool bClimbSocketBaseCaptured = false;
	void UpdateSkylineClimbOffset(float DeltaTime);

	// 연출 당김(cm) — 등반 후퇴와 같은 SocketOffset.X 기준에 더해진다
	float PresentationPullFromCm = 0.f;
	float PresentationPullToCm = 0.f;
	float AppliedPresentationPullCm = 0.f;
	float PresentationPullDuration = 0.f;
	double PresentationPullStartTime = -1.0;
	void UpdatePresentationPull();

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
