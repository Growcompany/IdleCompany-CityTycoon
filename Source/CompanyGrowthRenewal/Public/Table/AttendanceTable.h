#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Table/MissionTable.h"
#include "AttendanceTable.generated.h"

// DT_Attendance 행 (RowName = Day01…Day28). 28일 순환 보상표
USTRUCT(BlueprintType)
struct FAttendanceTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attendance")
	int32 Day = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attendance")
	TArray<FMissionReward> Rewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attendance")
	bool bMilestone = false;
};
