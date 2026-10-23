#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "Enum/ItemType.h"
#include "Table/FunnyPartPoolTable.h"
#include "EmployeeEquipmentTable.generated.h"

USTRUCT(BlueprintType)
struct FEmployeeEquipmentDefinitionRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentSlot EquipmentSlot = EEmployeeEquipmentSlot::Head;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentGrade Grade = EEmployeeEquipmentGrade::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PrimaryEffectId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SecondaryEffectId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> UniqueEffectIds;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AppearanceId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentEffectRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EffectId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText ManufacturingDescription;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Unit;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEquipmentEffectValueDisplay ValueDisplay = EEquipmentEffectValueDisplay::Increase;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bBasic = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<float> GradeValues;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> ScalarParameters;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AggregateCap = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float RankPower = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RulesVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Hook;
};

USTRUCT(BlueprintType)
struct FEquipmentPartOverride
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EFunnyPartSlot PartSlot = EFunnyPartSlot::Glasses;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PartRowId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Tint = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bOverrideTint = false;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentAppearanceRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AppearanceId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentSlot EquipmentSlot = EEmployeeEquipmentSlot::Head;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentGrade VisualGrade = EEmployeeEquipmentGrade::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FEquipmentPartOverride> PartOverrides;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bHideHair = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentGradeRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentGrade Grade = EEmployeeEquipmentGrade::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DismantleCores = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 EnhanceCoreCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UMaterialInterface> MeshMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UMaterialInterface> IconMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> BoxIcon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor VisualTint = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShineStrength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float RainbowStrength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float SparkleStrength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float FlowSpeed = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float PatternScale = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float WorldStrength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float IconIdleStrength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float IconFocusedStrength = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float IconRevealStrength = 0.f;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentSlotRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentSlot EquipmentSlot = EEmployeeEquipmentSlot::Head;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCanHide = false;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentEconomyRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CatalogVersion;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 InventoryCapacity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxEnhanceLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float PrimaryEnhancePerLevel = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float SecondaryScale = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 StarterTickets = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentGrade AutoLockFirstGrade = EEmployeeEquipmentGrade::Epic;

    // 채용 확정 시 신입에게 장비 1점을 끼워 줄 확률(0~100). 0 이면 신입은 항상 빈손이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "100")) int32 RecruitGiftChancePercent = 15;

    // 지급 등급. 상향하면 장비 뽑기의 가치가 그만큼 희석된다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEmployeeEquipmentGrade RecruitGiftGrade = EEmployeeEquipmentGrade::Common;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentBannerRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BannerId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PityGroupId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemType TicketType = EItemType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<int32> GradeWeights;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 EpicPity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 LegendaryPity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 AscendantPity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> DefinitionIds;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FDateTime StartsAtUtc;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FDateTime EndsAtUtc;
};
