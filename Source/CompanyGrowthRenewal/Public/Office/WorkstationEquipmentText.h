#pragma once

#include "CoreMinimal.h"
#include "Table/WorkstationTable.h"
#include "WorkstationEquipmentText.generated.h"

/** 책상 위 장비 품목. 표시명은 GetWorkstationEquipItemName 경유 — UMETA 은 에디터 전용이라 패키징에서 영어로 떨어진다. */
UENUM(BlueprintType)
enum class EWorkstationEquipItem : uint8
{
	None,
	Laptop,
	MonitorFlat,
	MonitorCurved,
	MonitorVertical,
	Tower,
	Keyboard,
	Mouse
};

/** 다음 단계에서 일어나는 변화의 종류. 추가와 교체는 플레이어에게 다른 사건이다. */
UENUM(BlueprintType)
enum class EWorkstationEquipDeltaKind : uint8
{
	None,
	Added,
	Replaced
};

USTRUCT(BlueprintType)
struct FWorkstationEquipDelta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Equipment")
	EWorkstationEquipDeltaKind Kind = EWorkstationEquipDeltaKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Equipment")
	EWorkstationEquipItem Item = EWorkstationEquipItem::None;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Equipment")
	FText ItemName;

	UPROPERTY(BlueprintReadOnly, Category = "Workstation|Equipment")
	FText Detail;
};

COMPANYGROWTHRENEWAL_API FText GetWorkstationEquipItemName(EWorkstationEquipItem Item);

/** 행이 켜 둔 품목 목록. 순서 = 노트북 → 모니터 → 세로 모니터 → 본체 → 키보드 → 마우스 (책상 위 배치 순) */
COMPANYGROWTHRENEWAL_API TArray<EWorkstationEquipItem> GetWorkstationEquipItems(const FComputerSetupLevelData& Row);

/** "노트북, 평면 모니터, 본체, 키보드, 마우스" */
COMPANYGROWTHRENEWAL_API FText BuildWorkstationEquipSummary(const FComputerSetupLevelData& Row);

/** 현재 행 → 다음 행 비교. NextRow 가 nullptr 이면 최대 레벨(Kind=None). */
COMPANYGROWTHRENEWAL_API FWorkstationEquipDelta BuildWorkstationEquipDelta(
	const FComputerSetupLevelData& CurrentRow, const FComputerSetupLevelData* NextRow);
