// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enum/LootBoxRarity.h"

/**
 * 직원창(홈 · 로스터 타일 · 잠재 드로어 · 장비 슬롯)이 공유하는 표시색/잉크 단일 정의.
 * 전역 등급색 테이블(FLootBoxRarityUtility)은 상점·가챠·도감이 함께 쓰므로 고치지 않는다.
 */
namespace EmployeeRarityDisplay
{
	// 전역 Common 은 무채 회색이라 다크 판에서 "색이 없는" 칸으로 읽힌다 — 이 창에서만 한 톤 올린다
	FORCEINLINE FLinearColor CommonColor()
	{
		return FLinearColor::FromSRGBColor(FColor(0x8F, 0xB4, 0xC9));
	}

	// 창 안 모든 등급 틴트(칩·글로우·림·줄 아웃라인·슬롯 프레임)가 지나는 단일 함수
	FORCEINLINE FLinearColor Color(ELootBoxRarity Rarity)
	{
		return Rarity == ELootBoxRarity::Common ? CommonColor() : FLootBoxRarityUtility::GetRarityColor(Rarity);
	}

	// 전역 표는 sRGB 16진 값을 선형 자리에 그대로 담아 화면에선 한 톤 옅게 나온다(칩·림은 그 옅은 톤에 맞춰져 있다).
	// 연출의 빛·띠처럼 16진 값 그대로의 진한 등급색이 필요한 곳만 이 함수를 쓴다
	FORCEINLINE FLinearColor VividColor(ELootBoxRarity Rarity)
	{
		return Rarity == ELootBoxRarity::Common ? CommonColor() : FLinearColor::FromSRGBColor(FLootBoxRarityUtility::GetRarityColor(Rarity).ToFColor(false));
	}
}

namespace EmployeeWindowInk
{
	// #ECEEF0 — 본문
	FORCEINLINE FLinearColor Body()
	{
		return FLinearColor::FromSRGBColor(FColor(0xEC, 0xEE, 0xF0));
	}

	// #A7B1BE — 보조/뮤트
	FORCEINLINE FLinearColor Mute()
	{
		return FLinearColor::FromSRGBColor(FColor(0xA7, 0xB1, 0xBE));
	}
}
