#pragma once

#include "CoreMinimal.h"
#include "TierLayout.generated.h"

// 티어 1개의 프로젝트 범위. 기본 = (T-1)*10+1 부터 10개, 해금 7, 수익 앵커 = StartIndex.
USTRUCT(BlueprintType)
struct FTierRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	int32 StartIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	int32 Count = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	int32 ClearToUnlock = 7;

	// 수익 기준 인덱스 — 이 티어 첫 프로젝트의 "규모 번호". 행을 끼워 넣어도 기존 티어 수익이 안 밀리게 분리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	int32 RevenueAnchor = 1;
};

/**
 * 티어 레이아웃 SOT. 정적 상수(10×10)였던 범위 산술을 데이터로 덮어쓸 수 있게 한 훅.
 * 런타임 = DT_TierLayout 을 읽어 SetLayout. 테이블 없음 = 기본 레이아웃(현행과 동일).
 */
struct COMPANYGROWTHRENEWAL_API FTierLayout
{
	static void SetLayout(const TArray<FTierRange>& InRanges);
	static void ResetToDefault();

	static int32 MaxTier();
	static int32 ProjectsPerTier(int32 Tier);
	static int32 ClearToUnlock(int32 Tier);
	static void GetRange(int32 Tier, int32& OutStart, int32& OutEnd);
	// 범위 밖 번호는 0 — 조용히 Clamp 하지 않는다(101행이 T10 으로 삼켜지던 함정 재발 방지)
	static int32 GetTierForProject(int32 ProjectIndex);
	// 수익 산식이 쓰는 규모 번호 = RevenueAnchor(T) + (N - StartIndex(T)). 기본 레이아웃에서는 N 그대로
	static int32 RevenueScaleIndex(int32 ProjectIndex);

private:
	static TArray<FTierRange>& Ranges();
	static const FTierRange* Find(int32 Tier);
	static void FillDefault(TArray<FTierRange>& OutRanges);
};
