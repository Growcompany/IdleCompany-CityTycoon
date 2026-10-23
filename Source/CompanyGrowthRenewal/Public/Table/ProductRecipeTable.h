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
 * 중간재 숨김 결정은 2026-10-03 폐기 — 전자 업종 프리미엄 구간은 선행 제품(칩)을
 * 보이는 입력으로 먹는다. 기초 전자·칩 구간은 여전히 원자재만 먹는다.
 * 근거: docs/superpowers/specs/2026-10-03-industry-roster-4-design.md §2
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

	// 제품 1개분의 생산 가능 수량을 확보하는 제조업 3분야 비용. 슬롯 순서는 EProductionDiscipline의 앞 3칸.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development")
	TArray<double> DevelopmentPointCosts = { 0.0, 0.0, 0.0 };

	// 최초 개발에 필요한 현재 착석 직원들의 분야별 능력 합계. 슬롯 순서는 제조업 3분야.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development")
	TArray<int32> RequiredTeamCapability = { 0, 0, 0 };

	// 최초 개발에 필요한 실제 직원 평균 근무 시간(초).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development")
	float ResearchWorkSeconds = 0.0f;

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

	// 제품 1개 기본 판매가. 판매처·수요·주문 배율 적용 전 금액.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	int64 BaseSalePrice = 0;

	// 에너지 소모량 (사우디 발전소에서 공급)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	int32 EnergyCost = 5;

	// ──────────── 제품 입력 (수직계열화) ────────────

	// 같은 업종 안에서 먹는 선행 제품의 ProjectIndex. 0 = 제품 입력 없음.
	// 업종 간 의존은 두지 않는다 — 한쪽이 미해금이면 생산이 막힌다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProductInput")
	int32 InputProjectIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProductInput")
	int32 InputProductQty = 0;

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

	// 제품 입력 조회. 반쪽 설정(한쪽만 채움)과 자기·전방 참조는 "입력 없음" 으로 떨어뜨린다 —
	// CSV 오타 하나가 생산을 영구 불가로 만드는 쪽보다 안전하다.
	// 입력 인덱스가 엄격히 작아야 통과하므로 사이클이 성립할 수 없다 — 따로 검사하지 않는다.
	bool GetProductInput(int32& OutProjectIndex, int32& OutQty) const
	{
		OutProjectIndex = 0;
		OutQty = 0;
		if (InputProjectIndex <= 0 || InputProductQty <= 0) return false;
		if (InputProjectIndex >= ProjectIndex) return false;
		OutProjectIndex = InputProjectIndex;
		OutQty = InputProductQty;
		return true;
	}

	FProductRecipeTable() = default;
};
