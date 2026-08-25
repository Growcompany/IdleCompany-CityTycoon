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

UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeExteriorShell : public AActor
{
	GENERATED_BODY()

public:
	AOfficeExteriorShell();

	UFUNCTION(BlueprintCallable, Category = "Office|Exterior")
	bool SynchronizeToInterior();

	UFUNCTION(BlueprintPure, Category = "Office|Exterior")
	FIntPoint GetLastBuiltTileCount() const;

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
	TObjectPtr<UInstancedStaticMeshComponent> SkylineBlocksISM;

	UPROPERTY(EditInstanceOnly, Category = "Office|Exterior")
	TObjectPtr<AOfficeInterior> TargetInterior;

	UPROPERTY(EditAnywhere, Category = "Office|Exterior|Preview")
	FIntPoint PreviewTileCount = FIntPoint(1, 2);

	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UStaticMesh> FacadeBayMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> StructureMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> GlassTrimMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Office|Exterior|Assets")
	TObjectPtr<UMaterialInterface> SkylineMaterial;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Office|Exterior|Structure",
		meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float TowerHeightCm = FOfficeExteriorLayoutInput::DefaultTowerHeightCm;

private:
	void ResolveTargetInterior();
	void HandleFootprintChanged(FIntPoint TileCount);
	bool BuildPreviewLayout();
	void ApplyLayout(const FOfficeExteriorLayoutResult& Layout);
	void ApplyConfiguredMaterials();
	void AlignToInterior();
	static void ConfigureVisualComponent(UPrimitiveComponent* Component);

	FIntPoint LastBuiltTileCount = FIntPoint::ZeroValue;
	bool bHasBuiltLayout = false;
	bool bWarnedMissingFacadeMesh = false;
	bool bWarnedMissingOfficeGlassSlot = false;
	bool bWarnedMissingInterior = false;
};
