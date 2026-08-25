// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NiagaraSystem.h"
#include "Enum/VFXType.h"
#include "VFXTable.generated.h"

/**
 * VFX 테이블 행 구조체
 * DataTable에서 VFX 에셋을 관리
 */
USTRUCT(BlueprintType)
struct FVFXTableRow : public FTableRowBase
{
	GENERATED_BODY()

	// VFX 타입 (키값으로 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	EVFXType VFXType = EVFXType::None;

	// 표시 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	FText DisplayName;

	// Niagara VFX 에셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> VFXAsset;

	// 기본 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float DefaultScale = 1.0f;

	// 타겟 기준 오프셋 (예: 캐릭터 머리 위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	FVector LocationOffset = FVector(0.f, 0.f, 100.f);

	FVFXTableRow()
		: VFXType(EVFXType::None)
		, DisplayName(FText::GetEmpty())
		, VFXAsset(nullptr)
		, DefaultScale(1.0f)
		, LocationOffset(FVector(0.f, 0.f, 100.f))
	{}
};
