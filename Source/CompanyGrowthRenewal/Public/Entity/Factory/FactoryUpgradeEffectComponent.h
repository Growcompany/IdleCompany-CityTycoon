#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FactoryUpgradeEffectComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

DECLARE_MULTICAST_DELEGATE(FFactoryUpgradeEffectEvent);

/** A short-lived panel shell around an attachment's local bounds. */
UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class COMPANYGROWTHRENEWAL_API UFactoryUpgradeEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFactoryUpgradeEffectComponent();

	UPROPERTY(EditAnywhere, Category="Factory|Upgrade Effect")
	TSoftObjectPtr<UStaticMesh> PanelMesh;

	UPROPERTY(EditAnywhere, Category="Factory|Upgrade Effect")
	TSoftObjectPtr<UMaterialInterface> PanelMaterial;

	/** Optional additive gold outline and reveal fragments. */
	UPROPERTY(EditAnywhere, Category="Factory|Upgrade Effect")
	TSoftObjectPtr<UMaterialInterface> AccentMaterial;

	UPROPERTY(EditAnywhere, Category="Factory|Upgrade Effect", meta=(ClampMin="1.5", ClampMax="2.5", UIMin="1.5", UIMax="2.5"))
	float Duration = 1.90f;

	UPROPERTY(EditAnywhere, Category="Factory|Upgrade Effect", meta=(ClampMin="0.0", ClampMax="6.0"))
	float PanelGap = 2.0f;

	UPROPERTY(EditAnywhere, Category="Factory|Upgrade Effect", meta=(ClampMin="0.0"))
	float ShellPadding = 65.0f;

	/** Repeated calls during playback are coalesced without changing the active shell. */
	bool Play(USceneComponent* Attachment, const FBox& LocalBounds);

	UFUNCTION(BlueprintCallable, Category="Factory|Upgrade Effect")
	bool PlayForBounds(USceneComponent* Attachment, FVector Center, FVector Extent);

	UFUNCTION(BlueprintCallable, Category="Factory|Upgrade Effect")
	void Cancel();

	UFUNCTION(BlueprintPure, Category="Factory|Upgrade Effect")
	bool IsPlaying() const { return bPlaying; }

	/** Target appearance must be swapped while the shell is fully closed. */
	FFactoryUpgradeEffectEvent OnConcealed;
	/** A short chimney puff can be triggered as the shell clears. */
	FFactoryUpgradeEffectEvent OnRevealed;
	FFactoryUpgradeEffectEvent OnFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FPanel
	{
		FVector Position = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector Scale = FVector::OneVector;
		FVector Normal = FVector::UpVector;
		FVector Tangent = FVector::ForwardVector;
		float Width = 100.f;
		float Height = 100.f;
		float ArrivalStart = 0.f;
		float PeelStart = 0.f;
		float Drift = 0.f;
	};

	void PrecacheAssets();
	void BuildPanels(const FBox& LocalBounds);
	void AddPanel(const FVector& Position, const FVector& Normal, const FVector& Tangent,
		float Width, float Height, float ArrivalStart, float PeelStart, float Drift);
	void Render(float NormalizedTime);
	void RenderAccents(float NormalizedTime);

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> PanelInstances;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> AccentInstances;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedAccentMaterial;

	TArray<FPanel> Panels;
	FBox ActiveShellBounds;
	float Elapsed = 0.f;
	float ActiveDuration = 1.90f;
	uint32 PlayGeneration = 0;
	bool bPlaying = false;
	bool bConcealedSent = false;
	bool bRevealedSent = false;
};
