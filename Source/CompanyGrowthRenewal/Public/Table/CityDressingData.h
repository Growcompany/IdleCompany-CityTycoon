#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "CityDressingData.generated.h"

USTRUCT(BlueprintType)
struct FCityDressingData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	FName PresetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	FName SlotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	bool bForAuthoredPlots = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	bool bPlaceAlongBoundary = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "1"))
	float BoundarySpacing = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "0"))
	float BoundaryInset = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "1"))
	int32 ExpectedSlotCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "0"))
	int32 PlotCellsPerPatch = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "1"))
	int32 MaxPatchesPerPlot = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "0"))
	int32 CullDistance = 40000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	FVector LocalLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	FRotator LocalRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	FVector LocalScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	FVector2D HalfExtent = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	bool bCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing")
	bool bNavigation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "0.01"))
	float ScaleVariationMin = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Dressing", meta = (ClampMin = "0.01"))
	float ScaleVariationMax = 1.f;
};
