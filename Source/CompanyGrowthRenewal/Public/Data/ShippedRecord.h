#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "ShippedRecord.generated.h"

// 출시작 이력 1건 — 리뷰 발표 시 기록, 도감 [출시작] 탭의 데이터 (specs/2026-06-29 §15 도감)
USTRUCT(BlueprintType)
struct FShippedProjectRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	FString ProjectName;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	ECompanyType Industry = ECompanyType::None;

	// DT_Project_* 행 인덱스 — 재개발 시 같은 작품을 찾아 갱신하기 위한 키(이름 매칭은 중복에 취약)
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

	// 누적 매출 — 이 출시작의 운영 매출을 결산(GenerateReport) 시점에 누적(근사: ProjectName 매칭)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Shipped")
	int64 CumulativeRevenue = 0;
};
