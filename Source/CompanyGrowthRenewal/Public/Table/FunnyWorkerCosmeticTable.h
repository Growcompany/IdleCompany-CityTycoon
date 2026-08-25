#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/EmployeeTypes.h"
#include "Enum/GachaTier.h"
#include "FunnyWorkerCosmeticTable.generated.h"

/**
 * Funny 직원 자동 판독축 — 부서/직급/등급을 색·토글로 인코딩.
 * RowName = "D{dept}_R{rank}_T{tier}" (AFunnyOfficeworker::MakeCosmeticRowName).
 * 색/임계값 코드 하드코딩 금지 — 본 DT가 단일 진실 원천.
 */
USTRUCT(BlueprintType)
struct FFunnyWorkerCosmeticTable : public FTableRowBase
{
	GENERATED_BODY()

	// RowName 이 실제 키. 아래는 가독성/검증용 미러.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEmployeeDepartment Department = EEmployeeDepartment::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEmployeeRank Rank = EEmployeeRank::Intern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGachaTier Tier = EGachaTier::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor OuterwearTint = FLinearColor::Gray;   // 부서 채널

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor PantsTint = FLinearColor(0.22f, 0.25f, 0.32f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor ShoeTint = FLinearColor::Black;       // 직급 단계

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWearGlasses = false;                          // 직급 임계값

	// 등급 — 팩 바디는 머티리얼 슬롯이 1개라 손만 칠할 수 없다. 바디 전체를 물들인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor BodyGoldTint = FLinearColor(1.0f, 0.766f, 0.336f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 0, UIMax = 1))
	float GoldBlend = 0.f;      // 0=개인 스킨톤, 1=골드

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 0, UIMax = 1))
	float GoldMetallic = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 0, UIMax = 10))
	float GoldEmissive = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor RimColor = FLinearColor::Transparent;  // 알파>0 = 가챠 공개 시 림 표시
};
