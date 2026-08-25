// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Office/WorkstationTypes.h"
#include "WorkstationCardTable.generated.h"

class UTexture2D;

/**
 * 업무공간 카드 데이터 테이블
 *
 * 배치 UI에서 표시할 업무공간(책상) 정보:
 * - 표시 정보 (이름, 설명, 아이콘)
 * - 해금 조건
 * - 기본 스탯 (책상 자체 보너스)
 * - 의자 개수 (1인용, 2인용, 4인용 등)
 * - 블루프린트 클래스 참조
 */
USTRUCT(BlueprintType)
struct FWorkstationCardTable : public FTableRowBase
{
	GENERATED_BODY()

	// 데이터 테이블의 Row Name (런타임에 할당)
	UPROPERTY(BlueprintReadOnly)
	FName RowName;

	// ========== 표시 정보 ==========

	// 업무공간 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText DisplayName;

	// 업무공간 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText Description;

	// UI 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TSoftObjectPtr<UTexture2D> UIIcon;

	// ========== 해금 조건 ==========

	// 해금 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unlock")
	int32 UnlockLevel = 1;

	// ========== 업무공간 속성 ==========

	// 업무공간 타입 (싱글/더블)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	EWorkstationType WorkstationType = EWorkstationType::Single;

	// 그리드 크기 (배치용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FIntPoint GridSize = FIntPoint(1, 1);

	// ========== 기본 스탯 (책상 자체 보너스) ==========

	// 기본 업무 효율 보너스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float BaseEfficiencyBonus = 0.0f;

	// ========== 슬롯 해금 ==========

	// 모니터 슬롯 해금 여부 (기본 제공 or 추가 구매)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slots")
	bool bMonitorSlotUnlocked = false;

	// 키보드 슬롯 해금 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slots")
	bool bKeyboardSlotUnlocked = false;

	// 본체 슬롯 해금 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slots")
	bool bComputerSlotUnlocked = false;

	// 액세서리1 슬롯 해금 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slots")
	bool bAccessory1SlotUnlocked = false;

	// 액세서리2 슬롯 해금 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slots")
	bool bAccessory2SlotUnlocked = false;

	// ========== 블루프린트 참조 ==========

	// 스폰할 WorkstationActor 블루프린트 클래스
	// 각 책상 타입마다 별도의 BP를 만들고 여기서 참조
	// Single: BP_SingleWorkstation_*, Double: BP_DoubleWorkstation_*
	// 기존 BP 호환을 위해 AWorkstationActorBase 사용 (Single/Double 모두 상속)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint")
	TSoftClassPtr<class AWorkstationActorBase> BlueprintClass;

	FWorkstationCardTable()
		: UnlockLevel(1)
		, WorkstationType(EWorkstationType::Single)
		, GridSize(FIntPoint(1, 1))
		, BaseEfficiencyBonus(0.0f)
		, bMonitorSlotUnlocked(false)
		, bKeyboardSlotUnlocked(false)
		, bComputerSlotUnlocked(false)
		, bAccessory1SlotUnlocked(false)
		, bAccessory2SlotUnlocked(false)
	{}

	// ========== 유틸리티 ==========

	FString ToString() const
	{
		return FString::Printf(TEXT("Name: %s, Type: %s, Grid: %dx%d, Unlock: Lv%d"),
			*DisplayName.ToString(),
			(WorkstationType == EWorkstationType::Double) ? TEXT("Double") : TEXT("Single"),
			GridSize.X, GridSize.Y, UnlockLevel);
	}

	// 의자 개수 반환 (편의 함수)
	int32 GetChairCount() const
	{
		return (WorkstationType == EWorkstationType::Double) ? 2 : 1;
	}
};
