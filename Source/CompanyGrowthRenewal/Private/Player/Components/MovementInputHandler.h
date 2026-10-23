// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "Player/Components/CameraFramingMath.h"
#include "MovementInputHandler.generated.h"


class APlayerCamera;
class UEnhancedInputLocalPlayerSubsystem;
class UCurveFloat;
struct FMinimalViewInfo;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class COMPANYGROWTHRENEWAL_API UMovementInputHandler : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMovementInputHandler();

	void Initialize(class UFloatingPawnMovement* Movement);
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);
	void SetBoundaryCenterWorld(const FVector& InBoundaryCenterWorld);
	FVector CalculatePlanarBoundaryOffset(const FVector& WorldLocation) const;
	bool SetPlanarMovementBounds(const FBox2D& InLocalBounds, const FTransform& InBoundsTransform,
		bool bInConstrainCameraView = false);
	FVector ClampToMovementBoundary(const FVector& WorldLocation) const;
	void ApplyMovementBoundary();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	/** 화면 터치 → 월드 평면 투영. 성공하면 true & outIntersection 세팅 */
	bool GetTouchIntersection(FVector& outIntersection) const;

private:
	friend struct FMovementInputHandlerTestAccess;
	friend class APlayerCamera;
	friend class UCityCameraBoundaryModifier;
	FMinimalViewInfo BuildBoundaryView(const FVector& PawnLocation, float Zoom, float ProjectionOffset) const;
	bool GetCameraMovementBounds(const FVector& PawnLocation, float Zoom, float ProjectionOffset, FBox2D& OutBounds) const;
	bool TryConstrainCameraPose(FVector& InOutLocation, float& InOutZoom, float ProjectionOffset) const;
	void ConstrainCameraView(FMinimalViewInfo& InOutView) const;
	void Move(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void Spin(const FInputActionValue& Value);
	// 드래그 입력용 콜백
	UFUNCTION()
	void OnDragStarted(const FInputActionValue& Value);

	UFUNCTION()
	void OnDragMove(const FInputActionValue& Value);

public:
	void InitDragMoveIMC() const;
	void ReleaseDragMoveIMC() const;

	//void PositionCheck();
	void UpdateZoom();
	UFUNCTION(BlueprintCallable, Category = "Camera|Zoom")
	void ApplyZoomSettings(); // Apply the rig and, when enabled, constrain its zoom and view to the playable area.

	// 줌 리그 순수 평가 — 컴포넌트 미접촉. 프레이밍 탐색은 이걸로만 계산(실제 적용은 ApplyZoomSettings/Tick)
	FZoomRigParams GetZoomRigParams() const;
	float EvaluateZoomKey(float InZoomValue) const;
	FZoomRigSample EvaluateZoomRig(float InZoomValue) const;

	void MoveTracking();
	void PositionCheck();

	// ZoomValue Getter/Setter (카메라 전환용)
	UFUNCTION(BlueprintPure, Category = "Camera|Zoom")
	float GetZoomValue() const { return ZoomValue; }

	UFUNCTION(BlueprintCallable, Category = "Camera|Zoom")
	void SetZoomValue(float NewValue) { ZoomValue = FMath::Clamp(NewValue, 0.0f, MaximumZoomValue); }

	// 리그 커브 자체는 유지하고 입력/전환이 사용할 정규화 범위만 제한한다.
	void SetMaximumZoomValue(float InMaximumZoomValue);
	float GetMaximumZoomValue() const { return MaximumZoomValue; }

	// 카메라 줌 땡기는 거리
public:
	// ArmLength = Lerp(Min, Max, ZoomCurve(ZoomValue)). 실제 카메라 거리는 소켓 후퇴량을 더한다.
	// 최근접 한계에 걸리는 저층 건물은 프레이밍 목표보다 작게 보이며, 남는 여백을 상하 대칭으로 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float MinZoomDistance = 3000.f;

	// 수동 줌과 포커스가 공유하는 최대 팔길이. 담기지 않는 건물은 상단을 고정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float MaxZoomDistance = 20000.f;

	// 줌 연동 카메라 피치·FOV 밴드 (lerpKey 0=가까이 → 1=멀리). 기본=MainMap,
	// OfficeCameraPawn·WorldMapCameraPawn 생성자에서 오버라이드.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomInPitch = -40.f;

	// 줌아웃에서 더 눕히면 지평선과 하늘이 빠져 화면이 닫힌다 — 멀어질수록 탁 트이게 하려면 완만해야 한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomOutPitch = -48.f;

	// FieldOfView 는 **수평** 값이고 MaintainYFOV 에서 수직은 1.778배 좁아진다(52 → vFOV 30°).
	// 구 30/20 은 vFOV 11~17°(환산 82~121mm) 망원이라 원근이 눌리고 주변시가 없어 전 구간이 답답했다.
	// `[개정 2026-10-04]` 40/46 → 52/58: 넓은 렌즈라야 **같은 거리에서 더 담긴다** = 높은 건물을 가까이서 다 담는다
	// (88m 건물 160m → 122m). MinZoomDistance 와 한 쌍 — 렌즈만 넓히면 짧은 건물이 바닥에 걸려 더 작아진다(§5.1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomInFOV = 52.f;

	// ⚠ In < Out 이 의도다. 좁아지면 줌아웃 이득의 1/3을 렌즈가 먹어, 답답할 때 플레이어가 하는
	// 유일한 행동(줌아웃)이 상쇄된다. "In>Out 이 정상" 으로 되돌리지 말 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomOutFOV = 58.f;

	// 이동 경계 반지름 (MainMap: 100000, OfficeMap: 작은 값)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Settings")
	float BoundaryRadius = 100000.f;

private:
	UFloatingPawnMovement* Movement = nullptr;
	APlayerCamera* Owner = nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = nullptr;
	FVector BoundaryCenterWorld = FVector::ZeroVector;
	FBox2D PlanarMovementBounds = FBox2D(ForceInit);
	FTransform MovementBoundsTransform = FTransform::Identity;
	bool bConstrainCameraView = false;
	mutable bool bReportedViewBoundsFailure = false;

	// Zoom
	float ZoomDirection = 0.f;
	float ZoomValue = 0.5f;
	float MaximumZoomValue = 1.f;
	// 콘텐츠 에셋(C_Zoom) — 쿠킹 빌드에서 GC 수거되어 댕글링되지 않도록 리플렉션 참조로 보유
	UPROPERTY()
	TObjectPtr<UCurveFloat> ZoomCurve = nullptr;

	// DragMove
	FVector StoredMove;
	FVector TargetHandle;
	void TrackMove();
	bool bIsDragging = false;
	bool bWasSingleTouch = false;  // 직전 프레임이 싱글터치였는지


	// Cursor
	float EdgeMoveDistance = 50.0f;
	void GetEdgeMove(FVector& Direction, float& Strength);
	void CursorDistFromViewportCenter(FVector2D mousePosFromViewportCenter, FVector& Direction, float& Strength) const;


	// Input

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> IMC_DragMoveContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Zoom;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Spin;

	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_DragMove;



};
