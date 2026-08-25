// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Components/WallPlacementHandler.h"
#include "Office/DecorationActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"

UWallPlacementHandler::UWallPlacementHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWallPlacementHandler::BeginPlay()
{
	Super::BeginPlay();
}

// ========== 배치 모드 제어 ==========

void UWallPlacementHandler::StartPlacement(TSubclassOf<ADecorationActor> DecorationClass, EWallSide WallSide)
{
	if (!DecorationClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[WallPlacementHandler] StartPlacement: DecorationClass is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[WallPlacementHandler] StartPlacement: %s on %s wall"),
		*DecorationClass->GetName(),
		WallSide == EWallSide::Left ? TEXT("Left") : TEXT("Right"));

	bIsPlacing = true;
	DecorationClassToPlace = DecorationClass;
	TargetWall = WallSide;

	// 미리보기 액터 생성
	CreatePreviewActor();

	// Input Mapping Context 추가
	if (PlacementMappingContext)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(PlacementMappingContext, MappingPriority);
				UE_LOG(LogTemp, Warning, TEXT("  PlacementMappingContext added"));
			}
		}
	}
}

void UWallPlacementHandler::EndPlacement()
{
	UE_LOG(LogTemp, Warning, TEXT("[WallPlacementHandler] EndPlacement"));

	bIsPlacing = false;
	DecorationClassToPlace = nullptr;

	// 미리보기 액터 제거
	DestroyPreviewActor();

	// Input Mapping Context 제거
	if (PlacementMappingContext)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(PlacementMappingContext);
				UE_LOG(LogTemp, Warning, TEXT("  PlacementMappingContext removed"));
			}
		}
	}
}

// ========== Enhanced Input 설정 ==========

void UWallPlacementHandler::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("[WallPlacementHandler] PlayerInputComponent is not UEnhancedInputComponent"));
		return;
	}

	if (!TouchAction)
	{
		UE_LOG(LogTemp, Error, TEXT("[WallPlacementHandler] TouchAction is not set"));
		return;
	}

	// 터치 입력 바인딩
	EnhancedInput->BindAction(TouchAction, ETriggerEvent::Started, this, &UWallPlacementHandler::OnTouchStarted);
	EnhancedInput->BindAction(TouchAction, ETriggerEvent::Ongoing, this, &UWallPlacementHandler::OnTouchOngoing);
	EnhancedInput->BindAction(TouchAction, ETriggerEvent::Triggered, this, &UWallPlacementHandler::OnTouchTriggered);

	UE_LOG(LogTemp, Warning, TEXT("[WallPlacementHandler] Input actions bound successfully"));
}

// ========== 입력 콜백 ==========

void UWallPlacementHandler::OnTouchStarted(const FInputActionValue& Value)
{
	if (!bIsPlacing)
	{
		return;
	}

	FVector2D ScreenPosition = Value.Get<FVector2D>();
	UE_LOG(LogTemp, Log, TEXT("[WallPlacementHandler] Touch Started at: (%.1f, %.1f)"),
		ScreenPosition.X, ScreenPosition.Y);

	// 미리보기 액터 위치 업데이트
	FVector WorldLocation;
	if (ScreenToWallLocation(ScreenPosition, WorldLocation))
	{
		UpdatePreviewLocation(WorldLocation);
	}
}

void UWallPlacementHandler::OnTouchOngoing(const FInputActionValue& Value)
{
	if (!bIsPlacing)
	{
		return;
	}

	FVector2D ScreenPosition = Value.Get<FVector2D>();

	// 미리보기 액터 위치 업데이트
	FVector WorldLocation;
	if (ScreenToWallLocation(ScreenPosition, WorldLocation))
	{
		UpdatePreviewLocation(WorldLocation);
	}
}

void UWallPlacementHandler::OnTouchTriggered(const FInputActionValue& Value)
{
	if (!bIsPlacing)
	{
		return;
	}

	FVector2D ScreenPosition = Value.Get<FVector2D>();
	UE_LOG(LogTemp, Warning, TEXT("[WallPlacementHandler] Touch Triggered at: (%.1f, %.1f)"),
		ScreenPosition.X, ScreenPosition.Y);

	// 최종 배치
	FVector WorldLocation;
	if (ScreenToWallLocation(ScreenPosition, WorldLocation))
	{
		if (CanPlaceAt(WorldLocation))
		{
			PlaceDecoration(WorldLocation);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  Cannot place at this location"));
		}
	}
}

// ========== 배치 검증 ==========

bool UWallPlacementHandler::CanPlaceAt(const FVector& WorldLocation) const
{
	// 벽 범위 내에 있는지 체크
	if (WorldLocation.X < WallXMin || WorldLocation.X > WallXMax)
	{
		UE_LOG(LogTemp, Log, TEXT("  Out of X range: %.1f (%.1f ~ %.1f)"),
			WorldLocation.X, WallXMin, WallXMax);
		return false;
	}

	if (WorldLocation.Z < WallZMin || WorldLocation.Z > WallZMax)
	{
		UE_LOG(LogTemp, Log, TEXT("  Out of Z range: %.1f (%.1f ~ %.1f)"),
			WorldLocation.Z, WallZMin, WallZMax);
		return false;
	}

	// 다른 장식품과 겹치는지 체크
	if (IsOverlapping(WorldLocation))
	{
		UE_LOG(LogTemp, Log, TEXT("  Overlapping with other decoration"));
		return false;
	}

	return true;
}

