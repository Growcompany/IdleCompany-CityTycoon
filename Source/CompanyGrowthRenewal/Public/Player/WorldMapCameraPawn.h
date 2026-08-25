// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/PlayerCamera.h"
#include "WorldMapCameraPawn.generated.h"

/**
 * WorldMap 전용 카메라 Pawn
 *
 * 특징:
 * - PlayerCamera를 상속받아 동일한 이동/줌 구조 사용
 * - 회전(Spin) 기능 비활성화
 * - 탑다운 카메라 각도
 * - 자유 이동 (래핑 없음, 타일 스트리밍으로 무한 스크롤)
 * - PlacementHandler, InteractableInputHandler 비활성화
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AWorldMapCameraPawn : public APlayerCamera
{
	GENERATED_BODY()

public:
	AWorldMapCameraPawn();

	// 카메라가 현재 어느 타일 좌표에 있는지 반환
	FIntPoint GetCurrentTileCoord() const;

	// 타일 1칸의 월드 크기
	// model 메시 크기(2422 x 1285) * Scale(50) = 121100 x 64250
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldMap Camera")
	FVector2D TileSize = FVector2D(121100.0, 64250.0);

	// Y축 이동 제한 (상하 경계)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldMap Camera")
	float MinY = -50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldMap Camera")
	float MaxY = 50000.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	// Y축 카메라 위치 제한
	void ClampVerticalPosition();
};
