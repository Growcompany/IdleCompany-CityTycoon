// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enum/OfficeExpansionType.h"
#include "Data/BuildingSaveData.h"
#include "Office/DecorationTypes.h"
#include "OfficeInterior.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnOfficeFootprintChanged, FIntPoint);

/**
 * Office 내부 인테리어 관리 클래스
 * - 벽, 바닥 메시 관리
 * - NavMesh 영역 정의
 * - 직원 스폰 가능 영역 제공
 * - 가구/장식 배치 관리
 * - 타일 기반 오피스 확장 시스템 (좌/우 방향)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeInterior : public AActor
{
	GENERATED_BODY()

public:
	AOfficeInterior();

	FOnOfficeFootprintChanged OnFootprintChanged;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// ========== 컴포넌트 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office")
	USceneComponent* RootScene;

	// 바닥 컴포넌트 (메시는 블루프린트에서 설정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Floor")
	UStaticMeshComponent* Floor;

	// 왼쪽 벽 컴포넌트 (메시는 블루프린트에서 설정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Walls")
	UStaticMeshComponent* WallLeft;

	// 오른쪽 벽 컴포넌트 (메시는 블루프린트에서 설정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Walls")
	UStaticMeshComponent* WallRight;

	// NavMesh가 생성될 영역을 정의하는 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Navigation")
	UBoxComponent* NavMeshBounds;

	// ========== 타일 설정 ==========

	// 타일 하나의 크기 (언리얼 유닛, 400x400)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office|Tile")
	float TileSize = 400.f;

	// 현재 X축 방향 타일 개수 (초기값 1)
	UPROPERTY(BlueprintReadOnly, Category = "Office|Tile|Runtime")
	int32 TileCountX = 1;

	// 현재 Y축 방향 타일 개수 (초기값 2)
	UPROPERTY(BlueprintReadOnly, Category = "Office|Tile|Runtime")
	int32 TileCountY = 2;

	// 시작 타일 수 (스타터 프리셋 부여분 — 확장 비용 지수의 기준점)
	UPROPERTY(BlueprintReadOnly, Category = "Office|Tile|Runtime")
	int32 StarterTileCountX = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Office|Tile|Runtime")
	int32 StarterTileCountY = 2;

	// 최대 X축 방향 타일 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office|Tile")
	int32 MaxTileCountX = 5;

	// 최대 Y축 방향 타일 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office|Tile")
	int32 MaxTileCountY = 6;

	// ========== 바닥 타일 ==========

	// 현재 사용 중인 바닥 타일 메시 (테이블에서 로드)
	UPROPERTY()
	UStaticMesh* FloorTileMesh;

	// 현재 선택된 바닥 타일의 DecorationData RowName
	UPROPERTY(BlueprintReadOnly, Category = "Office|Floor")
	FName CurrentFloorTileRowName;

	// 동적으로 생성된 바닥 타일 컴포넌트들
	UPROPERTY()
	TArray<UStaticMeshComponent*> FloorTiles;

	// ========== 기본 설정 ==========

	// 바닥 크기 (언리얼 유닛) - 타일 기반으로 계산됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office|Size")
	FVector2D FloorSize = FVector2D(400.f, 800.f);

	// 벽 높이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office|Size")
	float WallHeight = 400.f;

	// ========== 확장 비용 ==========

	// 방향별 확장 회차(현재-스타터 타일수)로 인덱싱하는 다이아 비용. 범위 밖은 마지막 값. BP_OfficeInterior 는 이 값을 덮지 않는다 — 여기가 정본.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office|Expansion")
	TArray<int32> ExpansionDiamondCosts = {30, 45, 68, 101};

public:
	// ========== NavMesh/스폰 관련 ==========

	/**
	 * NavMesh Bounds 영역 반환 (직원 스폰 시 사용)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Navigation")
	FBox GetNavMeshBoundsBox() const;

	/**
	 * 바닥 위 랜덤 위치 반환 (NavMesh 없이 바닥 영역 내)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Spawn")
	FVector GetRandomFloorLocation() const;

	/**
	 * 블루프린트에서 배치한 벽/바닥 컴포넌트 초기화
	 */
	UFUNCTION(BlueprintCallable, Category = "Office")
	void InitializeComponents();

	/**
	 * NavMesh Bounds 크기 업데이트
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Navigation")
	void UpdateNavMeshBounds();

	// ========== 타일 생성/관리 ==========

	/**
	 * 바닥 타일 생성 (타일 개수에 맞게)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Floor")
	void GenerateFloorTiles();

	/**
	 * 모든 바닥 타일 제거
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Floor")
	void ClearFloorTiles();

	/**
	 * 바닥 타일 메시 변경 (모든 타일에 적용)
	 * @param NewMesh 적용할 새 메시
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Floor")
	void SetFloorTileMesh(UStaticMesh* NewMesh);

	/**
	 * RowName으로 바닥 타일 로드 및 적용
	 * @param RowName DecorationData 테이블의 RowName
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Floor")
	void LoadFloorTileFromRowName(FName RowName);

	/**
	 * 현재 선택된 바닥 타일 RowName 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Floor")
	FName GetCurrentFloorTileRowName() const { return CurrentFloorTileRowName; }

	// ========== 확장 시스템 ==========

	/**
	 * 왼쪽으로 확장 (TileCountX++)
	 * @return 확장 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Expansion")
	bool ExpandLeft();

	/**
	 * 오른쪽으로 확장 (TileCountY++)
	 * @return 확장 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Expansion")
	bool ExpandRight();

	/**
	 * 해당 방향으로 확장 가능한지 확인
	 * @param Direction 확장 방향
	 * @return 확장 가능 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Expansion")
	bool CanExpand(EOfficeExpandDirection Direction) const;

	/**
	 * 해당 방향 확장 비용 계산
	 * @param Direction 확장 방향
	 * @return 확장 비용
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Expansion")
	int64 GetExpandCost(EOfficeExpandDirection Direction) const;

	/**
	 * 현재 오피스 크기 반환 (타일 기반 계산)
	 * @return 오피스 크기 (X: TileCountX * TileSize, Y: TileCountY * TileSize)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Expansion")
	FVector2D GetCurrentSize() const;

	/**
	 * 현재 타일 개수 반환
	 * @return (TileCountX, TileCountY)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Expansion")
	FIntPoint GetTileCount() const;

	/**
	 * 최대 타일 개수 반환
	 * @return (MaxTileCountX, MaxTileCountY)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Expansion")
	FIntPoint GetMaxTileCount() const;

	FBox2D GetCurrentFloorBoundsLocal() const;
	FBox2D GetMaxFloorBoundsLocal() const;
	float GetStructuralFloorZLocal() const;

	// ========== 벽 정보 (배치 시스템용) ==========

	/**
	 * 지정한 벽의 월드 위치 반환
	 * @param WallSide 벽 방향 (Left 또는 Right)
	 * @return 벽 중앙의 월드 위치
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Walls")
	FVector GetWallLocation(EWallSide WallSide) const;

	/**
	 * 지정한 벽의 법선 벡터 반환
	 * @param WallSide 벽 방향
	 * @return 벽의 법선 벡터 (장식품이 향할 방향)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Walls")
	FVector GetWallNormal(EWallSide WallSide) const;

	/**
	 * 지정한 벽의 FPlane 반환 (배치 투영용)
	 * @param WallSide 벽 방향
	 * @return 벽 평면
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Walls")
	FPlane GetWallPlane(EWallSide WallSide) const;

	/**
	 * 지정한 벽의 배치 가능 영역 반환 (Min, Max)
	 * @param WallSide 벽 방향
	 * @param OutMinBounds 최소 좌표 (왼쪽 하단)
	 * @param OutMaxBounds 최대 좌표 (오른쪽 상단)
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Walls")
	void GetWallPlacementBounds(EWallSide WallSide, FVector& OutMinBounds, FVector& OutMaxBounds) const;

	/**
	 * 벽 높이 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|Walls")
	float GetWallHeight() const { return WallHeight; }

	// ========== 저장/로드 ==========

	/**
	 * Office 저장 데이터에서 확장 상태 적용
	 * @param OfficeData 적용할 Office 저장 데이터
	 */
	UFUNCTION(BlueprintCallable, Category = "Office|SaveLoad")
	void ApplyOfficeSaveData(const FOfficeSaveData& OfficeData);

private:
	bool TryApplyValidatedFootprint(
		FIntPoint RequestedTileCount,
		FIntPoint RequestedStarterTileCount,
		bool bRequireExactRequestedTileCount);

	/**
	 * 확장에 따른 Floor/Wall 스케일 업데이트
	 */
	void UpdateScalesForExpansion();

	/**
	 * 확장 시 새 타일만 추가
	 */
	void AddTilesForExpansion(EOfficeExpandDirection Direction);
};
