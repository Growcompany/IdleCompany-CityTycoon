// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInputHandler.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInputHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class COMPANYGROWTHRENEWAL_API IInputHandler
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** 클릭했을 때 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void OnInteract(APlayerController* InstigatingPC);
	virtual void OnInteract_Implementation(APlayerController* InstigatingPC) {}

	/** 클릭을 뗐을 때 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void OnEndInteract(APlayerController* InstigatingPC);
	virtual void OnEndInteract_Implementation(APlayerController* InstigatingPC) {}
};
