#pragma once

#include "CoreMinimal.h"
#include "ShopTypes.generated.h"

// 상점 탭 분류 (DT_ShopItem.Tab 키 — Event는 v2 예약 슬롯)
UENUM(BlueprintType)
enum class EShopTab : uint8
{
	None = 0,
	Daily       UMETA(DisplayName = "일일 상점"),
	Weekly      UMETA(DisplayName = "주간 상점"),
	Diamond     UMETA(DisplayName = "다이아 상점"),
	Mileage     UMETA(DisplayName = "마일리지 상점"),
	Event       UMETA(DisplayName = "이벤트 상점"),
	Count UMETA(Hidden)
};

// 상점 결제 재화
UENUM(BlueprintType)
enum class EShopCurrency : uint8
{
	Money       UMETA(DisplayName = "머니"),
	Diamond     UMETA(DisplayName = "다이아"),
	Mileage     UMETA(DisplayName = "마일리지"),
};
