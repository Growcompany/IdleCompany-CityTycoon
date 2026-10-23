// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Common/ChipPlateSize.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void FChipPlateSizer::Bind(UImage* InPlateBG, UImage* InPlateLine)
{
	PlateBG = InPlateBG;
	PlateLine = InPlateLine;
	PlateBGMID = InPlateBG ? InPlateBG->GetDynamicMaterial() : nullptr;
	PlateLineMID = InPlateLine ? InPlateLine->GetDynamicMaterial() : nullptr;
	LastPlateSize = FVector2D::ZeroVector;
}

void FChipPlateSizer::Update()
{
	UMaterialInstanceDynamic* BGMID = PlateBGMID.Get();
	UMaterialInstanceDynamic* LineMID = PlateLineMID.Get();
	if (!BGMID && !LineMID)
	{
		return;
	}

	// 위젯 전체가 아니라 플레이트 자신의 크기여야 한다 — 내용 길이에 따라 위젯이 넓어질 수 있다
	const UWidget* SizeSource = PlateBG.IsValid()
		? static_cast<const UWidget*>(PlateBG.Get())
		: static_cast<const UWidget*>(PlateLine.Get());
	if (!SizeSource)
	{
		return;
	}

	const FVector2D PlateSize = SizeSource->GetCachedGeometry().GetLocalSize();
	if (PlateSize.X < 1.f || PlateSize.Y < 1.f || PlateSize.Equals(LastPlateSize, 0.5f))
	{
		return;
	}
	LastPlateSize = PlateSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (UMaterialInstanceDynamic* MID : { BGMID, LineMID })
	{
		if (!MID)
		{
			continue;
		}
		MID->SetScalarParameterValue(WpxParam, static_cast<float>(PlateSize.X));
		MID->SetScalarParameterValue(HpxParam, static_cast<float>(PlateSize.Y));
	}
}
