// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GachaTier.generated.h"

// 가챠 뽑기 등급 (채용권 종류와 1:1 대응)
UENUM(BlueprintType)
enum class EGachaTier : uint8
{
	Normal    UMETA(DisplayName = "일반"),
	Advanced  UMETA(DisplayName = "고급"),
	Premium   UMETA(DisplayName = "프리미엄")
};
