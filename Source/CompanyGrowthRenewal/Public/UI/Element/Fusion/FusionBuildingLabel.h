// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UWidget;

/**
 * 융합 슬롯·행에 쓰는 건물 표시명.
 * 업종명만으로는 같은 업종 건물이 구분되지 않아 카드 테이블의 건물명을 쓴다(선례 UOfflineGainRowWidget).
 */
namespace CGFusionLabel
{
	/** 인덱스 → 액터 → BuildingID → 카드 행. 조회 실패 시 "건물 N" 으로 내려간다. */
	COMPANYGROWTHRENEWAL_API FText BuildingDisplayName(const UWidget* Context, int32 BuildingIndex);
}
