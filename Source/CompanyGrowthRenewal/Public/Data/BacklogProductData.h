#pragma once
#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "BacklogProductData.generated.h"

// 클리어->자동화 모델: 깬 제품 1개 = 자동 순익 엔트리(벽시계 lazy 지수감쇠)
USTRUCT(BlueprintType)
struct FBacklogProductEntry
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") int32 ProjectID = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") int32 ProjectNumber = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") ECompanyType CompanyType = ECompanyType::None;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") FDateTime LaunchWallClock;   // 출시 UTC — 감쇠 기준
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") double BasePeakRevenuePerSec = 0.0; // 출시 peak(멀티 적용 전)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") double OperatingCostPerSec = 0.0;    // 운영비(배치 직원 비례)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") TArray<int32> AssignedEmployeeIDs;   // 묶인 직원(Phase 2 종료·재배치용)
};

// 무캡 오프라인 금고
USTRUCT(BlueprintType)
struct FOfflineVault
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Backlog") double Accumulated = 0.0;
};
