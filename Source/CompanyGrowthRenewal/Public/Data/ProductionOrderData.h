#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "ProductionOrderData.generated.h"

/**
 * 품질 등급별 기본 양산 수량 반환
 * S=50, A=30, B=15, C=8, D=3, F=1
 */
inline int32 GetBaseProductionQuantity(EQualityGrade Grade)
{
	switch (Grade)
	{
	case EQualityGrade::S: return 50;
	case EQualityGrade::A: return 30;
	case EQualityGrade::B: return 15;
	case EQualityGrade::C: return 8;
	case EQualityGrade::D: return 3;
	case EQualityGrade::F: return 1;
	default: return 1;
	}
}

/**
 * 생산 주문서
 * 제조업 회사의 양산 확정 시 생성되는 주문 데이터
 * 공장에서 이 주문서를 소비하여 제품을 생산
 */
USTRUCT(BlueprintType)
struct FProductionOrder
{
	GENERATED_BODY()

	// 주문 고유 ID
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	int32 OrderID = 0;

	// 회사 타입 (레시피 DataTable 조회에 필요)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	ECompanyType CompanyType = ECompanyType::None;

	// 프로젝트 인덱스 (1~100, 레시피 조회 키)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	int32 ProjectIndex = 0;

	// 제품 종류 (DataTable RowName)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	FName ProductID;

	// 제품 이름 (표시용)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	FString ProductName;

	// 개발한 건물 ID
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	int32 SourceBuildingID = INDEX_NONE;

	// 양산 수량 (품질 기반으로 결정)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	int32 Quantity = 0;

	// 남은 수량 (공장 생산 시 감소, 0이면 소진)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Order")
	int32 RemainingQuantity = 0;

	// 품질 등급
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Quality")
	EQualityGrade Grade = EQualityGrade::C;

	// 품질 점수 (0.5 ~ 2.0)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Quality")
	float QualityScore = 1.0f;

	// 주문서 소진 여부
	bool IsConsumed() const { return RemainingQuantity <= 0; }

	// 생산 가능 수량 (최대 Amount개 소비)
	int32 ConsumeQuantity(int32 Amount)
	{
		int32 Consumed = FMath::Min(Amount, RemainingQuantity);
		RemainingQuantity -= Consumed;
		return Consumed;
	}

	FProductionOrder() = default;
};
