#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
struct FEmployeeEquipmentGradeRow;

struct COMPANYGROWTHRENEWAL_API FEmployeeEquipmentVisuals
{
    // 3D와 UI가 같은 등급 파라미터를 사용한다. Strength는 각 표시 문맥이 선택한다.
    static void ApplyParameters(UMaterialInstanceDynamic* Material,
        const FEmployeeEquipmentGradeRow& Grade, float Strength, bool bAnimate = true);
};
