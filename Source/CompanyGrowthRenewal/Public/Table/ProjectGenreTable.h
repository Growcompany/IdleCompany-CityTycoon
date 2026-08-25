#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "ProjectGenreTable.generated.h"

// GDS 발견형 착수 — 산업별 장르 정의(개발 복잡도 + 해금 티어).
// CSV: Name(=RowName),Industry,Genre,Complexity,UnlockTier. 컬럼명 = UPROPERTY명 일치 필수.
USTRUCT(BlueprintType)
struct FProjectGenreRow : public FTableRowBase
{
	GENERATED_BODY()

	// 산업 식별자("Game"/"IT"/"Finance"). enum import 깨짐 방지를 위해 FString — 로드 시 ECompanyType 변환.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Genre")
	FString Industry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Genre")
	FName Genre = NAME_None;

	// 개발 복잡도(1~6). 후반 장르일수록 높음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Genre")
	int32 Complexity = 1;

	// 이 장르가 노출되는 최소 티어.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Genre")
	int32 UnlockTier = 1;

	// 피치 카드 "?" 웰 장르색 틴트. CSV 미지정 시 Gray → 코드 폴백(데이터주도).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Genre")
	FLinearColor Color = FLinearColor::Gray;
};
