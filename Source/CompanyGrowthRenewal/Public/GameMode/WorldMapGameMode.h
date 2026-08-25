// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WorldMapGameMode.generated.h"

/**
 * WorldMap 전용 GameMode
 *
 * - DefaultPawn: WorldMapCameraPawn
 * - PlayerController: WorldMapPlayerController
 * - 타일 스트리밍: 카메라 주변 3x3 타일을 동적 생성/삭제하여 무한 스크롤
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AWorldMapGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWorldMapGameMode();

protected:
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// ========== 타일 스트리밍 ==========

	// 원본 타일에 포함된 StaticMesh 액터들의 정보 (BeginPlay에서 수집)
	struct FTileMeshInfo
	{
		UStaticMesh* Mesh = nullptr;
		TArray<UMaterialInterface*> Materials;
		FTransform RelativeTransform; // 원본 타일 내 상대 위치
	};
	TArray<FTileMeshInfo> OriginalMeshInfos;

	// 생성된 타일 복사본들 (타일 좌표 → 스폰된 액터 배열)
	TMap<FIntPoint, TArray<AActor*>> SpawnedTiles;

	// 이전 프레임 카메라 타일 좌표 (변경 시에만 업데이트)
	FIntPoint LastCameraTileCoord = FIntPoint(INT32_MAX, INT32_MAX);

	// 카메라 X 변화 게이트 — Tick에서 두 업데이트 함수 호출 자체를 차단
	float LastCameraX = TNumericLimits<float>::Max();

	// 원본 타일의 메시 정보 수집
	void CollectOriginalTileMeshes();

	// 카메라 위치 기반 타일 업데이트
	void UpdateTiles();

	// 특정 타일 좌표에 메시 복사본 스폰
	void SpawnTile(const FIntPoint& TileCoord);

	// 특정 타일 좌표의 메시 복사본 제거
	void DestroyTile(const FIntPoint& TileCoord);

	// ========== Actor 래핑 (CountryActor, FacilityActor 등) ==========

	// 래핑 대상 액터 목록 + 원본 X 위치
	struct FWrappableActorInfo
	{
		AActor* Actor = nullptr;
		float OriginalX = 0.0f;
	};
	TArray<FWrappableActorInfo> WrappableActors;

	// 레벨의 래핑 대상 액터 수집
	void CollectWrappableActors();

	// 카메라 기준으로 액터 위치 래핑
	void UpdateWrappableActorPositions();
};
