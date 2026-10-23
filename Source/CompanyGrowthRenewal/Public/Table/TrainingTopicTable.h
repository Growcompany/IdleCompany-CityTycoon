#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "TrainingTopicTable.generated.h"

/** 업종별 기술 주제. RowName은 재정렬해도 유지하는 기술 식별자다. */
USTRUCT(BlueprintType)
struct FTrainingTopicRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECompanyType CompanyType = ECompanyType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Sequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
};
