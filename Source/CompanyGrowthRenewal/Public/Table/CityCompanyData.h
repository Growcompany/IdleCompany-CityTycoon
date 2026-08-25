#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h" // FCulture 완전 정의 (GetName())
#include "Engine/Texture2D.h"
#include "Enum/CompanyType.h"
#include "CityCompanyData.generated.h"

// 스카이라인 빌딩(BP_MB###) 1개 = 가상 회사 1개. (도시 인수 / City Acquisition)
USTRUCT(BlueprintType)
struct FCityCompanyData : public FTableRowBase
{
	GENERATED_BODY()

	// BP_MB### 에서 파싱한 키 (1~40). 마커/사인이 이 키로 회사를 찾는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company")
	int32 BuildingKey = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Name")
	FString NameKR;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Name")
	FString NameEN;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company")
	ECompanyType Industry = ECompanyType::None;

	// 마커/사인 글로우 강조색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company")
	FLinearColor BrandColor = FLinearColor(0.47f, 0.88f, 1.0f);

	// ↓ sub-project ②(인수 경제)용. 지금(UI 마커)에선 미사용이나 DT에 채워둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Economy")
	int64 MarketCapValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Economy")
	int64 AcquisitionCost = 0;

	// ↓ 스카이라인 드레싱(2026-06-26). 산업 내 티어 1..10, 목표 층수, 스킨/조명 ID.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Dressing")
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Dressing")
	int32 Stories = 6;

	// DT_BuildingSkin 행 ID (머티리얼)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Dressing")
	int32 SkinID = 100;

	// DT_BuildingLight 행 ID (발광색)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Dressing")
	int32 LightID = 100;

	// 도박 인수 경제 (2026-06-26)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq")
	int32 YieldMinPct = 50;   // 총수익 하한(인수가 %)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq")
	int32 YieldMaxPct = 300;  // 총수익 상한(인수가 %)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DripChance = 0.5f;   // 매 초 드립 발생 확률

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq")
	float DripChunkPct = 5.0f; // 드립 1회 지급액(인수가 %)

	// 크리트 틱 — 성공 드립 중 CritChance 확률로 지급액 ×CritMult (D8 금고 도박)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CritChance = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq")
	float CritMult = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq")
	FString Description;       // 모달 설명(가상 회사 소개)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Company|Acq")
	TSoftObjectPtr<UTexture2D> CompanyIcon; // 모달 아이콘/이미지

	// 표시용 기댓값(%)
	int32 GetEvPct() const { return (YieldMinPct + YieldMaxPct) / 2; }

	// 예상 회수 소요(초). 기대 초당 = 인수가 × DripChunkPct/100 × DripChance × (1 + CritChance×(CritMult−1)).
	// 인수가가 약분되어 EV% 와 드립 파라미터만으로 정해진다 — 굴림값이 아니라 기댓값 기반이라
	// "남은 시간에서 총 회수액을 역산" 하는 누설과 무관하다.
	float GetExpectedSeconds() const
	{
		const double PerSecPct = double(DripChunkPct) * double(DripChance) * (1.0 + double(CritChance) * (double(CritMult) - 1.0));
		return PerSecPct > 0.0 ? float(double(GetEvPct()) / PerSecPct) : 0.f;
	}

	// 회수 금액(원) — 표시는 전부 %가 아니라 돈으로 한다
	int64 GetExpectedReturn() const { return int64(double(AcquisitionCost) * double(GetEvPct()) / 100.0); }
	int64 GetMinReturn() const { return int64(double(AcquisitionCost) * double(YieldMinPct) / 100.0); }
	int64 GetMaxReturn() const { return int64(double(AcquisitionCost) * double(YieldMaxPct) / 100.0); }

	// 본전 이상 회수 확률(%) = P(회수율 >= 100). 균등분포 [Min,Max] 가정.
	int32 GetBreakEvenPct() const
	{
		if (YieldMaxPct <= 100) return 0;
		if (YieldMinPct >= 100) return 100;
		if (YieldMaxPct == YieldMinPct) return 0;
		return FMath::Clamp(FMath::RoundToInt((YieldMaxPct - 100) * 100.f / float(YieldMaxPct - YieldMinPct)), 0, 100);
	}

	// 현재 컬처로 표시명 선택 (마커용). 한국어→KR, 그 외→EN. 빈 값이면 EN 폴백.
	FString GetDisplayName() const
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		const FString& Picked = Culture.Contains(TEXT("ko")) ? NameKR : NameEN;
		return Picked.IsEmpty() ? NameEN : Picked;
	}
};
