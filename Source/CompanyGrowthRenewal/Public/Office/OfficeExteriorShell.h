#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Office/OfficeExteriorLayout.h"
#include "OfficeExteriorShell.generated.h"

class AOfficeInterior;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 사무실 풋프린트 아래의 건물 외벽과 하부 타워만 표시한다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeExteriorShell : public AActor
{
	GENERATED_BODY()

public:
	AOfficeExteriorShell();

	UFUNCTION(BlueprintCallable, Category = "Office|Exterior")
	bool SynchronizeToInterior();

	UFUNCTION(BlueprintCallable, Category = "Office|Exterior|Skyline")
	void SetSkylineClimbLift(float LiftCm);

	UFUNCTION(BlueprintPure, Category = "Office|Exterior|Skyline")
	float GetSkylineClimbLift() const { return SkylineClimbLiftCm; }

	UFUNCTION(BlueprintPure, Category = "Office|Exterior")
	FIntPoint GetLastBuiltTileCount() const { return LastBuiltTileCount; }

	UFUNCTION(BlueprintPure, Category = "Office|Exterior")
	int32 GetDynamicInstanceCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<USceneComponent> RootScene;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<UStaticMeshComponent> TowerBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<UStaticMeshComponent> TowerTransferLevel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<UStaticMeshComponent> ApronSlab;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<UInstancedStaticMeshComponent> FacadeApronISM;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<UInstancedStaticMeshComponent> FacadeFloorBandISM;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<UInstancedStaticMeshComponent> FacadeCornerISM;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office|Exterior")
	TObjectPtr<UInstancedStaticMeshComponent> SkylineClimbBandISM;

	UPROPERTY(EditInstanceOnly, Category = "Office|Exterior")
	TObjectPtr<AOfficeInterior> TargetInterior;
	UPROPERTY(EditAnywhere, Category = "Office|Exterior|Preview")
	FIntPoint PreviewTileCount = FIntPoint(1, 2);
	UPROPERTY(EditAnywhere, Category = "Office|Exterior|Preview", meta = (ClampMin = "0.0", Units = "cm"))
	float PreviewOfficeHeightCm = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UStaticMesh> FacadeBayMesh;
	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> StructureMaterial;
	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> TowerContinuationMaterial;
	UPROPERTY(EditAnywhere, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> RoofDeckMaterial;
	UPROPERTY(EditAnywhere, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> RecessMaterial;
	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> GlassTrimMaterial;
	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Structure", meta = (ClampMin = "1.0", Units = "cm"))
	float TowerHeightCm = FOfficeExteriorLayoutInput::DefaultTowerHeightCm;

private:
	void ResolveTargetInterior();
	void HandleFootprintChanged(FIntPoint TileCount);
	bool BuildLayoutFromInterior(FOfficeExteriorLayoutResult& OutLayout);
	bool BuildPreviewLayout();
	void ApplyLayout(const FOfficeExteriorLayoutResult& Layout);
	void RefreshSkylineClimbBands();
	void ApplyConfiguredMaterials();
	void AlignToInterior();
	static void ConfigureVisualComponent(UPrimitiveComponent* Component);

	UPROPERTY(Transient)
	float SkylineClimbLiftCm = 0.f;
	FOfficeExteriorLayoutResult LastClimbLayout;
	FIntPoint LastBuiltTileCount = FIntPoint::ZeroValue;
	bool bHasBuiltLayout = false;
	bool bWarnedMissingFacadeMesh = false;
	bool bWarnedMissingInterior = false;
};
