#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CompanyFeatureUnlockTable.generated.h"

/** 회사 건물별 오피스 기능 입장 조건. RowName은 기능 키다. */
USTRUCT(BlueprintType)
struct FCompanyFeatureUnlockTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 RequiredCompanyLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
};
