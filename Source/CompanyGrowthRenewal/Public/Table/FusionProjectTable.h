#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "FusionProjectTable.generated.h"

/** 콜라보 1종. 레벨은 세이브에, 요구치와 보상 곡선은 이 표에 둔다. */
USTRUCT(BlueprintType)
struct FFusionProjectTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<ECompanyType> RequiredIndustries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseRequirement = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RequirementMultiplier = 1.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 DurationSeconds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 BaseMarketCap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double MarketCapMultiplier = 1.16;

	/** 확정 보상만 사용하므로 비워 둔다. 임의의 랜덤 드랍으로 해석하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName LootTableKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredHQLevel = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 FirstSuccessDiamonds = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MilestoneEvery = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MilestoneTicketAmount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 DisplayOrder = 0;
};
