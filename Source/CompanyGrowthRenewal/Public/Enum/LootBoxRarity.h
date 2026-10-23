// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LootBoxRarity.generated.h"

// 루트 박스 등급 타입 (희귀도 오름차순)
UENUM(BlueprintType)
enum class ELootBoxRarity : uint8
{
	Common		UMETA(DisplayName = "Common"),      // ⚫ 일반
	Unusual		UMETA(DisplayName = "Unusual"),     // 🟢 고급
	Rare		UMETA(DisplayName = "Rare"),        // 🔵 레어
	Epic		UMETA(DisplayName = "Epic"),        // 🟣 에픽
	Legendary	UMETA(DisplayName = "Legendary"),   // 🟠 레전드리
	Mythic		UMETA(DisplayName = "Mythic")       // 🔴 신화
};

// 루트 박스 형태 타입
UENUM(BlueprintType)
enum class ELootBoxType : uint8
{
	Square		UMETA(DisplayName = "Square Chest"),
	Sphere		UMETA(DisplayName = "Sphere Chest")
};

/**
 * 루트박스 희귀도 관련 유틸리티
 * - 가중치 조회
 * - 랜덤 추첨 로직
 * - UI 색상 조회
 */
class COMPANYGROWTHRENEWAL_API FLootBoxRarityUtility
{
public:
	// 희귀도별 가중치 반환 (낮을수록 희귀)
	static int32 GetWeight(ELootBoxRarity Rarity)
	{
		switch (Rarity)
		{
		case ELootBoxRarity::Common:    return 50;  // 50%
		case ELootBoxRarity::Unusual:   return 25;  // 25%
		case ELootBoxRarity::Rare:      return 15;  // 15%
		case ELootBoxRarity::Epic:      return 7;   // 7%
		case ELootBoxRarity::Legendary: return 2;   // 2%
		case ELootBoxRarity::Mythic:    return 1;   // 1%
		default:                        return 1;
		}
	}

	// 희귀도별 UI 색상 반환
	static FLinearColor GetRarityColor(ELootBoxRarity Rarity)
	{
		switch (Rarity)
		{
		case ELootBoxRarity::Common:
			return FLinearColor(0xA6 / 255.f, 0xA6 / 255.f, 0xA6 / 255.f, 1.f); // 회색

		case ELootBoxRarity::Unusual:
			return FLinearColor(0x4C / 255.f, 0xD9 / 255.f, 0x64 / 255.f, 1.f); // 녹색

		case ELootBoxRarity::Rare:
			return FLinearColor(0x34 / 255.f, 0xAA / 255.f, 0xDC / 255.f, 1.f); // 파란색

		case ELootBoxRarity::Epic:
			return FLinearColor(0xAF / 255.f, 0x52 / 255.f, 0xDE / 255.f, 1.f); // 보라색

		case ELootBoxRarity::Legendary:
			return FLinearColor(0xFF / 255.f, 0xCC / 255.f, 0x00 / 255.f, 1.f); // 노란색

		case ELootBoxRarity::Mythic:
			return FLinearColor(0xFF / 255.f, 0x2D / 255.f, 0x55 / 255.f, 1.f); // 빨간색

		default:
			return FLinearColor(0xA6 / 255.f, 0xA6 / 255.f, 0xA6 / 255.f, 1.f);
		}
	}

	// 라이트 플레이트(크림 카드) 위 등급 텍스트 잉크 — 흰 바탕에서 골드가 묻히지 않게 채도↑ 명도↓
	static FLinearColor GetRarityInkOnLight(ELootBoxRarity Rarity)
	{
		FLinearColor HSV = GetRarityColor(Rarity).LinearRGBToHSV();
		HSV.G = FMath::Min(HSV.G * 1.25f, 1.f);
		HSV.B *= 0.86f;
		return HSV.HSVToLinearRGB();
	}

	// 희귀도 한글 이름 반환
	static FString GetKoreanName(ELootBoxRarity Rarity)
	{
		switch (Rarity)
		{
		case ELootBoxRarity::Common:    return TEXT("일반");
		case ELootBoxRarity::Unusual:   return TEXT("고급");
		case ELootBoxRarity::Rare:      return TEXT("레어");
		case ELootBoxRarity::Epic:      return TEXT("에픽");
		case ELootBoxRarity::Legendary: return TEXT("레전드");
		case ELootBoxRarity::Mythic:    return TEXT("신화");
		default:                        return TEXT("???");
		}
	}

	// 전체 희귀도 배열 (순회용)
	static TArray<ELootBoxRarity> GetAllRarities()
	{
		return {
			ELootBoxRarity::Common,
			ELootBoxRarity::Unusual,
			ELootBoxRarity::Rare,
			ELootBoxRarity::Epic,
			ELootBoxRarity::Legendary,
			ELootBoxRarity::Mythic
		};
	}

	// 가중치 기반 랜덤 희귀도 선택
	static ELootBoxRarity RollRandomRarity()
	{
		// 전체 가중치 합산
		int32 TotalWeight = 0;
		TArray<ELootBoxRarity> Rarities = GetAllRarities();
		for (ELootBoxRarity Rarity : Rarities)
		{
			TotalWeight += GetWeight(Rarity);
		}

		// 0 ~ (TotalWeight - 1) 사이 랜덤 값
		int32 RandomValue = FMath::RandRange(0, TotalWeight - 1);

		// 가중치 누적 합으로 선택
		int32 AccumulatedWeight = 0;
		for (ELootBoxRarity Rarity : Rarities)
		{
			AccumulatedWeight += GetWeight(Rarity);
			if (RandomValue < AccumulatedWeight)
			{
				return Rarity;
			}
		}

		// 폴백 (도달하지 않아야 함)
		return ELootBoxRarity::Common;
	}
};