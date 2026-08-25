// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/Officeworker/ModularOfficeworker.h"
#include "OfficeworkerFemale.generated.h"

/**
 *
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AOfficeworkerFemale : public AModularOfficeworker
{
	GENERATED_BODY()

public:
	AOfficeworkerFemale();

	virtual EEmployeeGender GetCharacterGender() const override { return EEmployeeGender::Female; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void SetHairCombination(int32 CombinationType, int32 RandomSeed) override;

protected:
	virtual void PostInitializeComponents() override;

	virtual void LoadAssetsAsync() override;
	virtual void OnAssetsLoaded() override;
};
