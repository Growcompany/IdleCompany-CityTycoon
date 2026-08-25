#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyTitle.h"
#include "Enum/CompanyType.h"
#include "RankingData.generated.h"

/**
 * 리더보드 항목 (PlayFab 조회 결과 → UI 표시용)
 */
USTRUCT(BlueprintType)
struct FRankingEntry
{
	GENERATED_BODY()

	// PlayFab 유저 ID (도시 방문 시 키로 사용)
	UPROPERTY(BlueprintReadOnly)
	FString PlayFabId;

	// 표시 이름
	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	// 순위 (1부터 시작)
	UPROPERTY(BlueprintReadOnly)
	int32 Rank = 0;

	// 누적 매출 (정렬 기준)
	UPROPERTY(BlueprintReadOnly)
	int64 TotalRevenue = 0;

	// 프로필 정보 (RankingProfile JSON에서 파싱)
	UPROPERTY(BlueprintReadOnly)
	int32 HQLevel = 1;

	UPROPERTY(BlueprintReadOnly)
	ECompanyTitle CompanyTitle = ECompanyTitle::Small;

	UPROPERTY(BlueprintReadOnly)
	int32 BuildingCount = 0;

	// 보유 빌딩 중 최고 프로젝트 티어 (JSON 키 "mt" — 구 "mbl"(빌딩 레벨 1~30)과 스케일이 달라 키를 재사용하지 않는다)
	UPROPERTY(BlueprintReadOnly)
	int32 MaxTier = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 EmployeeCount = 0;

	// 프로필 이미지 ID (DT_ProfileImage의 ImageID, 기본 0 = 빌딩1)
	UPROPERTY(BlueprintReadOnly)
	int32 ProfileImageID = 0;
};

/**
 * 도시 스냅샷 - 개별 빌딩 데이터 (방문 시 건물 스폰용)
 */
USTRUCT(BlueprintType)
struct FCitySnapshotBuilding
{
	GENERATED_BODY()

	// DataTable 행 이름 (건물 메쉬 결정)
	UPROPERTY()
	FName InteractableName;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector Scale = FVector::OneVector;

	// 층수 (외형 높이 결정)
	UPROPERTY()
	int32 Body_Module_Copies = 0;

	// 적용된 스킨 ID
	UPROPERTY()
	int32 AppliedSkinID = 100;

	// 업종
	UPROPERTY()
	ECompanyType CompanyType = ECompanyType::None;
};

/**
 * 도시 스냅샷 전체 (PlayFab에 JSON으로 업로드)
 */
USTRUCT(BlueprintType)
struct FCitySnapshot
{
	GENERATED_BODY()

	// 스키마 버전 (하위 호환용)
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	int32 HQLevel = 1;

	UPROPERTY()
	ECompanyTitle CompanyTitle = ECompanyTitle::Small;

	UPROPERTY()
	TArray<FCitySnapshotBuilding> Buildings;

	// 표시용 통계
	UPROPERTY()
	int32 TotalEmployees = 0;

	UPROPERTY()
	int64 TotalRevenueEarned = 0;
};
