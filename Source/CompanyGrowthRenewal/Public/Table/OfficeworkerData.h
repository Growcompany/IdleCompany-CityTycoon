// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/DataTable.h"
//#include "BehaviorTree/BehaviorTree.h"
//#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

#include "OfficeworkerData.generated.h"

USTRUCT(BlueprintType)
struct FOfficeworkerData : public FTableRowBase
{
	GENERATED_BODY()

	//UPROPERTY(EditAnywhere, BlueprintReadWrite)
	//TSoftObjectPtr<UBehaviorTree> BehaviorTree;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite)
	//TSoftObjectPtr<UAnimMontage> WorkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USkeletalMesh> Hat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> Tool;
};
