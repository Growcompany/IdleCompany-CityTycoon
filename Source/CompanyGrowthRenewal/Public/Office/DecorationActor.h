// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Office/DecorationTypes.h"
#include "DecorationActor.generated.h"

class UDataTable;
class UBoxComponent;
struct FDecorationCardTable;
struct FDecorationData;

/**
 * 모든 장식물의 베이스 클래스
 * 벽 장식, 바닥 장식, 가구 모두 이 클래스를 상속
 *
 * DataTable 패턴 사용:
 * - RowName으로 DT_DecorationCardTable에서 게임플레이 데이터 로드
 * - 동일한 RowName으로 DT_DecorationData에서 비주얼 데이터 로드
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ADecorationActor : public AActor
{
	GENERATED_BODY()

public:
	ADecorationActor();

protected:
	virtual void BeginPlay() override;

public:
	// ========== 컴포넌트 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoration")
	USceneComponent* RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoration")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoration")
	UBoxComponent* BoxComponent;
	// ========== DataTable 참조 ==========

	// DecorationCardTable의 Row Name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration|DataTable")
	FName DecorationCardTableRowName;

	// DecorationCardTable DataTable 참조
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration|DataTable")
	UDataTable* DecorationCardTable;

	// DecorationData DataTable 참조
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration|DataTable")
	UDataTable* DecorationDataTable;

	// ========== 캐시된 데이터 (런타임에 DataTable에서 로드) ==========

	// 장식물 카테고리 (캐시됨)
	UPROPERTY(BlueprintReadOnly, Category = "Decoration|Cached")
	EDecorationCategory Category;

	// 배치 가능한 표면 타입 (캐시됨)
	UPROPERTY(BlueprintReadOnly, Category = "Decoration|Cached")
	EDecorationSurface AllowedSurface;

	// 장식물 이름 (캐시됨)
	UPROPERTY(BlueprintReadOnly, Category = "Decoration|Cached")
	FText DecorationName;

	// 장식물 설명 (캐시됨)
	UPROPERTY(BlueprintReadOnly, Category = "Decoration|Cached")
	FText Description;

	// 구매 가격 (캐시됨)
	UPROPERTY(BlueprintReadOnly, Category = "Decoration|Cached")
	int32 Price = 0;

	// 해금 레벨 요구사항 (캐시됨)
	UPROPERTY(BlueprintReadOnly, Category = "Decoration|Cached")
	int32 UnlockLevel = 1;

public:
	// ========== 공개 함수 ==========

	/**
	 * 장식물 메시 설정
	 */
	UFUNCTION(BlueprintCallable, Category = "Decoration")
	void SetDecorationMesh(UStaticMesh* NewMesh);

	/**
	 * BoxComponent 크기 재계산 (메시 바운드에 맞게)
	 */
	void RecalcBoxExtent();

	/**
	 * 장식물이 특정 표면에 배치 가능한지 확인
	 */
	UFUNCTION(BlueprintCallable, Category = "Decoration")
	bool CanPlaceOnSurface(EDecorationSurface Surface) const;

	/**
	 * 장식물이 해금되었는지 확인
	 */
	UFUNCTION(BlueprintCallable, Category = "Decoration")
	bool IsUnlocked(int32 CurrentLevel) const;

	/**
	 * 배치 가능한 표면 타입 가져오기
	 */
	UFUNCTION(BlueprintCallable, Category = "Decoration")
	EDecorationSurface GetAllowedSurface() const { return AllowedSurface; }

	/**
	 * 장식물 카테고리 가져오기
	 */
	UFUNCTION(BlueprintCallable, Category = "Decoration")
	EDecorationCategory GetCategory() const { return Category; }
};
