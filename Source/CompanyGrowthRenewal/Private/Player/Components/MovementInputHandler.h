// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "MovementInputHandler.generated.h"


class APlayerCamera;
class UEnhancedInputLocalPlayerSubsystem;
class UCurveFloat;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class COMPANYGROWTHRENEWAL_API UMovementInputHandler : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMovementInputHandler();

	void Initialize(class UFloatingPawnMovement* Movement);
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	/** 화면 터치 → 월드 평면 투영. 성공하면 true & outIntersection 세팅 */
	bool GetTouchIntersection(FVector& outIntersection) const;

private:
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
	void ApplyZoomSettings(); // ZoomValue를 SpringArm 등에 적용만 (값 변경 안 함)
	void UpdateDof() const;
	void MoveTracking();
	void PositionCheck();

	// ZoomValue Getter/Setter (카메라 전환용)
	UFUNCTION(BlueprintPure, Category = "Camera|Zoom")
	float GetZoomValue() const { return ZoomValue; }

	UFUNCTION(BlueprintCallable, Category = "Camera|Zoom")
	void SetZoomValue(float NewValue) { ZoomValue = FMath::Clamp(NewValue, 0.0f, 1.0f); }

	// 카메라 줌 땡기는 거리
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float MinZoomDistance = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float MaxZoomDistance = 320000.f;

	// 줌 연동 카메라 피치·FOV 밴드 (lerpKey 0=가까이 → 1=멀리). 기본=MainMap, OfficeCameraPawn 생성자에서 오버라이드.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomInPitch = -40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomOutPitch = -55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomInFOV = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom Settings")
	float ZoomOutFOV = 20.f;

	// 이동 경계 반지름 (MainMap: 100000, OfficeMap: 작은 값)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Settings")
	float BoundaryRadius = 100000.f;

private:
	UFloatingPawnMovement* Movement = nullptr;
	APlayerCamera* Owner = nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = nullptr;

	// Zoom
	float ZoomDirection = 0.f;
	float ZoomValue = 0.5f;
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