bool UWallPlacementHandler::IsOverlapping(const FVector& WorldLocation) const
{
	// TODO: 주변에 이미 배치된 장식품이 있는지 체크
	// 현재는 항상 false 반환 (배치 가능)

	return false;
}

// ========== 좌표 변환 ==========

bool UWallPlacementHandler::ScreenToWallLocation(const FVector2D& ScreenPosition, FVector& OutWorldLocation) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return false;
	}

	// 스크린 좌표를 월드 좌표로 변환 (Raycast)
	FVector WorldPosition, WorldDirection;
	if (!PC->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldPosition, WorldDirection))
	{
		return false;
	}

	// 벽의 Y 좌표 (왼쪽 또는 오른쪽)
	float WallY = (TargetWall == EWallSide::Left) ? LeftWallYPosition : RightWallYPosition;

	// Ray와 벽 평면의 교점 계산
	// 벽 평면: Y = WallY (고정)
	// Ray: WorldPosition + t * WorldDirection
	// 교점: WorldPosition.Y + t * WorldDirection.Y = WallY
	// t = (WallY - WorldPosition.Y) / WorldDirection.Y

	if (FMath::IsNearlyZero(WorldDirection.Y))
	{
		// Ray가 벽 평면과 평행
		return false;
	}

	float t = (WallY - WorldPosition.Y) / WorldDirection.Y;

	if (t < 0)
	{
		// Ray가 벽 평면의 반대 방향을 향함
		return false;
	}

	// 교점 계산
	OutWorldLocation = WorldPosition + t * WorldDirection;
	OutWorldLocation.Y = WallY;  // 정확히 벽 표면에 위치

	UE_LOG(LogTemp, VeryVerbose, TEXT("  Wall location: (%.1f, %.1f, %.1f)"),
		OutWorldLocation.X, OutWorldLocation.Y, OutWorldLocation.Z);

	return true;
}

// ========== 미리보기 ==========

void UWallPlacementHandler::CreatePreviewActor()
{
	if (!DecorationClassToPlace)
	{
		return;
	}

	if (PreviewActor)
	{
		DestroyPreviewActor();
	}

	// 미리보기 액터 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PreviewActor = GetWorld()->SpawnActor<ADecorationActor>(DecorationClassToPlace, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!PreviewActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[WallPlacementHandler] Failed to spawn preview actor"));
		return;
	}

	// 미리보기 모드 설정 (반투명, 충돌 비활성화 등)
	PreviewActor->SetActorEnableCollision(false);

	// TODO: 머티리얼을 반투명으로 변경

	UE_LOG(LogTemp, Warning, TEXT("[WallPlacementHandler] Preview actor created"));
}

void UWallPlacementHandler::DestroyPreviewActor()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("[WallPlacementHandler] Preview actor destroyed"));
	}
}

void UWallPlacementHandler::UpdatePreviewLocation(const FVector& WorldLocation)
{
	if (!PreviewActor)
	{
		return;
	}

	// 배치 가능 여부에 따라 색상 변경
	bool bCanPlace = CanPlaceAt(WorldLocation);

	// TODO: 배치 가능 여부에 따라 색상 변경

	// 미리보기 액터 위치 업데이트
	PreviewActor->SetActorLocation(WorldLocation);

	// 벽을 향하도록 회전 설정
	FRotator Rotation = FRotator::ZeroRotator;
	if (TargetWall == EWallSide::Left)
	{
		Rotation.Yaw = 90.0f;  // 왼쪽 벽을 향함
	}
	else // Right
	{
		Rotation.Yaw = -90.0f;  // 오른쪽 벽을 향함
	}
	PreviewActor->SetActorRotation(Rotation);
}

// ========== 최종 배치 ==========

void UWallPlacementHandler::PlaceDecoration(const FVector& WorldLocation)
{
	if (!DecorationClassToPlace)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[WallPlacementHandler] PlaceDecoration at (%.1f, %.1f, %.1f)"),
		WorldLocation.X, WorldLocation.Y, WorldLocation.Z);

	// 장식품 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FRotator Rotation = FRotator::ZeroRotator;
	if (TargetWall == EWallSide::Left)
	{
		Rotation.Yaw = 90.0f;
	}
	else // Right
	{
		Rotation.Yaw = -90.0f;
	}

	ADecorationActor* NewDecoration = GetWorld()->SpawnActor<ADecorationActor>(
		DecorationClassToPlace,
		WorldLocation,
		Rotation,
		SpawnParams
	);

	if (NewDecoration)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Decoration placed successfully"));

		// TODO: 배치 완료 후 처리
		// - 플레이어 재화 차감
		// - 배치 기록 저장
		// - UI 업데이트 등
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  Failed to place decoration"));
	}
}
