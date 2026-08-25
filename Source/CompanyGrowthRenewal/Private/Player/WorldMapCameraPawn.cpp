// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/WorldMapCameraPawn.h"
#include "Player/Components/MovementInputHandler.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"

AWorldMapCameraPawn::AWorldMapCameraPawn()
	: Super()
{
	// 세계지도용 줌 범위 (모델 스케일에 맞춤)
	if (MovementInputHandler)
	{
		MovementInputHandler->MinZoomDistance = 5000.0f;
		MovementInputHandler->MaxZoomDistance = 200000.0f;

		// 타일 스트리밍으로 무한 이동 → 경계 제한 불필요
		MovementInputHandler->BoundaryRadius = 10000000.0f;
	}

	// SpringArm을 탑다운으로 설정
	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator(-70.0f, 0.0f, 0.0f));
		SpringArm->SocketOffset = FVector::ZeroVector;
		SpringArm->bEnableCameraLag = true;
		SpringArm->CameraLagSpeed = 8.0f;
	}

	// Collision 비활성화 (세계지도에서 불필요)
	if (Collision)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AWorldMapCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// MovementInputHandler만 바인딩 (Placement/Interactable 불필요)
	APawn::SetupPlayerInputComponent(PlayerInputComponent);

	if (MovementInputHandler)
	{
		MovementInputHandler->SetupPlayerInputComponent(PlayerInputComponent);
	}
}

void AWorldMapCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// 초기 줌 (멀리서 세계지도 전체가 보이도록)
	if (MovementInputHandler)
	{
		MovementInputHandler->SetZoomValue(0.7f);
		MovementInputHandler->ApplyZoomSettings();
	}
}

void AWorldMapCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ClampVerticalPosition();
}

void AWorldMapCameraPawn::ClampVerticalPosition()
{
	FVector Pos = GetActorLocation();

	// SpringArm 거리 기준으로 Y 범위 동적 조절
	// 가까이(ArmLength 작음) → 범위 넓음, 멀리(ArmLength 큼) → 범위 좁음
	float ArmLength = SpringArm ? SpringArm->TargetArmLength : 0.0f;
	float MinDist = MovementInputHandler ? MovementInputHandler->MinZoomDistance : 5000.0f;
	float MaxDist = MovementInputHandler ? MovementInputHandler->MaxZoomDistance : 200000.0f;

	// 0(가까이) ~ 1(멀리) 비율 계산
	float ZoomRatio = FMath::Clamp((ArmLength - MinDist) / (MaxDist - MinDist), 0.0f, 1.0f);

	// 가까이: 100% 범위, 멀리: 30% 범위
	float RangeScale = FMath::Lerp(1.0f, 0.5f, ZoomRatio);
	float DynamicMinY = MinY * RangeScale;
	float DynamicMaxY = MaxY * RangeScale;

	Pos.Y = FMath::Clamp(Pos.Y, DynamicMinY, DynamicMaxY);
	SetActorLocation(Pos);
}

FIntPoint AWorldMapCameraPawn::GetCurrentTileCoord() const
{
	const FVector Pos = GetActorLocation();

	// X축만 타일 좌표 계산 (Y축은 타일링 안 함 → 항상 0)
	const int32 TileX = FMath::FloorToInt(Pos.X / TileSize.X);

	return FIntPoint(TileX, 0);
}
