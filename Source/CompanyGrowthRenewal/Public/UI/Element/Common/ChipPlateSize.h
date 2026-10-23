// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UImage;
class UMaterialInstanceDynamic;

/**
 * SDF 칩 플레이트(MI_UI_ChipBG/ChipLine_EquipPlate) 2층에 실치수를 넣는다.
 * 공유 MI 의 Wpx/Hpx 는 씨앗값이라 그대로 쓰면 코너가 타원이 된다(카탈로그 §1, 선례 UWorkstationInfoWidget).
 * MID 는 이미지 브러시가 강참조하므로 여기서는 약참조만 들고 있다.
 */
struct COMPANYGROWTHRENEWAL_API FChipPlateSizer
{
	/** NativeConstruct 1회 — 틱마다 만들면 공유 MI 인스턴스가 프레임마다 새로 생긴다. */
	void Bind(UImage* InPlateBG, UImage* InPlateLine);

	/** NativeTick — 크기가 바뀐 프레임에만 재주입. */
	void Update();

private:
	TWeakObjectPtr<UImage> PlateBG;
	TWeakObjectPtr<UImage> PlateLine;
	TWeakObjectPtr<UMaterialInstanceDynamic> PlateBGMID;
	TWeakObjectPtr<UMaterialInstanceDynamic> PlateLineMID;
	FVector2D LastPlateSize = FVector2D::ZeroVector;
};
