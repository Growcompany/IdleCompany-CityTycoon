#pragma once

#include "CoreMinimal.h"
#include "Enum/QualityGrade.h"
#include "Enum/CompanyType.h"
#include "ProjectReportData.generated.h"

/**
 * 프로젝트 결산서 데이터
 * 운영 완료 후 결산서 표시에 사용
 */
USTRUCT(BlueprintType)
struct FProjectReportData
{
	GENERATED_BODY()

	// 프로젝트 ID (DataTable 키)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	int32 ProjectID = 0;

	// 프로젝트 번호
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	int32 ProjectNumber = 1;

	// 스테이지 번호
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	int32 StageNumber = 1;

	// 프로젝트 이름
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	FString ProjectName;

	// 회사 타입 (산업별 DataTable 복합키 조회용)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	ECompanyType CompanyType = ECompanyType::None;

	// 소속 건물 ID
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	int32 BuildingID = 0;

	// 품질 점수 (0.5 ~ 2.0)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	float QualityScore = 1.0f;

	// 품질 등급
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	EQualityGrade QualityGrade = EQualityGrade::C;

	// 직능별 결산 표시 — 활성 직능 순서(출시 제외). 병렬 배열, [i]=i번째 활성 직능.
	// 표시명은 담지 않는다 — 라벨 SOT 는 DT_DisciplineDisplay(GetDisciplineDisplayName) 하나뿐이다.
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	TArray<float> DisciplineScores;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	TArray<float> DisciplineTargets;

	// 압축된 위 배열을 고정 축(6칸)에 되흩뿌리기 위한 원래 슬롯 번호 — 없으면 성긴 직능 구성에서 값과 라벨이 어긋난다
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	TArray<int32> DisciplineSlots;

	// 총 수익
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	float TotalRevenueEarned = 0.0f;

	// 총 운영 시간
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	float TotalOperationTime = 0.0f;

	// 획득한 시가총액 (프로젝트 완료 보상, QualityScore × MarketCapMultiplier 배율 적용)
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Report")
	int64 MarketCapGained = 0;

	FProjectReportData()
		: ProjectID(0)
		, ProjectNumber(1)
		, StageNumber(1)
		, ProjectName(TEXT(""))
		, CompanyType(ECompanyType::None)
		, BuildingID(0)
		, QualityScore(1.0f)
		, QualityGrade(EQualityGrade::C)
		, TotalRevenueEarned(0.0f)
		, TotalOperationTime(0.0f)
		, MarketCapGained(0)
	{
	}
};
