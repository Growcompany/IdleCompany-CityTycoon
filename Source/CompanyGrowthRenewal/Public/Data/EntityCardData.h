// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Table/BuildableCardTable.h"
#include "Table/DecorationCardTable.h"
#include "Table/WorkstationCardTable.h"
#include "EmployeeTypes.h"
#include "EntityCardData.generated.h"

/**
 * ListView Entry용 엔티티 카드 데이터
 * 빌딩, 책상, 장식 등 배치 가능한 엔티티의 카드 데이터를 담는 래퍼
 */
UCLASS(BlueprintType)
class COMPANYGROWTHRENEWAL_API UEntityCardData : public UObject
{
	GENERATED_BODY()

public:
	// 건물 카드 정보
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	FBuildableCardTable BuildableInfo;

	// 장식 카드 정보
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	FDecorationCardTable DecorationInfo;

	// 업무공간 카드 정보
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	FWorkstationCardTable WorkstationInfo;

	// 직원 정보 (EmployeeListCard용)
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	FEmployeeInstance EmployeeInfo;

	// 등급 (스크롤 점프용)
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	ELootBoxRarity Rarity;

	// 본사 레벨 해금 잠금 — RefreshBuildingCards 에서 채움. true 이면 카드 클릭 차단.
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	bool bLevelLocked = false;

	// 잠금 이유 텍스트 (레벨 잠금 시 카드 프레임에 표시)
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	FText LevelLockReason;

	// 빌딩 가치존(크기·인원) 표시용 — RefreshBuildingCards 에서 FBuildingData 로 채움.
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	int32 FootprintWidthCells = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	int32 FootprintDepthCells = 1;

	// 신축(증축 0층) 시점 인원. 층을 올리면 늘어나므로 "최대"가 아니다 — 카드도 "기본 N명"으로 쓴다.
	UPROPERTY(BlueprintReadWrite, Category = "Entity")
	int32 NewBuildEmployees = 0;
};
