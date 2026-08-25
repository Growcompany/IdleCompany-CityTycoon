// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "Enum/InteractableType.h"
#include "Player/Components/PlacementHandler.h"
#include "Interfaces/IInputHandler.h"
#include "GameFramework/Actor.h"
#include "InteractableBaseActor.generated.h"

class UBoxComponent;
class UTextureRenderTarget2D;
class UPlacementHandler;
class UStaticMesh;
class UCurveFloat;
class FDelegateHandle;
class UEntityManager;

struct FTimeline;
struct FInteractableInfo;

DECLARE_MULTICAST_DELEGATE(FReCalcBoxExtentDelegate);

UCLASS()
class COMPANYGROWTHRENEWAL_API AInteractableBaseActor : public AActor, public IInputHandler
{
	GENERATED_BODY()
	
public:	
	AInteractableBaseActor();

	virtual void ReCalcBoxExtent() const;
	void UpdateNavBlockerState();
	void DetachNavBlockerForMove();
	void AttachNavBlockerToNewLocation();

	bool IsWoobleAnimationPlaying() const
	{
		return Wooble_Timeline.IsPlaying();
	}

private:
	void InitializeWithInteractableInfo();
	void CheckOverlappingActor();

protected:
	TSharedPtr<FReCalcBoxExtentDelegate> ReCalcBoxExtentDelegate = nullptr;

	// 데이터 테이블의 RowName (예: "Building_1", "Resource_Wood")
	UPROPERTY(EditAnywhere)
	FName InteractableRowName;

	UPROPERTY(EditAnywhere)
	FName Name;

	UPROPERTY(EditAnywhere)
	EInteractableType InteractableType = EInteractableType::None;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* DefaultSceneRoot;

	// Navigation Blocker
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation")
	UBoxComponent* NavBlocker;

	UPROPERTY(EditAnywhere, Category = "Spawn Info")
	float BoundGap = 0.0f;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MainMeshComponent;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* BoxComponent;
	UMaterial* ShapeDrawMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UCurveFloat* Wooble_Curve; // timeline

	FTimeline Wooble_Timeline;

	virtual void BeginPlay() override;

	virtual void InitializeStaticMesh(const FName& name) {}

	UFUNCTION()
	virtual void Wooble_TimelineUpdate(float scaleValue);
	UFUNCTION()
	virtual void Wooble_TimelineFinished();

	UPROPERTY()
	UEntityManager* EntityManager = nullptr;

	UFUNCTION()
	virtual void RegisterWithEntityManager() PURE_VIRTUAL(AInteractableBaseActor::RegisterWithEntityManager, );

	UFUNCTION()
	virtual void UnregisterWithEntityManager() PURE_VIRTUAL(AInteractableBaseActor::UnregisterWithEntityManager, );

public:
	virtual void Tick(float DeltaSeconds) override;

	// 배치 확정 후 EntityManager에 재등록
	void FinalizeEntityRegistration() { RegisterWithEntityManager(); }

	template <typename T>
	FDelegateHandle AddReCalcBoxExtentDelegateHandle(T* ClassInstance,
		void (T::* InFunc)())
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must be derived from UObject");
		if (!ReCalcBoxExtentDelegate)
		{
			ReCalcBoxExtentDelegate = MakeShareable(new FReCalcBoxExtentDelegate());
		}

		return ReCalcBoxExtentDelegate->AddUObject(ClassInstance, InFunc);
	}

	void RemoveReCalcBoxExtentDelegateHandle(FDelegateHandle delegateHandle)
	{
		if (!ReCalcBoxExtentDelegate)
		{
			return;
		}

		ReCalcBoxExtentDelegate->Remove(delegateHandle);
	}

	virtual void OnConstruction(const FTransform& Transform) override;

	// returnVal : work time
	virtual float Interact()
	{
		return 0.0f;
	}

	virtual void SetInteractableInfo(const FInteractableInfo& InInfo);

	FName GetInteractableName() const
	{
		return Name;
	}

	// 데이터테이블 RowName (예: "B1"). DT 조회 키 — Name(표시명) 과 다르므로 DT lookup 엔 이걸 쓸 것.
	FName GetInteractableRowName() const
	{
		return InteractableRowName;
	}

	FORCEINLINE UMeshComponent* GetMainMeshComponent() const
	{
		return MainMeshComponent;
	}

	virtual void PlayWobble();
	virtual void EndWooble();

	/// Sort Temp
	float DistSquared = 0.0f;
	void CalcTemporaryDistanceFromLocation(FVector InLocation);

	float GetDistSquared() const
	{
		return DistSquared;
	}
};
