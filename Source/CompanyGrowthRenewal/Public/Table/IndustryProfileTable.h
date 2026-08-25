#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "IndustryProfileTable.generated.h"

// GDS 코어루프 — 산업별 수익 곡선/리뷰 프로파일(착수 모달 시각화 pip + 실제 밸런스 계수).
// CSV 컬럼명 = UPROPERTY명 일치 필수.
USTRUCT(BlueprintType)
struct FIndustryProfileRow : public FTableRowBase
{
	GENERATED_BODY()

	// 산업 식별자("Game"/"IT"/"Finance"). enum import 깨짐 방지를 위해 FString — 로드 시 ECompanyType 변환.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile")
	FString Industry;

	// ── 착수 모달 막대(pip) 표시용 ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Pips")
	int32 PeakPips = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Pips")
	int32 TailPips = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Pips")
	int32 VolatilityPips = 0;

	// ── 실제 밸런스 계수 ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Balance")
	float PeakMult = 1.f;

	// 판매 수익 반감기(운영시간 대비 비율).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Balance")
	float HalfLifeFrac = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Balance")
	float Volatility = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Balance")
	float ReviewLeverage = 1.f;

	// 수주(클라 발주) 매력도.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IndustryProfile|Balance")
	float CommissionAppeal = 1.f;
};
