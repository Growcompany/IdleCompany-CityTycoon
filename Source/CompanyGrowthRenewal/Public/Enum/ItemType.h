// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ItemType.generated.h"

// 아이템 타입 열거형 (채용권, 강화 재료, 특수 아이템 등)
UENUM(BlueprintType)
enum class EItemType : uint8
{
	None = 0,

	// 채용권
	RecruitTicketNormal     UMETA(DisplayName = "일반 채용권"),
	RecruitTicketAdvanced   UMETA(DisplayName = "고급 채용권"),
	RecruitTicketPremium    UMETA(DisplayName = "프리미엄 채용권"),

	// 강화 재료
	EnhanceScroll           UMETA(DisplayName = "강화 주문서"),
	ProtectionScroll        UMETA(DisplayName = "보호 주문서"),
	DestructionShield       UMETA(DisplayName = "파괴방지권"),
	LuckyEnhanceTicket      UMETA(DisplayName = "럭키 강화권"),

	// 특수
	DepartmentTicket        UMETA(DisplayName = "부서 지정 채용권"),
	BusinessCardPaper       UMETA(DisplayName = "종이 명함"),
	AdditionalReroll        UMETA(DisplayName = "추가옵션 재설정권"),
	BusinessCardGold        UMETA(DisplayName = "골드 명함"),
	BusinessCardBlack       UMETA(DisplayName = "블랙 명함"),

	// 건물 특성 뽑기권 (2026-05-21 BUILDING_TRAIT_SYSTEM v1.1)
	BuildingTraitTicketNormal   UMETA(DisplayName = "일반 특성 뽑기권"),
	BuildingTraitTicketAdvanced UMETA(DisplayName = "고급 특성 뽑기권"),

	// 건물 스킨 뽑기권 (특성과 동일 모델 — 2티어 독립 천장)
	SkinTicketNormal            UMETA(DisplayName = "일반 스킨 뽑기권"),
	SkinTicketAdvanced          UMETA(DisplayName = "고급 스킨 뽑기권"),

	// 트렌드 갱신 (기획 보드 — 산업 트렌드 소재 재추첨 소모, 다이아 상점 판매)
	PitchRefreshScroll          UMETA(DisplayName = "트렌드 갱신 스크롤"),

	Count UMETA(Hidden)
};

/**
 * 아이템 표시명 — 런타임(UI) 표시는 반드시 이 함수를 경유할 것.
 * ⚠ `UEnum::GetDisplayNameTextByValue` 금지: UMETA 조회가 `#if WITH_EDITOR` 안에만 있어 패키징 빌드에서는
 *   식별자("BusinessCardPaper"/"EnhanceScroll")로 떨어진다. PIE 는 WITH_EDITOR=1 이라 멀쩡해 보여 안 걸린다.
 * 위 UMETA 문자열과 여기 문자열은 같이 고칠 것(에디터 표시 = UMETA, 런타임 표시 = 여기).
 */
inline FText GetItemTypeDisplayName(EItemType ItemType)
{
	switch (ItemType)
	{
	case EItemType::RecruitTicketNormal:         return FText::FromString(TEXT("일반 채용권"));
	case EItemType::RecruitTicketAdvanced:       return FText::FromString(TEXT("고급 채용권"));
	case EItemType::RecruitTicketPremium:        return FText::FromString(TEXT("프리미엄 채용권"));
	case EItemType::EnhanceScroll:               return FText::FromString(TEXT("강화 주문서"));
	case EItemType::ProtectionScroll:            return FText::FromString(TEXT("보호 주문서"));
	case EItemType::DestructionShield:           return FText::FromString(TEXT("파괴방지권"));
	case EItemType::LuckyEnhanceTicket:          return FText::FromString(TEXT("럭키 강화권"));
	case EItemType::DepartmentTicket:            return FText::FromString(TEXT("부서 지정 채용권"));
	case EItemType::BusinessCardPaper:           return FText::FromString(TEXT("종이 명함"));
	case EItemType::AdditionalReroll:            return FText::FromString(TEXT("추가옵션 재설정권"));
	case EItemType::BusinessCardGold:            return FText::FromString(TEXT("골드 명함"));
	case EItemType::BusinessCardBlack:           return FText::FromString(TEXT("블랙 명함"));
	case EItemType::BuildingTraitTicketNormal:   return FText::FromString(TEXT("일반 특성 뽑기권"));
	case EItemType::BuildingTraitTicketAdvanced: return FText::FromString(TEXT("고급 특성 뽑기권"));
	case EItemType::SkinTicketNormal:            return FText::FromString(TEXT("일반 스킨 뽑기권"));
	case EItemType::SkinTicketAdvanced:          return FText::FromString(TEXT("고급 스킨 뽑기권"));
	case EItemType::PitchRefreshScroll:          return FText::FromString(TEXT("트렌드 갱신 스크롤"));
	default:                                     return FText::GetEmpty();
	}
}
