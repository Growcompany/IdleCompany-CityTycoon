// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/LootBoxRarity.h"

/**
 * 건물 종류별 '처음 끼고 있는' 기본 스킨 — 종류마다 고정이라 배치 프리뷰부터 최종 색으로 보인다.
 * 등급 밴드는 발자국 칸 수로 나누고, Legendary/Mythic 은 스킨 가챠 전용이라 밴드에서 제외한다.
 */
namespace CGBuildingDefaultSkin
{
	/** 발자국 칸 수 -> 기본 스킨 등급 밴드 */
	COMPANYGROWTHRENEWAL_API ELootBoxRarity BandForFootprint(int32 WidthCells, int32 DepthCells);

	/** DT_Building 행 이름 -> 고정 기본 스킨 ID. 표에 없으면 INDEX_NONE. */
	COMPANYGROWTHRENEWAL_API int32 FindDefaultSkinID(FName BuildingRowName);

	/** 고정 표 전체 — 회귀 테스트와 기존 세이브 재배정이 순회한다 */
	COMPANYGROWTHRENEWAL_API const TMap<FName, int32>& GetTable();
}
