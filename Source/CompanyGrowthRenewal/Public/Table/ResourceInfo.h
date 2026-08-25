// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ResourceType.h"
#include "ResourceInfo.generated.h"

// 재화 정보 DataTable Row 구조체
// Row Name 예시: "Brick", "Money", "Diamond"
USTRUCT(BlueprintType)
struct FResourceInfo : public FTableRowBase
{
	GENERATED_BODY()

	// 재화 타입 (Enum)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	EResourceType ResourceType = EResourceType::None;

	// 표시 이름 (현지화 지원)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText DisplayName;

	// Tooltip/상세 화면용 짧은 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (MultiLine = "true"))
	FText Description;

	// 아이콘 이미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	// UI 색상 (선택적)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FLinearColor UIColor = FLinearColor::White;

	// 초기 지급량 (게임 시작 시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int64 InitialAmount = 0;

	// 최대 보유량 (-1이면 무제한)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int64 MaxAmount = -1;

	// 프리미엄 재화 여부 (현금으로 구매 가능한지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bIsPremium = false;

	// Money 기준 기준가. 0 이면 비-거래 자원 (UI 에서 가격 영역 collapse)
	// 무역/판매 시스템은 이 값을 baseline 으로 +/- 보정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 BasePrice = 0;
};
