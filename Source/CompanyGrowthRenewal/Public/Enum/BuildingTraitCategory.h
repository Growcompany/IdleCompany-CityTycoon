// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BuildingTraitCategory.generated.h"

// 건물 특성 분야 (19개). 분야:대상 = 1:1 — 대상이 겹치면 세트만 갈라져 플레이어가 손해를 본다.
// Manufacturing/Project 는 타입 전용이라 세트 보너스 없음.
UENUM(BlueprintType)
enum class EBuildingTraitCategory : uint8
{
	Revenue			UMETA(DisplayName = "수익"),
	Efficiency		UMETA(DisplayName = "효율"),
	Idle			UMETA(DisplayName = "방치"),
	Growth			UMETA(DisplayName = "성장"),
	Management		UMETA(DisplayName = "경영"),
	Fortune			UMETA(DisplayName = "행운"),

	// 타입 전용 (세트 보너스 없음)
	Manufacturing	UMETA(DisplayName = "제조전용"),
	Project			UMETA(DisplayName = "프로젝트전용"),

	RnD				UMETA(DisplayName = "R&D"),
	Welfare			UMETA(DisplayName = "복지"),
	Analytics		UMETA(DisplayName = "데이터"),
	// 구 법무/보안/위기관리 통합 — 도전 컨텐츠 폐기(2026-07-23)로 원 정체성이 갈 곳을 잃었다
	Risk			UMETA(DisplayName = "리스크"),
	ESG				UMETA(DisplayName = "ESG"),
	Global			UMETA(DisplayName = "글로벌"),
	CRM				UMETA(DisplayName = "CRM"),
	Network			UMETA(DisplayName = "파트너십"),
	IP				UMETA(DisplayName = "IP"),
	HR				UMETA(DisplayName = "HR"),
	Mindset			UMETA(DisplayName = "마인드셋")
};

/**
 * 특성 분야 표시명 — 런타임(UI) 표시는 반드시 이 함수를 경유할 것.
 * ⚠ `UEnum::GetDisplayNameTextByValue` 금지: UMETA 조회가 `#if WITH_EDITOR` 안에만 있어 패키징 빌드에서는
 *   식별자("Revenue"/"Efficiency")로 떨어진다. PIE 는 WITH_EDITOR=1 이라 멀쩡해 보여 안 걸린다.
 * 위 UMETA 문자열과 여기 문자열은 같이 고칠 것(에디터 표시 = UMETA, 런타임 표시 = 여기).
 */
inline FText GetBuildingTraitCategoryDisplayName(EBuildingTraitCategory Category)
{
	switch (Category)
	{
	case EBuildingTraitCategory::Revenue:       return FText::FromString(TEXT("수익"));
	case EBuildingTraitCategory::Efficiency:    return FText::FromString(TEXT("효율"));
	case EBuildingTraitCategory::Idle:          return FText::FromString(TEXT("방치"));
	case EBuildingTraitCategory::Growth:        return FText::FromString(TEXT("성장"));
	case EBuildingTraitCategory::Management:    return FText::FromString(TEXT("경영"));
	case EBuildingTraitCategory::Fortune:       return FText::FromString(TEXT("행운"));
	case EBuildingTraitCategory::Manufacturing: return FText::FromString(TEXT("제조전용"));
	case EBuildingTraitCategory::Project:       return FText::FromString(TEXT("프로젝트전용"));
	case EBuildingTraitCategory::RnD:           return FText::FromString(TEXT("R&D"));
	case EBuildingTraitCategory::Welfare:       return FText::FromString(TEXT("복지"));
	case EBuildingTraitCategory::Analytics:     return FText::FromString(TEXT("데이터"));
	case EBuildingTraitCategory::Risk:          return FText::FromString(TEXT("리스크"));
	case EBuildingTraitCategory::ESG:           return FText::FromString(TEXT("ESG"));
	case EBuildingTraitCategory::Global:        return FText::FromString(TEXT("글로벌"));
	case EBuildingTraitCategory::CRM:           return FText::FromString(TEXT("CRM"));
	case EBuildingTraitCategory::Network:       return FText::FromString(TEXT("파트너십"));
	case EBuildingTraitCategory::IP:            return FText::FromString(TEXT("IP"));
	case EBuildingTraitCategory::HR:            return FText::FromString(TEXT("HR"));
	case EBuildingTraitCategory::Mindset:       return FText::FromString(TEXT("마인드셋"));
	default:                                    return FText::GetEmpty();
	}
}
