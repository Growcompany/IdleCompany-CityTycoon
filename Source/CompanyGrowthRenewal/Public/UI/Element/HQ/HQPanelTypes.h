// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

enum class EHQIndustryState : uint8
{
	Operating,
	NotEntered,
	Locked
};

struct FHQIndustryMetrics
{
	int32 Buildings = 0;
	int32 Employees = 0;
	int32 HighestTier = 0;
	double NetPerSec = 0.0;
};

// 레벨업 보상 칸의 외형 — 목업 .tile 변형(기본 / 업종 테 / 금테 기능 / 어두운 메뉴 글리프 / 빌딩 사진)
enum class EHQRewardSlotStyle : uint8
{
	Plain,
	Industry,
	Feature,
	Menu,
	Building
};

// 보상 칸 한 개 — 각 해금의 원래 표(업종·공장·메뉴·빌딩)와 지급 규칙에서 파생한다
struct FHQRewardSlot
{
	EHQRewardSlotStyle Style = EHQRewardSlotStyle::Plain;
	UObject* Icon = nullptr;
	FText Caption;
	FText Count;
	FLinearColor Accent = FLinearColor::White;
};

// DT AccentColor(linear)에서 본사 패널 색을 파생한다. 목업(docs/05_UI/HQPanel_3Tab_MOCKUP.html)과 같은 sRGB 공식이다.
namespace HQIndustryPalette
{
	inline FLinearColor ScaleSRGB(const FLinearColor& Accent, float Factor)
	{
		const FColor S = Accent.ToFColor(true);
		auto Channel = [Factor](uint8 V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(V * Factor), 0, 255)); };
		return FLinearColor(FColor(Channel(S.R), Channel(S.G), Channel(S.B), 255));
	}

	inline FLinearColor TowardWhiteSRGB(const FLinearColor& Accent, float AccentAmount)
	{
		const FColor S = Accent.ToFColor(true);
		auto Channel = [AccentAmount](uint8 V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(255.f + (V - 255.f) * AccentAmount), 0, 255)); };
		return FLinearColor(FColor(Channel(S.R), Channel(S.G), Channel(S.B), 255));
	}

	inline FLinearColor RibbonTop(const FLinearColor& Accent)    { return ScaleSRGB(Accent, 0.92f); }
	inline FLinearColor RibbonBottom(const FLinearColor& Accent) { return ScaleSRGB(Accent, 0.76f); }
	inline FLinearColor StripFill(const FLinearColor& Accent)    { return TowardWhiteSRGB(Accent, 0.15f); }
	inline FLinearColor StripInk(const FLinearColor& Accent)     { return ScaleSRGB(Accent, 0.52f); }
}
