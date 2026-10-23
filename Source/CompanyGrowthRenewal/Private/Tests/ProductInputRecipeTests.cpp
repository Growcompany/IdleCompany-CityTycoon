#include "Misc/AutomationTest.h"
#include "Table/ProductRecipeTable.h"
#include "Manager/ProductionOrderManager.h"
#include "Enum/ResourceType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductInputAccessorValidationTest,
	"CGR.Recipe.ProductInput.AccessorValidation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FProductInputAccessorValidationTest::RunTest(const FString& Parameters)
{
	// 구조체 접근자만 본다 ― DT 로드·생산 상한 반영은 검증하지 않는다.
	int32 Index = -1;
	int32 Qty = -1;

	// 기본 = 제품 입력 없음 (기초 전자·칩 구간은 원자재만 먹는다)
	{
		FProductRecipeTable R;
		R.ProjectIndex = 10;
		TestFalse(TEXT("기본은 제품 입력 없음"), R.GetProductInput(Index, Qty));
		TestEqual(TEXT("없으면 인덱스 0"), Index, 0);
		TestEqual(TEXT("없으면 수량 0"), Qty, 0);
	}

	// 정상 — 프리미엄 제품이 칩을 먹는다
	{
		FProductRecipeTable R;
		R.ProjectIndex = 80;
		R.InputProjectIndex = 55;
		R.InputProductQty = 3;
		TestTrue(TEXT("정상 설정은 입력 있음"), R.GetProductInput(Index, Qty));
		TestEqual(TEXT("입력 인덱스"), Index, 55);
		TestEqual(TEXT("입력 수량"), Qty, 3);
	}

	// 자기참조 — 데이터 오타 하나로 생산이 영구 불가가 된다
	{
		FProductRecipeTable R;
		R.ProjectIndex = 80;
		R.InputProjectIndex = 80;
		R.InputProductQty = 2;
		TestFalse(TEXT("자기를 입력으로 쓰는 레시피는 거부"), R.GetProductInput(Index, Qty));
		TestEqual(TEXT("거부 시 인덱스 0"), Index, 0);
	}

	// 전방 참조 — 사다리는 항상 더 낮은 인덱스를 먹는다. 막지 않으면 2-사이클(071→072, 072→071)도 통과한다.
	{
		FProductRecipeTable R;
		R.ProjectIndex = 71;
		R.InputProjectIndex = 72;
		R.InputProductQty = 2;
		TestFalse(TEXT("더 높은 인덱스를 입력으로 쓰는 레시피는 거부"), R.GetProductInput(Index, Qty));
	}

	// 반쪽 설정 — 인덱스만
	{
		FProductRecipeTable R;
		R.ProjectIndex = 80;
		R.InputProjectIndex = 55;
		R.InputProductQty = 0;
		TestFalse(TEXT("수량 0 은 입력 없음"), R.GetProductInput(Index, Qty));
	}

	// 반쪽 설정 — 수량만
	{
		FProductRecipeTable R;
		R.ProjectIndex = 80;
		R.InputProjectIndex = 0;
		R.InputProductQty = 3;
		TestFalse(TEXT("인덱스 0 은 입력 없음"), R.GetProductInput(Index, Qty));
	}

	// 음수 수량
	{
		FProductRecipeTable R;
		R.ProjectIndex = 80;
		R.InputProjectIndex = 55;
		R.InputProductQty = -1;
		TestFalse(TEXT("음수 수량은 입력 없음"), R.GetProductInput(Index, Qty));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProducibleCapArbitrationTest,
	"CGR.Recipe.ProductInput.CapArbitration",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FProducibleCapArbitrationTest::RunTest(const FString& Parameters)
{
	// 추출한 중재 산술만 본다 ― GetMaxProducible·재고 조회·DT lookup 은 검증하지 않는다.

	// 자원이 병목 — 자원 이름을 그대로 내보낸다
	{
		EResourceType Bottleneck = EResourceType::Silicon;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/5, /*Product=*/99, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("자원 병목 상한"), Cap, 5);
		TestEqual(TEXT("자원 이름 유지"), Bottleneck, EResourceType::Silicon);
		TestTrue(TEXT("재료 바운드"), bSupplyBound);
	}

	// 제품이 병목 — 자원 이름을 비운다. 안 비우면 "실리콘을 캐라"는 틀린 안내가 나간다
	{
		EResourceType Bottleneck = EResourceType::Silicon;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/50, /*Product=*/3, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("제품 병목 상한"), Cap, 3);
		TestEqual(TEXT("제품 병목이면 자원 이름을 비운다"), Bottleneck, EResourceType::None);
		TestTrue(TEXT("상한이 주문보다 작으면 바운드"), bSupplyBound);
	}

	// 주문 잔여가 병목 — 둘 다 넉넉하면 병목 없음
	{
		EResourceType Bottleneck = EResourceType::Silicon;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/99, /*Product=*/99, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("주문 잔여 상한"), Cap, 20);
		TestEqual(TEXT("병목 없으면 None"), Bottleneck, EResourceType::None);
		TestFalse(TEXT("주문 바운드는 재료 바운드가 아니다"), bSupplyBound);
	}

	// 동률 — 제품과 자원이 같은 상한이면 자원 이름을 남긴다(기존 동작 보존)
	{
		EResourceType Bottleneck = EResourceType::Copper;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/4, /*Product=*/4, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("동률 상한"), Cap, 4);
		TestEqual(TEXT("동률이면 자원 이름 유지"), Bottleneck, EResourceType::Copper);
	}

	// 제품 재고 0 — 생산 불가
	{
		EResourceType Bottleneck = EResourceType::Silicon;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/50, /*Product=*/0, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("칩 재고 0 이면 0개"), Cap, 0);
		TestEqual(TEXT("자원 이름 비움"), Bottleneck, EResourceType::None);
	}

	// 제품 입력 없는 레시피(ProductCap = MAX_int32) — Task 10 전까지 모든 레시피가 이 경로라
	// 이 변경의 회귀 표면 전부다. 중재가 재료 상한을 그대로 통과시켜야 한다
	{
		EResourceType Bottleneck = EResourceType::Copper;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/7, /*Product=*/MAX_int32, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("제품 입력 없으면 재료 상한 그대로"), Cap, 7);
		TestEqual(TEXT("제품 입력 없으면 자원 이름 유지"), Bottleneck, EResourceType::Copper);
		TestTrue(TEXT("제품 입력 없어도 재료가 막으면 공급 바운드"), bSupplyBound);
	}

	// 제품 입력 없음 + 주문 잔여가 병목 — 구 코드의 FMath::Min(MaterialCap, OrderCap) 과 같아야 한다
	{
		EResourceType Bottleneck = EResourceType::Copper;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/99, /*Product=*/MAX_int32, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("제품 입력 없고 주문이 막으면 주문 잔여"), Cap, 20);
		TestEqual(TEXT("제품 입력 없이 주문이 막으면 자원 이름 비움"), Bottleneck, EResourceType::None);
		TestFalse(TEXT("제품 입력 없을 때 주문 바운드는 공급 바운드가 아니다"), bSupplyBound);
	}

	// 경계 — 재료 상한이 주문 잔여와 "같을" 때. 공급이 먼저 막은 게 아니므로 자원 이름을 비운다.
	// (Cap < OrderCap) 이 (Cap <= OrderCap) 로 뒤집히면 이 케이스가 첫 희생자다
	{
		EResourceType Bottleneck = EResourceType::Silicon;
		bool bSupplyBound = false;
		const int32 Cap = ArbitrateProducibleCap(/*Material=*/20, /*Product=*/99, /*Order=*/20, Bottleneck, bSupplyBound);
		TestEqual(TEXT("재료 상한 == 주문 잔여면 상한 20"), Cap, 20);
		TestEqual(TEXT("같을 때는 공급 바운드가 아니므로 자원 이름을 비운다"), Bottleneck, EResourceType::None);
		TestFalse(TEXT("같을 때는 공급 바운드 아님"), bSupplyBound);
	}

	return true;
}
#endif
