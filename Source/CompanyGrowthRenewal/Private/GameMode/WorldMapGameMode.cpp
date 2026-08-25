// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/WorldMapGameMode.h"
#include "Player/WorldMapCameraPawn.h"
#include "Player/WorldMapPlayerController.h"
#include "Manager/UIManagerSubsystem.h"
#include "Entity/Country/CountryActor.h"
#include "Entity/Facility/FacilityBaseActor.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AWorldMapGameMode::AWorldMapGameMode()
{
	DefaultPawnClass = AWorldMapCameraPawn::StaticClass();
	PlayerControllerClass = AWorldMapPlayerController::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AWorldMapGameMode::StartPlay()
{
	Super::StartPlay();

	CollectOriginalTileMeshes();
	CollectWrappableActors();

	UE_LOG(LogTemp, Log, TEXT("[WorldMapGameMode] StartPlay - Collected %d meshes for tiling, %d wrappable actors"),
		OriginalMeshInfos.Num(), WrappableActors.Num());

	// 시작 시 즉시 주변 타일 생성 (첫 프레임부터 보이도록)
	UpdateTiles();

	// WorldMap UI 표시
	if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
	{
		UIManager->ShowWorldMapUI();
	}
}

void AWorldMapGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 카메라가 X축으로 움직이지 않으면 타일/래핑 갱신은 결과가 같으므로 skip
	AWorldMapCameraPawn* CameraPawn = Cast<AWorldMapCameraPawn>(
		UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!CameraPawn) return;

	const float CameraX = CameraPawn->GetActorLocation().X;
	if (FMath::IsNearlyEqual(CameraX, LastCameraX, 0.1f)) return;
	LastCameraX = CameraX;

	UpdateTiles();
	UpdateWrappableActorPositions();
}

void AWorldMapGameMode::CollectOriginalTileMeshes()
{
	// WorldMap 레벨에 배치된 모든 StaticMeshActor를 수집
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStaticMeshActor::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		if (!MeshActor) continue;

		UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
		if (!MeshComp || !MeshComp->GetStaticMesh()) continue;

		FTileMeshInfo Info;
		Info.Mesh = MeshComp->GetStaticMesh();
		Info.RelativeTransform = MeshActor->GetActorTransform();

		// 머티리얼 복사
		for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
		{
			Info.Materials.Add(MeshComp->GetMaterial(i));
		}

		OriginalMeshInfos.Add(Info);
	}

	// 원본 타일 (0,0)은 이미 레벨에 존재하므로 SpawnedTiles에 빈 배열로 등록
	SpawnedTiles.Add(FIntPoint(0, 0), TArray<AActor*>());
}

void AWorldMapGameMode::UpdateTiles()
{
	AWorldMapCameraPawn* CameraPawn = Cast<AWorldMapCameraPawn>(
		UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!CameraPawn) return;

	const FIntPoint CurrentTile = CameraPawn->GetCurrentTileCoord();

	// 타일 좌표가 변경되지 않았으면 아무것도 안 함
	if (CurrentTile == LastCameraTileCoord) return;
	LastCameraTileCoord = CurrentTile;

	// 카메라 주변 좌우 타일만 필요 (Y축은 타일링 안 함)
	TSet<FIntPoint> NeededTiles;
	for (int32 dx = -1; dx <= 1; dx++)
	{
		NeededTiles.Add(FIntPoint(CurrentTile.X + dx, 0));
	}

	// 범위 밖 타일 삭제
	TArray<FIntPoint> TilesToRemove;
	for (auto& Pair : SpawnedTiles)
	{
		if (!NeededTiles.Contains(Pair.Key))
		{
			TilesToRemove.Add(Pair.Key);
		}
	}
	for (const FIntPoint& Coord : TilesToRemove)
	{
		DestroyTile(Coord);
	}

	// 필요한데 아직 없는 타일 생성
	for (const FIntPoint& Coord : NeededTiles)
	{
		if (!SpawnedTiles.Contains(Coord))
		{
			SpawnTile(Coord);
		}
	}
}

