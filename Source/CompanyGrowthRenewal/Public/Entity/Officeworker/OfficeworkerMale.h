// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/Officeworker/ModularOfficeworker.h"
#include "OfficeworkerMale.generated.h"

/**
 *
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeworkerMale : public AModularOfficeworker
{
	GENERATED_BODY()

public:
	AOfficeworkerMale();

	virtual EEmployeeGender GetCharacterGender() const override { return EEmployeeGender::Male; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void SetHairCombination(int32 CombinationType, int32 RandomSeed) override;

protected:
	virtual void PostInitializeComponents() override;

	virtual void LoadAssetsAsync() override;
	virtual void OnAssetsLoaded() override;
};
