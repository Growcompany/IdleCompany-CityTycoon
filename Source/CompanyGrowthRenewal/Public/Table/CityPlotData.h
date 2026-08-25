#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CityPlotData.generated.h"

// 도시 블록(부지) 1개 = DT 행 1개. RowName = PlotId.
USTRUCT(BlueprintType)
struct FCityPlotData : public FTableRowBase
{
    GENERATED_BODY()

    // 블록 중심 월드 좌표
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Center = FVector::ZeroVector;

    // 부지 격자 칸 수 (가로/세로). 블록 142m ÷ 28.4m = 5×5. 배치 가용 영역 = GridCols×GridRows 셀.
    // (반경/Extent 는 격자에서 산출 — ACityPlotActor::Init / GetOwnedPlotAt 가 GridCols*FootprintCellSize/2 로 derive)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GridCols = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GridRows = 5;

    // 인수 가격(Money) — 안쪽 싸게 → 바깥 비싸게
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 MoneyPrice = 0;

    // 이 블록에 지을 수 있는 건물 최대 채수
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BuildingCapacity = 1;

    // 시작부터 소유(시작 부지 = true 1개)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOwnedAtStart = false;

    // 이 부지에 입주한 회사들(BP_MB 키). 빈 배열 = 빈 부지(바로 구매 가능).
    // 병합 부지는 여러 블록에 걸쳐 회사도 여럿 서 있다 — 전원 Cleared 여야 부지를 살 수 있다.
    // CSV 표기: "(21,14,7)" / 빈 부지는 "()".
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> OccupantCompanyKeys;
};