void AWorldMapGameMode::SpawnTile(const FIntPoint& TileCoord)
{
	AWorldMapCameraPawn* CameraPawn = Cast<AWorldMapCameraPawn>(
		UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!CameraPawn) return;

	const FVector2D TileSize = CameraPawn->TileSize;
	const FVector TileOffset(TileCoord.X * TileSize.X, TileCoord.Y * TileSize.Y, 0.0f);

	TArray<AActor*> SpawnedActors;

	for (const FTileMeshInfo& Info : OriginalMeshInfos)
	{
		// 원본 위치에 타일 오프셋만 추가한 새 위치
		FVector NewLocation = Info.RelativeTransform.GetLocation() + TileOffset;

		// Deferred 스폰으로 메시/머티리얼 설정 후 FinishSpawning
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AStaticMeshActor* NewActor = GetWorld()->SpawnActorDeferred<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), FTransform::Identity);

		if (NewActor)
		{
			UStaticMeshComponent* MeshComp = NewActor->GetStaticMeshComponent();

			// Movable로 설정 (런타임 변경 허용)
			MeshComp->SetMobility(EComponentMobility::Movable);

			// 메시 설정
			MeshComp->SetStaticMesh(Info.Mesh);

			// 머티리얼 적용
			for (int32 i = 0; i < Info.Materials.Num(); i++)
			{
				if (Info.Materials[i])
				{
					MeshComp->SetMaterial(i, Info.Materials[i]);
				}
			}

			// 원본과 동일한 Transform + 타일 오프셋 적용
			FTransform FinalTransform = Info.RelativeTransform;
			FinalTransform.SetLocation(NewLocation);

			NewActor->FinishSpawning(FinalTransform);

			SpawnedActors.Add(NewActor);
		}
	}

	SpawnedTiles.Add(TileCoord, SpawnedActors);

	UE_LOG(LogTemp, Log, TEXT("[WorldMapGameMode] Spawned tile (%d, %d) - %d actors"),
		TileCoord.X, TileCoord.Y, SpawnedActors.Num());
}

void AWorldMapGameMode::DestroyTile(const FIntPoint& TileCoord)
{
	if (TArray<AActor*>* Actors = SpawnedTiles.Find(TileCoord))
	{
		for (AActor* Actor : *Actors)
		{
			if (Actor)
			{
				Actor->Destroy();
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[WorldMapGameMode] Destroyed tile (%d, %d)"),
			TileCoord.X, TileCoord.Y);
	}

	SpawnedTiles.Remove(TileCoord);
}

void AWorldMapGameMode::CollectWrappableActors()
{
	// CountryActor 수집
	TArray<AActor*> CountryActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACountryActor::StaticClass(), CountryActors);
	for (AActor* Actor : CountryActors)
	{
		FWrappableActorInfo Info;
		Info.Actor = Actor;
		Info.OriginalX = Actor->GetActorLocation().X;
		WrappableActors.Add(Info);
	}

	// FacilityBaseActor 수집
	TArray<AActor*> FacilityActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFacilityBaseActor::StaticClass(), FacilityActors);
	for (AActor* Actor : FacilityActors)
	{
		FWrappableActorInfo Info;
		Info.Actor = Actor;
		Info.OriginalX = Actor->GetActorLocation().X;
		WrappableActors.Add(Info);
	}
}

void AWorldMapGameMode::UpdateWrappableActorPositions()
{
	AWorldMapCameraPawn* CameraPawn = Cast<AWorldMapCameraPawn>(
		UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!CameraPawn) return;

	const float TileWidth = CameraPawn->TileSize.X;
	const float CameraX = CameraPawn->GetActorLocation().X;
	const float HalfTile = TileWidth * 0.5f;

	for (FWrappableActorInfo& Info : WrappableActors)
	{
		if (!Info.Actor) continue;

		FVector Pos = Info.Actor->GetActorLocation();
		float DeltaX = Pos.X - CameraX;

		// 카메라 기준 ±반타일 범위를 벗어나면 반대쪽으로 텔레포트
		if (DeltaX > HalfTile)
		{
			Pos.X -= TileWidth;
			Info.Actor->SetActorLocation(Pos);
		}
		else if (DeltaX < -HalfTile)
		{
			Pos.X += TileWidth;
			Info.Actor->SetActorLocation(Pos);
		}
	}
}
