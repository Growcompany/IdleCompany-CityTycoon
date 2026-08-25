#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "Enum/RawMaterialType.h"
#include "Enum/ResourceType.h"
#include "ProductRecipeTable.generated.h"

/**
 * FProductRecipeTable
 * 제조업 제품별 원자재 레시피
 *
 * RowName = "Recipe001" ~ "Recipe100" (ProjectIndex에 대응)
 * CompanyType별로 별도 DataTable 사용:
 *   DT_Recipe_Electronics, DT_Recipe_Automobile, DT_Recipe_Semiconductor
 *
 * 중간재는 공장 라인이 내부에서 자동 처리하므로,
 * 플레이어에게는 "원자재 → 최종제품" 으로 보임
 */
USTRUCT(BlueprintType)
struct FProductRecipeTable : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 회사 타입 (Electronics, Automobile, Semiconductor)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	ECompanyType CompanyType = ECompanyType::None;

	// 프로젝트 인덱스 (1~100, FProjectData와 연동)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	int32 ProjectIndex = 0;

	// ──────────── 원자재 소모량 (10종) ────────────

	// 철광석 (호주 주력)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 IronOre = 0;

	// 구리 (캐나다 주력)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 Copper = 0;

	// 실리콘 (브라질 주력)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 Silicon = 0;

	// 리튬 (호주)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 Lithium = 0;

	// 석유 (사우디 독점)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 Oil = 0;

	// 희토류 (중국 독점)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 RareEarth = 0;

	// 알루미늄 (호주, 캐나다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 Aluminum = 0;

	// 목재 (캐나다 주력)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 Wood = 0;

	// 금 (남아공 독점)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 GoldOre = 0;

	// 다이아몬드 (남아공 독점)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RawMaterial")
	int32 DiamondOre = 0;

	// ──────────── 생산 파라미터 ────────────

	// 라인 제작 시간 (초, 중간재 가공 포함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	float ProductionTimeSec = 30.0f;

	// 기본 시가총액 (판매 시 획득, 무역항 보너스/주문서 배율 적용 전)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	int32 BaseMarketCap = 10;

	// 에너지 소모량 (사우디 발전소에서 공급)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	int32 EnergyCost = 5;

	// ──────────── 헬퍼 함수 ────────────

	// ERawMaterialType으로 해당 원자재 소모량 조회
	int32 GetMaterialAmount(ERawMaterialType Type) const
	{
		switch (Type)
		{
		case ERawMaterialType::IronOre:    return IronOre;
		case ERawMaterialType::Copper:     return Copper;
		case ERawMaterialType::Silicon:    return Silicon;
		case ERawMaterialType::Lithium:    return Lithium;
		case ERawMaterialType::Oil:        return Oil;
		case ERawMaterialType::RareEarth:  return RareEarth;
		case ERawMaterialType::Aluminum:   return Aluminum;
		case ERawMaterialType::Wood:       return Wood;
		case ERawMaterialType::Gold:       return GoldOre;
		case ERawMaterialType::DiamondOre: return DiamondOre;
		default: return 0;
		}
	}

	// 소모량 > 0 인 재료만 (EResourceType, 개당소요) 로 평탄화.
	// 표시·상한계산·차감이 전부 같은 목록을 봐야 하므로 여기가 단일 출처 —
	// 호출부에서 10종을 손으로 나열하면 원자재가 늘 때 조용히 어긋난다.
	void CollectMaterials(TArray<TPair<EResourceType, int32>>& Out) const
	{
		Out.Reset();
		auto Push = [&Out](EResourceType Type, int32 Amount)
		{
			if (Amount > 0) Out.Emplace(Type, Amount);
		};
		Push(EResourceType::IronOre,    IronOre);
		Push(EResourceType::Copper,     Copper);
		Push(EResourceType::Silicon,    Silicon);
		Push(EResourceType::Lithium,    Lithium);
		Push(EResourceType::Oil,        Oil);
		Push(EResourceType::RareEarth,  RareEarth);
		Push(EResourceType::Aluminum,   Aluminum);
		Push(EResourceType::Wood,       Wood);
		Push(EResourceType::Gold,       GoldOre);
		Push(EResourceType::DiamondOre, DiamondOre);
	}

	// 위 목록에 등장할 수 있는 자원 전체 (레시피 무관). 자원 변화 필터 등에 사용.
	static const TArray<EResourceType>& GetAllRawMaterialTypes()
	{
		static const TArray<EResourceType> Types = {
			EResourceType::IronOre, EResourceType::Copper, EResourceType::Silicon,
			EResourceType::Lithium, EResourceType::Oil,    EResourceType::RareEarth,
			EResourceType::Aluminum, EResourceType::Wood,  EResourceType::Gold,
			EResourceType::DiamondOre
		};
		return Types;
	}

	// 총 원자재 소모량 합계
	int32 GetTotalMaterialAmount() const
	{
		return IronOre + Copper + Silicon + Lithium + Oil
			+ RareEarth + Aluminum + Wood + GoldOre + DiamondOre;
	}

	// 사용하는 원자재 종류 수
	int32 GetMaterialTypeCount() const
	{
		int32 Count = 0;
		if (IronOre > 0)    ++Count;
		if (Copper > 0)     ++Count;
		if (Silicon > 0)    ++Count;
		if (Lithium > 0)    ++Count;
		if (Oil > 0)        ++Count;
		if (RareEarth > 0)  ++Count;
		if (Aluminum > 0)   ++Count;
		if (Wood > 0)       ++Count;
		if (GoldOre > 0)    ++Count;
		if (DiamondOre > 0) ++Count;
		return Count;
	}

	FProductRecipeTable() = default;
};
