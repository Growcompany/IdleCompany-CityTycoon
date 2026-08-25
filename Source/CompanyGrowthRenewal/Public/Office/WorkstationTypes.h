// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorkstationTypes.generated.h"

/**
 * 업무공간 슬롯 타입
 */
UENUM(BlueprintType)
enum class EWorkstationSlot : uint8
{
	Chair			UMETA(DisplayName = "의자"),
	Laptop			UMETA(DisplayName = "노트북"),
	Monitor			UMETA(DisplayName = "모니터"),
	Keyboard		UMETA(DisplayName = "키보드"),
	Mouse			UMETA(DisplayName = "마우스"),
	Computer		UMETA(DisplayName = "본체"),
	Accessory1		UMETA(DisplayName = "악세서리1"),
	Accessory2		UMETA(DisplayName = "악세서리2"),
	Max				UMETA(Hidden)
};

/**
 * 업무공간 타입 (의자 개수 기준)
 */
UENUM(BlueprintType)
enum class EWorkstationType : uint8
{
	Single		UMETA(DisplayName = "싱글 (의자 1개)"),
	Double		UMETA(DisplayName = "더블 (의자 2개)")
};

/**
 * 모니터 타입
 */
UENUM(BlueprintType)
enum class EMonitorType : uint8
{
	None		UMETA(DisplayName = "없음"),
	Flat		UMETA(DisplayName = "일반 모니터"),
	Curved		UMETA(DisplayName = "커브드 모니터"),
	Vertical	UMETA(DisplayName = "세로 모니터")
};

UENUM(BlueprintType)
enum class ELaptopPlacement : uint8
{
	None	UMETA(DisplayName = "없음"),
	Center	UMETA(DisplayName = "중앙"),
	Side	UMETA(DisplayName = "측면")
};

UENUM(BlueprintType)
enum class EPrimaryMonitorType : uint8
{
	None	UMETA(DisplayName = "없음"),
	Flat	UMETA(DisplayName = "평면"),
	Curved	UMETA(DisplayName = "커브드")
};

/**
 * 두 번째 모니터. 구 설계의 두 듀얼 레이아웃이 정확히 이 축에서 갈린다.
 * Curved 일 때 주모니터는 Monitor_Single 이 아니라 Monitor_DualLeft 에 놓인다 (구 Lv3B 그대로).
 */
UENUM(BlueprintType)
enum class ESecondMonitorType : uint8
{
	None		UMETA(DisplayName = "없음"),
	Vertical	UMETA(DisplayName = "세로"),
	Curved		UMETA(DisplayName = "커브드 듀얼")
};

/**
 * 컴퓨터 세팅 레벨
 * 업무공간의 장비 구성 단계
 *
 * Lv1부터 Lv6까지 한 단계씩 진행하는 선형 업그레이드
 */
UENUM(BlueprintType)
enum class EComputerSetupLevel : uint8
{
	Level1	UMETA(DisplayName = "Lv1"),
	Level2	UMETA(DisplayName = "Lv2"),
	Level3	UMETA(DisplayName = "Lv3"),
	Level4	UMETA(DisplayName = "Lv4"),
	Level5	UMETA(DisplayName = "Lv5"),
	Level6	UMETA(DisplayName = "Lv6"),
	// 승급 경로 밖 — Lv6 의 다른 외형이다. TryGetNextComputerSetupLevel 은 Level6 에서 멈춘다.
	Level6Twin	UMETA(DisplayName = "Lv6 (커브드 듀얼)"),
	Max						UMETA(Hidden)
};

/** 성공 시에만 OutNextLevel을 갱신한다. */
FORCEINLINE bool TryGetNextComputerSetupLevel(
	EComputerSetupLevel CurrentLevel,
	EComputerSetupLevel& OutNextLevel)
{
	const uint8 CurrentValue = static_cast<uint8>(CurrentLevel);
	const uint8 LastValue = static_cast<uint8>(EComputerSetupLevel::Level6);
	if (CurrentValue >= LastValue)
	{
		return false;
	}

	OutNextLevel = static_cast<EComputerSetupLevel>(CurrentValue + 1);
	return true;
}

/** 표시용 레벨 번호. Level6Twin 은 Lv6 의 다른 외형이라 같은 6 을 쓴다 — enum 순번을 그대로 쓰면 7 이 된다. */
FORCEINLINE int32 GetComputerSetupLevelNumber(EComputerSetupLevel Level)
{
	return Level == EComputerSetupLevel::Level6Twin
		? static_cast<int32>(EComputerSetupLevel::Level6) + 1
		: static_cast<int32>(Level) + 1;
}

FORCEINLINE bool IsMaxComputerSetupLevel(EComputerSetupLevel Level)
{
	return Level == EComputerSetupLevel::Level6 || Level == EComputerSetupLevel::Level6Twin;
}

/** 최대 레벨의 반대 외형. 최대 레벨이 아니면 false 이고 OutOther 는 건드리지 않는다. */
FORCEINLINE bool TryGetMaxVariantCounterpart(EComputerSetupLevel Level, EComputerSetupLevel& OutOther)
{
	if (Level == EComputerSetupLevel::Level6)
	{
		OutOther = EComputerSetupLevel::Level6Twin;
		return true;
	}
	if (Level == EComputerSetupLevel::Level6Twin)
	{
		OutOther = EComputerSetupLevel::Level6;
		return true;
	}
	return false;
}

// FWorkstationItemSkinData, FComputerSetupLevelData DataTable 구조체들은 Table/WorkstationTable.h로 이동됨

/**
 * 슬롯 스킨 상태
 */
USTRUCT(BlueprintType)
struct FSlotSkinState
{
	GENERATED_BODY()

	// 슬롯이 활성화되어 있는지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	bool bIsActive = false;

	// 현재 적용된 스킨 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FName CurrentSkinID;

	FSlotSkinState()
		: bIsActive(false)
	{}
};

/**
 * 의자 슬롯 상태 (의자는 여러 개일 수 있음)
 */
USTRUCT(BlueprintType)
struct FChairSlotState
{
	GENERATED_BODY()

	// 현재 적용된 스킨 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FName CurrentSkinID;

	// 배정된 직원 EmployeeID (-1이면 미배정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	int32 OccupantEmployeeID = -1;

	FChairSlotState() {}
};

/**
 * 업무공간 저장 데이터
 */
USTRUCT(BlueprintType)
struct FWorkstationSaveData
{
	GENERATED_BODY()

	// 업무공간 타입 (Single/Double)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EWorkstationType WorkstationType = EWorkstationType::Single;

	// 업무공간 타입 ID (책상 종류)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FName WorkstationTypeID;

	// 배치 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FTransform Transform;

	// 현재 컴퓨터 세팅 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EComputerSetupLevel ComputerSetupLevel = EComputerSetupLevel::Level1;

	// 각 슬롯별 스킨 상태 (노트북, 모니터, 키보드마우스 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TMap<EWorkstationSlot, FSlotSkinState> SlotStates;

	// 의자 슬롯별 상태 (여러 개)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<FChairSlotState> ChairStates;
};
