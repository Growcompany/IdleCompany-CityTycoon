#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CriticDisplayTable.generated.h"

// 산업별 비평가 4명 표시명 (리뷰 /40 발표 — specs/2026-06-29 §3.4). CSV 컬럼명 = UPROPERTY명.
USTRUCT(BlueprintType)
struct FCriticDisplayRow : public FTableRowBase
{
	GENERATED_BODY()

	// "Game"/"IT"/"Finance" (StringToCompanyType 파싱)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FString Industry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FText Critic1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FText Critic2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FText Critic3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FText Critic4;

	// 비평가별 관심 직능 — 콤마 구분 EProductionDiscipline 식별자("Plan,Dev" — DisplayName 아님).
	// 빈값 = 전체 평가자(전 직능 가중, 유저평점 류). CSV 컬럼명 = UPROPERTY명.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FString Critic1Focus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FString Critic2Focus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FString Critic3Focus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Critic")
	FString Critic4Focus;
};
