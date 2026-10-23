#include "Utils/EmployeeEquipmentVisuals.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Table/EmployeeEquipmentTable.h"

void FEmployeeEquipmentVisuals::ApplyParameters(UMaterialInstanceDynamic* Material,
    const FEmployeeEquipmentGradeRow& Grade, float Strength, bool bAnimate)
{
    if (!Material) { return; }
    Material->SetVectorParameterValue(TEXT("EquipmentFXTint"), Grade.VisualTint);
    Material->SetScalarParameterValue(TEXT("EquipmentFXShine"), Grade.ShineStrength);
    Material->SetScalarParameterValue(TEXT("EquipmentFXRainbow"), Grade.RainbowStrength);
    Material->SetScalarParameterValue(TEXT("EquipmentFXSparkle"), Grade.SparkleStrength);
    Material->SetScalarParameterValue(TEXT("EquipmentFXSpeed"), Grade.FlowSpeed);
    Material->SetScalarParameterValue(TEXT("EquipmentFXScale"), Grade.PatternScale);
    Material->SetScalarParameterValue(TEXT("EquipmentFXStrength"), Strength);
    Material->SetScalarParameterValue(TEXT("EquipmentFXAnimate"), bAnimate ? 1.f : 0.f);
    Material->SetScalarParameterValue(TEXT("EquipmentFXPhase"), 0.f);
}
