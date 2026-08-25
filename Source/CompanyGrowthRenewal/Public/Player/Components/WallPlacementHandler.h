// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "Office/DecorationTypes.h"
#include "WallPlacementHandler.generated.h"

class UInputAction;
class UInputMappingContext;
class ADecorationActor;

/**
 * 벽 표면에 장식품을 배치하는 입력 핸들러
 *
 * 기능:
 * - Enhanced Input을 사용하여 터치/마우스 입력 처리
 * - 터치 위치를 벽 표면 좌표로 변환
 * - 배치 가능 여부 체크 (벽 범위 내, 겹침 여부)
 * - 미리보기 표시 및 최종 배치
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class COMPANYGROWTHRENEWAL_API UWallPlacementHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UWallPlacementHandler();

protected:
	virtual void BeginPlay() override;

public:
	// ========== 배치 모드 제어 ==========

	/**
	 * 벽 배치 모드 시작
	 * @param DecorationClass 배치할 장식품 클래스
	 * @param WallSide 배치할 벽 (왼쪽 또는 오른쪽)
	 */
	UFUNCTION(BlueprintCallable, Category = "Wall Placement")
	void StartPlacement(TSubclassOf<ADecorationActor> DecorationClass, EWallSide WallSide);

	/**
	 * 벽 배치 모드 종료
	 */
	UFUNCTION(BlueprintCallable, Category = "Wall Placement")
	void EndPlacement();

	/**
	 * 현재 배치 모드인지 확인
	 */
	UFUNCTION(BlueprintCallable, Category = "Wall Placement")
	bool IsPlacing() const { return bIsPlacing; }

	// ========== Enhanced Input 설정 ==========

	/**
	 * PlayerInputComponent에 입력 바인딩 등록
	 */
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);

private:
	// ========== Enhanced Input Actions ==========

	// 터치 액션 (Started/Ongoing/Triggered)
	UPROPERTY(EditAnywhere, Category = "Wall Placement|Input")
	UInputAction* TouchAction;

	// Input Mapping Context
	UPROPERTY(EditAnywhere, Category = "Wall Placement|Input")
	UInputMappingContext* PlacementMappingContext;

	// Input Mapping Context 우선순위
	UPROPERTY(EditAnywhere, Category = "Wall Placement|Input")
	int32 MappingPriority = 1;

	// ========== 입력 콜백 ==========

	// 터치 시작
	void OnTouchStarted(const FInputActionValue& Value);

	// 터치 진행 중
	void OnTouchOngoing(const FInputActionValue& Value);

	// 터치 완료
	void OnTouchTriggered(const FInputActionValue& Value);

	// ========== 배치 상태 ==========

	// 배치 모드 활성화 여부
	bool bIsPlacing = false;

	// 배치할 장식품 클래스
	UPROPERTY()
	TSubclassOf<ADecorationActor> DecorationClassToPlace;

	// 배치할 벽
	EWallSide TargetWall = EWallSide::Left;

	// 미리보기 액터
	UPROPERTY()
	ADecorationActor* PreviewActor = nullptr;

	// ========== 벽 표면 설정 ==========

	// 왼쪽 벽의 Y 좌표
	UPROPERTY(EditAnywhere, Category = "Wall Placement|Settings")
	float LeftWallYPosition = -500.0f;

	// 오른쪽 벽의 Y 좌표
	UPROPERTY(EditAnywhere, Category = "Wall Placement|Settings")
	float RightWallYPosition = 500.0f;

	// 벽의 X 좌표 범위 (Min ~ Max)
	UPROPERTY(EditAnywhere, Category = "Wall Placement|Settings")
	float WallXMin = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Wall Placement|Settings")
	float WallXMax = 1000.0f;

	// 벽의 Z 좌표 범위 (바닥 ~ 천장)
	UPROPERTY(EditAnywhere, Category = "Wall Placement|Settings")
	float WallZMin = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Wall Placement|Settings")
	float WallZMax = 300.0f;

	// ========== 배치 검증 ==========

	// 배치 가능 여부 체크
	bool CanPlaceAt(const FVector& WorldLocation) const;

	// 다른 장식품과 겹치는지 체크
	bool IsOverlapping(const FVector& WorldLocation) const;

	// ========== 좌표 변환 ==========

	// 스크린 좌표를 벽 표면 월드 좌표로 변환
	bool ScreenToWallLocation(const FVector2D& ScreenPosition, FVector& OutWorldLocation) const;

	// ========== 미리보기 ==========

	// 미리보기 액터 생성
	void CreatePreviewActor();

	// 미리보기 액터 제거
	void DestroyPreviewActor();

	// 미리보기 액터 위치 업데이트
	void UpdatePreviewLocation(const FVector& WorldLocation);

	// ========== 최종 배치 ==========

	// 장식품 최종 배치
	void PlaceDecoration(const FVector& WorldLocation);
};
