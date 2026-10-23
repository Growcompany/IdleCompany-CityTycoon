#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FunnyGarmentStyleTable.generated.h"

UENUM(BlueprintType)
enum class EFunnyGarmentMode : uint8
{
	Flat	UMETA(DisplayName = "단색"),
	Polo	UMETA(DisplayName = "폴로 카라"),
	Raglan	UMETA(DisplayName = "라글란 소매"),
	Band	UMETA(DisplayName = "가로 밴드"),

	Max		UMETA(Hidden)
};

/**
 * 의상 존 — 옷 한 벌을 규칙으로 나눠 칠한다. 새 UV/텍스처/메시 없이 PreSkinnedPosition 만 쓴다.
 * RowName = 메시 이름(예 SK_Outerwear_White_004). 메시마다 z 범위·어깨 폭이 달라 공용 수식 하나로는 안 된다.
 */
USTRUCT(BlueprintType)
struct FFunnyGarmentStyleTable : public FTableRowBase
{
	GENERATED_BODY()

	// 메시 실측 경계 — 셰이더가 높이를 0~1 로 정규화하는 데 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoneMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoneMax = 1.f;

	// 중심에서 가장 먼 정점까지(cm). 커프스는 이 값이 45 를 넘는 긴소매에서만 켜진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float XMax = 0.f;

	// 비트마스크: bit0=단색 bit1=폴로 bit2=라글란 bit3=밴드. 민소매에 라글란을 주면 소매가 없어 아무것도 안 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AllowedModes = 1;

	// 높이 0~1 기준. 이 위가 카라 밴드이고, 앞쪽 노치만 비운다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CollarTop = 0.905f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CollarNotch = 4.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PlacketWidth = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SleeveSplit = 11.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PipingWidth = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BandCount = 7.f;

	// 0 이면 커프스를 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CuffStart = 6.f;
};
