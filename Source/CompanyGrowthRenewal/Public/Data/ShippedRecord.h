#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "ShippedRecord.generated.h"

// 건물별 출시작 이력 1건 — 리뷰 발표 시 기록, 도감 [출시작] 탭의 데이터 (specs/2026-06-29 §15 도감)
USTRUCT(BlueprintType)
struct FShippedProjectRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	FString ProjectName;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	ECompanyType Industry = ECompanyType::None;

	// 같은 산업의 다른 건물도 같은 프로젝트를 출시할 수 있다.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	int32 BuildingIndex = INDEX_NONE;

	// DT_Project_* 행 인덱스 — 같은 건물에서 재개발 시 작품을 찾아 갱신한다.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	int32 ProjectIndex = 0;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	FName Genre = NAME_None;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	FName Material = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Shipped")
	EQualityGrade QualityGrade = EQualityGrade::C;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	int32 ReviewScore = 0;

	// 누적 매출 — 이 건물의 해당 출시작 운영 매출을 결산(GenerateReport) 시점에 누적한다.
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	int64 CumulativeRevenue = 0;
};

namespace ShippedProjectRecords
{
	inline const FShippedProjectRecord* Find(const TArray<FShippedProjectRecord>& Records,
		int32 BuildingIndex, ECompanyType Industry, int32 ProjectIndex)
	{
		return Records.FindByPredicate([BuildingIndex, Industry, ProjectIndex](const FShippedProjectRecord& Record)
		{
			return Record.BuildingIndex == BuildingIndex && Record.Industry == Industry
				&& Record.ProjectIndex == ProjectIndex;
		});
	}

	inline FShippedProjectRecord* Find(TArray<FShippedProjectRecord>& Records,
		int32 BuildingIndex, ECompanyType Industry, int32 ProjectIndex)
	{
		return Records.FindByPredicate([BuildingIndex, Industry, ProjectIndex](const FShippedProjectRecord& Record)
		{
			return Record.BuildingIndex == BuildingIndex && Record.Industry == Industry
				&& Record.ProjectIndex == ProjectIndex;
		});
	}
}
