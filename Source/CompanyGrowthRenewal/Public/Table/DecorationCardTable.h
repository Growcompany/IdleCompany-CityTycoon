// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Office/DecorationTypes.h"
#include "DecorationCardTable.generated.h"

/**
 * 장식품 카드 데이터 테이블
 *
 * 각 장식품의 게임플레이 데이터를 관리:
 * - 표시 정보 (이름, 설명, 아이콘)
 * - 카테고리 및 배치 표면
 * - 가격 및 해금 조건
 * - 그리드 크기 (가구의 경우)
 */
USTRUCT(BlueprintType)
struct FDecorationCardTable : public FTableRowBase
{
	GENERATED_BODY()

	// 데이터 테이블의 Row Name을 저장 (런타임에 할당됨)
	UPROPERTY(BlueprintReadOnly)
	FName RowName;

	// ========== 표시 정보 ==========

	// 장식품 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FName Name;

	// 장식품 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText Description;

	// UI 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> UIIcon;

	// ========== 카테고리 ==========

	// 배치 가능한 표면 (Wall, Floor, Grid)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	EDecorationSurface AllowedSurface;

	// 장식품 카테고리 (WallPicture, FloorProp, Desk, Chair 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	EDecorationCategory Category;

	// ========== 가격 및 해금 ==========

	// 구매 가격 (게임 내 화폐)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 Price;

	// 해금 레벨 (플레이어가 이 레벨에 도달해야 구매 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 UnlockLevel;

	// 프리미엄 아이템 여부 (실제 화폐로만 구매 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	bool bIsPremium;

	// ========== 생성자 ==========

	FDecorationCardTable()
		: AllowedSurface(EDecorationSurface::Grid)
		, Category(EDecorationCategory::Furniture)
		, Price(100)
		, UnlockLevel(1)
		, bIsPremium(false)
	{}

	// ========== 유틸리티 함수 ==========

	FString ToString() const
	{
		FString Out = "";
		Out += "Name: " + Name.ToString() + ", ";
		Out += "Category: " + StaticEnum<EDecorationCategory>()->GetNameStringByValue((int64)Category) + ", ";
		Out += "Surface: " + StaticEnum<EDecorationSurface>()->GetNameStringByValue((int64)AllowedSurface) + ", ";
		Out += "Price: " + FString::FromInt(Price) + ", ";
		Out += "UnlockLevel: " + FString::FromInt(UnlockLevel);

		return Out;
	}
};
