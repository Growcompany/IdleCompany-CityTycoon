// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/EmployeeTypes.h"   // EEmployeeDepartment, EEmployeeRank
#include "Enum/GachaTier.h"       // EGachaTier
#include "WorkerCosmeticTable.generated.h"

class UStaticMesh;

/**
 * 스틱맨 직원(AStickOfficeworker) 외형 = 색 + 액세서리.
 * 3 축 → 3 시각 채널 (docs/01_Systems/Employee/WORKER_VISUAL_REDESIGN.md §3):
 *  · 희귀도(EGachaTier)        → 골드 손(HandMetalGold) + 림 색(RimColor)
 *  · 부서(EEmployeeDepartment) → 넥타이 색(TieColor)
 *  · 직급(EEmployeeRank)       → 안경 표시(bWearGlasses) + 신발 단계색(ShoeColor)
 *
 * RowName = 결정적 합성 키 "D{dept}_R{rank}_T{tier}" (AStickOfficeworker::MakeCosmeticRowName).
 * 단일 진실 원천 — 색/임계값은 코드 하드코딩 금지, 본 DT만 수정.
 */
USTRUCT(BlueprintType)
struct FWorkerCosmeticTable : public FTableRowBase
{
	GENERATED_BODY()

	// 축 식별자 (RowName이 실제 키, 아래는 가독성/검증용 미러)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEmployeeDepartment Department = EEmployeeDepartment::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEmployeeRank Rank = EEmployeeRank::Intern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGachaTier Tier = EGachaTier::Normal;

	// 머티리얼 벡터 파라미터 (RGBA 0~1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TieColor = FLinearColor::Gray;        // 상의(셔츠=토르소+팔) 색 — 부서 채널

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor PantsColor = FLinearColor(0.22f, 0.25f, 0.32f, 1.f);  // 하의(바지=골반+다리) 색

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor ShoeColor = FLinearColor::Black;      // 직급 단계

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor SkinTone = FLinearColor(0.9f, 0.75f, 0.6f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor RimColor = FLinearColor::Transparent; // 희귀도 채널 (알파>0 = 림 표시)

	// 희귀도 손 금색 타겟 (HandMetalGold 블렌드 대상). 코드 하드코딩 금지 — 본 DT에서 튜닝.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor GoldHandColor = FLinearColor(1.0f, 0.766f, 0.336f);

	// 머티리얼 스칼라 파라미터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 0, UIMax = 1))
	float HandMetalGold = 0.f;                          // 0=피부, 1=황금 손

	// 액세서리
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWearGlasses = false;                          // 직급 임계값 (예: 과장 이상)

	// 빈칸이면 AStickOfficeworker::DefaultGlassesMesh 폴백
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> GlassesMesh;
};
