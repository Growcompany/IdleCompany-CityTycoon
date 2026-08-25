// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UIVFXTexTable.generated.h"

/**
 * UI VFX 텍스처 레지스트리 행 (DT_UIVFXTexture) — 키 하나로 연출 텍스처+브러시 크기+기본 틴트를 공급.
 * 레이아웃(위치/클리핑)은 WBP의 빈 Image가, 에셋은 이 DT가 단일 진실.
 * 소스 카탈로그: docs/05_UI/UI_VFX_SOURCEBOOK.md
 */
USTRUCT(BlueprintType)
struct FUIVFXTexRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Texture;

	// 브러시 ImageSize (슬롯 레이아웃 크기와 별개)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D ImageSize = FVector2D(64.0, 64.0);

	// 기본 틴트 (linear) — 등급색 등 런타임 결정이 필요하면 소비처가 덮어씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Tint = FLinearColor::White;
};
